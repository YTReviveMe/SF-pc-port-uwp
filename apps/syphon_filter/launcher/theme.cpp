#include "theme.hpp"

#ifdef _WIN32

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>

namespace sf::platform::launcher {
namespace {

HFONT createThemeFont(int logical_height, int weight, const wchar_t *face,
                      UINT dpi) noexcept {
  const auto height =
      MulDiv(logical_height, static_cast<int>(std::max(dpi, 1U)),
             USER_DEFAULT_SCREEN_DPI);
  return CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                     CLEARTYPE_QUALITY, FF_DONTCARE, face);
}

void deleteGdiObject(HGDIOBJ &object) noexcept {
  if (object != nullptr) {
    DeleteObject(object);
    object = nullptr;
  }
}

void destroyIcon(HICON &icon) noexcept {
  if (icon != nullptr) {
    DestroyIcon(icon);
    icon = nullptr;
  }
}

HICON createFallbackIcon(int size) noexcept {
  BITMAPV5HEADER header{};
  header.bV5Size = sizeof(header);
  header.bV5Width = size;
  header.bV5Height = -size;
  header.bV5Planes = 1;
  header.bV5BitCount = 32;
  header.bV5Compression = BI_BITFIELDS;
  header.bV5RedMask = 0x00ff0000U;
  header.bV5GreenMask = 0x0000ff00U;
  header.bV5BlueMask = 0x000000ffU;
  header.bV5AlphaMask = 0xff000000U;

  void *bits{};
  const auto color =
      CreateDIBSection(nullptr, reinterpret_cast<BITMAPINFO *>(&header),
                       DIB_RGB_COLORS, &bits, nullptr, 0);
  if (color == nullptr || bits == nullptr) {
    if (color != nullptr) {
      DeleteObject(color);
    }
    return nullptr;
  }

  auto *pixels = static_cast<std::uint32_t *>(bits);
  for (auto y = 0; y < size; ++y) {
    for (auto x = 0; x < size; ++x) {
      const auto border = x < 2 || y < 2 || x >= size - 2 || y >= size - 2;
      const auto diagonal =
          std::abs(x - y) <= std::max(1, size / 16) ||
          std::abs((size - 1 - x) - y) <= std::max(1, size / 16);
      const auto center = std::abs(x - size / 2) <= std::max(1, size / 12) ||
                          std::abs(y - size / 2) <= std::max(1, size / 12);
      const auto rgb = center && diagonal ? 0x00f29a2eU
                       : border           ? 0x005067c4U
                                          : 0x00070d1dU;
      pixels[static_cast<std::size_t>(y * size + x)] = 0xff000000U | rgb;
    }
  }

  const auto mask = CreateBitmap(size, size, 1, 1, nullptr);
  if (mask == nullptr) {
    DeleteObject(color);
    return nullptr;
  }
  ICONINFO info{};
  info.fIcon = TRUE;
  info.hbmColor = color;
  info.hbmMask = mask;
  const auto icon = CreateIconIndirect(&info);
  DeleteObject(mask);
  DeleteObject(color);
  return icon;
}

HICON createThemeIcon(int logical_size, UINT dpi) noexcept {
  const auto size =
      std::max(1, MulDiv(logical_size, static_cast<int>(std::max(dpi, 1U)),
                         USER_DEFAULT_SCREEN_DPI));
  if (const auto resource =
          LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1), IMAGE_ICON,
                     size, size, LR_DEFAULTCOLOR)) {
    return static_cast<HICON>(resource);
  }
  return createFallbackIcon(size);
}

COLORREF accentForRole(ActionButtonRole role) noexcept {
  switch (role) {
  case ActionButtonRole::primary:
    return ThemeColors::primary;
  case ActionButtonRole::danger:
    return ThemeColors::danger;
  case ActionButtonRole::archive:
    return ThemeColors::archive;
  case ActionButtonRole::secondary:
  default:
    return ThemeColors::border;
  }
}

void fillSolid(HDC dc, const RECT &bounds, COLORREF color) noexcept {
  const auto brush = static_cast<HBRUSH>(GetStockObject(DC_BRUSH));
  const auto previous_color = SetDCBrushColor(dc, color);
  FillRect(dc, &bounds, brush);
  SetDCBrushColor(dc, previous_color);
}

void drawBorder(HDC dc, const RECT &bounds, COLORREF color) noexcept {
  const auto previous_pen = SelectObject(dc, GetStockObject(DC_PEN));
  const auto previous_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
  const auto previous_color = SetDCPenColor(dc, color);
  Rectangle(dc, bounds.left, bounds.top, bounds.right, bounds.bottom);
  SetDCPenColor(dc, previous_color);
  SelectObject(dc, previous_brush);
  SelectObject(dc, previous_pen);
}

void drawButtonLabel(const DRAWITEMSTRUCT &item, HFONT font, COLORREF color,
                     bool pressed) noexcept {
  std::array<wchar_t, 256U> label{};
  GetWindowTextW(item.hwndItem, label.data(), static_cast<int>(label.size()));

  const auto previous_mode = SetBkMode(item.hDC, TRANSPARENT);
  const auto previous_color = SetTextColor(item.hDC, color);
  const auto previous_font =
      font != nullptr ? SelectObject(item.hDC, font) : nullptr;
  auto text_bounds = item.rcItem;
  if (pressed) {
    OffsetRect(&text_bounds, 1, 1);
  }
  DrawTextW(item.hDC, label.data(), -1, &text_bounds,
            DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS |
                DT_NOPREFIX);
  if ((item.itemState & ODS_FOCUS) != 0U) {
    InflateRect(&text_bounds, -4, -4);
    DrawFocusRect(item.hDC, &text_bounds);
  }
  if (previous_font != nullptr && previous_font != HGDI_ERROR) {
    SelectObject(item.hDC, previous_font);
  }
  SetTextColor(item.hDC, previous_color);
  SetBkMode(item.hDC, previous_mode);
}

void drawButton(const DRAWITEMSTRUCT &item, HFONT font, COLORREF accent,
                COLORREF resting_fill, COLORREF text_color,
                bool active_tab) noexcept {
  if (item.hDC == nullptr || item.hwndItem == nullptr) {
    return;
  }
  const auto pressed = (item.itemState & ODS_SELECTED) != 0U;
  const auto disabled = (item.itemState & ODS_DISABLED) != 0U;
  const auto fill = pressed ? ThemeColors::pressed_fill : resting_fill;
  const auto border = disabled ? ThemeColors::grid : accent;
  const auto text = disabled ? ThemeColors::muted_text : text_color;

  fillSolid(item.hDC, item.rcItem, fill);
  drawBorder(item.hDC, item.rcItem, border);
  if (active_tab && !disabled) {
    const auto previous_pen = SelectObject(item.hDC, GetStockObject(DC_PEN));
    const auto previous_color = SetDCPenColor(item.hDC, accent);
    MoveToEx(item.hDC, item.rcItem.left + 2, item.rcItem.bottom - 2, nullptr);
    LineTo(item.hDC, item.rcItem.right - 2, item.rcItem.bottom - 2);
    SetDCPenColor(item.hDC, previous_color);
    SelectObject(item.hDC, previous_pen);
  }
  drawButtonLabel(item, font, text, pressed);
}

} // namespace

ThemeResources::ThemeResources(UINT dpi) noexcept {
  static_cast<void>(reset(dpi));
}

ThemeResources::~ThemeResources() { release(); }

bool ThemeResources::reset(UINT dpi) noexcept {
  dpi = std::max(dpi, 1U);
  if (dpi_ == dpi && valid()) {
    return true;
  }
  auto title = createThemeFont(-32, FW_BOLD, L"Bahnschrift SemiCondensed", dpi);
  auto heading =
      createThemeFont(-18, FW_BOLD, L"Bahnschrift SemiCondensed", dpi);
  auto ui = createThemeFont(-16, FW_NORMAL, L"Bahnschrift", dpi);
  auto background = CreateSolidBrush(ThemeColors::background);
  auto panel = CreateSolidBrush(ThemeColors::panel);
  auto large_icon = createThemeIcon(32, dpi);
  auto small_icon = createThemeIcon(16, dpi);

  if (title == nullptr || heading == nullptr || ui == nullptr ||
      background == nullptr || panel == nullptr) {
    auto title_object = static_cast<HGDIOBJ>(title);
    auto heading_object = static_cast<HGDIOBJ>(heading);
    auto ui_object = static_cast<HGDIOBJ>(ui);
    auto background_object = static_cast<HGDIOBJ>(background);
    auto panel_object = static_cast<HGDIOBJ>(panel);
    deleteGdiObject(title_object);
    deleteGdiObject(heading_object);
    deleteGdiObject(ui_object);
    deleteGdiObject(background_object);
    deleteGdiObject(panel_object);
    destroyIcon(large_icon);
    destroyIcon(small_icon);
    return false;
  }

  release();
  dpi_ = dpi;
  title_font_ = title;
  heading_font_ = heading;
  ui_font_ = ui;
  background_brush_ = background;
  panel_brush_ = panel;
  large_icon_ = large_icon;
  small_icon_ = small_icon;
  return true;
}

bool ThemeResources::valid() const noexcept {
  return title_font_ != nullptr && heading_font_ != nullptr &&
         ui_font_ != nullptr && background_brush_ != nullptr &&
         panel_brush_ != nullptr;
}

void ThemeResources::release() noexcept {
  destroyIcon(small_icon_);
  destroyIcon(large_icon_);
  auto panel = static_cast<HGDIOBJ>(panel_brush_);
  auto background = static_cast<HGDIOBJ>(background_brush_);
  auto ui = static_cast<HGDIOBJ>(ui_font_);
  auto heading = static_cast<HGDIOBJ>(heading_font_);
  auto title = static_cast<HGDIOBJ>(title_font_);
  deleteGdiObject(panel);
  deleteGdiObject(background);
  deleteGdiObject(ui);
  deleteGdiObject(heading);
  deleteGdiObject(title);
  panel_brush_ = nullptr;
  background_brush_ = nullptr;
  ui_font_ = nullptr;
  heading_font_ = nullptr;
  title_font_ = nullptr;
}

HWND createControl(HWND parent, const wchar_t *class_name, const wchar_t *text,
                   DWORD style, ControlBounds bounds, int control_id,
                   HFONT font, DWORD extended_style) noexcept {
  const auto control = CreateWindowExW(
      extended_style, class_name, text, WS_CHILD | WS_VISIBLE | style, bounds.x,
      bounds.y, bounds.width, bounds.height, parent,
      reinterpret_cast<HMENU>(static_cast<std::intptr_t>(control_id)),
      GetModuleHandleW(nullptr), nullptr);
  if (control != nullptr) {
    SendMessageW(control, WM_SETFONT,
                 reinterpret_cast<WPARAM>(
                     font != nullptr ? font : GetStockObject(DEFAULT_GUI_FONT)),
                 TRUE);
  }
  return control;
}

void drawActionButton(const DRAWITEMSTRUCT &item, HFONT font,
                      ActionButtonRole role) noexcept {
  drawButton(item, font, accentForRole(role), ThemeColors::panel,
             ThemeColors::text, false);
}

void drawTabButton(const DRAWITEMSTRUCT &item, HFONT font,
                   bool active) noexcept {
  drawButton(item, font, active ? ThemeColors::primary : ThemeColors::border,
             active ? ThemeColors::active_tab_fill : ThemeColors::panel,
             active ? ThemeColors::text : ThemeColors::muted_text, active);
}

} // namespace sf::platform::launcher

#endif
