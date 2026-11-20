/* Tests of the Dancing Cells exact cover engine (CellsCover, selected with
 * solverSettings_t::bitParallel off, and wherever the bit-parallel engine
 * declines a matrix) and of the assembler_1 range symmetry breaker
 * (solverSettings_t::rangeSymmetryBreaker).
 *
 * The engine is compared with the default engine on real puzzles (which
 * must find the same assemblies, as multisets), through stops, donations,
 * saved state and restore, and is driven directly on small hand-built
 * matrices.
 */
#include <catch2/catch_test_macros.hpp>

#include "lib/assembler.h"
#include "lib/assembler_1.h"
#include "lib/assembly.h"
#include "lib/cells_cover.h"
#include "lib/gridtype.h"
#include "lib/problem.h"
#include "lib/puzzle.h"
#include "lib/voxel.h"
#include "tools/xml.h"
#include "test_helpers.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <set>
#include <sstream>
#include <stop_token>
#include <string>
#include <vector>

namespace {

using bttest::ScopedSettings;

/* a canonical fingerprint of every assembly, so runs compare as multisets */
class Recorder : public assembler_cb {
public:
  std::multiset<std::string> fingerprints;

  bool assembly(std::unique_ptr<assembly_c> a) override {
    std::string s;
    for (unsigned int i = 0; i < a->placementCount(); i++) {
      s += std::to_string(i);
      if (a->isPlaced(i))
        s += ":" + std::to_string(a->getX(i)) + "," + std::to_string(a->getY(i)) +
             "," + std::to_string(a->getZ(i)) + "," +
             std::to_string(static_cast<unsigned int>(a->getTransformation(i))) + ";";
      else
        s += ":-;";
    }
    fingerprints.insert(std::move(s));
    return true;
  }
};

using Set = std::multiset<std::string>;

std::string versionOf(const std::string & xml) {
  size_t tag = xml.find("<assembler");
  REQUIRE(tag != std::string::npos);
  size_t a = xml.find("version=\"", tag);
  REQUIRE(a != std::string::npos);
  a += 9;
  size_t b = xml.find('"', a);
  REQUIRE(b != std::string::npos);
  return xml.substr(a, b - a);
}

std::string contentOf(const std::string & xml) {
  size_t a = xml.find("<assembler");
  REQUIRE(a != std::string::npos);
  a = xml.find('>', a);
  REQUIRE(a != std::string::npos);
  a++;
  size_t b = xml.find("</assembler>", a);
  REQUIRE(b != std::string::npos);
  return xml.substr(a, b - a);
}

std::string saveState(const assembler_1_c & assm) {
  std::ostringstream str;
  xmlWriter_c xml(str);
  assm.save(xml);
  return str.str();
}

/* The remainder type of saved state: 0 none, 2 bit-parallel seeds, 3
 * Dancing Cells seeds. A stopped search saves state 1 and then the type,
 * so it tells which engine a stopped run used.
 */
unsigned int remainderType(const std::string & state) {
  std::istringstream in(contentOf(state));
  unsigned int st = 0, type = 0;
  REQUIRE(bool(in >> st));
  if (st != 1)
    return 0;
  REQUIRE(bool(in >> type));
  return type;
}

std::unique_ptr<puzzle_c> loadPuzzle(const char * path) {
  auto p = puzzle_c::load(path);
  REQUIRE(p != nullptr);
  REQUIRE(p->getNumberOfProblems() > 0);
  return p;
}

/* all assemblies, with the settings in scope */
Set solveAll(problem_c & problem, unsigned int threads) {
  Recorder cb;
  assembler_1_c assm(makeSolverSpec(problem), bttest::settings());
  assm.setNumThreads(threads);
  REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
  assm.assemble(&cb);
  return std::move(cb.fingerprints);
}

/* A run that stops after k assemblies; returns the saved state. */
std::string stopAfter(problem_c & problem, unsigned int threads, unsigned int k, Recorder & cb) {
  assembler_1_c assm(makeSolverSpec(problem), bttest::settings());
  assm.setNumThreads(threads);
  REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
  unsigned int seen = 0;
  assm.assemble([&](std::unique_ptr<assembly_c> a) -> bool {
    cb.assembly(std::move(a));
    return ++seen < k;
  });
  REQUIRE(seen == k);
  return saveState(assm);
}

/* restore a saved state into a fresh assembler (settings in scope) and
 * run it to the end */
Set resumeAll(problem_c & problem, unsigned int threads, const std::string & state) {
  assembler_1_c restored(makeSolverSpec(problem), bttest::settings());
  restored.setNumThreads(threads);
  REQUIRE(restored.createMatrix(false, false) == assembler_c::ERR_NONE);
  const std::string payload = contentOf(state);
  REQUIRE(restored.setPosition(payload.c_str(), versionOf(state).c_str()) == assembler_c::ERR_NONE);
  Recorder cb;
  restored.assemble(&cb);
  return std::move(cb.fingerprints);
}

Set unite(Set a, const Set & b) {
  a.insert(b.begin(), b.end());
  return a;
}

struct Puzzle {
  const char * path;
  const char * what;
  size_t assemblies;
};

/* the assembler_1 puzzles: duplicate shapes, piece ranges, variable voxels
 * with fixed counts (the one case the cells engine takes that is not
 * range-free), all small */
const Puzzle kPuzzles[] = {
  {"examples/CubeInCage.xmpuzzle",       "duplicate shapes", 96},
  {"examples/PiecesOfEight.xmpuzzle",    "piece ranges", 6},
  {"examples/DemoMirrorParadox.xmpuzzle","ranges and duplicates", 1},
  {"examples/PelikanBurr.xmpuzzle",      "variable voxels, fixed counts", 12},
};

} // namespace

TEST_CASE("Dancing Cells finds the same assemblies as the default engine",
          "[cells][assembler][simd][equivalence]") {
  for (const auto & pz : kPuzzles) {
    for (unsigned int threads : {1u, 4u}) {
      INFO(pz.path << " (" << pz.what << "), " << threads << " threads");
      auto p = loadPuzzle(pz.path);
      problem_c & problem = *p->getProblem(0);

      Set baseline;
      {
        ScopedSettings env;
        env->bitParallel = true;
        baseline = solveAll(problem, threads);
      }
      REQUIRE(!baseline.empty());

      Set cells;
      {
        ScopedSettings env;
        env->bitParallel = false;
        cells = solveAll(problem, threads);
      }
      CHECK(cells.size() == baseline.size());
      CHECK(cells == baseline);

      /* the counts dancing links found when it was still here (removed
       * 2026-11-16), so the baseline itself is not the odd one out */
      CHECK(baseline.size() == pz.assemblies);
    }
  }
}

/* There is no accessor naming the engine of a run. What a stopped run
 * leaves is observable: save() writes the remainder as type 3 for this
 * engine's seeds and type 2 for the bit-parallel engine's.
 */
TEST_CASE("the cells setting selects the Dancing Cells engine",
          "[cells][assembler][save]") {
  for (const auto & pz : kPuzzles) {
    /* a stop after one assembly needs a second one to leave a remainder */
    if (std::string(pz.path).find("DemoMirror") != std::string::npos)
      continue;
    for (unsigned int threads : {1u, 4u}) {
      INFO(pz.path << " (" << pz.what << "), " << threads << " threads");
      auto p = loadPuzzle(pz.path);
      problem_c & problem = *p->getProblem(0);

      /* the variable-voxel puzzle has fixed counts, so it takes cells too;
       * a stop after one assembly needs at least two assemblies */
      for (bool cells : {false, true}) {
        ScopedSettings env;
        env->bitParallel = !cells;
        Recorder cb;
        const std::string state = stopAfter(problem, threads, 1, cb);
        CHECK(remainderType(state) == (cells ? 3u : 2u));
        CHECK(versionOf(state) == "2.2");
      }
    }
  }
}

TEST_CASE("Dancing Cells donates and replays seeds exactly once",
          "[cells][assembler][parallel][resume]") {
  const char * paths[] = {
    "examples/CubeInCage.xmpuzzle",
    "examples/PiecesOfEight.xmpuzzle",
  };
  for (const char * path : paths) {
    auto p = loadPuzzle(path);
    problem_c & problem = *p->getProblem(0);

    Set baseline;
    {
      ScopedSettings env;
      env->bitParallel = true;
      baseline = solveAll(problem, 4);
    }
    REQUIRE(baseline.size() > 3);

    for (unsigned long every : {1ul, 3ul, 7ul}) {
      for (unsigned int threads : {1u, 4u}) {
        INFO(path << ", every " << every << ", " << threads << " threads");
        ScopedSettings env;
        env->bitParallel = false;
        env->donateEvery = every;

        Recorder cb;
        assembler_1_c assm(makeSolverSpec(problem), bttest::settings());
        assm.setNumThreads(threads);
        REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
        assm.assemble(&cb);
        CHECK(assm.getDonations() > 0);
        CHECK(cb.fingerprints == baseline);
      }
    }
  }
}

TEST_CASE("Dancing Cells stop and continue delivers every assembly exactly once",
          "[cells][assembler][parallel][resume]") {
  struct Case { unsigned int threads; unsigned long every; };
  const Case cases[] = {{1, 0}, {4, 0}, {1, 3}, {4, 3}};
  const char * paths[] = {"examples/CubeInCage.xmpuzzle", "examples/PiecesOfEight.xmpuzzle"};

  for (const char * path : paths) {
    auto p = loadPuzzle(path);
    problem_c & problem = *p->getProblem(0);

    Set baseline;
    {
      ScopedSettings env;
      env->bitParallel = true;
      baseline = solveAll(problem, 4);
    }
    REQUIRE(baseline.size() > 3);
    const unsigned int n = static_cast<unsigned int>(baseline.size());

    for (const auto & c : cases) {
      for (unsigned int k : {1u, 2u, n / 2, n - 1}) {
        INFO(path << ", " << c.threads << " threads, every " << c.every << ", stop after " << k);
        ScopedSettings env;
        env->bitParallel = false;
        env->donateEvery = c.every;

        assembler_1_c assm(makeSolverSpec(problem), bttest::settings());
        assm.setNumThreads(c.threads);
        REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);

        Recorder cb1, cb2;
        unsigned int seen = 0;
        assm.assemble([&](std::unique_ptr<assembly_c> a) -> bool {
          cb1.assembly(std::move(a));
          return ++seen < k;
        });
        REQUIRE(seen == k);
        assm.assemble(&cb2);

        CHECK(unite(cb1.fingerprints, cb2.fingerprints) == baseline);
        CHECK(cb1.fingerprints.size() + cb2.fingerprints.size() == baseline.size());
      }
    }
  }
}

TEST_CASE("Dancing Cells state saved, restored and continued is exact-once",
          "[cells][assembler][parallel][resume][save]") {
  const char * paths[] = {"examples/CubeInCage.xmpuzzle", "examples/PiecesOfEight.xmpuzzle"};
  struct Case { unsigned int threads; unsigned long every; };
  const Case cases[] = {{1, 0}, {4, 0}, {4, 3}};

  for (const char * path : paths) {
    auto p = loadPuzzle(path);
    problem_c & problem = *p->getProblem(0);

    Set baseline;
    {
      ScopedSettings env;
      env->bitParallel = true;
      baseline = solveAll(problem, 4);
    }
    REQUIRE(baseline.size() > 3);
    const unsigned int n = static_cast<unsigned int>(baseline.size());

    for (const auto & c : cases) {
      for (unsigned int k : {1u, n / 2}) {
        for (bool cellsOnRestore : {true, false}) {
          INFO(path << ", " << c.threads << " threads, every " << c.every << ", stop after " << k
               << ", cells on restore " << cellsOnRestore);
          Recorder cb1;
          std::string state;
          {
            ScopedSettings env;
            env->bitParallel = false;
            env->donateEvery = c.every;
            state = stopAfter(problem, c.threads, k, cb1);
          }
          REQUIRE(remainderType(state) == 3u);

          ScopedSettings env;
          env->bitParallel = !cellsOnRestore;
          env->donateEvery = c.every;

          /* the remainder keeps its engine: stop the continued run again
           * and the new remainder is still the cells engine's, then finish
           * it under the other setting */
          {
            assembler_1_c restored(makeSolverSpec(problem), bttest::settings());
            restored.setNumThreads(c.threads);
            REQUIRE(restored.createMatrix(false, false) == assembler_c::ERR_NONE);
            const std::string payload = contentOf(state);
            REQUIRE(restored.setPosition(payload.c_str(), versionOf(state).c_str())
                    == assembler_c::ERR_NONE);
            Recorder cb2;
            unsigned int seen = 0;
            restored.assemble([&](std::unique_ptr<assembly_c> a) -> bool {
              cb2.assembly(std::move(a));
              return ++seen < 1;
            });
            REQUIRE(seen == 1);
            const std::string state2 = saveState(restored);
            CHECK(remainderType(state2) == 3u);

            ScopedSettings flip;
            flip->bitParallel = cellsOnRestore;
            const Set rest = resumeAll(problem, c.threads, state2);
            const Set all = unite(unite(cb1.fingerprints, cb2.fingerprints), rest);
            CHECK(all == baseline);
            CHECK(cb1.fingerprints.size() + cb2.fingerprints.size() + rest.size() == baseline.size());
          }

          /* and a plain restore and finish */
          const Set rest = resumeAll(problem, c.threads, state);
          CHECK(unite(cb1.fingerprints, rest) == baseline);
          CHECK(cb1.fingerprints.size() + rest.size() == baseline.size());
        }
      }
    }
  }
}

namespace {

/* the tokens of a payload, so a test can edit one */
std::vector<std::string> tokens(const std::string & payload) {
  std::istringstream in(payload);
  std::vector<std::string> t;
  std::string s;
  while (in >> s)
    t.push_back(s);
  return t;
}

std::string join(const std::vector<std::string> & t) {
  std::string s;
  for (const auto & x : t)
    s += x + " ";
  return s;
}

/* index of the first step kind of the first seed of a type 2/3 payload:
 * state, type, count, then prefix (n, n ids) and the kinds (n, n values);
 * -1 when that prefix is empty */
int firstKindIndex(const std::vector<std::string> & t) {
  const size_t countAt = 2;   // state, type, then the seed count
  const size_t n = std::stoul(t.at(countAt + 1));
  if (n == 0)
    return -1;
  const size_t kindsAt = countAt + 2 + n;
  REQUIRE(std::stoul(t.at(kindsAt)) == n);
  return static_cast<int>(kindsAt + 1);
}

} // namespace

TEST_CASE("restore validates the step kinds of Dancing Cells and bit-parallel remainders",
          "[cells][assembler][resume][save]") {
  /* the native grid: the tampered steps below are valid or not for this
   * puzzle's matrix, which NG's rhombic grid (the default read) changes */
  auto p = puzzle_c::load("examples/PiecesOfEight.xmpuzzle", cutCubeGrids_e::native);
  problem_c & problem = *p->getProblem(0);

  auto restoreCode = [&](const std::string & payload, const std::string & version) {
    assembler_1_c assm(makeSolverSpec(problem), bttest::settings());
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    return assm.setPosition(payload.c_str(), version.c_str());
  };

  for (bool cells : {true, false}) {
    INFO(std::string(cells ? "type 3" : "type 2"));
    ScopedSettings env;
    env->bitParallel = !cells;
    Recorder cb;
    const std::string state = stopAfter(problem, 1, 1, cb);
    REQUIRE(remainderType(state) == (cells ? 3u : 2u));
    const std::string version = versionOf(state);
    std::vector<std::string> t = tokens(contentOf(state));

    /* untouched, it restores */
    CHECK(restoreCode(join(t), version) == assembler_c::ERR_NONE);

    const int k = firstKindIndex(t);
    REQUIRE(k > 0);

    /* a kind no engine has */
    t[k] = "3";
    CHECK(restoreCode(join(t), version) == assembler_c::ERR_CAN_NOT_RESTORE_SYNTAX);

    /* kind 2 (selected, other branch open) exists only for cells */
    t[k] = "2";
    CHECK(restoreCode(join(t), version) ==
          (cells ? assembler_c::ERR_NONE : assembler_c::ERR_CAN_NOT_RESTORE_SYNTAX));
  }

  /* a type 3 remainder naming a row the matrix does not have */
  {
    ScopedSettings env;
    env->bitParallel = false;
    Recorder cb;
    const std::string state = stopAfter(problem, 1, 1, cb);
    std::vector<std::string> t = tokens(contentOf(state));
    REQUIRE(firstKindIndex(t) > 0);
    t[2 + 2] = "999999";   // first prefix node
    CHECK(restoreCode(join(t), versionOf(state)) == assembler_c::ERR_CAN_NOT_RESTORE_SYNTAX);
  }
}

/* ---- the engine on hand-built matrices ---------------------------------- */

namespace {

using Rows = std::vector<std::vector<unsigned int>>;

/* Every column is a voxel column (exactly once) unless said otherwise;
 * node ids are 100 + the row's index. */
std::unique_ptr<CellsCover> makeCover(unsigned int cols, const Rows & rows,
                                      const std::vector<unsigned int> & optional = {},
                                      unsigned int holes = 0) {
  auto cover = std::make_unique<CellsCover>(cols, 1, bttest::settings());
  for (unsigned int c = 1; c <= cols; c++) {
    const bool opt = std::find(optional.begin(), optional.end(), c) != optional.end();
    cover->setColumnBounds(c, opt ? 0 : 1, 1, true, false, false, opt);
  }
  cover->setHoles(holes);
  for (size_t i = 0; i < rows.size(); i++)
    cover->addRow(100 + static_cast<unsigned int>(i), 0, 0, static_cast<unsigned int>(i), 0, rows[i],
                  std::vector<unsigned int>(rows[i].size(), 1));
  return cover;
}

using Sols = std::set<std::vector<unsigned int>>;

/* solutions as sorted node id lists; also the number of reports */
Sols solveAllRows(const ISimdHuangCover & cover, size_t * reports = nullptr) {
  Sols out;
  size_t n = 0;
  std::atomic<uint64_t> iterations{0};
  cover.solve([&](const std::vector<unsigned int> & s) {
    std::vector<unsigned int> v = s;
    std::sort(v.begin(), v.end());
    out.insert(v);
    n++;
    return true;
  }, std::stop_token{}, iterations);
  if (reports)
    *reports = n;
  return out;
}

/* tilings of an h x w board by dominoes: one column per cell */
Rows dominoRows(unsigned int h, unsigned int w) {
  Rows rows;
  for (unsigned int y = 0; y < h; y++)
    for (unsigned int x = 0; x < w; x++) {
      const unsigned int c = y * w + x + 1;
      if (x + 1 < w)
        rows.push_back({c, c + 1});
      if (y + 1 < h)
        rows.push_back({c, c + w});
    }
  return rows;
}

HuangSeed makeSeed(std::vector<unsigned int> nodes, std::vector<uint8_t> kinds) {
  HuangSeed s;
  s.prefix = std::move(nodes);
  s.pivotShape = std::move(kinds);
  return s;
}

} // namespace

TEST_CASE("CellsCover solves plain exact cover problems", "[cells][cover]") {
  SECTION("Knuth's seven-column example has exactly one solution") {
    // A B C D E F G = 1..7
    const Rows rows = {{3, 5, 6}, {1, 4, 7}, {2, 3, 6}, {1, 4}, {2, 7}, {4, 5, 7}};
    auto cover = makeCover(7, rows);
    size_t reports = 0;
    const Sols s = solveAllRows(*cover, &reports);
    CHECK(reports == 1);
    REQUIRE(s.size() == 1);
    // rows 3, 0 and 4: {1,4} {3,5,6} {2,7}
    CHECK(*s.begin() == std::vector<unsigned int>{100, 103, 104});
  }
  SECTION("domino tilings") {
    const struct { unsigned int h, w, count; } boards[] = {
      {2, 2, 2}, {2, 3, 3}, {3, 3, 0}, {2, 4, 5}, {4, 4, 36}, {3, 4, 11},
    };
    for (const auto & b : boards) {
      INFO(b.h << "x" << b.w);
      auto cover = makeCover(b.h * b.w, dominoRows(b.h, b.w));
      size_t reports = 0;
      const Sols s = solveAllRows(*cover, &reports);
      CHECK(reports == b.count);
      CHECK(s.size() == b.count);
    }
  }
  SECTION("a search can be repeated on the same engine") {
    auto cover = makeCover(6, dominoRows(2, 3));
    CHECK(solveAllRows(*cover).size() == 3);
    CHECK(solveAllRows(*cover).size() == 3);
  }
  SECTION("a stopping callback ends the search") {
    auto cover = makeCover(16, dominoRows(4, 4));
    size_t n = 0;
    std::atomic<uint64_t> iterations{0};
    cover->solve([&](const std::vector<unsigned int> &) { return ++n < 5; },
                 std::stop_token{}, iterations);
    CHECK(n == 5);
    CHECK(iterations.load() > 0);
  }
  SECTION("a stop request is honoured") {
    auto cover = makeCover(16, dominoRows(4, 4));
    std::stop_source src;
    src.request_stop();
    size_t n = 0;
    std::atomic<uint64_t> iterations{0};
    cover->solve([&](const std::vector<unsigned int> &) { n++; return true; },
                 src.get_token(), iterations);
    CHECK(n == 0);
  }
}

TEST_CASE("CellsCover reports a set of identical pieces once, not once per order",
          "[cells][cover]") {
  /* Two copies of one piece (column 1, used 0..2 times) and three of
   * another (column 2, 0..3 times), each covering one voxel: every voxel
   * (columns 3..5) is given to one of the two. All 2^3 assignments but
   * the one that gives all three to the two-copy piece are solutions.
   */
  CellsCover cover(5, 2, bttest::settings());
  cover.setColumnBounds(1, 0, 2, false, true, false, false);
  cover.setColumnBounds(2, 0, 3, false, true, false, false);
  for (unsigned int c = 3; c <= 5; c++)
    cover.setColumnBounds(c, 1, 1, true, false, false, false);
  unsigned int id = 100;
  for (unsigned int shape = 0; shape < 2; shape++)
    for (unsigned int v = 3; v <= 5; v++)
      cover.addRow(id++, shape, shape + 1, 0, 0, {shape + 1, v}, {1, 1});

  size_t reports = 0;
  const Sols s = solveAllRows(cover, &reports);
  CHECK(reports == 7);
  CHECK(s.size() == 7);
  for (const auto & sol : s) {
    CHECK(sol.size() == 3);
    size_t first = std::count_if(sol.begin(), sol.end(), [](unsigned int n) { return n < 103; });
    CHECK(first <= 2);
  }
}

TEST_CASE("CellsCover limits the unfilled optional columns with setHoles",
          "[cells][cover]") {
  /* voxels 1, 2 are required; holes 3, 4 may stay empty. A fills voxel 1
   * and hole 3, B voxel 2 and hole 4, C and D fill the voxels alone. */
  const Rows rows = {{1, 3}, {2, 4}, {1}, {2}};
  const struct { unsigned int holes; size_t count; } cases[] = {{0, 1}, {1, 3}, {2, 4}};
  for (const auto & c : cases) {
    INFO(c.holes << " holes");
    auto cover = makeCover(4, rows, {3, 4}, c.holes);
    size_t reports = 0;
    const Sols s = solveAllRows(*cover, &reports);
    CHECK(reports == c.count);
    CHECK(s.size() == c.count);
  }
}

TEST_CASE("CellsCover never selects a row of a column that takes nothing",
          "[cells][cover]") {
  /* column 1 a voxel, column 2 a hole, column 3 a shape column with a
   * maximum of 0. Row 100 fills the voxel; row 101 fills the hole and
   * would place the shape: it must never be part of a solution. */
  auto make = [](unsigned int shapeMin) {
    auto cover = std::make_unique<CellsCover>(3, 1, bttest::settings());
    cover->setColumnBounds(1, 1, 1, true, false, false, false);
    cover->setColumnBounds(2, 0, 1, true, false, false, true);
    cover->setColumnBounds(3, shapeMin, 0, false, true, false, false);
    cover->setHoles(1);
    cover->addRow(100, 0, 0, 0, 0, {1}, {1});
    cover->addRow(101, 0, 3, 0, 0, {2, 3}, {1, 1});
    return cover;
  };
  CHECK(solveAllRows(*make(0)) == Sols{{100}});
  // a column that wants something and takes nothing: no solution
  CHECK(solveAllRows(*make(1)).empty());
}

TEST_CASE("CellsCover decides whether a seed replays", "[cells][cover]") {
  // two tilings of a 2x2 board: rows 0, 1 = the horizontal pair, rows 2, 3 = the vertical
  const Rows rows = {{1, 2}, {3, 4}, {1, 3}, {2, 4}};
  auto cover = makeCover(4, rows);
  using K = CellsCover;

  CHECK(cover->seedReplays(HuangSeed{}));
  CHECK(cover->seedReplays(makeSeed({100}, {K::STEP_SELECT})));
  CHECK(cover->seedReplays(makeSeed({100}, {K::STEP_SELECT_OPEN})));
  CHECK(cover->seedReplays(makeSeed({100, 101}, {K::STEP_SELECT, K::STEP_SELECT})));
  CHECK(cover->seedReplays(makeSeed({102}, {K::STEP_DISCARD})));
  CHECK(cover->seedReplays(makeSeed({102, 100}, {K::STEP_DISCARD, K::STEP_SELECT})));
  /* a seed that leaves a column without any row is dead, not invalid */
  CHECK(cover->seedReplays(makeSeed({100, 101}, {K::STEP_DISCARD, K::STEP_DISCARD})));

  /* a row the matrix does not have */
  CHECK_FALSE(cover->seedReplays(makeSeed({999}, {K::STEP_SELECT})));
  CHECK_FALSE(cover->seedReplays(makeSeed({100, 999}, {K::STEP_SELECT, K::STEP_SELECT})));
  /* two rows that conflict (both cover column 1) */
  CHECK_FALSE(cover->seedReplays(makeSeed({100, 102}, {K::STEP_SELECT, K::STEP_SELECT})));
  /* the same row twice */
  CHECK_FALSE(cover->seedReplays(makeSeed({100, 100}, {K::STEP_SELECT, K::STEP_SELECT})));
  /* a kind that does not exist, and kinds missing */
  CHECK_FALSE(cover->seedReplays(makeSeed({100}, {3})));
  CHECK_FALSE(cover->seedReplays(makeSeed({100}, {})));
}

TEST_CASE("CellsCover::solveSubtree searches only what its prefix and hidden rows leave",
          "[cells][cover]") {
  /* 4x4 dominoes: 36 tilings. The tilings with a given row are those the
   * unrestricted search reports that contain it. */
  auto cover = makeCover(16, dominoRows(4, 4));
  const Sols all = solveAllRows(*cover);
  REQUIRE(all.size() == 36);

  auto subtree = [&](std::vector<unsigned int> prefix, std::vector<unsigned int> hidden) {
    Sols out;
    std::atomic<uint64_t> iterations{0};
    size_t n = 0;
    cover->solveSubtree(prefix, hidden, [&](const std::vector<unsigned int> & s) {
      std::vector<unsigned int> v = s;
      std::sort(v.begin(), v.end());
      out.insert(v);
      n++;
      return true;
    }, std::stop_token{}, iterations);
    CHECK(n == out.size());
    return out;
  };
  auto has = [](const std::vector<unsigned int> & sol, unsigned int node) {
    return std::find(sol.begin(), sol.end(), node) != sol.end();
  };

  /* row 0 = {1,2}, row 1 = {1,5}; row 2 = {2,3} */
  const Sols withRow0 = subtree({100}, {});
  Sols expected;
  for (const auto & s : all)
    if (has(s, 100))
      expected.insert(s);
  REQUIRE(!expected.empty());
  CHECK(withRow0 == expected);

  const Sols hiddenRow1 = subtree({}, {101});
  expected.clear();
  for (const auto & s : all)
    if (!has(s, 101))
      expected.insert(s);
  CHECK(hiddenRow1 == expected);

  const Sols both = subtree({100}, {104});
  expected.clear();
  for (const auto & s : all)
    if (has(s, 100) && !has(s, 104))
      expected.insert(s);
  REQUIRE(!expected.empty());
  CHECK(both == expected);
  CHECK(both.size() < withRow0.size());

  /* the prefix and its complement partition the search */
  const Sols without0 = subtree({}, {100});
  CHECK(without0.size() + withRow0.size() == all.size());

  /* rows that do not fit together are an error, not a silent empty result */
  CHECK_THROWS(subtree({100, 101}, {}));
}

/* ---- the range symmetry breaker ----------------------------------------- */

namespace {

/* A 3x4 board filled by an L tromino used exactly once and shapes used a
 * range of times. The result has rotational symmetry (the half turns of
 * its box), so the breaker has something to break.
 */
std::unique_ptr<puzzle_c> makeRangePuzzle() {
  auto puzzle = std::make_unique<puzzle_c>(std::make_unique<gridType_c>(gridType_c::GT_BRICKS));
  const gridType_c & gt = *puzzle->getGridType();
  /* the shape used exactly once is the L tromino, first, and has no
   * symmetry of its own: turning it by a symmetry of the result gives
   * another orientation, which the breaker can leave out */
  const unsigned int ell    = puzzle->addShape(bttest::fromLayers(gt, {{"##", "#."}}));
  const unsigned int domino = puzzle->addShape(bttest::fromLayers(gt, {{"##"}}));
  const unsigned int bar    = puzzle->addShape(bttest::fromLayers(gt, {{"###"}}));
  const unsigned int dot    = puzzle->addShape(bttest::fromLayers(gt, {{"#"}}));
  const unsigned int result = puzzle->addShape(bttest::fromLayers(gt, {{"####", "####", "####"}}));
  problem_c * problem = puzzle->getProblem(puzzle->addProblem());
  problem->setResultId(result);
  problem->setShapeMinimum(ell, 1);
  problem->setShapeMaximum(ell, 1);
  problem->setShapeMinimum(domino, 0);
  problem->setShapeMaximum(domino, 3);
  problem->setShapeMinimum(bar, 0);
  problem->setShapeMaximum(bar, 1);
  problem->setShapeMinimum(dot, 0);
  problem->setShapeMaximum(dot, 4);
  return puzzle;
}

} // namespace

TEST_CASE("the range symmetry breaker does not change the assemblies found",
          "[cells][assembler][symmetry][simd][equivalence]") {
  auto p = makeRangePuzzle();
  problem_c & problem = *p->getProblem(0);

  struct Engine { const char * name; bool bitParallel; };
  const Engine engines[] = {
    {"default", true}, {"cells", false},
  };

  Set reference;
  for (const auto & e : engines) {
    for (unsigned int threads : {1u, 4u}) {
      INFO(e.name << ", " << threads << " threads");
      Set on, off;
      {
        ScopedSettings env;
        env->bitParallel = e.bitParallel;
        env->rangeSymmetryBreaker = true;
        on = solveAll(problem, threads);
      }
      {
        ScopedSettings env;
        env->bitParallel = e.bitParallel;
        env->rangeSymmetryBreaker = false;
        off = solveAll(problem, threads);
      }
      REQUIRE(on.size() > 1);
      CHECK(on.size() == off.size());
      CHECK(on == off);
      if (reference.empty())
        reference = on;
      CHECK(on == reference);
    }
  }
}

TEST_CASE("the range symmetry breaker restricts the placements of a shape used exactly once",
          "[cells][assembler][symmetry]") {
  auto p = makeRangePuzzle();
  problem_c & problem = *p->getProblem(0);

  auto placements = [&](bool breaker) {
    ScopedSettings env;
    env->rangeSymmetryBreaker = breaker;
    assembler_1_c assm(makeSolverSpec(problem), bttest::settings());
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    // piece 0 is the L, the only shape with min == max == 1
    return assm.getPiecePlacementCount(0);
  };
  const unsigned int on = placements(true), off = placements(false);
  CHECK(on > 0);
  CHECK(on < off);
}

TEST_CASE("the range symmetry breaker leaves the examples' assemblies alone",
          "[cells][assembler][symmetry][simd]") {
  for (const char * path : {"examples/DemoMirrorParadox.xmpuzzle", "examples/PiecesOfEight.xmpuzzle"}) {
    INFO(path);
    auto p = loadPuzzle(path);
    problem_c & problem = *p->getProblem(0);
    Set on, off;
    {
      ScopedSettings env;
      env->rangeSymmetryBreaker = true;
      on = solveAll(problem, 1);
    }
    {
      ScopedSettings env;
      env->rangeSymmetryBreaker = false;
      off = solveAll(problem, 1);
    }
    REQUIRE(!on.empty());
    CHECK(on == off);
  }
}

/* solverSettings_t::mergeColumns: the engines get one column for voxel
 * columns covered by the same rows. Same assemblies with either engine,
 * and a search stopped under one value finishes under the other, since the
 * saved remainder names rows, not columns. The rhombic puzzle merges the
 * most (a rhombic dodecahedron is many cells); the others cover duplicate
 * shapes, piece ranges and variable voxels, whose hole columns stay. */
TEST_CASE("merging identical voxel columns leaves assemblies and saved searches alone",
          "[cells][assembler][simd][save]") {
  const char * paths[] = {
    "examples/PiecesOfEight.xmpuzzle", "examples/CubeInCage.xmpuzzle",
    "examples/DemoPieceGenerator.xmpuzzle", "examples/PelikanBurr.xmpuzzle",
  };
  for (const char * path : paths) {
    for (bool bitParallel : {true, false}) {
      INFO(path << (bitParallel ? ", default engine" : ", Dancing Cells"));
      auto p = loadPuzzle(path);
      problem_c & problem = *p->getProblem(0);
      Set merged, plain;
      {
        ScopedSettings env;
        env->bitParallel = bitParallel;
        env->mergeColumns = true;
        merged = solveAll(problem, 2);
      }
      {
        ScopedSettings env;
        env->bitParallel = bitParallel;
        env->mergeColumns = false;
        plain = solveAll(problem, 2);
      }
      REQUIRE(!merged.empty());
      CHECK(merged == plain);

      if (merged.size() < 2)
        continue;
      for (bool mergeFirst : {true, false}) {
        INFO("stopped with merging " << (mergeFirst ? "on" : "off"));
        Recorder cb;
        std::string state;
        {
          ScopedSettings env;
          env->bitParallel = bitParallel;
          env->mergeColumns = mergeFirst;
          state = stopAfter(problem, 1, 1, cb);
        }
        ScopedSettings env;
        env->bitParallel = bitParallel;
        env->mergeColumns = !mergeFirst;
        const Set rest = resumeAll(problem, 1, state);
        CHECK(unite(cb.fingerprints, rest) == merged);
        CHECK(cb.fingerprints.size() + rest.size() == merged.size());
      }
    }
  }
}

/* A matrix over the bit-parallel engine's widest tier (32768 columns)
 * unmerged but far within it merged runs bit-parallel with merging on (the
 * default) and saves a bit-parallel remainder. That remainder must continue
 * with merging off too: setPosition() and the next run judge and build it
 * merged. Two slabs stacked into a 33x33x31 block: 33759 voxel columns,
 * merged one per layer. */
TEST_CASE("a bit-parallel search that fits the engine only merged continues with merging off",
          "[cells][assembler][simd][save]") {
  auto puzzle = std::make_unique<puzzle_c>(std::make_unique<gridType_c>(gridType_c::GT_BRICKS));
  const gridType_c & gt = *puzzle->getGridType();
  const unsigned int thin  = puzzle->addShape(std::unique_ptr<voxel_c>(gt.getVoxel(33, 33, 15, voxel_c::VX_FILLED)));
  const unsigned int thick = puzzle->addShape(std::unique_ptr<voxel_c>(gt.getVoxel(33, 33, 16, voxel_c::VX_FILLED)));
  const unsigned int block = puzzle->addShape(std::unique_ptr<voxel_c>(gt.getVoxel(33, 33, 31, voxel_c::VX_FILLED)));
  problem_c & problem = *puzzle->getProblem(puzzle->addProblem());
  problem.setResultId(block);
  problem.setShapeMinimum(thin, 1);
  problem.setShapeMaximum(thin, 1);
  problem.setShapeMinimum(thick, 1);
  problem.setShapeMaximum(thick, 1);

  Set all;
  {
    ScopedSettings env;
    env->mergeColumns = true;
    all = solveAll(problem, 2);
  }
  REQUIRE(!all.empty());

  // stop the merged run as its task pool is seeded: everything is left as
  // bit-parallel seeds
  std::string state;
  {
    ScopedSettings env;
    env->mergeColumns = true;
    assembler_1_c assm(makeSolverSpec(problem), bttest::settings());
    assm.setNumThreads(2);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    assm.setPoolSeedHook([&]() { assm.stop(); });
    Recorder none;
    assm.assemble(&none);
    CHECK(none.fingerprints.empty());
    state = saveState(assm);
  }
  REQUIRE(remainderType(state) == 2);

  ScopedSettings env;
  env->mergeColumns = false;
  {
    INFO("restored with merging off");
    CHECK(resumeAll(problem, 2, state) == all);
  }
  {
    INFO("restored with merging off, stopped again, continued in the same assembler");
    assembler_1_c restored(makeSolverSpec(problem), bttest::settings());
    restored.setNumThreads(2);
    REQUIRE(restored.createMatrix(false, false) == assembler_c::ERR_NONE);
    const std::string payload = contentOf(state);
    REQUIRE(restored.setPosition(payload.c_str(), versionOf(state).c_str()) == assembler_c::ERR_NONE);
    restored.setPoolSeedHook([&]() { restored.stop(); });
    Recorder first;
    restored.assemble(&first);
    REQUIRE(remainderType(saveState(restored)) == 2);
    restored.setPoolSeedHook({});
    Recorder rest;
    restored.assemble(&rest);
    CHECK(unite(first.fingerprints, rest.fingerprints) == all);
  }
}

/* Dancing Cells is the engine wherever the bit-parallel one does not run:
 * a matrix it declines (here through its memory budget) and variable
 * voxels together with piece ranges, which it does not model; and every
 * matrix with the bit-parallel engine switched off.
 */
TEST_CASE("Dancing Cells runs where the bit-parallel engine does not",
          "[cells][assembler][save]") {
  struct Case {
    const char * path;
    bool overBudget; // else: declined for variable voxels with piece ranges
    size_t assemblies; // what dancing links found (removed 2026-11-16)
  };
  const Case cases[] = {
    { "examples/CubeInCage.xmpuzzle", true, 96 },
    { "examples/DemoPieceGenerator.xmpuzzle", false, 928 },
  };
  for (const auto & c : cases) {
    INFO(c.path);
    auto p = loadPuzzle(c.path);
    problem_c & problem = *p->getProblem(0);

    Set forced;
    {
      ScopedSettings env;
      env->bitParallel = false;
      forced = solveAll(problem, 4);
      Recorder cb;
      CHECK(remainderType(stopAfter(problem, 4, 1, cb)) == 3u);
    }
    CHECK(forced.size() == c.assemblies);

    {
      ScopedSettings env;
      if (c.overBudget)
        env->huangMemoryBytes = 0;
      CHECK(solveAll(problem, 1) == forced);
      CHECK(solveAll(problem, 4) == forced);
      Recorder cb;
      CHECK(remainderType(stopAfter(problem, 4, 1, cb)) == 3u);
    }
  }
}

/* A bit-parallel remainder restores only on a matrix that engine can take
 * at all: not on one it declines for variable voxels with piece ranges.
 * The memory budget and the bitParallel setting do not count: the seeds
 * keep the engine that left them.
 */
TEST_CASE("restore refuses a bit-parallel remainder the bit-parallel engine cannot take",
          "[cells][assembler][save]") {
  auto restore = [](problem_c & problem, const char * payload) {
    assembler_1_c assm(makeSolverSpec(problem), bttest::settings());
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    return assm.setPosition(payload, "2.2");
  };

  {
    INFO("variable voxels with piece ranges");
    auto p = loadPuzzle("examples/DemoPieceGenerator.xmpuzzle");
    problem_c & problem = *p->getProblem(0);
    CHECK(restore(problem, "1 2 1 0 0 0") == assembler_c::ERR_CAN_NOT_RESTORE_SYNTAX);
    CHECK(restore(problem, "1 3 1 0 0 0") == assembler_c::ERR_NONE);
  }
  {
    INFO("over the memory budget, with the bit-parallel engine off");
    auto p = loadPuzzle("examples/CubeInCage.xmpuzzle");
    problem_c & problem = *p->getProblem(0);
    ScopedSettings env;
    env->huangMemoryBytes = 0;
    env->bitParallel = false;
    CHECK(restore(problem, "1 2 1 0 0 0") == assembler_c::ERR_NONE);
    CHECK(restore(problem, "1 3 1 0 0 0") == assembler_c::ERR_NONE);
  }
}

/* The search is divided into tasks before the workers start (so it runs in
 * parallel without split on demand too), at most splitFanout rows of a node
 * singly and one task for the rest. Every division must cover the search
 * exactly once, and so must a stop that lands among the tasks.
 */
TEST_CASE("Dancing Cells divides the search into tasks exactly once",
          "[cells][assembler][parallel][resume]") {
  const char * paths[] = {
    "examples/CubeInCage.xmpuzzle",
    "examples/PiecesOfEight.xmpuzzle",
    "examples/DemoPieceGenerator.xmpuzzle",
  };
  for (const char * path : paths) {
    auto p = loadPuzzle(path);
    problem_c & problem = *p->getProblem(0);

    Set baseline;
    {
      ScopedSettings env;
      env->bitParallel = false;
      env->donate = false;
      baseline = solveAll(problem, 1);
    }
    REQUIRE(baseline.size() > 3);

    for (unsigned int fanout : {1u, 2u, 3u, 16u}) {
      for (bool donate : {false, true}) {
        for (unsigned int threads : {2u, 4u}) {
          INFO(path << ", fanout " << fanout << ", donate " << donate << ", " << threads << " threads");
          ScopedSettings env;
          env->bitParallel = false;
          env->splitFanout = fanout;
          env->donate = donate;
          CHECK(solveAll(problem, threads) == baseline);

          // stopped half way, saved and restored
          Recorder cb;
          const std::string state = stopAfter(problem, threads, static_cast<unsigned int>(baseline.size() / 2), cb);
          const Set rest = resumeAll(problem, threads, state);
          CHECK(unite(cb.fingerprints, rest) == baseline);
          CHECK(cb.fingerprints.size() + rest.size() == baseline.size());
        }
      }
    }
  }
}
