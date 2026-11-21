/* The shape editor's tool operations on the described grids (BurrTools NG's
 * grids as data): flips, turns and nudges from the grid's own
 * transformations and lattice (latticeGridInfo_c::flip, turn, shiftVector),
 * scaling where it is exact, the grid scale, and moving a shape to the
 * origin or the middle of its box (voxel_lattice_c). They give the FLTK
 * GUI's tool tab for these grids what the native prism grid's had in 0.8.
 */
#include <catch2/catch_test_macros.hpp>

#include "lib/assembly.h"
#include "lib/gridtype.h"
#include "lib/latticegridinfo.h"
#include "lib/voxel.h"
#include "lib/voxel_lattice.h"
#include "test_helpers.h"

#include <cmath>
#include <memory>
#include <random>
#include <vector>

namespace {

const gridType_c::gridType DESCRIBED[] = {
  gridType_c::GT_HEXAGONAL_PRISM, gridType_c::GT_CAIRO_PRISM, gridType_c::GT_RHOMBIC_TETRAHEDRA,
  gridType_c::GT_OCTAHEDRAL_TETRAHEDRA, gridType_c::GT_DESCRIBED_BRICKS, gridType_c::GT_DESCRIBED_TRIANGULAR_PRISM,
};

std::unique_ptr<voxel_c> randomShape(const gridType_c & gt, std::mt19937 & rng, int size, int count) {
  std::unique_ptr<voxel_c> v(gt.getVoxel(size, size, size, voxel_c::VX_EMPTY));
  std::uniform_int_distribution<int> d(0, size - 1);
  for (int i = 0, placed = 0; i < 2000 && placed < count; i++) {
    const int x = d(rng), y = d(rng), z = d(rng);
    if (v->validCoordinate(x, y, z) && v->isEmpty(x, y, z)) {
      v->setState(x, y, z, voxel_c::VX_FILLED);
      placed++;
    }
  }
  v->minimizePiece();
  v->initHotspot();
  return v;
}

// the volume of the cell at (x, y, z), from its faces (divergence theorem)
double cellVolume(const latticeGridInfo_c & inf, int x, int y, int z) {
  double vol = 0;
  std::vector<float> c;
  for (unsigned int f = 0; f < inf.faces(x, y, z); f++) {
    c.clear();
    inf.faceCorners(x, y, z, f, 0, 0, c);
    for (size_t i = 3; i + 5 < c.size(); i += 3) {
      const double a[3] = {c[0], c[1], c[2]}, b[3] = {c[i], c[i+1], c[i+2]}, d[3] = {c[i+3], c[i+4], c[i+5]};
      vol += a[0]*(b[1]*d[2]-b[2]*d[1]) - a[1]*(b[0]*d[2]-b[2]*d[0]) + a[2]*(b[0]*d[1]-b[1]*d[0]);
    }
  }
  return std::fabs(vol) / 6;
}

double shapeVolume(const voxel_c & v) {
  const latticeGridInfo_c & inf = latticeGridInfo_c::get(v.getGridType()->getType());
  double vol = 0;
  for (unsigned int z = 0; z < v.getZ(); z++)
    for (unsigned int y = 0; y < v.getY(); y++)
      for (unsigned int x = 0; x < v.getX(); x++)
        if (v.getState(x, y, z) != voxel_c::VX_EMPTY) vol += cellVolume(inf, x, y, z);
  return vol;
}

bool physicalIs(const latticeGridInfo_c & inf, unsigned char t, const double want[9]) {
  if (t == TND) return false;
  double m[9];
  inf.physicalMatrix(t, m);
  for (int k = 0; k < 9; k++)
    if (std::fabs(m[k] - want[k]) > 1e-6) return false;
  return true;
}

}

TEST_CASE("lattice tools: every described grid has the flips; the turns are the grid's smallest", "[lattice][tools]") {
  for (gridType_c::gridType t : DESCRIBED) {
    INFO(bttest::gridName(t));
    const latticeGridInfo_c & inf = latticeGridInfo_c::get(t);
    for (int a = 0; a < 3; a++) {
      double flip[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
      flip[4 * a] = -1;
      CHECK(physicalIs(inf, inf.flip(a), flip));
    }
    const bool prism = t == gridType_c::GT_HEXAGONAL_PRISM || t == gridType_c::GT_CAIRO_PRISM ||
                       t == gridType_c::GT_DESCRIBED_TRIANGULAR_PRISM;
    const double zTurn = (t == gridType_c::GT_HEXAGONAL_PRISM || t == gridType_c::GT_DESCRIBED_TRIANGULAR_PRISM) ? 60 : 90;
    for (int a = 0; a < 3; a++)
      for (bool acw : {true, false}) {
        double deg = 0;
        const unsigned char tr = inf.turn(a, acw, &deg);
        REQUIRE(tr != TND);
        CHECK(std::fabs(deg - (a == 2 ? zTurn : (prism ? 180 : 90))) < 1e-4);  // the matrices are floats
        // a turn about the axis, by that angle, in that sense
        const double r = deg * M_PI / 180 * (acw ? 1 : -1);
        double want[9] = {0};
        const int i = (a + 1) % 3, j = (a + 2) % 3;
        want[4 * a] = 1;
        want[3 * i + i] = std::cos(r); want[3 * i + j] = -std::sin(r);
        want[3 * j + i] = std::sin(r); want[3 * j + j] = std::cos(r);
        CHECK(physicalIs(inf, tr, want));
      }
  }
}

TEST_CASE("lattice tools: on the described cubes the turns and flips are the cube tab's", "[lattice][tools]") {
  // ToolTab_0: "clockwise" (tasks 6, 9, 10) is transformation 1, 12, 16;
  // the flips (12, 13, 14) are 24, 34, 32. ToolTab_lattice maps clockwise
  // to turn(axis, true)
  gridType_c cube(gridType_c::GT_BRICKS);
  std::unique_ptr<voxel_c> v(cube.getVoxel(2, 2, 2, voxel_c::VX_EMPTY));
  const latticeGridInfo_c & inf = latticeGridInfo_c::get(gridType_c::GT_DESCRIBED_BRICKS);
  const unsigned int cw[3] = {1, 12, 16}, fl[3] = {24, 34, 32};
  for (int a = 0; a < 3; a++) {
    double m[9];
    v->getTransformMatrix(cw[a], m);
    CHECK(physicalIs(inf, inf.turn(a, true), m));
    v->getTransformMatrix(fl[a], m);
    CHECK(physicalIs(inf, inf.flip(a), m));
  }
}

TEST_CASE("lattice tools: the nudges are lattice vectors along their axis, and up then down is no change", "[lattice][tools]") {
  std::mt19937 rng(7);
  for (gridType_c::gridType t : DESCRIBED) {
    INFO(bttest::gridName(t));
    const latticeGridInfo_c & inf = latticeGridInfo_c::get(t);
    gridType_c gt(t);
    for (int a = 0; a < 3; a++) {
      int x, y, z;
      REQUIRE(inf.shiftVector(a, x, y, z));
      CHECK(inf.isLatticeVector(x, y, z));
      float p[3] = {(float)x, (float)y, (float)z};
      inf.toPhysical(p[0], p[1], p[2]);
      CHECK(p[a] > 0);
      // the prisms' axis and the cube-like grids' axes are exact
      if (a == 2 || t == gridType_c::GT_DESCRIBED_BRICKS || t == gridType_c::GT_RHOMBIC_TETRAHEDRA ||
          t == gridType_c::GT_OCTAHEDRAL_TETRAHEDRA) {
        CHECK(std::fabs(p[(a + 1) % 3]) < 1e-4);
        CHECK(std::fabs(p[(a + 2) % 3]) < 1e-4);
      }

      std::unique_ptr<voxel_c> v = randomShape(gt, rng, 5, 6);
      std::unique_ptr<voxel_c> w(gt.getVoxel(v.get()));
      // room on every side, made by a lattice move
      w->resize(v->getX() + 2 * std::abs(x) + 6, v->getY() + 2 * std::abs(y) + 6, v->getZ() + 2 * std::abs(z) + 6, 0);
      dynamic_cast<voxel_lattice_c &>(*w).centreInBox();
      std::unique_ptr<voxel_c> u(gt.getVoxel(w.get()));
      w->translate(x, y, z, 0);
      CHECK(w->countState(voxel_c::VX_FILLED) == u->countState(voxel_c::VX_FILLED));
      w->translate(-x, -y, -z, 0);
      CHECK(w->identicalInBB(u.get()));
    }
  }
}

TEST_CASE("lattice tools: scaling is exact on cubes, triangles, rhombic and tetra-octa, refused on hexagons and Cairo",
          "[lattice][tools]") {
  std::mt19937 rng(11);
  for (gridType_c::gridType t : DESCRIBED) {
    INFO(bttest::gridName(t));
    gridType_c gt(t);
    const bool exact = t != gridType_c::GT_HEXAGONAL_PRISM && t != gridType_c::GT_CAIRO_PRISM;
    for (unsigned int k : {2u, 3u}) {
      INFO("k = " << k);
      std::unique_ptr<voxel_c> v = randomShape(gt, rng, 4, 5);
      auto * lv = dynamic_cast<voxel_lattice_c *>(v.get());
      REQUIRE(lv);
      CHECK(lv->canScale(k) == exact);
      const double before = shapeVolume(*v);
      const unsigned int cells = v->countState(voxel_c::VX_FILLED);
      lv->scale(k);
      if (exact) {
        // the volume grows by k^3; on cubes and triangles every cell is k^3 cells
        CHECK(std::fabs(shapeVolume(*v) - k * k * k * before) < 1e-3 * before);
        if (t == gridType_c::GT_DESCRIBED_BRICKS || t == gridType_c::GT_DESCRIBED_TRIANGULAR_PRISM)
          CHECK(v->countState(voxel_c::VX_FILLED) == k * k * k * cells);
      } else {
        CHECK(v->countState(voxel_c::VX_FILLED) == cells);  // unchanged
      }
    }
  }
}

TEST_CASE("lattice tools: the grid scale of cubes keeps the edges and the open faces", "[lattice][tools]") {
  gridType_c gt(gridType_c::GT_DESCRIBED_BRICKS);
  // one cube: every face open, so the whole shell of the 5x5x5 block
  std::unique_ptr<voxel_c> one(gt.getVoxel(1, 1, 1, voxel_c::VX_FILLED));
  dynamic_cast<voxel_lattice_c &>(*one).scale(5, true);
  CHECK(one->countState(voxel_c::VX_FILLED) == 125 - 27);
  // two cubes side by side: the shared face is closed, its 3x3 middle goes
  std::unique_ptr<voxel_c> two(gt.getVoxel(2, 1, 1, voxel_c::VX_FILLED));
  dynamic_cast<voxel_lattice_c &>(*two).scale(5, true);
  CHECK(two->countState(voxel_c::VX_FILLED) == 2 * (125 - 27 - 9));
}

TEST_CASE("lattice tools: to the origin and to the middle move by a lattice vector and stay in the box",
          "[lattice][tools]") {
  std::mt19937 rng(5);
  for (gridType_c::gridType t : DESCRIBED) {
    INFO(bttest::gridName(t));
    gridType_c gt(t);
    const latticeGridInfo_c & inf = latticeGridInfo_c::get(t);
    for (int i = 0; i < 5; i++) {
      std::unique_ptr<voxel_c> v = randomShape(gt, rng, 4, 4);
      v->resize(v->getX() + 9, v->getY() + 9, v->getZ() + 9, 0);
      std::unique_ptr<voxel_c> orig(gt.getVoxel(v.get()));
      const unsigned int cells = v->countState(voxel_c::VX_FILLED);
      auto & lv = dynamic_cast<voxel_lattice_c &>(*v);

      lv.centreInBox();
      CHECK(v->countState(voxel_c::VX_FILLED) == cells);
      CHECK(v->boundX1() > 0);

      lv.moveToOrigin();
      CHECK(v->countState(voxel_c::VX_FILLED) == cells);
      int vx, vy, vz;
      inf.alignDown((int)v->boundX1(), (int)v->boundY1(), (int)v->boundZ1(), vx, vy, vz);
      CHECK((vx == 0 && vy == 0 && vz == 0));

      lv.centreInBox();
      CHECK(v->countState(voxel_c::VX_FILLED) == cells);
      // centred: the free room on both sides differs by less than a lattice step
      CHECK(std::abs((int)v->boundX1() - ((int)v->getX() - 1 - (int)v->boundX2())) < 2 * 6 + 1);

      std::unique_ptr<voxel_c> a(gt.getVoxel(v.get())), b(gt.getVoxel(orig.get()));
      a->minimizePiece();
      b->minimizePiece();
      CHECK(a->identicalInBB(b.get()));
    }
  }
}

