#include "../launcher.hpp"

#include "controls_page.hpp"
#include "dossier_page.hpp"
#include "graphics_page.hpp"
#include "launch_page.hpp"
#include "settings.hpp"
#include "text.hpp"
#include "theme.hpp"

#ifdef _WIN32

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace sf::platform {
namespace {

constexpr wchar_t launcher_class_name[] = L"SyphonFilterPCLauncher";
constexpr int launch_tab_control_id = 4001;
constexpr int graphics_tab_control_id = 4002;
constexpr int controls_tab_control_id = 4003;
constexpr int dossiers_tab_control_id = 4004;
constexpr int play_control_id = 4010;
constexpr int close_control_id = 4011;
constexpr int logical_client_width = 960;
constexpr int logical_client_height = 720;
constexpr int minimum_client_width = 800;
constexpr int minimum_client_height = 620;

enum class ActivePage : std::uint8_t {
  launch,
  graphics,
  controls,
  dossiers,
};

struct LauncherState {
  GraphicsSettings settings;
  LauncherState() : theme(USER_DEFAULT_SCREEN_DPI) {}

  KeyboardMouseBindings input;
  game::GameLanguage language{game::GameLanguage::english};
  std::filesystem::path cue_path;
  launcher::ThemeResources theme;
  launcher::LaunchPage launch_page;
  launcher::GraphicsPage graphics_page;
  launcher::ControlsPage controls_page;
  launcher::DossierPage dossier_page;
  std::array<HWND, 4U> tab_buttons{};
  HWND play_button{};
  HWND close_button{};
  RECT page_bounds{};
  ActivePage active_page{ActivePage::launch};
  bool dossier_available{};
  bool accepted{};
  bool window_created{};
};

void showSettingsSaveError(game::GameLanguage language) noexcept {
  const auto &strings = launcher::textFor(language).validation;
  MessageBoxW(nullptr, strings.settings_save_failed_message.data(),
              strings.settings_save_failed_title.data(),
              MB_OK | MB_ICONWARNING | MB_TASKMODAL);
}

[[nodiscard]] bool isRussian(const LauncherState &state) noexcept {
  return state.language == game::GameLanguage::russian_vit;
}

[[nodiscard]] UINT systemDpi() noexcept {
  using GetDpiForSystemFn = UINT(WINAPI *)();
  const auto user32 = GetModuleHandleW(L"user32.dll");
  const auto get_dpi = user32 != nullptr
                           ? reinterpret_cast<GetDpiForSystemFn>(
                                 GetProcAddress(user32, "GetDpiForSystem"))
                           : nullptr;
  return get_dpi != nullptr ? std::max(get_dpi(), 1U) : USER_DEFAULT_SCREEN_DPI;
}

[[nodiscard]] int scaleForDpi(int value, UINT dpi) noexcept {
  return MulDiv(value, static_cast<int>(std::max(dpi, 1U)),
                USER_DEFAULT_SCREEN_DPI);
}

class PaintSession final {
public:
  explicit PaintSession(HWND window) noexcept
      : window_(window), dc_(BeginPaint(window, &paint_)) {}

  ~PaintSession() {
    if (dc_ != nullptr) {
      EndPaint(window_, &paint_);
    }
  }

  PaintSession(const PaintSession &) = delete;
  PaintSession &operator=(const PaintSession &) = delete;

  [[nodiscard]] HDC dc() const noexcept { return dc_; }

private:
  HWND window_{};
  PAINTSTRUCT paint_{};
  HDC dc_{};
};
[[nodiscard]] std::wstring widenUtf8(std::string_view text) {
  if (text.empty()) {
    return {};
  }
  const auto required =
      MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                          static_cast<int>(text.size()), nullptr, 0);
  if (required <= 0) {
    return {text.begin(), text.end()};
  }
  std::wstring result(static_cast<std::size_t>(required), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                      static_cast<int>(text.size()), result.data(), required);
  return result;
}

[[nodiscard]] RECT pageBounds(HWND window) noexcept {
  RECT client{};
  GetClientRect(window, &client);
  constexpr LONG margin = 24;
  constexpr LONG header_height = 132;
  constexpr LONG footer_height = 70;
  return RECT{margin, header_height,
              std::max(margin + 1L, client.right - margin),
              std::max(header_height + 1L, client.bottom - footer_height)};
}

void applyPageVisibility(HWND window, LauncherState &state) {
  if (state.active_page == ActivePage::launch) {
    state.launch_page.show();
  } else {
    state.launch_page.hide();
  }
  if (state.active_page == ActivePage::graphics) {
    state.graphics_page.show();
  } else {
    state.graphics_page.hide();
  }
  if (state.active_page == ActivePage::controls) {
    state.controls_page.show();
  } else {
    state.controls_page.hide();
  }
  state.dossier_page.setVisible(state.active_page == ActivePage::dossiers &&
                                state.dossier_available);
  InvalidateRect(window, nullptr, FALSE);
}

void selectPage(HWND window, LauncherState &state, ActivePage page) {
  if (page == ActivePage::dossiers && !state.dossier_available) {
    return;
  }
  state.active_page = page;
  applyPageVisibility(window, state);
}

void applyLanguage(HWND window, LauncherState &state) {
  const auto &text = launcher::textFor(state.language);
  SetWindowTextW(window, text.shell.window_title.data());
  SetWindowTextW(state.tab_buttons[0], text.shell.launch_tab.data());
  SetWindowTextW(state.tab_buttons[1], text.shell.graphics_tab.data());
  SetWindowTextW(state.tab_buttons[2], text.shell.controls_tab.data());
  SetWindowTextW(state.tab_buttons[3], text.shell.dossiers_tab.data());
  SetWindowTextW(state.play_button, text.shell.play.data());
  SetWindowTextW(state.close_button, text.shell.close.data());
  state.launch_page.setLanguage(state.language);
  state.graphics_page.setLanguage(state.language);
  state.controls_page.setRussian(isRussian(state));
  state.dossier_page.setLanguage(isRussian(state));
  InvalidateRect(window, nullptr, FALSE);
}

void layoutLauncher(HWND window, LauncherState &state) {
  RECT client{};
  GetClientRect(window, &client);
  constexpr int margin = 24;
  constexpr int tab_top = 78;
  constexpr int tab_height = 42;
  constexpr int tab_gap = 8;
  const auto available =
      std::max(4, static_cast<int>(client.right) - margin * 2 - tab_gap * 3);
  const auto tab_width = available / 4;
  for (std::size_t index = 0U; index < state.tab_buttons.size(); ++index) {
    const auto x = margin + static_cast<int>(index) * (tab_width + tab_gap);
    const auto width = index + 1U == state.tab_buttons.size()
                           ? static_cast<int>(client.right) - margin - x
                           : tab_width;
    MoveWindow(state.tab_buttons[index], x, tab_top, width, tab_height, TRUE);
  }

  constexpr int footer_height = 42;
  constexpr int footer_bottom = 14;
  constexpr int close_width = 124;
  constexpr int play_width = 148;
  constexpr int action_gap = 12;
  const auto footer_y = std::max(0, static_cast<int>(client.bottom) -
                                        footer_bottom - footer_height);
  const auto close_x =
      std::max(margin, static_cast<int>(client.right) - margin - close_width);
  const auto play_x = std::max(margin, close_x - action_gap - play_width);
  MoveWindow(state.play_button, play_x, footer_y, play_width, footer_height,
             TRUE);
  MoveWindow(state.close_button, close_x, footer_y, close_width, footer_height,
             TRUE);

  state.page_bounds = pageBounds(window);
  state.launch_page.layout(state.page_bounds);
  state.graphics_page.layout(state.page_bounds);
  state.controls_page.layout(state.page_bounds);
  state.dossier_page.layout(state.page_bounds);
}

void paintLauncher(HWND window, LauncherState &state) {
  PaintSession paint{window};
  const auto dc = paint.dc();
  if (dc == nullptr) {
    return;
  }
  RECT client{};
  GetClientRect(window, &client);
  FillRect(dc, &client, state.theme.backgroundBrush());

  const auto grid_pen = CreatePen(PS_SOLID, 1, launcher::ThemeColors::grid);
  const auto old_pen =
      grid_pen != nullptr ? SelectObject(dc, grid_pen) : nullptr;
  if (grid_pen != nullptr) {
    for (auto x = 0; x < client.right; x += 32) {
      MoveToEx(dc, x, 126, nullptr);
      LineTo(dc, x, client.bottom);
    }
    for (auto y = 126; y < client.bottom; y += 32) {
      MoveToEx(dc, 0, y, nullptr);
      LineTo(dc, client.right, y);
    }
  }

  if (state.active_page != ActivePage::dossiers || !state.dossier_available) {
    FillRect(dc, &state.page_bounds, state.theme.panelBrush());
    const auto border_pen =
        CreatePen(PS_SOLID, 2, launcher::ThemeColors::border);
    if (border_pen != nullptr) {
      const auto previous_pen = SelectObject(dc, border_pen);
      const auto previous_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
      Rectangle(dc, state.page_bounds.left, state.page_bounds.top,
                state.page_bounds.right, state.page_bounds.bottom);
      SelectObject(dc, previous_brush);
      SelectObject(dc, previous_pen);
      DeleteObject(border_pen);
    }
  }

  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, launcher::ThemeColors::text);
  const auto previous_font = SelectObject(dc, state.theme.titleFont());
  RECT title{28, 16, client.right - 28, 64};
  DrawTextW(dc, L"SYPHON FILTER // PC", -1, &title,
            DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
  if (previous_font != nullptr && previous_font != HGDI_ERROR) {
    SelectObject(dc, previous_font);
  }

  if (state.active_page == ActivePage::dossiers && state.dossier_available) {
    state.dossier_page.paint(dc, state.page_bounds);
  }

  if (grid_pen != nullptr) {
    if (old_pen != nullptr && old_pen != HGDI_ERROR) {
      SelectObject(dc, old_pen);
    }
    DeleteObject(grid_pen);
  }
}

[[nodiscard]] bool isShellButton(int control_id) noexcept {
  return (control_id >= launch_tab_control_id &&
          control_id <= dossiers_tab_control_id) ||
         control_id == play_control_id || control_id == close_control_id;
}

void drawShellButton(const DRAWITEMSTRUCT &item,
                     const LauncherState &state) noexcept {
  switch (item.CtlID) {
  case launch_tab_control_id:
    launcher::drawTabButton(item, state.theme.headingFont(),
                            state.active_page == ActivePage::launch);
    break;
  case graphics_tab_control_id:
    launcher::drawTabButton(item, state.theme.headingFont(),
                            state.active_page == ActivePage::graphics);
    break;
  case controls_tab_control_id:
    launcher::drawTabButton(item, state.theme.headingFont(),
                            state.active_page == ActivePage::controls);
    break;
  case dossiers_tab_control_id:
    launcher::drawTabButton(item, state.theme.headingFont(),
                            state.active_page == ActivePage::dossiers);
    break;
  case play_control_id:
    launcher::drawActionButton(item, state.theme.headingFont(),
                               launcher::ActionButtonRole::primary);
    break;
  case close_control_id:
    launcher::drawActionButton(item, state.theme.headingFont(),
                               launcher::ActionButtonRole::danger);
    break;
  default:
    break;
  }
}

[[nodiscard]] bool createShellControls(HWND window, LauncherState &state) {
  constexpr std::array<int, 4U> tab_ids{
      launch_tab_control_id, graphics_tab_control_id, controls_tab_control_id,
      dossiers_tab_control_id};
  for (std::size_t index = 0U; index < state.tab_buttons.size(); ++index) {
    state.tab_buttons[index] = launcher::createControl(
        window, L"BUTTON", L"", WS_TABSTOP | BS_OWNERDRAW,
        launcher::ControlBounds{}, tab_ids[index], state.theme.headingFont());
    if (state.tab_buttons[index] == nullptr) {
      return false;
    }
  }
  state.play_button = launcher::createControl(
      window, L"BUTTON", L"", WS_TABSTOP | BS_OWNERDRAW,
      launcher::ControlBounds{}, play_control_id, state.theme.headingFont());
  state.close_button = launcher::createControl(
      window, L"BUTTON", L"", WS_TABSTOP | BS_OWNERDRAW,
      launcher::ControlBounds{}, close_control_id, state.theme.headingFont());
  return state.play_button != nullptr && state.close_button != nullptr;
}

[[nodiscard]] bool createPages(HWND window, LauncherState &state) {
  state.page_bounds = pageBounds(window);
  if (!state.launch_page.create(
          window, state.page_bounds,
          launcher::LaunchPageStyle{state.theme.headingFont(),
                                    state.theme.uiFont()},
          state.cue_path, state.language)) {
    return false;
  }
  if (!state.graphics_page.create(
          window, state.page_bounds,
          launcher::GraphicsPageStyle{state.theme.uiFont()}, state.settings,
          state.language)) {
    return false;
  }
  const launcher::ControlsPageStyle controls_style{
      .heading_font = state.theme.headingFont(),
      .ui_font = state.theme.uiFont(),
      .panel_brush = state.theme.panelBrush(),
      .panel_color = launcher::ThemeColors::panel,
      .grid_color = launcher::ThemeColors::grid,
      .border_color = launcher::ThemeColors::border,
      .text_color = launcher::ThemeColors::text,
      .muted_text_color = launcher::ThemeColors::muted_text,
  };
  if (!state.controls_page.create(
          window, state.page_bounds, controls_style, state.input,
          state.settings.controller_bindings,
          state.settings.controller_protocol,
          state.settings.controller_vibration, isRussian(state))) {
    return false;
  }
  const launcher::DossierPageStyle dossier_style{
      .title_font = state.theme.titleFont(),
      .heading_font = state.theme.headingFont(),
      .ui_font = state.theme.uiFont(),
      .background_brush = state.theme.backgroundBrush(),
      .panel_brush = state.theme.panelBrush(),
      .grid_color = launcher::ThemeColors::grid,
      .text_color = launcher::ThemeColors::text,
      .muted_text_color = launcher::ThemeColors::muted_text,
      .accent_color = launcher::ThemeColors::archive,
  };
  state.dossier_available =
      state.dossier_page.create(window, dossier_style, isRussian(state));
  EnableWindow(state.tab_buttons[3], state.dossier_available ? TRUE : FALSE);
  applyLanguage(window, state);
  layoutLauncher(window, state);
  applyPageVisibility(window, state);
  return true;
}

void destroyPages(LauncherState &state) noexcept {
  state.dossier_page.shutdown();
  state.controls_page.shutdown();
  state.graphics_page.shutdown();
  state.launch_page.shutdown();
}

LRESULT CALLBACK launcherWindowProcImpl(HWND window, UINT message,
                                        WPARAM w_param, LPARAM l_param) {
  auto *state = reinterpret_cast<LauncherState *>(
      GetWindowLongPtrW(window, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    const auto *create = reinterpret_cast<const CREATESTRUCTW *>(l_param);
    if (create == nullptr || create->lpCreateParams == nullptr) {
      return FALSE;
    }
    state = static_cast<LauncherState *>(create->lpCreateParams);
    SetLastError(ERROR_SUCCESS);
    const auto previous = SetWindowLongPtrW(window, GWLP_USERDATA,
                                            reinterpret_cast<LONG_PTR>(state));
    if (previous == 0 && GetLastError() != ERROR_SUCCESS) {
      return FALSE;
    }
  }
  if (state == nullptr) {
    return DefWindowProcW(window, message, w_param, l_param);
  }

  switch (message) {
  case WM_CREATE:
    SendMessageW(window, WM_SETICON, ICON_BIG,
                 reinterpret_cast<LPARAM>(state->theme.largeIcon()));
    SendMessageW(window, WM_SETICON, ICON_SMALL,
                 reinterpret_cast<LPARAM>(state->theme.smallIcon()));
    if (!createShellControls(window, *state) || !createPages(window, *state)) {
      return -1;
    }
    state->window_created = true;
    return 0;
  case WM_SIZE:
    if (w_param != SIZE_MINIMIZED) {
      layoutLauncher(window, *state);
      InvalidateRect(window, nullptr, FALSE);
    }
    return 0;
  case WM_GETMINMAXINFO: {
    auto *limits = reinterpret_cast<MINMAXINFO *>(l_param);
    if (limits != nullptr) {
      RECT minimum{0, 0, scaleForDpi(minimum_client_width, state->theme.dpi()),
                   scaleForDpi(minimum_client_height, state->theme.dpi())};
      const auto style =
          static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE));
      const auto extended_style =
          static_cast<DWORD>(GetWindowLongPtrW(window, GWL_EXSTYLE));
      if (AdjustWindowRectEx(&minimum, style, FALSE, extended_style) != FALSE) {
        limits->ptMinTrackSize.x = minimum.right - minimum.left;
        limits->ptMinTrackSize.y = minimum.bottom - minimum.top;
      }
    }
    return 0;
  }
  case WM_PAINT:
    paintLauncher(window, *state);
    return 0;
  case WM_ERASEBKGND:
    return 1;
  case WM_CTLCOLORSTATIC:
  case WM_CTLCOLORBTN: {
    const auto dc = reinterpret_cast<HDC>(w_param);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, launcher::ThemeColors::text);
    return reinterpret_cast<LRESULT>(state->theme.panelBrush());
  }
  case WM_CTLCOLOREDIT:
  case WM_CTLCOLORLISTBOX: {
    const auto dc = reinterpret_cast<HDC>(w_param);
    SetBkColor(dc, launcher::ThemeColors::panel);
    SetTextColor(dc, launcher::ThemeColors::text);
    return reinterpret_cast<LRESULT>(state->theme.panelBrush());
  }
  case WM_DRAWITEM: {
    const auto *item = reinterpret_cast<const DRAWITEMSTRUCT *>(l_param);
    if (item == nullptr) {
      return FALSE;
    }
    if (state->active_page == ActivePage::controls &&
        state->controls_page.handleDrawItem(*item)) {
      return TRUE;
    }
    if (item->CtlType == ODT_BUTTON && isShellButton(item->CtlID)) {
      drawShellButton(*item, *state);
      return TRUE;
    }
    return FALSE;
  }
  case WM_TIMER:
    if (state->active_page == ActivePage::controls &&
        state->controls_page.handleTimer(static_cast<UINT_PTR>(w_param))) {
      return 0;
    }
    break;
  case WM_KEYDOWN:
    if (state->dossier_page.handleKey(w_param)) {
      return 0;
    }
    break;
  case WM_COMMAND: {
    bool handled{};
    switch (state->active_page) {
    case ActivePage::launch: {
      const auto previous_language = state->language;
      handled = state->launch_page.handleCommand(w_param, l_param);
      if (handled && previous_language != state->language) {
        applyLanguage(window, *state);
      }
      break;
    }
    case ActivePage::graphics:
      handled = state->graphics_page.handleCommand(w_param, l_param);
      break;
    case ActivePage::controls:
      handled = state->controls_page.handleCommand(w_param, l_param);
      break;
    case ActivePage::dossiers:
      handled = state->dossier_page.handleCommand(w_param, l_param);
      break;
    }
    if (handled) {
      return 0;
    }
    if (HIWORD(w_param) != BN_CLICKED) {
      return 0;
    }
    switch (LOWORD(w_param)) {
    case launch_tab_control_id:
      selectPage(window, *state, ActivePage::launch);
      return 0;
    case graphics_tab_control_id:
      selectPage(window, *state, ActivePage::graphics);
      return 0;
    case controls_tab_control_id:
      selectPage(window, *state, ActivePage::controls);
      return 0;
    case dossiers_tab_control_id:
      selectPage(window, *state, ActivePage::dossiers);
      return 0;
    case play_control_id:
      if (!state->launch_page.validate(window)) {
        selectPage(window, *state, ActivePage::launch);
        return 0;
      }
      if (!state->graphics_page.validateAndCommit(window)) {
        selectPage(window, *state, ActivePage::graphics);
        return 0;
      }
      state->accepted = true;
      DestroyWindow(window);
      return 0;
    case close_control_id:
      DestroyWindow(window);
      return 0;
    default:
      break;
    }
    break;
  }
  case WM_CLOSE:
    DestroyWindow(window);
    return 0;
  case WM_DESTROY:
    destroyPages(*state);
    if (state->window_created) {
      state->window_created = false;
      PostQuitMessage(0);
    }
    return 0;
  case WM_NCDESTROY:
    SetWindowLongPtrW(window, GWLP_USERDATA, 0);
    return DefWindowProcW(window, message, w_param, l_param);
  default:
    break;
  }
  return DefWindowProcW(window, message, w_param, l_param);
}

LRESULT CALLBACK launcherWindowProc(HWND window, UINT message, WPARAM w_param,
                                    LPARAM l_param) noexcept {
  try {
    return launcherWindowProcImpl(window, message, w_param, l_param);
  } catch (...) {
    if (message == WM_NCCREATE) {
      return FALSE;
    }
    if (message == WM_CREATE) {
      return -1;
    }
    return DefWindowProcW(window, message, w_param, l_param);
  }
}
[[nodiscard]] bool registerLauncherClass(const LauncherState &state) noexcept {
  WNDCLASSEXW window_class{};
  window_class.cbSize = sizeof(window_class);
  window_class.style = CS_HREDRAW | CS_VREDRAW;
  window_class.lpfnWndProc = launcherWindowProc;
  window_class.hInstance = GetModuleHandleW(nullptr);
  window_class.hIcon = state.theme.largeIcon();
  window_class.hIconSm = state.theme.smallIcon();
  window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
  window_class.lpszClassName = launcher_class_name;
  if (RegisterClassExW(&window_class) != 0U) {
    return true;
  }
  return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

void centerWindow(HWND window) noexcept {
  RECT work_area{};
  if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0) == FALSE) {
    return;
  }
  RECT bounds{};
  if (GetWindowRect(window, &bounds) == FALSE) {
    return;
  }
  const auto width = bounds.right - bounds.left;
  const auto height = bounds.bottom - bounds.top;
  SetWindowPos(window, nullptr,
               work_area.left + (work_area.right - work_area.left - width) / 2,
               work_area.top + (work_area.bottom - work_area.top - height) / 2,
               0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

} // namespace

void loadLauncherSettings(GraphicsSettings &graphics,
                          KeyboardMouseBindings &input,
                          game::GameLanguage &language) noexcept {
  try {
    launcher_settings::loadSettingsFile(graphics, input, language);
  } catch (...) {
  }
}

bool saveLauncherControllerSettings(const ControllerButtonBindings &bindings,
                                    bool vibration) noexcept {
  try {
    return launcher_settings::saveControllerSettingsFile(bindings, vibration);
  } catch (...) {
    return false;
  }
}

bool showLauncher(GraphicsSettings &settings, KeyboardMouseBindings &input,
                  game::GameLanguage &language,
                  std::filesystem::path &cue_path) {
  LauncherState state{};
  state.settings = settings;
  state.input = input;
  state.language = language;
  state.cue_path =
      cue_path.empty() ? launcher_settings::loadGameImagePath() : cue_path;
  static_cast<void>(state.theme.reset(systemDpi()));
  if (!state.theme.valid() || !registerLauncherClass(state)) {
    showLauncherError("STARTUP FAILED",
                      "The launcher user interface could not be initialized.");
    return false;
  }

  const auto dpi = state.theme.dpi();
  RECT bounds{0, 0, scaleForDpi(logical_client_width, dpi),
              scaleForDpi(logical_client_height, dpi)};
  constexpr DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
  AdjustWindowRectEx(&bounds, style, FALSE, 0U);
  const auto &text = launcher::textFor(state.language);
  const auto window =
      CreateWindowExW(0U, launcher_class_name, text.shell.window_title.data(),
                      style, CW_USEDEFAULT, CW_USEDEFAULT,
                      bounds.right - bounds.left, bounds.bottom - bounds.top,
                      nullptr, nullptr, GetModuleHandleW(nullptr), &state);
  if (window == nullptr) {
    UnregisterClassW(launcher_class_name, GetModuleHandleW(nullptr));
    showLauncherError("STARTUP FAILED",
                      "The launcher window could not be created.");
    return false;
  }

  centerWindow(window);
  ShowWindow(window, SW_SHOW);
  UpdateWindow(window);
  MSG message{};
  BOOL message_result{};
  while ((message_result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
    if (state.controls_page.handleInput(message)) {
      continue;
    }
    if (state.active_page == ActivePage::dossiers && state.dossier_available &&
        message.message == WM_KEYDOWN &&
        state.dossier_page.handleKey(message.wParam)) {
      continue;
    }
    if (IsDialogMessageW(window, &message) == FALSE) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
  }

  if (IsWindow(window) != FALSE) {
    DestroyWindow(window);
  }
  UnregisterClassW(launcher_class_name, GetModuleHandleW(nullptr));
  static_cast<void>(message_result);

  if (state.accepted) {
    settings = state.settings;
    input = state.input;
    language = state.language;
    cue_path = state.cue_path;
    bool settings_saved{};
    try {
      settings_saved = launcher_settings::saveSettingsFile(settings, input,
                                                           language, cue_path);
    } catch (...) {
      settings_saved = false;
    }
    if (!settings_saved) {
      showSettingsSaveError(language);
    }
  }
  return state.accepted;
}

bool retailCheatMarkerExists() noexcept {
  return launcher_settings::cheatMarkerExists();
}

void showLauncherError(std::string_view title,
                       std::string_view message) noexcept {
  try {
    const auto wide_title = widenUtf8(title);
    const auto wide_message = widenUtf8(message);
    MessageBoxW(nullptr, wide_message.c_str(), wide_title.c_str(),
                MB_OK | MB_ICONERROR);
  } catch (...) {
  }
}

} // namespace sf::platform

#else

namespace sf::platform {

void loadLauncherSettings(GraphicsSettings &, KeyboardMouseBindings &,
                          game::GameLanguage &) noexcept {}

bool saveLauncherControllerSettings(const ControllerButtonBindings &,
                                    bool) noexcept {
  return false;
}

bool showLauncher(GraphicsSettings &, KeyboardMouseBindings &,
                  game::GameLanguage &, std::filesystem::path &) {
  return true;
}

bool retailCheatMarkerExists() noexcept { return false; }

void showLauncherError(std::string_view, std::string_view) noexcept {}

} // namespace sf::platform

#endif
