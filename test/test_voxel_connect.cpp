#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"

#include "lib/gridtype.h"
#include "lib/voxel.h"

#include <memory>

using namespace bttest;

TEST_CASE("voxel: a solid block is face connected", "[voxel][connect][grid]") {
  /* "Solid block" has to mean a block of cells the grid accepts, not a
     block of coordinates.

     connected() and unionFind() both filter through validCoordinate
     (voxel.cpp:425, voxel.cpp:485), while countState() and the ASCII
     fixtures address raw storage slots. A 2x2x2 of cube-grid coordinates
     contains none that GT_RHOMBIC or GT_TETRA_OCTA accept, so written that
     way this case handed those two grids a shape with no participating
     cells and collected the vacuous "an empty shape is connected" answer
     -- the very thing the case below exists to cover separately.

     Hence the box size and the non-vacuity assertion: both are load
     bearing, not belt-and-braces. */
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);

    std::unique_ptr<voxel_c> v = makeVoxel(gt, LEGAL_SHAPE_BOX, LEGAL_SHAPE_BOX, LEGAL_SHAPE_BOX);
    v->setAll(voxel_c::VX_FILLED);

    /* more than one, or "they hang together" is trivially true */
    REQUIRE(countValidFilled(*v) > 1);

    /* type 0 is face connectivity; check the filled voxels hang together */
    REQUIRE(v->connected(0, true, voxel_c::VX_EMPTY));
  }
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

/* The remaining cases above stay on GT_BRICKS.

   Connectivity is not a grid-agnostic property, and parametrizing these
   does not produce a weaker assertion, it produces a false one. Each grid
   defines its own neighbour relation through voxel_c::getNeighbor, so two
   cells that share no face on the cube grid may share one on the rhombic
   grid and vice versa. Run over ALL_GRIDS, "two separated voxels are not
   connected" fails because on some grids they are; "an L shape is
   connected" fails because on some grids it is not; and fillHoles closes a
   different number of cells because the cavity it is asked about is a
   different shape.

   The ASCII fixtures compound it: as test_voxel.cpp's note on the sparse
   grids records, art written for the cube grid does not describe a legal
   shape on GT_SPHERES, GT_RHOMBIC or GT_TETRA_OCTA at all.

   So the cube-grid cases keep their concrete, checkable claims, and what
   actually is shared across grids is asserted below on its own terms. */

TEST_CASE("voxel: face connectivity implies edge and corner connectivity on every grid",
          "[voxel][connect][grid]") {
  /* The one connectivity property that holds whatever the neighbour
     relation is, because the three types are nested by construction: cells
     sharing a face necessarily share an edge and a corner. voxel.h states
     the nesting ("face, edge or corner") and this is what holds it to it.

     Asserted over a solid block, which is face-connected on every grid --
     established by the parametrized case above, so this is not assuming
     it. A shape that were NOT face-connected would satisfy the implication
     vacuously and the case would be testing nothing, which is why the
     premise is re-asserted here rather than taken on trust. */
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);

    std::unique_ptr<voxel_c> v = makeVoxel(gt, LEGAL_SHAPE_BOX, LEGAL_SHAPE_BOX, LEGAL_SHAPE_BOX);
    v->setAll(voxel_c::VX_FILLED);

    /* the implication is vacuous over a shape with no participating cells,
       so pin that there are some before leaning on it */
    REQUIRE(countValidFilled(*v) > 1);

    REQUIRE(v->connected(0, false, voxel_c::VX_FILLED));   // face -- the premise
    REQUIRE(v->connected(1, false, voxel_c::VX_FILLED));   // edge
    REQUIRE(v->connected(2, false, voxel_c::VX_FILLED));   // corner
  }
}

TEST_CASE("voxel: a single filled cell is connected under every connectivity type on every grid",
          "[voxel][connect][grid]") {
  /* The degenerate case, which no grid may get wrong: one cell has nothing
     to be disconnected from. Worth pinning because a connectivity search
     that starts from the wrong seed, or counts a component of size one as
     no component at all, fails exactly here and nowhere else. */
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);

    for (char type = 0; type < 3; type++) {
      INFO("connectivity type " << (int)type);

      /* The cell has to be one the grid accepts. (1,1,1) is not: GT_SPHERES
         wants an even coordinate sum and GT_RHOMBIC's predicate rejects it,
         and because countState() counts storage slots while connected()
         filters through validCoordinate, filling it there satisfied the
         count assertion and then took the empty-shape branch -- passing
         without once exercising the single-cell path this case is named
         for. Ask the grid for a cell instead. */
      std::unique_ptr<voxel_c> v = legalShape(gt, LEGAL_SHAPE_BOX, 1);
      REQUIRE(v != nullptr);

      REQUIRE(v->countState(voxel_c::VX_FILLED) == 1);
      REQUIRE(countValidFilled(*v) == 1);        // and the grid agrees it is a cell
      REQUIRE(v->connected(type, false, voxel_c::VX_FILLED));
    }
  }
}

TEST_CASE("voxel: an empty space reports connected rather than crashing on every grid",
          "[voxel][connect][grid]") {
  /* connected() has to pick a starting cell; with none to pick it must
     answer rather than reading past the end. The answer is "yes" -- there
     is no disconnected group -- which is the vacuous truth, but it is the
     branch that a search seeding itself unconditionally would fall off. */
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);

    std::unique_ptr<voxel_c> v = makeVoxel(gt, 3, 3, 3);
    REQUIRE(v->countState(voxel_c::VX_FILLED) == 0);

    REQUIRE(v->connected(0, false, voxel_c::VX_FILLED));
  }
}
