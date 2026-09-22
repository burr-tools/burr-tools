#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"

#include "lib/gridtype.h"
#include "lib/symmetries.h"
#include "lib/voxel.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <set>
#include <tuple>
#include <vector>

using namespace bttest;

/* Properties every grid's voxel_c implementation must satisfy, asserted
   over shapes that are LEGAL on the grid being tested.

   That last part is the whole reason this file exists. test_voxel.cpp's
   cases build fixtures from ASCII art, which addresses coordinates directly
   and therefore only describes a real shape on the two dense grids --
   GT_SPHERES, GT_RHOMBIC and GT_TETRA_OCTA accept a sparse subset of
   coordinates, and a cell written outside it is not a differently-shaped
   piece, it is not a piece. Geometry operations (transform, hotspots,
   bounding boxes) consult validCoordinate and abort or misbehave on such a
   fixture, so those cases stay single-grid.

   Here the fixtures are built the other way round: ask the grid which
   coordinates it accepts, and fill those. That gives every grid a shape it
   can actually reason about, and makes the cross-grid assertions below mean
   something on all five rather than on two.

   The assertions are properties rather than values, as the design doc asks:
   a transformation composed with its inverse is the identity, transforming
   preserves the cell count, the neighbour relation is symmetric. A value
   that happened to match on all five grids would be a coincidence; these
   are obligations. */

/* validCoordinates(), legalShape() and the box size live in
   test_helpers.h: the same "ask the grid which cells it accepts" rule that
   this file is built on is needed by the connectivity and scaling cases
   too, and a second copy would be a second thing to get wrong. */
namespace {

constexpr int BOX = LEGAL_SHAPE_BOX;

} // namespace

TEST_CASE("grid: every grid offers valid coordinates to build a shape from", "[voxel][grid]") {
  /* The premise the rest of this file rests on. Without it a grid offering
     no valid coordinates would make every case below pass vacuously over an
     empty shape. */
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);

    std::unique_ptr<voxel_c> v = makeVoxel(gt, BOX, BOX, BOX);
    std::vector<std::tuple<int, int, int>> coords = validCoordinates(*v, BOX);

    REQUIRE(coords.size() >= 4);

    /* and each reported coordinate really is inside the box it was searched
       in -- a validCoordinate that ignored its arguments would pass the
       count check above. Scanned to one assertion rather than three per
       coordinate. */
    bool allInBox = true;
    for (auto [x, y, z] : coords)
      if (x < 0 || x >= BOX || y < 0 || y >= BOX || z < 0 || z >= BOX)
        allInBox = false;

    REQUIRE(allInBox);
  }
}

TEST_CASE("grid: transforming a legal shape preserves how many cells it has", "[voxel][grid][transform]") {
  /* A transformation is a rigid motion of the grid: it may move cells and
     it may fail outright on grids where a given transformation is not
     realisable, but it may never create or destroy one. This is the
     cheapest property that catches a rotation table mapping two source
     cells onto the same destination. */
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);

    std::unique_ptr<voxel_c> shape = legalShape(gt, BOX, 4);
    REQUIRE(shape != nullptr);

    const unsigned int cells = shape->countState(voxel_c::VX_FILLED);
    REQUIRE(cells == 4);

    const unsigned int n = gt.getSymmetries()->getNumTransformationsMirror();

    unsigned int realised = 0;
    for (unsigned int tr = 0; tr < n; tr++) {
      std::unique_ptr<voxel_c> copy = copyVoxel(gt, *shape);

      INFO("transformation " << tr);
      if (!copy->transform(tr)) continue;   // not realisable on this grid

      realised++;
      REQUIRE(copy->countState(voxel_c::VX_FILLED) == cells);
    }

    /* at least the identity must have been realisable, or the loop body
       above never ran and this case asserted nothing */
    REQUIRE(realised > 0);
  }
}

TEST_CASE("grid: a transformation composed with its inverse is the identity on every grid",
          "[voxel][grid][transform]") {
  /* The property the design doc names as the model for this package:
     prefer an obligation that holds everywhere to a value that happens to
     coincide. For every transformation the grid realises there must be one
     that undoes it, and applying both must return the original shape cell
     for cell.

     Found by search rather than by table, because the inverse's NUMBER is
     grid-specific -- which is exactly the sort of thing that must not be
     hardcoded into a cross-grid case. */
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);

    std::unique_ptr<voxel_c> shape = legalShape(gt, BOX, 4);
    REQUIRE(shape != nullptr);

    const symmetries_c * sym = gt.getSymmetries();
    const unsigned int n = sym->getNumTransformations();

    unsigned int checked = 0;

    for (unsigned int tr = 0; tr < n; tr++) {
      std::unique_ptr<voxel_c> moved = copyVoxel(gt, *shape);
      if (!moved->transform(tr)) continue;

      /* look for a transformation that brings it back */
      bool foundInverse = false;
      for (unsigned int inv = 0; inv < n && !foundInverse; inv++) {
        std::unique_ptr<voxel_c> back = copyVoxel(gt, *moved);
        if (!back->transform(inv)) continue;
        if (back->identicalInBB(shape.get(), false)) foundInverse = true;
      }

      INFO("transformation " << tr << " has no inverse among the " << n
           << " non-mirror transformations");
      REQUIRE(foundInverse);
      checked++;
    }

    REQUIRE(checked > 0);
  }
}

TEST_CASE("grid: the neighbour relation is symmetric on every grid", "[voxel][grid][neighbour]") {
  /* If b is a face neighbour of a then a must be one of b. An asymmetric
     neighbour table makes connectivity depend on which cell the search
     happens to start from, which is the kind of defect that shows up as an
     intermittently unsolvable puzzle rather than as a crash.

     Checked over the grid's own valid coordinates, and only for neighbours
     that are themselves valid and inside the box -- a neighbour outside the
     space has no cell to look back from.

     Scanned rather than asserted per pair: there are a few thousand pairs
     per grid and one REQUIRE each would inflate the suite's assertion count
     by four figures without covering anything further. The failure still
     names the offending pair, through the INFO. */
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);

    std::unique_ptr<voxel_c> v = makeVoxel(gt, BOX, BOX, BOX);

    auto inBox = [](int x, int y, int z) {
      return x >= 0 && x < BOX && y >= 0 && y < BOX && z >= 0 && z < BOX;
    };

    unsigned int pairs = 0;
    bool asymmetric = false;
    int fx = 0, fy = 0, fz = 0, gx = 0, gy = 0, gz = 0;

    for (auto [x, y, z] : validCoordinates(*v, BOX)) {
      for (unsigned int i = 0; !asymmetric; i++) {
        int nx, ny, nz;
        if (!v->getNeighbor(i, 0, x, y, z, &nx, &ny, &nz)) break;
        if (!inBox(nx, ny, nz)) continue;
        if (!v->validCoordinate(nx, ny, nz)) continue;

        bool mutual = false;
        for (unsigned int j = 0; !mutual; j++) {
          int bx, by, bz;
          if (!v->getNeighbor(j, 0, nx, ny, nz, &bx, &by, &bz)) break;
          if (bx == x && by == y && bz == z) mutual = true;
        }

        pairs++;
        if (!mutual) {
          asymmetric = true;
          fx = x; fy = y; fz = z; gx = nx; gy = ny; gz = nz;
        }
      }
      if (asymmetric) break;
    }

    /* the grid must actually have produced some neighbour pairs, or the
       scan proved nothing */
    REQUIRE(pairs > 0);

    INFO("(" << fx << "," << fy << "," << fz << ") lists ("
         << gx << "," << gy << "," << gz << ") as a face neighbour but not the reverse");
    REQUIRE_FALSE(asymmetric);
  }
}

TEST_CASE("grid: a cell is never its own neighbour on any grid", "[voxel][grid][neighbour]") {
  /* Scanned, for the same reason as the case above. */
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);

    std::unique_ptr<voxel_c> v = makeVoxel(gt, BOX, BOX, BOX);

    unsigned int seen = 0;
    bool selfNeighbour = false;
    int fx = 0, fy = 0, fz = 0;
    unsigned int ftyp = 0, fidx = 0;

    for (auto [x, y, z] : validCoordinates(*v, BOX)) {
      for (unsigned int typ = 0; typ < 3 && !selfNeighbour; typ++)
        for (unsigned int i = 0; ; i++) {
          int nx, ny, nz;
          if (!v->getNeighbor(i, typ, x, y, z, &nx, &ny, &nz)) break;
          seen++;
          if (nx == x && ny == y && nz == z) {
            selfNeighbour = true;
            fx = x; fy = y; fz = z; ftyp = typ; fidx = i;
            break;
          }
        }
      if (selfNeighbour) break;
    }

    REQUIRE(seen > 0);

    INFO("cell (" << fx << "," << fy << "," << fz << ") lists itself at type "
         << ftyp << " index " << fidx);
    REQUIRE_FALSE(selfNeighbour);
  }
}

TEST_CASE("grid: normalizeTransformation picks a transformation that produces the same shape",
          "[voxel][grid][transform]") {
  /* voxel.h documents normalizeTransformation as returning the smallest
     transformation number giving a shape identical to the one asked for.
     Asserted as that identity rather than as a number, so it holds on every
     grid without a per-grid table -- and so a stub returning 0 fails. */
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);

    std::unique_ptr<voxel_c> shape = legalShape(gt, BOX, 4);
    REQUIRE(shape != nullptr);

    const unsigned int n = gt.getSymmetries()->getNumTransformationsMirror();

    unsigned int checked = 0;

    for (unsigned int tr = 0; tr < n; tr++) {
      const unsigned char norm = shape->normalizeTransformation(tr);
      REQUIRE(norm <= tr);

      std::unique_ptr<voxel_c> byTr = copyVoxel(gt, *shape);
      if (!byTr->transform(tr)) continue;

      std::unique_ptr<voxel_c> byNorm = copyVoxel(gt, *shape);
      INFO("transformation " << tr << " normalised to " << (int)norm);
      REQUIRE(byNorm->transform(norm));

      REQUIRE(byTr->identicalInBB(byNorm.get(), false));
      checked++;
    }

    REQUIRE(checked > 0);
  }
}

TEST_CASE("grid: a shape's self-symmetries are exactly the transformations that fix it",
          "[voxel][grid][transform]") {
  /* selfSymmetries() is a packed set; symmetrieContainsTransformation reads
     it back. The two must agree with what transform() actually does, which
     is the only definition that is not circular -- and checking it over a
     legal shape on each grid exercises every grid's own symmetry
     calculation rather than only the cube grid's. */
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);

    std::unique_ptr<voxel_c> shape = legalShape(gt, BOX, 4);
    REQUIRE(shape != nullptr);

    const symmetries_c * sym = gt.getSymmetries();
    const symmetries_t self = shape->selfSymmetries();

    for (unsigned int tr = 0; tr < sym->getNumTransformations(); tr++) {
      std::unique_ptr<voxel_c> copy = copyVoxel(gt, *shape);

      const bool claimed = sym->symmetrieContainsTransformation(self, tr);
      const bool actual  = copy->transform(tr) && copy->identicalInBB(shape.get(), false);

      INFO("transformation " << tr << ": claimed " << claimed << ", actual " << actual);
      REQUIRE(claimed == actual);
    }
  }
}

TEST_CASE("grid: transformPoint on the coordinate basis does not recover a linear map on every grid",
          "[voxel][grid][transform]") {
  /* Not a property the grids owe anyone -- a pin on one they deliberately do
     NOT provide, because a caller that assumes otherwise gets a crash on one
     grid and silent nonsense on another.

     transformPoint() is documented as transforming "around the origin", which
     reads like an invitation to recover a transformation's 3x3 linear part by
     feeding it (1,0,0), (0,1,0), (0,0,1). That works on the three cube-derived
     grids and fails on the other two:

       - GT_SPHERES asserts its input has an even coordinate sum (voxel_2.cpp),
         which no single basis vector satisfies, so the probe throws.
       - GT_TRIANGULAR_PRISM's mapping is affine, not linear: a parity-dependent
         offset is folded in (voxel_1.cpp), so the images of the basis vectors
         are not the columns of anything orthogonal. The tell is the
         determinant, which comes out 2 rather than +-1.

     voxelFrame_c's transform-preview hint (via computeTransformGeometry() in
     src/gui/tooltabs.cpp) probes exactly this way, so it guards with both a
     try/catch and a determinant check and draws no hint rather than a wrong
     one. If a future grid change made the probe safe and orthogonal
     everywhere, this case would fail and that guard could be simplified. */

  auto linearPartDeterminant = [](const voxel_c & v, unsigned int trans, int * det) -> bool {
    int m[3][3];
    const int basis[3][3] = { {1,0,0}, {0,1,0}, {0,0,1} };

    try {
      for (int c = 0; c < 3; c++) {
        int x = basis[c][0], y = basis[c][1], z = basis[c][2];
        v.transformPoint(&x, &y, &z, trans);
        m[0][c] = x; m[1][c] = y; m[2][c] = z;
      }
    } catch (const std::exception &) {
      return false;   // grid rejected the probe outright
    }

    *det = m[0][0]*(m[1][1]*m[2][2] - m[1][2]*m[2][1])
         - m[0][1]*(m[1][0]*m[2][2] - m[1][2]*m[2][0])
         + m[0][2]*(m[1][0]*m[2][1] - m[1][1]*m[2][0]);
    return true;
  };

  SECTION("the sphere grid rejects the basis probe instead of answering") {
    gridType_c gt(gridType_c::GT_SPHERES);
    std::unique_ptr<voxel_c> v = makeVoxel(gt, BOX, BOX, BOX);

    int det = 0;
    REQUIRE_FALSE(linearPartDeterminant(*v, 9, &det));
  }

  SECTION("the triangular grid answers, but not with an orthogonal matrix") {
    gridType_c gt(gridType_c::GT_TRIANGULAR_PRISM);
    std::unique_ptr<voxel_c> v = makeVoxel(gt, BOX, BOX, BOX);

    /* transformation 1 is a clean 60 degree rotation about Z in the grid's own
       table, so a correctly recovered linear part would have determinant 1. */
    int det = 0;
    REQUIRE(linearPartDeterminant(*v, 1, &det));
    REQUIRE(det != 1);
    REQUIRE(det != -1);
  }

  SECTION("the cube-derived grids do give an orthogonal matrix") {
    const gridType_c::gridType cubeLike[] = {
      gridType_c::GT_BRICKS, gridType_c::GT_RHOMBIC, gridType_c::GT_TETRA_OCTA,
    };

    for (gridType_c::gridType t : cubeLike) {
      INFO("grid " << gridName(t));
      gridType_c gt(t);
      std::unique_ptr<voxel_c> v = makeVoxel(gt, BOX, BOX, BOX);

      int det = 0;
      REQUIRE(linearPartDeterminant(*v, 1, &det));
      REQUIRE(std::abs(det) == 1);
    }
  }
}

TEST_CASE("grid: getTransformMatrix returns an orthogonal matrix for every transformation, on every grid",
          "[voxel][grid][transform]") {
  /* The transform-preview hint in src/gui/tooltabs.cpp (computeTransformGeometry())
     needs the actual geometric rotation/mirror a transformation represents, to draw
     an axis-aligned arc or arrow. Probing transformPoint() with synthetic points
     (as an earlier version of that code did) breaks on two grids: GT_SPHERES
     asserts its input has an even coordinate sum, which no unit basis vector
     satisfies, and GT_TRIANGULAR_PRISM's transformPoint() is affine (see the note
     on voxel_c::transformPoint), so no probe recovers a linear map from it at all.

     getTransformMatrix() sidesteps both by reading each grid's own internal
     rotation-matrix table directly instead of reconstructing it. This case is the
     property that makes that safe to rely on: for every transformation on every
     grid, the returned 3x3 matrix is genuinely orthogonal (determinant +-1, unit
     columns, mutually perpendicular) - a real rotation or mirror, not numerical
     noise - which is exactly the property the sphere/triangular-grid probe could
     not establish. */
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);

    std::unique_ptr<voxel_c> v = makeVoxel(gt, BOX, BOX, BOX);
    const unsigned int n = gt.getSymmetries()->getNumTransformationsMirror();
    REQUIRE(n > 0);

    for (unsigned int trans = 0; trans < n; trans++) {
      INFO("transformation " << trans);

      double m[9];
      v->getTransformMatrix(trans, m);

      // columns are unit length and mutually orthogonal
      for (int c = 0; c < 3; c++) {
        double len2 = m[c]*m[c] + m[3+c]*m[3+c] + m[6+c]*m[6+c];
        REQUIRE(std::abs(len2 - 1.0) < 1e-6);
      }
      for (int c1 = 0; c1 < 3; c1++)
        for (int c2 = c1+1; c2 < 3; c2++) {
          double dot = m[c1]*m[c2] + m[3+c1]*m[3+c2] + m[6+c1]*m[6+c2];
          REQUIRE(std::abs(dot) < 1e-6);
        }

      double det = m[0]*(m[4]*m[8]-m[5]*m[7])
                 - m[1]*(m[3]*m[8]-m[5]*m[6])
                 + m[2]*(m[3]*m[7]-m[4]*m[6]);
      REQUIRE(std::abs(std::abs(det) - 1.0) < 1e-6);
    }
  }
}

TEST_CASE("grid: getTransformMatrix agrees with transformPoint on the sphere grid's axis-aligned transforms",
          "[voxel][grid][transform]") {
  /* GT_SPHERES' NUM_TRANSFORMATIONS_MIRROR is 240 - far more than the 48 a plain
     cubic lattice has - because it is the full icosahedral-like symmetry of a
     sphere packing, and most of those 240 rotations do NOT map the underlying
     integer (even-coordinate-sum) lattice back onto itself exactly. transformPoint()
     says as much in its own comment ("it is possible that the resulting coordinates
     are invalid ... we assume the error ... will cancel out later"): it rounds to
     the nearest integer regardless, so comparing it against the exact continuous
     matrix from getTransformMatrix() only makes sense for the subset of
     transformations that genuinely are lattice-exact - the axis-aligned 90/180
     degree rotations and mirrors, which is exactly what src/gui/tooltabs.cpp's
     ToolTab_2 (the sphere-grid Pieces-tab buttons) exposes. Checking those confirms
     getTransformMatrix() reads the *same* rotation the grid actually applies for
     everything the GUI can ask for, not merely *some* orthogonal matrix. */
  gridType_c gt(gridType_c::GT_SPHERES);
  std::unique_ptr<voxel_c> v = makeVoxel(gt, BOX, BOX, BOX);

  // exactly the voxel_c::transform() indices ToolTab_2::applyTask() passes through
  const unsigned int uiTransforms[] = { 9, 14, 5, 16, 2, 6, 120, 124, 141 };

  const int px[3] = { 2, 0, 0 };  // even coordinate sum, satisfies voxel_2_c's precondition

  for (unsigned int trans : uiTransforms) {
    INFO("transformation " << trans);

    int x = px[0], y = px[1], z = px[2];
    v->transformPoint(&x, &y, &z, trans);

    double m[9];
    v->getTransformMatrix(trans, m);
    double ex = m[0]*px[0] + m[1]*px[1] + m[2]*px[2];
    double ey = m[3]*px[0] + m[4]*px[1] + m[5]*px[2];
    double ez = m[6]*px[0] + m[7]*px[1] + m[8]*px[2];

    REQUIRE(std::abs(ex - x) < 1e-6);
    REQUIRE(std::abs(ey - y) < 1e-6);
    REQUIRE(std::abs(ez - z) < 1e-6);
  }
}

namespace {

/* Verbatim copy of nullSpaceOf3x3()/computeTransformGeometry() from
   src/gui/tooltabs.cpp. Kept as free functions (not per-test lambdas) so every
   case below that needs it - "does a hint show up at all" and "is its direction
   actually correct" are different properties, and conflating them into one test
   hides which one broke - shares a single copy that has to be kept in sync with
   the real code, rather than three that quietly drift apart. */

bool nullSpaceOf3x3(const double m[9], double lambda, double axis[3]) {
  double a[9];
  for (int i = 0; i < 9; i++)
    a[i] = m[i] + ((i % 3 == i / 3) ? lambda : 0.0);

  double cand[3][3] = {
    { a[1]*a[5]-a[2]*a[4], a[2]*a[3]-a[0]*a[5], a[0]*a[4]-a[1]*a[3] },
    { a[1]*a[8]-a[2]*a[7], a[2]*a[6]-a[0]*a[8], a[0]*a[7]-a[1]*a[6] },
    { a[4]*a[8]-a[5]*a[7], a[5]*a[6]-a[3]*a[8], a[3]*a[7]-a[4]*a[6] },
  };

  int best = -1;
  double bestLen = 1e-4;
  for (int i = 0; i < 3; i++) {
    double len = std::sqrt(cand[i][0]*cand[i][0] + cand[i][1]*cand[i][1] + cand[i][2]*cand[i][2]);
    if (len > bestLen) { bestLen = len; best = i; }
  }
  if (best < 0)
    return false;

  axis[0] = cand[best][0]/bestLen;
  axis[1] = cand[best][1]/bestLen;
  axis[2] = cand[best][2]/bestLen;
  return true;
}

// returns 0 (none), 1 (rotation, axis/angleDeg set) or 2 (mirror, axis set)
int computeTransformGeometry(const voxel_c & v, int transformIdx, double axis[3], double * angleDeg) {
  double m[9];
  v.getTransformMatrix((unsigned int)transformIdx, m);

  double det = m[0]*(m[4]*m[8] - m[5]*m[7])
             - m[1]*(m[3]*m[8] - m[5]*m[6])
             + m[2]*(m[3]*m[7] - m[4]*m[6]);

  if (det > 0) {
    double trace = m[0] + m[4] + m[8];
    double cosA = std::max(-1.0, std::min(1.0, (trace - 1.0)*0.5));
    double angle = std::acos(cosA);
    if (angle < 0.02)
      return 0;

    double w[3] = { m[7]-m[5], m[2]-m[6], m[3]-m[1] };
    double wlen = std::sqrt(w[0]*w[0] + w[1]*w[1] + w[2]*w[2]);
    if (wlen > 1e-4) {
      axis[0] = w[0]/wlen;
      axis[1] = w[1]/wlen;
      axis[2] = w[2]/wlen;
    } else if (!nullSpaceOf3x3(m, -1.0, axis)) {
      return 0;
    }

    *angleDeg = angle*180.0/3.1415927;
    return 1;
  }

  if (det < 0)
    return nullSpaceOf3x3(m, 1.0, axis) ? 2 : 0;

  return 0;
}

} // namespace

TEST_CASE("grid: the transform-preview hint resolves to a rotation or mirror for every button, on every grid",
          "[voxel][grid][transform]") {
  /* The property the user actually cares about: the GUI hint (an arc for a
     rotation, a double-headed arrow for a mirror) must show up for every
     rotate/flip button on every grid, not just the cubic ones, and not just the
     ones that happen to avoid a 180 degree rotation - which is why the axis
     extraction falls back to the null space of (M -/+ I) there - and not just
     the ones where transformPoint() happens to be reliable enough to probe
     (broken on GT_SPHERES and GT_TRIANGULAR_PRISM, see the two cases above,
     which is why this reads getTransformMatrix() directly instead). */
  struct TabButtons {
    const char * name;
    gridType_c::gridType grid;
    std::vector<int> transforms;
  };

  const TabButtons tabs[] = {
    { "ToolTab_0 (bricks)",       gridType_c::GT_BRICKS,           {3,1,12,4,20,16,24,34,32} },
    { "ToolTab_1 (triangular)",   gridType_c::GT_TRIANGULAR_PRISM, {9,6,1,5,12,15,18} },
    { "ToolTab_2 (spheres)",      gridType_c::GT_SPHERES,          {9,14,5,16,2,6,120,124,141} },
    { "ToolTab_3 (rhombic)",      gridType_c::GT_RHOMBIC,          {3,1,12,4,20,16,24,34,32} },
    { "ToolTab_4 (tetra-octa)",   gridType_c::GT_TETRA_OCTA,       {3,1,12,4,20,16,24,34,32} },
  };

  for (const TabButtons & tab : tabs) {
    INFO(tab.name);
    gridType_c gt(tab.grid);
    std::unique_ptr<voxel_c> v = makeVoxel(gt, BOX, BOX, BOX);

    for (int trans : tab.transforms) {
      INFO("transformation " << trans);
      double axis[3], angle = 0;
      REQUIRE(computeTransformGeometry(*v, trans, axis, &angle) != 0);
    }
  }
}

TEST_CASE("grid: the transform-preview hint points opposite ways for a CW/CCW button pair",
          "[voxel][grid][transform]") {
  /* Regression for a real bug: an earlier version of computeTransformGeometry()
     extracted the rotation axis from the null space of (M-I) unconditionally (to
     handle 180 degree rotations, where the antisymmetric part below vanishes).
     But a null-space vector's sign is arbitrary - nothing ties it to which way
     the rotation actually turns - so the "rotate 90 CW" and "rotate 90 CCW"
     buttons around the same axis got the same-looking arrow, which is exactly
     what was reported. The antisymmetric part of M does carry that sign
     correctly (its magnitude is 2*sin(angle), so it points whichever way makes
     that positive), which is why it has to be tried first and the null space
     used only as the 180-degree fallback.

     ToolTab_0's "rotate around X" / "rotate around X the other way" buttons
     (task 6 -> transform(1), task 7 -> transform(3), see tooltabs.cpp) are
     exactly such a pair - tabs_0/rotmatrix.inc transform 1 sends +Y to +Z and
     transform 3 sends +Y to -Z, i.e. +90 and -90 degrees about the same physical
     axis. A correct extraction reports the same angle for both with the axis
     flipped; the bug reported both axes pointing the same way. */
  gridType_c gt(gridType_c::GT_BRICKS);
  std::unique_ptr<voxel_c> v = makeVoxel(gt, BOX, BOX, BOX);

  const struct { int cw, ccw; } pairs[] = {
    { 1, 3 },    // rotate about X, +90 vs -90 (ToolTab_0 task 6 / 7)
    { 4, 12 },   // rotate about Y (task 8 / 9)
    { 16, 20 },  // rotate about Z (task 10 / 11)
  };

  for (const auto & p : pairs) {
    INFO("transforms " << p.cw << " / " << p.ccw);

    double axisCW[3], axisCCW[3], angleCW = 0, angleCCW = 0;
    REQUIRE(computeTransformGeometry(*v, p.cw,  axisCW,  &angleCW)  == 1);
    REQUIRE(computeTransformGeometry(*v, p.ccw, axisCCW, &angleCCW) == 1);

    REQUIRE(std::abs(angleCW - angleCCW) < 1e-6);

    // anti-parallel: dot product close to -1, not +1
    double dot = axisCW[0]*axisCCW[0] + axisCW[1]*axisCCW[1] + axisCW[2]*axisCCW[2];
    REQUIRE(dot < -0.99);
  }
}
