#include "sf/platform/xbox_game_image.hpp"

#include "sf/core/error.hpp"
#include "sf/disc/cue_sheet.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <regex>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace sf::platform {

namespace {

bool hasCueExtension(const std::filesystem::path& path) {
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return extension == ".cue";
}

std::optional<std::filesystem::path> findSingleCueFile(
    const std::filesystem::path& directory) {
    std::error_code error;
    std::optional<std::filesystem::path> cue_path;
    for (std::filesystem::directory_iterator iterator{directory, error}, end;
         !error && iterator != end; iterator.increment(error)) {
        std::error_code file_error;
        if (!iterator->is_regular_file(file_error) || file_error ||
            !hasCueExtension(iterator->path())) {
            continue;
        }
        if (cue_path) {
            return std::nullopt;
        }
        cue_path = iterator->path();
    }
    if (error) {
        return std::nullopt;
    }
    return cue_path;
}

} // namespace

XboxGameImagePaths xboxGameImagePaths(const std::filesystem::path& local_state) {
    const auto game_directory = local_state / "Game";
    return XboxGameImagePaths{
        .local_state = local_state,
        .game_directory = game_directory,
        .cue_path = game_directory / "game.cue",
        .binary_path = game_directory / "game.bin",
    };
}

void ensureXboxGameImageDirectory(const XboxGameImagePaths& paths) {
    std::error_code error;
    std::filesystem::create_directories(paths.game_directory, error);
    if (error) {
        throw core::Error{
            core::ErrorCode::io,
            "Cannot create Xbox LocalState game directory: " + error.message()};
    }
}

std::string canonicalizeXboxGameCue(std::string_view cue_text,
                                    std::string& source_binary_name) {
    const std::regex file_pattern{
        R"(^\s*FILE\s+\"([^\"]+)\"\s+BINARY\s*$)",
        std::regex_constants::icase};
    const std::string source{cue_text};
    const std::sregex_iterator first{source.begin(), source.end(), file_pattern};
    const std::sregex_iterator last;
    if (first == last || std::next(first) != last) {
        throw std::runtime_error{
            "The selected CUE must describe exactly one BIN track"};
    }

    const std::filesystem::path source_name{(*first)[1].str()};
    if (source_name.empty() || source_name.has_parent_path() ||
        source_name.filename() != source_name) {
        throw std::runtime_error{
            "The selected CUE references a BIN outside its folder"};
    }
    source_binary_name = source_name.string();
    return std::regex_replace(source, file_pattern, "FILE \"game.bin\" BINARY");
}

bool importXboxGameImageFromLocalState(const XboxGameImagePaths& paths) {
    try {
        const auto source_cue = findSingleCueFile(paths.game_directory);
        if (!source_cue) {
            return false;
        }

        std::ifstream input{*source_cue, std::ios::binary};
        const std::string source_text{std::istreambuf_iterator<char>{input}, {}};
        if (!input.good() && !input.eof()) {
            return false;
        }

        std::string source_binary_name;
        const auto canonical_cue =
            canonicalizeXboxGameCue(source_text, source_binary_name);
        const auto source_binary = paths.game_directory / source_binary_name;
        std::error_code binary_error;
        if (!std::filesystem::is_regular_file(source_binary, binary_error) ||
            binary_error) {
            return false;
        }

        if (source_binary.lexically_normal() !=
            paths.binary_path.lexically_normal()) {
            std::error_code destination_error;
            if (std::filesystem::exists(paths.binary_path, destination_error) ||
                destination_error) {
                return false;
            }
            std::error_code rename_error;
            std::filesystem::rename(source_binary, paths.binary_path, rename_error);
            if (rename_error) {
                return false;
            }
        }

        std::ofstream output{paths.cue_path,
                             std::ios::binary | std::ios::trunc};
        output << canonical_cue;
        if (!output.good()) {
            return false;
        }
        output.close();
        return findXboxGameImage(paths).has_value();
    } catch (...) {
        return false;
    }
}

std::optional<std::filesystem::path> findXboxGameImage(
    const XboxGameImagePaths& paths) noexcept {
    try {
        std::error_code cue_error;
        if (!std::filesystem::is_regular_file(paths.cue_path, cue_error) || cue_error) {
            return std::nullopt;
        }

        const auto track = disc::CueSheet::load(paths.cue_path).dataTrack();
        std::error_code binary_error;
        if (!std::filesystem::is_regular_file(paths.binary_path, binary_error) || binary_error ||
            track.binary_path.lexically_normal() != paths.binary_path.lexically_normal()) {
            return std::nullopt;
        }
        return paths.cue_path;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace sf::platform
