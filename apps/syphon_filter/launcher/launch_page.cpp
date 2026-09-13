#include "launch_page.hpp"

#ifdef _WIN32

#include <commdlg.h>

#include "text.hpp"
#include "theme.hpp"

#include <algorithm>
#include <array>
#include <cwctype>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace sf::platform::launcher {
namespace {

std::wstring controlText(HWND control) {
  const auto length = GetWindowTextLengthW(control);
  if (length <= 0) {
    return {};
  }
  std::wstring text(static_cast<std::size_t>(length) + 1U, L'\0');
  const auto copied = GetWindowTextW(control, text.data(), length + 1);
  text.resize(copied > 0 ? static_cast<std::size_t>(copied) : 0U);
  return text;
}

bool isCueImage(const std::filesystem::path &path) {
  auto extension = path.extension().wstring();
  std::ranges::transform(extension, extension.begin(), [](wchar_t character) {
    return static_cast<wchar_t>(std::towlower(character));
  });
  return extension == L".cue";
}

std::wstring fileDialogFilter(const LauncherText &text) {
  std::wstring filter;
  const auto append = [&filter](std::wstring_view value) {
    filter.append(value);
    filter.push_back(L'\0');
  };
  append(text.launch.cue_files);
  append(L"*.cue");
  append(text.launch.all_files);
  append(L"*.*");
  filter.push_back(L'\0');
  return filter;
}

void showValidationNotice(HWND owner, std::wstring_view title,
                          std::wstring_view message) noexcept {
  MessageBoxW(owner, message.data(), title.data(), MB_OK | MB_ICONWARNING);
}

} // namespace

struct LaunchPage::Impl {
  HWND parent{};
  HWND image_label{};
  HWND image_edit{};
  HWND browse_button{};
  HWND image_hint{};
  HWND language_label{};
  HWND language_combo{};
  HWND status{};
  std::filesystem::path *cue_path{};
  game::GameLanguage *language{};
  RECT bounds{};
  bool is_visible{};

  [[nodiscard]] std::array<HWND, 7U> controls() const noexcept {
    return {image_label,    image_edit,     browse_button, image_hint,
            language_label, language_combo, status};
  }

  void updatePathFromEdit() {
    *cue_path = std::filesystem::path{controlText(image_edit)};
    updateStatus();
  }

  void updateStatus() {
    const auto &text = textFor(*language);
    if (cue_path->empty()) {
      SetWindowTextW(status, text.launch.no_image_selected.data());
      return;
    }
    auto display_name = cue_path->filename().wstring();
    if (display_name.empty()) {
      display_name = cue_path->wstring();
    }
    auto status_text = std::wstring{text.launch.selected_image_prefix};
    status_text.append(display_name);
    SetWindowTextW(status, status_text.c_str());
  }

  void browse() {
    std::array<wchar_t, 32768U> selected{};
    const auto current = controlText(image_edit);
    if (current.size() < selected.size()) {
      std::copy(current.begin(), current.end(), selected.begin());
    }

    const auto &text = textFor(*language);
    const auto filter = fileDialogFilter(text);
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = parent;
    dialog.lpstrFilter = filter.c_str();
    dialog.lpstrFile = selected.data();
    dialog.nMaxFile = static_cast<DWORD>(selected.size());
    dialog.lpstrTitle = text.launch.game_image.data();
    dialog.lpstrDefExt = L"cue";
    dialog.nFilterIndex = 1U;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY |
                   OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&dialog) == FALSE) {
      return;
    }

    *cue_path = std::filesystem::path{selected.data()};
    SetWindowTextW(image_edit, cue_path->c_str());
    updateStatus();
    SetFocus(image_edit);
    SendMessageW(image_edit, EM_SETSEL, 0, -1);
  }
};

LaunchPage::LaunchPage() = default;

LaunchPage::~LaunchPage() { shutdown(); }

bool LaunchPage::create(HWND parent, const RECT &bounds,
                        const LaunchPageStyle &style,
                        std::filesystem::path &cue_path,
                        game::GameLanguage &language) {
  shutdown();
  if (parent == nullptr) {
    return false;
  }
  try {
    impl_ = std::make_unique<Impl>();
    impl_->parent = parent;
    impl_->cue_path = &cue_path;
    impl_->language = &language;

    impl_->image_label =
        launcher::createControl(parent, L"STATIC", L"", SS_LEFT | SS_NOPREFIX,
                                ControlBounds{}, 0, style.heading_font);
    impl_->image_edit = launcher::createControl(
        parent, L"EDIT", cue_path.c_str(),
        WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, ControlBounds{},
        game_image_control_id, style.ui_font);
    impl_->browse_button = launcher::createControl(
        parent, L"BUTTON", L"", WS_TABSTOP | BS_PUSHBUTTON | BS_FLAT,
        ControlBounds{}, browse_image_control_id, style.heading_font);
    impl_->image_hint =
        launcher::createControl(parent, L"STATIC", L"", SS_LEFT | SS_NOPREFIX,
                                ControlBounds{}, 0, style.ui_font);
    impl_->language_label =
        launcher::createControl(parent, L"STATIC", L"", SS_LEFT | SS_NOPREFIX,
                                ControlBounds{}, 0, style.heading_font);
    impl_->language_combo = launcher::createControl(
        parent, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        ControlBounds{}, language_control_id, style.ui_font);
    impl_->status =
        launcher::createControl(parent, L"STATIC", L"", SS_LEFT | SS_NOPREFIX,
                                ControlBounds{}, 0, style.ui_font);

    for (const auto control : impl_->controls()) {
      if (control == nullptr) {
        shutdown();
        return false;
      }
    }
    layout(bounds);
    SendMessageW(impl_->image_edit, EM_SETLIMITTEXT, 32767U, 0);
    setLanguage(language);
    hide();
    return true;
  } catch (...) {
    shutdown();
    return false;
  }
}

void LaunchPage::show() noexcept {
  if (impl_ == nullptr) {
    return;
  }
  impl_->is_visible = true;
  for (const auto control : impl_->controls()) {
    ShowWindow(control, SW_SHOW);
  }
  InvalidateRect(impl_->parent, &impl_->bounds, FALSE);
}

void LaunchPage::hide() noexcept {
  if (impl_ == nullptr) {
    return;
  }
  impl_->is_visible = false;
  for (const auto control : impl_->controls()) {
    ShowWindow(control, SW_HIDE);
  }
  InvalidateRect(impl_->parent, &impl_->bounds, FALSE);
}

void LaunchPage::layout(const RECT &bounds) noexcept {
  if (impl_ == nullptr) {
    return;
  }
  impl_->bounds = bounds;
  constexpr LONG padding = 24;
  constexpr LONG browse_width = 112;
  constexpr LONG control_height = 28;
  const auto left = bounds.left + padding;
  const auto right = std::max(left + 260L, bounds.right - padding);
  const auto content_width = right - left;
  const auto browse_x = right - browse_width;
  const auto edit_width = std::max(120L, browse_x - left - 12L);

  MoveWindow(impl_->image_label, left, bounds.top + 22, content_width, 24,
             TRUE);
  MoveWindow(impl_->image_edit, left, bounds.top + 52, edit_width,
             control_height, TRUE);
  MoveWindow(impl_->browse_button, browse_x, bounds.top + 49, browse_width, 34,
             TRUE);
  MoveWindow(impl_->image_hint, left, bounds.top + 90, content_width, 48, TRUE);
  MoveWindow(impl_->language_label, left, bounds.top + 156, content_width, 24,
             TRUE);
  MoveWindow(impl_->language_combo, left, bounds.top + 186,
             std::min(280L, content_width), 180, TRUE);
  MoveWindow(impl_->status, left, bounds.top + 240, content_width, 48, TRUE);
}

void LaunchPage::setLanguage(game::GameLanguage language) {
  if (impl_ == nullptr) {
    return;
  }
  *impl_->language = language;
  const auto &text = textFor(language);
  SetWindowTextW(impl_->image_label, text.launch.game_image.data());
  SetWindowTextW(impl_->browse_button, text.shell.browse.data());
  SetWindowTextW(impl_->image_hint, text.launch.game_image_hint.data());
  SetWindowTextW(impl_->language_label, text.launch.text_language.data());

  SendMessageW(impl_->language_combo, CB_RESETCONTENT, 0, 0);
  SendMessageW(impl_->language_combo, CB_ADDSTRING, 0,
               reinterpret_cast<LPARAM>(text.launch.english_language.data()));
  SendMessageW(impl_->language_combo, CB_ADDSTRING, 0,
               reinterpret_cast<LPARAM>(text.launch.russian_language.data()));
  SendMessageW(impl_->language_combo, CB_SETCURSEL,
               language == game::GameLanguage::russian_vit ? 1 : 0, 0);
  impl_->updateStatus();
  InvalidateRect(impl_->parent, &impl_->bounds, FALSE);
}

bool LaunchPage::handleCommand(WPARAM w_param, LPARAM l_param) {
  if (impl_ == nullptr) {
    return false;
  }
  const auto id = LOWORD(w_param);
  const auto notification = HIWORD(w_param);
  const auto source = reinterpret_cast<HWND>(l_param);
  if (id == browse_image_control_id && notification == BN_CLICKED &&
      source == impl_->browse_button) {
    impl_->browse();
    return true;
  }
  if (id == game_image_control_id && notification == EN_CHANGE &&
      source == impl_->image_edit) {
    impl_->updatePathFromEdit();
    return true;
  }
  if (id == language_control_id && notification == CBN_SELCHANGE &&
      source == impl_->language_combo) {
    const auto selection = static_cast<int>(
        SendMessageW(impl_->language_combo, CB_GETCURSEL, 0, 0));
    setLanguage(selection == 1 ? game::GameLanguage::russian_vit
                               : game::GameLanguage::english);
    return true;
  }
  return false;
}

bool LaunchPage::validate(HWND owner) {
  if (impl_ == nullptr) {
    return false;
  }
  impl_->updatePathFromEdit();
  const auto &text = textFor(*impl_->language);
  std::error_code error;
  if (impl_->cue_path->empty() || !isCueImage(*impl_->cue_path) ||
      !std::filesystem::is_regular_file(*impl_->cue_path, error) || error) {
    showValidationNotice(owner != nullptr ? owner : impl_->parent,
                         text.validation.disc_image_required_title,
                         text.validation.disc_image_required_message);
    SetFocus(impl_->image_edit);
    SendMessageW(impl_->image_edit, EM_SETSEL, 0, -1);
    return false;
  }

  auto absolute_path = std::filesystem::absolute(*impl_->cue_path, error);
  if (!error) {
    *impl_->cue_path = std::move(absolute_path);
    SetWindowTextW(impl_->image_edit, impl_->cue_path->c_str());
    impl_->updateStatus();
  }

  if (!game::localizationPackAvailable(*impl_->language)) {
    showValidationNotice(owner != nullptr ? owner : impl_->parent,
                         text.validation.language_pack_missing_title,
                         text.validation.language_pack_missing_message);
    SetFocus(impl_->language_combo);
    return false;
  }
  return true;
}

void LaunchPage::shutdown() noexcept {
  if (impl_ == nullptr) {
    return;
  }
  for (const auto control : impl_->controls()) {
    if (control != nullptr && IsWindow(control) != FALSE) {
      DestroyWindow(control);
    }
  }
  impl_.reset();
}

bool LaunchPage::visible() const noexcept {
  return impl_ != nullptr && impl_->is_visible;
}

} // namespace sf::platform::launcher

#endif
