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
#include "lib/stl.h"
#include "test_helpers.h"

#include <future>
#include <memory>

namespace {

/* pure turns only keep the table builds in these tests short */
const rotationSettings_t SMALL = { { 0.081, 0 }, {}, {} };

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
  const rotationSettings_t settings = { { 0.07, 0 }, {}, {} };
  releaseRotationTables();

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
  auto d = sharedRotationTables({ { 0.05, 0 }, {}, {} }, 3);
  CHECK(d != a);

  /* c serves everything a did, so a is not kept */
  std::weak_ptr<const rotationGeometry::rotationTables_c> watch = a;
  a.reset();
  b.reset();
  CHECK(watch.expired());

  /* the two most recent sets (d, c) outlive their users until released */
  std::weak_ptr<const rotationGeometry::rotationTables_c> watchC = c, watchD = d;
  c.reset();
  d.reset();
  CHECK_FALSE(watchC.expired());
  CHECK_FALSE(watchD.expired());
  releaseRotationTables();
  CHECK(watchC.expired());
  CHECK(watchD.expired());
}

TEST_CASE("the most recently used rotation tables are kept for the next solve", "[rotations]") {

  const rotationSettings_t first = { { 0.071, 0 }, {}, {} }, second = { { 0.072, 0 }, {}, {} }, third = { { 0.073, 0 }, {}, {} };
  releaseRotationTables();

  /* one design after another: the second request is served by the kept set */
  std::weak_ptr<const rotationGeometry::rotationTables_c> watch = sharedRotationTables(first, 2);
  REQUIRE_FALSE(watch.expired());
  CHECK(sharedRotationTables(first, 2) == watch.lock());

  /* a third set of settings pushes out the least recently used one */
  std::weak_ptr<const rotationGeometry::rotationTables_c> watchSecond = sharedRotationTables(second, 2);
  CHECK(sharedRotationTables(first, 2) == watch.lock());   // first is now the most recent
  sharedRotationTables(third, 2);
  CHECK_FALSE(watch.expired());
  CHECK(watchSecond.expired());

  releaseRotationTables();
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

TEST_CASE("a problem's stored model wins over the settings' default", "[rotations]") {

  auto puzzle = loadPuzzle("examples/PelikanBurr.xmpuzzle");
  problem_c & problem = *puzzle->getProblem(0);

  auto plain = rotationContextFor(problem, SMALL);
  REQUIRE(plain != nullptr);
  CHECK(plain->getSettings().model.play == SMALL.model.play);
  CHECK(plain->getSettings().model.chamfer == SMALL.model.chamfer);

  problem.setRotationModel({ 0.06, 0.04 });
  auto stored = rotationContextFor(problem, SMALL);
  REQUIRE(stored != nullptr);
  CHECK(stored->getSettings().model.play == 0.06);
  CHECK(stored->getSettings().model.chamfer == 0.04);
  /* the rest of the settings are the solver's */
  CHECK(stored->getSettings().amounts == SMALL.amounts);
  CHECK(rotationSettingsFor(problem, SMALL).model.chamfer == 0.04);
}

TEST_CASE("the shared pair cache keeps the first set stored for a key", "[rotations]") {

  rotationContext_c context(pelikan(), rotationSettings_t{}, std::async(std::launch::deferred, [] { return pelikanTables(); }).share());
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
    CHECK_FALSE(disassembler_0_c(problem, bttest::settings()).rotationMoves());
    CHECK(disassembler_0_c(problem, bttest::settings(), context).rotationMoves());
  }

  SECTION("disassembler pool") {
    auto ignore = [](uint64_t, std::unique_ptr<assembly_c>, std::unique_ptr<separation_c>) {};
    for (unsigned int threads : { 1u, 2u }) {
      disassemblerPool_c off(problem, bttest::withThreads(threads), ignore);
      CHECK(off.getRotationContext() == nullptr);
      disassemblerPool_c on(problem, bttest::withThreads(threads), ignore, context);
      CHECK(on.getRotationContext() == context.get());
    }
  }

  SECTION("solver") {
    /* while the tables are alive the solver reuses them instead of building */
    solverSettings_t settings;
    settings.threads = 2;
    settings.rotation = SMALL;
    solveThread_c plain(problem, solveThread_c::PAR_DISASSM, settings);
    CHECK_FALSE(plain.rotationMoves());

    solveThread_c rotating(problem, solveThread_c::PAR_DISASSM | solveThread_c::PAR_ROTATION_MOVES, settings);
    CHECK(rotating.rotationMoves());

    /* rotation moves are part of the disassembly: nothing without it */
    solveThread_c noDisassembly(problem, solveThread_c::PAR_ROTATION_MOVES, settings);
    CHECK_FALSE(noDisassembly.rotationMoves());
  }
}

TEST_CASE("a shape's pieces are checked for the model of the problems using it", "[rotations][stl]") {

  puzzle_c puzzle(std::make_unique<gridType_c>(gridType_c::GT_BRICKS));
  const unsigned int piece = puzzle.addShape(1, 1, 1);
  const unsigned int other = puzzle.addShape(2, 1, 1);
  problem_c & a = *puzzle.getProblem(puzzle.addProblem());
  problem_c & b = *puzzle.getProblem(puzzle.addProblem());
  a.setShapeMaximum(piece, 1);
  b.setShapeMaximum(piece, 2);

  const rotationGeometry::model_t def = rotationSettings_t{}.model;
  CHECK(rotationModelOf(a).play == def.play);

  /* both problems at the default, and a shape no problem uses */
  REQUIRE(rotationModelForShape(puzzle, piece));
  CHECK(rotationModelForShape(puzzle, piece)->chamfer == def.chamfer);
  REQUIRE(rotationModelForShape(puzzle, other));
  CHECK(rotationModelForShape(puzzle, other)->play == def.play);

  /* the views draw stored models only */
  CHECK_FALSE(storedRotationModelForShape(puzzle, piece));
  CHECK_FALSE(storedRotationModelForShape(puzzle, other));

  a.setRotationModel({ 0.0126, 0.05 });
  CHECK(rotationModelOf(a).chamfer == 0.05);
  CHECK_FALSE(rotationModelForShape(puzzle, piece));   // the problems disagree
  CHECK_FALSE(storedRotationModelForShape(puzzle, piece));   // b stores none
  b.setRotationModel({ 0.0126, 0.05 });
  REQUIRE(rotationModelForShape(puzzle, piece));
  CHECK(rotationModelForShape(puzzle, piece)->play == 0.0126);
  REQUIRE(storedRotationModelForShape(puzzle, piece));
  CHECK(storedRotationModelForShape(puzzle, piece)->chamfer == 0.05);

  /* the export dialog's note */
  std::unique_ptr<stlExporter_c> stl(puzzle.getGridType()->getStlExporter());
  stl->followModel({ 0.0126, 0.05 });
  CHECK(stlModelNote(*stl, rotationModelForShape(puzzle, piece)).find("the pieces rotation moves are checked for")
        != std::string::npos);
  stl->setParameter(4, 1.0);
  CHECK(stlModelNote(*stl, rotationModelForShape(puzzle, piece)).find("but rotation moves are checked for play 0.0126")
        != std::string::npos);
  CHECK(stlModelNote(*stl, std::nullopt).find("different play and bevel") != std::string::npos);

  gridType_c rhombic(gridType_c::GT_RHOMBIC);
  std::unique_ptr<stlExporter_c> other_grid(rhombic.getStlExporter());
  CHECK(stlModelNote(*other_grid, def).empty());
}
