#pragma once

#include "sf/game/localization.hpp"

#include <array>
#include <cstddef>
#include <string_view>

namespace sf::platform::launcher {

struct ShellText {
  std::wstring_view window_title;
  std::wstring_view launch_tab;
  std::wstring_view graphics_tab;
  std::wstring_view controls_tab;
  std::wstring_view dossiers_tab;
  std::wstring_view play;
  std::wstring_view close;
  std::wstring_view browse;
};

struct LaunchPageText {
  std::wstring_view game_image;
  std::wstring_view game_image_hint;
  std::wstring_view selected_image_prefix;
  std::wstring_view no_image_selected;
  std::wstring_view text_language;
  std::wstring_view english_language;
  std::wstring_view russian_language;
  std::wstring_view cue_files;
  std::wstring_view all_files;
};

struct GraphicsPageText {
  std::wstring_view resolution;
  std::wstring_view aspect_ratio;
  std::wstring_view antialiasing;
  std::wstring_view frame_limit;
  std::wstring_view bilinear_filtering;
  std::wstring_view trilinear_filtering;
  std::wstring_view anisotropic_filtering;
  std::wstring_view volumetric_effects;
  std::wstring_view mission_skyboxes;
  std::wstring_view vertical_sync;
  std::wstring_view borderless_fullscreen;
  std::wstring_view disabled;
  std::wstring_view smaa_ultra;
  std::wstring_view msaa_2x;
  std::wstring_view msaa_4x;
  std::wstring_view msaa_8x;
  std::wstring_view unlimited;
  std::wstring_view fps_suffix;
  std::wstring_view adaptive_aspect;
  std::wstring_view original_aspect;
};

inline constexpr std::size_t keyboard_action_text_count = 31U;
inline constexpr std::size_t controller_action_text_count = 9U;
inline constexpr std::size_t controller_stick_layout_text_count = 3U;

struct ControlsPageText {
  std::wstring_view heading;
  std::wstring_view input_device;
  std::wstring_view keyboard_mouse;
  std::wstring_view controller;
  std::wstring_view controller_backend;
  std::wstring_view automatic_protocol;
  std::wstring_view xinput_protocol;
  std::wstring_view direct_input_protocol;
  std::wstring_view raw_input_protocol;
  std::wstring_view vibration;
  std::wstring_view assignments;
  std::wstring_view binding_controls;
  std::wstring_view change_prefix;
  std::wstring_view clear;
  std::wstring_view restore_defaults;
  std::wstring_view stick_layout;
  std::wstring_view next_layout;
  std::wstring_view select_action_hint;
  std::wstring_view choose_button_hint;
  std::wstring_view choose_layout_hint;
  std::wstring_view movement_hint;
  std::wstring_view waiting_for_input;
  std::wstring_view keyboard_capture_hint;
  std::wstring_view waiting_for_controller;
  std::wstring_view controller_capture_hint;
  std::wstring_view connect_controller_hint;
  std::wstring_view no_controller;
  std::wstring_view xbox_controller;
  std::wstring_view playstation_controller;
  std::wstring_view nintendo_controller;
  std::wstring_view generic_controller;
  std::wstring_view binding_updated;
  std::wstring_view binding_cleared;
  std::wstring_view capture_cancelled;
  std::wstring_view defaults_restored;
  std::wstring_view layout_updated;
  std::wstring_view controller_init_failed;
  std::wstring_view controller_disconnected;
  std::wstring_view action_fallback;
  std::array<std::wstring_view, keyboard_action_text_count> keyboard_actions;
  std::array<std::wstring_view, controller_action_text_count>
      controller_actions;
  std::array<std::wstring_view, controller_stick_layout_text_count>
      stick_layouts;
};

struct DossiersPageText {
  std::wstring_view title;
  std::wstring_view subtitle;
  std::wstring_view previous;
  std::wstring_view next;
  std::wstring_view file;
  std::wstring_view navigation_hint;
};

struct ValidationText {
  std::wstring_view disc_image_required_title;
  std::wstring_view disc_image_required_message;
  std::wstring_view invalid_resolution_title;
  std::wstring_view invalid_resolution_message;
  std::wstring_view language_pack_missing_title;
  std::wstring_view language_pack_missing_message;
  std::wstring_view dossiers_unavailable_title;
  std::wstring_view dossier_decoder_unavailable_message;
  std::wstring_view dossier_files_unavailable_message;
  std::wstring_view restricted_access_title;
  std::wstring_view unsupported_disc_title;
  std::wstring_view startup_failed_title;
  std::wstring_view settings_save_failed_title;
  std::wstring_view settings_save_failed_message;
  std::wstring_view unexpected_error_title;
};

struct LauncherText {
  ShellText shell;
  LaunchPageText launch;
  GraphicsPageText graphics;
  ControlsPageText controls;
  DossiersPageText dossiers;
  ValidationText validation;
};

[[nodiscard]] const LauncherText &textFor(game::GameLanguage language) noexcept;

} // namespace sf::platform::launcher
