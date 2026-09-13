#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace sf::platform {

inline constexpr std::uint32_t persistent_fire_volume_flag = 1U;

// A persistent flame must retain enough authored depth to read as a volume
// from oblique cameras instead of collapsing back into a billboard.
inline constexpr double persistent_fire_minimum_depth_ratio = 0.82;

struct PersistentFireVolumeLayout {
  double center_x{};
  double center_y{};
  double center_z{};
  double radius_x{};
  double radius_y{};
  double radius_z{};
};

[[nodiscard]] constexpr std::uint32_t
persistentFireVolumeSeed(std::size_t emitter_identity) noexcept {
  auto value = static_cast<std::uint32_t>(emitter_identity) + 0x9e3779b9U;
  value ^= value >> 16U;
  value *= 0x7feb352dU;
  value ^= value >> 15U;
  value *= 0x846ca68bU;
  value ^= value >> 16U;
  return value;
}

// Destroyed Subway keeps CFIREB/CFIREC outside the resident guest object
// table, but their retail particle controller still writes the exact mission
// source index into source_slot. Recover that authored identity only while the
// scene object has no guest binding; a live binding remains authoritative.
[[nodiscard]] constexpr bool persistentFireSourceMatchesSceneObject(
    std::uint32_t mission_index, std::int16_t source_slot,
    std::int32_t scene_guest_slot, std::size_t scene_source_index) noexcept {
  if (source_slot < 0) {
    return false;
  }
  if (scene_guest_slot >= 0) {
    return source_slot == scene_guest_slot;
  }
  constexpr auto destroyed_subway_mission_index = std::uint32_t{1U};
  return mission_index == destroyed_subway_mission_index &&
         static_cast<std::size_t>(source_slot) == scene_source_index;
}

// FUN_8004fd20's two attached EXPL particles remain inside this exact
// controller-relative envelope for their complete recycling lifetime. A
// single point can overlap neighbouring Base Escape emitters, so ownership is
// accepted only when every live member of the controller group fits exactly
// one candidate.
inline constexpr std::int32_t persistent_fire_particle_minimum_x = -152;
inline constexpr std::int32_t persistent_fire_particle_maximum_x = 207;
inline constexpr std::int32_t persistent_fire_particle_minimum_y = -189;
inline constexpr std::int32_t persistent_fire_particle_maximum_y = 1;
inline constexpr std::int32_t persistent_fire_particle_minimum_z = -152;
inline constexpr std::int32_t persistent_fire_particle_maximum_z = 207;

struct PersistentFireParticlePoint {
  std::int32_t x{};
  std::int32_t y{};
  std::int32_t z{};
};

struct PersistentFireEmitterCandidate {
  std::size_t identity{};
  PersistentFireParticlePoint anchor;
};

[[nodiscard]] constexpr bool persistentFireParticleFitsEmitter(
    const PersistentFireParticlePoint &particle,
    const PersistentFireEmitterCandidate &emitter) noexcept {
  const auto x = static_cast<std::int64_t>(particle.x) - emitter.anchor.x;
  const auto y = static_cast<std::int64_t>(particle.y) - emitter.anchor.y;
  const auto z = static_cast<std::int64_t>(particle.z) - emitter.anchor.z;
  return x >= persistent_fire_particle_minimum_x &&
         x <= persistent_fire_particle_maximum_x &&
         y >= persistent_fire_particle_minimum_y &&
         y <= persistent_fire_particle_maximum_y &&
         z >= persistent_fire_particle_minimum_z &&
         z <= persistent_fire_particle_maximum_z;
}

[[nodiscard]] constexpr std::optional<std::size_t>
uniquePersistentFireEmitterMatch(
    std::span<const PersistentFireParticlePoint> controller_group,
    std::span<const PersistentFireEmitterCandidate> emitters) noexcept {
  if (controller_group.empty()) {
    return std::nullopt;
  }
  auto match = std::optional<std::size_t>{};
  for (const auto &emitter : emitters) {
    if (!std::ranges::all_of(controller_group, [&](const auto &particle) {
          return persistentFireParticleFitsEmitter(particle, emitter);
        })) {
      continue;
    }
    if (match) {
      return std::nullopt;
    }
    match = emitter.identity;
  }
  return match;
}

// Static CFIRE is rooted at an authored floor contact. Keep the analytic
// volume above that point; the retail particles may rise and recycle without
// moving the volume envelope itself.
[[nodiscard]] constexpr PersistentFireVolumeLayout persistentFireVolumeLayout(
    double anchor_x, double anchor_y, double anchor_z, double half_width,
    double half_height, double depth_radius) noexcept {
  const auto radius_x = std::max(half_width, 1.0);
  const auto radius_y = std::max(half_height, 1.0);
  const auto radius_z =
      std::max(std::max(depth_radius,
                        radius_x * persistent_fire_minimum_depth_ratio), 1.0);
  return {anchor_x, anchor_y - radius_y, anchor_z, radius_x, radius_y,
          radius_z};
}

} // namespace sf::platform
