/* The layer model of a 2D editor for the described grids (src/uicore/layermodel,
 * design/2026-11-19-ng-merge-plan.md 5.6).
 */
#include <catch2/catch_test_macros.hpp>

#include "uicore/layermodel.h"
#include "lib/gridtype.h"
#include "lib/latticegridinfo.h"
#include "test_helpers.h"

#include <cmath>
#include <map>
#include <set>

using namespace btui;

namespace {

Point2 centroid(const std::vector<Point2> & poly) {
  Point2 c;
  for (const Point2 & p : poly) { c.x += p.x; c.y += p.y; }
  c.x /= poly.size();
  c.y /= poly.size();
  return c;
}

const gridType_c::gridType PRISMS[] = {
  gridType_c::GT_DESCRIBED_TRIANGULAR_PRISM, gridType_c::GT_HEXAGONAL_PRISM, gridType_c::GT_CAIRO_PRISM,
};

size_t cornersOf(gridType_c::gridType t) {
  switch (t) {
    case gridType_c::GT_DESCRIBED_TRIANGULAR_PRISM: return 3;
    case gridType_c::GT_HEXAGONAL_PRISM: return 6;
    default: return 5;  // Cairo pentagons
  }
}

} // namespace

TEST_CASE("layer model: the native grids and an empty space have no layers, and asking past the end is harmless",
          "[ui][layermodel]") {
  for (gridType_c::gridType t : bttest::ALL_GRIDS) {
    INFO(bttest::gridName(t));
    gridType_c gt(t);
    const LayerModel m(gt, 6, 6, 6);
    CHECK((m.layerCount() > 0) == gt.isDescribed());
    // a layer that does not exist: nothing, no throw
    CHECK(m.cells(m.layerCount()).empty());
    CHECK(m.height(m.layerCount()) == 0.0);
    CHECK(m.hit(m.layerCount(), {0, 0}) == -1);
    CHECK(m.layerOf(-1, 0, 0) == -1);
    CHECK(m.layerOf(6, 0, 0) == -1);

    const LayerModel empty(gt, 0, 6, 6);
    CHECK(empty.layerCount() == 0);
  }
}

TEST_CASE("layer model: every outline is a convex counter-clockwise polygon, layers go up, every cell has a home layer",
          "[ui][layermodel]") {
  for (gridType_c::gridType t : bttest::ALL_GRIDS) {
    gridType_c gt(t);
    if (!gt.isDescribed()) continue;
    INFO(bttest::gridName(t));
    const LayerModel m(gt, 6, 6, 6);
    std::set<std::array<int, 3>> seen;
    for (unsigned int l = 0; l < m.layerCount(); l++) {
      if (l > 0) CHECK(m.height(l) > m.height(l - 1));
      CHECK_FALSE(m.cells(l).empty());
      for (const LayerCell & c : m.cells(l)) {
        REQUIRE(c.outline.size() >= 3);
        CHECK(polygonArea(c.outline) > 1e-6);
        // convex: every corner turns left
        for (size_t i = 0; i < c.outline.size(); i++) {
          const Point2 & a = c.outline[i];
          const Point2 & b = c.outline[(i + 1) % c.outline.size()];
          const Point2 & d = c.outline[(i + 2) % c.outline.size()];
          CHECK((b.x - a.x) * (d.y - a.y) - (b.y - a.y) * (d.x - a.x) > 0);
        }
        seen.insert(c.voxel);
      }
    }
    for (const auto & v : seen) {
      const int home = m.layerOf(v[0], v[1], v[2]);
      REQUIRE(home >= 0);
      bool there = false;
      for (const LayerCell & c : m.cells(home)) there |= c.voxel == v;
      CHECK(there);
    }
  }
}

TEST_CASE("layer model: on the prism grids every cell is in exactly one layer, with the prism's cross-section",
          "[ui][layermodel]") {
  for (gridType_c::gridType t : PRISMS) {
    INFO(bttest::gridName(t));
    gridType_c gt(t);
    const LayerModel m(gt, 8, 8, 8);
    REQUIRE(m.layerCount() > 1);
    std::map<std::array<int, 3>, int> count;
    double area = -1;
    for (unsigned int l = 0; l < m.layerCount(); l++)
      for (const LayerCell & c : m.cells(l)) {
        count[c.voxel]++;
        CHECK(c.outline.size() == cornersOf(t));
        CHECK(m.layerOf(c.voxel[0], c.voxel[1], c.voxel[2]) == (int)l);
        if (t != gridType_c::GT_CAIRO_PRISM) {
          // triangles and hexagons are all congruent
          if (area < 0) area = polygonArea(c.outline);
          CHECK(std::fabs(polygonArea(c.outline) - area) < 1e-4);
        }
      }
    for (const auto & [v, n] : count) CHECK(n == 1);
  }
}

TEST_CASE("layer model: a layer's outlines do not overlap, and a cell's centre hits that cell",
          "[ui][layermodel]") {
  for (gridType_c::gridType t : bttest::ALL_GRIDS) {
    gridType_c gt(t);
    if (!gt.isDescribed()) continue;
    INFO(bttest::gridName(t));
    const LayerModel m(gt, 6, 6, 6);
    for (unsigned int l = 0; l < m.layerCount(); l++) {
      const std::vector<LayerCell> & cs = m.cells(l);
      for (size_t i = 0; i < cs.size(); i++) {
        const Point2 c = centroid(cs[i].outline);
        CHECK(m.hit(l, c) == (int)i);
        // the cross-sections of cells that fill space cannot overlap
        for (size_t j = 0; j < cs.size(); j++)
          if (j != i) CHECK_FALSE(inConvexPolygon(cs[j].outline, c));
      }
      // far outside the space
      Point2 lo, hi;
      m.bounds(lo, hi);
      CHECK(m.hit(l, {hi.x + 100, hi.y + 100}) == -1);
      for (const LayerCell & c : cs)
        for (const Point2 & p : c.outline) {
          CHECK(p.x >= lo.x - 1e-9); CHECK(p.y >= lo.y - 1e-9);
          CHECK(p.x <= hi.x + 1e-9); CHECK(p.y <= hi.y + 1e-9);
        }
    }
  }
}

TEST_CASE("layer model: every cell of the space is in some layer", "[ui][layermodel]") {
  for (gridType_c::gridType t : bttest::ALL_GRIDS) {
    gridType_c gt(t);
    if (!gt.isDescribed()) continue;
    INFO(bttest::gridName(t));
    const LayerModel m(gt, 6, 6, 6);
    std::set<std::array<int, 3>> listed;
    for (unsigned int l = 0; l < m.layerCount(); l++)
      for (const LayerCell & c : m.cells(l)) listed.insert(c.voxel);
    const latticeGridInfo_c & inf = latticeGridInfo_c::get(t);
    std::set<std::array<int, 3>> voxels;
    for (int z = 0; z < 6; z++)
      for (int y = 0; y < 6; y++)
        for (int x = 0; x < 6; x++)
          if (inf.isVoxel(x, y, z)) voxels.insert({x, y, z});
    CHECK(listed == voxels);
  }
}

TEST_CASE("layer model: polygon helpers", "[ui][layermodel]") {
  const std::vector<Point2> square{{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  CHECK(polygonArea(square) == 1.0);
  CHECK(inConvexPolygon(square, {0.5, 0.5}));
  CHECK(inConvexPolygon(square, {1, 0.5}));  // on the border
  CHECK_FALSE(inConvexPolygon(square, {1.5, 0.5}));
  CHECK_FALSE(inConvexPolygon({{0, 0}, {1, 0}}, {0.5, 0}));
}
