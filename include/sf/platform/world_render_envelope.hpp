#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>

namespace sf::platform {

struct WorldRenderEnvelopeSelection {
  std::size_t render_count{};
  std::size_t admitted_lookahead_count{};
  bool topology_valid{};
};

// terrain_models is authored as one connected route whose exact first segment
// is the current presentation shell. Rendering may expose only a cumulative
// resource-ready prefix of that route; a gap would show a future interior
// through the current room boundary.
[[nodiscard]] inline WorldRenderEnvelopeSelection
selectWorldRenderEnvelope(std::span<const std::uint16_t> presentation_models,
                          std::span<const std::uint16_t> terrain_models,
                          std::size_t admitted_lookahead_count) noexcept {
  const auto fail_closed =
      WorldRenderEnvelopeSelection{presentation_models.size(), 0U, false};
  if (terrain_models.size() < presentation_models.size() ||
      !std::ranges::equal(terrain_models.first(presentation_models.size()),
                          presentation_models)) {
    return fail_closed;
  }

  // Duplicate identities mean the route is no longer a strict topology
  // prefix. Do not let a bogus admission count jump across that discontinuity.
  for (auto index = std::size_t{}; index < terrain_models.size(); ++index) {
    if (std::ranges::find(terrain_models.first(index), terrain_models[index]) !=
        terrain_models.begin() + static_cast<std::ptrdiff_t>(index)) {
      return fail_closed;
    }
  }

  const auto available_lookahead =
      terrain_models.size() - presentation_models.size();
  const auto admitted = std::min(admitted_lookahead_count, available_lookahead);
  return WorldRenderEnvelopeSelection{presentation_models.size() + admitted,
                                      admitted, true};
}

[[nodiscard]] inline std::span<const std::uint16_t>
worldRenderEnvelope(std::span<const std::uint16_t> presentation_models,
                    std::span<const std::uint16_t> terrain_models,
                    std::size_t admitted_lookahead_count) noexcept {
  const auto selection = selectWorldRenderEnvelope(
      presentation_models, terrain_models, admitted_lookahead_count);
  if (!selection.topology_valid) {
    return presentation_models;
  }
  return terrain_models.first(selection.render_count);
}

} // namespace sf::platform
