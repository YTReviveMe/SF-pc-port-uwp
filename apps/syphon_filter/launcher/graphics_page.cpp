#include "graphics_page.hpp"

#ifdef _WIN32

#include "text.hpp"
#include "theme.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cwctype>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sf::platform::launcher {
namespace {

constexpr int minimum_resolution_width = 320;
constexpr int minimum_resolution_height = 240;

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

void setControlText(HWND control, std::wstring_view text) {
  if (control == nullptr) {
    return;
  }
  const auto owned = std::wstring{text};
  SetWindowTextW(control, owned.c_str());
}

std::optional<int> parseResolutionDimension(std::wstring_view text) noexcept {
  if (text.empty()) {
    return std::nullopt;
  }
  int value{};
  for (const auto character : text) {
    if (character < L'0' || character > L'9') {
      return std::nullopt;
    }
    const auto digit = static_cast<int>(character - L'0');
    if (value > (std::numeric_limits<int>::max() - digit) / 10) {
      return std::nullopt;
    }
    value = value * 10 + digit;
  }
  return value;
}

std::optional<std::pair<int, int>> parseResolution(std::wstring_view text) {
  std::wstring compact;
  compact.reserve(text.size());
  for (const auto character : text) {
    if (std::iswspace(character) == 0) {
      compact.push_back(character);
    }
  }

  const auto separator = compact.find_first_of(L"xX\u00d7");
  if (separator == std::wstring::npos ||
      compact.find_first_of(L"xX\u00d7", separator + 1U) !=
          std::wstring::npos) {
    return std::nullopt;
  }
  const auto width = parseResolutionDimension(
      std::wstring_view{compact}.substr(0U, separator));
  const auto height = parseResolutionDimension(
      std::wstring_view{compact}.substr(separator + 1U));
  if (!width || !height || *width < minimum_resolution_width ||
      *height < minimum_resolution_height) {
    return std::nullopt;
  }
  return std::pair{*width, *height};
}

bool checked(HWND parent, int control_id) noexcept {
  return IsDlgButtonChecked(parent, control_id) == BST_CHECKED;
}

void setChecked(HWND parent, int control_id, bool value) noexcept {
  CheckDlgButton(parent, control_id, value ? BST_CHECKED : BST_UNCHECKED);
}

void moveControl(HWND control, int x, int y, int width, int height) noexcept {
  if (control != nullptr) {
    MoveWindow(control, x, y, std::max(width, 0), std::max(height, 0), TRUE);
  }
}

} // namespace

struct GraphicsPage::Impl {
  GraphicsSettings *settings{};
  HWND parent{};
  GraphicsPageStyle style{};
  game::GameLanguage language{game::GameLanguage::english};
  bool created{};
  bool is_visible{};

  HWND resolution_label{};
  HWND resolution_combo{};
  HWND aspect_label{};
  HWND aspect_combo{};
  HWND antialiasing_label{};
  HWND antialiasing_combo{};
  HWND frame_limit_label{};
  HWND frame_limit_combo{};
  HWND bilinear_check{};
  HWND trilinear_check{};
  HWND anisotropic_check{};
  HWND volumetric_effects_check{};
  HWND mission_skyboxes_check{};
  HWND vsync_check{};
  HWND fullscreen_check{};

  std::vector<std::pair<int, int>> resolutions;
  std::vector<std::uint32_t> frame_limits;
  std::vector<HWND> controls;

  HWND add(const wchar_t *class_name, DWORD control_style, int control_id,
           HFONT font = nullptr) {
    const auto control = launcher::createControl(
        parent, class_name, L"", control_style, ControlBounds{}, control_id,
        font != nullptr ? font : style.ui_font);
    if (control != nullptr) {
      controls.push_back(control);
    }
    return control;
  }

  bool allCreated() const noexcept {
    return resolution_label != nullptr && resolution_combo != nullptr &&
           aspect_label != nullptr && aspect_combo != nullptr &&
           antialiasing_label != nullptr && antialiasing_combo != nullptr &&
           frame_limit_label != nullptr && frame_limit_combo != nullptr &&
           bilinear_check != nullptr &&
           trilinear_check != nullptr && anisotropic_check != nullptr &&
           volumetric_effects_check != nullptr &&
           mission_skyboxes_check != nullptr && vsync_check != nullptr &&
           fullscreen_check != nullptr;
  }

  [[nodiscard]] bool isCommandSource(int control_id,
                                     HWND source) const noexcept {
    if (source == nullptr) {
      return false;
    }
    switch (control_id) {
    case resolution_control_id:
      return source == resolution_combo;
    case aspect_control_id:
      return source == aspect_combo;
    case antialiasing_control_id:
      return source == antialiasing_combo;
    case frame_limit_control_id:
      return source == frame_limit_combo;
    case bilinear_control_id:
      return source == bilinear_check;
    case trilinear_control_id:
      return source == trilinear_check;
    case anisotropic_control_id:
      return source == anisotropic_check;
    case volumetric_effects_control_id:
      return source == volumetric_effects_check;
    case mission_skyboxes_control_id:
      return source == mission_skyboxes_check;
    case vsync_control_id:
      return source == vsync_check;
    case fullscreen_control_id:
      return source == fullscreen_check;
    default:
      return false;
    }
  }

  void populateResolutions() {
    std::set<std::pair<int, int>> unique;
    for (DWORD index = 0;; ++index) {
      DEVMODEW mode{};
      mode.dmSize = sizeof(mode);
      if (EnumDisplaySettingsW(nullptr, index, &mode) == FALSE) {
        break;
      }
      if (mode.dmPelsWidth >= minimum_resolution_width &&
          mode.dmPelsHeight >= minimum_resolution_height) {
        unique.emplace(static_cast<int>(mode.dmPelsWidth),
                       static_cast<int>(mode.dmPelsHeight));
      }
    }
    unique.emplace(640, 480);
    unique.emplace(settings->width, settings->height);
    resolutions.assign(unique.begin(), unique.end());
    std::ranges::sort(resolutions, [](const auto &left, const auto &right) {
      const auto left_area = static_cast<long long>(left.first) * left.second;
      const auto right_area =
          static_cast<long long>(right.first) * right.second;
      return left_area == right_area ? left < right : left_area < right_area;
    });

    SendMessageW(resolution_combo, CB_RESETCONTENT, 0, 0);
    int selected{};
    for (std::size_t index = 0; index < resolutions.size(); ++index) {
      const auto [width, height] = resolutions[index];
      const auto label =
          std::to_wstring(width) + L" x " + std::to_wstring(height);
      SendMessageW(resolution_combo, CB_ADDSTRING, 0,
                   reinterpret_cast<LPARAM>(label.c_str()));
      if (width == settings->width && height == settings->height) {
        selected = static_cast<int>(index);
      }
    }
    SendMessageW(resolution_combo, CB_SETCURSEL, static_cast<WPARAM>(selected),
                 0);
  }

  void buildFrameLimits() {
    constexpr std::array<std::uint32_t, 9U> standard{
        0U, 30U, 60U, 72U, 90U, 120U, 144U, 165U, 240U,
    };
    frame_limits.assign(standard.begin(), standard.end());
    if (std::ranges::find(frame_limits, settings->frame_limit) ==
        frame_limits.end()) {
      frame_limits.push_back(settings->frame_limit);
      std::ranges::sort(frame_limits);
    }
  }

  int initialAntialiasingIndex() const noexcept {
    if (settings->smaa) {
      return 1;
    }
    switch (settings->msaa_samples) {
    case 2:
      return 2;
    case 4:
      return 3;
    case 8:
      return 4;
    default:
      return 0;
    }
  }

  int initialFrameLimitIndex() const noexcept {
    const auto found = std::ranges::find(frame_limits, settings->frame_limit);
    return found == frame_limits.end()
               ? 0
               : static_cast<int>(std::distance(frame_limits.begin(), found));
  }

  void populateAspect(int selected) {
    const auto &strings = textFor(language).graphics;
    SendMessageW(aspect_combo, CB_RESETCONTENT, 0, 0);
    SendMessageW(aspect_combo, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(strings.adaptive_aspect.data()));
    SendMessageW(aspect_combo, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(strings.original_aspect.data()));
    SendMessageW(aspect_combo, CB_SETCURSEL,
                 static_cast<WPARAM>(std::clamp(selected, 0, 1)), 0);
  }

  void populateAntialiasing(int selected) {
    const auto &strings = textFor(language).graphics;
    const std::array labels{
        strings.disabled, strings.smaa_ultra, strings.msaa_2x,
        strings.msaa_4x,  strings.msaa_8x,
    };
    SendMessageW(antialiasing_combo, CB_RESETCONTENT, 0, 0);
    for (const auto label : labels) {
      SendMessageW(antialiasing_combo, CB_ADDSTRING, 0,
                   reinterpret_cast<LPARAM>(label.data()));
    }
    SendMessageW(antialiasing_combo, CB_SETCURSEL,
                 static_cast<WPARAM>(std::clamp(selected, 0, 4)), 0);
  }

  void populateFrameLimits(int selected) {
    const auto &strings = textFor(language).graphics;
    SendMessageW(frame_limit_combo, CB_RESETCONTENT, 0, 0);
    for (const auto limit : frame_limits) {
      const auto label = limit == 0U ? std::wstring{strings.unlimited}
                                     : std::to_wstring(limit) +
                                           std::wstring{strings.fps_suffix};
      SendMessageW(frame_limit_combo, CB_ADDSTRING, 0,
                   reinterpret_cast<LPARAM>(label.c_str()));
    }
    const auto maximum =
        frame_limits.empty() ? 0 : static_cast<int>(frame_limits.size() - 1U);
    SendMessageW(frame_limit_combo, CB_SETCURSEL,
                 static_cast<WPARAM>(std::clamp(selected, 0, maximum)), 0);
  }

  void applyLanguage(game::GameLanguage new_language) {
    auto aspect_index =
        static_cast<int>(SendMessageW(aspect_combo, CB_GETCURSEL, 0, 0));
    if (aspect_index < 0) {
      aspect_index =
          settings->aspect_ratio == AspectRatioMode::adaptive ? 0 : 1;
    }
    auto antialiasing_index =
        static_cast<int>(SendMessageW(antialiasing_combo, CB_GETCURSEL, 0, 0));
    if (antialiasing_index < 0) {
      antialiasing_index = initialAntialiasingIndex();
    }
    auto frame_limit_index =
        static_cast<int>(SendMessageW(frame_limit_combo, CB_GETCURSEL, 0, 0));
    if (frame_limit_index < 0) {
      frame_limit_index = initialFrameLimitIndex();
    }

    language = new_language;
    const auto &strings = textFor(language).graphics;
    setControlText(resolution_label, strings.resolution);
    setControlText(aspect_label, strings.aspect_ratio);
    setControlText(antialiasing_label, strings.antialiasing);
    setControlText(frame_limit_label, strings.frame_limit);
    setControlText(bilinear_check, strings.bilinear_filtering);
    setControlText(trilinear_check, strings.trilinear_filtering);
    setControlText(anisotropic_check, strings.anisotropic_filtering);
    setControlText(volumetric_effects_check, strings.volumetric_effects);
    setControlText(mission_skyboxes_check, strings.mission_skyboxes);
    setControlText(vsync_check, strings.vertical_sync);
    setControlText(fullscreen_check, strings.borderless_fullscreen);
    populateAspect(aspect_index);
    populateAntialiasing(antialiasing_index);
    populateFrameLimits(frame_limit_index);
  }

  void enforceFilterDependencies() noexcept {
    const auto bilinear = checked(parent, bilinear_control_id);
    if (!bilinear) {
      setChecked(parent, trilinear_control_id, false);
      setChecked(parent, anisotropic_control_id, false);
    }
    EnableWindow(trilinear_check, bilinear ? TRUE : FALSE);
    EnableWindow(anisotropic_check, bilinear ? TRUE : FALSE);
  }
};

GraphicsPage::GraphicsPage() : impl_(std::make_unique<Impl>()) {}

GraphicsPage::~GraphicsPage() { shutdown(); }

bool GraphicsPage::create(HWND parent, const RECT &bounds,
                          const GraphicsPageStyle &style,
                          GraphicsSettings &settings,
                          game::GameLanguage language) {
  shutdown();
  try {
    if (parent == nullptr) {
      return false;
    }

    impl_->parent = parent;
    impl_->style = style;
    impl_->settings = &settings;
    impl_->language = language;

    impl_->resolution_label = impl_->add(L"STATIC", SS_LEFT | SS_NOPREFIX, 0);
    impl_->resolution_combo = impl_->add(
        L"COMBOBOX", WS_TABSTOP | CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL,
        resolution_control_id);
    impl_->aspect_label = impl_->add(L"STATIC", SS_LEFT | SS_NOPREFIX, 0);
    impl_->aspect_combo =
        impl_->add(L"COMBOBOX", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                   aspect_control_id);
    impl_->antialiasing_label = impl_->add(L"STATIC", SS_LEFT | SS_NOPREFIX, 0);
    impl_->antialiasing_combo =
        impl_->add(L"COMBOBOX", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                   antialiasing_control_id);
    impl_->frame_limit_label = impl_->add(L"STATIC", SS_LEFT | SS_NOPREFIX, 0);
    impl_->frame_limit_combo =
        impl_->add(L"COMBOBOX", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                   frame_limit_control_id);
    impl_->bilinear_check = impl_->add(L"BUTTON", WS_TABSTOP | BS_AUTOCHECKBOX,
                                       bilinear_control_id);
    impl_->trilinear_check = impl_->add(L"BUTTON", WS_TABSTOP | BS_AUTOCHECKBOX,
                                        trilinear_control_id);
    impl_->anisotropic_check = impl_->add(
        L"BUTTON", WS_TABSTOP | BS_AUTOCHECKBOX, anisotropic_control_id);
    impl_->volumetric_effects_check = impl_->add(
        L"BUTTON", WS_TABSTOP | BS_AUTOCHECKBOX, volumetric_effects_control_id);
    impl_->mission_skyboxes_check = impl_->add(
        L"BUTTON", WS_TABSTOP | BS_AUTOCHECKBOX, mission_skyboxes_control_id);
    impl_->vsync_check =
        impl_->add(L"BUTTON", WS_TABSTOP | BS_AUTOCHECKBOX, vsync_control_id);
    impl_->fullscreen_check = impl_->add(
        L"BUTTON", WS_TABSTOP | BS_AUTOCHECKBOX, fullscreen_control_id);

    if (!impl_->allCreated()) {
      shutdown();
      return false;
    }
    SendMessageW(impl_->resolution_combo, CB_LIMITTEXT, 31U, 0);

    impl_->created = true;
    impl_->is_visible = true;
    impl_->populateResolutions();
    impl_->buildFrameLimits();

    const auto filtered = settings.bilinear_filtering ||
                          settings.trilinear_filtering ||
                          settings.anisotropic_filtering;
    setChecked(parent, bilinear_control_id, filtered);
    setChecked(parent, trilinear_control_id, settings.trilinear_filtering);
    setChecked(parent, anisotropic_control_id, settings.anisotropic_filtering);
    setChecked(parent, volumetric_effects_control_id,
               settings.volumetric_effects);
    setChecked(parent, mission_skyboxes_control_id,
               settings.mission_skyboxes);
    setChecked(parent, vsync_control_id, settings.vsync);
    setChecked(parent, fullscreen_control_id, settings.fullscreen);

    impl_->applyLanguage(language);
    impl_->enforceFilterDependencies();
    layout(bounds);
    return true;
  } catch (...) {
    shutdown();
    return false;
  }
}

void GraphicsPage::show() noexcept {
  if (!impl_->created) {
    return;
  }
  impl_->is_visible = true;
  for (const auto control : impl_->controls) {
    ShowWindow(control, SW_SHOW);
  }
}

void GraphicsPage::hide() noexcept {
  impl_->is_visible = false;
  for (const auto control : impl_->controls) {
    ShowWindow(control, SW_HIDE);
  }
}

void GraphicsPage::layout(const RECT &bounds) noexcept {
  if (!impl_->created) {
    return;
  }
  constexpr int padding = 24;
  constexpr int column_gap = 32;
  constexpr int label_height = 20;
  constexpr int combo_height = 180;
  constexpr int check_height = 28;
  constexpr int check_gap = 10;

  const auto available_width =
      std::max(0L, bounds.right - bounds.left - padding * 2 - column_gap);
  const auto column_width =
      std::max(220, static_cast<int>(available_width / 2));
  const auto left = static_cast<int>(bounds.left) + padding;
  const auto right = left + column_width + column_gap;
  const auto top = static_cast<int>(bounds.top) + 16;

  moveControl(impl_->resolution_label, left, top, column_width, label_height);
  moveControl(impl_->resolution_combo, left, top + 22, column_width,
              combo_height);
  moveControl(impl_->aspect_label, left, top + 68, column_width, label_height);
  moveControl(impl_->aspect_combo, left, top + 90, column_width, combo_height);
  moveControl(impl_->antialiasing_label, left, top + 136, column_width,
              label_height);
  moveControl(impl_->antialiasing_combo, left, top + 158, column_width,
              combo_height);
  moveControl(impl_->frame_limit_label, left, top + 204, column_width,
              label_height);
  moveControl(impl_->frame_limit_combo, left, top + 226, column_width,
              combo_height);

  const std::array checks{
      impl_->bilinear_check,    impl_->trilinear_check,
      impl_->anisotropic_check, impl_->volumetric_effects_check,
      impl_->mission_skyboxes_check, impl_->vsync_check,
      impl_->fullscreen_check,
  };
  for (std::size_t index = 0; index < checks.size(); ++index) {
    moveControl(checks[index], right,
                top + static_cast<int>(index) * (check_height + check_gap),
                column_width, check_height);
  }
}

void GraphicsPage::setLanguage(game::GameLanguage language) {
  if (!impl_->created) {
    impl_->language = language;
    return;
  }
  impl_->applyLanguage(language);
}

bool GraphicsPage::handleCommand(WPARAM w_param, LPARAM l_param) {
  if (!impl_->created) {
    return false;
  }

  const auto control_id = static_cast<int>(LOWORD(w_param));
  const auto source = reinterpret_cast<HWND>(l_param);
  if (!impl_->isCommandSource(control_id, source)) {
    return false;
  }
  const auto notification = HIWORD(w_param);
  if (notification == BN_CLICKED && (control_id == bilinear_control_id ||
                                     control_id == trilinear_control_id ||
                                     control_id == anisotropic_control_id)) {
    impl_->enforceFilterDependencies();
    return true;
  }
  if ((control_id == resolution_control_id &&
       (notification == CBN_SELCHANGE || notification == CBN_EDITCHANGE)) ||
      ((control_id == aspect_control_id ||
        control_id == antialiasing_control_id ||
        control_id == frame_limit_control_id) &&
       notification == CBN_SELCHANGE)) {
    return true;
  }
  if (notification == BN_CLICKED &&
      (control_id == volumetric_effects_control_id ||
       control_id == mission_skyboxes_control_id ||
       control_id == vsync_control_id || control_id == fullscreen_control_id)) {
    return true;
  }
  return false;
}

bool GraphicsPage::validateAndCommit(HWND owner) {
  if (!impl_->created || impl_->settings == nullptr) {
    return false;
  }

  const auto resolution = parseResolution(controlText(impl_->resolution_combo));
  if (!resolution) {
    const auto &strings = textFor(impl_->language).validation;
    const auto title = std::wstring{strings.invalid_resolution_title};
    const auto message = std::wstring{strings.invalid_resolution_message};
    MessageBoxW(owner, message.c_str(), title.c_str(), MB_OK | MB_ICONERROR);
    SetFocus(impl_->resolution_combo);
    SendMessageW(impl_->resolution_combo, CB_SETEDITSEL, 0, MAKELPARAM(0, -1));
    return false;
  }

  auto updated = *impl_->settings;
  updated.width = resolution->first;
  updated.height = resolution->second;

  constexpr std::array<int, 5U> samples{0, 0, 2, 4, 8};
  const auto antialiasing = static_cast<int>(
      SendMessageW(impl_->antialiasing_combo, CB_GETCURSEL, 0, 0));
  if (antialiasing >= 0 &&
      static_cast<std::size_t>(antialiasing) < samples.size()) {
    updated.smaa = antialiasing == 1;
    updated.msaa_samples = samples[static_cast<std::size_t>(antialiasing)];
  }

  const auto frame_limit = static_cast<int>(
      SendMessageW(impl_->frame_limit_combo, CB_GETCURSEL, 0, 0));
  if (frame_limit >= 0 &&
      static_cast<std::size_t>(frame_limit) < impl_->frame_limits.size()) {
    updated.frame_limit =
        impl_->frame_limits[static_cast<std::size_t>(frame_limit)];
  }

  updated.aspect_ratio =
      SendMessageW(impl_->aspect_combo, CB_GETCURSEL, 0, 0) == 0
          ? AspectRatioMode::adaptive
          : AspectRatioMode::original_4_3;
  updated.bilinear_filtering = checked(impl_->parent, bilinear_control_id);
  updated.trilinear_filtering = updated.bilinear_filtering &&
                                checked(impl_->parent, trilinear_control_id);
  updated.anisotropic_filtering =
      updated.bilinear_filtering &&
      checked(impl_->parent, anisotropic_control_id);
  updated.volumetric_effects =
      checked(impl_->parent, volumetric_effects_control_id);
  updated.mission_skyboxes =
      checked(impl_->parent, mission_skyboxes_control_id);
  updated.vsync = checked(impl_->parent, vsync_control_id);
  updated.fullscreen = checked(impl_->parent, fullscreen_control_id);

  *impl_->settings = updated;
  return true;
}

void GraphicsPage::shutdown() noexcept {
  if (!impl_) {
    return;
  }
  for (auto iterator = impl_->controls.rbegin();
       iterator != impl_->controls.rend(); ++iterator) {
    if (*iterator != nullptr && IsWindow(*iterator) != FALSE) {
      DestroyWindow(*iterator);
    }
  }
  impl_->controls.clear();
  impl_->resolutions.clear();
  impl_->frame_limits.clear();
  impl_->settings = nullptr;
  impl_->parent = nullptr;
  impl_->created = false;
  impl_->is_visible = false;
  impl_->resolution_label = nullptr;
  impl_->resolution_combo = nullptr;
  impl_->aspect_label = nullptr;
  impl_->aspect_combo = nullptr;
  impl_->antialiasing_label = nullptr;
  impl_->antialiasing_combo = nullptr;
  impl_->frame_limit_label = nullptr;
  impl_->frame_limit_combo = nullptr;
  impl_->bilinear_check = nullptr;
  impl_->trilinear_check = nullptr;
  impl_->anisotropic_check = nullptr;
  impl_->volumetric_effects_check = nullptr;
  impl_->mission_skyboxes_check = nullptr;
  impl_->vsync_check = nullptr;
  impl_->fullscreen_check = nullptr;
}

bool GraphicsPage::visible() const noexcept {
  return impl_ != nullptr && impl_->created && impl_->is_visible;
}

} // namespace sf::platform::launcher

#endif
