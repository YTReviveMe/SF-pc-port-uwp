#pragma once

#include "sf/game/localization.hpp"
#include "sf/platform/host.hpp"

#include <filesystem>

namespace sf::platform::launcher_settings {

[[nodiscard]] std::filesystem::path executableDirectory();
[[nodiscard]] std::filesystem::path loadGameImagePath();

void loadSettingsFile(GraphicsSettings &graphics, KeyboardMouseBindings &input,
                      game::GameLanguage &language);

[[nodiscard]] bool saveSettingsFile(const GraphicsSettings &graphics,
                                    const KeyboardMouseBindings &input,
                                    game::GameLanguage language,
                                    const std::filesystem::path &cue_path);

[[nodiscard]] bool
saveControllerSettingsFile(const ControllerButtonBindings &bindings,
                           bool vibration);

[[nodiscard]] bool cheatMarkerExists() noexcept;

} // namespace sf::platform::launcher_settings
