/* Rhombic and tetra-octa puzzles on BurrTools NG's grids (step C1 of
 * design/2026-11-19-ng-merge-plan.md, cutCubeGrids_e): files of the native
 * types are converted when they are read, saved assemblies with them; the
 * FLTK GUI reads them natively.
 *
 * The cells a converted piece covers are checked against NG's importer's
 * map, written out here again: tetra-octa cells stay where they are; each
 * period of 5 of the rhombic grid is spread to 6, and every native rhombic
 * voxel becomes the two NG cells on either side of its residue 2.
 */
#include <catch2/catch_test_macros.hpp>

#include "lib/assembly.h"
#include "lib/converter.h"
#include "lib/gridtype.h"
#include "lib/latticegridinfo.h"
#include "lib/problem.h"
#include "lib/puzzle.h"
#include "lib/solution.h"
#include "lib/symmetries.h"
#include "lib/voxel.h"
#include "lib/solver/solverspec.h"
#include "tools/xml.h"

#include <algorithm>
#include <array>
#include <memory>
#include <random>
#include <set>
#include <sstream>
#include <vector>

namespace {

typedef std::array<int, 3> cell_t;

/* every example on the native rhombic and tetra-octa grids */
const char * const EXAMPLES[] = {
  "examples/12PieceSeparation.xmpuzzle",
  "examples/AugmentedSecondStellation.xmpuzzle",
  "examples/BrokenSticks.xmpuzzle",
  "examples/DiagonalCube.xmpuzzle",
  "examples/FourPieceTetrahedron.xmpuzzle",
  "examples/HexSticks.xmpuzzle",
  "examples/PermutatedThirdStellation.xmpuzzle",
  "examples/PiecesOfEight.xmpuzzle",
};

/* NG's importer's map of one native cell (import.cpp at fdae7951) */
std::vector<cell_t> importerCells(gridType_c::gridType native, const cell_t & c) {
  if (native == gridType_c::GT_TETRA_OCTA)
    return { c };
  REQUIRE(native == gridType_c::GT_RHOMBIC);
  cell_t n;
  int along = -1;
  for (int k = 0; k < 3; k++) {
    const int r = c[k] % 5;
    n[k] = 6 * (c[k] / 5) + (r > 2 ? r + 1 : r);
    if (r == 2) along = k;
  }
  REQUIRE(along >= 0);
  cell_t m = n;
  m[along]++;
  return { n, m };
}

std::set<cell_t> filledCells(const voxel_c & v) {
  std::set<cell_t> res;
  for (unsigned int z = 0; z < v.getZ(); z++)
    for (unsigned int y = 0; y < v.getY(); y++)
      for (unsigned int x = 0; x < v.getX(); x++)
        if (v.getState(x, y, z) != voxel_c::VX_EMPTY)
          res.insert({(int)x, (int)y, (int)z});
  return res;
}

std::set<cell_t> mapped(gridType_c::gridType native, const std::set<cell_t> & cells, const cell_t & shift) {
  std::set<cell_t> res;
  for (const cell_t & c : cells)
    for (const cell_t & n : importerCells(native, c))
      res.insert({n[0] - shift[0], n[1] - shift[1], n[2] - shift[2]});
  return res;
}

cell_t lowest(const std::set<cell_t> & cells) {
  cell_t lo = *cells.begin();
  for (const cell_t & c : cells)
    for (int k = 0; k < 3; k++) lo[k] = std::min(lo[k], c[k]);
  return lo;
}

std::string saved(const puzzle_c & p) {
  std::ostringstream out;
  {
    xmlWriter_c xml(out);
    p.save(xml);
  }
  return out.str();
}

std::unique_ptr<puzzle_c> readBack(const std::string & text) {
  std::istringstream in(text);
  xmlParser_c pars(in);
  return puzzle_c::read(pars);
}

} // namespace

TEST_CASE("cut cube grids: the native transformations are NG's with the same matrix", "[lattice][cutcube]") {
  for (gridType_c::gridType native : {gridType_c::GT_BRICKS, gridType_c::GT_RHOMBIC, gridType_c::GT_TETRA_OCTA}) {
    INFO("grid " << native);
    const latticeGridInfo_c & inf = latticeGridInfo_c::get(describedCounterpart(native));
    REQUIRE(inf.transformations() == 48);
    std::set<unsigned char> seen;
    for (unsigned int t = 0; t < 48; t++) {
      INFO("transformation " << t);
      const unsigned char d = describedTransformation(native, t);
      REQUIRE(d != TND);
      CHECK(d < inf.transformations());
      // rotations stay rotations, the mirrored ones mirrored
      CHECK((t < 24) == (d < inf.rotations()));
      seen.insert(d);
    }
    CHECK(seen.size() == 48);
    CHECK(describedTransformation(native, 48) == TND);
  }
  CHECK(describedTransformation(gridType_c::GT_TRIANGULAR_PRISM, 0) == TND);
  CHECK(describedTransformation(gridType_c::GT_SPHERES, 0) == TND);
}

TEST_CASE("cut cube grids: a shape converted, then turned, is the shape turned, then converted", "[lattice][cutcube]") {
  std::mt19937 rng(20261120);
  for (gridType_c::gridType native : {gridType_c::GT_RHOMBIC, gridType_c::GT_TETRA_OCTA}) {
    INFO("grid " << native);
    for (int round = 0; round < 6; round++) {
      // a random shape and each of its transformations, as one native puzzle
      puzzle_c p(new gridType_c(native));
      const int size = native == gridType_c::GT_RHOMBIC ? 10 : 6;
      std::unique_ptr<voxel_c> v(p.getGridType()->getVoxel(size, size, size, voxel_c::VX_EMPTY));
      std::uniform_int_distribution<int> d(0, size - 1);
      for (int i = 0, placed = 0; i < 4000 && placed < 7; i++) {
        const int x = d(rng), y = d(rng), z = d(rng);
        if (v->validCoordinate(x, y, z) && v->isEmpty(x, y, z)) {
          v->setState(x, y, z, voxel_c::VX_FILLED);
          placed++;
        }
      }
      v->minimizePiece();
      v->initHotspot();
      for (unsigned int t = 0; t < 48; t++) {
        std::unique_ptr<voxel_c> c(p.getGridType()->getVoxel(v.get()));
        REQUIRE(c->transform(t));
        c->minimizePiece();
        p.addShape(c.release());
      }

      std::unique_ptr<puzzle_c> ng(convertToDescribed(&p));
      REQUIRE(ng);
      const voxel_c * base = ng->getShape(0);   // transformation 0 is the identity
      for (unsigned int t = 0; t < 48; t++) {
        INFO("transformation " << t);
        std::unique_ptr<voxel_c> turned(ng->getGridType()->getVoxel(base));
        REQUIRE(turned->transform(describedTransformation(native, t)));
        CHECK(turned->identicalInBB(ng->getShape(t)));
      }
    }
  }
}

TEST_CASE("cut cube grids: examples load onto NG's grids, their saved assemblies on the same cells", "[lattice][cutcube]") {
  for (const char * file : EXAMPLES) {
    INFO(file);
    auto nat = puzzle_c::load(file, cutCubeGrids_e::native);
    auto ng = puzzle_c::load(file);
    const gridType_c::gridType native = nat->getGridType()->getType();
    REQUIRE((native == gridType_c::GT_RHOMBIC || native == gridType_c::GT_TETRA_OCTA));
    CHECK(ng->getGridType()->getType() == describedCounterpart(native));
    REQUIRE(ng->getNumberOfShapes() == nat->getNumberOfShapes());
    REQUIRE(ng->getNumberOfProblems() == nat->getNumberOfProblems());
    CHECK(ng->getComment() == nat->getComment());
    CHECK(ng->colorNumber() == nat->colorNumber());

    // each shape covers the cells the importer maps its cells to
    for (unsigned int s = 0; s < nat->getNumberOfShapes(); s++) {
      const std::set<cell_t> a = filledCells(*nat->getShape(s));
      const std::set<cell_t> b = filledCells(*ng->getShape(s));
      if (a.empty()) { CHECK(b.empty()); continue; }
      const std::set<cell_t> m = mapped(native, a, {0, 0, 0});
      const cell_t lm = lowest(m), lb = lowest(b);
      CHECK(mapped(native, a, {lm[0] - lb[0], lm[1] - lb[1], lm[2] - lb[2]}) == b);
    }

    for (unsigned int pr = 0; pr < nat->getNumberOfProblems(); pr++) {
      INFO("problem " << pr);
      const problem_c & pn = *nat->getProblem(pr);
      const problem_c & pg = *ng->getProblem(pr);
      CHECK(pg.getName() == pn.getName());
      CHECK(pg.getSolveState() == pn.getSolveState());
      if (pn.numAssembliesKnown()) CHECK(pg.getNumAssemblies() == pn.getNumAssemblies());
      REQUIRE(pg.getNumberOfSavedSolutions() == pn.getNumberOfSavedSolutions());
      if (!pn.resultValid() || pn.getNumberOfSavedSolutions() == 0) continue;

      // the assemblies' frame is the result's: the shift between the frames
      const std::set<cell_t> rn = filledCells(*getResultShape(pn));
      const std::set<cell_t> rg = filledCells(*getResultShape(pg));
      const cell_t lm = lowest(mapped(native, rn, {0, 0, 0})), lg = lowest(rg);
      const cell_t shift = {lm[0] - lg[0], lm[1] - lg[1], lm[2] - lg[2]};
      REQUIRE(mapped(native, rn, shift) == rg);

      for (unsigned int s = 0; s < pn.getNumberOfSavedSolutions(); s++) {
        INFO("assembly " << s);
        const assembly_c & an = *pn.getSavedSolution(s)->getAssembly();
        const assembly_c & ag = *pg.getSavedSolution(s)->getAssembly();
        CHECK(pg.getSavedSolution(s)->getAssemblyNumber() == pn.getSavedSolution(s)->getAssemblyNumber());
        REQUIRE(ag.placementCount() == an.placementCount());
        for (unsigned int i = 0; i < an.placementCount(); i++)
          CHECK(ag.isPlaced(i) == an.isPlaced(i));

        const auto sn = an.createPieceSpaces(*makeSolverSpec(pn));
        const auto sg = ag.createPieceSpaces(*makeSolverSpec(pg));
        REQUIRE(sg.size() == sn.size());
        std::set<cell_t> all;
        for (size_t i = 0; i < sn.size(); i++) {
          INFO("placed piece " << i);
          const std::set<cell_t> cg = filledCells(*sg[i]);
          CHECK(mapped(native, filledCells(*sn[i]), shift) == cg);
          all.insert(cg.begin(), cg.end());
        }
        // and together they lie in the converted result
        CHECK(std::includes(rg.begin(), rg.end(), all.begin(), all.end()));
      }
    }
  }
}

TEST_CASE("cut cube grids: a converted file saves and loads back unchanged", "[lattice][cutcube][xml]") {
  for (const char * file : EXAMPLES) {
    INFO(file);
    auto ng = puzzle_c::load(file);
    const std::string once = saved(*ng);
    CHECK(once.find("<gridType type=\"3\"") == std::string::npos);
    CHECK(once.find("<gridType type=\"4\"") == std::string::npos);
    auto back = readBack(once);
    CHECK(back->getGridType()->getType() == ng->getGridType()->getType());
    CHECK(saved(*back) == once);
  }
}

TEST_CASE("cut cube grids: reading natively keeps the file's grid, and the native writer writes it again", "[lattice][cutcube][xml]") {
  auto nat = puzzle_c::load("examples/HexSticks.xmpuzzle", cutCubeGrids_e::native);
  CHECK(nat->getGridType()->getType() == gridType_c::GT_RHOMBIC);
  const std::string once = saved(*nat);
  std::istringstream in(once);
  xmlParser_c pars(in);
  auto back = puzzle_c::read(pars, cutCubeGrids_e::native);
  CHECK(back->getGridType()->getType() == gridType_c::GT_RHOMBIC);
  CHECK(saved(*back) == once);
}

TEST_CASE("cut cube grids: new puzzles and conversions from bricks use the grids asked for", "[lattice][cutcube]") {
  using gt = gridType_c;
  CHECK(gt::offeredForNewPuzzles(gt::GT_RHOMBIC_TETRAHEDRA));
  CHECK(gt::offeredForNewPuzzles(gt::GT_OCTAHEDRAL_TETRAHEDRA));
  CHECK_FALSE(gt::offeredForNewPuzzles(gt::GT_RHOMBIC));
  CHECK_FALSE(gt::offeredForNewPuzzles(gt::GT_TETRA_OCTA));
  CHECK(gt::offeredForNewPuzzles(gt::GT_RHOMBIC, cutCubeGrids_e::native));
  CHECK(gt::offeredForNewPuzzles(gt::GT_TETRA_OCTA, cutCubeGrids_e::native));
  CHECK_FALSE(gt::offeredForNewPuzzles(gt::GT_RHOMBIC_TETRAHEDRA, cutCubeGrids_e::native));
  CHECK(gt::offeredForNewPuzzles(gt::GT_HEXAGONAL_PRISM));
  CHECK(gt::offeredForNewPuzzles(gt::GT_CAIRO_PRISM));
  CHECK(gt::offeredForNewPuzzles(gt::GT_HEXAGONAL_PRISM, cutCubeGrids_e::native));
  CHECK(gt::offeredForNewPuzzles(gt::GT_CAIRO_PRISM, cutCubeGrids_e::native));
  // the prisms on NG's grids, in every GUI (FLTK edits them since PR 30)
  CHECK(gt::offeredForNewPuzzles(gt::GT_DESCRIBED_TRIANGULAR_PRISM));
  CHECK(gt::offeredForNewPuzzles(gt::GT_DESCRIBED_TRIANGULAR_PRISM, cutCubeGrids_e::native));
  for (cutCubeGrids_e cut : {cutCubeGrids_e::ng, cutCubeGrids_e::native}) {
    CHECK(gt::offeredForNewPuzzles(gt::GT_BRICKS, cut));
    CHECK_FALSE(gt::offeredForNewPuzzles(gt::GT_TRIANGULAR_PRISM, cut));
    CHECK(gt::offeredForNewPuzzles(gt::GT_SPHERES, cut));
    CHECK_FALSE(gt::offeredForNewPuzzles(gt::GT_DESCRIBED_BRICKS, cut));
  }

  CHECK(canConvert(gt::GT_BRICKS, gt::GT_RHOMBIC_TETRAHEDRA));
  CHECK(canConvert(gt::GT_BRICKS, gt::GT_OCTAHEDRAL_TETRAHEDRA));
  CHECK_FALSE(canConvert(gt::GT_BRICKS, gt::GT_RHOMBIC));
  CHECK(canConvert(gt::GT_BRICKS, gt::GT_RHOMBIC, cutCubeGrids_e::native));
  CHECK(canConvert(gt::GT_BRICKS, gt::GT_TETRA_OCTA, cutCubeGrids_e::native));
  CHECK_FALSE(canConvert(gt::GT_BRICKS, gt::GT_RHOMBIC_TETRAHEDRA, cutCubeGrids_e::native));

  // a 2x1x1 brick: to NG's grids, the same cells as through the native grid
  puzzle_c bricks(new gridType_c(gt::GT_BRICKS));
  bricks.getShape(bricks.addShape(2, 1, 1))->setAll(voxel_c::VX_FILLED);
  for (auto [ng, native] : {std::pair{gt::GT_RHOMBIC_TETRAHEDRA, gt::GT_RHOMBIC},
                            std::pair{gt::GT_OCTAHEDRAL_TETRAHEDRA, gt::GT_TETRA_OCTA}}) {
    std::unique_ptr<puzzle_c> a(doConvert(&bricks, ng));
    std::unique_ptr<puzzle_c> b(doConvert(&bricks, native, cutCubeGrids_e::native));
    REQUIRE(a);
    REQUIRE(b);
    CHECK(a->getGridType()->getType() == ng);
    CHECK(b->getGridType()->getType() == native);
    std::unique_ptr<puzzle_c> c(convertToDescribed(b.get()));
    CHECK(a->getShape(0)->identicalInBB(c->getShape(0)));
    CHECK(doConvert(&bricks, native) == nullptr);
  }
}
