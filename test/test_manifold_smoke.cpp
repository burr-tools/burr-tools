#include <catch2/catch_test_macros.hpp>
#include <manifold/manifold.h>
#include <cmath>

TEST_CASE("manifold smoke: hull, minkowski, boolean", "[manifold]") {
  using namespace manifold;
  Manifold cube = Manifold::Cube({1, 1, 1});
  std::vector<vec3> oct = {{0.1, 0, 0}, {-0.1, 0, 0}, {0, 0.1, 0}, {0, -0.1, 0}, {0, 0, 0.1}, {0, 0, -0.1}};
  Manifold bit = Manifold::Hull(oct);
  Manifold sum = cube.MinkowskiSum(bit);
  CHECK(sum.Status() == Manifold::Error::NoError);
  CHECK(sum.Volume() > cube.Volume());
  CHECK(sum.Genus() == 0);
  Manifold diff = sum - cube;
  CHECK(diff.Status() == Manifold::Error::NoError);
  CHECK(std::fabs(diff.Volume() - (sum.Volume() - cube.Volume())) < 1e-9);
}
