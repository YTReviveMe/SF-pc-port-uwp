#pragma once

#ifdef _WIN32

#include "sf/platform/host.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

namespace sf::platform::launcher {

// Launcher-side SDL controller discovery and button capture. SDL types stay
// private so ControlsPage depends only on controller-domain values.
class ControllerCapture final {
public:
  ControllerCapture();
  ~ControllerCapture();

  ControllerCapture(const ControllerCapture &) = delete;
  ControllerCapture &operator=(const ControllerCapture &) = delete;
  ControllerCapture(ControllerCapture &&) = delete;
  ControllerCapture &operator=(ControllerCapture &&) = delete;

  [[nodiscard]] bool initialize(ControllerProtocol protocol) noexcept;
  [[nodiscard]] bool update() noexcept;

  [[nodiscard]] bool connected() const noexcept;
  [[nodiscard]] ControllerPromptFamily family() const noexcept;
  [[nodiscard]] std::wstring_view name() const noexcept;

  void beginCapture() noexcept;
  void cancelCapture() noexcept;
  [[nodiscard]] bool pollCancelRequest() noexcept;
  [[nodiscard]] std::optional<std::uint32_t> pollCapturedButton() noexcept;
  void shutdown() noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace sf::platform::launcher

#endif