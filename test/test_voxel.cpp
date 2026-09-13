#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"

#include "lib/gridtype.h"
#include "lib/voxel.h"

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
  gridType_c gt(gridType_c::GT_BRICKS);
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

TEST_CASE("voxel: get2 returns empty outside the space", "[voxel]") {
  gridType_c gt(gridType_c::GT_BRICKS);
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

TEST_CASE("voxel: the ASCII fixture helper maps axis extents without transposition", "[voxel]") {
  /*
   * The dimensions here are all different (3, 2, 1), so a fromLayers that
   * assigned the extents to the wrong axes -- e.g. building a 2x3x1 space
   * instead of 3x2x1 -- is caught here. This case proves the EXTENT
   * mapping; it cannot by itself prove the coordinate mapping, because its
   * one filled voxel has two zero coordinates (see the companion case
   * below for that).
   */
  gridType_c gt(gridType_c::GT_BRICKS);

  std::unique_ptr<voxel_c> v = fromLayers(gt, {
    { "..#",
      "..." },
  });

  REQUIRE(v->getX() == 3);
  REQUIRE(v->getY() == 2);
  REQUIRE(v->getZ() == 1);
}

TEST_CASE("voxel: the ASCII fixture helper maps axis positions without transposition", "[voxel]") {
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
  gridType_c gt(gridType_c::GT_BRICKS);

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

TEST_CASE("voxel: the ASCII fixture helper places voxels where it says", "[voxel]") {
  gridType_c gt(gridType_c::GT_BRICKS);

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

TEST_CASE("voxel: fromLayers pads ragged rows and maps '+' to VX_VARIABLE", "[voxel]") {
  gridType_c gt(gridType_c::GT_BRICKS);

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
  gridType_c gt(gridType_c::GT_BRICKS);
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

TEST_CASE("voxel: the bounding box tracks the filled voxels", "[voxel][bbox]") {
  gridType_c gt(gridType_c::GT_BRICKS);

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

TEST_CASE("voxel: minimizePiece crops to the bounding box", "[voxel][bbox]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  std::unique_ptr<voxel_c> v = fromLayers(gt, {
    { "....",
      ".##.",
      "....",
      "...." },
  });

  const unsigned int before = v->countState(voxel_c::VX_FILLED);
  REQUIRE(before == 2);

  v->minimizePiece();

  /* the shape survives, the padding does not */
  REQUIRE(v->countState(voxel_c::VX_FILLED) == before);
  REQUIRE(v->getX() == 2);
  REQUIRE(v->getY() == 1);
  REQUIRE(v->getZ() == 1);
  REQUIRE(v->isFilled(0, 0, 0));
  REQUIRE(v->isFilled(1, 0, 0));
}

TEST_CASE("voxel: translate moves the shape and fills the vacated cells", "[voxel][bbox]") {
  gridType_c gt(gridType_c::GT_BRICKS);

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

TEST_CASE("voxel: translating out and back is the identity", "[voxel][bbox]") {
  gridType_c gt(gridType_c::GT_BRICKS);

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

TEST_CASE("voxel: resize keeps the overlapping region", "[voxel][bbox]") {
  gridType_c gt(gridType_c::GT_BRICKS);

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

TEST_CASE("voxel: the hotspot defaults to the origin and moves when set", "[voxel][hotspot]") {
  gridType_c gt(gridType_c::GT_BRICKS);
  std::unique_ptr<voxel_c> v = makeVoxel(gt, 3, 3, 3);

  REQUIRE(v->getHx() == 0);
  REQUIRE(v->getHy() == 0);
  REQUIRE(v->getHz() == 0);

  v->setHotspot(1, 2, 0);

  REQUIRE(v->getHx() == 1);
  REQUIRE(v->getHy() == 2);
  REQUIRE(v->getHz() == 0);
}

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
  gridType_c gt(gridType_c::GT_BRICKS);

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
