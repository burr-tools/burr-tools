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

#include <future>
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

std::shared_ptr<rotationContext_c> pelikanContext(void) {
  static auto context = rotationContextFor(pelikan(), SMALL);
  return context;
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

TEST_CASE("a rotation context exists only for cube grids and shares the tables", "[rotations]") {

  auto context = pelikanContext();
  REQUIRE(context != nullptr);
  CHECK(&context->tables() == pelikanTables().get());

  /* built in the background: the same tables once they are there */
  auto background = rotationContextFor(pelikan(), SMALL, true);
  REQUIRE(background != nullptr);
  CHECK(&background->tables() == pelikanTables().get());

  auto hex = loadPuzzle("examples/HexSticks.xmpuzzle");
  CHECK(rotationContextFor(*hex->getProblem(0), SMALL) == nullptr);
}

TEST_CASE("the shared pair cache keeps the first set stored for a key", "[rotations]") {

  rotationContext_c context(pelikan(), defaultRotationSettings(), std::async(std::launch::deferred, [] { return pelikanTables(); }).share());
  const rotationPairKey_t key = { 1, 2 }, other = { 1, 3 };

  const rotationPivotSet_t * found = nullptr;
  CHECK_FALSE(context.lookup(key, &found));

  auto first = std::make_unique<rotationPivotSet_t>();
  const rotationPivotSet_t * firstRaw = first.get();
  CHECK(context.store(key, first) == firstRaw);
  CHECK(first == nullptr);   // taken over by the cache

  /* a second worker that computed the same pair gets the first set back */
  auto second = std::make_unique<rotationPivotSet_t>();
  CHECK(context.store(key, second) == firstRaw);
  CHECK(context.lookup(key, &found));
  CHECK(found == firstRaw);

  /* "never blocks" is cached too */
  std::unique_ptr<rotationPivotSet_t> none;
  CHECK(context.store(other, none) == nullptr);
  found = firstRaw;
  CHECK(context.lookup(other, &found));
  CHECK(found == nullptr);

  CHECK(context.getSkipped() == 0);
  context.noteSkipped(rotationContext_c::SKIP_RADIUS);
  CHECK(context.getSkipped() == rotationContext_c::SKIP_RADIUS);
}

TEST_CASE("rotation contexts reach the disassemblers only when requested", "[rotations]") {

  problem_c & problem = pelikan();
  auto context = pelikanContext();
  REQUIRE(context != nullptr);

  SECTION("single disassembler") {
    CHECK_FALSE(disassembler_0_c(problem).rotationMoves());
    CHECK(disassembler_0_c(problem, context).rotationMoves());
  }

  SECTION("disassembler pool") {
    auto ignore = [](uint64_t, std::unique_ptr<assembly_c>, std::unique_ptr<separation_c>) {};
    for (unsigned int threads : { 1u, 2u }) {
      disassemblerPool_c off(problem, threads, ignore);
      CHECK(off.getRotationContext() == nullptr);
      disassemblerPool_c on(problem, threads, ignore, context);
      CHECK(on.getRotationContext() == context.get());
    }
  }

  SECTION("solver") {
    /* while the tables are alive the solver reuses them instead of building */
    solveThread_c plain(problem, solveThread_c::PAR_DISASSM, 2, SMALL);
    CHECK_FALSE(plain.rotationMoves());

    solveThread_c rotating(problem, solveThread_c::PAR_DISASSM | solveThread_c::PAR_ROTATION_MOVES, 2, SMALL);
    CHECK(rotating.rotationMoves());

    /* rotation moves are part of the disassembly: nothing without it */
    solveThread_c noDisassembly(problem, solveThread_c::PAR_ROTATION_MOVES, 2, SMALL);
    CHECK_FALSE(noDisassembly.rotationMoves());
  }
}
