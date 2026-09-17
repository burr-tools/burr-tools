#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"

#include "lib/gridtype.h"
#include "lib/symmetries.h"
#include "lib/voxel.h"

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
