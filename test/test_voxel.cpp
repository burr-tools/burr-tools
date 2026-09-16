#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"

#include "lib/gridtype.h"
#include "lib/voxel.h"

#include <iterator>
#include <memory>
#include <vector>

using namespace bttest;

TEST_CASE("voxel: a new space has the requested size and is empty", "[voxel]") {
  /* ported from the old Boost voxel_0_test */
  gridType_c gt(gridType_c::GT_BRICKS);
  std::unique_ptr<voxel_c> v = makeVoxel(gt, 1, 2, 3);

  REQUIRE(v->getX() == 1);
  REQUIRE(v->getY() == 2);
  REQUIRE(v->getZ() == 3);
  REQUIRE(v->getXYZ() == 6);

  for (unsigned int i = 0; i < v->getXYZ(); i++)
    REQUIRE(v->getState(i) == voxel_c::VX_EMPTY);
}

TEST_CASE("voxel: every grid builds an empty space of the right size", "[voxel]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);
    std::unique_ptr<voxel_c> v = makeVoxel(gt, 2, 3, 4);

    REQUIRE(v->getX() == 2);
    REQUIRE(v->getY() == 3);
    REQUIRE(v->getZ() == 4);
    REQUIRE(v->getXYZ() == 24);
    REQUIRE(v->countState(voxel_c::VX_EMPTY) == 24);
    REQUIRE(v->countState(voxel_c::VX_FILLED) == 0);
    REQUIRE(v->getGridType()->getType() == t);
  }
}

TEST_CASE("voxel: setState and getState agree on all three states", "[voxel]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);
    std::unique_ptr<voxel_c> v = makeVoxel(gt, 3, 1, 1);

    v->setState(0, 0, 0, voxel_c::VX_FILLED);
    v->setState(1, 0, 0, voxel_c::VX_VARIABLE);
    /* (2,0,0) stays empty */

    REQUIRE(v->isFilled(0, 0, 0));
    REQUIRE(v->isVariable(1, 0, 0));
    REQUIRE(v->isEmpty(2, 0, 0));

    REQUIRE(v->countState(voxel_c::VX_FILLED) == 1);
    REQUIRE(v->countState(voxel_c::VX_VARIABLE) == 1);
    REQUIRE(v->countState(voxel_c::VX_EMPTY) == 1);
  }
}

TEST_CASE("voxel: colour is independent of state", "[voxel]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);
    std::unique_ptr<voxel_c> v = makeVoxel(gt, 2, 1, 1);

    v->setState(0, 0, 0, voxel_c::VX_FILLED);
    v->setColor(0, 0, 0, 5);

    REQUIRE(v->isFilled(0, 0, 0));
    REQUIRE(v->getColor(0, 0, 0) == 5);

    /* changing state must not disturb the colour */
    v->setState(0, 0, 0, voxel_c::VX_VARIABLE);
    REQUIRE(v->isVariable(0, 0, 0));
    REQUIRE(v->getColor(0, 0, 0) == 5);

    /* and changing colour must not disturb the state */
    v->setColor(0, 0, 0, 63);
    REQUIRE(v->isVariable(0, 0, 0));
    REQUIRE(v->getColor(0, 0, 0) == 63);
  }
}

TEST_CASE("voxel: get2 returns empty outside the space", "[voxel]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);
    std::unique_ptr<voxel_c> v = makeVoxel(gt, 2, 2, 2);
    v->setAll(voxel_c::VX_FILLED);

    /* inside is filled */
    REQUIRE(v->getState2(0, 0, 0) == voxel_c::VX_FILLED);
    REQUIRE(v->getState2(1, 1, 1) == voxel_c::VX_FILLED);

    /* every direction out of bounds reads as empty, never as a crash */
    REQUIRE(v->getState2(-1, 0, 0) == voxel_c::VX_EMPTY);
    REQUIRE(v->getState2(0, -1, 0) == voxel_c::VX_EMPTY);
    REQUIRE(v->getState2(0, 0, -1) == voxel_c::VX_EMPTY);
    REQUIRE(v->getState2(2, 0, 0) == voxel_c::VX_EMPTY);
    REQUIRE(v->getState2(0, 2, 0) == voxel_c::VX_EMPTY);
    REQUIRE(v->getState2(0, 0, 2) == voxel_c::VX_EMPTY);

    REQUIRE(v->isEmpty2(-1, -1, -1));
    REQUIRE(v->isFilled2(1, 1, 1));
  }
}

TEST_CASE("voxel: the ASCII fixture helper maps axis extents without transposition", "[voxel]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);
    /*
     * The dimensions here are all different (3, 2, 1), so a fromLayers that
     * assigned the extents to the wrong axes -- e.g. building a 2x3x1 space
     * instead of 3x2x1 -- is caught here. This case proves the EXTENT
     * mapping; it cannot by itself prove the coordinate mapping, because its
     * one filled voxel has two zero coordinates (see the companion case
     * below for that).
     */

    std::unique_ptr<voxel_c> v = fromLayers(gt, {
      { "..#",
        "..." },
    });

    REQUIRE(v->getX() == 3);
    REQUIRE(v->getY() == 2);
    REQUIRE(v->getZ() == 1);
  }
}

TEST_CASE("voxel: the ASCII fixture helper maps axis positions without transposition", "[voxel]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);
    /*
     * A cubic 3x3x3 space, so no pairwise axis swap changes the extents (an
     * extent-only check like the case above would be blind here). The one
     * filled voxel sits at (x=2, y=1, z=0) -- three mutually distinct
     * coordinates -- so every one of the three possible pairwise swaps
     * (X<->Y, X<->Z, Y<->Z) relocates it to a different, still in-bounds,
     * position:
     *   X<->Y swap -> (1,2,0)   X<->Z swap -> (0,1,2)   Y<->Z swap -> (2,0,1)
     * None of those coincide with (2,1,0), so a positional REQUIRE catches
     * every swap directly, without depending on a bt_assert bounds-check
     * exception (which compiles away under -DNDEBUG).
     */

    std::unique_ptr<voxel_c> v = fromLayers(gt, {
      { "...",
        "..#",
        "..." },
      { "...",
        "...",
        "..." },
      { "...",
        "...",
        "..." },
    });

    REQUIRE(v->isFilled(2, 1, 0));
    REQUIRE(v->countState(voxel_c::VX_FILLED) == 1);
  }
}

TEST_CASE("voxel: the ASCII fixture helper places voxels where it says", "[voxel]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);

    /* an L in the z=0 layer, one lone voxel in z=1 */
    std::unique_ptr<voxel_c> v = fromLayers(gt, {
      { "##",
        "#." },
      { "#.",
        ".." },
    });

    REQUIRE(v->getX() == 2);
    REQUIRE(v->getY() == 2);
    REQUIRE(v->getZ() == 2);

    REQUIRE(v->isFilled(0, 0, 0));
    REQUIRE(v->isFilled(1, 0, 0));
    REQUIRE(v->isFilled(0, 1, 0));
    REQUIRE(v->isEmpty(1, 1, 0));

    REQUIRE(v->isFilled(0, 0, 1));
    REQUIRE(v->isEmpty(1, 0, 1));

    REQUIRE(v->countState(voxel_c::VX_FILLED) == 4);
  }
}

TEST_CASE("voxel: fromLayers pads ragged rows and maps '+' to VX_VARIABLE", "[voxel]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);

    /*
     * layer 0 has a single 4-char row; layer 1 has three 1-char rows. The
     * space must be padded out to the widest row (sx=4) and the tallest
     * layer (sy=3), and every character not supplied by the ragged art
     * must default to VX_EMPTY rather than being left uninitialized.
     */
    std::unique_ptr<voxel_c> v = fromLayers(gt, {
      { "+..#" },
      { "#", "+", "." },
    });

    REQUIRE(v->getX() == 4);
    REQUIRE(v->getY() == 3);
    REQUIRE(v->getZ() == 2);

    /* '+' maps to VX_VARIABLE */
    REQUIRE(v->isVariable(0, 0, 0));
    REQUIRE(v->isVariable(0, 1, 1));

    /* '#' still maps to VX_FILLED in the same ragged art */
    REQUIRE(v->isFilled(3, 0, 0));
    REQUIRE(v->isFilled(0, 0, 1));

    /* a cell past the end of a row shorter than the widest row is padded empty */
    REQUIRE(v->isEmpty(1, 0, 1));

    /* a row past the end of a layer with fewer rows than the tallest layer is padded empty */
    REQUIRE(v->isEmpty(0, 2, 0));

    REQUIRE(v->countState(voxel_c::VX_VARIABLE) == 2);
    REQUIRE(v->countState(voxel_c::VX_FILLED) == 2);
  }
}

TEST_CASE("voxel: getIndex and indexToXYZ are inverses over the whole space", "[voxel][index]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);
    std::unique_ptr<voxel_c> v = makeVoxel(gt, 3, 4, 5);

    for (unsigned int z = 0; z < v->getZ(); z++)
      for (unsigned int y = 0; y < v->getY(); y++)
        for (unsigned int x = 0; x < v->getX(); x++) {
          int idx = v->getIndex(x, y, z);
          REQUIRE(idx >= 0);
          REQUIRE(static_cast<unsigned int>(idx) < v->getXYZ());

          unsigned int rx = 0, ry = 0, rz = 0;
          REQUIRE(v->indexToXYZ(idx, &rx, &ry, &rz));
          REQUIRE(rx == x);
          REQUIRE(ry == y);
          REQUIRE(rz == z);
        }
  }
}

TEST_CASE("voxel: every index maps to a distinct coordinate", "[voxel][index]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);
    std::unique_ptr<voxel_c> v = makeVoxel(gt, 3, 4, 5);

    std::vector<bool> seen(v->getXYZ(), false);

    for (unsigned int i = 0; i < v->getXYZ(); i++) {
      unsigned int x = 0, y = 0, z = 0;
      REQUIRE(v->indexToXYZ(i, &x, &y, &z));

      int back = v->getIndex(x, y, z);
      REQUIRE(static_cast<unsigned int>(back) == i);

      REQUIRE_FALSE(seen[i]);
      seen[i] = true;
    }

    for (bool s : seen)
      REQUIRE(s);
  }
}

TEST_CASE("voxel: the bounding box tracks the filled voxels", "[voxel][bbox]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);

    /* a single voxel at (1,1,0) in a 3x3x1 space */
    std::unique_ptr<voxel_c> v = fromLayers(gt, {
      { "...",
        ".#.",
        "..." },
    });

    REQUIRE(v->boundX1() == 1);
    REQUIRE(v->boundX2() == 1);
    REQUIRE(v->boundY1() == 1);
    REQUIRE(v->boundY2() == 1);
    REQUIRE(v->boundZ1() == 0);
    REQUIRE(v->boundZ2() == 0);
  }
}

TEST_CASE("voxel: minimizePiece crops away the padding on every grid -- to a space whose size "
          "the grid rounds to its own block", "[voxel][bbox]") {
  /* The shape is the same two touching cells on every grid, padded out to
     4x4x1. What minimizePiece guarantees is uniform; what the resulting
     SPACE looks like is not, and this case separates the two rather than
     weakening the assertion until one number fits all five grids.

     The uniform part -- the property minimizePiece actually owns -- is that
     the bounding box ends up exactly around the shape: two cells wide in x,
     one in y and z, with nothing filled outside it and no cell lost.

     The space is another matter. The rhombic and tetra-octa grids pack
     several sub-cells into each real grid position, so a space whose extent
     is not a whole number of those blocks does not describe a legal shape.
     voxel_3_c::minimizePiece (rhombic) therefore restores the lower
     corner's position modulo 5 and rounds the size up to a multiple of 5;
     voxel_4_c::minimizePiece (tetra-octa) restores it modulo 6 and rounds
     to a multiple of 3. The other three grids have a block of one and crop
     exactly.

     So the expected size is per-grid, as the design doc asks for where
     grids genuinely differ: a shared expectation loose enough to hold on
     all five would be satisfied by a minimizePiece that did nothing at
     all. */
  struct Expect {
    gridType_c::gridType grid;
    unsigned int sx, sy, sz;   //< the space minimizePiece leaves behind
  };

  const Expect expected[] = {
    { gridType_c::GT_BRICKS,           2, 1, 1 },
    { gridType_c::GT_TRIANGULAR_PRISM, 2, 1, 1 },
    { gridType_c::GT_SPHERES,          2, 1, 1 },
    { gridType_c::GT_RHOMBIC,          5, 5, 5 },   // rounded up to a multiple of 5
    { gridType_c::GT_TETRA_OCTA,       3, 3, 3 },   // rounded up to a multiple of 3
  };

  /* every grid is accounted for, so adding one to ALL_GRIDS without
     deciding what it should do here fails rather than silently skipping */
  REQUIRE(std::size(expected) == std::size(ALL_GRIDS));

  for (const Expect & e : expected) {
    INFO("grid " << gridName(e.grid));
    gridType_c gt(e.grid);

    std::unique_ptr<voxel_c> v = fromLayers(gt, {
      { "....",
        ".##.",
        "....",
        "...." },
    });

    const unsigned int before = v->countState(voxel_c::VX_FILLED);
    REQUIRE(before == 2);

    v->minimizePiece();

    /* uniform: nothing gained, nothing lost */
    REQUIRE(v->countState(voxel_c::VX_FILLED) == before);

    /* uniform: the bounding box is exactly the shape, wherever in the space
       the grid had to leave it */
    REQUIRE(v->boundX2() - v->boundX1() == 1);
    REQUIRE(v->boundY2() == v->boundY1());
    REQUIRE(v->boundZ2() == v->boundZ1());
    REQUIRE(v->isFilled(v->boundX1(),     v->boundY1(), v->boundZ1()));
    REQUIRE(v->isFilled(v->boundX1() + 1, v->boundY1(), v->boundZ1()));

    /* per-grid: the space the grid's own block rule leaves behind */
    REQUIRE(v->getX() == e.sx);
    REQUIRE(v->getY() == e.sy);
    REQUIRE(v->getZ() == e.sz);

    /* and it is a fixpoint -- minimizing an already-minimal piece must not
       keep growing it, which is the failure mode the rounding invites */
    v->minimizePiece();
    REQUIRE(v->getX() == e.sx);
    REQUIRE(v->getY() == e.sy);
    REQUIRE(v->getZ() == e.sz);
    REQUIRE(v->countState(voxel_c::VX_FILLED) == before);
  }
}

TEST_CASE("voxel: translate moves the shape and fills the vacated cells", "[voxel][bbox]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);

    std::unique_ptr<voxel_c> v = fromLayers(gt, {
      { "#..",
        "...",
        "..." },
    });

    REQUIRE(v->isFilled(0, 0, 0));

    v->translate(1, 1, 0, voxel_c::VX_EMPTY);

    REQUIRE(v->isEmpty(0, 0, 0));
    REQUIRE(v->isFilled(1, 1, 0));
    REQUIRE(v->countState(voxel_c::VX_FILLED) == 1);
  }
}

TEST_CASE("voxel: translating out and back is the identity", "[voxel][bbox]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);

    /* the shape must sit clear of the boundary in the direction of travel on
       EVERY axis: anything shifted off the edge cannot come back, so a round
       trip is lossless only when nothing falls off. Two filled cells, one
       per z-layer, so the round trip genuinely exercises dz (a sign error in
       translate's z branch would otherwise pass the whole suite unnoticed:
       no other case here ever passes a non-zero dz). */
    std::unique_ptr<voxel_c> original = fromLayers(gt, {
      { "....",
        ".#..",
        "...." },
      { "....",
        ".#..",
        "...." },
      { "....",
        "....",
        "...." },
    });
    std::unique_ptr<voxel_c> moved = copyVoxel(gt, *original);

    moved->translate(1, -1, 1, voxel_c::VX_EMPTY);
    moved->translate(-1, 1, -1, voxel_c::VX_EMPTY);

    REQUIRE(*moved == *original);
  }
}

TEST_CASE("voxel: resize keeps the overlapping region", "[voxel][bbox]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);

    std::unique_ptr<voxel_c> v = fromLayers(gt, {
      { "##",
        "##" },
    });

    REQUIRE(v->countState(voxel_c::VX_FILLED) == 4);

    /* growing keeps every voxel and pads with the filler */
    v->resize(4, 4, 1, voxel_c::VX_EMPTY);
    REQUIRE(v->getX() == 4);
    REQUIRE(v->getY() == 4);
    REQUIRE(v->countState(voxel_c::VX_FILLED) == 4);
    REQUIRE(v->isFilled(0, 0, 0));
    REQUIRE(v->isFilled(1, 1, 0));
    REQUIRE(v->isEmpty(3, 3, 0));

    /* shrinking discards what falls outside */
    v->resize(1, 1, 1, voxel_c::VX_EMPTY);
    REQUIRE(v->getX() == 1);
    REQUIRE(v->countState(voxel_c::VX_FILLED) == 1);
  }
}

TEST_CASE("voxel: the hotspot defaults to the origin and moves when set", "[voxel][hotspot]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);
    std::unique_ptr<voxel_c> v = makeVoxel(gt, 3, 3, 3);

    REQUIRE(v->getHx() == 0);
    REQUIRE(v->getHy() == 0);
    REQUIRE(v->getHz() == 0);

    v->setHotspot(1, 2, 0);

    REQUIRE(v->getHx() == 1);
    REQUIRE(v->getHy() == 2);
    REQUIRE(v->getHz() == 0);
  }
}

/* The scaling and mirror cases below stay on GT_BRICKS, deliberately, and
   the grid-spanning coverage for those operations is in the capability
   cases at the end of this file instead.

   Two reasons, both discovered by parametrizing them and watching what
   happened:

   Scaling is not a universal operation. voxel_c::scale() and
   voxel_c::scaleDown() are no-op defaults -- the header says so outright,
   "not all grids have such a possibility (e.g. spheres can't be scaled)".
   GT_SPHERES overrides neither, so scaling it does nothing;
   GT_TRIANGULAR_PRISM overrides scale but not scaleDown, so it scales up
   and never back down. A case asserting "scaling up multiplies the filled
   count by eight" cannot hold on all five, and loosening it until it does
   would leave an assertion satisfied by a scale() that did nothing at all.

   More fundamentally, an ASCII fixture is not a legal shape on the sparse
   grids. fromLayers() sets cells at every coordinate the art names, but
   GT_SPHERES, GT_RHOMBIC and GT_TETRA_OCTA only accept a subset: on the
   rhombic grid a 3x3x3 corner contains just six valid positions, and
   (0,0,0) is not one of them. Cells written outside them are not a
   differently-shaped piece, they are not a piece. That is invisible to the
   storage-level cases above -- setState, indexing, bounding boxes and
   translation do not consult validCoordinate -- but geometry does, which is
   why the mirror case does not merely fail on another grid, it aborts on
   voxel_2_c's own ((hx+hy+hz) & 1) == 0 assertion.

   So these keep the one grid whose art means what it looks like, and the
   cross-grid behaviour is asserted where it can be stated truthfully. */

TEST_CASE("voxel: scaling up multiplies the filled voxel count", "[voxel][scale]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  std::unique_ptr<voxel_c> v = fromLayers(gt, {
    { "#." },
  });

  const unsigned int before = v->countState(voxel_c::VX_FILLED);
  REQUIRE(before == 1);

  v->scale(2, false);

  /* each voxel becomes a 2x2x2 block */
  REQUIRE(v->countState(voxel_c::VX_FILLED) == before * 8);
}

TEST_CASE("voxel: scaling up then down is the identity", "[voxel][scale]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  std::unique_ptr<voxel_c> original = fromLayers(gt, {
    { "##",
      "#." },
  });
  std::unique_ptr<voxel_c> roundtrip = copyVoxel(gt, *original);

  roundtrip->scale(2, false);
  REQUIRE(roundtrip->scaleDown(2, true));

  REQUIRE(roundtrip->countState(voxel_c::VX_FILLED)
          == original->countState(voxel_c::VX_FILLED));
  REQUIRE(roundtrip->identicalInBB(original.get()));
}

TEST_CASE("voxel: scaleDown refuses a shape smaller than the divisor", "[voxel][scale]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  /* a single voxel cannot be halved. This only exercises the size guard at
     voxel_0.cpp:256 (sx < by || sy < by || sz < by); it never reaches the
     block-uniformity scan at voxel_0.cpp:258-272, which needs a shape at
     least as large as the divisor in every dimension. */
  std::unique_ptr<voxel_c> v = fromLayers(gt, {
    { "#" },
  });

  REQUIRE_FALSE(v->scaleDown(2, false));
}

TEST_CASE("voxel: scaleDown refuses a block that is not uniform", "[voxel][scale]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  /* a 2x2x2 space, exactly the size of the divisor, so the size guard at
     voxel_0.cpp:256 passes and the block-uniformity scan at :258-272 is
     actually reached. Its single candidate 2x2x2 block is one filled cell
     among seven empty ones, so no shift (shx/shy/shz) can make it uniform:
     the scan must return false for every shift, and the function as a
     whole returns false. */
  std::unique_ptr<voxel_c> v = fromLayers(gt, {
    { "#.",
      ".." },
    { "..",
      ".." },
  });

  REQUIRE_FALSE(v->scaleDown(2, false));
}

TEST_CASE("voxel: getMirrorTransform finds the mirror of a chiral shape", "[voxel][mirror]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  /* an S/Z tetromino pair: chiral in the plane, so one is the other's mirror */
  std::unique_ptr<voxel_c> s = fromLayers(gt, {
    { ".##",
      "##." },
  });
  std::unique_ptr<voxel_c> z = fromLayers(gt, {
    { "##.",
      ".##" },
  });

  const unsigned char tr = s->getMirrorTransform(z.get());

  /* 0 means "no mirror transformation relates these two" */
  INFO("mirror transform " << (int)tr);
  REQUIRE(tr != 0);

  /* getMirrorTransform searches only the mirror range and returns 0 when no
     mirror transformation relates the two shapes. An achiral shape IS related
     to itself by one, so a square also answers non-zero. (voxel.cpp:265) */
  std::unique_ptr<voxel_c> square = fromLayers(gt, {
    { "##",
      "##" },
  });
  REQUIRE(square->getMirrorTransform(square.get()) != 0);
}

TEST_CASE("voxel: identicalInBB is colour-sensitive", "[voxel]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);

    /* two copies with identical geometry, differing only in the colour of
       one filled cell */
    std::unique_ptr<voxel_c> a = fromLayers(gt, {
      { "##" },
    });
    std::unique_ptr<voxel_c> b = copyVoxel(gt, *a);
    b->setColor(0, 0, 0, 1);

    /* state (geometry) alone still matches ... */
    REQUIRE(a->identicalInBB(b.get(), false));
    /* ... but with includeColors == true (the default), it does not */
    REQUIRE_FALSE(a->identicalInBB(b.get(), true));
    REQUIRE_FALSE(a->identicalInBB(b.get()));
  }
}

TEST_CASE("voxel: copying preserves the name", "[voxel]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);

    std::unique_ptr<voxel_c> original = fromLayers(gt, {
      { "#." },
    });
    original->setName("original shape");

    /* bttest::copyVoxel(gt, orig) calls gridType_c::getVoxel(const voxel_c &),
       which binds to the reference overload and constructs the new shape via
       voxel_0_c(const voxel_c & orig) : voxel_c(orig) { } -- so this exercises
       voxel_c's REFERENCE copy constructor (voxel.cpp, `voxel_c(const voxel_c &
       orig)`). */
    std::unique_ptr<voxel_c> byRef = copyVoxel(gt, *original);
    REQUIRE(byRef->getName() == "original shape");

    /* gridType_c also has a getVoxel(const voxel_c *) overload, which
       mainWindow_c::cb_CopyShape() (src/gui/mainwindow.cpp) actually uses --
       puzzle_c::getShape() returns a voxel_c*, so that call resolves to the
       pointer overload and constructs the copy via voxel_0_c(const voxel_c *
       orig) : voxel_c(orig) { }, exercising voxel_c's POINTER copy
       constructor (voxel.cpp, `voxel_c(const voxel_c * orig)`). copyVoxel()
       only has a reference overload, so call gt.getVoxel() directly here to
       reach the pointer constructor and cover the exact path the GUI's Copy
       button takes. */
    std::unique_ptr<voxel_c> byPtr(gt.getVoxel(original.get()));
    REQUIRE(byPtr->getName() == "original shape");
  }
}


/* ------------------------------------------------------------------ */
/* what differs between grids                                          */
/* ------------------------------------------------------------------ */

TEST_CASE("voxel: which coordinates hold a cell at all differs by grid", "[voxel][grid]") {
  /* The three sparse grids subdivide space: a coordinate triple addresses a
     slot that may or may not correspond to a real cell of that grid's
     solid. validCoordinate is how a caller asks, and it is the reason an
     ASCII fixture built for the cube grid is not a shape on the others.

     Counted over a fixed 3x3x3 corner so the numbers are comparable, and
     asserted per grid rather than as an inequality: "fewer than 27" would
     hold for a validCoordinate that rejected everything. */
  struct Expect {
    gridType_c::gridType grid;
    unsigned int validInCorner;   //< of the 27 coordinates in a 3x3x3 corner
  };

  const Expect expected[] = {
    { gridType_c::GT_BRICKS,           27 },   // dense: every coordinate is a cube
    { gridType_c::GT_TRIANGULAR_PRISM, 27 },   // dense: the subdivision is within a cell
    { gridType_c::GT_SPHERES,          14 },
    { gridType_c::GT_RHOMBIC,           6 },
    { gridType_c::GT_TETRA_OCTA,        5 },
  };

  REQUIRE(std::size(expected) == std::size(ALL_GRIDS));

  for (const Expect & e : expected) {
    INFO("grid " << gridName(e.grid));
    gridType_c gt(e.grid);
    std::unique_ptr<voxel_c> v = makeVoxel(gt, 6, 6, 6);

    unsigned int valid = 0;
    for (int z = 0; z < 3; z++)
      for (int y = 0; y < 3; y++)
        for (int x = 0; x < 3; x++)
          if (v->validCoordinate(x, y, z)) valid++;

    REQUIRE(valid == e.validInCorner);
  }
}

TEST_CASE("voxel: the origin is a cell on the dense grids and not on the rhombic or tetra-octa "
          "ones", "[voxel][grid]") {
  /* The specific fact that makes cube-grid ASCII art meaningless on those
     two, stated on its own so a reader meets it directly rather than having
     to infer it from a count. */
  for (gridType_c::gridType t : { gridType_c::GT_BRICKS, gridType_c::GT_TRIANGULAR_PRISM,
                                  gridType_c::GT_SPHERES }) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);
    REQUIRE(makeVoxel(gt, 6, 6, 6)->validCoordinate(0, 0, 0));
  }

  for (gridType_c::gridType t : { gridType_c::GT_RHOMBIC, gridType_c::GT_TETRA_OCTA }) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);
    REQUIRE_FALSE(makeVoxel(gt, 6, 6, 6)->validCoordinate(0, 0, 0));
  }
}

TEST_CASE("voxel: scaling up is implemented by some grids and is a documented no-op on the rest",
          "[voxel][scale][grid]") {
  /* voxel_c::scale() does nothing by default and three grids override it.
     The contract is per-grid, so the expectation is too: a shared one loose
     enough for all five would be met by a scale() that never did anything.

     The fixture is a cube-grid shape, which the earlier note explains is
     not a legal piece on the sparse grids -- so this asserts only what
     survives that: whether the operation changes the space at all. The
     grids where the filled count is a meaningful number are the ones whose
     art is meaningful, and those get the ratio checked. */
  struct Expect {
    gridType_c::gridType grid;
    bool resizesSpace;      //< scale() enlarges the voxel space
    bool multipliesCells;   //< ...and by a factor of 8 in filled cells, on a legal shape
  };

  const Expect expected[] = {
    { gridType_c::GT_BRICKS,           true,  true  },
    { gridType_c::GT_TRIANGULAR_PRISM, true,  true  },
    { gridType_c::GT_SPHERES,          false, false },   // overrides neither; cannot be scaled
    { gridType_c::GT_RHOMBIC,          true,  false },
    { gridType_c::GT_TETRA_OCTA,       true,  false },
  };

  REQUIRE(std::size(expected) == std::size(ALL_GRIDS));

  for (const Expect & e : expected) {
    INFO("grid " << gridName(e.grid));
    gridType_c gt(e.grid);

    std::unique_ptr<voxel_c> v = fromLayers(gt, { { "##",
                                                   "#." } });

    const unsigned int beforeCells = v->countState(voxel_c::VX_FILLED);
    const unsigned int bx = v->getX(), by = v->getY(), bz = v->getZ();

    v->scale(2, false);

    const bool grew = v->getX() > bx || v->getY() > by || v->getZ() > bz;
    REQUIRE(grew == e.resizesSpace);

    if (e.multipliesCells)
      REQUIRE(v->countState(voxel_c::VX_FILLED) == beforeCells * 8);

    if (!e.resizesSpace) {
      /* the no-op default leaves the space untouched in every respect, not
         merely the same size */
      REQUIRE(v->getX() == bx);
      REQUIRE(v->getY() == by);
      REQUIRE(v->getZ() == bz);
      REQUIRE(v->countState(voxel_c::VX_FILLED) == beforeCells);
    }
  }
}

TEST_CASE("voxel: scaleDown reports whether the grid supports it before doing anything",
          "[voxel][scale][grid]") {
  /* scaleDown(by, false) is the documented "could you?" query -- the
     minimise-all-shapes path asks every shape before scaling any of them.
     Only the grids overriding it can ever answer yes; the base
     implementation returns false unconditionally.

     GT_TRIANGULAR_PRISM is the interesting one: it overrides scale() but
     NOT scaleDown(), so it can be scaled up and never back down. That
     asymmetry is invisible from the header's grid list and is exactly what
     a shared assertion would have hidden. */
  struct Expect {
    gridType_c::gridType grid;
    bool canEverScaleDown;
  };

  const Expect expected[] = {
    { gridType_c::GT_BRICKS,           true  },
    { gridType_c::GT_TRIANGULAR_PRISM, false },   // overrides scale() but not scaleDown()
    { gridType_c::GT_SPHERES,          false },
    { gridType_c::GT_RHOMBIC,          true  },
    { gridType_c::GT_TETRA_OCTA,       true  },
  };

  REQUIRE(std::size(expected) == std::size(ALL_GRIDS));

  for (const Expect & e : expected) {
    INFO("grid " << gridName(e.grid));
    gridType_c gt(e.grid);

    /* a 2x2x2 block of cube-grid cells, scaled up so that on the grids that
       do implement scaleDown there is genuinely something to scale back */
    std::unique_ptr<voxel_c> v = makeVoxel(gt, 2, 2, 2);
    v->setAll(voxel_c::VX_FILLED);
    v->scale(2, false);

    const bool answered = v->scaleDown(2, false);

    /* the query must not have changed anything -- action == false */
    const unsigned int cells = v->countState(voxel_c::VX_FILLED);
    REQUIRE(v->scaleDown(2, false) == answered);
    REQUIRE(v->countState(voxel_c::VX_FILLED) == cells);

    if (!e.canEverScaleDown) {
      REQUIRE_FALSE(answered);

      /* and asking it to act is refused too, rather than half-doing it */
      REQUIRE_FALSE(v->scaleDown(2, true));
      REQUIRE(v->countState(voxel_c::VX_FILLED) == cells);
    }
  }
}
