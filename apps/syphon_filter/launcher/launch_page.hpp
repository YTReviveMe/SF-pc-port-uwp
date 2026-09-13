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

#include <filesystem>
#include <memory>

namespace sf::platform::launcher {

struct LaunchPageStyle {
  HFONT heading_font{};
  HFONT ui_font{};
};

class LaunchPage final {
public:
  static constexpr int game_image_control_id = 1012;
  static constexpr int browse_image_control_id = 1013;
  static constexpr int language_control_id = 1015;

  LaunchPage();
  ~LaunchPage();

  LaunchPage(const LaunchPage &) = delete;
  LaunchPage &operator=(const LaunchPage &) = delete;
  LaunchPage(LaunchPage &&) = delete;
  LaunchPage &operator=(LaunchPage &&) = delete;

  [[nodiscard]] bool create(HWND parent, const RECT &bounds,
                            const LaunchPageStyle &style,
                            std::filesystem::path &cue_path,
                            game::GameLanguage &language);
  void show() noexcept;
  void hide() noexcept;
  void layout(const RECT &bounds) noexcept;
  void setLanguage(game::GameLanguage language);
  [[nodiscard]] bool handleCommand(WPARAM w_param, LPARAM l_param);
  [[nodiscard]] bool validate(HWND owner);
  void shutdown() noexcept;

  [[nodiscard]] bool visible() const noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace sf::platform::launcher

#endif
