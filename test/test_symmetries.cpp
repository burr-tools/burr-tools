#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"

#include "lib/gridtype.h"
#include "lib/symmetries.h"
#include "lib/voxel.h"

#include <memory>

using namespace bttest;

namespace {

/**
 * A deliberately lopsided shape with a trivial rotation stabiliser,
 * including mirrors: no non-identity transformation maps it onto itself.
 *
 * Note: the brief's original fixture here ((0,0,0),(1,0,0),(0,1,0),(0,0,1)
 * -- an L-tromino plus one cube stacked on top of the corner) is NOT
 * actually asymmetric: it has one non-empty cell one step out along each
 * of the three axes from a shared corner, so it is invariant under the
 * 120-degree rotation about the corner's space diagonal that cyclically
 * permutes x/y/z. BurrTools correctly detects that symmetry
 * (selfSymmetries comes back non-trivial for it), so using it would have
 * made a test that relies on trivial symmetry assert something false.
 * This shape is a genuine spiral with no repeated arm lengths, which has
 * no non-trivial self symmetry under the cube's rotation group.
 */
std::unique_ptr<voxel_c> lopsidedSpiral(const gridType_c & gt) {
  return fromLayers(gt, {
    { "#.",
      ".." },
    { "##",
      ".." },
    { ".#",
      ".#" },
  });
}

} // namespace

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

    /* a 2-cell domino has a rotation stabiliser of order 8: transform(0)
       could secretly apply any of those 8 rotations and this case would
       still pass. lopsidedSpiral has a TRIVIAL stabiliser (including
       mirrors), so transform(0) producing anything other than the true
       identity would show up as a mismatch below. */
    std::unique_ptr<voxel_c> v = lopsidedSpiral(gt);

    std::unique_ptr<voxel_c> copy = copyVoxel(gt, *v);

    REQUIRE(copy->transform(0));

    /* GT_RHOMBIC and GT_TETRA_OCTA pad the voxel space to a multiple of an
       internal cell size (5 / 6 respectively) before any transform,
       including the identity, so the space can grow; the content within
       the bounding box is still untouched, so compare with identicalInBB
       rather than ==.

       KNOWN LIMITATION, GT_TRIANGULAR_PRISM and GT_SPHERES only:
       voxel_1_c::transform() (src/lib/voxel_1.cpp:47) and
       voxel_2_c::transform() (src/lib/voxel_2.cpp:33) both open with
       `if (nr == 0) return true;` -- for these two grids transform(0) is a
       hardcoded no-op that never reaches the rotation math at all. No
       choice of fixture can turn this REQUIRE into a real check of
       "transform 0 behaves as the identity" on those two grids: the code
       guarantees `v` is untouched before this assertion even runs, so the
       assertion below is trivially true there by construction. It stays
       in the loop (rather than being skipped) so the case still documents
       the actual, if trivial, per-grid behaviour instead of silently
       narrowing ALL_GRIDS down to three. */
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
       fixture's cell layout or land on a TND composition, so they
       get NO composition coverage here at all -- the 576 sphere pairs
       actually asserted below are precisely t1,t2 in {0..23}, already
       covered three times over by GT_BRICKS/GT_RHOMBIC/GT_TETRA_OCTA.
       Building an FCC-aligned fixture to close this gap is out of scope
       for this task; this comment exists so nobody mistakes the loop
       bound `n` for actual coverage of all `n` sphere transformations. */
    unsigned int checked = 0;
    for (unsigned int t1 = 0; t1 < n; t1++)
      for (unsigned int t2 = 0; t2 < n; t2++) {
        unsigned char combined = sym->transAdd(t1, t2);
        if (combined == TND) continue;

        INFO("t1 " << t1 << " t2 " << t2 << " combined " << (int)combined);

        std::unique_ptr<voxel_c> stepwise = fromLayers(gt, {
          { "#.#",
            ".#.",
            "..." },
        });
        std::unique_ptr<voxel_c> direct = copyVoxel(gt, *stepwise);

        if (!stepwise->transform(t1)) continue;
        if (!stepwise->transform(t2)) continue;
        if (!direct->transform(combined)) continue;

        checked++;
        REQUIRE(stepwise->identicalInBB(direct.get()));
      }

    /* REQUIRE(checked > 0) alone floors this branch's most valuable
       coverage at a single assertion: the continues just above skip
       silently on any transform() failure, so a regression that made
       transform() always return false would collapse `checked` from
       thousands down to 1 and this case would still go green. Pin the
       measured value exactly, so a silent collapse in coverage fails
       loudly instead of passing quietly.
       Measured directly from this test body (printf, three repeated runs,
       all identical -- the loop is purely deterministic, no RNG involved)
       before being encoded here. GT_BRICKS, GT_SPHERES, GT_RHOMBIC and
       GT_TETRA_OCTA all measure 576 (n=24, and voxel_0_c::transform, which
       GT_RHOMBIC/GT_TETRA_OCTA delegate to after padding, never fails);
       GT_TRIANGULAR_PRISM measures fewer because voxel_1_c::transform can
       legitimately return false (src/lib/voxel_1.cpp:429). */
    const unsigned int expectedChecked =
      (t == gridType_c::GT_TRIANGULAR_PRISM) ? 144u : 576u;
    REQUIRE(checked == expectedChecked);
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

      std::unique_ptr<voxel_c> rotated = copyVoxel(gt, *original);
      if (!rotated->transform(tr)) continue;

      /* a rotation never adds or removes material */
      REQUIRE(rotated->countState(voxel_c::VX_FILLED)
              == original->countState(voxel_c::VX_FILLED));

      /* and the shape is still recognisably the same one */
      REQUIRE(original->identicalWithRots(rotated.get(), false, false));
    }
  }
}

TEST_CASE("symmetries: identicalWithRots is colour-sensitive", "[symmetry][color]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  /* lopsidedSpiral has a trivial rotation stabiliser, including mirrors:
     no non-identity transform maps it onto itself geometrically. That
     means transform 0 is the ONLY transform under which these two copies
     can possibly agree, which isolates the includeColors flag as the only
     thing that can flip the result below -- a coincidental geometric match
     at some other transform cannot smuggle a false positive/negative in. */
  std::unique_ptr<voxel_c> original = lopsidedSpiral(gt);
  std::unique_ptr<voxel_c> recoloured = copyVoxel(gt, *original);
  recoloured->setColor(0, 0, 0, 1);

  /* state (geometry) alone still matches, via transform 0 */
  REQUIRE(original->identicalWithRots(recoloured.get(), false, false));
  /* with colours included, no transform makes the two agree */
  REQUIRE_FALSE(original->identicalWithRots(recoloured.get(), false, true));
}

TEST_CASE("symmetries: a cube is symmetric under every transformation", "[symmetry]") {
  /* deliberately GT_BRICKS only, unlike the ALL_GRIDS loop its siblings
     use: a filled 2x2x2 space is a geometric cube only under GT_BRICKS. On
     the other four grids the same voxel extents describe a lattice
     fragment (a rhombic/tetrahedral-octahedral/etc. cell cluster) with no
     such symmetry, so iterating ALL_GRIDS here would assert something
     FALSE. Do not "fix" this into an ALL_GRIDS loop. */
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

  /* see lopsidedSpiral's doc comment above for why this particular shape,
     and not the brief's original fixture, is the one that is genuinely
     asymmetric. */
  std::unique_ptr<voxel_c> lopsided = lopsidedSpiral(gt);

  const symmetries_t s = lopsided->selfSymmetries();

  /* transformation 0 always maps a shape onto itself */
  REQUIRE(gt.getSymmetries()->symmetrieContainsTransformation(s, 0));
  REQUIRE(unSymmetric(s));
}

TEST_CASE("symmetries: normalizeTransformation returns a usable transformation", "[symmetry]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);

    /* built only from (x+y+z)-even cells; see the note in the transAdd
       case above on why the fixture must avoid the sphere grid's invalid
       lattice positions, now that this case applies transform() itself */
    std::unique_ptr<voxel_c> v = fromLayers(gt, {
      { "#.#",
        ".#.",
        "..." },
    });

    const unsigned int n = gt.getSymmetries()->getNumTransformationsMirror();

    for (unsigned int tr = 0; tr < n; tr++) {
      const unsigned char norm = v->normalizeTransformation(tr);
      INFO("transformation " << tr << " normalized to " << (int)norm);
      REQUIRE(norm < n);

      /* REQUIRE(norm < n) alone is satisfied by ANY in-range value,
         including a stub `return 0;`. voxel.h:573-575 documents the real
         contract: norm must be the smallest transformation number that
         results in a shape IDENTICAL to applying tr. Assert that directly:
         transforming one copy by tr and another by norm must land on the
         same shape. norm <= tr is also part of "smallest", but by itself
         it is satisfied by the same `return 0;` stub, so it is checked in
         addition to, never instead of, the identity check. */
      REQUIRE(norm <= tr);

      std::unique_ptr<voxel_c> byTr = copyVoxel(gt, *v);
      if (!byTr->transform(tr)) continue;

      std::unique_ptr<voxel_c> byNorm = copyVoxel(gt, *v);
      REQUIRE(byNorm->transform(norm));

      REQUIRE(byTr->identicalInBB(byNorm.get()));
    }
  }
}
