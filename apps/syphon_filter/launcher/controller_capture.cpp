#include "controller_capture.hpp"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <SDL.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cwctype>
#include <filesystem>
#include <string>
#include <string_view>

namespace sf::platform::launcher {
namespace {
std::wstring widenAscii(std::string_view text) {
  return {text.begin(), text.end()};
}

std::wstring widenUtf8(std::string_view text) {
  if (text.empty()) {
    return {};
  }
  const auto required =
      MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                          static_cast<int>(text.size()), nullptr, 0);
  if (required <= 0) {
    return widenAscii(text);
  }
  std::wstring result(static_cast<std::size_t>(required), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                      static_cast<int>(text.size()), result.data(), required);
  return result;
}

std::filesystem::path executableDirectory() {
  std::array<wchar_t, 32768U> buffer{};
  const auto length = GetModuleFileNameW(nullptr, buffer.data(),
                                         static_cast<DWORD>(buffer.size()));
  if (length == 0U || length >= buffer.size()) {
    return std::filesystem::current_path();
  }
  return std::filesystem::path{buffer.data()}.parent_path();
}

void configureControllerProtocol(ControllerProtocol protocol) noexcept {
  const auto set = [](const char *name, bool enabled) {
    static_cast<void>(
        SDL_SetHintWithPriority(name, enabled ? "1" : "0", SDL_HINT_OVERRIDE));
  };
  const auto automatic = protocol == ControllerProtocol::automatic;
  set(SDL_HINT_XINPUT_ENABLED,
      automatic || protocol == ControllerProtocol::xinput);
  set(SDL_HINT_DIRECTINPUT_ENABLED,
      automatic || protocol == ControllerProtocol::direct_input);
  set(SDL_HINT_JOYSTICK_RAWINPUT,
      automatic || protocol == ControllerProtocol::raw_input);
  set(SDL_HINT_JOYSTICK_RAWINPUT_CORRELATE_XINPUT, true);
  set(SDL_HINT_JOYSTICK_WGI, automatic);
  set(SDL_HINT_JOYSTICK_HIDAPI, automatic);
}

ControllerPromptFamily controllerFamilyFromName(std::wstring name) noexcept {
  std::ranges::transform(name, name.begin(),
                         [](wchar_t value) { return std::towlower(value); });
  const auto contains = [&name](std::wstring_view value) {
    return name.find(value) != std::wstring::npos;
  };
  if (contains(L"xbox") || contains(L"xinput")) {
    return ControllerPromptFamily::xbox;
  }
  if (contains(L"playstation") || contains(L"dualshock") ||
      contains(L"dualsense") || contains(L"ps3") || contains(L"ps4") ||
      contains(L"ps5")) {
    return ControllerPromptFamily::playstation;
  }
  if (contains(L"nintendo") || contains(L"switch") || contains(L"joy-con")) {
    return ControllerPromptFamily::nintendo;
  }
  return ControllerPromptFamily::generic;
}

} // namespace

struct ControllerCapture::Impl {
  bool initialize(ControllerProtocol protocol) noexcept {
    if (initialized_) {
      return true;
    }
    configureControllerProtocol(protocol);
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
      return false;
    }
    initialized_ = true;
    SDL_JoystickEventState(SDL_ENABLE);
    SDL_GameControllerEventState(SDL_ENABLE);
    static_cast<void>(SDL_GameControllerAddMappingsFromFile(
        (executableDirectory() / L"gamecontrollerdb.txt").string().c_str()));
    static_cast<void>(update());
    return true;
  }

  bool update() noexcept {
    if (!initialized_) {
      return false;
    }
    const auto previous_instance = instance_id_;
    const auto previous_family = family_;
    const auto previous_name = name_;
    SDL_PumpEvents();
    SDL_GameControllerUpdate();
    SDL_JoystickUpdate();
    SDL_FlushEvents(SDL_JOYAXISMOTION, SDL_JOYDEVICEREMOVED);
    SDL_FlushEvents(SDL_CONTROLLERAXISMOTION, SDL_CONTROLLERDEVICEREMAPPED);
    if (joystick_ != nullptr &&
        SDL_JoystickGetAttached(joystick_) == SDL_FALSE) {
      closeController();
    }
    if (joystick_ == nullptr) {
      openFirstController();
    }
    return previous_instance != instance_id_ || previous_family != family_ ||
           previous_name != name_;
  }

  [[nodiscard]] bool connected() const noexcept { return joystick_ != nullptr; }
  [[nodiscard]] ControllerPromptFamily family() const noexcept {
    return family_;
  }
  [[nodiscard]] std::wstring_view name() const noexcept { return name_; }

  void beginCapture() noexcept {
    capture_active_ = true;
    previous_buttons_ = sampleButtons();
    capture_armed_ = previous_buttons_ == 0U;
    previous_cancel_ = sampleCancelButton();
  }

  void cancelCapture() noexcept {
    capture_active_ = false;
    capture_armed_ = false;
    previous_buttons_ = 0U;
  }

  [[nodiscard]] bool pollCancelRequest() noexcept {
    if (!capture_active_) {
      return false;
    }
    const auto cancel = sampleCancelButton();
    const auto pressed = cancel && !previous_cancel_;
    previous_cancel_ = cancel;
    if (pressed) {
      cancelCapture();
    }
    return pressed;
  }

  [[nodiscard]] std::optional<std::uint32_t> pollCapturedButton() noexcept {
    if (!capture_active_) {
      return std::nullopt;
    }
    static_cast<void>(update());
    const auto buttons = sampleButtons();
    if (!capture_armed_) {
      previous_buttons_ = buttons;
      if (buttons == 0U) {
        capture_armed_ = true;
      }
      return std::nullopt;
    }
    const auto pressed = buttons & ~previous_buttons_;
    previous_buttons_ = buttons;
    if (!std::has_single_bit(pressed) ||
        (pressed & game::bindable_controller_button_mask) != pressed) {
      return std::nullopt;
    }
    cancelCapture();
    return pressed;
  }

  void shutdown() noexcept {
    cancelCapture();
    closeController();
    if (initialized_) {
      SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
      initialized_ = false;
    }
  }

  static ControllerPromptFamily
  familyFromController(SDL_GameController *controller,
                       std::wstring_view name) noexcept {
    switch (SDL_GameControllerGetType(controller)) {
    case SDL_CONTROLLER_TYPE_XBOX360:
    case SDL_CONTROLLER_TYPE_XBOXONE:
      return ControllerPromptFamily::xbox;
    case SDL_CONTROLLER_TYPE_PS3:
    case SDL_CONTROLLER_TYPE_PS4:
    case SDL_CONTROLLER_TYPE_PS5:
      return ControllerPromptFamily::playstation;
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
      return ControllerPromptFamily::nintendo;
    default:
      return controllerFamilyFromName(std::wstring{name});
    }
  }

  void openFirstController() noexcept {
    for (auto device_index = 0; device_index < SDL_NumJoysticks();
         ++device_index) {
      if (SDL_IsGameController(device_index) == SDL_TRUE) {
        controller_ = SDL_GameControllerOpen(device_index);
        if (controller_ != nullptr) {
          joystick_ = SDL_GameControllerGetJoystick(controller_);
        }
      } else {
        joystick_ = SDL_JoystickOpen(device_index);
      }
      if (joystick_ == nullptr) {
        continue;
      }
      instance_id_ = SDL_JoystickInstanceID(joystick_);
      const auto *device_name = controller_ != nullptr
                                    ? SDL_GameControllerName(controller_)
                                    : SDL_JoystickName(joystick_);
      name_ = widenUtf8(device_name != nullptr ? device_name : "Controller");
      family_ = controller_ != nullptr
                    ? familyFromController(controller_, name_)
                    : controllerFamilyFromName(name_);
      return;
    }
  }

  void closeController() noexcept {
    if (controller_ != nullptr) {
      SDL_GameControllerClose(controller_);
    } else if (joystick_ != nullptr) {
      SDL_JoystickClose(joystick_);
    }
    controller_ = nullptr;
    joystick_ = nullptr;
    instance_id_ = -1;
    family_ = ControllerPromptFamily::generic;
    name_.clear();
  }

  [[nodiscard]] std::uint32_t sampleButtons() const noexcept {
    if (joystick_ == nullptr) {
      return 0U;
    }
    std::uint32_t result{};
    const auto set = [&result](std::uint32_t mask, bool pressed) {
      if (pressed) {
        result |= mask;
      }
    };
    if (controller_ != nullptr) {
      const auto button = [this](SDL_GameControllerButton value) {
        return SDL_GameControllerGetButton(controller_, value) != 0;
      };
      set(game::controller_cross_button, button(SDL_CONTROLLER_BUTTON_A));
      set(game::controller_circle_button, button(SDL_CONTROLLER_BUTTON_B));
      set(game::controller_square_button, button(SDL_CONTROLLER_BUTTON_X));
      set(game::controller_triangle_button, button(SDL_CONTROLLER_BUTTON_Y));
      set(game::controller_l1_button,
          button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER));
      set(game::controller_r1_button,
          button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER));
      set(game::controller_select_button, button(SDL_CONTROLLER_BUTTON_BACK));
      set(game::controller_l2_button,
          SDL_GameControllerGetAxis(controller_,
                                    SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16384);
      set(game::controller_r2_button,
          SDL_GameControllerGetAxis(controller_,
                                    SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16384);
      return result & game::bindable_controller_button_mask;
    }

    const auto joystick_button = [this](int index) {
      return index < SDL_JoystickNumButtons(joystick_) &&
             SDL_JoystickGetButton(joystick_, index) != 0;
    };
    set(game::controller_cross_button, joystick_button(0));
    set(game::controller_circle_button, joystick_button(1));
    set(game::controller_square_button, joystick_button(2));
    set(game::controller_triangle_button, joystick_button(3));
    set(game::controller_l1_button, joystick_button(4));
    set(game::controller_r1_button, joystick_button(5));
    set(game::controller_select_button, joystick_button(6));
    const auto axes = SDL_JoystickNumAxes(joystick_);
    const auto buttons = SDL_JoystickNumButtons(joystick_);
    const auto button_triggers = axes < 5 && buttons >= 12;
    if (button_triggers) {
      set(game::controller_l2_button, joystick_button(10));
      set(game::controller_r2_button, joystick_button(11));
    } else if (axes == 5) {
      const auto trigger_axis = SDL_JoystickGetAxis(joystick_, 2);
      set(game::controller_l2_button, trigger_axis < -16384);
      set(game::controller_r2_button, trigger_axis > 16384);
    } else if (axes >= 6) {
      set(game::controller_l2_button,
          SDL_JoystickGetAxis(joystick_, 4) > 16384);
      set(game::controller_r2_button,
          SDL_JoystickGetAxis(joystick_, 5) > 16384);
    }
    return result & game::bindable_controller_button_mask;
  }

  [[nodiscard]] bool sampleCancelButton() const noexcept {
    if (controller_ != nullptr) {
      return SDL_GameControllerGetButton(controller_,
                                         SDL_CONTROLLER_BUTTON_START) != 0;
    }
    return joystick_ != nullptr && SDL_JoystickNumButtons(joystick_) > 7 &&
           SDL_JoystickGetButton(joystick_, 7) != 0;
  }

  bool initialized_{};
  SDL_GameController *controller_{};
  SDL_Joystick *joystick_{};
  SDL_JoystickID instance_id_{-1};
  ControllerPromptFamily family_{ControllerPromptFamily::generic};
  std::wstring name_;
  bool capture_active_{};
  bool capture_armed_{};
  std::uint32_t previous_buttons_{};
  bool previous_cancel_{};
};

ControllerCapture::ControllerCapture() : impl_(std::make_unique<Impl>()) {}

ControllerCapture::~ControllerCapture() { shutdown(); }

bool ControllerCapture::initialize(ControllerProtocol protocol) noexcept {
  return impl_->initialize(protocol);
}

bool ControllerCapture::update() noexcept { return impl_->update(); }

bool ControllerCapture::connected() const noexcept {
  return impl_->connected();
}

ControllerPromptFamily ControllerCapture::family() const noexcept {
  return impl_->family();
}

std::wstring_view ControllerCapture::name() const noexcept {
  return impl_->name();
}

void ControllerCapture::beginCapture() noexcept { impl_->beginCapture(); }

void ControllerCapture::cancelCapture() noexcept { impl_->cancelCapture(); }

bool ControllerCapture::pollCancelRequest() noexcept {
  return impl_->pollCancelRequest();
}

std::optional<std::uint32_t> ControllerCapture::pollCapturedButton() noexcept {
  return impl_->pollCapturedButton();
}

void ControllerCapture::shutdown() noexcept { impl_->shutdown(); }

} // namespace sf::platform::launcher

#endif
