#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"

#include "lib/bt_assert.h"
#include "lib/gridtype.h"
#include "lib/symmetries.h"
#include "lib/voxel.h"

#include <memory>

using namespace bttest;

TEST_CASE("symmetries: every grid reports a transformation count", "[symmetry]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);
    const symmetries_c * sym = gt.getSymmetries();
    REQUIRE(sym != nullptr);

    const unsigned int plain  = sym->getNumTransformations();
    const unsigned int mirror = sym->getNumTransformationsMirror();

    REQUIRE(plain > 0);
    /* the mirrored count includes the plain ones, so it is never smaller */
    REQUIRE(mirror >= plain);
  }
}

TEST_CASE("symmetries: transformation 0 is the identity", "[symmetry]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);

    std::unique_ptr<voxel_c> v = makeVoxel(gt, 2, 2, 2);
    v->setState(0, 0, 0, voxel_c::VX_FILLED);
    v->setState(1, 0, 0, voxel_c::VX_FILLED);

    std::unique_ptr<voxel_c> copy(gt.getVoxel(v.get()));

    REQUIRE(copy->transform(0));
    /* some grids (e.g. GT_RHOMBIC, GT_TETRA_OCTA) pad the voxel space to a
       multiple of an internal cell size before any transform, including the
       identity, so the space can grow; the content within the bounding box
       is still untouched, so compare with identicalInBB rather than == */
    REQUIRE(copy->identicalInBB(v.get()));
  }
}

TEST_CASE("symmetries: transAdd composes transformations", "[symmetry]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);
    const symmetries_c * sym = gt.getSymmetries();

    const unsigned int n = sym->getNumTransformations();

    /* applying t1 then t2 must equal applying transAdd(t1, t2) in one go,
       whenever the composed transformation exists.

       The fixture is built only from cells with (x+y+z) even, because the
       spheres grid (GT_SPHERES) only has sphere centres on that half of a
       cubic lattice; the other cells are not valid sphere positions, and
       voxel_2_c::transform() silently drops filled cells that don't land on
       one (src/lib/voxel_2.cpp, the `if (((x+y+z) & 1) == 0)` guard around
       the per-voxel rotation loop). Using only even-sum cells keeps the
       fixture meaningful on every grid, spheres included.

       KNOWN COVERAGE GAP, GT_SPHERES only: this fixture only survives
       voxel_2_c::transform() for transformations 0-23 (the 24 plain cube
       rotations; transMult rows 0-23 are exactly the ones with no TND
       entries). Transformations 24-119 either fail transform() on this
       fixture's cell layout or land on a TND/thrown composition, so they
       get NO composition coverage here at all -- the 576 sphere pairs
       actually asserted below are precisely t1,t2 in {0..23}, already
       covered three times over by GT_BRICKS/GT_RHOMBIC/GT_TETRA_OCTA.
       Building an FCC-aligned fixture to close this gap is out of scope
       for this task; this comment exists so nobody mistakes the loop
       bound `n` for actual coverage of all `n` sphere transformations. */
    unsigned int thrown = 0, checked = 0;
    for (unsigned int t1 = 0; t1 < n; t1++)
      for (unsigned int t2 = 0; t2 < n; t2++) {
        unsigned char combined;
        try {
          combined = sym->transAdd(t1, t2);
        } catch (const assert_exception &) {
          /* KNOWN BUG, src/lib/symmetries_2.cpp:80: an extra bt_assert fires
             instead of returning TND, which src/lib/symmetries.h:129
             documents as the correct result when a composition does not
             exist. Debug builds only -- under NDEBUG bt_assert expands to
             ((void)0) and this branch is never taken, so release builds
             already return TND correctly. Counted and pinned below so this
             workaround cannot outlive the bug. */
          thrown++;
          continue;
        }
        if (combined == TND) continue;

        INFO("t1 " << t1 << " t2 " << t2 << " combined " << (int)combined);

        std::unique_ptr<voxel_c> stepwise = fromLayers(gt, {
          { "#.#",
            ".#.",
            "..." },
        });
        std::unique_ptr<voxel_c> direct(gt.getVoxel(stepwise.get()));

        if (!stepwise->transform(t1)) continue;
        if (!stepwise->transform(t2)) continue;
        if (!direct->transform(combined)) continue;

        checked++;
        REQUIRE(stepwise->identicalInBB(direct.get()));
      }

    /* 6912 of the 14400 plain-transformation sphere pairs hold TND in
       tabs_2/transmult.inc and hit the symmetries_2.cpp:80 bug above. When
       that bug is fixed, transAdd() starts returning TND for those pairs
       instead of throwing, the catch above stops firing, thrown drops to 0,
       and this REQUIRE fails -- which is the signal to delete the try/catch
       and this REQUIRE together. */
    REQUIRE(thrown == (t == gridType_c::GT_SPHERES ? 6912u : 0u));
    REQUIRE(checked > 0);
  }
}

TEST_CASE("symmetries: a shape is identical to its own rotations", "[symmetry]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);
    const symmetries_c * sym = gt.getSymmetries();

    /* built only from (x+y+z)-even cells; see the note in the transAdd
       case above on why the fixture must avoid the sphere grid's invalid
       lattice positions */
    std::unique_ptr<voxel_c> original = fromLayers(gt, {
      { "#.#",
        ".#.",
        "..." },
    });

    for (unsigned int tr = 0; tr < sym->getNumTransformations(); tr++) {
      INFO("transformation " << tr);

      std::unique_ptr<voxel_c> rotated(gt.getVoxel(original.get()));
      if (!rotated->transform(tr)) continue;

      /* a rotation never adds or removes material */
      REQUIRE(rotated->countState(voxel_c::VX_FILLED)
              == original->countState(voxel_c::VX_FILLED));

      /* and the shape is still recognisably the same one */
      REQUIRE(original->identicalWithRots(rotated.get(), false, false));
    }
  }
}

TEST_CASE("symmetries: a cube is symmetric under every transformation", "[symmetry]") {
  gridType_c gt(gridType_c::GT_BRICKS);
  const symmetries_c * sym = gt.getSymmetries();

  std::unique_ptr<voxel_c> cube = makeVoxel(gt, 2, 2, 2);
  cube->setAll(voxel_c::VX_FILLED);

  const symmetries_t s = cube->selfSymmetries();

  /* a solid cube maps onto itself under every rotation the grid has */
  for (unsigned int tr = 0; tr < sym->getNumTransformations(); tr++) {
    INFO("transformation " << tr);
    REQUIRE(sym->symmetrieContainsTransformation(s, tr));
  }
}

TEST_CASE("symmetries: an asymmetric shape has the trivial symmetry group", "[symmetry]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  /* a deliberately lopsided shape. Note: the brief's original fixture here
     ((0,0,0),(1,0,0),(0,1,0),(0,0,1) -- an L-tromino plus one cube stacked
     on top of the corner) is NOT actually asymmetric: it has one non-empty
     cell one step out along each of the three axes from a shared corner, so
     it is invariant under the 120-degree rotation about the corner's space
     diagonal that cyclically permutes x/y/z. BurrTools correctly detects
     that symmetry (selfSymmetries comes back non-trivial for it), so using
     it here would have made this test assert something false. This shape
     is a genuine spiral with no repeated arm lengths, which has no
     non-trivial self symmetry under the cube's rotation group. */
  std::unique_ptr<voxel_c> lopsided = fromLayers(gt, {
    { "#.",
      ".." },
    { "##",
      ".." },
    { ".#",
      ".#" },
  });

  const symmetries_t s = lopsided->selfSymmetries();

  /* transformation 0 always maps a shape onto itself */
  REQUIRE(gt.getSymmetries()->symmetrieContainsTransformation(s, 0));
  REQUIRE(unSymmetric(s));
}

TEST_CASE("symmetries: normalizeTransformation returns a usable transformation", "[symmetry]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);

    std::unique_ptr<voxel_c> v = fromLayers(gt, {
      { "##",
        "#." },
    });

    const unsigned int n = gt.getSymmetries()->getNumTransformationsMirror();

    for (unsigned int tr = 0; tr < n; tr++) {
      const unsigned char norm = v->normalizeTransformation(tr);
      INFO("transformation " << tr << " normalized to " << (int)norm);
      REQUIRE(norm < n);
    }
  }
}
