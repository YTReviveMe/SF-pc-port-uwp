#include "controls_page.hpp"
#include "controller_capture.hpp"
#include "text.hpp"

#ifdef _WIN32

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sf::platform::launcher {
namespace {

constexpr std::size_t controller_stick_layout_row =
    game::controller_action_count;
constexpr std::size_t controller_controls_row_count =
    game::controller_action_count + 1U;

std::wstring widenAscii(std::string_view text) {
  return {text.begin(), text.end()};
}

std::wstring keyboardActionLabel(KeyboardMouseAction action,
                                 const ControlsPageText &text) {
  const auto index = static_cast<std::size_t>(action);
  if (index >= text.keyboard_actions.size()) {
    return std::wstring{text.action_fallback};
  }
  return std::wstring{text.keyboard_actions[index]};
}

std::wstring controllerActionLabel(game::ControllerAction action,
                                   const ControlsPageText &text) {
  const auto index = static_cast<std::size_t>(action);
  if (index >= text.controller_actions.size()) {
    return std::wstring{text.action_fallback};
  }
  return std::wstring{text.controller_actions[index]};
}

std::wstring stickLayoutLabel(game::ControllerStickLayout layout,
                              const ControlsPageText &text) {
  const auto index = static_cast<std::size_t>(layout);
  if (index >= text.stick_layouts.size()) {
    return std::wstring{text.stick_layouts.back()};
  }
  return std::wstring{text.stick_layouts[index]};
}

std::optional<KeyboardMouseInput>
keyboardInputFromVirtualKey(WPARAM virtual_key, LPARAM key_data) noexcept {
  if (virtual_key >= 'A' && virtual_key <= 'Z') {
    return static_cast<KeyboardMouseInput>(
        static_cast<std::uint16_t>(KeyboardMouseInput::a) +
        static_cast<std::uint16_t>(virtual_key - 'A'));
  }
  if (virtual_key >= '1' && virtual_key <= '9') {
    return static_cast<KeyboardMouseInput>(
        static_cast<std::uint16_t>(KeyboardMouseInput::digit_1) +
        static_cast<std::uint16_t>(virtual_key - '1'));
  }
  if (virtual_key == '0') {
    return KeyboardMouseInput::digit_0;
  }
  if (virtual_key >= VK_F1 && virtual_key <= VK_F12) {
    return static_cast<KeyboardMouseInput>(
        static_cast<std::uint16_t>(KeyboardMouseInput::f1) +
        static_cast<std::uint16_t>(virtual_key - VK_F1));
  }
  if (virtual_key >= VK_F13 && virtual_key <= VK_F24) {
    return static_cast<KeyboardMouseInput>(
        static_cast<std::uint16_t>(KeyboardMouseInput::f13) +
        static_cast<std::uint16_t>(virtual_key - VK_F13));
  }
  if (virtual_key >= VK_NUMPAD1 && virtual_key <= VK_NUMPAD9) {
    return static_cast<KeyboardMouseInput>(
        static_cast<std::uint16_t>(KeyboardMouseInput::keypad_1) +
        static_cast<std::uint16_t>(virtual_key - VK_NUMPAD1));
  }
  if (virtual_key == VK_NUMPAD0) {
    return KeyboardMouseInput::keypad_0;
  }

  const auto extended =
      (static_cast<std::uint64_t>(key_data) & (1ULL << 24U)) != 0U;
  if (virtual_key == VK_SHIFT) {
    const auto scan_code = static_cast<UINT>((key_data >> 16U) & 0xffU);
    virtual_key = MapVirtualKeyW(scan_code, MAPVK_VSC_TO_VK_EX);
  } else if (virtual_key == VK_CONTROL) {
    virtual_key = extended ? VK_RCONTROL : VK_LCONTROL;
  } else if (virtual_key == VK_MENU) {
    virtual_key = extended ? VK_RMENU : VK_LMENU;
  }

  switch (virtual_key) {
  case VK_RETURN:
    return extended ? KeyboardMouseInput::keypad_enter
                    : KeyboardMouseInput::enter;
  case VK_ESCAPE:
    return KeyboardMouseInput::escape;
  case VK_BACK:
    return KeyboardMouseInput::backspace;
  case VK_TAB:
    return KeyboardMouseInput::tab;
  case VK_SPACE:
    return KeyboardMouseInput::space;
  case VK_OEM_MINUS:
    return KeyboardMouseInput::minus;
  case VK_OEM_PLUS:
    return KeyboardMouseInput::equals;
  case VK_OEM_4:
    return KeyboardMouseInput::left_bracket;
  case VK_OEM_6:
    return KeyboardMouseInput::right_bracket;
  case VK_OEM_5:
    return KeyboardMouseInput::backslash;
  case VK_OEM_1:
    return KeyboardMouseInput::semicolon;
  case VK_OEM_7:
    return KeyboardMouseInput::apostrophe;
  case VK_OEM_3:
    return KeyboardMouseInput::grave;
  case VK_OEM_COMMA:
    return KeyboardMouseInput::comma;
  case VK_OEM_PERIOD:
    return KeyboardMouseInput::period;
  case VK_OEM_2:
    return KeyboardMouseInput::slash;
  case VK_OEM_102:
    return KeyboardMouseInput::non_us_backslash;
  case VK_CAPITAL:
    return KeyboardMouseInput::caps_lock;
  case VK_SNAPSHOT:
    return KeyboardMouseInput::print_screen;
  case VK_SCROLL:
    return KeyboardMouseInput::scroll_lock;
  case VK_PAUSE:
    return KeyboardMouseInput::pause;
  case VK_INSERT:
    return KeyboardMouseInput::insert;
  case VK_HOME:
    return KeyboardMouseInput::home;
  case VK_PRIOR:
    return KeyboardMouseInput::page_up;
  case VK_DELETE:
    return KeyboardMouseInput::delete_key;
  case VK_END:
    return KeyboardMouseInput::end;
  case VK_NEXT:
    return KeyboardMouseInput::page_down;
  case VK_RIGHT:
    return KeyboardMouseInput::right;
  case VK_LEFT:
    return KeyboardMouseInput::left;
  case VK_DOWN:
    return KeyboardMouseInput::down;
  case VK_UP:
    return KeyboardMouseInput::up;
  case VK_NUMLOCK:
    return KeyboardMouseInput::num_lock;
  case VK_DIVIDE:
    return KeyboardMouseInput::keypad_divide;
  case VK_MULTIPLY:
    return KeyboardMouseInput::keypad_multiply;
  case VK_SUBTRACT:
    return KeyboardMouseInput::keypad_minus;
  case VK_ADD:
    return KeyboardMouseInput::keypad_plus;
  case VK_DECIMAL:
    return KeyboardMouseInput::keypad_period;
  case VK_APPS:
    return KeyboardMouseInput::application;
  case VK_LCONTROL:
    return KeyboardMouseInput::left_control;
  case VK_LSHIFT:
    return KeyboardMouseInput::left_shift;
  case VK_LMENU:
    return KeyboardMouseInput::left_alt;
  case VK_LWIN:
    return KeyboardMouseInput::left_gui;
  case VK_RCONTROL:
    return KeyboardMouseInput::right_control;
  case VK_RSHIFT:
    return KeyboardMouseInput::right_shift;
  case VK_RMENU:
    return KeyboardMouseInput::right_alt;
  case VK_RWIN:
    return KeyboardMouseInput::right_gui;
  default:
    return std::nullopt;
  }
}

std::optional<KeyboardMouseInput> capturedInput(const MSG &message) noexcept {
  if ((message.message == WM_KEYDOWN || message.message == WM_SYSKEYDOWN) &&
      (static_cast<std::uint64_t>(message.lParam) & (1ULL << 30U)) == 0U) {
    return keyboardInputFromVirtualKey(message.wParam, message.lParam);
  }
  switch (message.message) {
  case WM_LBUTTONDOWN:
    return KeyboardMouseInput::mouse_left;
  case WM_RBUTTONDOWN:
    return KeyboardMouseInput::mouse_right;
  case WM_MBUTTONDOWN:
    return KeyboardMouseInput::mouse_middle;
  case WM_XBUTTONDOWN:
    return GET_XBUTTON_WPARAM(message.wParam) == XBUTTON1
               ? KeyboardMouseInput::mouse_x1
               : KeyboardMouseInput::mouse_x2;
  case WM_MOUSEWHEEL:
    return GET_WHEEL_DELTA_WPARAM(message.wParam) >= 0
               ? KeyboardMouseInput::mouse_wheel_up
               : KeyboardMouseInput::mouse_wheel_down;
  default:
    return std::nullopt;
  }
}

HWND createControl(HWND parent, const wchar_t *class_name, const wchar_t *text,
                   DWORD style, int id, HFONT font) {
  const auto control = CreateWindowExW(
      0, class_name, text, WS_CHILD | style, 0, 0, 1, 1, parent,
      id == 0 ? nullptr
              : reinterpret_cast<HMENU>(static_cast<std::intptr_t>(id)),
      GetModuleHandleW(nullptr), nullptr);
  if (control != nullptr) {
    SendMessageW(control, WM_SETFONT,
                 reinterpret_cast<WPARAM>(
                     font != nullptr ? font : GetStockObject(DEFAULT_GUI_FONT)),
                 TRUE);
  }
  return control;
}

} // namespace

struct ControlsPage::Impl {
  HWND parent{};
  ControlsPageStyle style;
  KeyboardMouseBindings *keyboard{};
  ControllerButtonBindings *controller{};
  ControllerProtocol *protocol{};
  bool *vibration{};
  bool russian{};
  bool visible{};
  bool controller_mode{};
  std::size_t selected{};
  std::optional<std::size_t> capture;
  ControllerCapture controller_capture;
  std::vector<HWND> controls;

  HWND heading{};
  HWND input_device_label{};
  HWND input_device_combo{};
  HWND protocol_label{};
  HWND protocol_combo{};
  HWND vibration_checkbox{};
  HWND assignments_label{};
  HWND list{};
  HWND binding_label{};
  HWND change_button{};
  HWND clear_button{};
  HWND defaults_button{};
  HWND status{};
  HWND hint{};

  [[nodiscard]] bool validModel() const noexcept {
    return keyboard != nullptr && controller != nullptr &&
           protocol != nullptr && vibration != nullptr;
  }

  void add(HWND control) {
    if (control != nullptr) {
      controls.push_back(control);
    }
  }

  [[nodiscard]] const ControlsPageText &localizedText() const noexcept {
    const auto language =
        russian ? game::GameLanguage::russian_vit : game::GameLanguage::english;
    return textFor(language).controls;
  }

  void setStatus(std::wstring_view text) const {
    if (status != nullptr) {
      const auto value = std::wstring{text};
      SetWindowTextW(status, value.c_str());
    }
  }

  std::wstring controllerButtonName(std::uint32_t mask) const {
    return widenAscii(controllerButtonPromptName(
        controller_capture.family(), static_cast<std::uint16_t>(mask)));
  }

  std::wstring deviceDescription() const {
    const auto &text = localizedText();
    if (!controller_capture.connected()) {
      return std::wstring{text.no_controller};
    }
    auto result = std::wstring{controller_capture.name()};
    if (!result.empty()) {
      result += L"  /  ";
    }
    switch (controller_capture.family()) {
    case ControllerPromptFamily::xbox:
      return result + std::wstring{text.xbox_controller};
    case ControllerPromptFamily::playstation:
      return result + std::wstring{text.playstation_controller};
    case ControllerPromptFamily::nintendo:
      return result + std::wstring{text.nintendo_controller};
    case ControllerPromptFamily::generic:
    default:
      return result + std::wstring{text.generic_controller};
    }
  }

  void refreshList() {
    if (!validModel() || list == nullptr) {
      return;
    }
    const auto &text = localizedText();
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    if (controller_mode) {
      for (const auto &metadata : game::controllerActionCatalog()) {
        const auto row =
            controllerActionLabel(metadata.action, text) + L"    [" +
            controllerButtonName((*controller)[metadata.action]) + L"]";
        SendMessageW(list, LB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(row.c_str()));
      }
      const auto stick_row = std::wstring{text.stick_layout} + L"    [" +
                             stickLayoutLabel(controller->stick_layout, text) +
                             L"]";
      SendMessageW(list, LB_ADDSTRING, 0,
                   reinterpret_cast<LPARAM>(stick_row.c_str()));
    } else {
      for (std::size_t index = 0; index < keyboard_mouse_action_count;
           ++index) {
        const auto action = static_cast<KeyboardMouseAction>(index);
        const auto row =
            keyboardActionLabel(action, text) + L"    [" +
            widenAscii(keyboardMouseInputName((*keyboard)[action])) + L"]";
        SendMessageW(list, LB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(row.c_str()));
      }
    }
    const auto count = controller_mode ? controller_controls_row_count
                                       : keyboard_mouse_action_count;
    selected = std::min(selected, count - 1U);
    SendMessageW(list, LB_SETCURSEL, static_cast<WPARAM>(selected), 0);
    if (controller_mode) {
      if (selected == controller_stick_layout_row) {
        SetWindowTextW(change_button, text.next_layout.data());
      } else {
        const auto label = std::wstring{text.change_prefix} +
                           controllerButtonName(controller->buttons[selected]);
        SetWindowTextW(change_button, label.c_str());
      }
      EnableWindow(clear_button, FALSE);
    } else {
      const auto action = static_cast<KeyboardMouseAction>(selected);
      const auto label =
          std::wstring{text.change_prefix} +
          widenAscii(keyboardMouseInputName((*keyboard)[action]));
      SetWindowTextW(change_button, label.c_str());
      EnableWindow(clear_button, TRUE);
    }
    InvalidateRect(parent, nullptr, FALSE);
  }

  void cancelBindingCapture() {
    capture.reset();
    controller_capture.cancelCapture();
    refreshList();
    setStatus(localizedText().capture_cancelled);
  }

  void assignControllerButton(std::uint32_t button) {
    if (!controller_mode || selected >= game::controller_action_count) {
      return;
    }
    const auto result = game::rebindControllerButton(
        *controller, static_cast<game::ControllerAction>(selected), button);
    if (result == game::ControllerRebindResult::invalid) {
      return;
    }
    capture.reset();
    controller_capture.cancelCapture();
    refreshList();
    const auto message = std::wstring{localizedText().binding_updated} + L"  " +
                         deviceDescription();
    SetWindowTextW(status, message.c_str());
  }

  void beginBindingCapture() {
    if (!validModel()) {
      return;
    }
    if (controller_mode) {
      if (selected == controller_stick_layout_row) {
        controller->stick_layout =
            game::cycledControllerStickLayout(controller->stick_layout);
        capture.reset();
        controller_capture.cancelCapture();
        refreshList();
        setStatus(localizedText().layout_updated);
        return;
      }
      if (!controller_capture.initialize(*protocol)) {
        setStatus(localizedText().controller_init_failed);
        return;
      }
      static_cast<void>(controller_capture.update());
      capture = selected;
      controller_capture.beginCapture();
      const auto &text = localizedText();
      SetWindowTextW(change_button, text.waiting_for_controller.data());
      if (controller_capture.connected()) {
        const auto message = std::wstring{text.controller_capture_hint} +
                             L"  " + deviceDescription();
        SetWindowTextW(status, message.c_str());
      } else {
        setStatus(text.connect_controller_hint);
      }
      return;
    }
    capture = selected;
    const auto &text = localizedText();
    SetWindowTextW(change_button, text.waiting_for_input.data());
    setStatus(text.keyboard_capture_hint);
  }

  void setControllerMode(bool enabled) {
    controller_mode = enabled;
    selected = 0U;
    capture.reset();
    controller_capture.cancelCapture();
    if (controller_mode && visible && validModel()) {
      static_cast<void>(controller_capture.initialize(*protocol));
      static_cast<void>(controller_capture.update());
    }
    refreshList();
    if (controller_mode) {
      const auto message = deviceDescription();
      SetWindowTextW(status, message.c_str());
    } else {
      setStatus(localizedText().select_action_hint);
    }
  }

  void applyProtocolSelection() {
    const auto selected_protocol =
        static_cast<int>(SendMessageW(protocol_combo, CB_GETCURSEL, 0, 0));
    if (selected_protocol < static_cast<int>(ControllerProtocol::automatic) ||
        selected_protocol > static_cast<int>(ControllerProtocol::raw_input)) {
      return;
    }
    *protocol = static_cast<ControllerProtocol>(selected_protocol);
    capture.reset();
    controller_capture.shutdown();
    if (controller_mode && visible) {
      static_cast<void>(controller_capture.initialize(*protocol));
      static_cast<void>(controller_capture.update());
    }
    refreshList();
    if (controller_mode) {
      const auto message = deviceDescription();
      SetWindowTextW(status, message.c_str());
    }
  }

  void populateLocalizedControls() {
    const auto &text = localizedText();
    const auto input_selection = controller_mode ? 1 : 0;
    SendMessageW(input_device_combo, CB_RESETCONTENT, 0, 0);
    SendMessageW(input_device_combo, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(text.keyboard_mouse.data()));
    SendMessageW(input_device_combo, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(text.controller.data()));
    SendMessageW(input_device_combo, CB_SETCURSEL, input_selection, 0);

    SendMessageW(protocol_combo, CB_RESETCONTENT, 0, 0);
    SendMessageW(protocol_combo, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(text.automatic_protocol.data()));
    SendMessageW(protocol_combo, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(text.xinput_protocol.data()));
    SendMessageW(protocol_combo, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(text.direct_input_protocol.data()));
    SendMessageW(protocol_combo, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(text.raw_input_protocol.data()));
    SendMessageW(protocol_combo, CB_SETCURSEL, static_cast<WPARAM>(*protocol),
                 0);

    SetWindowTextW(heading, text.heading.data());
    SetWindowTextW(input_device_label, text.input_device.data());
    SetWindowTextW(protocol_label, text.controller_backend.data());
    SetWindowTextW(vibration_checkbox, text.vibration.data());
    SetWindowTextW(assignments_label, text.assignments.data());
    SetWindowTextW(binding_label, text.binding_controls.data());
    SetWindowTextW(clear_button, text.clear.data());
    SetWindowTextW(defaults_button, text.restore_defaults.data());
    SetWindowTextW(hint, text.movement_hint.data());
    refreshList();
  }

  void drawButton(const DRAWITEMSTRUCT &item) const {
    const auto pressed = (item.itemState & ODS_SELECTED) != 0U;
    const auto disabled = (item.itemState & ODS_DISABLED) != 0U;
    const auto fill = pressed ? RGB(24, 39, 77) : style.panel_color;
    const auto fill_brush = CreateSolidBrush(fill);
    FillRect(item.hDC, &item.rcItem, fill_brush);
    DeleteObject(fill_brush);
    const auto border_pen = CreatePen(
        PS_SOLID, 2, disabled ? style.grid_color : style.border_color);
    const auto old_pen = SelectObject(item.hDC, border_pen);
    SelectObject(item.hDC, GetStockObject(NULL_BRUSH));
    Rectangle(item.hDC, item.rcItem.left, item.rcItem.top, item.rcItem.right,
              item.rcItem.bottom);
    SelectObject(item.hDC, old_pen);
    DeleteObject(border_pen);
    std::array<wchar_t, 160U> label{};
    GetWindowTextW(item.hwndItem, label.data(), static_cast<int>(label.size()));
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC,
                 disabled ? style.muted_text_color : style.text_color);
    SelectObject(item.hDC, style.heading_font);
    auto bounds = item.rcItem;
    if (pressed) {
      OffsetRect(&bounds, 1, 1);
    }
    DrawTextW(item.hDC, label.data(), -1, &bounds,
              DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    if ((item.itemState & ODS_FOCUS) != 0U) {
      InflateRect(&bounds, -4, -4);
      DrawFocusRect(item.hDC, &bounds);
    }
  }
};

ControlsPage::ControlsPage() : impl_(std::make_unique<Impl>()) {}
ControlsPage::~ControlsPage() { shutdown(); }

bool ControlsPage::create(HWND parent, const RECT &bounds,
                          const ControlsPageStyle &style,
                          KeyboardMouseBindings &keyboard,
                          ControllerButtonBindings &controller,
                          ControllerProtocol &protocol, bool &vibration,
                          bool russian) {
  shutdown();
  impl_->parent = parent;
  impl_->style = style;
  impl_->keyboard = &keyboard;
  impl_->controller = &controller;
  impl_->protocol = &protocol;
  impl_->vibration = &vibration;
  impl_->russian = russian;

  const auto add = [this](HWND control) {
    impl_->add(control);
    return control;
  };
  impl_->heading =
      add(createControl(parent, L"STATIC", L"", 0, 0, style.heading_font));
  impl_->input_device_label =
      add(createControl(parent, L"STATIC", L"", 0, 0, style.ui_font));
  impl_->input_device_combo =
      add(createControl(parent, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST,
                        input_device_control_id, style.ui_font));
  impl_->protocol_label =
      add(createControl(parent, L"STATIC", L"", 0, 0, style.ui_font));
  impl_->protocol_combo =
      add(createControl(parent, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST,
                        controller_protocol_control_id, style.ui_font));
  impl_->vibration_checkbox =
      add(createControl(parent, L"BUTTON", L"", WS_TABSTOP | BS_AUTOCHECKBOX,
                        controller_vibration_control_id, style.ui_font));
  impl_->assignments_label =
      add(createControl(parent, L"STATIC", L"", 0, 0, style.heading_font));
  impl_->list = add(createControl(parent, L"LISTBOX", L"",
                                  WS_TABSTOP | WS_BORDER | WS_VSCROLL |
                                      LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                                  binding_list_control_id, style.ui_font));
  impl_->binding_label =
      add(createControl(parent, L"STATIC", L"", 0, 0, style.heading_font));
  impl_->change_button =
      add(createControl(parent, L"BUTTON", L"", WS_TABSTOP | BS_OWNERDRAW,
                        change_binding_control_id, style.heading_font));
  impl_->clear_button =
      add(createControl(parent, L"BUTTON", L"", WS_TABSTOP | BS_OWNERDRAW,
                        clear_binding_control_id, style.heading_font));
  impl_->defaults_button =
      add(createControl(parent, L"BUTTON", L"", WS_TABSTOP | BS_OWNERDRAW,
                        default_bindings_control_id, style.heading_font));
  impl_->status =
      add(createControl(parent, L"STATIC", L"", SS_LEFT, 0, style.ui_font));
  impl_->hint =
      add(createControl(parent, L"STATIC", L"", SS_LEFT, 0, style.ui_font));

  if (impl_->controls.size() != 14U) {
    shutdown();
    return false;
  }
  CheckDlgButton(parent, controller_vibration_control_id,
                 vibration ? BST_CHECKED : BST_UNCHECKED);
  impl_->populateLocalizedControls();
  impl_->setControllerMode(false);
  layout(bounds);
  return true;
}

void ControlsPage::show() {
  if (impl_->parent == nullptr || impl_->visible) {
    return;
  }
  impl_->visible = true;
  for (const auto control : impl_->controls) {
    ShowWindow(control, SW_SHOW);
  }
  SetTimer(impl_->parent, capture_timer_id, 16U, nullptr);
  if (impl_->controller_mode && impl_->validModel()) {
    static_cast<void>(impl_->controller_capture.initialize(*impl_->protocol));
    static_cast<void>(impl_->controller_capture.update());
    impl_->refreshList();
    const auto status = impl_->deviceDescription();
    SetWindowTextW(impl_->status, status.c_str());
  }
}

void ControlsPage::hide() noexcept {
  if (impl_->parent != nullptr) {
    KillTimer(impl_->parent, capture_timer_id);
  }
  impl_->capture.reset();
  impl_->controller_capture.cancelCapture();
  for (const auto control : impl_->controls) {
    if (IsWindow(control) != FALSE) {
      ShowWindow(control, SW_HIDE);
    }
  }
  impl_->visible = false;
}

void ControlsPage::layout(const RECT &bounds) noexcept {
  if (impl_->controls.empty()) {
    return;
  }
  const auto left = static_cast<int>(bounds.left) + 20;
  const auto top = static_cast<int>(bounds.top) + 10;
  const auto right = static_cast<int>(bounds.right) - 20;
  const auto bottom = static_cast<int>(bounds.bottom) - 12;
  const auto width = std::max(600, right - left);
  const auto body_top = top + 96;
  const auto body_height = std::max(220, bottom - body_top);
  const auto gap = 18;
  const auto list_width = std::max(340, width * 58 / 100);
  const auto action_left = left + list_width + gap;
  const auto action_width = std::max(220, right - action_left);

  MoveWindow(impl_->heading, left, top, width, 24, TRUE);
  MoveWindow(impl_->input_device_label, left, top + 34, 112, 22, TRUE);
  MoveWindow(impl_->input_device_combo, left + 116, top + 30, 180, 120, TRUE);
  MoveWindow(impl_->protocol_label, left + 312, top + 34, 120, 22, TRUE);
  MoveWindow(impl_->protocol_combo, left + 430, top + 30, 280, 128, TRUE);
  MoveWindow(impl_->vibration_checkbox, left + 722, top + 31,
             std::max(120, right - (left + 722)), 24, TRUE);
  MoveWindow(impl_->assignments_label, left, top + 70, list_width, 22, TRUE);
  MoveWindow(impl_->list, left, body_top, list_width, body_height, TRUE);
  MoveWindow(impl_->binding_label, action_left, top + 70, action_width, 22,
             TRUE);
  MoveWindow(impl_->change_button, action_left, body_top, action_width, 42,
             TRUE);
  MoveWindow(impl_->clear_button, action_left, body_top + 54, action_width, 38,
             TRUE);
  MoveWindow(impl_->defaults_button, action_left, body_top + 104, action_width,
             38, TRUE);
  MoveWindow(impl_->status, action_left, body_top + 158, action_width, 74,
             TRUE);
  MoveWindow(impl_->hint, action_left, body_top + 244, action_width,
             std::max(42, body_height - 244), TRUE);
}

void ControlsPage::setRussian(bool russian) {
  if (impl_->russian == russian || impl_->controls.empty()) {
    return;
  }
  impl_->russian = russian;
  impl_->capture.reset();
  impl_->controller_capture.cancelCapture();
  impl_->populateLocalizedControls();
  if (impl_->controller_mode) {
    const auto status = impl_->deviceDescription();
    SetWindowTextW(impl_->status, status.c_str());
  } else {
    impl_->setStatus(impl_->localizedText().select_action_hint);
  }
}

bool ControlsPage::handleCommand(WPARAM w_param, LPARAM) {
  if (impl_->controls.empty()) {
    return false;
  }
  const auto id = static_cast<int>(LOWORD(w_param));
  const auto notification = static_cast<int>(HIWORD(w_param));
  if (id == input_device_control_id && notification == CBN_SELCHANGE) {
    impl_->setControllerMode(
        SendMessageW(impl_->input_device_combo, CB_GETCURSEL, 0, 0) == 1);
    return true;
  }
  if (id == controller_protocol_control_id && notification == CBN_SELCHANGE) {
    impl_->applyProtocolSelection();
    return true;
  }
  if (id == binding_list_control_id &&
      (notification == LBN_SELCHANGE || notification == LBN_DBLCLK)) {
    const auto selected = SendMessageW(impl_->list, LB_GETCURSEL, 0, 0);
    const auto count = impl_->controller_mode ? controller_controls_row_count
                                              : keyboard_mouse_action_count;
    if (selected >= 0 && static_cast<std::size_t>(selected) < count) {
      impl_->selected = static_cast<std::size_t>(selected);
      impl_->capture.reset();
      impl_->controller_capture.cancelCapture();
      impl_->refreshList();
      if (impl_->controller_mode) {
        if (impl_->selected == controller_stick_layout_row) {
          impl_->setStatus(impl_->localizedText().choose_layout_hint);
        } else {
          impl_->setStatus(impl_->localizedText().choose_button_hint);
        }
      } else {
        impl_->setStatus(impl_->localizedText().select_action_hint);
      }
      if (notification == LBN_DBLCLK) {
        impl_->beginBindingCapture();
      }
    }
    return true;
  }
  if (notification != BN_CLICKED) {
    return false;
  }
  if (id == controller_vibration_control_id) {
    *impl_->vibration =
        IsDlgButtonChecked(impl_->parent, controller_vibration_control_id) ==
        BST_CHECKED;
    return true;
  }
  if (id == change_binding_control_id) {
    impl_->beginBindingCapture();
    return true;
  }
  if (id == clear_binding_control_id) {
    if (!impl_->controller_mode) {
      (*impl_->keyboard)[static_cast<KeyboardMouseAction>(impl_->selected)] =
          KeyboardMouseInput::none;
      impl_->capture.reset();
      impl_->refreshList();
      impl_->setStatus(impl_->localizedText().binding_cleared);
    }
    return true;
  }
  if (id == default_bindings_control_id) {
    if (impl_->controller_mode) {
      *impl_->controller = ControllerButtonBindings{};
    } else {
      *impl_->keyboard = defaultKeyboardMouseBindings();
    }
    impl_->capture.reset();
    impl_->controller_capture.cancelCapture();
    impl_->refreshList();
    impl_->setStatus(impl_->localizedText().defaults_restored);
    return true;
  }
  return false;
}

bool ControlsPage::handleTimer(UINT_PTR timer_id) {
  if (timer_id != capture_timer_id) {
    return false;
  }
  if (!impl_->visible || !impl_->controller_mode || !impl_->validModel()) {
    return true;
  }
  if (!impl_->controller_capture.initialize(*impl_->protocol)) {
    return true;
  }
  const auto was_connected = impl_->controller_capture.connected();
  const auto device_changed = impl_->controller_capture.update();
  if (impl_->capture && was_connected &&
      !impl_->controller_capture.connected()) {
    impl_->cancelBindingCapture();
    impl_->setStatus(impl_->localizedText().controller_disconnected);
    return true;
  }
  if (impl_->capture && impl_->controller_capture.pollCancelRequest()) {
    impl_->cancelBindingCapture();
    return true;
  }
  if (impl_->capture) {
    if (const auto button = impl_->controller_capture.pollCapturedButton()) {
      impl_->assignControllerButton(*button);
      return true;
    }
  }
  if (device_changed) {
    impl_->refreshList();
    if (!impl_->capture) {
      const auto status = impl_->deviceDescription();
      SetWindowTextW(impl_->status, status.c_str());
    }
  }
  return true;
}

bool ControlsPage::handleInput(const MSG &message) {
  if (!impl_->visible || !impl_->capture) {
    return false;
  }
  const auto escape =
      (message.message == WM_KEYDOWN || message.message == WM_SYSKEYDOWN) &&
      message.wParam == VK_ESCAPE &&
      (static_cast<std::uint64_t>(message.lParam) & (1ULL << 30U)) == 0U;
  if (escape) {
    impl_->cancelBindingCapture();
    return true;
  }
  if (impl_->controller_mode) {
    return false;
  }
  if (const auto captured = capturedInput(message)) {
    (*impl_->keyboard)[static_cast<KeyboardMouseAction>(*impl_->capture)] =
        *captured;
    impl_->capture.reset();
    impl_->refreshList();
    impl_->setStatus(impl_->localizedText().binding_updated);
    return true;
  }
  return false;
}

bool ControlsPage::handleDrawItem(const DRAWITEMSTRUCT &item) const {
  if (item.CtlType != ODT_BUTTON ||
      (item.CtlID != change_binding_control_id &&
       item.CtlID != clear_binding_control_id &&
       item.CtlID != default_bindings_control_id)) {
    return false;
  }
  impl_->drawButton(item);
  return true;
}

void ControlsPage::shutdown() noexcept {
  hide();
  impl_->controller_capture.shutdown();
  for (auto iterator = impl_->controls.rbegin();
       iterator != impl_->controls.rend(); ++iterator) {
    if (IsWindow(*iterator) != FALSE) {
      DestroyWindow(*iterator);
    }
  }
  impl_->controls.clear();
  impl_->parent = nullptr;
  impl_->keyboard = nullptr;
  impl_->controller = nullptr;
  impl_->protocol = nullptr;
  impl_->vibration = nullptr;
  impl_->capture.reset();
  impl_->visible = false;
}

bool ControlsPage::visible() const noexcept { return impl_->visible; }
bool ControlsPage::capturing() const noexcept {
  return impl_->capture.has_value();
}

} // namespace sf::platform::launcher

#endif
