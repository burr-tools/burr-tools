/* Tests for btui's 2D grid cells: one cell per voxel of a layer, in the
 * shape of its voxel type, none overlapping, every one found by a hit test
 * at its centre (C08).
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "test_helpers.h"

#include "../src/uicore/gridcells.h"

#include "../src/lib/voxel.h"

#include <set>
#include <utility>
#include <vector>

using namespace btui;
using Catch::Matchers::WithinAbs;

namespace {
  Pt centroid(const GridCell & c) {
    if (c.round)
      return c.centre;
    Pt s;
    for (const Pt & p : c.outline) {
      s.x += p.x;
      s.y += p.y;
    }
    return { s.x / double(c.outline.size()), s.y / double(c.outline.size()) };
  }

  double area(const std::vector<Pt> & poly) {
    double a = 0;
    for (size_t i = 0; i < poly.size(); i++) {
      const Pt & p = poly[i];
      const Pt & q = poly[(i + 1) % poly.size()];
      a += p.x * q.y - q.x * p.y;
    }
    return a / 2;
  }

  /* the voxels of a layer the voxel type has */
  int validInLayer(const voxel_c & v, Plane p, int layer) {
    const PlaneAxes a = planeAxes(p);
    const int d[3] = { int(v.getX()), int(v.getY()), int(v.getZ()) };
    int n = 0;
    for (int r = 0; r < d[a.row]; r++)
      for (int c = 0; c < d[a.col]; c++) {
        const Cell3 x = voxelAt(p, c, r, layer);
        n += v.validCoordinate(x.x, x.y, x.z) ? 1 : 0;
      }
    return n;
  }
}

TEST_CASE("a brick layer is cols x rows unit squares, numbered for rulers", "[ui][gridcells]") {
  gridType_c gt(gridType_c::GT_BRICKS);
  auto v = bttest::makeVoxel(gt, 4, 3, 5);
  GridLayout l = gridLayout(*v, Plane::XZ, 2);
  CHECK(l.squares);
  CHECK(l.cols == 4);
  CHECK(l.rows == 5);
  CHECK(l.width == 4);
  CHECK(l.height == 5);
  REQUIRE(l.cells.size() == 20);
  const int i = cellAt(l, { 1.5, 3.5 });
  REQUIRE(i >= 0);
  CHECK(l.cells[size_t(i)].voxel == Cell3{ 1, 2, 3 });
  CHECK(cellAt(l, { -0.5, 1 }) == -1);
  CHECK(cellAt(l, { 2, 5.5 }) == -1);
}

TEST_CASE("every voxel type's layer: one cell per voxel, counter-clockwise, inside the layout, hit at its centre", "[ui][gridcells]") {
  for (gridType_c::gridType t : bttest::ALL_GRIDS) {
    INFO(bttest::gridName(t));
    gridType_c gt(t);
    const unsigned s = unsigned(sizeStep(t));
    auto v = bttest::makeVoxel(gt, 2 * s + (s > 1 ? 0 : 3), 2 * s + (s > 1 ? 0 : 2), 2 * s);
    for (int layer = 0; layer < int(v->getZ()); layer++) {
      INFO("layer " << layer);
      GridLayout l = gridLayout(*v, Plane::XY, layer);
      CHECK(int(l.cells.size()) == validInLayer(*v, Plane::XY, layer));
      CHECK_FALSE(l.cells.empty());
      for (size_t i = 0; i < l.cells.size(); i++) {
        const GridCell & c = l.cells[i];
        CHECK(area(c.outline) > 0);
        for (const Pt & p : c.outline) {
          CHECK(p.x >= -1e-9);
          CHECK(p.y >= -1e-9);
          CHECK(p.x <= l.width + 1e-9);
          CHECK(p.y <= l.height + 1e-9);
        }
        CHECK(cellAt(l, centroid(c)) == int(i));
      }
    }
  }
}

TEST_CASE("cellAt finds what a scan of every cell would, through its buckets", "[ui][gridcells]") {
  // PR 181 review: cellAt looks in the point's unit square only; the cell
  // it finds is the first, in order, that holds the point -- edges and
  // corners included, where neighbours meet -- and none outside every cell
  auto scan = [](const GridLayout & l, Pt p) {
    for (size_t i = 0; i < l.cells.size(); i++) {
      const GridCell & c = l.cells[i];
      if (c.round) {
        const double dx = p.x - c.centre.x, dy = p.y - c.centre.y;
        if (dx * dx + dy * dy <= c.radius * c.radius)
          return int(i);
        continue;
      }
      bool in = true;
      for (size_t k = 0; k < c.outline.size() && in; k++) {
        const Pt & a = c.outline[k];
        const Pt & b = c.outline[(k + 1) % c.outline.size()];
        in = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x) >= -1e-9;
      }
      if (in)
        return int(i);
    }
    return -1;
  };
  for (gridType_c::gridType t : bttest::ALL_GRIDS) {
    INFO(bttest::gridName(t));
    gridType_c gt(t);
    const unsigned s = unsigned(sizeStep(t));
    auto v = bttest::makeVoxel(gt, 3 * s + (s > 1 ? 0 : 4), 2 * s + (s > 1 ? 0 : 3), 2 * s);
    for (int layer = 0; layer < int(v->getZ()); layer++) {
      const GridLayout l = gridLayout(*v, Plane::XY, layer);
      REQUIRE_FALSE(l.cells.empty());
      int hits = 0, checked = 0;
      // a fine sweep, every edge and corner of the unit squares, a margin outside
      for (double y = -0.5; y <= l.height + 0.5; y += 1.0 / 24)
        for (double x = -0.5; x <= l.width + 0.5; x += 1.0 / 24) {
          const Pt p{ x, y };
          const int want = scan(l, p);
          if (cellAt(l, p) != want) {
            INFO("layer " << layer << " at " << x << ", " << y);
            CHECK(cellAt(l, p) == want);
          }
          hits += want >= 0;
          checked++;
        }
      CHECK(hits > 0);
      CHECK(checked > 0);
      // every corner of every cell: on edges they share
      for (const GridCell & c : l.cells)
        for (const Pt & q : c.outline)
          CHECK(cellAt(l, q) == scan(l, q));
    }
  }
  CHECK(cellAt(GridLayout{}, { 0.5, 0.5 }) == -1);
}

TEST_CASE("prism triangles alternate: base down where x+y is even", "[ui][gridcells]") {
  gridType_c gt(gridType_c::GT_TRIANGULAR_PRISM);
  auto v = bttest::makeVoxel(gt, 3, 2, 1);
  GridLayout l = gridLayout(*v, Plane::XY, 0);
  CHECK_THAT(l.width, WithinAbs(2.0, 1e-9));
  REQUIRE(l.cells.size() == 6);
  // (0,0): base on y=0, apex at the top; (1,0): apex at the bottom
  CHECK(cellAt(l, { 0.5, 0.1 }) == 0);
  CHECK(cellAt(l, { 1.0, 0.75 }) == 1);
}

TEST_CASE("sphere layers hold the voxels whose x+y+z is even, as touching circles", "[ui][gridcells]") {
  gridType_c gt(gridType_c::GT_SPHERES);
  auto v = bttest::makeVoxel(gt, 3, 3, 2);
  GridLayout l0 = gridLayout(*v, Plane::XY, 0);
  GridLayout l1 = gridLayout(*v, Plane::XY, 1);
  CHECK(l0.cells.size() == 5);
  CHECK(l1.cells.size() == 4);
  for (const GridCell & c : l0.cells)
    CHECK(c.round);
}

TEST_CASE("arrow keys step to the next cell in their direction, for every voxel type", "[ui][gridcells]") {
  // the grid by keyboard (WCAG 2.1.1): from a cell, each arrow reaches a
  // neighbour or stops at the edge; every cell is reachable
  auto at = [](const GridLayout & l, int col, int row) {
    for (size_t i = 0; i < l.cells.size(); i++)
      if (l.cells[i].col == col && l.cells[i].row == row)
        return int(i);
    return -1;
  };
  gridType_c bricks(gridType_c::GT_BRICKS);
  auto v = bttest::makeVoxel(bricks, 3, 3, 1);
  const GridLayout l = gridLayout(*v, Plane::XY, 0);
  const int mid = at(l, 1, 1);
  CHECK(stepCell(l, mid, 1, 0) == at(l, 2, 1));
  CHECK(stepCell(l, mid, -1, 0) == at(l, 0, 1));
  CHECK(stepCell(l, mid, 0, 1) == at(l, 1, 2));          // up: rows count upward
  CHECK(stepCell(l, mid, 0, -1) == at(l, 1, 0));
  CHECK(stepCell(l, at(l, 2, 1), 1, 0) == -1);           // the edge
  CHECK(nearestCell(l, { 1.5, 1.5 }) == mid);

  for (gridType_c::gridType t : bttest::ALL_GRIDS) {
    INFO(bttest::gridName(t));
    gridType_c gt(t);
    auto w = bttest::makeVoxel(gt, 6, 6, 6);
    const GridLayout g = gridLayout(*w, Plane::XY, 2);
    REQUIRE(!g.cells.empty());
    // from the first cell, the four arrows reach every cell
    std::set<int> seen{ 0 };
    std::vector<int> todo{ 0 };
    while (!todo.empty()) {
      const int c = todo.back();
      todo.pop_back();
      for (auto [dx, dy] : { std::pair{ 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } }) {
        const int n = stepCell(g, c, dx, dy);
        if (n >= 0 && seen.insert(n).second)
          todo.push_back(n);
      }
    }
    CHECK(seen.size() == g.cells.size());
  }
}

TEST_CASE("the prism's other planes, and layers out of range, have no cells", "[ui][gridcells]") {
  for (gridType_c::gridType t : bttest::ALL_GRIDS) {
    INFO(bttest::gridName(t));
    gridType_c gt(t);
    auto v = bttest::makeVoxel(gt, 5, 5, 5);
    CHECK(gridLayout(*v, Plane::XY, -1).cells.empty());
    CHECK(gridLayout(*v, Plane::XY, 5).cells.empty());
    CHECK(gridLayout(*v, Plane::XZ, 5).cells.empty());
    CHECK(gridLayout(*v, Plane::XZ, 0).cells.empty() == (t == gridType_c::GT_TRIANGULAR_PRISM));
    CHECK(gridLayout(*v, Plane::YZ, 0).cells.empty() == (t == gridType_c::GT_TRIANGULAR_PRISM));
  }
}

TEST_CASE("an XZ or YZ layer of a symmetric grid is the XY layer with the axes swapped", "[ui][gridcells]") {
  // spheres, rhombic and tetra-octa: the same drawing, the cells' voxels
  // with the plane's axes; each cell hit at its own centre
  for (gridType_c::gridType t : { gridType_c::GT_SPHERES, gridType_c::GT_RHOMBIC, gridType_c::GT_TETRA_OCTA }) {
    INFO(bttest::gridName(t));
    gridType_c gt(t);
    auto v = bttest::makeVoxel(gt, 10, 10, 10);
    for (int layer : { 0, 1, 2, 3, 7 }) {
      INFO("layer " << layer);
      const GridLayout xy = gridLayout(*v, Plane::XY, layer);
      for (Plane p : { Plane::XZ, Plane::YZ }) {
        const GridLayout l = gridLayout(*v, p, layer);
        REQUIRE(l.cells.size() == xy.cells.size());
        CHECK(l.width == xy.width);
        CHECK(l.height == xy.height);
        for (size_t i = 0; i < l.cells.size(); i++) {
          const GridCell & a = l.cells[i];
          const GridCell & b = xy.cells[i];
          CHECK(a.col == b.col);
          CHECK(a.row == b.row);
          CHECK(a.voxel == voxelAt(p, a.col, a.row, layer));
          CHECK(v->validCoordinate(a.voxel.x, a.voxel.y, a.voxel.z));
          REQUIRE(a.outline.size() == b.outline.size());
          Pt m{ 0, 0 };
          for (const Pt & q : a.outline) {
            m.x += q.x / double(a.outline.size());
            m.y += q.y / double(a.outline.size());
          }
          CHECK(cellAt(l, m) == int(i));
        }
      }
    }
  }
}
