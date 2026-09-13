#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"

#include "lib/gridtype.h"
#include "lib/voxel.h"

#include <memory>

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
