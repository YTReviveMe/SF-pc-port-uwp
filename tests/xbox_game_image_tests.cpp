#include "sf/platform/xbox_game_image.hpp"
#include "test_support.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error{message};
    }
}

void writeTextFile(const std::filesystem::path& path, const char* text) {
    std::ofstream stream{path, std::ios::binary};
    stream << text;
    require(stream.good(), "Cannot create temporary test file");
}

void testCanonicalGameImageLayout() {
    sf::test::TemporaryDirectory temporary{"sf_xbox_game_image_tests"};
    const auto paths = sf::platform::xboxGameImagePaths(temporary.path() / "LocalState");
    sf::platform::ensureXboxGameImageDirectory(paths);

    require(std::filesystem::is_directory(paths.game_directory),
        "Game directory was not created under LocalState");
    writeTextFile(paths.binary_path, "test track");
    writeTextFile(paths.cue_path,
        "FILE \"game.bin\" BINARY\n"
        "  TRACK 01 MODE2/2352\n"
        "    INDEX 01 00:00:00\n");

    const auto discovered = sf::platform::findXboxGameImage(paths);
    require(discovered && *discovered == paths.cue_path,
        "Canonical LocalState game image was not found");
}

void testExternalCueReferenceIsRejected() {
    sf::test::TemporaryDirectory temporary{"sf_xbox_game_image_tests"};
    const auto paths = sf::platform::xboxGameImagePaths(temporary.path() / "LocalState");
    sf::platform::ensureXboxGameImageDirectory(paths);
    writeTextFile(paths.binary_path, "test track");
    writeTextFile(paths.cue_path,
        "FILE \"another.bin\" BINARY\n"
        "  TRACK 01 MODE2/2352\n"
        "    INDEX 01 00:00:00\n");

    require(!sf::platform::findXboxGameImage(paths),
        "CUE that points outside the canonical LocalState image was accepted");
}

void testUploadedLocalStatePairIsCanonicalized() {
    sf::test::TemporaryDirectory temporary{"sf_xbox_game_image_tests"};
    const auto paths = sf::platform::xboxGameImagePaths(temporary.path() / "LocalState");
    sf::platform::ensureXboxGameImageDirectory(paths);

    const auto uploaded_bin = paths.game_directory / "Syphon Filter (USA).bin";
    const auto uploaded_cue = paths.game_directory / "Syphon Filter (USA).cue";
    writeTextFile(uploaded_bin, "test track");
    writeTextFile(uploaded_cue,
        "FILE \"Syphon Filter (USA).bin\" BINARY\n"
        "  TRACK 01 MODE2/2352\n"
        "    INDEX 01 00:00:00\n");

    require(sf::platform::importXboxGameImageFromLocalState(paths),
        "Uploaded LocalState CUE/BIN pair was not canonicalized");
    require(std::filesystem::is_regular_file(paths.binary_path),
        "Uploaded BIN was not moved into the canonical location");
    auto canonical_cue = std::ifstream{paths.cue_path, std::ios::binary};
    const std::string cue_text{std::istreambuf_iterator<char>{canonical_cue}, {}};
    require(cue_text.find("FILE \"game.bin\" BINARY") != std::string::npos,
        "Canonical CUE does not refer to game.bin");

    const auto discovered = sf::platform::findXboxGameImage(paths);
    require(discovered && *discovered == paths.cue_path,
        "Canonicalized LocalState game image was not found");
}

} // namespace

int main() {
    try {
        testCanonicalGameImageLayout();
        testExternalCueReferenceIsRejected();
        testUploadedLocalStatePairIsCanonicalized();
        std::cout << "Xbox game-image tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Xbox game-image tests failed: " << error.what() << '\n';
        return 1;
    }
}
