#pragma once

#include "sf/game/hud.hpp"
#include "sf/game/legacy_bridge_types.hpp"

#include <cstddef>

namespace sf::platform {

// These utilities keep their authored English captions inside the original
// optic. The Russian font atlas reuses the same glyph UVs for Cyrillic, so
// drawing the untouched retail TEXT packets through that atlas produces
// transliterated garbage. The virus scanner uses the same packet path as the
// two rifle scopes even though it has no custom SCOPED.TIM frame.
[[nodiscard]] constexpr bool
usesRetailEnglishOpticText(game::WeaponId weapon) noexcept {
  return weapon == game::WeaponId::nightvision_rifle ||
         weapon == game::WeaponId::sniper_rifle ||
         weapon == game::WeaponId::virus_scanner;
}

[[nodiscard]] constexpr bool retailRifleScopeOverlayActive(
    bool first_person_aim, std::uint8_t interface_mode,
    std::uint8_t first_person_aim_mode) noexcept {
  return first_person_aim &&
         ((interface_mode == 2U && first_person_aim_mode == 2U) ||
          (interface_mode == 3U && first_person_aim_mode == 3U));
}

[[nodiscard]] constexpr double retailRifleScopeGameplayHudVisibility(
    bool first_person_aim, std::uint8_t interface_mode,
    std::uint8_t first_person_aim_mode,
    double normal_hud_visibility) noexcept {
  // Scope material remains in its authored optic pass; only the ordinary
  // armor/radar/weapon HUD is hidden. No retained phase means release restores
  // the caller's live HUD visibility in the same presentation frame.
  return retailRifleScopeOverlayActive(first_person_aim, interface_mode,
                                       first_person_aim_mode)
             ? 0.0
             : normal_hud_visibility;
}

// The viral detector deliberately uses two different retail states: camera
// aim mode 4 selects first person, while INTERFACE mode 5 owns its 28-line
// sight and pulsing target dot.
[[nodiscard]] constexpr bool retailVirusScannerOverlayActive(
    bool first_person_aim, std::uint8_t interface_mode,
    std::uint8_t first_person_aim_mode) noexcept {
  return first_person_aim && interface_mode == 5U &&
         first_person_aim_mode == 4U;
}

// FUN_80041830 renders the ordinary sniper sight inside the same 320x160
// presentation window used by the original PS1 interface transition. The
// native scene must not leak through above or below that authored aperture.
// SVD/night-vision owns a different full-screen material path.
[[nodiscard]] constexpr bool retailSniperScopeLetterboxActive(
    bool first_person_aim, std::uint8_t optic_mode) noexcept {
  return first_person_aim && optic_mode == 2U;
}

inline constexpr int retail_sniper_scope_aperture_width = 320;
inline constexpr int retail_sniper_scope_bar_height = 40;

// Retail scope captions live in the centered TEXT slots and have no backdrop.
// Classify the live packet itself instead of its optional source string:
// the bridge can legitimately observe an empty string or only the first few
// typewriter glyphs while the original scope label is being revealed.
[[nodiscard]] constexpr bool
isRetailScopeMessage(bool scoped, game::LegacyUiMessageChannel channel,
                     bool has_backdrop, std::size_t glyph_count) noexcept {
  return scoped && channel == game::LegacyUiMessageChannel::centered &&
         !has_backdrop && glyph_count != 0U;
}

[[nodiscard]] constexpr bool
useRetailEnglishScopeFont(bool scoped, bool russian_language,
                          game::LegacyUiMessageChannel channel,
                          bool has_backdrop, std::size_t glyph_count) noexcept {
  return russian_language &&
         isRetailScopeMessage(scoped, channel, has_backdrop, glyph_count);
}

} // namespace sf::platform
