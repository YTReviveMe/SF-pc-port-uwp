#pragma once

#include "sf/game/dynamic_lighting.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>

namespace sf::platform {

struct Park2FlameScreenPoint {
  double x{};
  double y{};
};

struct Park2FlameProjectionBasis {
  game::DynamicLightPoint eye;
  game::DynamicLightPoint right;
  game::DynamicLightPoint down;
  game::DynamicLightPoint forward;
  double projection{1.0};
  double near_clip_depth{};
};

struct Park2FlameWorldGeometry {
  std::array<game::DynamicLightPoint, 4U> corners{};
  std::array<game::DynamicLightPoint, 2U> centres{};
  std::array<double, 2U> depths{};
};

// PARK2 stores both a recovered world centre and the exact centre-origin XY
// produced by the retail GTE. The world centre supplies depth only: using its
// lateral coordinates discards the absolute packet midpoint and can reflect
// Girdeux's stream above the nozzle. Unproject the authored XY at that depth
// so rendering and lighting share the same original centreline.
[[nodiscard]] inline std::optional<Park2FlameWorldGeometry>
recoverPark2FlameWorldGeometry(
    std::array<Park2FlameScreenPoint, 4U> screen_corners,
    std::array<game::DynamicLightPoint, 2U> recovered_centres,
    const Park2FlameProjectionBasis &basis) noexcept {
  const auto finite_point = [](const game::DynamicLightPoint &point) {
    return std::isfinite(point.x) && std::isfinite(point.y) &&
           std::isfinite(point.z);
  };
  if (!finite_point(basis.eye) || !finite_point(basis.right) ||
      !finite_point(basis.down) || !finite_point(basis.forward) ||
      !std::isfinite(basis.projection) || basis.projection <= 0.0 ||
      !std::isfinite(basis.near_clip_depth)) {
    return std::nullopt;
  }
  for (const auto &corner : screen_corners) {
    if (!std::isfinite(corner.x) || !std::isfinite(corner.y)) {
      return std::nullopt;
    }
  }

  const auto dot = [](const game::DynamicLightPoint &first,
                      const game::DynamicLightPoint &second) {
    return first.x * second.x + first.y * second.y + first.z * second.z;
  };
  const auto unproject = [&](Park2FlameScreenPoint point, double depth) {
    const auto horizontal = point.x * depth / basis.projection;
    const auto vertical = point.y * depth / basis.projection;
    return game::DynamicLightPoint{
        basis.eye.x + basis.forward.x * depth + basis.right.x * horizontal +
            basis.down.x * vertical,
        basis.eye.y + basis.forward.y * depth + basis.right.y * horizontal +
            basis.down.y * vertical,
        basis.eye.z + basis.forward.z * depth + basis.right.z * horizontal +
            basis.down.z * vertical,
    };
  };

  Park2FlameWorldGeometry result;
  for (std::size_t pair = 0U; pair < result.centres.size(); ++pair) {
    if (!finite_point(recovered_centres[pair])) {
      return std::nullopt;
    }
    const auto delta = game::DynamicLightPoint{
        recovered_centres[pair].x - basis.eye.x,
        recovered_centres[pair].y - basis.eye.y,
        recovered_centres[pair].z - basis.eye.z,
    };
    const auto depth = dot(delta, basis.forward);
    if (!std::isfinite(depth) || depth <= basis.near_clip_depth) {
      return std::nullopt;
    }
    result.depths[pair] = depth;
    const auto first_corner = pair * 2U;
    const auto midpoint = Park2FlameScreenPoint{
        (screen_corners[first_corner].x + screen_corners[first_corner + 1U].x) *
            0.5,
        (screen_corners[first_corner].y + screen_corners[first_corner + 1U].y) *
            0.5,
    };
    result.centres[pair] = unproject(midpoint, depth);
    result.corners[first_corner] =
        unproject(screen_corners[first_corner], depth);
    result.corners[first_corner + 1U] =
        unproject(screen_corners[first_corner + 1U], depth);
  }
  return result;
}
// The exact packet chain belongs to the current guest camera, while Girdeux's
// HMD and weapon are presented from the current native pose. Move the complete
// chain by one constant offset from its nearest centreline endpoint to the
// posed muzzle. A uniform translation retains every authored bend and width.
[[nodiscard]] inline std::optional<game::DynamicLightPoint>
park2FlameAnchorTranslation(std::span<const game::DynamicLightPoint> centreline,
                            game::DynamicLightPoint muzzle) noexcept {
  const auto finite_point = [](const game::DynamicLightPoint &point) {
    return std::isfinite(point.x) && std::isfinite(point.y) &&
           std::isfinite(point.z);
  };
  if (!finite_point(muzzle)) {
    return std::nullopt;
  }
  const game::DynamicLightPoint *nearest{};
  auto nearest_distance = std::numeric_limits<double>::infinity();
  for (const auto &point : centreline) {
    if (!finite_point(point)) {
      continue;
    }
    const auto x = point.x - muzzle.x;
    const auto y = point.y - muzzle.y;
    const auto z = point.z - muzzle.z;
    const auto distance = x * x + y * y + z * z;
    if (distance < nearest_distance) {
      nearest = &point;
      nearest_distance = distance;
    }
  }
  if (nearest == nullptr) {
    return std::nullopt;
  }
  return game::DynamicLightPoint{muzzle.x - nearest->x, muzzle.y - nearest->y,
                                 muzzle.z - nearest->z};
}

inline void
translatePark2FlameWorldGeometry(Park2FlameWorldGeometry &geometry,
                                 game::DynamicLightPoint translation) noexcept {
  const auto translate = [translation](game::DynamicLightPoint &point) {
    point.x += translation.x;
    point.y += translation.y;
    point.z += translation.z;
  };
  for (auto &point : geometry.corners) {
    translate(point);
  }
  for (auto &point : geometry.centres) {
    translate(point);
  }
}

} // namespace sf::platform
