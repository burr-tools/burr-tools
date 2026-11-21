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

TEST_CASE("layer model region: a point selects its cell, the whole box the whole layer", "[ui][layermodel]") {
  for (gridType_c::gridType t : bttest::ALL_GRIDS) {
    gridType_c gt(t);
    if (!gt.isDescribed()) continue;
    INFO(bttest::gridName(t));
    const LayerModel m(gt, 6, 6, 6);
    Point2 lo, hi;
    m.bounds(lo, hi);
    for (unsigned int l = 0; l < m.layerCount(); l++) {
      const unsigned int n = static_cast<unsigned int>(m.cells(l).size());
      for (unsigned int i = 0; i < n; i++) {
        const Point2 c = m.centre(l, i);
        CHECK(m.hit(l, c) == (int)i);
        const std::vector<CellRef> r = m.region(l, c, c, 0);
        REQUIRE(r.size() == 1);
        CHECK(r[0] == CellRef{l, i});
      }
      CHECK(m.region(l, lo, hi, 0).size() == n);
    }
    CHECK(m.region(m.layerCount(), lo, hi, 0).empty());
    CHECK(m.centre(m.layerCount(), 0).x == 0.0);
  }
}

TEST_CASE("layer model region: a drag between two cells of a row takes the row between them", "[ui][layermodel]") {
  // triangles: from the first to the last cell whose outline the line through
  // the first's centre crosses, the drag along that line takes all of them,
  // up and down triangles alike
  for (gridType_c::gridType t : PRISMS) {
    INFO(bttest::gridName(t));
    gridType_c gt(t);
    const LayerModel m(gt, 8, 8, 2);
    const unsigned int l = 0;
    for (unsigned int i = 0; i < m.cells(l).size(); i++) {
      const std::vector<CellRef> row = m.region(l, m.centre(l, i), m.centre(l, i), LayerModel::TOOL_STACK_X);
      REQUIRE_FALSE(row.empty());
      // every cell of the row lies on the line
      const double y = m.centre(l, i).y;
      for (const CellRef & r : row) {
        double yl = 1e9, yh = -1e9;
        for (const Point2 & p : m.cells(l)[r.index].outline) { yl = std::min(yl, p.y); yh = std::max(yh, p.y); }
        CHECK(yl < y);
        CHECK(y < yh);
      }
      // the drag from the row's leftmost to its rightmost cell along the line
      double xl = 1e9, xh = -1e9;
      for (const CellRef & r : row) { xl = std::min(xl, m.centre(l, r.index).x); xh = std::max(xh, m.centre(l, r.index).x); }
      const std::vector<CellRef> drag = m.region(l, {xl, y}, {xh, y}, 0);
      CHECK(drag.size() == row.size());
    }
  }
}

TEST_CASE("layer model region: stacking in z takes the cells above and below, at most one per layer", "[ui][layermodel]") {
  // the triangular and hexagonal prisms stack along the voxel diagonal
  // (1,1,1), so a box's layers are slanted slices and a column can leave the
  // box; the Cairo prisms stack along z, so every column is full
  for (gridType_c::gridType t : PRISMS) {
    INFO(bttest::gridName(t));
    gridType_c gt(t);
    const LayerModel m(gt, 6, 6, 5);
    for (unsigned int l = 0; l < m.layerCount(); l++)
      for (unsigned int i = 0; i < m.cells(l).size(); i++) {
        const Point2 c = m.centre(l, i);
        const std::vector<CellRef> col = m.region(l, c, c, LayerModel::TOOL_STACK_Z);
        std::set<unsigned int> layers;
        for (const CellRef & r : col) {
          CHECK(layers.insert(r.layer).second);
          const Point2 d = m.centre(r.layer, r.index);
          CHECK(std::hypot(d.x - c.x, d.y - c.y) < 1e-6);
        }
        CHECK(layers.count(l) == 1);
        if (t == gridType_c::GT_CAIRO_PRISM) CHECK(col.size() == m.layerCount());

        // mirror in z: this cell, and the one at the same place in the
        // mirrored layer if there is one
        const std::vector<CellRef> mz = m.region(l, c, c, LayerModel::TOOL_MIRROR_Z);
        REQUIRE_FALSE(mz.empty());
        for (const CellRef & r : mz) CHECK((r.layer == l || r.layer == m.layerCount() - 1 - l));
        if (t == gridType_c::GT_CAIRO_PRISM) CHECK(mz.size() == (2 * l + 1 == m.layerCount() ? 1u : 2u));
      }
  }
}

TEST_CASE("layer model region: mirrors use the grid's plane nearest the middle and add only true images",
          "[ui][layermodel]") {
  for (gridType_c::gridType t : bttest::ALL_GRIDS) {
    gridType_c gt(t);
    if (!gt.isDescribed()) continue;
    for (unsigned int size : {5u, 6u, 7u}) {
      INFO(bttest::gridName(t) << " size " << size);
      const LayerModel m(gt, size, size, size);
      Point2 lo, hi;
      m.bounds(lo, hi);
      for (int axis = 0; axis < 2; axis++) {
        INFO("axis " << axis);
        const unsigned int tool = axis == 0 ? LayerModel::TOOL_MIRROR_X : LayerModel::TOOL_MIRROR_Y;
        // every described grid mirrors in x and in y (Cairo too, about planes
        // that need not pass through the middle of a box)
        const double plane = m.mirrorPlane(axis);
        REQUIRE_FALSE(std::isnan(plane));
        size_t cells = 0, paired = 0, missingAll = 0;
        for (unsigned int l = 0; l < m.layerCount(); l++)
          for (unsigned int i = 0; i < m.cells(l).size(); i++) {
            const Point2 c = m.centre(l, i);
            unsigned int missing = 99;
            const std::vector<CellRef> r = m.region(l, c, c, tool, &missing);
            REQUIRE((r.size() == 1 || r.size() == 2));
            CHECK(missing <= 1);
            missingAll += missing;
            cells++;
            bool self = false;
            for (const CellRef & x : r) {
              self |= x == CellRef{l, i};
              CHECK(x.layer == l);
              if (x == CellRef{l, i}) continue;
              paired++;
              // the image of the centre, and the same cell shape mirrored
              const Point2 d = m.centre(x.layer, x.index);
              const double want[2] = {axis == 0 ? 2 * plane - c.x : c.x, axis == 1 ? 2 * plane - c.y : c.y};
              CHECK(std::fabs(d.x - want[0]) < 1e-5);
              CHECK(std::fabs(d.y - want[1]) < 1e-5);
              CHECK(m.cells(x.layer)[x.index].outline.size() == m.cells(l)[i].outline.size());
            }
            CHECK(self);
            // a cell whose image is missing: its image's centre is outside every
            // cell of the layer, or lands on a cell of another shape
            if (missing == 1) {
              const Point2 img{axis == 0 ? 2 * plane - c.x : c.x, axis == 1 ? 2 * plane - c.y : c.y};
              const int h = m.hit(l, img);
              if (h >= 0) {
                const Point2 e = m.centre(l, (unsigned int)h);
                CHECK(std::hypot(e.x - img.x, e.y - img.y) > 1e-6);
              }
            }
          }
        // the plane is the grid's nearest to the middle: within half a cell's width
        CHECK(std::fabs(plane - (axis == 0 ? (lo.x + hi.x) / 2 : (lo.y + hi.y) / 2)) < 2.0);
        // every grid has partners now, Cairo included
        CHECK(paired > 0);
        CHECK(m.hasMirror(tool));
        // on cubes every cell has its image (those on the plane are their own)
        if (t == gridType_c::GT_DESCRIBED_BRICKS) CHECK(missingAll == 0);
      }
    }
    CHECK_FALSE(LayerModel(gt, 6, 6, 6).hasMirror(LayerModel::TOOL_STACK_X));
  }
}
