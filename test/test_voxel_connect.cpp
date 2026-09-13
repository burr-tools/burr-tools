#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"

#include "lib/gridtype.h"
#include "lib/voxel.h"

#include <memory>

using namespace bttest;

TEST_CASE("voxel: a solid block is face connected", "[voxel][connect]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  std::unique_ptr<voxel_c> v = fromLayers(gt, {
    { "##",
      "##" },
  });

  /* type 0 is face connectivity; check the filled voxels hang together */
  REQUIRE(v->connected(0, true, voxel_c::VX_EMPTY));
}

TEST_CASE("voxel: two separated voxels are not connected", "[voxel][connect]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  /* a gap between the two filled cells */
  std::unique_ptr<voxel_c> v = fromLayers(gt, {
    { "#.#" },
  });

  REQUIRE_FALSE(v->connected(0, true, voxel_c::VX_EMPTY));
}

TEST_CASE("voxel: diagonal voxels are not face connected but are edge connected", "[voxel][connect]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  /* corner-to-corner touch only: (0,0,0) and (1,1,0) share only the edge
     between them, not a face */
  std::unique_ptr<voxel_c> v = fromLayers(gt, {
    { "#.",
      ".#" },
  });

  /* type 0 is face connectivity: no shared face, so not connected */
  REQUIRE_FALSE(v->connected(0, true, voxel_c::VX_EMPTY));

  /* type 1 is edge connectivity. voxel_0_c::getNeighbor's type-1 branch
     (src/lib/voxel_0.cpp:172-188) lists the 12 edge neighbours of a cell,
     including idx 0: (x-1, y-1, z) -- exactly the relationship between
     (1,1,0) and (0,0,0) in this fixture, so under edge connectivity the
     SAME shape IS connected. */
  REQUIRE(v->connected(1, true, voxel_c::VX_EMPTY));
}

TEST_CASE("voxel: an L shape is connected", "[voxel][connect]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  std::unique_ptr<voxel_c> v = fromLayers(gt, {
    { "#..",
      "#..",
      "###" },
  });

  REQUIRE(v->connected(0, true, voxel_c::VX_EMPTY));
}

TEST_CASE("voxel: connectivity spans layers", "[voxel][connect]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  /* one voxel per layer, stacked in z */
  std::unique_ptr<voxel_c> stacked = fromLayers(gt, {
    { "#" },
    { "#" },
  });
  REQUIRE(stacked->connected(0, true, voxel_c::VX_EMPTY));

  /* one voxel per layer, offset so they only touch at an edge */
  std::unique_ptr<voxel_c> offset = fromLayers(gt, {
    { "#." },
    { ".#" },
  });
  REQUIRE_FALSE(offset->connected(0, true, voxel_c::VX_EMPTY));
}

TEST_CASE("voxel: fillHoles closes an enclosed cavity", "[voxel][connect]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  /* a ring in a single layer: the centre is enclosed in x and y but open
     in z, so a 3D hole fill must not close it */
  std::unique_ptr<voxel_c> ring = fromLayers(gt, {
    { "###",
      "#.#",
      "###" },
  });

  const unsigned int before = ring->countState(voxel_c::VX_FILLED);
  ring->fillHoles(0);

  /* open in z, so nothing is enclosed and nothing is filled */
  REQUIRE(ring->countState(voxel_c::VX_FILLED) == before);

  /* a fully enclosed 3x3x3 shell has one trapped cell at the centre */
  std::unique_ptr<voxel_c> shell = fromLayers(gt, {
    { "###",
      "###",
      "###" },
    { "###",
      "#.#",
      "###" },
    { "###",
      "###",
      "###" },
  });

  REQUIRE(shell->isEmpty(1, 1, 1));
  REQUIRE(shell->countState(voxel_c::VX_FILLED) == 26);

  shell->fillHoles(0);

  REQUIRE(shell->countState(voxel_c::VX_FILLED) == 27);
  REQUIRE_FALSE(shell->isEmpty(1, 1, 1));
}

TEST_CASE("voxel: counting states adds up to the whole space", "[voxel][connect]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);
    std::unique_ptr<voxel_c> v = makeVoxel(gt, 2, 2, 2);

    v->setState(0, 0, 0, voxel_c::VX_FILLED);
    v->setState(1, 0, 0, voxel_c::VX_FILLED);
    v->setState(0, 1, 0, voxel_c::VX_VARIABLE);

    const unsigned int total = v->countState(voxel_c::VX_EMPTY)
                             + v->countState(voxel_c::VX_FILLED)
                             + v->countState(voxel_c::VX_VARIABLE);

    REQUIRE(total == v->getXYZ());
    REQUIRE(v->countState(voxel_c::VX_FILLED) == 2);
    REQUIRE(v->countState(voxel_c::VX_VARIABLE) == 1);
  }
}
