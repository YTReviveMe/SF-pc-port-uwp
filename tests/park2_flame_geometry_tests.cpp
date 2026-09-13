#include "sf/platform/park2_flame_geometry.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char *message) {
  if (!condition) {
    throw std::runtime_error{message};
  }
}

bool near(double first, double second) {
  return std::abs(first - second) <= 0.000001;
}

void testAbsolutePacketMidpointOwnsCentreline() {
  const auto geometry = sf::platform::recoverPark2FlameWorldGeometry(
      std::array{
          sf::platform::Park2FlameScreenPoint{9.0, 19.0},
          sf::platform::Park2FlameScreenPoint{11.0, 21.0},
          sf::platform::Park2FlameScreenPoint{19.0, 39.0},
          sf::platform::Park2FlameScreenPoint{21.0, 41.0},
      },
      std::array{
          sf::game::DynamicLightPoint{999.0, -999.0, 230.0},
          sf::game::DynamicLightPoint{-999.0, 999.0, 430.0},
      },
      sf::platform::Park2FlameProjectionBasis{
          {10.0, 20.0, 30.0},
          {1.0, 0.0, 0.0},
          {0.0, 1.0, 0.0},
          {0.0, 0.0, 1.0},
          100.0,
          32.0,
      });
  require(geometry.has_value(), "valid PARK2 flame geometry was rejected");
  require(near(geometry->centres[0].x, 30.0) &&
              near(geometry->centres[0].y, 60.0) &&
              near(geometry->centres[0].z, 230.0),
          "first centre did not use the absolute retail packet midpoint");
  require(near(geometry->centres[1].x, 90.0) &&
              near(geometry->centres[1].y, 180.0) &&
              near(geometry->centres[1].z, 430.0),
          "second centre did not use the absolute retail packet midpoint");
  require(near(geometry->corners[0].x, 28.0) &&
              near(geometry->corners[0].y, 58.0) &&
              near(geometry->corners[0].z, 230.0),
          "retail corner was not unprojected at recovered depth");
}

void testNearClippedGeometryFailsClosed() {
  const auto geometry = sf::platform::recoverPark2FlameWorldGeometry(
      {},
      std::array{
          sf::game::DynamicLightPoint{0.0, 0.0, 31.0},
          sf::game::DynamicLightPoint{0.0, 0.0, 64.0},
      },
      sf::platform::Park2FlameProjectionBasis{
          {},
          {1.0, 0.0, 0.0},
          {0.0, 1.0, 0.0},
          {0.0, 0.0, 1.0},
          100.0,
          32.0,
      });
  require(!geometry, "near-clipped PARK2 flame geometry was accepted");
}

} // namespace

int main() {
  try {
    testAbsolutePacketMidpointOwnsCentreline();
    testNearClippedGeometryFailsClosed();
    std::cout << "park2 flame geometry tests passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
