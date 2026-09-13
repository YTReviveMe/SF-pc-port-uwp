#include "dossier_page.hpp"
#include "text.hpp"

#ifdef _WIN32

#include <objidl.h>

#ifndef GDIPVER
#define GDIPVER 0x0110
#endif
#include <gdiplus.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>

namespace sf::platform::launcher {
namespace {

constexpr std::size_t dossier_page_count = 4U;

std::filesystem::path executableDirectory() {
  std::array<wchar_t, 32768U> buffer{};
  const auto length = GetModuleFileNameW(nullptr, buffer.data(),
                                         static_cast<DWORD>(buffer.size()));
  if (length == 0U || length >= buffer.size()) {
    std::error_code error;
    return std::filesystem::current_path(error);
  }
  return std::filesystem::path{buffer.data()}.parent_path();
}

std::array<std::filesystem::path, dossier_page_count> dossierFiles() {
  auto directory = executableDirectory() / L"assets" / L"dossiers" / L"screens";
  std::error_code error;
  if (!std::filesystem::is_directory(directory, error) || error) {
    error.clear();
    const auto current = std::filesystem::current_path(error);
    if (!error) {
      directory = current / L"assets" / L"dossiers" / L"screens";
    }
  }
  return {
      directory / L"dossier_01.png",
      directory / L"dossier_02.png",
      directory / L"dossier_03.png",
      directory / L"dossier_04.png",
  };
}

std::unique_ptr<Gdiplus::Bitmap>
loadDossierImage(const std::filesystem::path &path) {
  std::error_code error;
  if (!std::filesystem::is_regular_file(path, error) || error) {
    return {};
  }
  auto image = std::make_unique<Gdiplus::Bitmap>(path.c_str());
  if (image->GetLastStatus() != Gdiplus::Ok || image->GetWidth() == 0U ||
      image->GetHeight() == 0U) {
    return {};
  }

  Gdiplus::Sharpen sharpen;
  const Gdiplus::SharpenParams parameters{1.0F, 14.0F};
  if (sharpen.SetParameters(&parameters) == Gdiplus::Ok) {
    RECT region{0, 0, static_cast<LONG>(image->GetWidth()),
                static_cast<LONG>(image->GetHeight())};
    static_cast<void>(image->ApplyEffect(&sharpen, &region));
  }
  return image;
}

HWND createButton(HWND parent, const wchar_t *text, int id, HFONT font) {
  const auto button = CreateWindowExW(
      0, L"BUTTON", text, WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON | BS_FLAT, 0, 0,
      0, 0, parent, reinterpret_cast<HMENU>(static_cast<std::intptr_t>(id)),
      GetModuleHandleW(nullptr), nullptr);
  if (button != nullptr) {
    const auto selected_font =
        font != nullptr ? font
                        : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(selected_font),
                 TRUE);
  }
  return button;
}

HBRUSH fallbackBrush(HBRUSH brush, int stock_brush) noexcept {
  return brush != nullptr ? brush
                          : static_cast<HBRUSH>(GetStockObject(stock_brush));
}

HFONT fallbackFont(HFONT font) noexcept {
  return font != nullptr ? font
                         : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
}

RECT normalizedBounds(RECT bounds) noexcept {
  bounds.right = std::max(bounds.right, bounds.left + 1L);
  bounds.bottom = std::max(bounds.bottom, bounds.top + 1L);
  return bounds;
}

bool sameRect(const RECT &left, const RECT &right) noexcept {
  return left.left == right.left && left.top == right.top &&
         left.right == right.right && left.bottom == right.bottom;
}

} // namespace

struct DossierPage::Impl {
  HWND parent{};
  HWND previous_button{};
  HWND next_button{};
  DossierPageStyle style{};
  std::array<std::unique_ptr<Gdiplus::Bitmap>, dossier_page_count> images{};
  ULONG_PTR gdiplus_token{};
  std::size_t page{};
  RECT bounds{};
  bool has_bounds{};
  bool visible{};
  bool russian{};

  [[nodiscard]] const DossiersPageText &localizedText() const noexcept {
    const auto language =
        russian ? game::GameLanguage::russian_vit : game::GameLanguage::english;
    return textFor(language).dossiers;
  }

  void layout(RECT new_bounds) noexcept {
    new_bounds = normalizedBounds(new_bounds);
    if (has_bounds && sameRect(bounds, new_bounds)) {
      return;
    }
    bounds = new_bounds;
    has_bounds = true;

    constexpr LONG padding = 12;
    constexpr LONG footer_height = 44;
    constexpr LONG previous_width = 112;
    constexpr LONG next_width = 92;
    constexpr LONG button_height = 32;
    const auto y = std::max(bounds.top, bounds.bottom - footer_height + 4L);
    const auto previous_x = bounds.left + padding;
    const auto next_x = previous_x + previous_width + 10L;
    if (previous_button != nullptr) {
      MoveWindow(previous_button, previous_x, y, previous_width, button_height,
                 TRUE);
    }
    if (next_button != nullptr) {
      MoveWindow(next_button, next_x, y, next_width, button_height, TRUE);
    }
  }

  [[nodiscard]] RECT imagePanel() const noexcept {
    constexpr LONG horizontal_padding = 12;
    constexpr LONG header_height = 58;
    constexpr LONG footer_height = 46;
    return RECT{bounds.left + horizontal_padding, bounds.top + header_height,
                std::max(bounds.left + horizontal_padding + 1L,
                         bounds.right - horizontal_padding),
                std::max(bounds.top + header_height + 1L,
                         bounds.bottom - footer_height)};
  }

  void updateNavigation() noexcept {
    if (previous_button != nullptr) {
      EnableWindow(previous_button, page > 0U ? TRUE : FALSE);
    }
    if (next_button != nullptr) {
      EnableWindow(next_button, page + 1U < images.size() ? TRUE : FALSE);
    }
  }

  void invalidate() const noexcept {
    if (parent != nullptr && has_bounds) {
      InvalidateRect(parent, &bounds, FALSE);
    }
  }

  bool selectPage(std::size_t new_page) noexcept {
    if (new_page >= images.size() || images[new_page] == nullptr) {
      return false;
    }
    page = new_page;
    updateNavigation();
    invalidate();
    return true;
  }
};

DossierPage::DossierPage() = default;

DossierPage::~DossierPage() { shutdown(); }

bool DossierPage::create(HWND parent, const DossierPageStyle &style,
                         bool russian) {
  shutdown();
  if (parent == nullptr) {
    return false;
  }
  try {
    impl_ = std::make_unique<Impl>();
    impl_->parent = parent;
    impl_->style = style;
    impl_->russian = russian;

    Gdiplus::GdiplusStartupInputEx startup_input{};
    if (Gdiplus::GdiplusStartup(&impl_->gdiplus_token, &startup_input,
                                nullptr) != Gdiplus::Ok) {
      shutdown();
      return false;
    }

    const auto files = dossierFiles();
    for (std::size_t page = 0U; page < files.size(); ++page) {
      impl_->images[page] = loadDossierImage(files[page]);
      if (impl_->images[page] == nullptr) {
        shutdown();
        return false;
      }
    }

    const auto &text = impl_->localizedText();
    impl_->previous_button = createButton(
        parent, text.previous.data(), previous_control_id, style.heading_font);
    impl_->next_button = createButton(parent, text.next.data(), next_control_id,
                                      style.heading_font);
    if (impl_->previous_button == nullptr || impl_->next_button == nullptr) {
      shutdown();
      return false;
    }
    impl_->updateNavigation();
    setVisible(false);
    return true;
  } catch (...) {
    shutdown();
    return false;
  }
}

void DossierPage::setVisible(bool visible) noexcept {
  if (impl_ == nullptr) {
    return;
  }
  impl_->visible = visible;
  const auto show = visible ? SW_SHOW : SW_HIDE;
  if (impl_->previous_button != nullptr) {
    ShowWindow(impl_->previous_button, show);
  }
  if (impl_->next_button != nullptr) {
    ShowWindow(impl_->next_button, show);
  }
  impl_->invalidate();
}

void DossierPage::setLanguage(bool russian) noexcept {
  if (impl_ == nullptr) {
    return;
  }
  impl_->russian = russian;
  const auto &text = impl_->localizedText();
  if (impl_->previous_button != nullptr) {
    SetWindowTextW(impl_->previous_button, text.previous.data());
  }
  if (impl_->next_button != nullptr) {
    SetWindowTextW(impl_->next_button, text.next.data());
  }
  impl_->invalidate();
}

void DossierPage::layout(const RECT &bounds) noexcept {
  if (impl_ != nullptr) {
    impl_->layout(bounds);
  }
}

bool DossierPage::handleCommand(WPARAM w_param, LPARAM l_param) {
  if (impl_ == nullptr || !impl_->visible) {
    return false;
  }
  const auto control_id = static_cast<int>(LOWORD(w_param));
  const auto notification = HIWORD(w_param);
  const auto source = reinterpret_cast<HWND>(l_param);
  if (control_id == previous_control_id && notification == BN_CLICKED &&
      source == impl_->previous_button) {
    if (impl_->page > 0U) {
      static_cast<void>(impl_->selectPage(impl_->page - 1U));
    }
    return true;
  }
  if (control_id == next_control_id && notification == BN_CLICKED &&
      source == impl_->next_button) {
    if (impl_->page + 1U < impl_->images.size()) {
      static_cast<void>(impl_->selectPage(impl_->page + 1U));
    }
    return true;
  }
  return false;
}

bool DossierPage::handleKey(WPARAM key) {
  if (impl_ == nullptr || !impl_->visible) {
    return false;
  }
  if (key == VK_LEFT || key == 'A') {
    if (impl_->page > 0U) {
      static_cast<void>(impl_->selectPage(impl_->page - 1U));
    }
    return true;
  }
  if (key == VK_RIGHT || key == 'D') {
    if (impl_->page + 1U < impl_->images.size()) {
      static_cast<void>(impl_->selectPage(impl_->page + 1U));
    }
    return true;
  }
  return false;
}

void DossierPage::paint(HDC dc, const RECT &bounds) {
  if (impl_ == nullptr || !impl_->visible || dc == nullptr) {
    return;
  }
  impl_->layout(bounds);
  const auto &text = impl_->localizedText();

  const auto saved_dc = SaveDC(dc);
  if (saved_dc == 0) {
    return;
  }
  IntersectClipRect(dc, impl_->bounds.left, impl_->bounds.top,
                    impl_->bounds.right, impl_->bounds.bottom);
  FillRect(dc, &impl_->bounds,
           fallbackBrush(impl_->style.background_brush, BLACK_BRUSH));

  HGDIOBJ old_pen{};
  const auto grid_pen = CreatePen(PS_SOLID, 1, impl_->style.grid_color);
  if (grid_pen != nullptr) {
    old_pen = SelectObject(dc, grid_pen);
    for (auto x = impl_->bounds.left; x < impl_->bounds.right; x += 32) {
      MoveToEx(dc, x, impl_->bounds.top + 54, nullptr);
      LineTo(dc, x, impl_->bounds.bottom);
    }
    for (auto y = impl_->bounds.top + 54; y < impl_->bounds.bottom; y += 32) {
      MoveToEx(dc, impl_->bounds.left, y, nullptr);
      LineTo(dc, impl_->bounds.right, y);
    }
  }

  const auto panel = impl_->imagePanel();
  FillRect(dc, &panel, fallbackBrush(impl_->style.panel_brush, BLACK_BRUSH));
  const auto border_pen = CreatePen(PS_SOLID, 2, impl_->style.accent_color);
  if (border_pen != nullptr) {
    if (old_pen == nullptr) {
      old_pen = SelectObject(dc, border_pen);
    } else {
      SelectObject(dc, border_pen);
    }
    SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, panel.left, panel.top, panel.right, panel.bottom);
  }

  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, impl_->style.accent_color);
  SelectObject(dc, fallbackFont(impl_->style.title_font));
  RECT title{impl_->bounds.left + 16, impl_->bounds.top + 4,
             impl_->bounds.left + 198, impl_->bounds.top + 44};
  DrawTextW(dc, text.title.data(), -1, &title,
            DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

  SetTextColor(dc, impl_->style.text_color);
  SelectObject(dc, fallbackFont(impl_->style.ui_font));
  RECT subtitle{impl_->bounds.left + 204, impl_->bounds.top + 8,
                impl_->bounds.right - 16, impl_->bounds.top + 44};
  DrawTextW(dc, text.subtitle.data(), -1, &subtitle,
            DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

  const auto accent_pen = CreatePen(PS_SOLID, 1, impl_->style.accent_color);
  if (accent_pen != nullptr) {
    if (old_pen == nullptr) {
      old_pen = SelectObject(dc, accent_pen);
    } else {
      SelectObject(dc, accent_pen);
    }
    MoveToEx(dc, impl_->bounds.left + 16, impl_->bounds.top + 50, nullptr);
    LineTo(dc, impl_->bounds.right - 16, impl_->bounds.top + 50);
  }

  const auto &image = impl_->images[impl_->page];
  if (image != nullptr) {
    constexpr LONG image_padding = 10;
    const auto available_width = static_cast<int>(
        std::max<LONG>(1L, panel.right - panel.left - image_padding * 2L));
    const auto available_height = static_cast<int>(
        std::max<LONG>(1L, panel.bottom - panel.top - image_padding * 2L));
    const auto image_width = static_cast<double>(image->GetWidth());
    const auto image_height = static_cast<double>(image->GetHeight());
    const auto scale =
        std::min(static_cast<double>(available_width) / image_width,
                 static_cast<double>(available_height) / image_height);
    const auto width =
        std::max(1, static_cast<int>(std::lround(image_width * scale)));
    const auto height =
        std::max(1, static_cast<int>(std::lround(image_height * scale)));
    const auto x = panel.left + (panel.right - panel.left - width) / 2;
    const auto y = panel.top + (panel.bottom - panel.top - height) / 2;
    Gdiplus::Graphics graphics{dc};
    graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    graphics.DrawImage(image.get(), x, y, width, height);
  }

  SelectObject(dc, fallbackFont(impl_->style.heading_font));
  SetTextColor(dc, impl_->style.accent_color);
  const auto page_label = std::wstring{text.file} + L" " +
                          std::to_wstring(impl_->page + 1U) + L" / " +
                          std::to_wstring(impl_->images.size());
  RECT page_text{impl_->bounds.left + 226, impl_->bounds.bottom - 42,
                 impl_->bounds.left + 356, impl_->bounds.bottom - 6};
  DrawTextW(dc, page_label.c_str(), -1, &page_text,
            DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

  SelectObject(dc, fallbackFont(impl_->style.ui_font));
  SetTextColor(dc, impl_->style.muted_text_color);
  RECT hint{impl_->bounds.left + 364, impl_->bounds.bottom - 42,
            impl_->bounds.right - 12, impl_->bounds.bottom - 6};
  DrawTextW(dc, text.navigation_hint.data(), -1, &hint,
            DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

  RestoreDC(dc, saved_dc);
  if (accent_pen != nullptr) {
    DeleteObject(accent_pen);
  }
  if (border_pen != nullptr) {
    DeleteObject(border_pen);
  }
  if (grid_pen != nullptr) {
    DeleteObject(grid_pen);
  }
}

void DossierPage::shutdown() noexcept {
  if (impl_ == nullptr) {
    return;
  }
  if (impl_->previous_button != nullptr &&
      IsWindow(impl_->previous_button) != FALSE) {
    DestroyWindow(impl_->previous_button);
  }
  if (impl_->next_button != nullptr && IsWindow(impl_->next_button) != FALSE) {
    DestroyWindow(impl_->next_button);
  }
  impl_->previous_button = nullptr;
  impl_->next_button = nullptr;
  for (auto &image : impl_->images) {
    image.reset();
  }
  if (impl_->gdiplus_token != 0U) {
    Gdiplus::GdiplusShutdown(impl_->gdiplus_token);
    impl_->gdiplus_token = 0U;
  }
  impl_.reset();
}

bool DossierPage::available() const noexcept { return impl_ != nullptr; }

std::size_t DossierPage::page() const noexcept {
  return impl_ != nullptr ? impl_->page : 0U;
}

} // namespace sf::platform::launcher

#endif
