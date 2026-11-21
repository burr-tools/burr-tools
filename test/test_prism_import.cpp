/* Triangular prism puzzles on BurrTools NG's grid (step C3 of
 * design/2026-11-19-ng-merge-plan.md): files of the native type 1 are
 * converted when they are read, everywhere, with their saved assemblies and
 * disassemblies.
 *
 * The cells a converted piece covers are checked against NG's importer's map
 * (written out here again): native (x, y, z) becomes, with
 * x' = x + 1, y' = floor((3y + 1) / 2) - floor((x + y mod 2) / 2),
 * the NG cell (x' + z, y' + z, -x' - y' + z).
 */
#include <catch2/catch_test_macros.hpp>

#include "lib/assembly.h"
#include "lib/converter.h"
#include "lib/disassembly.h"
#include "lib/gridtype.h"
#include "lib/problem.h"
#include "lib/puzzle.h"
#include "lib/solution.h"
#include "lib/voxel.h"
#include "lib/solver/solverspec.h"
#include "tools/xml.h"

#include <algorithm>
#include <array>
#include <memory>
#include <set>
#include <sstream>
#include <vector>

namespace {

typedef std::array<int, 3> cell_t;

const char * const EXAMPLES[] = {
  "examples/Bermuda.xmpuzzle",
  "examples/Prisgon.xmpuzzle",
};

int fdiv2(int a) { return a >= 0 ? a / 2 : -((-a + 1) / 2); }

cell_t importerCell(const cell_t & c) {
  const int x = c[0], y = c[1], z = c[2];
  const int xn = x + 1;
  const int yn = fdiv2(3 * y + 1) - fdiv2(x + ((y % 2) + 2) % 2);
  return { xn + z, yn + z, -xn - yn + z };
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

std::set<cell_t> moved(const std::set<cell_t> & cells, int dx, int dy, int dz) {
  std::set<cell_t> res;
  for (const cell_t & c : cells)
    res.insert({c[0] + dx, c[1] + dy, c[2] + dz});
  return res;
}

std::set<cell_t> mapped(const std::set<cell_t> & cells, const cell_t & shift) {
  std::set<cell_t> res;
  for (const cell_t & c : cells) {
    const cell_t n = importerCell(c);
    res.insert({n[0] - shift[0], n[1] - shift[1], n[2] - shift[2]});
  }
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

/* every state of both separations, piece by piece: the converted piece's
 * cells moved from its assembled position to its position in the state are
 * the native piece's cells moved the same way, through the importer's map
 * (a state holds positions, as the assembly does) */
struct piece_s {
  std::set<cell_t> cells;
  cell_t pos;
};

void checkSeparation(const separation_c & sn, const separation_c & sg,
                     const std::vector<piece_s> & nativeCells,
                     const std::vector<piece_s> & describedCells, const cell_t & shift) {
  REQUIRE(sg.getMoves() == sn.getMoves());
  REQUIRE(sg.getPieces() == sn.getPieces());
  for (unsigned int s = 0; s <= sn.getMoves(); s++) {
    const state_c & a = *sn.getState(s);
    const state_c & b = *sg.getState(s);
    std::set<cell_t> occupied;
    for (unsigned int i = 0; i < sn.getPieceNumber(); i++) {
      INFO("state " << s << ", piece " << sn.getPieceName(i));
      CHECK(b.pieceRemoved(i) == a.pieceRemoved(i));
      if (a.pieceRemoved(i)) continue;
      const unsigned int p = sn.getPieceName(i);
      const piece_s & n = nativeCells[p];
      const piece_s & d = describedCells[p];
      const std::set<cell_t> g = moved(d.cells, b.getX(i) - d.pos[0], b.getY(i) - d.pos[1], b.getZ(i) - d.pos[2]);
      CHECK(mapped(moved(n.cells, a.getX(i) - n.pos[0], a.getY(i) - n.pos[1], a.getZ(i) - n.pos[2]), shift) == g);
      for (const cell_t & c : g)
        CHECK(occupied.insert(c).second);  // no two pieces in one cell
    }
  }
  REQUIRE(bool(sg.getRemoved()) == bool(sn.getRemoved()));
  REQUIRE(bool(sg.getLeft()) == bool(sn.getLeft()));
  if (sn.getRemoved()) checkSeparation(*sn.getRemoved(), *sg.getRemoved(), nativeCells, describedCells, shift);
  if (sn.getLeft()) checkSeparation(*sn.getLeft(), *sg.getLeft(), nativeCells, describedCells, shift);
}

} // namespace

TEST_CASE("prisms: examples load onto NG's grid, saved assemblies and disassemblies on the same cells",
          "[lattice][prismimport]") {
  for (const char * file : EXAMPLES) {
    INFO(file);
    auto nat = puzzle_c::load(file, cutCubeGrids_e::nativeAll);
    REQUIRE(nat->getGridType()->getType() == gridType_c::GT_TRIANGULAR_PRISM);
    for (cutCubeGrids_e cut : { cutCubeGrids_e::ng, cutCubeGrids_e::native }) {
      // every reader but burrTxt --native-grids converts prisms, the FLTK GUI's too
      auto ng = puzzle_c::load(file, cut);
      REQUIRE(ng->getGridType()->getType() == gridType_c::GT_DESCRIBED_TRIANGULAR_PRISM);
      REQUIRE(ng->getNumberOfShapes() == nat->getNumberOfShapes());
      REQUIRE(ng->getNumberOfProblems() == nat->getNumberOfProblems());

      for (unsigned int pr = 0; pr < nat->getNumberOfProblems(); pr++) {
        INFO("problem " << pr);
        const problem_c & pn = *nat->getProblem(pr);
        const problem_c & pg = *ng->getProblem(pr);
        CHECK(pg.getSolveState() == pn.getSolveState());
        REQUIRE(pg.getNumberOfSavedSolutions() == pn.getNumberOfSavedSolutions());
        if (!pn.resultValid() || pn.getNumberOfSavedSolutions() == 0) continue;

        const std::set<cell_t> rn = filledCells(*getResultShape(pn));
        const std::set<cell_t> rg = filledCells(*getResultShape(pg));
        const cell_t lm = lowest(mapped(rn, {0, 0, 0})), lg = lowest(rg);
        const cell_t shift = {lm[0] - lg[0], lm[1] - lg[1], lm[2] - lg[2]};
        REQUIRE(mapped(rn, shift) == rg);

        for (unsigned int s = 0; s < pn.getNumberOfSavedSolutions(); s++) {
          INFO("solution " << s);
          const solution_c & soln = *pn.getSavedSolution(s);
          const solution_c & solg = *pg.getSavedSolution(s);
          CHECK(solg.getAssemblyNumber() == soln.getAssemblyNumber());
          CHECK(solg.getSolutionNumber() == soln.getSolutionNumber());
          const auto sn = soln.getAssembly()->createPieceSpaces(*makeSolverSpec(pn));
          const auto sg = solg.getAssembly()->createPieceSpaces(*makeSolverSpec(pg));
          REQUIRE(sg.size() == sn.size());
          std::vector<piece_s> cn, cg;
          const assembly_c & an = *soln.getAssembly();
          const assembly_c & ag = *solg.getAssembly();
          size_t k = 0;
          for (unsigned int i = 0; i < an.placementCount(); i++) {
            if (!an.isPlaced(i)) continue;
            REQUIRE(k < sn.size());
            cn.push_back({filledCells(*sn[k]), {an.getX(i), an.getY(i), an.getZ(i)}});
            cg.push_back({filledCells(*sg[k]), {ag.getX(i), ag.getY(i), ag.getZ(i)}});
            CHECK(mapped(cn.back().cells, shift) == cg.back().cells);
            k++;
          }
          REQUIRE(bool(solg.getDisassembly()) == bool(soln.getDisassembly()));
          if (soln.getDisassembly())
            checkSeparation(*soln.getDisassembly(), *solg.getDisassembly(), cn, cg, shift);
        }
      }
    }
  }
}

TEST_CASE("prisms: a converted file saves and loads back unchanged", "[lattice][prismimport][xml]") {
  for (const char * file : EXAMPLES) {
    INFO(file);
    auto ng = puzzle_c::load(file);
    const std::string once = saved(*ng);
    CHECK(once.find("<gridType type=\"1\"") == std::string::npos);
    std::istringstream in(once);
    xmlParser_c pars(in);
    auto back = puzzle_c::read(pars);
    CHECK(back->getGridType()->getType() == gridType_c::GT_DESCRIBED_TRIANGULAR_PRISM);
    CHECK(saved(*back) == once);
  }
}
