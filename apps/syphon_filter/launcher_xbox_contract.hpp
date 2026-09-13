#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>

namespace sf::game {
enum class GameLanguage : std::uint8_t;
struct ControllerButtonBindings;
}

namespace sf::platform {
struct GraphicsSettings;
struct KeyboardMouseBindings;

void loadLauncherSettings(
    GraphicsSettings& graphics,
    KeyboardMouseBindings& input,
    game::GameLanguage& language) noexcept;

[[nodiscard]] bool saveLauncherControllerSettings(
    const game::ControllerButtonBindings& bindings,
    bool vibration) noexcept;

[[nodiscard]] bool showLauncher(
    GraphicsSettings& settings,
    KeyboardMouseBindings& input,
    game::GameLanguage& language,
    std::filesystem::path& cue_path);

[[nodiscard]] bool retailCheatMarkerExists() noexcept;

void showLauncherError(std::string_view title, std::string_view message) noexcept;
} // namespace sf::platform
