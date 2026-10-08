/* Tests for btui::DepthSorter, the 3D view's back-to-front order of
 * translucent triangles, and the allocation counter the performance tests
 * share (alloccount.h). */
#include <catch2/catch_test_macros.hpp>

#include "alloccount.h"
#include "../src/uicore/depthsort.h"

#include <cmath>

using btui::DepthSorter;
using btui::Mat4;
using btui::Vec3;

namespace {

  /* a view that looks down -z after turning `deg` degrees about y, pulled
   * back by `back` along its own z */
  Mat4 viewTurned(float deg, float back = 0.0f) {
    const float r = deg * 3.14159265f / 180.0f;
    Mat4 m;
    m(0, 0) = std::cos(r);  m(0, 2) = std::sin(r);
    m(2, 0) = -std::sin(r); m(2, 2) = std::cos(r);
    m(2, 3) = -back;
    return m;
  }

  /* centres spread along z: triangle i at z = i */
  std::vector<Vec3> row(size_t n) {
    std::vector<Vec3> c;
    for (size_t i = 0; i < n; i++)
      c.push_back({ 0.0f, 0.0f, float(i) });
    return c;
  }
}

TEST_CASE("triangles come farthest first, three indices each", "[ui][depthsort]") {
  DepthSorter s;
  std::vector<std::uint32_t> idx;
  // looking down -z: the most negative z is farthest, triangle 0 at z = 0
  // is farther than triangle 2 at z = 2
  s.sort(row(3), Mat4{}, 100, idx);
  REQUIRE(idx == std::vector<std::uint32_t>{ 100, 101, 102, 103, 104, 105, 106, 107, 108 });
  // turned round, the order turns round
  s.sort(row(3), viewTurned(180), 100, idx);
  REQUIRE(idx == std::vector<std::uint32_t>{ 106, 107, 108, 103, 104, 105, 100, 101, 102 });
}

TEST_CASE("only the view's rotation changes the order", "[ui][depthsort]") {
  // panning and zooming move the view along or across its axes: the depth
  // order stays, so the 3D view does not sort again for them
  CHECK(btui::sameRotation(viewTurned(30), viewTurned(30, 5.0f)));
  Mat4 panned = viewTurned(30);
  panned(0, 3) = 2.0f;
  panned(1, 3) = -1.0f;
  CHECK(btui::sameRotation(viewTurned(30), panned));
  CHECK_FALSE(btui::sameRotation(viewTurned(30), viewTurned(31)));
}

TEST_CASE("sorting again for another view allocates nothing", "[ui][depthsort][alloc]") {
  // the 3D view sorts every frame of an orbit; once the sorter has sorted a
  // list, its storage and the index list's are reused
  const std::vector<Vec3> centres = row(20000);
  DepthSorter s;
  std::vector<std::uint32_t> idx;
  s.sort(centres, viewTurned(0), 0, idx);
  const btui::test::AllocCount count;
  for (int deg = 1; deg <= 30; deg++)
    s.sort(centres, viewTurned(float(deg)), 0, idx);
  CHECK(count.bytes() == 0);
  CHECK(count.calls() == 0);
  CHECK(idx.size() == centres.size() * 3);
}
