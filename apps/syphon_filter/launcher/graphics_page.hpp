#pragma once

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "sf/game/localization.hpp"
#include "sf/platform/host.hpp"

#include <memory>

namespace sf::platform::launcher {

struct GraphicsPageStyle {
  HFONT ui_font{};
};

class GraphicsPage final {
public:
  static constexpr int resolution_control_id = 1001;
  static constexpr int aspect_control_id = 1002;
  static constexpr int antialiasing_control_id = 1003;
  static constexpr int bilinear_control_id = 1004;
  static constexpr int fullscreen_control_id = 1005;
  static constexpr int anisotropic_control_id = 1011;
  static constexpr int vsync_control_id = 1016;
  static constexpr int frame_limit_control_id = 1017;
  static constexpr int trilinear_control_id = 1018;
  static constexpr int volumetric_effects_control_id = 1023;
  static constexpr int mission_skyboxes_control_id = 1024;

  GraphicsPage();
  ~GraphicsPage();

  GraphicsPage(const GraphicsPage &) = delete;
  GraphicsPage &operator=(const GraphicsPage &) = delete;
  GraphicsPage(GraphicsPage &&) = delete;
  GraphicsPage &operator=(GraphicsPage &&) = delete;

  [[nodiscard]] bool create(HWND parent, const RECT &bounds,
                            const GraphicsPageStyle &style,
                            GraphicsSettings &settings,
                            game::GameLanguage language);
  void show() noexcept;
  void hide() noexcept;
  void layout(const RECT &bounds) noexcept;
  void setLanguage(game::GameLanguage language);
  [[nodiscard]] bool handleCommand(WPARAM w_param, LPARAM l_param);
  [[nodiscard]] bool validateAndCommit(HWND owner);

  void shutdown() noexcept;
  [[nodiscard]] bool visible() const noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace sf::platform::launcher

#endif
