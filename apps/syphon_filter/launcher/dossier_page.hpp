#pragma once

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstddef>
#include <memory>

namespace sf::platform::launcher {

struct DossierPageStyle {
  HFONT title_font{};
  HFONT heading_font{};
  HFONT ui_font{};
  HBRUSH background_brush{};
  HBRUSH panel_brush{};
  COLORREF grid_color{RGB(18, 28, 61)};
  COLORREF text_color{RGB(174, 190, 255)};
  COLORREF muted_text_color{RGB(103, 126, 190)};
  COLORREF accent_color{RGB(183, 239, 67)};
};

class DossierPage final {
public:
  static constexpr int previous_control_id = 3001;
  static constexpr int next_control_id = 3002;

  DossierPage();
  ~DossierPage();

  DossierPage(const DossierPage &) = delete;
  DossierPage &operator=(const DossierPage &) = delete;
  DossierPage(DossierPage &&) = delete;
  DossierPage &operator=(DossierPage &&) = delete;

  [[nodiscard]] bool create(HWND parent, const DossierPageStyle &style,
                            bool russian);
  void setVisible(bool visible) noexcept;
  void setLanguage(bool russian) noexcept;
  void layout(const RECT &bounds) noexcept;
  [[nodiscard]] bool handleCommand(WPARAM w_param, LPARAM l_param);
  [[nodiscard]] bool handleKey(WPARAM key);
  void paint(HDC dc, const RECT &bounds);
  void shutdown() noexcept;

  [[nodiscard]] bool available() const noexcept;
  [[nodiscard]] std::size_t page() const noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace sf::platform::launcher

#endif
