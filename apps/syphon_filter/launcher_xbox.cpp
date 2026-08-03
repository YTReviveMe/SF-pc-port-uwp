#include "launcher_xbox_contract.hpp"

#include "sf/platform/xbox_game_image.hpp"

#include <SDL.h>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.UI.Core.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace sf::platform {
namespace {

using winrt::Windows::Storage::ApplicationData;
using winrt::Windows::Storage::CreationCollisionOption;
using winrt::Windows::Storage::FileIO;
using winrt::Windows::Storage::NameCollisionOption;
using winrt::Windows::Storage::StorageFile;
using winrt::Windows::Storage::StorageFolder;
using winrt::Windows::Storage::Pickers::FolderPicker;
using winrt::Windows::Storage::Pickers::PickerLocationId;
using winrt::Windows::Storage::Pickers::PickerViewMode;
using winrt::Windows::UI::Core::CoreProcessEventsOption;
using winrt::Windows::UI::Core::CoreWindow;

struct ImportState final {
  std::atomic<bool> finished{};
  std::atomic<bool> succeeded{};
  std::string error;
};

std::filesystem::path localStatePath() {
  return std::filesystem::path{
      winrt::to_string(ApplicationData::Current().LocalFolder().Path())};
}

winrt::fire_and_forget importExternalGameImageAsync(
    std::shared_ptr<ImportState> state, XboxGameImagePaths paths) {
  try {
    FolderPicker picker;
    picker.ViewMode(PickerViewMode::List);
    picker.SuggestedStartLocation(PickerLocationId::ComputerFolder);
    picker.FileTypeFilter().Append(L"*");

    const StorageFolder source_folder = co_await picker.PickSingleFolderAsync();
    if (!source_folder) {
      state->error = "Game-image folder selection was cancelled";
      state->finished.store(true, std::memory_order_release);
      co_return;
    }

    const auto files = co_await source_folder.GetFilesAsync();
    StorageFile source_cue{nullptr};
    for (const auto &file : files) {
      const auto extension =
          std::filesystem::path{file.Name().c_str()}.extension().wstring();
      if (_wcsicmp(extension.c_str(), L".cue") == 0) {
        if (source_cue) {
          throw std::runtime_error{
              "The selected folder contains more than one CUE file"};
        }
        source_cue = file;
      }
    }
    if (!source_cue) {
      throw std::runtime_error{"No CUE file was found in the selected folder"};
    }

    std::string source_binary_name;
    const auto canonical_cue = canonicalizeXboxGameCue(
        winrt::to_string(co_await FileIO::ReadTextAsync(source_cue)),
        source_binary_name);
    const auto source_binary = co_await source_folder.GetFileAsync(
        winrt::to_hstring(source_binary_name));

    const auto local_folder = ApplicationData::Current().LocalFolder();
    const auto game_folder = co_await local_folder.CreateFolderAsync(
        L"Game", CreationCollisionOption::OpenIfExists);
    static_cast<void>(co_await source_binary.CopyAsync(
        game_folder, L"game.bin", NameCollisionOption::ReplaceExisting));
    const auto cue_file = co_await game_folder.CreateFileAsync(
        L"game.cue", CreationCollisionOption::ReplaceExisting);
    co_await FileIO::WriteTextAsync(cue_file, winrt::to_hstring(canonical_cue));

    if (!findXboxGameImage(paths)) {
      throw std::runtime_error{
          "Imported files do not form a supported single-BIN CUE layout"};
    }
    state->succeeded.store(true, std::memory_order_release);
  } catch (const winrt::hresult_error &error) {
    state->error = winrt::to_string(error.message());
  } catch (const std::exception &error) {
    state->error = error.what();
  }
  state->finished.store(true, std::memory_order_release);
}

bool importExternalGameImage(const XboxGameImagePaths &paths) {
  const auto state = std::make_shared<ImportState>();
  if (SDL_Init(SDL_INIT_EVENTS) != 0) {
    return false;
  }

  // Picker and brokered-storage continuations resume through the UWP UI
  // dispatcher while SDL_main is waiting for the import to complete.
  const auto core_window = CoreWindow::GetForCurrentThread();
  if (!core_window) {
    return false;
  }
  const auto dispatcher = core_window.Dispatcher();
  importExternalGameImageAsync(state, paths);
  while (!state->finished.load(std::memory_order_acquire)) {
    dispatcher.ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
    SDL_PumpEvents();
    SDL_Delay(16U);
  }
  const auto succeeded = state->succeeded.load(std::memory_order_acquire);
  if (!succeeded && !state->error.empty()) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Xbox game-image import: %s",
                 state->error.c_str());
    std::ofstream error_file{paths.game_directory / "import-error.txt",
                             std::ios::out | std::ios::trunc};
    error_file << state->error << '\n';
  }
  return succeeded;
}

} // namespace

void loadLauncherSettings(
    GraphicsSettings &, KeyboardMouseBindings &, game::GameLanguage &) noexcept {}

bool showGraphicsLauncher(
    GraphicsSettings &, KeyboardMouseBindings &, game::GameLanguage &,
    std::filesystem::path &cue_path) {
  const auto paths = xboxGameImagePaths(localStatePath());
  ensureXboxGameImageDirectory(paths);
  if (const auto existing = findXboxGameImage(paths)) {
    cue_path = *existing;
    return true;
  }

  if (importXboxGameImageFromLocalState(paths)) {
    cue_path = paths.cue_path;
    return true;
  }

  if (!importExternalGameImage(paths)) {
    return false;
  }
  const auto imported = findXboxGameImage(paths);
  if (!imported) {
    return false;
  }
  cue_path = *imported;
  return true;
}

bool retailCheatMarkerExists() noexcept {
  return false;
}

void showLauncherError(std::string_view title, std::string_view message) noexcept {
  SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%.*s: %.*s",
               static_cast<int>(title.size()), title.data(),
               static_cast<int>(message.size()), message.data());
}

} // namespace sf::platform
