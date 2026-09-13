#pragma once

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "sf/platform/host.hpp"

#include <memory>

namespace sf::platform::launcher {

struct ControlsPageStyle {
  HFONT heading_font{};
  HFONT ui_font{};
  HBRUSH panel_brush{};
  COLORREF panel_color{RGB(7, 13, 29)};
  COLORREF grid_color{RGB(18, 28, 61)};
  COLORREF border_color{RGB(80, 103, 196)};
  COLORREF text_color{RGB(174, 190, 255)};
  COLORREF muted_text_color{RGB(103, 126, 190)};
};

// Embeddable launcher tab. The page edits the staged settings supplied by
// the owner; persistence remains the launcher's responsibility. It owns no
// message loop: the parent forwards WM_COMMAND, WM_TIMER and input messages.
class ControlsPage final {
public:
  static constexpr int binding_list_control_id = 2001;
  static constexpr int change_binding_control_id = 2002;
  static constexpr int clear_binding_control_id = 2003;
  static constexpr int default_bindings_control_id = 2004;
  static constexpr int input_device_control_id = 2006;
  static constexpr int controller_protocol_control_id = 1021;
  static constexpr int controller_vibration_control_id = 1022;
  static constexpr UINT_PTR capture_timer_id = 0x5346U;

  ControlsPage();
  ~ControlsPage();

  ControlsPage(const ControlsPage &) = delete;
  ControlsPage &operator=(const ControlsPage &) = delete;
  ControlsPage(ControlsPage &&) = delete;
  ControlsPage &operator=(ControlsPage &&) = delete;

  [[nodiscard]] bool
  create(HWND parent, const RECT &bounds, const ControlsPageStyle &style,
         KeyboardMouseBindings &keyboard, ControllerButtonBindings &controller,
         ControllerProtocol &protocol, bool &vibration, bool russian);
  void show();
  void hide() noexcept;
  void layout(const RECT &bounds) noexcept;
  void setRussian(bool russian);

  [[nodiscard]] bool handleCommand(WPARAM w_param, LPARAM l_param);
  [[nodiscard]] bool handleTimer(UINT_PTR timer_id);
  [[nodiscard]] bool handleInput(const MSG &message);
  [[nodiscard]] bool handleDrawItem(const DRAWITEMSTRUCT &item) const;

  void shutdown() noexcept;

  [[nodiscard]] bool visible() const noexcept;
  [[nodiscard]] bool capturing() const noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace sf::platform::launcher

#endif
