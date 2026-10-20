#include <catch2/catch_test_macros.hpp>

#include "lib/assembly.h"
#include "lib/disassembler_0.h"
#include "lib/disassemblerpool.h"
#include "lib/disassembly.h"
#include "lib/gridtype.h"
#include "lib/problem.h"
#include "lib/puzzle.h"
#include "lib/rotationtables.h"
#include "lib/solvethread.h"

#include <memory>

namespace {

/* pure turns only keep the table builds in these tests short */
const rotationSettings_t SMALL = { { 0.081, 0 }, {} };

std::unique_ptr<puzzle_c> loadPuzzle(const char * path) {
  auto p = puzzle_c::load(path);
  REQUIRE(p != nullptr);
  REQUIRE(p->getNumberOfProblems() > 0);
  return p;
}

/* One cube puzzle and its tables for the whole file: while the tables are
 * alive, every later request for them is served from the cache. */
problem_c & pelikan(void) {
  static auto puzzle = loadPuzzle("examples/PelikanBurr.xmpuzzle");
  return *puzzle->getProblem(0);
}

std::shared_ptr<const rotationGeometry::rotationTables_c> pelikanTables(void) {
  static auto tables = rotationTablesFor(pelikan(), SMALL);
  return tables;
}

}

TEST_CASE("rotation tables are shared per settings and freed with their last user", "[rotations]") {

  /* settings of its own, so tables other tests keep alive do not interfere */
  const rotationSettings_t settings = { { 0.07, 0 }, {} };

  auto a = sharedRotationTables(settings, 3);
  REQUIRE(a != nullptr);
  CHECK(a->getRadius() == 3);
  CHECK(a->size() == 1);

  /* same settings, smaller radius: the cached table serves it */
  auto b = sharedRotationTables(settings, 2);
  CHECK(b == a);

  /* larger radius or other settings: separate tables */
  auto c = sharedRotationTables(settings, 4);
  CHECK(c != a);
  CHECK(c->getRadius() == 4);
  auto d = sharedRotationTables({ { 0.05, 0 }, {} }, 3);
  CHECK(d != a);

  /* the cache does not keep tables alive: no singleton lifetime */
  std::weak_ptr<const rotationGeometry::rotationTables_c> watch = a;
  a.reset();
  b.reset();
  CHECK(watch.expired());
}

TEST_CASE("rotation tables exist only for cube grids", "[rotations]") {

  auto tables = pelikanTables();
  REQUIRE(tables != nullptr);
  CHECK(tables->getRadius() >= rotationRadiusFor(pelikan()));
  CHECK(rotationTablesFor(pelikan(), SMALL) == tables);

  auto hex = loadPuzzle("examples/HexSticks.xmpuzzle");
  REQUIRE(hex->getGridType()->getType() != gridType_c::GT_BRICKS);
  CHECK(rotationTablesFor(*hex->getProblem(0), SMALL) == nullptr);
}

TEST_CASE("rotation tables reach the disassemblers only when requested", "[rotations]") {

  problem_c & problem = pelikan();
  auto tables = pelikanTables();
  REQUIRE(tables != nullptr);

  SECTION("single disassembler") {
    CHECK_FALSE(disassembler_0_c(problem).rotationMoves());
    CHECK(disassembler_0_c(problem, tables).rotationMoves());
  }

  SECTION("disassembler pool") {
    auto ignore = [](uint64_t, std::unique_ptr<assembly_c>, std::unique_ptr<separation_c>) {};
    for (unsigned int threads : { 1u, 2u }) {
      disassemblerPool_c off(problem, threads, ignore);
      CHECK(off.getRotationTables() == nullptr);
      disassemblerPool_c on(problem, threads, ignore, tables);
      CHECK(on.getRotationTables() == tables.get());
    }
  }

  SECTION("solver") {
    /* while `tables` is alive the solver reuses it instead of building */
    solveThread_c plain(problem, solveThread_c::PAR_DISASSM, 2, SMALL);
    CHECK_FALSE(plain.rotationMoves());

    solveThread_c rotating(problem, solveThread_c::PAR_DISASSM | solveThread_c::PAR_ROTATION_MOVES, 2, SMALL);
    CHECK(rotating.rotationMoves());

    /* rotation moves are part of the disassembly: nothing without it */
    solveThread_c noDisassembly(problem, solveThread_c::PAR_ROTATION_MOVES, 2, SMALL);
    CHECK_FALSE(noDisassembly.rotationMoves());
  }
}
