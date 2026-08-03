#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace sf::platform {

// The UWP sandbox only gives the runtime unrestricted random access to its
// LocalState folder. External media is therefore imported here before the
// disc reader opens it, rather than keeping a fragile picker permission alive
// while the game streams sectors from the BIN.
struct XboxGameImagePaths final {
    std::filesystem::path local_state;
    std::filesystem::path game_directory;
    std::filesystem::path cue_path;
    std::filesystem::path binary_path;
};

[[nodiscard]] XboxGameImagePaths xboxGameImagePaths(
    const std::filesystem::path& local_state);

// Creates LocalState/Game. It never creates, downloads, or modifies game
// data; the UWP picker and Device Portal are responsible for the import.
void ensureXboxGameImageDirectory(const XboxGameImagePaths& paths);

// Validates a single-BIN CUE and rewrites its FILE entry for the canonical
// LocalState/Game/game.bin layout. The referenced BIN name is returned through
// source_binary_name. Throws std::runtime_error when the CUE is unsupported.
[[nodiscard]] std::string canonicalizeXboxGameCue(
    std::string_view cue_text, std::string& source_binary_name);

// Looks for exactly one CUE/BIN pair already uploaded to LocalState/Game and
// normalizes it to game.cue and game.bin. This permits Device Portal uploads
// to retain their original filenames. Returns false when no safe pair exists.
[[nodiscard]] bool importXboxGameImageFromLocalState(
    const XboxGameImagePaths& paths);

// Returns the canonical CUE only when it resolves to LocalState/Game/game.bin.
// This keeps CUE-relative BIN access inside the UWP sandbox.
[[nodiscard]] std::optional<std::filesystem::path> findXboxGameImage(
    const XboxGameImagePaths& paths) noexcept;

} // namespace sf::platform
