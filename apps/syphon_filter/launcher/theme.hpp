#pragma once

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>

namespace sf::platform::launcher {

struct ThemeColors final {
  static constexpr COLORREF background = RGB(3, 7, 17);
  static constexpr COLORREF panel = RGB(7, 13, 29);
  static constexpr COLORREF grid = RGB(18, 28, 61);
  static constexpr COLORREF border = RGB(80, 103, 196);
  static constexpr COLORREF text = RGB(174, 190, 255);
  static constexpr COLORREF muted_text = RGB(103, 126, 190);
  static constexpr COLORREF primary = RGB(68, 211, 151);
  static constexpr COLORREF archive = RGB(183, 239, 67);
  static constexpr COLORREF danger = RGB(224, 89, 95);
  static constexpr COLORREF pressed_fill = RGB(24, 39, 77);
  static constexpr COLORREF active_tab_fill = RGB(18, 28, 61);
};

struct ControlBounds {
  int x{};
  int y{};
  int width{};
  int height{};
};

enum class ActionButtonRole : std::uint8_t {
  secondary,
  primary,
  danger,
  archive,
};

class ThemeResources final {
public:
  explicit ThemeResources(UINT dpi = USER_DEFAULT_SCREEN_DPI) noexcept;
  ~ThemeResources();

  ThemeResources(const ThemeResources &) = delete;
  ThemeResources &operator=(const ThemeResources &) = delete;
  ThemeResources(ThemeResources &&) = delete;
  ThemeResources &operator=(ThemeResources &&) = delete;

  [[nodiscard]] bool reset(UINT dpi) noexcept;
  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] UINT dpi() const noexcept { return dpi_; }
  [[nodiscard]] HFONT titleFont() const noexcept { return title_font_; }
  [[nodiscard]] HFONT headingFont() const noexcept { return heading_font_; }
  [[nodiscard]] HFONT uiFont() const noexcept { return ui_font_; }
  [[nodiscard]] HBRUSH backgroundBrush() const noexcept {
    return background_brush_;
  }
  [[nodiscard]] HBRUSH panelBrush() const noexcept { return panel_brush_; }
  [[nodiscard]] HICON largeIcon() const noexcept { return large_icon_; }
  [[nodiscard]] HICON smallIcon() const noexcept { return small_icon_; }

private:
  void release() noexcept;

  UINT dpi_{USER_DEFAULT_SCREEN_DPI};
  HFONT title_font_{};
  HFONT heading_font_{};
  HFONT ui_font_{};
  HBRUSH background_brush_{};
  HBRUSH panel_brush_{};
  HICON large_icon_{};
  HICON small_icon_{};
};

[[nodiscard]] HWND createControl(HWND parent, const wchar_t *class_name,
                                 const wchar_t *text, DWORD style,
                                 ControlBounds bounds, int control_id,
                                 HFONT font = nullptr,
                                 DWORD extended_style = 0U) noexcept;

void drawActionButton(
    const DRAWITEMSTRUCT &item, HFONT font,
    ActionButtonRole role = ActionButtonRole::secondary) noexcept;

void drawTabButton(const DRAWITEMSTRUCT &item, HFONT font,
                   bool active) noexcept;

} // namespace sf::platform::launcher

#endif
