#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "lib/gridtype.h"
#include "lib/problem.h"
#include "lib/puzzle.h"
#include "lib/solver/math.h"
#include "lib/solver/shape.h"
#include "lib/voxel.h"
#include "test_helpers.h"

#include <cmath>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace {

std::vector<gridType_c::gridType> allGridTypes() {
  return {
    gridType_c::GT_BRICKS,
    gridType_c::GT_TRIANGULAR_PRISM,
    gridType_c::GT_SPHERES,
    gridType_c::GT_RHOMBIC,
    gridType_c::GT_TETRA_OCTA,
    gridType_c::GT_HEXAGONAL_PRISM,
    gridType_c::GT_CAIRO_PRISM,
    gridType_c::GT_RHOMBIC_TETRAHEDRA,
    gridType_c::GT_OCTAHEDRAL_TETRAHEDRA,
    gridType_c::GT_DESCRIBED_BRICKS,
    gridType_c::GT_DESCRIBED_TRIANGULAR_PRISM,
  };
}

} // namespace

TEST_CASE("core linear-algebra value types", "[shape][math]") {
  const vec3i_t a{ 1, -2, 3 };
  const vec3i_t b{ 4, 5, -6 };
  CHECK(a + b == vec3i_t{ 5, 3, -3 });
  CHECK(a - b == vec3i_t{ -3, -7, 9 });
  CHECK(-a == vec3i_t{ -1, 2, -3 });
  CHECK(2 * a == vec3i_t{ 2, -4, 6 });
  CHECK(dot(a, b) == 4 - 10 - 18);
  CHECK(cross(vec3i_t{ 1, 0, 0 }, vec3i_t{ 0, 1, 0 }) == vec3i_t{ 0, 0, 1 });

  // 90-degree rotation about +Z
  const mat3d_t rotZ({
    0.0, -1.0, 0.0,
    1.0,  0.0, 0.0,
    0.0,  0.0, 1.0
  });
  CHECK(rotZ.determinant() == Catch::Approx(1.0));
  CHECK(rotZ * vec3d_t{ 1.0, 0.0, 2.0 } == vec3d_t{ 0.0, 1.0, 2.0 });
  CHECK(rotZ * rotZ.transpose() == mat3d_t::identity());

  const pose3d_t p1{ rotZ, { 3.0, -4.0, 5.0 } };
  const pose3d_t p2{ mat3d_t::identity(), { 1.0, 2.0, -1.0 } };
  const vec3d_t pt{ 2.0, -1.0, 4.0 };
  const vec3d_t composed = (p1 * p2).apply(pt);
  const vec3d_t sequential = p1.apply(p2.apply(pt));
  CHECK(composed.x == Catch::Approx(sequential.x));
  CHECK(composed.y == Catch::Approx(sequential.y));
  CHECK(composed.z == Catch::Approx(sequential.z));

  const vec3d_t rt = p1.inverse().apply(p1.apply(pt));
  CHECK(rt.x == Catch::Approx(pt.x));
  CHECK(rt.y == Catch::Approx(pt.y));
  CHECK(rt.z == Catch::Approx(pt.z));

  float mat4[16];
  p1.toMat4ColMajor(mat4);
  CHECK(mat4[0] == 0.0f);
  CHECK(mat4[1] == 1.0f);
  CHECK(mat4[4] == -1.0f);
  CHECK(mat4[10] == 1.0f);
  CHECK(mat4[12] == 3.0f);
  CHECK(mat4[13] == -4.0f);
  CHECK(mat4[14] == 5.0f);
  CHECK(mat4[15] == 1.0f);
}

TEST_CASE("(n, k) cell addressing is a total bijection on every grid", "[shape][grid]") {
  for (gridType_c::gridType gtEnum : allGridTypes()) {
    DYNAMIC_SECTION("gridType " << static_cast<int>(gtEnum)) {
      gridType_c gt(gtEnum);
      std::unique_ptr<voxel_c> probe(gt.getVoxel(1, 1, 1, voxel_c::VX_EMPTY));
      const unsigned int K = gt.kindCount();
      REQUIRE(K >= 1);

      // 1. Every (n, k) with 0 <= k < K maps to a valid voxel coordinate and round-trips.
      // Note: legacy voxel_3_c::validCoordinate uses signed `% 5` and is only
      // defined for non-negative coordinates; (n, k) handles all signs cleanly.
      for (int nz = -2; nz <= 2; nz++)
        for (int ny = -2; ny <= 2; ny++)
          for (int nx = -2; nx <= 2; nx++)
            for (unsigned int k = 0; k < K; k++) {
              const cellAddr_t c{ { nx, ny, nz }, static_cast<uint16_t>(k) };
              const vec3i_t v = gt.cellToVoxel(c);
              if (gtEnum != gridType_c::GT_RHOMBIC || (v.x >= 0 && v.y >= 0 && v.z >= 0))
                CHECK(probe->validCoordinate(v.x, v.y, v.z));
              const auto back = gt.voxelToCell(v);
              REQUIRE(back.has_value());
              CHECK(*back == c);
            }

      // 2. Every voxel coordinate in [-6..6]^3 maps iff validCoordinate() is true.
      const int lo = (gtEnum == gridType_c::GT_RHOMBIC) ? 0 : -6;
      const int hi = (gtEnum == gridType_c::GT_RHOMBIC) ? 10 : 6;
      for (int z = lo; z <= hi; z++)
        for (int y = lo; y <= hi; y++)
          for (int x = lo; x <= hi; x++) {
            const bool valid = probe->validCoordinate(x, y, z);
            const auto cell = gt.voxelToCell({ x, y, z });
            CHECK(cell.has_value() == valid);
            if (cell) {
              CHECK(cell->k < K);
              CHECK(gt.cellToVoxel(*cell) == vec3i_t{ x, y, z });
            }
          }

      // 3. Lattice translations match onGrid() and commute with cellToVoxel().
      for (int dz = -2; dz <= 2; dz++)
        for (int dy = -2; dy <= 2; dy++)
          for (int dx = -2; dx <= 2; dx++) {
            const vec3i_t dn{ dx, dy, dz };
            const vec3i_t dv = gt.latticeToVoxelDelta(dn);
            CHECK(probe->onGrid(dv.x, dv.y, dv.z));
            const auto back = gt.voxelDeltaToLattice(dv);
            REQUIRE(back.has_value());
            CHECK(*back == dn);

            const cellAddr_t c{ { 1, -1, 2 }, static_cast<uint16_t>(K - 1) };
            CHECK(gt.cellToVoxel(c.translated(dn)) == gt.cellToVoxel(c) + dv);
          }

      // 4. Non-onGrid voxel deltas are rejected by voxelDeltaToLattice().
      for (int z = -5; z <= 5; z++)
        for (int y = -5; y <= 5; y++)
          for (int x = -5; x <= 5; x++) {
            const bool on = probe->onGrid(x, y, z);
            const auto lat = gt.voxelDeltaToLattice({ x, y, z });
            CHECK(lat.has_value() == on);
            if (lat)
              CHECK(gt.latticeToVoxelDelta(*lat) == vec3i_t{ x, y, z });
          }

      // 5. Out-of-range k throws unconditionally.
      CHECK_THROWS_AS(gt.cellToVoxel({ { 0, 0, 0 }, static_cast<uint16_t>(K) }), assert_exception);
    }
  }
}

TEST_CASE("shape_c operations match voxel_c on every grid type", "[shape][grid]") {
  for (gridType_c::gridType gtEnum : allGridTypes()) {
    DYNAMIC_SECTION("gridType " << static_cast<int>(gtEnum)) {
      gridType_c gt(gtEnum);
      const unsigned int K = gt.kindCount();

      // Build a small asymmetric shape over (n, k) with mixed states and colours.
      shape_c s;
      s.set({ { 0, 0, 0 }, 0 }, cellState_e::Filled, 1);
      s.set({ { 1, 0, 0 }, 0 }, cellState_e::Filled, 0);
      s.set({ { 0, 1, 0 }, static_cast<uint16_t>(K > 1 ? 1 : 0) }, cellState_e::Filled, 2);
      s.set({ { 0, 0, 1 }, 0 }, cellState_e::Variable, 0);

      CHECK(s.size() == 4);
      CHECK(s.filledCount() == 3);
      CHECK(s.variableCount() == 1);

      // Normalisation and translation invariance
      const shape_c shifted = s.translated({ 3, -2, 5 });
      CHECK(s.equalUpToTranslation(shifted, true));
      CHECK(s.normalised() == shifted.normalised());

      vec3i_t shift{};
      const shape_c norm = shifted.normalised(&shift);
      CHECK(norm.translated(shift) == shifted);

      // Round-trip through dense voxel_c storage
      auto v = s.toVoxel(gt);
      REQUIRE(v != nullptr);
      const shape_c roundtripped = shape_c::fromVoxel(*v);
      CHECK(roundtripped.equalUpToTranslation(s, true));

      // Every grid transformation matches voxel_c::transform() up to lattice translation
      const unsigned int nt = gt.getSymmetries()->getNumTransformationsMirror();
      for (unsigned int tr = 0; tr < nt; tr++) {
        std::unique_ptr<voxel_c> vt(gt.getVoxel(v.get()));
        const bool vOk = vt->transform(tr);
        const auto sTr = roundtripped.transformed(gt, tr);
        CHECK(sTr.has_value() == vOk);
        if (vOk && sTr) {
          const shape_c vtShape = shape_c::fromVoxel(*vt);
          CHECK(sTr->equalUpToTranslation(vtShape, true));

          // Check physical transform matrix matches voxel_c::getTransformMatrix
          double vm[9];
          vt->getTransformMatrix(tr, vm);
          const mat3d_t pm = gt.physicalTransformMatrix(tr);
          for (int i = 0; i < 9; i++)
            CHECK(pm.m[i] == Catch::Approx(vm[i]).margin(1e-9));
        }
      }

      // symmetries() and selfSymmetries() match voxel_c::selfSymmetries() (which respects colours)
      const symmetries_t vsym = v->selfSymmetries();
      CHECK(roundtripped.selfSymmetries(gt) == vsym);
      std::vector<unsigned int> expectedTurns;
      for (unsigned int tr = 0; tr < nt; tr++)
        if (gt.getSymmetries()->symmetrieContainsTransformation(vsym, tr))
          expectedTurns.push_back(tr);
      CHECK(roundtripped.symmetries(gt, true) == expectedTurns);
    }
  }
}

TEST_CASE("shape_c matches voxel_c across bundled example puzzles", "[shape]") {
  const std::vector<std::string> files = {
    "examples/PelikanBurr.xmpuzzle",
    "examples/Bermuda.xmpuzzle",
    "examples/Prisgon.xmpuzzle",
    "examples/BallRoom.xmpuzzle",
    "examples/HexSticks.xmpuzzle",
    "examples/AugmentedSecondStellation.xmpuzzle",
    "examples/FourPieceTetrahedron.xmpuzzle",
    "examples/CubeInCage.xmpuzzle",
  };

  for (const auto & path : files) {
    DYNAMIC_SECTION(path) {
      auto p = puzzle_c::load(path);
      REQUIRE(p != nullptr);
      const gridType_c & gt = *p->getGridType();
      const unsigned int nt = gt.getSymmetries()->getNumTransformations();

      for (unsigned int si = 0; si < p->getNumberOfShapes(); si++) {
        const voxel_c * v = p->getShape(si);
        REQUIRE(v != nullptr);
        const shape_c s = shape_c::fromVoxel(*v);

        CHECK(s.filledCount() == v->countState(voxel_c::VX_FILLED));
        CHECK(s.variableCount() == v->countState(voxel_c::VX_VARIABLE));
        CHECK(s.selfSymmetries(gt) == v->selfSymmetries());

        // Check round-trip toVoxel -> identicalInBB
        auto vBack = s.toVoxel(gt);
        CHECK(v->identicalInBB(vBack.get(), true));

        // Sample rotations and check hotspot offset formula matches voxel_c::transform
        const unsigned int step = (nt > 24) ? 10 : 3;
        for (unsigned int tr = 0; tr < nt; tr += step) {
          std::unique_ptr<voxel_c> vt(gt.getVoxel(v));
          const bool ok = vt->transform(tr);
          const auto st = s.transformed(gt, tr);
          CHECK(st.has_value() == ok);
          if (ok && st && !s.empty()) {
            const shape_c vtShape = shape_c::fromVoxel(*vt);
            CHECK(st->equalUpToTranslation(vtShape, true));

            // Verify transformVoxelOffset reproduces voxel_c's transformed hotspot relative to anchor
            const vec3i_t origH{ v->getHx(), v->getHy(), v->getHz() };
            const vec3i_t origA = gt.cellToVoxel(s.cells().front().cell);
            const auto off = gt.transformVoxelOffset(origH, origA, tr);
            REQUIRE(off.has_value());

            // Find where origA landed inside vt:
            // Compare via shape cells: vt's hotspot minus the image of origA
            const vec3i_t vtH{ vt->getHx(), vt->getHy(), vt->getHz() };
            // For every cell in s, its transformed coordinate in vt is vtAnchor + (st_cell_vox - st_anchor_vox);
            // here check specifically when tr == 0 or via matching cell position in vt.
            if (gt.getType() != gridType_c::GT_SPHERES) {
              const auto mappedA = gt.transformCell(s.cells().front().cell, tr);
              REQUIRE(mappedA.has_value());
              const vec3i_t dn = vtShape.cells().front().cell.n - st->cells().front().cell.n;
              const vec3i_t vtA = gt.cellToVoxel(mappedA->translated(dn));
              CHECK(vtH - vtA == *off);
            }
          }
        }
      }
    }
  }
}
