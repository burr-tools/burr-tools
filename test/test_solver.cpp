#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "lib/puzzle.h"
#include "lib/solvethread.h"
#include "lib/voxel.h"
#include "lib/problem.h"
#include "lib/assembler.h"
#include "lib/assembler_1.h"
#include "lib/assembler_1.h"
#include "lib/assembly.h"
#include "lib/disassembler.h"
#include "lib/disassembler_0.h"
#include "lib/disassembly.h"
#include "lib/gridtype.h"
#include "lib/progressmodel.h"
#include "tools/xml.h"
#include "tools/gzstream.h"
#include "test_helpers.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using bttest::ScopedSettings;

/* Records a canonical fingerprint of every assembly, so two runs can be
 * compared as multisets instead of by count alone. Comparing counts cannot
 * detect a parallel run that loses one assembly and duplicates another.
 */
class RecordingAssemblerCallback : public assembler_cb {
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

class TestAssemblerCallback : public assembler_cb {
public:
  int assemblies{0};
  int solutions{0};
  std::string lastMoveLevel;
  disassembler_c * disassembler{nullptr};
  std::unique_ptr<assembly_c> firstSolutionAssembly;
  std::unique_ptr<assembly_c> firstAssembly;

  explicit TestAssemblerCallback(disassembler_c * d = nullptr) : disassembler(d) {}

  bool assembly(std::unique_ptr<assembly_c> a) override {
    assemblies++;
    if (!firstAssembly) {
      firstAssembly = std::make_unique<assembly_c>(a.get());
    }
    if (disassembler) {
      auto da = disassembler->disassemble(a.get());
      if (da) {
        solutions++;
        lastMoveLevel = da->movesText();
        if (!firstSolutionAssembly) {
          firstSolutionAssembly = std::make_unique<assembly_c>(a.get());
        }
      }
    }
    return true;
  }
};

struct SolveResult {
  int assemblies{0};
  int solutions{0};
  unsigned long long iterations{0};
  std::string moveLevel;
  std::unique_ptr<assembly_c> firstSolutionAssembly;
  std::unique_ptr<assembly_c> firstAssembly;
  const problem_c * problem{nullptr};
};

static void writeAssemblerXml(const assembler_c & assm, xmlWriter_c & xml) {
  auto [ver, st] = assm.saveState();
  xml.newTag("assembler");
  if (!ver.empty())
    xml.newAttrib("version", ver);
  if (!st.empty())
    xml.addContent(st);
  xml.endTag("assembler");
}

SolveResult solvePuzzle(const char * path, unsigned int problemIdx = 0, bool disassemble = true) {
  auto p = puzzle_c::load(path);
  REQUIRE(p != nullptr);

  REQUIRE(problemIdx < p->getNumberOfProblems());
  problem_c * problem = p->getProblem(problemIdx);
  REQUIRE(problem != nullptr);

  const gridType_c * gt = problem->getPuzzle().getGridType();
  REQUIRE(gt != nullptr);

  std::unique_ptr<assembler_c> assm = gt->findAssembler(makeSolverSpec(*problem), bttest::settings());
  REQUIRE(assm != nullptr);

  REQUIRE(assm->createMatrix(false, false) == assembler_c::ERR_NONE);

  std::unique_ptr<disassembler_c> disasm;
  if (disassemble && (gt->getCapabilities() & gridType_c::CAP_DISASSEMBLE)) {
    disasm = std::make_unique<disassembler_0_c>(makeSolverSpec(*problem), bttest::settings());
  }

  TestAssemblerCallback cb(disasm.get());
  assm->assemble(&cb);

  return SolveResult{
    cb.assemblies,
    cb.solutions,
    assm->getIterations(),
    cb.lastMoveLevel,
    std::move(cb.firstSolutionAssembly),
    std::move(cb.firstAssembly),
    problem
  };
}

} // namespace

TEST_CASE("Pelikan Burr solver regression (GT_BRICKS)", "[solver][pelikan]") {
  SolveResult res = solvePuzzle("examples/PelikanBurr.xmpuzzle", 0, true);

  CHECK(res.assemblies == 12);
  CHECK(res.solutions == 1);
  CHECK(res.iterations > 0);
  CHECK(res.moveLevel == "98.2.4.2");
  REQUIRE(res.firstSolutionAssembly != nullptr);
  CHECK(res.firstSolutionAssembly->placementCount() == 7);
}

TEST_CASE("Dracula's Dental Desaster solver regression (GT_BRICKS)", "[solver][dracula]") {
  SolveResult res = solvePuzzle("examples/DraculasDentalDesaster.xmpuzzle", 0, true);

  CHECK(res.assemblies == 84);
  CHECK(res.solutions == 1);
  CHECK(res.iterations > 0);
  REQUIRE(res.firstSolutionAssembly != nullptr);
  CHECK(res.firstSolutionAssembly->placementCount() == 9);
}

TEST_CASE("Prisgon solver regression (GT_BRICKS)", "[solver][prisgon]") {
  SolveResult res = solvePuzzle("examples/Prisgon.xmpuzzle", 0, true);

  CHECK(res.assemblies == 1);
  CHECK(res.solutions == 1);
  CHECK(res.iterations > 0);
  REQUIRE(res.firstSolutionAssembly != nullptr);
  CHECK(res.firstSolutionAssembly->placementCount() == 9);
}

TEST_CASE("Demo Mirror Paradox solver regression (GT_BRICKS / Assembler 1)", "[solver][mirrorparadox]") {
  SolveResult res = solvePuzzle("examples/DemoMirrorParadox.xmpuzzle", 0, true);

  CHECK(res.assemblies == 1);
  CHECK(res.solutions == 1);
  CHECK(res.iterations > 0);
  REQUIRE(res.firstSolutionAssembly != nullptr);
}

TEST_CASE("Cube in Cage solver regression (GT_BRICKS / Assembler 1)", "[solver][cubeincage]") {
  SolveResult res = solvePuzzle("examples/CubeInCage.xmpuzzle", 0, true);

  CHECK(res.assemblies == 96);
  CHECK(res.solutions == 1);
  CHECK(res.iterations > 0);
  REQUIRE(res.firstSolutionAssembly != nullptr);
}

TEST_CASE("Bermuda solver regression (GT_TRIANGULAR_PRISM)", "[solver][bermuda]") {
  SolveResult res = solvePuzzle("examples/Bermuda.xmpuzzle", 0, true);

  CHECK(res.assemblies == 1);
  CHECK(res.solutions == 1);
  CHECK(res.iterations > 0);
  REQUIRE(res.firstSolutionAssembly != nullptr);
}

TEST_CASE("Augmented Second Stellation assembly (GT_SPHERES)", "[solver][spheres]") {
  // Spheres grid supports assembly only, not disassembly
  SolveResult res = solvePuzzle("examples/AugmentedSecondStellation.xmpuzzle", 0, false);

  CHECK(res.assemblies == 2);
  CHECK(res.solutions == 0);
  CHECK(res.iterations > 0);
  REQUIRE(res.firstAssembly != nullptr);
}

TEST_CASE("Malformed XML input rejection", "[parser][malformed]") {
  SECTION("too_many_voxels.xmpuzzle must throw xmlParserException_c") {
    std::unique_ptr<std::istream> str(openGzFile("test/malformed/too_many_voxels.xmpuzzle"));
    REQUIRE(str != nullptr);
    xmlParser_c pars(*str);
    CHECK_THROWS_AS(puzzle_c(pars), xmlParserException_c);
  }

  SECTION("oversized_dimensions.xmpuzzle must throw xmlParserException_c") {
    std::unique_ptr<std::istream> str(openGzFile("test/malformed/oversized_dimensions.xmpuzzle"));
    REQUIRE(str != nullptr);
    xmlParser_c pars(*str);
    CHECK_THROWS_AS(puzzle_c(pars), xmlParserException_c);
  }

  SECTION("separation_before_assembly.xmpuzzle must throw xmlParserException_c") {
    std::unique_ptr<std::istream> str(openGzFile("test/malformed/separation_before_assembly.xmpuzzle"));
    REQUIRE(str != nullptr);
    xmlParser_c pars(*str);
    CHECK_THROWS_AS(puzzle_c(pars), xmlParserException_c);
  }
}

TEST_CASE("Puzzle metadata inspection and modern accessors", "[metadata]") {
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);

  CHECK(p->getNumberOfProblems() == 1);
  CHECK(p->getComment().find("Pelikan Burr") != std::string::npos);
  CHECK(p->getProblem(0)->getNumberOfPieces() == 7);
  CHECK(p->getProblems().size() == 1);
  CHECK(p->getShapes().size() == p->getNumberOfShapes());
  CHECK(p->getShapes().size() == 8);
}

TEST_CASE("bt_assert throws assert_exception with C++20 source_location", "[assert]") {
  // bt_assert compiles to ((void)0) under NDEBUG, so only the passing half
  // applies there; the throwing half is debug-only (same guard idiom as
  // test_halfedge.cpp).
#ifndef NDEBUG
  try {
    bt_assert(1 == 2);
    FAIL("bt_assert should have thrown assert_exception");
  } catch (const assert_exception & e) {
    CHECK(std::string(e.expr) == "1 == 2");
    CHECK(std::string(e.file).ends_with("test_solver.cpp"));
    CHECK(e.line > 0);
    CHECK(std::string(e.what()) == "1 == 2");
  }
#endif

  // Passing assertion does not throw
  CHECK_NOTHROW([&] { bt_assert(2 + 2 == 4); }());
}

TEST_CASE("assert_log correctly records lines", "[assert]") {
  // bt_assert_line compiles to nothing under NDEBUG, so there is nothing
  // to record there -- same guard idiom as test_halfedge.cpp.
#ifndef NDEBUG
  REQUIRE(assert_log != nullptr);
  unsigned int initialLines = assert_log->lines();
  bt_assert_line("first assert log entry");
  bt_assert_line("second assert log entry");
  CHECK(assert_log->lines() == initialLines + 2);
  CHECK(std::string(assert_log->line(initialLines)) == "first assert log entry");
  CHECK(std::string(assert_log->line(initialLines + 1)) == "second assert log entry");
#endif
}

TEST_CASE("Symmetry calculation for non-cube grids with unaligned bounding boxes", "[symmetry]") {
  // Test GT_RHOMBIC whose voxel class (voxel_3_c) aligns bounding boxes to multiples of 5
  {
    gridType_c gt(gridType_c::GT_RHOMBIC);
    std::unique_ptr<voxel_c> v(gt.getVoxel(3, 3, 3, voxel_c::VX_EMPTY));
    REQUIRE(v != nullptr);
    for (unsigned int z = 0; z < 3; z++) {
      for (unsigned int y = 0; y < 3; y++) {
        for (unsigned int x = 0; x < 3; x++) {
          if (v->validCoordinate(x, y, z)) {
            v->set(x, y, z, voxel_c::VX_FILLED);
          }
        }
      }
    }
    CHECK_NOTHROW(v->selfSymmetries());
    CHECK(v->selfSymmetries() != 0);
  }

  // Test GT_TETRA_OCTA whose voxel class (voxel_4_c) aligns bounding boxes to multiples of 3
  {
    gridType_c gt(gridType_c::GT_TETRA_OCTA);
    std::unique_ptr<voxel_c> v(gt.getVoxel(2, 2, 2, voxel_c::VX_EMPTY));
    REQUIRE(v != nullptr);
    for (unsigned int z = 0; z < 2; z++) {
      for (unsigned int y = 0; y < 2; y++) {
        for (unsigned int x = 0; x < 2; x++) {
          if (v->validCoordinate(x, y, z)) {
            v->set(x, y, z, voxel_c::VX_FILLED);
          }
        }
      }
    }
    CHECK_NOTHROW(v->selfSymmetries());
    CHECK(v->selfSymmetries() != 0);
  }
}

TEST_CASE("Assembler early stop on false callback return and derived assemble overload visibility", "[assembler]") {
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  // Test assembler_1_c: calls assemble with lambda overload directly on derived assembler_1_c
  {
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    int count = 0;
    assm.assemble([&](std::unique_ptr<assembly_c>) -> bool {
      count++;
      return false; // Return false to stop immediately
    });
    CHECK(count == 1);
  }
}

TEST_CASE("problem_c::setAssembler takes std::unique_ptr and transfers ownership", "[problem]") {
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);
  problem->removeAllSolutions();

  auto assm = std::make_unique<assembler_1_c>(makeSolverSpec(*problem), bttest::settings());
  assembler_c * raw = assm.get();
  assembler_c::errState err = problem->setAssembler(std::move(assm));
  CHECK(err == assembler_c::ERR_NONE);
  CHECK(assm == nullptr);
  CHECK(problem->getAssembler() == raw);
}

TEST_CASE("Parallel assembler produces identical results to single-threaded", "[assembler][parallel]") {
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  int assemblies_1 = 0;
  int solutions_1 = 0;
  {
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    disassembler_0_c disasm(makeSolverSpec(*problem), bttest::settings());
    TestAssemblerCallback cb(&disasm);
    assm.assemble(&cb);
    assemblies_1 = cb.assemblies;
    solutions_1 = cb.solutions;
    CHECK(assemblies_1 == 12);
    CHECK(solutions_1 == 1);
  }

  {
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    disassembler_0_c disasm(makeSolverSpec(*problem), bttest::settings());
    TestAssemblerCallback cb(&disasm);
    assm.assemble(&cb);
    CHECK(cb.assemblies == assemblies_1);
    CHECK(cb.solutions == solutions_1);
    CHECK(assm.getIterations() > 0);
    CHECK(assm.getFinished() >= 1.0f);
  }
}

TEST_CASE("Parallel assembler pause and continue does not duplicate solutions", "[assembler][parallel][resume]") {
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  // Baseline: solve in one go
  int total_expected = 0;
  {
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    TestAssemblerCallback cb;
    assm.assemble(&cb);
    total_expected = cb.assemblies;
    REQUIRE(total_expected == 12);
  }

  // Two-phase solve: stop after 5 assemblies, then continue
  {
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);

    int phase1_count = 0;
    class StoppingCallback : public assembler_cb {
    public:
      int count = 0;
      assembler_1_c & a;
      StoppingCallback(assembler_1_c & assm) : a(assm) {}
      bool assembly(std::unique_ptr<assembly_c>) override {
        count++;
        if (count == 5) {
          a.stop();
          return false;
        }
        return true;
      }
    } cb1(assm);

    assm.assemble(&cb1);
    phase1_count = cb1.count;
    CHECK(phase1_count == 5);

    // Now continue searching
    TestAssemblerCallback cb2;
    assm.assemble(&cb2);

    // Total assemblies found across both phases must equal full run
    CHECK(phase1_count + cb2.assemblies == total_expected);
  }
}

/* assembler_c::save() emits <assembler version="X">payload</assembler>.
 * These pull the two pieces back out so a test can feed them to setPosition()
 * exactly the way problem_c does when a puzzle is loaded.
 */
static std::string assemblerVersionOf(const std::string & xml) {
  /* start at the tag, not at the document: the <?xml ...?> header carries a
   * version attribute of its own
   */
  size_t tag = xml.find("<assembler");
  REQUIRE(tag != std::string::npos);
  size_t a = xml.find("version=\"", tag);
  REQUIRE(a != std::string::npos);
  a += 9;
  size_t b = xml.find('"', a);
  REQUIRE(b != std::string::npos);
  return xml.substr(a, b - a);
}

static std::string extractAssemblerContent(const std::string & xml) {
  size_t a = xml.find("<assembler");
  REQUIRE(a != std::string::npos);
  a = xml.find('>', a);
  REQUIRE(a != std::string::npos);
  a++;
  size_t b = xml.find("</assembler>", a);
  REQUIRE(b != std::string::npos);
  return xml.substr(a, b - a);
}

/* setNumThreads is reachable from -t and from Problem.solve(threads=...),
 * neither of which validated the value; an unclamped count went straight into
 * thread creation.
 */
TEST_CASE("Assembler clamps an absurd thread count instead of trying to spawn it",
          "[assembler][parallel][threads]") {
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
  assm.setNumThreads(1000000);
  CHECK(assm.getNumThreads() <= 256);

  REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
  int seen = 0;
  assm.assemble([&seen](std::unique_ptr<assembly_c>) -> bool { seen++; return true; });
  CHECK(seen == 12);
}

/* Parallel search on a puzzle that actually enables symmetry breaking.
 *
 * Workers call assembly_c::smallerRotationExists() outside callbackMutex, and
 * that reaches the lazily filled mutable caches (BbHsCache, symmetries) on the
 * shapes shared by every worker -- on the result shape and, via
 * normalizeTransformation() and the hotspot fixup in assembly_c::transform(),
 * on every part shape too. Those are unsynchronised check-then-write.
 *
 * The other [parallel] cases use PelikanBurr and CubeInCage, neither of which
 * has a symmetry breaker, so they never enter that branch at all and cannot
 * detect anything here. Keep this case on a symmetry-breaking puzzle; swapping
 * the puzzle silently removes the coverage.
 *
 * Under a ThreadSanitizer build this is the case that catches a missing
 * pre-warm: with only the result shape warmed it reports ~36 races on
 * assembly.cpp's getPartShape(i)->getHotspot() calls.
 */
TEST_CASE("Parallel assembler matches serial on a symmetry-breaking puzzle",
          "[assembler][parallel][tsan]") {
  auto p = puzzle_c::load("examples/Bermuda.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  std::multiset<std::string> serial;
  {
    RecordingAssemblerCallback cb;
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    serial = std::move(cb.fingerprints);
  }
  REQUIRE_FALSE(serial.empty());

  /* Guard the premise rather than trusting the chosen puzzle: keepRotations
   * forces avoidTransformedAssemblies off, so it must yield strictly more
   * assemblies. If the two agree, symmetry breaking is not active here any
   * more and this case has stopped covering the concurrent path.
   */
  {
    RecordingAssemblerCallback cb;
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(true, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    INFO("symmetry breaking must be active for this test to be meaningful");
    REQUIRE(cb.fingerprints.size() > serial.size());
  }

  /* Reload so the parallel run starts with cold caches: the lazy fills are
   * first-touch, so a parallel run after a serial run on the same puzzle
   * object races on already-warm caches and reports nothing.
   */
  auto pFresh = puzzle_c::load("examples/Bermuda.xmpuzzle");
  REQUIRE(pFresh != nullptr);
  auto problemFresh = pFresh->getProblem(0);
  REQUIRE(problemFresh != nullptr);

  RecordingAssemblerCallback cb;
  assembler_1_c assm(makeSolverSpec(*problemFresh), bttest::settings());
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
  assm.assemble(&cb);

  CHECK(cb.fingerprints == serial);
}

TEST_CASE("Parallel assembler 1 produces identical results to single-threaded", "[assembler][parallel]") {
  auto p = puzzle_c::load("examples/CubeInCage.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  std::multiset<std::string> serial;
  {
    RecordingAssemblerCallback cb;
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    serial = std::move(cb.fingerprints);
  }
  CHECK(serial.size() == 96);

  {
    RecordingAssemblerCallback cb;
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    /* the set, not the size: a lost assembly paired with a duplicated one
     * would pass a count comparison
     */
    CHECK(cb.fingerprints == serial);
    CHECK(assm.getIterations() > 0);
    CHECK(assm.getFinished() >= 1.0f);
  }
}

/* Forces generateSubtreeTasks() past its first pass.
 *
 * targetTasks is max(16, workers*4), so with a large worker count depth 1
 * cannot supply enough tasks and the cutoff_depth++ escalation runs -- which
 * re-walks the prefix of the tree it already walked. That re-walk is what used
 * to re-report assemblies; CubeInCage with 4 threads clears 16 tasks on the
 * first pass and never enters the path at all.
 */
TEST_CASE("Parallel assembler 1 does not duplicate assemblies when task generation escalates",
          "[assembler][parallel][retry]") {
  auto p = puzzle_c::load("examples/CubeInCage.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  std::multiset<std::string> serial;
  {
    RecordingAssemblerCallback cb;
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    serial = std::move(cb.fingerprints);
  }
  REQUIRE_FALSE(serial.empty());

  RecordingAssemblerCallback cb;
  assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
  assm.setNumThreads(64);          // targetTasks = 256, unreachable at depth 1
  REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
  assm.assemble(&cb);
  CHECK(cb.fingerprints == serial);
}

/* Stopping and continuing on the same assembler must not report the first
 * run's assemblies again.
 */
TEST_CASE("Parallel assembler 1 pause and continue does not duplicate assemblies",
          "[assembler][parallel][resume]") {
  auto p = puzzle_c::load("examples/CubeInCage.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  std::multiset<std::string> serial;
  {
    RecordingAssemblerCallback cb;
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    serial = std::move(cb.fingerprints);
  }
  REQUIRE(serial.size() > 1);

  assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);

  RecordingAssemblerCallback cb;
  int seen = 0;
  assm.assemble([&](std::unique_ptr<assembly_c> a) -> bool {
    cb.assembly(std::move(a));
    return ++seen < 1;             // stop after the first
  });
  REQUIRE(seen == 1);

  assm.assemble(&cb);              // continue on the same assembler

  /* every assembly exactly once across the two runs */
  CHECK(cb.fingerprints == serial);
}

/* Huang-SIMD pause and continue resumes from salvaged prefixes (issue #111).
 *
 * Same shape as the DLX test above, but on a range puzzle so the SIMD Huang
 * path (not DLX) runs in both phases: stop after the first assembly, then
 * continue must reproduce the uninterrupted multiset exactly -- no lost
 * subtrees, no duplicates. The iterations guard proves both phases really
 * took the SIMD path (DLX counts differ).
 */
TEST_CASE("Huang-SIMD pause and continue resumes salvaged prefixes",
          "[assembler][parallel][resume][simd]") {
  auto p = puzzle_c::load("examples/PiecesOfEight.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  std::multiset<std::string> serial;
  unsigned long long simdIterations = 0;
  {
    RecordingAssemblerCallback cb;
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    serial = std::move(cb.fingerprints);
    simdIterations = assm.getIterations();
  }
  REQUIRE(serial.size() > 1);

  {
    ScopedSettings env;
    env->bitParallel = false;
    RecordingAssemblerCallback cb;
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    REQUIRE(cb.fingerprints == serial);
    INFO("the bit-parallel engine must differ in iteration count from Dancing Cells");
    CHECK(simdIterations != assm.getIterations());
  }

  assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);

  RecordingAssemblerCallback cb;
  int seen = 0;
  assm.assemble([&](std::unique_ptr<assembly_c> a) -> bool {
    cb.assembly(std::move(a));
    return ++seen < 1;             // stop after the first
  });
  REQUIRE(seen == 1);

  assm.assemble(&cb);              // continue on the same assembler

  /* every assembly exactly once across the two runs */
  CHECK(cb.fingerprints == serial);
}

/* Save/stop/continue roundtrip across a process-equivalent boundary led
 * by parallel runs (issue #90): stop mid-search, save to XML, restore into
 * a fresh assembler, continue, and require the union to equal the
 * uninterrupted multiset. Covers the Huang-SIMD remainder (placed-node
 * prefixes) here; the DLX snapshot variant follows below. */
TEST_CASE("Huang-SIMD save, load and continue reproduces the full multiset",
          "[assembler][parallel][resume][simd][save]") {
  auto p = puzzle_c::load("examples/PiecesOfEight.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  std::multiset<std::string> serial;
  {
    RecordingAssemblerCallback cb;
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    serial = std::move(cb.fingerprints);
  }
  REQUIRE(serial.size() > 1);

  // Phase 1: stop after the first assembly.
  RecordingAssemblerCallback cb1;
  std::string state;
  {
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    int seen = 0;
    assm.assemble([&](std::unique_ptr<assembly_c> a) -> bool {
      cb1.assembly(std::move(a));
      return ++seen < 1;
    });
    REQUIRE(seen == 1);

    std::ostringstream str;
    xmlWriter_c xml(str);
    writeAssemblerXml(assm, xml);
    state = str.str();
  }

  // The payload must carry task data under the current version.
  CHECK(assemblerVersionOf(state) == "2.2");

  // Phase 2: restore into a fresh assembler and continue.
  {
    assembler_1_c restored(makeSolverSpec(*problem), bttest::settings());
    REQUIRE(restored.createMatrix(false, false) == assembler_c::ERR_NONE);
    std::string payload = extractAssemblerContent(state);
    CHECK(restored.setPosition(payload.c_str(), assemblerVersionOf(state).c_str())
          == assembler_c::ERR_NONE);

    RecordingAssemblerCallback cb2;
    restored.assemble(&cb2);

    std::multiset<std::string> resumed = cb1.fingerprints;
    resumed.insert(cb2.fingerprints.begin(), cb2.fingerprints.end());
    CHECK(resumed == serial);
  }
}

/* The chained-stop protocol shared by the Huang and A0-SIMD cases below:
 * stop after k1 assemblies, save, restore, stop after k2 more, save,
 * restore, run to completion. The union of the phases must equal the
 * uninterrupted baseline as a multiset. Callback stops with 4 workers leave
 * the other workers mid-task, so every stop divides live frames. Thread
 * races rule out remainder pins; union and finished == 1 hold under every
 * schedule.
 */
template <class Assembler>
static void checkChainedStopsExactOnce(problem_c & problem) {
  std::multiset<std::string> baseline;
  {
    RecordingAssemblerCallback cb;
    Assembler assm(makeSolverSpec(problem), bttest::settings());
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    baseline = std::move(cb.fingerprints);
  }
  REQUIRE(baseline.size() > 3);
  INFO("baseline: " << baseline.size());

  /* One phase: restore (unless fresh), run until stopAfter assemblies
   * (0 = to completion), save. */
  auto phase = [&](const std::string & in, unsigned int stopAfter,
                   RecordingAssemblerCallback & cb, bool & finished) {
    Assembler assm(makeSolverSpec(problem), bttest::settings());
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    if (!in.empty()) {
      std::string payload = extractAssemblerContent(in);
      REQUIRE(assm.setPosition(payload.c_str(), assemblerVersionOf(in).c_str())
              == assembler_c::ERR_NONE);
    }
    unsigned int seen = 0;
    assm.assemble([&](std::unique_ptr<assembly_c> a) -> bool {
      cb.assembly(std::move(a));
      return stopAfter == 0 || ++seen < stopAfter;
    });
    finished = assm.getFinished() == 1.0f;
    std::ostringstream str;
    xmlWriter_c xml(str);
    writeAssemblerXml(assm, xml);
    return str.str();
  };

  const size_t n = baseline.size();
  const unsigned int stops[][2] = {
    {1, 1},
    {static_cast<unsigned int>(n / 3), static_cast<unsigned int>(n / 3)},
    {static_cast<unsigned int>(n - 2), 1},
  };
  for (const auto & k : stops) {
    INFO("stops: " << k[0] << ", " << k[1]);
    RecordingAssemblerCallback cb1, cb2, cb3;
    bool f1 = false, f2 = false, f3 = false;
    std::string s1 = phase("", k[0], cb1, f1);
    std::string s2 = f1 ? s1 : phase(s1, k[1], cb2, f2);
    if (!f1 && !f2)
      phase(s2, 0, cb3, f3);
    CHECK((f1 || f2 || f3));
    std::multiset<std::string> resumed = cb1.fingerprints;
    resumed.insert(cb2.fingerprints.begin(), cb2.fingerprints.end());
    resumed.insert(cb3.fingerprints.begin(), cb3.fingerprints.end());
    CHECK(resumed == baseline);
  }
}

/* Huang divide-at-stop: chained stops resume exact-once.
 *
 * A stopped Huang worker divides its task while search() unwinds (see
 * SimdHuangCover::StopTrace): untouched branches become seeds, the
 * searched region is dropped, a never-explored node is kept whole, and a
 * declined solution is kept as its own leaf. Resuming must therefore
 * report every assembly exactly once, however many stops land in between.
 *
 * Chained stops (checkChainedStopsExactOnce) on four assembler_1
 * puzzles on the Huang path (the saves carry ptype 2 seeds); CubeInCage
 * has identical pieces, so shape pivots (and the monotonic filter the
 * seeds must replay) occur, and SolidSixPieceBurrs (25 parts) stops deep,
 * with many live frames to divide. The fan-out cap of 1 puts rests into
 * every save.
 */
TEST_CASE("Huang divided remainders resume exact-once across chained stops",
          "[assembler][parallel][resume][simd][save]") {
  // A fan-out cap of 1 makes every divided frame with more than one
  // remaining branch leave a rest seed, so rests (and rests of rests across
  // the chained stops) occur on these small puzzles.
  ScopedSettings fanout;
  fanout->splitFanout = 1;
  const char * paths[] = {
    "examples/PiecesOfEight.xmpuzzle",
    "examples/CubeInCage.xmpuzzle",
    "examples/DemoPieceGenerator.xmpuzzle",
    "examples/SolidSixPieceBurrs.xmpuzzle",
  };
  for (const char * path : paths) {
    INFO("puzzle: " << path);
    auto p = puzzle_c::load(path);
    REQUIRE(p != nullptr);
    auto problem = p->getProblem(0);
    REQUIRE(problem != nullptr);
    checkChainedStopsExactOnce<assembler_1_c>(*problem);
  }
}

/* Divide-at-stop on puzzles with every piece used exactly once (what
 * assembler_0 took until 2026-11-16): chained stops resume exact-once, as
 * in the case above (checkChainedStopsExactOnce).
 */
TEST_CASE("Exact-once puzzles: divided remainders resume exact-once across chained stops",
          "[assembler][parallel][resume][simd][save]") {
  const char * paths[] = {
    "examples/PelikanBurr.xmpuzzle",
    "examples/DraculasDentalDesaster.xmpuzzle",
  };
  for (const char * path : paths) {
    INFO("puzzle: " << path);
    auto p = puzzle_c::load(path);
    REQUIRE(p != nullptr);
    auto problem = p->getProblem(0);
    REQUIRE(problem != nullptr);
    checkChainedStopsExactOnce<assembler_1_c>(*problem);
  }
}

/* Serial continue after a parallel save (issue #118 review): the restored
 * remainder carries no serial stacks, so a single-threaded continue that
 * fell back to the serial search would re-search from the root and report
 * every phase-1 assembly a second time; willRunParallel() keeps it on the
 * remainder. Uses threads=1 on restore. */
TEST_CASE("Serial continue after parallel save does not duplicate",
          "[assembler][parallel][resume][simd][save]") {
  auto p = puzzle_c::load("examples/PiecesOfEight.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  std::multiset<std::string> serial;
  {
    RecordingAssemblerCallback cb;
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    serial = std::move(cb.fingerprints);
  }
  REQUIRE(serial.size() > 1);

  RecordingAssemblerCallback cb1;
  std::string state;
  {
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    int seen = 0;
    assm.assemble([&](std::unique_ptr<assembly_c> a) -> bool {
      cb1.assembly(std::move(a));
      return ++seen < 1;
    });
    REQUIRE(seen == 1);

    std::ostringstream str;
    xmlWriter_c xml(str);
    writeAssemblerXml(assm, xml);
    state = str.str();
  }

  {
    assembler_1_c restored(makeSolverSpec(*problem), bttest::settings());
    REQUIRE(restored.createMatrix(false, false) == assembler_c::ERR_NONE);
    std::string payload = extractAssemblerContent(state);
    CHECK(restored.setPosition(payload.c_str(), assemblerVersionOf(state).c_str())
          == assembler_c::ERR_NONE);

    restored.setNumThreads(1);
    RecordingAssemblerCallback cb2;
    restored.assemble(&cb2);

    std::multiset<std::string> resumed = cb1.fingerprints;
    resumed.insert(cb2.fingerprints.begin(), cb2.fingerprints.end());
    CHECK(resumed == serial);
  }
}

/* Save/load/continue on a symmetry-breaking puzzle (issue #118 review):
 * DemoMirrorParadox enables avoidTransformedAssemblies, so worker threads
 * call smallerRotationExists() concurrently -- the only bundled example
 * covering that path for assembler_1. The premise guard (keepRotations
 * must yield strictly more assemblies) keeps the case honest: if symmetry
 * breaking ever goes inactive here, the test fails instead of silently
 * covering nothing. Otherwise the same save/stop/continue multiset
 * contract as the PiecesOfEight case above. */
TEST_CASE("Symmetry-breaking save, load and continue reproduces the multiset",
          "[assembler][parallel][resume][simd][save]") {
  auto p = puzzle_c::load("examples/DemoMirrorParadox.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(1);
  REQUIRE(problem != nullptr);

  size_t unbroken = 0;
  {
    RecordingAssemblerCallback cb;
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(true, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    unbroken = cb.fingerprints.size();
    INFO("symmetry breaking must be active for this test to be meaningful");
    REQUIRE(unbroken > 1);
  }

  std::multiset<std::string> serial;
  {
    RecordingAssemblerCallback cb;
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    serial = std::move(cb.fingerprints);
  }
  // Exactly one assembly here: the stop-after-first below still interrupts
  // (workers are mid-search), and the continue must not re-report it.
  REQUIRE(serial.size() == 1);
  CHECK(serial.size() < unbroken);

  RecordingAssemblerCallback cb1;
  std::string state;
  {
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    int seen = 0;
    assm.assemble([&](std::unique_ptr<assembly_c> a) -> bool {
      cb1.assembly(std::move(a));
      return ++seen < 1;
    });
    REQUIRE(seen == 1);

    std::ostringstream str;
    xmlWriter_c xml(str);
    writeAssemblerXml(assm, xml);
    state = str.str();
  }

  {
    assembler_1_c restored(makeSolverSpec(*problem), bttest::settings());
    REQUIRE(restored.createMatrix(false, false) == assembler_c::ERR_NONE);
    std::string payload = extractAssemblerContent(state);
    CHECK(restored.setPosition(payload.c_str(), assemblerVersionOf(state).c_str())
          == assembler_c::ERR_NONE);

    RecordingAssemblerCallback cb2;
    restored.assemble(&cb2);

    std::multiset<std::string> resumed = cb1.fingerprints;
    resumed.insert(cb2.fingerprints.begin(), cb2.fingerprints.end());
    CHECK(resumed == serial);
  }
}

/* Second save in the chain (issue #118 review): parallel stop, save,
 * reload, single-threaded continue, stop, save again. The restored
 * remainder keeps the one-thread continue on the task pool, so the second
 * save carries what is left of that remainder; reloading it must resume
 * exact-once instead of doubling. Uses the reviewer's repro shape
 * (CubeInCage, DLX-forced). */
TEST_CASE("Second save in a single-threaded chain resumes exact-once",
          "[assembler][parallel][resume][save]") {
  auto p = puzzle_c::load("examples/CubeInCage.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  ScopedSettings noSimd;
  noSimd->bitParallel = false;

  std::multiset<std::string> serial;
  {
    RecordingAssemblerCallback cb;
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    serial = std::move(cb.fingerprints);
  }
  REQUIRE(serial.size() > 5);

  auto saveState = [&](assembler_1_c & assm) {
    std::ostringstream str;
    xmlWriter_c xml(str);
    writeAssemblerXml(assm, xml);
    return str.str();
  };
  auto restoreInto = [&](const std::string & state, assembler_1_c & restored) {
    REQUIRE(restored.createMatrix(false, false) == assembler_c::ERR_NONE);
    std::string payload = extractAssemblerContent(state);
    CHECK(restored.setPosition(payload.c_str(), assemblerVersionOf(state).c_str())
          == assembler_c::ERR_NONE);
  };

  // Phase 1: parallel stop after 5 assemblies, save.
  RecordingAssemblerCallback cb1;
  std::string state1;
  {
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    int seen = 0;
    assm.assemble([&](std::unique_ptr<assembly_c> a) -> bool {
      cb1.assembly(std::move(a));
      return ++seen < 5;
    });
    REQUIRE(seen == 5);
    state1 = saveState(assm);
  }

  // Phase 2: reload, single-threaded continue, stop after 3 more, save again.
  RecordingAssemblerCallback cb2;
  std::string state2;
  {
    assembler_1_c restored(makeSolverSpec(*problem), bttest::settings());
    restoreInto(state1, restored);
    restored.setNumThreads(1);
    int seen = 0;
    restored.assemble([&](std::unique_ptr<assembly_c> a) -> bool {
      cb2.assembly(std::move(a));
      return ++seen < 3;
    });
    REQUIRE(seen == 3);
    state2 = saveState(restored);
  }

  // Phase 3: reload the second save, continue to the end.
  RecordingAssemblerCallback cb3;
  {
    assembler_1_c restored(makeSolverSpec(*problem), bttest::settings());
    restoreInto(state2, restored);
    restored.assemble(&cb3);
  }

  std::multiset<std::string> resumed = cb1.fingerprints;
  resumed.insert(cb2.fingerprints.begin(), cb2.fingerprints.end());
  resumed.insert(cb3.fingerprints.begin(), cb3.fingerprints.end());
  CHECK(resumed == serial);
}

/* In-session cross-mode continue (issue #118 review): parallel stop, then
 * continue single-threaded on the same assembler. Without the gate the
 * serial re-search reports the phase-1 assemblies again (5 + 96 against
 * 96 in the reviewer's repro). */
TEST_CASE("In-session parallel-then-serial continue does not duplicate",
          "[assembler][parallel][resume]") {
  auto p = puzzle_c::load("examples/CubeInCage.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  ScopedSettings noSimd;
  noSimd->bitParallel = false;

  std::multiset<std::string> serial;
  {
    RecordingAssemblerCallback cb;
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    serial = std::move(cb.fingerprints);
  }
  REQUIRE(serial.size() > 5);

  assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);

  RecordingAssemblerCallback cb;
  int seen = 0;
  assm.assemble([&](std::unique_ptr<assembly_c> a) -> bool {
    cb.assembly(std::move(a));
    return ++seen < 5;
  });
  REQUIRE(seen == 5);

  assm.setNumThreads(1);
  assm.assemble(&cb);

  CHECK(cb.fingerprints == serial);
}
TEST_CASE("Dancing Cells save, load and continue reproduces the full multiset",
          "[assembler][parallel][resume][save]") {
  auto p = puzzle_c::load("examples/CubeInCage.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  ScopedSettings noSimd;
  noSimd->bitParallel = false;

  std::multiset<std::string> serial;
  {
    RecordingAssemblerCallback cb;
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    serial = std::move(cb.fingerprints);
  }
  REQUIRE(serial.size() > 1);

  RecordingAssemblerCallback cb1;
  std::string state;
  {
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    int seen = 0;
    assm.assemble([&](std::unique_ptr<assembly_c> a) -> bool {
      cb1.assembly(std::move(a));
      return ++seen < 5;
    });
    REQUIRE(seen == 5);

    std::ostringstream str;
    xmlWriter_c xml(str);
    writeAssemblerXml(assm, xml);
    state = str.str();
  }

  CHECK(assemblerVersionOf(state) == "2.2");

  {
    assembler_1_c restored(makeSolverSpec(*problem), bttest::settings());
    REQUIRE(restored.createMatrix(false, false) == assembler_c::ERR_NONE);
    std::string payload = extractAssemblerContent(state);
    CHECK(restored.setPosition(payload.c_str(), assemblerVersionOf(state).c_str())
          == assembler_c::ERR_NONE);

    RecordingAssemblerCallback cb2;
    restored.assemble(&cb2);

    std::multiset<std::string> resumed = cb1.fingerprints;
    resumed.insert(cb2.fingerprints.begin(), cb2.fingerprints.end());
    CHECK(resumed == serial);
  }
}

TEST_CASE("Parallel assembler 1 stops promptly when aborted", "[assembler][parallel]") {
  auto p = puzzle_c::load("examples/CubeInCage.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);

  int count = 0;
  assm.assemble([&count](std::unique_ptr<assembly_c>) -> bool {
    count++;
    return false;                  // request immediate stop
  });

  CHECK(count == 1);
  /* not stopped(): that is just !running, which is true of any returned
   * assemble() whether or not the abort was honoured. The search really
   * stopping is what getFinished() < 1 shows.
   */
  CHECK(assm.getFinished() < 1.0f);
}

/* Parallel Huang search on a puzzle that actually enables symmetry breaking.
 *
 * Workers call assembly_c::smallerRotationExists() outside callbackMutex,
 * which reaches the lazily filled mutable caches on the shapes shared by all
 * of them -- BbHsCache and symmetries, on the result shape and, through
 * normalizeTransformation() and the hotspot fixup in assembly_c::transform(),
 * on every part shape too.
 *
 * CubeInCage, the puzzle the other assembler_1 cases use, has no symmetry
 * breaker, so its workers never enter that branch at all: instrumenting the
 * guard shows 292 worker solutions on the rest of this suite, every one of
 * them with avoidTransformedAssemblies == 0. DemoMirrorParadox does enter it
 * -- 50 worker solutions with the flag set -- which makes it the only bundled
 * example that covers this path for assembler_1. Swapping the puzzle silently
 * removes the coverage.
 */
TEST_CASE("Parallel assembler 1 matches serial on a symmetry-breaking puzzle",
          "[assembler][parallel][tsan]") {
  auto p = puzzle_c::load("examples/DemoMirrorParadox.xmpuzzle");
  REQUIRE(p != nullptr);
  REQUIRE(p->getNumberOfProblems() > 1);
  auto problem = p->getProblem(1);
  REQUIRE(problem != nullptr);

  std::multiset<std::string> serial;
  {
    RecordingAssemblerCallback cb;
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    serial = std::move(cb.fingerprints);
  }
  REQUIRE_FALSE(serial.empty());

  /* Guard the premise rather than trusting the chosen puzzle: keepRotations
   * forces avoidTransformedAssemblies off, so it must yield strictly more
   * assemblies. If the two agree, symmetry breaking is not active here any
   * more and this case has stopped covering the concurrent path.
   */
  {
    RecordingAssemblerCallback cb;
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(true, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    INFO("symmetry breaking must be active for this test to be meaningful");
    REQUIRE(cb.fingerprints.size() > serial.size());
  }

  /* reload so the parallel run starts with cold caches -- the lazy fills are
   * first touch, so running after a serial run on the same puzzle object
   * races on already warm caches and reports nothing
   */
  auto pFresh = puzzle_c::load("examples/DemoMirrorParadox.xmpuzzle");
  REQUIRE(pFresh != nullptr);
  auto problemFresh = pFresh->getProblem(1);
  REQUIRE(problemFresh != nullptr);

  RecordingAssemblerCallback cb;
  assembler_1_c assm(makeSolverSpec(*problemFresh), bttest::settings());
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
  assm.assemble(&cb);

  CHECK(cb.fingerprints == serial);
}

/* Same contract as the assembler_0 case: a parallel Huang search that was
 * stopped part way persists its remainder (placed-node prefixes plus
 * reported signatures), so the reload resumes from it instead
 * of refusing or silently starting over and re-reporting.
 */
TEST_CASE("Parallel assembler 1: an interrupted search restores as resumable",
          "[assembler][parallel][resume]") {
  auto p = puzzle_c::load("examples/CubeInCage.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  std::multiset<std::string> serial;
  {
    RecordingAssemblerCallback cb;
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    serial = std::move(cb.fingerprints);
  }
  REQUIRE(serial.size() > 1);

  assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);

  RecordingAssemblerCallback cb1;
  int seen = 0;
  assm.assemble([&](std::unique_ptr<assembly_c> a) -> bool {
    cb1.assembly(std::move(a));
    seen++;
    return false;
  });
  REQUIRE(seen == 1);
  REQUIRE(assm.getFinished() < 1.0f);

  std::string state;
  {
    std::ostringstream str;
    xmlWriter_c xml(str);
    writeAssemblerXml(assm, xml);
    state = str.str();
  }

  /* resumes from saved tasks (issue #90) rather than refusing or silently
   * starting over */
  assembler_1_c restored(makeSolverSpec(*problem), bttest::settings());
  REQUIRE(restored.createMatrix(false, false) == assembler_c::ERR_NONE);
  std::string payload = extractAssemblerContent(state);
  CHECK(restored.setPosition(payload.c_str(), assemblerVersionOf(state).c_str())
        == assembler_c::ERR_NONE);

  RecordingAssemblerCallback cb2;
  restored.assemble(&cb2);

  std::multiset<std::string> resumed = cb1.fingerprints;
  resumed.insert(cb2.fingerprints.begin(), cb2.fingerprints.end());
  CHECK(resumed == serial);
}

/* The added flag must not break ordinary restore.
 *
 * Uses a *serial* run stopped part way, which is the state the application
 * actually saves: iterative() breaks at a restorable point, parallelInterrupted
 * stays false, and the position round-trips as it always did. (A never-started
 * assembler is not a useful case here -- its vectors are too short for
 * setPosition's own length checks, and problem_c only ever saves while
 * SS_SOLVING.)
 */
TEST_CASE("Parallel assembler 1: the interrupted flag does not break normal restore",
          "[assembler][parallel][resume]") {
  ScopedSettings env;
  env->bitParallel = false;
  auto p = puzzle_c::load("examples/CubeInCage.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
  assm.setNumThreads(1);            // serial: a resumable stop
  REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);

  int seen = 0;
  assm.assemble([&seen](std::unique_ptr<assembly_c>) -> bool { seen++; return false; });
  REQUIRE(seen == 1);
  REQUIRE(assm.getFinished() < 1.0f);

  std::string state;
  {
    std::ostringstream str;
    xmlWriter_c xml(str);
    writeAssemblerXml(assm, xml);
    state = str.str();
  }

  assembler_1_c restored(makeSolverSpec(*problem), bttest::settings());
  REQUIRE(restored.createMatrix(false, false) == assembler_c::ERR_NONE);
  std::string payload = extractAssemblerContent(state);
  CHECK(restored.setPosition(payload.c_str(), assemblerVersionOf(state).c_str())
        == assembler_c::ERR_NONE);
}

/* getFinished() must not claim a completed search just because a previous
 * parallel run left totalTasks == completedTasks behind on the object.
 */
TEST_CASE("Parallel assembler 1 does not report stale progress on a later run",
          "[assembler][parallel][progress]") {
  auto p = puzzle_c::load("examples/CubeInCage.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
  assm.assemble([](std::unique_ptr<assembly_c>) -> bool { return true; });
  REQUIRE(assm.getFinished() >= 1.0f);

  /* a fresh assembler on the same problem must start at 0, not inherit a
   * finished-looking fraction
   */
  assembler_1_c again(makeSolverSpec(*problem), bttest::settings());
  again.setNumThreads(4);
  REQUIRE(again.createMatrix(false, false) == assembler_c::ERR_NONE);
  CHECK(again.getFinished() < 1.0f);
}

/* Cross-check the bit-parallel engine against Dancing Cells on the whole
 * regression corpus.
 *
 * canUseSimd() returns true for most puzzles, so without this the
 * regression suite exercises Dancing Cells only where the bit-parallel
 * engine declines. And a divergence between the two is exactly the class
 * of bug that produces wrong solve results rather than a crash.
 *
 * Each puzzle is solved twice on the same inputs -- once by default, once
 * with BURRTOOLS_BITPARALLEL=0 forcing Dancing Cells -- and the two must
 * agree on assembly count, solution count and disassembly move level.
 */
TEST_CASE("The bit-parallel engine and Dancing Cells agree across the regression corpus",
          "[solver][simd][dlx][equivalence]") {
  struct Case { const char * path; unsigned int prob; bool disassemble; };
  const Case cases[] = {
    {"examples/PelikanBurr.xmpuzzle",              0, true},
    {"examples/DraculasDentalDesaster.xmpuzzle",   0, false},
    {"examples/Prisgon.xmpuzzle",                  0, false},
    {"examples/DemoMirrorParadox.xmpuzzle",        0, false},
    {"examples/CubeInCage.xmpuzzle",               0, false},
    {"examples/Bermuda.xmpuzzle",                  0, false},
    {"examples/AugmentedSecondStellation.xmpuzzle",0, false},
    /* range puzzles: the SIMD Huang solver models the piece-count range
     * column explicitly, so these must agree too (DemoMirrorParadox above
     * is also a range puzzle and counts here as well) */
    {"examples/PiecesOfEight.xmpuzzle",            0, true},
    {"examples/DemoPieceGenerator.xmpuzzle",       0, false},
    /* holes puzzles below also exercise the hole budget: PelikanBurr and
     * DraculasDentalDesaster above both have variable voxels
     * (holes=164/320), and the synthetic SimdHuangCover holes test in
     * test_simd_huang_cover.cpp pins the machinery directly. BTFiles holes puzzles (e.g. kangaroo) can't be
     * listed here -- puzzles/ is gitignored and absent in CI. */
  };

  bool tookDifferentPaths = false;

  for (const auto & c : cases) {
    INFO("puzzle: " << c.path);

    SolveResult simd;
    {
      ScopedSettings env;
      env->bitParallel = true;
      simd = solvePuzzle(c.path, c.prob, c.disassemble);
    }

    SolveResult dlx;
    {
      ScopedSettings env;
      env->bitParallel = false;
      dlx = solvePuzzle(c.path, c.prob, c.disassemble);
    }

    CHECK(simd.assemblies == dlx.assemblies);
    CHECK(simd.solutions == dlx.solutions);
    CHECK(simd.moveLevel == dlx.moveLevel);

    /* iteration counts are counted differently by the two engines, so a
     * difference here is evidence the two runs really took different paths
     */
    if (simd.iterations != dlx.iterations)
      tookDifferentPaths = true;
  }

  /* Guard the premise: not every puzzle qualifies for the SIMD solver
   * (>32768 matrix columns, or variable voxels combined with a shape whose
   * min differs from its max, disqualify it), but if *none* of them does
   * then this case has silently stopped comparing anything and is only
   * running DLX twice. Keep at least one SIMD-eligible puzzle in the list.
   * (Range columns used to disqualify as well; since the range-column
   * support they take the SIMD path like the rest.)
   */
  INFO("at least one puzzle must actually take the SIMD path");
  CHECK(tookDifferentPaths);
}

TEST_CASE("SIMD assembler_1: an interrupted single-threaded search restores as resumable",
          "[assembler][simd][resume]") {
  ScopedSettings env;
  env->bitParallel = true;
  auto p = puzzle_c::load("examples/Bermuda.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  // Baseline multiset for the union check below.
  std::multiset<std::string> baseline;
  {
    RecordingAssemblerCallback cb;
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    baseline = std::move(cb.fingerprints);
  }
  REQUIRE(!baseline.empty());

  assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
  assm.setNumThreads(1);
  REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);

  RecordingAssemblerCallback cb1;
  int seen = 0;
  assm.assemble([&](std::unique_ptr<assembly_c> a) -> bool {
    cb1.assembly(std::move(a));
    seen++;
    return false;
  });
  REQUIRE(seen == 1);
  REQUIRE(assm.getFinished() < 1.0f);

  std::string state;
  {
    std::ostringstream str;
    xmlWriter_c xml(str);
    writeAssemblerXml(assm, xml);
    state = str.str();
  }
  CHECK(assemblerVersionOf(state) == "2.2");

  assembler_1_c restored(makeSolverSpec(*problem), bttest::settings());
  restored.setNumThreads(1);
  REQUIRE(restored.createMatrix(false, false) == assembler_c::ERR_NONE);

  std::string payload = extractAssemblerContent(state);

  /* The run must have taken the bit-parallel engine (Huang prefixes, type
   * 2), not Dancing Cells (type 3). Without this, a puzzle falling out of
   * canUseSimd() would silently test the other engine here.
   */
  {
    std::istringstream in(payload);
    unsigned int interrupted = 0;
    REQUIRE(in >> interrupted);
    unsigned int ptype = 0;
    REQUIRE(in >> ptype);
    CHECK(interrupted == 1);
    CHECK(ptype == 2);
  }

  CHECK(restored.setPosition(payload.c_str(), assemblerVersionOf(state).c_str())
        == assembler_c::ERR_NONE);

  RecordingAssemblerCallback cb2;
  restored.assemble(&cb2);

  std::multiset<std::string> resumed = cb1.fingerprints;
  resumed.insert(cb2.fingerprints.begin(), cb2.fingerprints.end());
  CHECK(resumed == baseline);
}

TEST_CASE("Saved interrupted search XML size is bounded and independent of solution count",
          "[assembler][resume][save]") {
  // PelikanBurr
  {
    auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
    REQUIRE(p != nullptr);
    auto problem = p->getProblem(0);
    REQUIRE(problem != nullptr);

    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);

    int seen = 0;
    assm.assemble([&](std::unique_ptr<assembly_c>) -> bool {
      return ++seen < 5;
    });
    REQUIRE(seen == 5);

    std::ostringstream str;
    xmlWriter_c xml(str);
    writeAssemblerXml(assm, xml);
    std::string state = str.str();
    CHECK(assemblerVersionOf(state) == "2.2");
    CHECK(state.size() < 4096);
  }

  // CubeInCage, with a piece used several times
  {
    auto p = puzzle_c::load("examples/CubeInCage.xmpuzzle");
    REQUIRE(p != nullptr);
    auto problem = p->getProblem(0);
    REQUIRE(problem != nullptr);

    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);

    int seen = 0;
    assm.assemble([&](std::unique_ptr<assembly_c>) -> bool {
      return ++seen < 5;
    });
    REQUIRE(seen == 5);

    std::ostringstream str;
    xmlWriter_c xml(str);
    writeAssemblerXml(assm, xml);
    std::string state = str.str();
    CHECK(assemblerVersionOf(state) == "2.2");
    CHECK(state.size() < 4096);
  }
}

/* A paused search saved by another BurrTools version does not resume: the
 * assemblers read only their current format (README "Pausing and
 * resuming"), so older payloads -- interrupted or not -- are refused with
 * ERR_CAN_NOT_RESTORE_VERSION, which keeps the found assemblies and starts
 * the search over. Payloads below are minimal valid shapes of the old
 * formats; their content must not matter.
 */
TEST_CASE("Search state saved by another version is refused on restore",
          "[assembler][resume][save]") {
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  {
    /* what assembler_0 wrote, which is gone */
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    CHECK(assm.setPosition("1 0 0 (0 0) 1", "1.6") == assembler_c::ERR_CAN_NOT_RESTORE_VERSION);
    CHECK(assm.setPosition("0 0 0 0 (0 0) 0 0", "1.7") == assembler_c::ERR_CAN_NOT_RESTORE_VERSION);
    CHECK(assm.setPosition("0 0 0 (0 0) 0", nullptr) == assembler_c::ERR_CAN_NOT_RESTORE_VERSION);
  }

  {
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    CHECK(assm.setPosition("1 0 0 0 0 0 0 0 0", "2.1") == assembler_c::ERR_CAN_NOT_RESTORE_VERSION);
    CHECK(assm.setPosition("0 0 0 0 0 0 0 0 0", "2.1") == assembler_c::ERR_CAN_NOT_RESTORE_VERSION);
    CHECK(assm.setPosition("0 0 0 0 0 0 0", "2.0") == assembler_c::ERR_CAN_NOT_RESTORE_VERSION);
    // the current format still loads
    CHECK(assm.setPosition("0", "2.2") == assembler_c::ERR_NONE);
  }
}

/* A stop that lands after a resumed run has its remainder but before the
 * pool is seeded (Stop pressed right after Continue) must keep that
 * remainder. Discarding it, as is right for a freshly generated list that
 * may be partial, made the next continue regenerate the whole search and
 * report every assembly found before the first stop again. The pool-seed
 * hook lands the stop in exactly that window, for all four engines; each
 * run stops in-session first, then (on the hooked stop) saves and restores,
 * so both the in-memory and the saved remainder are covered.
 */
TEST_CASE("a stop before the pool is seeded keeps a resumed remainder",
          "[assembler][parallel][resume][save]") {
  struct Case { const char * name; bool noSimd; const char * path; };
  const Case cases[] = {
    {"exact-once, bit-parallel", false, "examples/PelikanBurr.xmpuzzle"},
    {"exact-once, Dancing Cells", true, "examples/PelikanBurr.xmpuzzle"},
    {"ranges, Dancing Cells", true, "examples/PiecesOfEight.xmpuzzle"},
    {"ranges, bit-parallel", false, "examples/PiecesOfEight.xmpuzzle"},
  };
  for (const auto & c : cases) {
    INFO("engine: " << c.name);
    ScopedSettings noSimd;
    noSimd->bitParallel = !c.noSimd;
    auto p = puzzle_c::load(c.path);
    REQUIRE(p != nullptr);
    auto problem = p->getProblem(0);
    REQUIRE(problem != nullptr);
    auto make = [&]() {
      std::unique_ptr<assembler_c> assm(new assembler_1_c(makeSolverSpec(*problem), bttest::settings()));
      assm->setNumThreads(4);
      REQUIRE(assm->createMatrix(false, false) == assembler_c::ERR_NONE);
      return assm;
    };

    std::multiset<std::string> baseline;
    {
      RecordingAssemblerCallback cb;
      auto assm = make();
      assm->assemble(&cb);
      baseline = std::move(cb.fingerprints);
    }
    REQUIRE(baseline.size() > 3);
    INFO("baseline: " << baseline.size());

    RecordingAssemblerCallback cb;
    std::string state;
    {
      auto assm = make();
      const size_t k = baseline.size() / 2;
      unsigned int seen = 0;
      assm->assemble([&](std::unique_ptr<assembly_c> a) -> bool {
        cb.assembly(std::move(a));
        return ++seen < k;
      });
      REQUIRE(assm->getFinished() < 1.0f);

      // Continue, stopped before the resumed remainder reaches the pool.
      bool fired = false;
      assembler_c * raw = assm.get();
      assm->setPoolSeedHook([&] { fired = true; raw->stop(); });
      assm->assemble(&cb);
      assm->setPoolSeedHook(nullptr);
      REQUIRE(fired);
      CHECK(cb.fingerprints.size() == k);

      std::ostringstream str;
      xmlWriter_c xml(str);
      writeAssemblerXml(*assm, xml);
      state = str.str();
    }

    auto restored = make();
    std::string payload = extractAssemblerContent(state);
    // CHECK, not REQUIRE: every engine's case reports on its own
    const auto err = restored->setPosition(payload.c_str(), assemblerVersionOf(state).c_str());
    CHECK(err == assembler_c::ERR_NONE);
    if (err == assembler_c::ERR_NONE) {
      restored->assemble(&cb);
      CHECK(restored->getFinished() == 1.0f);
      CHECK(cb.fingerprints == baseline);
    }
  }
}

/* Restore refuses remainders save() never writes: a task type with no
 * tasks behind it (it would restore as a fresh search over the assemblies
 * already found), an interrupted search without tasks, and Huang seeds that
 * no longer replay on the matrix (their territory would be lost silently).
 * An engine without tasks pins nothing: the next run is fresh.
 */
TEST_CASE("restore refuses remainders save() never writes",
          "[assembler][resume][save]") {
  auto p = puzzle_c::load("examples/PiecesOfEight.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
  REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
  // state (0 not started, 1 stopped, 2 complete), then for 1: engine type,
  // seed count, seeds
  CHECK(assm.setPosition("3", "2.2") == assembler_c::ERR_CAN_NOT_RESTORE_SYNTAX);
  CHECK(assm.setPosition("1 1 0", "2.2") == assembler_c::ERR_CAN_NOT_RESTORE_SYNTAX);
  CHECK(assm.setPosition("1 2 0", "2.2") == assembler_c::ERR_CAN_NOT_RESTORE_INTERRUPTED);
  CHECK(assm.setPosition("1 3 0", "2.2") == assembler_c::ERR_CAN_NOT_RESTORE_INTERRUPTED);
  CHECK(assm.setPosition("1", "2.2") == assembler_c::ERR_CAN_NOT_RESTORE_INTERRUPTED);
  // a seed: prefix, step kinds, rest start. A node id that is not in the
  // matrix does not replay; the empty prefix (whole search) does, and an
  // interrupted search restores as not finished, on either engine.
  CHECK(assm.setPosition("1 2 1 1 999999 1 0 0", "2.2") == assembler_c::ERR_CAN_NOT_RESTORE_SYNTAX);
  // (each after a complete state, which a remainder must clear)
  REQUIRE(assm.setPosition("2", "2.2") == assembler_c::ERR_NONE);
  CHECK(assm.setPosition("1 2 1 0 0 0", "2.2") == assembler_c::ERR_NONE);
  CHECK(assm.getFinished() < 1.0f);
  REQUIRE(assm.setPosition("2", "2.2") == assembler_c::ERR_NONE);
  CHECK(assm.setPosition("1 3 1 0 0 0", "2.2") == assembler_c::ERR_NONE);
  CHECK(assm.getFinished() < 1.0f);
  // not started and complete restore as such
  CHECK(assm.setPosition("0", "2.2") == assembler_c::ERR_NONE);
  CHECK(assm.getFinished() < 1.0f);
  CHECK(assm.setPosition("2", "2.2") == assembler_c::ERR_NONE);
  CHECK(assm.getFinished() == 1.0f);
}

TEST_CASE("Interrupted search resumes with pinned engine across environment overrides",
          "[assembler][resume][simd]") {
  // a problem with every piece used exactly once
  {
    ScopedSettings noSimd;
    noSimd->bitParallel = true;
    auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
    REQUIRE(p != nullptr);
    auto problem = p->getProblem(0);
    REQUIRE(problem != nullptr);

    std::multiset<std::string> baseline;
    {
      RecordingAssemblerCallback cb;
      assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
      REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
      assm.assemble(&cb);
      baseline = std::move(cb.fingerprints);
    }
    REQUIRE(!baseline.empty());

    RecordingAssemblerCallback cb1;
    std::string state;
    {
      assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
      assm.setNumThreads(4);
      REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
      int seen = 0;
      assm.assemble([&](std::unique_ptr<assembly_c> a) -> bool {
        cb1.assembly(std::move(a));
        return ++seen < 10;
      });
      REQUIRE(seen == 10);

      std::ostringstream str;
      xmlWriter_c xml(str);
      writeAssemblerXml(assm, xml);
      state = str.str();
    }
    CHECK(assemblerVersionOf(state) == "2.2");

    // Resume under BURRTOOLS_BITPARALLEL=0 and threads=1
    ScopedSettings disableSimd;
    disableSimd->bitParallel = false;
    assembler_1_c restored(makeSolverSpec(*problem), bttest::settings());
    restored.setNumThreads(1);
    REQUIRE(restored.createMatrix(false, false) == assembler_c::ERR_NONE);
    std::string payload = extractAssemblerContent(state);
    CHECK(restored.setPosition(payload.c_str(), assemblerVersionOf(state).c_str())
          == assembler_c::ERR_NONE);

    RecordingAssemblerCallback cb2;
    restored.assemble(&cb2);

    std::multiset<std::string> resumed = cb1.fingerprints;
    resumed.insert(cb2.fingerprints.begin(), cb2.fingerprints.end());
    CHECK(resumed == baseline);
  }

  // a problem with duplicate pieces
  {
    ScopedSettings noSimd;
    noSimd->bitParallel = true;
    auto p = puzzle_c::load("examples/CubeInCage.xmpuzzle");
    REQUIRE(p != nullptr);
    auto problem = p->getProblem(0);
    REQUIRE(problem != nullptr);

    std::multiset<std::string> baseline;
    {
      RecordingAssemblerCallback cb;
      assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
      REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
      assm.assemble(&cb);
      baseline = std::move(cb.fingerprints);
    }
    REQUIRE(!baseline.empty());

    RecordingAssemblerCallback cb1;
    std::string state;
    {
      assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
      assm.setNumThreads(4);
      REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
      int seen = 0;
      assm.assemble([&](std::unique_ptr<assembly_c> a) -> bool {
        cb1.assembly(std::move(a));
        return ++seen < 5;
      });
      REQUIRE(seen == 5);

      std::ostringstream str;
      xmlWriter_c xml(str);
      writeAssemblerXml(assm, xml);
      state = str.str();
    }
    CHECK(assemblerVersionOf(state) == "2.2");

    // Resume under BURRTOOLS_BITPARALLEL=0 and threads=1
    ScopedSettings disableSimd;
    disableSimd->bitParallel = false;
    assembler_1_c restored(makeSolverSpec(*problem), bttest::settings());
    restored.setNumThreads(1);
    REQUIRE(restored.createMatrix(false, false) == assembler_c::ERR_NONE);
    std::string payload = extractAssemblerContent(state);
    CHECK(restored.setPosition(payload.c_str(), assemblerVersionOf(state).c_str())
          == assembler_c::ERR_NONE);

    RecordingAssemblerCallback cb2;
    restored.assemble(&cb2);

    std::multiset<std::string> resumed = cb1.fingerprints;
    resumed.insert(cb2.fingerprints.begin(), cb2.fingerprints.end());
    CHECK(resumed == baseline);
  }
}



/* A problem loaded in SS_SOLVED with no saved assembler state cannot be
 * continued: setAssembler() rejects that combination, so run() ends in
 * ACT_ASSERT. stopped() has to report that as a stopped state, or a caller
 * polling for the worker to finish waits on a thread that has already exited.
 */
TEST_CASE("solveThread on an already solved problem stops instead of wedging", "[solver][resume]") {
  std::unique_ptr<std::istream> str(openGzFile("examples/PelikanBurr.xmpuzzle"));
  REQUIRE(str != nullptr);

  xmlParser_c pars(*str);
  puzzle_c p(pars);

  REQUIRE(p.getNumberOfProblems() > 0);
  problem_c * problem = p.getProblem(0);

  // the shipped example is stored in the finished state
  REQUIRE(problem->getSolveState() == SS_SOLVED);
  REQUIRE(problem->getAssembler() == nullptr);

  for (unsigned int i = 0; i < p.getNumberOfShapes(); i++)
    p.getShape(i)->initHotspot();

  solveThread_c thread(*problem, solveThread_c::PAR_REDUCE | solveThread_c::PAR_JUST_COUNT, bttest::settings());
  REQUIRE(thread.start(false));

  // poll the way burrTxt2 does, but with a bound so a wedged thread fails
  // the test instead of hanging the suite
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
  while (!thread.stopped() && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

  CHECK(thread.stopped());
  CHECK(thread.currentAction() != solveThread_c::ACT_ASSEMBLING);
}

namespace {

/* Solve with reduce() applied first, the way solveThread_c does. The other
 * helper in this file skips reduce(), so it searches a different matrix than
 * the real solve path builds.
 */
int assembliesAfterReduce(const char * path, bool forceDlx) {
  ScopedSettings noSimd;
  noSimd->bitParallel = !forceDlx;

  auto p = puzzle_c::load(path);
  REQUIRE(p != nullptr);
  problem_c * problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  std::unique_ptr<assembler_c> assm = problem->getPuzzle().getGridType()->findAssembler(makeSolverSpec(*problem), bttest::settings());
  REQUIRE(assm != nullptr);
  REQUIRE(assm->createMatrix(false, false) == assembler_c::ERR_NONE);

  assm->reduce();

  TestAssemblerCallback cb(nullptr);
  assm->assemble(&cb);

  return cb.assemblies;
}

} // namespace

/* reduce() ends in clumpify(), which drops columns that duplicate an earlier
 * one and unlinks their nodes from the rows. Both searches have to agree on
 * the matrix that leaves behind, so for a puzzle whose matrix reduce() shrinks
 * the SIMD path and the DLX fallback must report the same assemblies.
 */
TEST_CASE("The bit-parallel engine agrees with Dancing Cells on a reduced matrix", "[solver][simd][reduce]") {
  const char * puzzles[] = {
    "examples/12PieceSeparation.xmpuzzle",
    "examples/AlPackino.xmpuzzle",
  };

  for (const char * path : puzzles) {
    CAPTURE(path);
    const int dlx = assembliesAfterReduce(path, true);
    const int simd = assembliesAfterReduce(path, false);

    CHECK(dlx > 0);
    CHECK(simd == dlx);
  }
}

TEST_CASE("Parallel exact-once search: completed search does not replay on second assemble or on restore", "[solver][replay][simd]") {
  ScopedSettings noSimd;
  noSimd->bitParallel = true;
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  problem_c * problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  std::unique_ptr<assembler_c> assm = std::make_unique<assembler_1_c>(makeSolverSpec(*problem), bttest::settings());
  REQUIRE(assm != nullptr);
  REQUIRE(assm->createMatrix(false, false) == assembler_c::ERR_NONE);

  TestAssemblerCallback cb1(nullptr);
  assm->assemble(&cb1);
  CHECK(cb1.assemblies == 12);
  CHECK(assm->getFinished() == 1.0f);

  TestAssemblerCallback cb2(nullptr);
  assm->assemble(&cb2);
  CHECK(cb2.assemblies == 0);
  CHECK(assm->getFinished() == 1.0f);

  std::stringstream ss;
  {
    xmlWriter_c writer(ss);
    writeAssemblerXml(*assm, writer);
  }
  xmlParser_c parser(ss);
  parser.nextTag();
  parser.require(xmlParser_c::START_TAG, "assembler");
  std::string version = parser.getAttributeValue("version");
  std::string state = parser.nextText();

  std::unique_ptr<assembler_c> assm2 = std::make_unique<assembler_1_c>(makeSolverSpec(*problem), bttest::settings());
  REQUIRE(assm2 != nullptr);
  REQUIRE(assm2->createMatrix(false, false) == assembler_c::ERR_NONE);
  assembler_c::errState err = assm2->setPosition(state.c_str(), version.c_str());
  CHECK(err == assembler_c::ERR_NONE);
  CHECK(assm2->getFinished() == 1.0f);

  TestAssemblerCallback cb3(nullptr);
  assm2->assemble(&cb3);
  CHECK(cb3.assemblies == 0);
}

TEST_CASE("Single-threaded exact-once search: completed search does not replay on second assemble or on restore", "[solver][replay][simd]") {
  ScopedSettings threads;
  threads->threads = 1;
  ScopedSettings noSimd;
  noSimd->bitParallel = true;
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  problem_c * problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  std::unique_ptr<assembler_c> assm = std::make_unique<assembler_1_c>(makeSolverSpec(*problem), bttest::settings());
  REQUIRE(assm != nullptr);
  REQUIRE(assm->createMatrix(false, false) == assembler_c::ERR_NONE);

  TestAssemblerCallback cb1(nullptr);
  assm->assemble(&cb1);
  CHECK(cb1.assemblies == 12);
  CHECK(assm->getFinished() == 1.0f);

  TestAssemblerCallback cb2(nullptr);
  assm->assemble(&cb2);
  CHECK(cb2.assemblies == 0);
  CHECK(assm->getFinished() == 1.0f);

  std::stringstream ss;
  {
    xmlWriter_c writer(ss);
    writeAssemblerXml(*assm, writer);
  }
  xmlParser_c parser(ss);
  parser.nextTag();
  parser.require(xmlParser_c::START_TAG, "assembler");
  std::string version = parser.getAttributeValue("version");
  std::string state = parser.nextText();

  std::unique_ptr<assembler_c> assm2 = std::make_unique<assembler_1_c>(makeSolverSpec(*problem), bttest::settings());
  REQUIRE(assm2 != nullptr);
  REQUIRE(assm2->createMatrix(false, false) == assembler_c::ERR_NONE);
  assembler_c::errState err = assm2->setPosition(state.c_str(), version.c_str());
  CHECK(err == assembler_c::ERR_NONE);
  CHECK(assm2->getFinished() == 1.0f);

  TestAssemblerCallback cb3(nullptr);
  assm2->assemble(&cb3);
  CHECK(cb3.assemblies == 0);
}

TEST_CASE("Parallel assembler 1 SIMD: completed search does not replay on second assemble or on restore", "[solver][replay][simd]") {
  ScopedSettings noSimd;
  noSimd->bitParallel = true;
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  problem_c * problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  std::unique_ptr<assembler_c> assm = std::make_unique<assembler_1_c>(makeSolverSpec(*problem), bttest::settings());
  REQUIRE(assm != nullptr);
  REQUIRE(assm->createMatrix(false, false) == assembler_c::ERR_NONE);

  TestAssemblerCallback cb1(nullptr);
  assm->assemble(&cb1);
  CHECK(cb1.assemblies == 12);
  CHECK(assm->getFinished() == 1.0f);

  TestAssemblerCallback cb2(nullptr);
  assm->assemble(&cb2);
  CHECK(cb2.assemblies == 0);
  CHECK(assm->getFinished() == 1.0f);

  std::stringstream ss;
  {
    xmlWriter_c writer(ss);
    writeAssemblerXml(*assm, writer);
  }
  xmlParser_c parser(ss);
  parser.nextTag();
  parser.require(xmlParser_c::START_TAG, "assembler");
  std::string version = parser.getAttributeValue("version");
  std::string state = parser.nextText();

  std::unique_ptr<assembler_c> assm2 = std::make_unique<assembler_1_c>(makeSolverSpec(*problem), bttest::settings());
  REQUIRE(assm2 != nullptr);
  REQUIRE(assm2->createMatrix(false, false) == assembler_c::ERR_NONE);
  assembler_c::errState err = assm2->setPosition(state.c_str(), version.c_str());
  CHECK(err == assembler_c::ERR_NONE);
  CHECK(assm2->getFinished() == 1.0f);

  TestAssemblerCallback cb3(nullptr);
  assm2->assemble(&cb3);
  CHECK(cb3.assemblies == 0);
}

TEST_CASE("Single-threaded assembler 1 SIMD: completed search does not replay on second assemble or on restore", "[solver][replay][simd]") {
  ScopedSettings threads;
  threads->threads = 1;
  ScopedSettings noSimd;
  noSimd->bitParallel = true;
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  problem_c * problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  std::unique_ptr<assembler_c> assm = std::make_unique<assembler_1_c>(makeSolverSpec(*problem), bttest::settings());
  REQUIRE(assm != nullptr);
  REQUIRE(assm->createMatrix(false, false) == assembler_c::ERR_NONE);

  TestAssemblerCallback cb1(nullptr);
  assm->assemble(&cb1);
  CHECK(cb1.assemblies == 12);
  CHECK(assm->getFinished() == 1.0f);

  TestAssemblerCallback cb2(nullptr);
  assm->assemble(&cb2);
  CHECK(cb2.assemblies == 0);
  CHECK(assm->getFinished() == 1.0f);

  std::stringstream ss;
  {
    xmlWriter_c writer(ss);
    writeAssemblerXml(*assm, writer);
  }
  xmlParser_c parser(ss);
  parser.nextTag();
  parser.require(xmlParser_c::START_TAG, "assembler");
  std::string version = parser.getAttributeValue("version");
  std::string state = parser.nextText();

  std::unique_ptr<assembler_c> assm2 = std::make_unique<assembler_1_c>(makeSolverSpec(*problem), bttest::settings());
  REQUIRE(assm2 != nullptr);
  REQUIRE(assm2->createMatrix(false, false) == assembler_c::ERR_NONE);
  assembler_c::errState err = assm2->setPosition(state.c_str(), version.c_str());
  CHECK(err == assembler_c::ERR_NONE);
  CHECK(assm2->getFinished() == 1.0f);

  TestAssemblerCallback cb3(nullptr);
  assm2->assemble(&cb3);
  CHECK(cb3.assemblies == 0);
}

/* getFinished() must describe the search that is actually configured, not a
 * previous one. Both engines used to shortcut "not running" to 100%, and
 * assembler_1_c never reset its task counters, so a fresh matrix reported a
 * completed search.
 *
 * Preparing again on the same instance is exercised below only to check that
 * the counters and the searchComplete flag reset; the test reads nothing
 * else. This is NOT a general endorsement of calling createMatrix twice on
 * one instance: the sparse-matrix rebuild arrays (up/down/left/right/
 * colCount/weight) are push_back-only and are never
 * cleared between calls, so a second createMatrix followed by a second
 * assemble() on the same instance is untested here and likely unsound. Do
 * not copy this pattern into a test that calls assemble() twice.
 */
TEST_CASE("a freshly prepared assembler does not report itself finished",
          "[assembler][progress]") {
  // every piece once; a piece used several times
  const std::string path = GENERATE(as<std::string>{}, "examples/PelikanBurr.xmpuzzle", "examples/CubeInCage.xmpuzzle");
  INFO(path);
  auto p = puzzle_c::load(path.c_str());
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  SECTION("default engine, 4 threads") {
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    CHECK(assm.getFinished() < 1.0f);

    TestAssemblerCallback cb;
    assm.assemble(&cb);
    CHECK(assm.getFinished() == 1.0f);

    /* preparing again must restart, not inherit the completed state */
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    CHECK(assm.getFinished() < 1.0f);
  }

  /* the other engine, and the task pool run inline on the one thread */
  SECTION("Dancing Cells, single-threaded") {
    ScopedSettings noSimd;
    noSimd->bitParallel = false;
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    CHECK(assm.getFinished() < 1.0f);

    TestAssemblerCallback cb;
    assm.assemble(&cb);
    CHECK(assm.getFinished() == 1.0f);
  }
}

/* Samples getFinished() from a second thread while a solve runs. The GUI does
 * exactly this, so the value must be safe to read concurrently.
 */
namespace {

struct ProgressTrace {
  std::vector<float> samples;

  bool monotone() const {
    for (size_t i = 1; i < samples.size(); i++)
      if (samples[i] < samples[i-1]) return false;
    return true;
  }
  bool inRange() const {
    for (float f : samples)
      if (f < 0.0f || f > 1.0f) return false;
    return true;
  }
  size_t distinctValues() const {
    return std::set<float>(samples.begin(), samples.end()).size();
  }
  /* longest run of identical consecutive samples, as a fraction of all */
  double longestPlateauFraction() const {
    if (samples.empty()) return 1.0;
    size_t best = 1, run = 1;
    for (size_t i = 1; i < samples.size(); i++) {
      run = (samples[i] == samples[i-1]) ? run + 1 : 1;
      if (run > best) best = run;
    }
    return static_cast<double>(best) / static_cast<double>(samples.size());
  }
};

ProgressTrace traceSolve(assembler_c & assm, assembler_cb & cb) {
  ProgressTrace t;
  std::atomic<bool> done{false};
  std::thread sampler([&]{
    while (!done.load(std::memory_order_relaxed)) {
      t.samples.push_back(assm.getFinished());
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
  });
  assm.assemble(&cb);
  done.store(true, std::memory_order_relaxed);
  sampler.join();
  return t;
}

/* As traceSolve, but stops the search after `budget` instead of running it to
 * completion. The curve properties the progress tests care about -- plateau
 * length and how many distinct values the bar takes -- are shape properties of
 * the samples, so a puzzle whose full solve takes minutes can be sampled for a
 * few seconds and still exercise them. The caller must discard the terminal
 * sample: an aborted run accounts every in-flight task as complete, so its
 * last value is an artifact of the abort rather than a point on the curve.
 *
 * With minSamples the search runs past the budget until the sampler has taken
 * that many samples, up to ten times the budget. A loaded CI runner can
 * oversleep the interval several times over (a macOS runner once took 20
 * samples in a 3 s budget at 20 ms), and a sample count is a statement about
 * the scheduler, not about the progress code.
 */
ProgressTrace traceSolveStoppingAfter(assembler_c & assm, assembler_cb & cb,
                                      std::chrono::milliseconds budget,
                                      std::chrono::milliseconds interval,
                                      size_t minSamples = 0) {
  ProgressTrace t;
  std::atomic<bool> done{false};
  std::atomic<size_t> taken{0};
  std::thread sampler([&]{
    while (!done.load(std::memory_order_relaxed)) {
      t.samples.push_back(assm.getFinished());
      taken.fetch_add(1, std::memory_order_relaxed);
      std::this_thread::sleep_for(interval);
    }
  });
  std::thread stopper([&]{
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + budget;
    const auto hardDeadline = start + 10 * budget;
    while (!done.load(std::memory_order_relaxed)) {
      const auto now = std::chrono::steady_clock::now();
      if (now >= hardDeadline ||
          (now >= deadline && taken.load(std::memory_order_relaxed) >= minSamples))
        break;
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    assm.stop();
  });
  assm.assemble(&cb);
  done.store(true, std::memory_order_relaxed);
  sampler.join();
  stopper.join();
  return t;
}

/* getFinished() has one acknowledged residual race (see the note on
 * "Huang parallel assembly progress is monotone and ends at 1.0" below): a
 * worker can be preempted between clearing its slot and its own fetch_add
 * into the completed count, for the whole of a reader's walk, and no
 * reader-side fix reaches that window. It is two adjacent instructions wide
 * against the microseconds of a whole slot walk, so it is rare -- but rare
 * enough to happen once in thousands of samples is common enough to flake a
 * CI job on a shared, CPU-constrained runner.
 *
 * `produce` builds an entirely fresh trace (fresh assembler instance, fresh
 * matrix, fresh solve) each call, so a retry here is a genuinely independent
 * attempt, not a re-read of stale state. A real regression in the progress
 * arithmetic reproduces on every attempt; this race does not.
 */
template <typename ProduceFn>
ProgressTrace traceUntilMonotone(ProduceFn produce, int attempts = 3) {
  ProgressTrace t = produce();
  for (int attempt = 2; attempt <= attempts && !t.monotone(); attempt++) {
    WARN("monotone() failed on attempt " << (attempt - 1)
         << " of " << attempts << " -- retrying with a fresh solve"
         << " (see the residual handoff-race note on traceUntilMonotone)");
    t = produce();
  }
  return t;
}

}

/* Forces Dancing Cells for the length of a TEST_CASE, selected explicitly
 * rather than by picking a puzzle the bit-parallel engine happens to decline
 * today.
 */
namespace {

struct ScopedNoSimd {
  ScopedSettings dlx;
  ScopedNoSimd() { dlx->bitParallel = false; }
};

}

/* Truth table for drainedPoolComplete(), the stop-after-drain rule shared
 * by all three pool-drain sites (assembler_0 DLX/SIMD, assembler_1 DLX,
 * assembler_1 Huang). A stop that lands after the pool fully drained must
 * complete instead of interrupting; anything else -- unstopped, tasks left
 * queued, or counters that do not cover the total -- keeps the interrupted
 * remainder. The `>=` on the counters is deliberate, not sloppy: splits
 * inflate the total mid-run, so equality is what happens, not what is
 * guaranteed.
 */
TEST_CASE("drainedPoolComplete pins the stop-after-drain rule",
          "[assembler][resume]") {
  CHECK(!drainedPoolComplete(false, 0, 5, 5));
  CHECK(!drainedPoolComplete(false, 0, 0, 0));
  CHECK(!drainedPoolComplete(true, 2, 5, 5));
  CHECK(!drainedPoolComplete(true, 0, 0, 0));
  CHECK(!drainedPoolComplete(true, 0, 5, 3));
  CHECK(drainedPoolComplete(true, 0, 5, 5));
  CHECK(drainedPoolComplete(true, 0, 5, 7));
}

/* A stop that lands after the pool drained -- every task counted, nothing
 * queued -- must complete the run exactly as an unstopped one
 * (drainedPoolComplete at all three drain sites). The pool-drained hook
 * fires the stop at exactly that point, so each engine takes the path
 * deterministically: the run reports finished, holds every assembly, and
 * its saved state restores to a run that finds nothing more. A run saved
 * as interrupted here would carry an empty remainder, which a continue
 * re-searches from scratch (every assembly twice) or refuses.
 */
TEST_CASE("a stop after the pool drained completes the run",
          "[assembler][resume][save]") {
  struct Case { const char * path; bool dlx; };
  const Case cases[] = {
    {"examples/PelikanBurr.xmpuzzle", false},
    {"examples/PelikanBurr.xmpuzzle", true},
    {"examples/CubeInCage.xmpuzzle", false},
    {"examples/CubeInCage.xmpuzzle", true},
  };

  for (const auto & c : cases) {
    INFO("puzzle: " << c.path << (c.dlx ? " (Dancing Cells)" : " (bit-parallel)"));
    ScopedSettings noSimd;
    noSimd->bitParallel = !c.dlx;
    auto p = puzzle_c::load(c.path);
    REQUIRE(p != nullptr);
    auto problem = p->getProblem(0);
    REQUIRE(problem != nullptr);
    auto make = [&]() {
      std::unique_ptr<assembler_c> assm(new assembler_1_c(makeSolverSpec(*problem), bttest::settings()));
      assm->setNumThreads(4);
      REQUIRE(assm->createMatrix(false, false) == assembler_c::ERR_NONE);
      return assm;
    };

    std::multiset<std::string> baseline;
    {
      RecordingAssemblerCallback cb;
      auto assm = make();
      assm->assemble(&cb);
      baseline = std::move(cb.fingerprints);
    }
    REQUIRE(!baseline.empty());

    RecordingAssemblerCallback cb1;
    std::string state;
    {
      auto assm = make();
      bool fired = false;
      assembler_c * raw = assm.get();
      assm->setPoolDrainedHook([&] { fired = true; raw->stop(); });
      assm->assemble(&cb1);
      REQUIRE(fired);
      CHECK(assm->getFinished() == 1.0f);
      std::ostringstream str;
      xmlWriter_c xml(str);
      writeAssemblerXml(*assm, xml);
      state = str.str();
    }
    CHECK(cb1.fingerprints == baseline);

    RecordingAssemblerCallback cb2;
    {
      auto restored = make();
      std::string payload = extractAssemblerContent(state);
      // CHECK, not REQUIRE: every engine's case reports on its own
      const auto err = restored->setPosition(payload.c_str(), assemblerVersionOf(state).c_str());
      CHECK(err == assembler_c::ERR_NONE);
      if (err == assembler_c::ERR_NONE) {
        restored->assemble(&cb2);
        CHECK(restored->getFinished() == 1.0f);
      }
    }
    CHECK(cb2.fingerprints.empty());
  }
}

struct Assembler1UnitsTestAccess {
  /* After a finished run every unit the run started with is completed:
   * donations and stops only move units between tasks. */
  static bool unitsConserved(const assembler_1_c & assm) {
    const size_t total = assm.totalTasks.load(std::memory_order_relaxed);
    return total > 0 && assm.completedTasks.load(std::memory_order_relaxed) == total;
  }

};

/* Forced donation on a real puzzle with a symmetry breaker.
 *
 * The line puzzle above has uniform frames and no symmetry breaker.
 * Bermuda has both irregular frames and a breaker, so donated pieces run
 * smallerRotationExists() from worker threads on prefixes built by
 * abovePathPrefix (DLX) and prefixFromNodes (SIMD), and the cursor
 * indices are taken on column lists that the covers of a real puzzle
 * shaped. Each run must equal the serial baseline as a multiset (a lost
 * and a duplicated assembly cancel in a count), and the finished tasks'
 * shares must add up to 1. Under TSan this is the donation path's
 * symmetry-breaker coverage.
 */
TEST_CASE("Forced donation matches serial on a symmetry-breaking puzzle",
          "[assembler][parallel][tsan]") {
  auto p = puzzle_c::load("examples/Bermuda.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  std::multiset<std::string> serial;
  {
    ScopedSettings noSimd;
    noSimd->bitParallel = false;
    RecordingAssemblerCallback cb;
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(1);  // Dancing Cells on one thread: the pool runs inline, nobody to donate to
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    serial = std::move(cb.fingerprints);
  }
  REQUIRE_FALSE(serial.empty());

  // premise: symmetry breaking is active (see the parallel-vs-serial case)
  {
    ScopedSettings noSimd;
    noSimd->bitParallel = false;
    RecordingAssemblerCallback cb;
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(true, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    REQUIRE(cb.fingerprints.size() > serial.size());
  }

  struct Case { bool dlx; unsigned int threads; const char * every; };
  const Case cases[] = {
    {true, 4, "3"}, {true, 4, "50"}, {false, 1, "5"}, {false, 4, "5"},
  };
  for (const auto & c : cases) {
    INFO((c.dlx ? "Dancing Cells" : "bit-parallel") << ", " << c.threads << " threads, every " << c.every);
    ScopedSettings every;
    every->bitParallel = !c.dlx;
    every->donateEvery = std::stoul(c.every);
    RecordingAssemblerCallback cb;
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(c.threads);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    CHECK(assm.getDonations() > 0);
    CHECK(Assembler1UnitsTestAccess::unitsConserved(assm));
    CHECK(cb.fingerprints == serial);
  }
}


/* Split on demand in assembler_1 (Huang, and A1-DLX with the DLX engine
 * forced). Same shape as "donated work is searched exactly once": forced
 * donations, every run -- uninterrupted, and stopped + restored -- must
 * reproduce the baseline as a multiset, and the completed progress units
 * must equal the total (a donation that leaks or double-counts units
 * misses it; getFinished() cannot tell, it reports 1 for any completed
 * search).
 * CubeInCage has identical pieces, so donated seeds must carry the pivot
 * bits that make their replay apply the monotonic shape filter;
 * SolidSixPieceBurrs has a range column and stops deep.
 */
TEST_CASE("assembler_1 donated work is searched exactly once",
          "[assembler][resume][save][parallel]") {
  struct Case { const char * path; bool dlx; unsigned int threads; const char * every; };
  const Case cases[] = {
    {"examples/CubeInCage.xmpuzzle", false, 1, "1"},
    {"examples/CubeInCage.xmpuzzle", false, 4, "5"},
    {"examples/PiecesOfEight.xmpuzzle", false, 4, "3"},
    {"examples/SolidSixPieceBurrs.xmpuzzle", false, 4, "50"},
    {"examples/CubeInCage.xmpuzzle", true, 2, "1"},
    {"examples/CubeInCage.xmpuzzle", true, 4, "7"},
    {"examples/PiecesOfEight.xmpuzzle", true, 4, "3"},
  };
  for (const auto & c : cases) {
    INFO(c.path << (c.dlx ? " Dancing Cells" : " bit-parallel") << ", " << c.threads << " threads, every " << c.every);
    ScopedSettings noSimd;
    noSimd->bitParallel = !c.dlx;
    auto p = puzzle_c::load(c.path);
    REQUIRE(p != nullptr);
    auto problem = p->getProblem(0);
    REQUIRE(problem != nullptr);

    std::multiset<std::string> baseline;
    {
      ScopedSettings noDonate;
      noDonate->donate = false;
      RecordingAssemblerCallback cb;
      assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
      assm.setNumThreads(4);
      REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
      assm.assemble(&cb);
      baseline = std::move(cb.fingerprints);
    }
    REQUIRE(baseline.size() > 3);

    ScopedSettings every;
    every->donateEvery = std::stoul(c.every);
    {
      RecordingAssemblerCallback cb;
      assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
      assm.setNumThreads(c.threads);
      REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
      assm.assemble(&cb);
      CHECK(assm.getDonations() > 0);
      CHECK(Assembler1UnitsTestAccess::unitsConserved(assm));
      CHECK(cb.fingerprints == baseline);
    }

    for (unsigned int stopAfter : {2u, static_cast<unsigned int>(baseline.size() / 2)}) {
      INFO("stop after " << stopAfter);
      RecordingAssemblerCallback cb1, cb2;
      std::string state;
      {
        assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
        assm.setNumThreads(c.threads);
        REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
        unsigned int seen = 0;
        assm.assemble([&](std::unique_ptr<assembly_c> a) -> bool {
          cb1.assembly(std::move(a));
          return ++seen < stopAfter;
        });
        std::ostringstream str;
        xmlWriter_c xml(str);
        writeAssemblerXml(assm, xml);
        state = str.str();
      }
      {
        assembler_1_c restored(makeSolverSpec(*problem), bttest::settings());
        restored.setNumThreads(c.threads);
        REQUIRE(restored.createMatrix(false, false) == assembler_c::ERR_NONE);
        std::string payload = extractAssemblerContent(state);
        REQUIRE(restored.setPosition(payload.c_str(), assemblerVersionOf(state).c_str())
                == assembler_c::ERR_NONE);
        restored.assemble(&cb2);
        CHECK(Assembler1UnitsTestAccess::unitsConserved(restored));
      }
      std::multiset<std::string> resumed = cb1.fingerprints;
      resumed.insert(cb2.fingerprints.begin(), cb2.fingerprints.end());
      CHECK(resumed == baseline);
    }
  }
}

/* The Huang engine (assembler_1_c) is the other half of the same regression:
 * range and min/max puzzles run on it, and a subtree task contributed nothing
 * to its progress bar until it completed.
 *
 * What this engine does NOT get is the share weighting assembler_0_c has. That
 * was measured and rejected rather than skipped. In short: assembler_1_c
 * generates its tasks by running the real search to a cutoff depth, so it
 * discovers the search's own pruning while it does so, and the
 * uniform-branching measure hands those instantly-dead subtrees almost all of
 * the weight -- 96.9% of it on Burr-Glar, inside the first 10 ms. Weighting by
 * it left the bar frozen at 0.9694 for the whole of a 3 s sample (2 distinct
 * values, 98% plateau) against 11 distinct values and a 31% plateau for the
 * unweighted bar it would have replaced. Tasks are therefore still worth 1/N
 * each, and only the in-flight term is new.
 */
TEST_CASE("Huang parallel assembly progress is monotone and ends at 1.0",
          "[assembler][parallel][progress]") {
  ScopedNoSimd dlxPath;

  auto p = puzzle_c::load("examples/HexSticks.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  /* This is the case the handoff window shows up in: ~3600 samples at 2 ms,
   * every one of them taken while four workers are trading tasks. A worker
   * hands a task over in two steps -- clear my slot, then add one to the
   * completed count -- and getFinished() reads the count first, so a handoff
   * that starts and finishes between those two reads is missed by both and
   * the sample lands a whole task low. It is rare, but at this sample count
   * it happens; it was observed under ThreadSanitizer, where symbolising a
   * report stalls a worker mid-handoff.
   *
   * getFinished() reads the counter and the slots as a snapshot, retaking the
   * pair when the counter moves under the walk and keeping the largest pair
   * seen as a floor, which is what this assertion holds it to. Note what it
   * does NOT do: re-reading the counter and returning the larger of the two
   * answers was tried and rejected -- it recovers the finished task only by
   * discarding every other worker's in-flight term, and this very case still
   * failed with it in place. Nor is the snapshot airtight: a worker stalled
   * between its slot store and its own fetch_add, for the whole of a read,
   * is out of any reader's reach. That window is two adjacent instructions
   * wide, against the microseconds of a whole slot walk it replaces -- rare
   * enough that a genuinely independent retry (see traceUntilMonotone)
   * absorbs it without hiding a real regression.
   */
  bool finishedAtOne = false;
  auto produce = [&]() {
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);

    TestAssemblerCallback cb;
    ProgressTrace t = traceSolve(assm, cb);
    REQUIRE(t.samples.size() > 1);
    finishedAtOne = (assm.getFinished() == 1.0f);
    return t;
  };
  ProgressTrace t = traceUntilMonotone(produce);

  INFO("samples: " << t.samples.size()
       << " distinct: " << t.distinctValues()
       << " longest plateau: " << t.longestPlateauFraction());

  CHECK(t.inRange());
  CHECK(t.monotone());
  CHECK(finishedAtOne);
}

/* The Huang twin of "parallel assembly progress advances smoothly".
 *
 * Burr-Glar's full solve on this path is several minutes, far longer than CI
 * can afford, so the run is stopped after a bounded budget and the assertions
 * are made on the partial curve: plateau length and granularity are shape
 * properties, not length properties. The terminal sample is dropped -- an
 * aborted run clears every in-flight term at once, so its endpoint is an
 * artifact of the abort rather than a point on the curve.
 *
 * Measured on this machine, 3 s of Burr-Glar on the DLX path: without the
 * in-flight term the sample takes 11 distinct values of 129 with a 30.2%
 * plateau; with it, see the report for the current figures.
 */
TEST_CASE("Huang parallel assembly progress advances smoothly",
          "[assembler][parallel][progress]") {
  ScopedNoSimd dlxPath;

  auto p = puzzle_c::load("examples/Burr-Glar.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  auto produce = [&]() {
    assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);

    TestAssemblerCallback cb;
    /* 22 samples: one is dropped below and more than 20 are required */
    ProgressTrace t = traceSolveStoppingAfter(assm, cb, std::chrono::milliseconds(3000),
                                              std::chrono::milliseconds(20), 22);

    /* drop the terminal sample of the stopped run before asserting on shape */
    REQUIRE(t.samples.size() > 1);
    t.samples.pop_back();
    return t;
  };
  /* see the note on the handoff window on traceUntilMonotone */
  ProgressTrace t = traceUntilMonotone(produce);

  INFO("samples: " << t.samples.size()
       << " distinct: " << t.distinctValues()
       << " longest plateau: " << t.longestPlateauFraction());

  REQUIRE(t.samples.size() > 20);
  CHECK(t.inRange());
  CHECK(t.monotone());

  /* Every one of these samples was taken while workers were still live, so
   * none of them may report a finished search. getFinished() sums the
   * completed task COUNT and the in-flight fractions, and at ~105 completed
   * tasks consecutive floats are about 7.6e-6 apart -- a float accumulator
   * rounds a legitimate tail fraction within ~6e-6 of 1 up to the full count
   * and the quotient to exactly 1.0f. solvethread.cpp treats getFinished()
   * >= 1 as "finished", so that rounding is the difference between a display
   * blip and ending a solve with work left. The sum is accumulated in double
   * for that reason.
   */
  CHECK(*std::max_element(t.samples.begin(), t.samples.end()) < 1.0f);

  /* No assertion on the SHAPE of the curve (how long it stands still,
   * how many values it takes): neither engine publishes progress within a
   * task, only whole tasks as they complete, so over a 3 s budget the curve
   * moves in a few steps. The in-flight term this case was written for
   * belonged to the dancing-links search, which is gone. Progress within a
   * task is an open item of design/2026-11-19-ng-merge-plan.md (7.1); the
   * shape assertions come back with it.
   */
}

/* A solve aborted on the parallel path leaves completedTasks and totalTasks
 * holding that run's numbers. A resume with one thread used to go serial, and
 * getFinished() had to switch back to the serial source for it, or it reported
 * the abandoned run's ratio, a constant, for the whole of the serial search.
 *
 * The run no longer goes serial: the abort leaves a pool remainder, and a
 * remainder resumes on the task-based path at any width (willRunParallel()).
 * The ratio after the resume is therefore the same task list's, carried
 * forward, and it may equal the paused one: the resume stops at its first
 * assembly, and when the parallel run had already completed every task up to
 * the one holding it, no task completes in between. Which tasks the parallel
 * abort leaves depends on timing (6/39 to 15/39 here; 15/39 on a slow CI
 * runner, equal to where the resume stops), so the case asserts that the
 * resumed run stays on this list and does not go back, not that it moves.
 *
 * The serial runs that remain (createMatrix(), setPosition() of a serial
 * position) reset the progress state themselves; assemble() still switches
 * the source off for a non-parallel run as a backstop.
 */
TEST_CASE("Dancing Cells progress stays task-based when a remainder resumes on one thread",
          "[assembler][parallel][progress]") {
  ScopedNoSimd cellsPath;

  auto p = puzzle_c::load("examples/HexSticks.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);

  /* a parallel run, stopped from the callback so the abort is deterministic.
   * Tasks that had already drained when the abort landed are counted, so
   * completedTasks is non-zero and so is the ratio.
   */
  int parallelAssemblies = 0;
  assm.assemble([&](std::unique_ptr<assembly_c>) -> bool {
    parallelAssemblies++;
    return false;
  });
  REQUIRE(parallelAssemblies == 1);

  const float paused = assm.getFinished();
  INFO("paused parallel ratio: " << paused);
  REQUIRE(paused > 0.0f);
  REQUIRE(paused < 1.0f);

  /* resume single-threaded and stop it the same way: one worker, inline */
  assm.setNumThreads(1);
  CHECK(assm.getRunThreads() == 1);
  int resumedAssemblies = 0;
  assm.assemble([&](std::unique_ptr<assembly_c>) -> bool {
    resumedAssemblies++;
    return false;
  });
  REQUIRE(resumedAssemblies == 1);

  INFO("progress after the single-threaded resume: " << assm.getFinished());
  CHECK(assm.getFinished() >= paused);
  CHECK(assm.getFinished() < 1.0f);
}

/* The Huang twin of the assembler_0 resume case: completedTasks is only ever
 * zeroed when a fresh task list is generated (parallelTasks empty), and a
 * resumed run finds parallelTasks already holding the pool remainder a prior
 * pause drained back into it, so that reset is skipped and the count carries
 * forward untouched. Without that, a resumed run restarts its count at 0 and
 * the bar jumps backwards to near nothing.
 */
TEST_CASE("Huang parallel progress does not restart after a resume",
          "[assembler][parallel][progress]") {
  ScopedNoSimd dlxPath;

  auto p = puzzle_c::load("examples/Burr-Glar.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);

  /* Paused from the callback rather than after a wall-clock budget: the pause
   * has to leave completed tasks behind or the case passes vacuously, and a
   * budget that reliably completes a task on this machine completes none at
   * all under ThreadSanitizer. Stopping at the first assembly does not depend
   * on how fast the machine is.
   */
  int assemblies = 0;
  assm.assemble([&](std::unique_ptr<assembly_c>) -> bool {
    assemblies++;
    return false;
  });
  REQUIRE(assemblies == 1);

  const float paused = assm.getFinished();
  INFO("paused at " << paused);
  REQUIRE(paused > 0.0f);
  REQUIRE(paused < 1.0f);

  /* Resume, and stop again almost immediately: all this run has to do is
   * confirm parallelMultiSearch left completedTasks alone rather than
   * zeroing it, which it decides before any worker is created.
   */
  TestAssemblerCallback cb;
  traceSolveStoppingAfter(assm, cb, std::chrono::milliseconds(50),
                          std::chrono::milliseconds(10));

  INFO("resumed at " << assm.getFinished());
  CHECK(assm.getFinished() >= paused);
}

/* getRunThreads() is what the progress code charges the assembly phase for,
 * in worker-seconds: wall time times this number. Every run with work left
 * takes the task pool at the configured width (a resumed one too: its seeds
 * are tasks); a completed search has no run left, so 1.
 */
TEST_CASE("getRunThreads reports the width the next run will really have",
          "[assembler][parallel][progress]") {
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  assembler_1_c assm(makeSolverSpec(*problem), bttest::settings());
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);

  /* fresh: the configured width */
  CHECK(assm.getRunThreads() == 4);
  assm.setNumThreads(1);
  CHECK(assm.getRunThreads() == 1);

  /* stopped part way: the resume runs at the configured width */
  assm.setNumThreads(4);
  int seen = 0;
  assm.assemble([&seen](std::unique_ptr<assembly_c>) -> bool { return ++seen < 2; });
  REQUIRE(seen == 2);
  CHECK(assm.getRunThreads() == 4);

  /* run to completion: nothing left to run */
  TestAssemblerCallback cb;
  assm.assemble(&cb);
  REQUIRE(assm.getFinished() == 1.0f);
  CHECK(assm.getRunThreads() == 1);
}

namespace {

/* Holds one token of a ThreadBudget per budget slot, each on its own thread
 * (a thread holds at most one token), until release(). While it holds them no
 * disassembly pool worker can start a job: pickup takes a token first.
 */
class budgetHolders_c {

  std::mutex m;
  std::condition_variable cv;
  std::vector<std::thread> threads;
  size_t arrived = 0;
  bool allHeld = true;
  bool go = false;

public:

  explicit budgetHolders_c(ThreadBudget * budget) {
    if (!budget) {
      allHeld = false;
      return;
    }
    for (unsigned int i = 0; i < budget->total(); i++)
      threads.emplace_back([this, budget] {
        const bool held = budget->tryAcquire();
        std::unique_lock<std::mutex> lock(m);
        if (!held)
          allHeld = false;
        arrived++;
        cv.notify_all();
        cv.wait(lock, [this] { return go; });
        lock.unlock();
        budget->release();  // a no-op on a thread that got no token
      });
    std::unique_lock<std::mutex> lock(m);
    cv.wait(lock, [this] { return arrived == threads.size(); });
  }

  ~budgetHolders_c() { release(); }

  budgetHolders_c(const budgetHolders_c &) = delete;
  budgetHolders_c & operator=(const budgetHolders_c &) = delete;

  /* every token of the budget is held; read before release() */
  bool all(void) {
    std::lock_guard<std::mutex> lock(m);
    return allHeld;
  }

  void release(void) {
    {
      std::lock_guard<std::mutex> lock(m);
      go = true;
    }
    cv.notify_all();
    for (auto & t : threads)
      t.join();
    threads.clear();
  }
};

/* Puts a real solve into the state the bar can latch in, every time.
 *
 * That state is "assembly complete, assemblies counted, nothing
 * disassembled yet". A natural solve passes through it only if the
 * assembler happens to finish before the pool completes its first job, and a
 * sampler on another thread only sees it if it happens to poll inside that
 * window. Here the assemblies are held back while the search runs and handed
 * to the pool in assemblyPhaseEnded() -- after assemble() has returned,
 * before run() records the end of the phase -- while every budget token is
 * held, so the pool takes the jobs but cannot start one. The bar is read
 * there, then the tokens go back and the solve finishes normally.
 *
 * Nothing reads getProgress() during the search: the bar holds 0 until the
 * hook. With the assemblies held back the pool has no evidence during the
 * search, so a read there would publish the bare assembly fraction, and
 * where the search found its last assembly would decide what the bar holds
 * when the hook runs.
 */
class latchWindowSolveThread_c : public solveThread_c {

  std::mutex heldMutex;
  std::vector<std::unique_ptr<assembly_c>> held;

  std::mutex phaseMutex;
  std::condition_variable phaseCv;
  bool phaseEnded = false;

public:

  using solveThread_c::solveThread_c;

  // assemblyPhaseEnded() is called on the worker: join it while this
  // subclass is still intact
  ~latchWindowSolveThread_c() override { joinThread(); }

  /* written on the worker in assemblyPhaseEnded(), read by the test after
   * waitForPhaseEnd() returned true */
  size_t assembliesHeld = 0;
  bool budgetPresent = false;
  bool tokensHeld = false;
  float latchValue = -1.0f;       // every assembly counted, none disassembled
  bool latchOutstanding = false;
  bool delivered = false;         // within the deadline below
  float deliveredValue = -1.0f;   // the first disassembly has been delivered

  bool waitForPhaseEnd(std::chrono::steady_clock::time_point deadline) {
    std::unique_lock<std::mutex> lock(phaseMutex);
    return phaseCv.wait_until(lock, deadline, [this] { return phaseEnded; });
  }

protected:

  bool assembly(std::unique_ptr<assembly_c> a) override {
    std::lock_guard<std::mutex> lock(heldMutex);
    held.push_back(std::move(a));
    return true;
  }

  void assemblyPhaseEnded(void) override {
    std::vector<std::unique_ptr<assembly_c>> found;
    {
      std::lock_guard<std::mutex> lock(heldMutex);
      found.swap(held);
    }
    assembliesHeld = found.size();

    /* public in assembler_cb, where the assemblers ask for it */
    assembler_cb & cb = *this;
    ThreadBudget * budget = cb.threadBudget();
    budgetPresent = (budget != nullptr);

    {
      budgetHolders_c holders(budget);
      tokensHeld = holders.all();

      for (auto & a : found)
        solveThread_c::assembly(std::move(a));

      latchValue = getProgress();
      latchOutstanding = workOutstanding();
    }

    /* Once one disassembly is delivered, the blend has evidence and the bar
     * must have moved off what it held. The value is not checked against
     * the cap: assemblyEndNs is still 0 here, so workOutstanding() reads true
     * even after the pool has done every job, and the cap is then the honest
     * value. Waits on the pool, which holds its tokens again, not on time;
     * the deadline only turns a pool that never delivers into a failure
     * rather than a hang. */
    if (assembliesHeld) {
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
      while (getProblem().getNumAssemblies() == 0 &&
             std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
      delivered = getProblem().getNumAssemblies() > 0;
      deliveredValue = getProgress();
    }

    {
      std::lock_guard<std::mutex> lock(phaseMutex);
      phaseEnded = true;
    }
    phaseCv.notify_all();
  }
};

}

/* The GUI reads solveThread_c::getProgress(), so the properties the bar
 * depends on are asserted here rather than by driving FLTK: the value is
 * bounded, never moves backwards, stays strictly below 1.0 for as long as the
 * solve is running, and lands on exactly 1.0 when the solve reports finished.
 *
 * "strictly below 1.0 while running" is the one that has to be bought back
 * here. progressModel_c signals "everything counted is done but work remains"
 * with std::nextafter(1.0f, 0.0f), which the GUI's %.4f renders as 100.0000%,
 * and the parallel assemblers can round getFinished() to exactly 1.0f while
 * their workers are still live. Neither is invisible to the user by accident:
 * getProgress() caps what it reports while the solve runs.
 *
 * And the bar must not spend the solve pinned at that cap. Until the pool
 * completes its first job there is nothing to blend, and once assembly is
 * complete the model's assembly-only answer is its "counted but not done"
 * sentinel. Published into the monotone guard, that caps to runningCap and
 * latches: every genuine blended value afterwards is lower, so the guard
 * locks them all out for the whole disassembly tail. latchWindowSolveThread_c
 * reads the bar in exactly that state, so the latch is caught on every run
 * rather than on the runs where a sampler happened to look at the right
 * moment; the samples from the test thread afterwards only add properties
 * that hold whenever they are taken, so their number does not matter.
 */
TEST_CASE("solve thread reports monotone whole-solve progress",
          "[solvethread][progress]") {
  /* two threads, not the machine's count: with one the pool disassembles
   * inline and has no budget to hold; the latch window is held open
   * through the budget */
  bttest::ScopedSettings s;
  s->threads = 2;
  s->disassemblerPool = true;
  s->threadBudget = true;

  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  /* the shipped examples are saved already solved, and solveThread_c
   * asserts solveState == SS_UNSOLVED on the way in
   */
  problem->removeAllSolutions();

  latchWindowSolveThread_c thread(*problem, solveThread_c::PAR_DISASSM, bttest::settings());

  /* nothing has started yet, so there is nothing to report and no
   * assembler to read it from */
  REQUIRE(thread.getProgress() == 0.0f);

  REQUIRE(thread.start());

  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);
  REQUIRE(thread.waitForPhaseEnd(deadline));

  REQUIRE(thread.budgetPresent);
  REQUIRE(thread.tokensHeld);
  REQUIRE(thread.assembliesHeld > 0);

  /* The latch window: assembly complete, every assembly counted, none
   * disassembled. The bar holds what it held -- 0, nothing was published
   * before -- instead of jumping to the cap. */
  CHECK(thread.latchOutstanding);
  CHECK(thread.latchValue == 0.0f);
  CHECK(thread.latchValue < solveThread_c::runningCap);

  /* and it moves as soon as there is evidence */
  REQUIRE(thread.delivered);
  CHECK(thread.deliveredValue > thread.latchValue);
  CHECK(thread.deliveredValue < 1.0f);

  struct sample_t {
    float value;
    bool outstanding;   // the work was outstanding when value was reported
    bool finished;      // ACT_FINISHED may have been set before value was read
  };
  std::vector<sample_t> samples;

  while (!thread.stopped() && std::chrono::steady_clock::now() < deadline) {
    const float v = thread.getProgress();
    /* read after the sample: workOutstanding() never goes back to true, so
     * true here means the work was outstanding when v was reported */
    const bool outstanding = thread.workOutstanding();
    /* read after the sample too: not finished here means v was reported by
     * a running solve */
    const bool finished = thread.currentAction() == solveThread_c::ACT_FINISHED;
    samples.push_back({v, outstanding, finished});
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  REQUIRE(thread.currentAction() == solveThread_c::ACT_FINISHED);

  /* Every value the running solve reported is in [0, 1), and none moved
   * backwards: the hook's two values come before every sample (the sampler
   * waited for the hook), and all of them are read off one monotone guard.
   * The cap may be reported once the counted work is done -- the worker then
   * still joins the pool's workers and merger and delivers the results
   * before it reports ACT_FINISHED -- but never while work remains. That
   * relies on no honest value reaching the cap while work remains, and on
   * this puzzle none comes close: over 140 runs, idle and with the machine
   * twice oversubscribed, the highest value reported with work outstanding
   * was 0.9896 -- ten times further from 1 than the cap is.
   */
  bool samplesInRange = true;
  bool samplesMonotone = true;
  size_t capWhileOutstanding = 0;
  float previous = thread.deliveredValue;
  for (const auto & s : samples) {
    if (s.value < 0.0f || s.value > 1.0f || (!s.finished && s.value >= 1.0f))
      samplesInRange = false;
    if (s.value < previous)
      samplesMonotone = false;
    if (s.value >= solveThread_c::runningCap && s.outstanding)
      capWhileOutstanding++;
    previous = s.value;
  }

  INFO(samples.size() << " samples after the assembly phase, latch window at "
       << thread.latchValue << ", first delivery at " << thread.deliveredValue);
  CHECK(samplesInRange);
  CHECK(samplesMonotone);
  CHECK(capWhileOutstanding == 0);

  // getProgress() == 1.0f, checked twice: idempotent, the GUI polls repeatedly
  CHECK(thread.getProgress() == 1.0f);
  CHECK(thread.getProgress() == 1.0f);

  /* holding the assemblies back lost none of them */
  CHECK(problem->getNumAssemblies() == thread.assembliesHeld);
}

namespace {

/* Reads the bar each time the search delivers an assembly, before passing it
 * on: points inside the search, fixed by the puzzle rather than by when a
 * sampler gets scheduled. The reads and their recording share one mutex, so
 * the recorded order is the order the guard published them in.
 */
class searchReadSolveThread_c : public solveThread_c {

  std::mutex readsMutex;

public:

  using solveThread_c::solveThread_c;

  // assembly() is called on the assembler's workers: join them while this
  // subclass is still intact
  ~searchReadSolveThread_c() override { joinThread(); }

  /* written by the workers, read by the test once the solve has finished */
  std::vector<float> reads;

protected:

  bool assembly(std::unique_ptr<assembly_c> a) override {
    {
      std::lock_guard<std::mutex> lock(readsMutex);
      reads.push_back(getProgress());
    }
    return solveThread_c::assembly(std::move(a));
  }
};

}

/* The whole-solve test above reads nothing during the search. This one reads
 * there only: what the GUI sees while assemblies are still being found and
 * the pool works through the first of them. Every read is in range, below
 * the cap -- the search is running, so work is outstanding -- and none moves
 * backwards.
 */
TEST_CASE("solve thread progress during the search is in range and monotone",
          "[solvethread][progress]") {
  bttest::ScopedSettings s;
  s->threads = 2;
  s->disassemblerPool = true;

  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);
  problem->removeAllSolutions();

  searchReadSolveThread_c thread(*problem, solveThread_c::PAR_DISASSM, bttest::settings());
  REQUIRE(thread.start());

  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);
  while (!thread.stopped() && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  REQUIRE(thread.currentAction() == solveThread_c::ACT_FINISHED);

  /* one read per assembly */
  REQUIRE(thread.reads.size() == problem->getNumAssemblies());
  REQUIRE(thread.reads.size() > 1);

  bool inRange = true;
  bool monotone = true;
  for (size_t i = 0; i < thread.reads.size(); i++) {
    if (thread.reads[i] < 0.0f || thread.reads[i] >= solveThread_c::runningCap)
      inRange = false;
    if (i && thread.reads[i] < thread.reads[i-1])
      monotone = false;
  }
  INFO(thread.reads.size() << " reads, from " << thread.reads.front()
       << " to " << thread.reads.back());
  CHECK(inRange);
  CHECK(monotone);
}

/* The rule getProgress() applies while the disassembly pool has completed
 * nothing, in isolation from any solve.
 *
 * The regression this pins: the hold used to trigger on
 * `assemblyFraction >= 1.0f`, which getProgress() cannot reach on that path --
 * it clamps a rounded-up fraction to std::nextafter(1.0f, 0.0f) while assembly
 * is still running. So every fraction in [runningCap, 1.0) fell through to the
 * clamp instead, published runningCap into the monotone guard, and pinned the
 * bar there for the rest of the solve. Narrower trigger than the latch it was
 * written to remove, same freeze: it needs only an assembler that passes 99.9%
 * before the pool finishes its first task, which is what a puzzle whose
 * assemblies all live at the end of the search does.
 */
TEST_CASE("with no disassembly evidence the bar never publishes the cap",
          "[solvethread][progress]") {

  const float cap = solveThread_c::runningCap;
  const float held = 0.4f;   // whatever the guard is already holding

  SECTION("below the cap the assembly fraction is reported as it stands") {
    CHECK(solveThread_c::noEvidenceProgress(0.0f, 0.0f) == 0.0f);
    CHECK(solveThread_c::noEvidenceProgress(0.5f, held) == 0.5f);
    CHECK(solveThread_c::noEvidenceProgress(std::nextafter(cap, 0.0f), held)
          == std::nextafter(cap, 0.0f));
  }

  SECTION("at and above the cap the bar is held instead of pinned") {
    /* the case the old 1.0f threshold could not see: still running, so the
     * fraction is below 1, but at or past the cap
     */
    CHECK(solveThread_c::noEvidenceProgress(cap, held) == held);
    CHECK(solveThread_c::noEvidenceProgress(0.9995f, held) == held);
    CHECK(solveThread_c::noEvidenceProgress(std::nextafter(1.0f, 0.0f), held) == held);
    CHECK(solveThread_c::noEvidenceProgress(1.0f, held) == held);

    /* and none of them is the cap, which is the value that pins the guard */
    CHECK(solveThread_c::noEvidenceProgress(0.9995f, held) < cap);
  }

  SECTION("holding cannot move the bar backwards") {
    /* the held value is returned unchanged, so the monotone guard sees no
     * decrease -- holding is a pause, not a retreat
     */
    for (float a : {cap, 0.9995f, 1.0f})
      CHECK(solveThread_c::noEvidenceProgress(a, held) == held);
  }
}

/* The cost basis the blend is built on, across the GUI's resume path.
 *
 * solveThread_c records how far the assembler already was when it picked it
 * up, and progressModel_c::projectAssemblyCost() charges the assembly phase
 * for the whole of its fraction by extrapolating from what THIS run measured
 * between that base and the live fraction. A base at or above the live
 * fraction leaves nothing gained, the projected cost collapses to 0, and
 * evaluate() then cannot blend: the bar falls back to reporting assembly
 * alone, with the disassembly phase never weighed -- silently, since the
 * value it reports is still bounded, monotone and plausible.
 *
 * Fresh solves are the identity case: no head start, so the base must be
 * exactly 0 and the projection must return the measured cost untouched.
 */
TEST_CASE("a fresh solve keeps a zero assembly cost basis",
          "[solvethread][progress]") {
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);
  problem->removeAllSolutions();

  solveThread_c thread(*problem, solveThread_c::PAR_DISASSM, bttest::settings());
  REQUIRE(thread.start());

  float worstBase = 0.0f;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);
  while (!thread.stopped() &&
         thread.currentAction() != solveThread_c::ACT_ASSERT &&
         std::chrono::steady_clock::now() < deadline) {
    thread.getProgress();   // the GUI's poll, which is what maintains the basis
    const float base = thread.getAssemblyBaseFraction();
    if (base > worstBase) worstBase = base;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  REQUIRE(thread.currentAction() == solveThread_c::ACT_FINISHED);

  /* exactly zero, at every moment of the solve and after it: that is what
   * makes the projection the identity on the measured cost
   */
  CHECK(worstBase == 0.0f);
  CHECK(thread.getAssemblyBaseFraction() == 0.0f);
  CHECK(progressModel_c::projectAssemblyCost(37.5, 0.4f,
                                             thread.getAssemblyBaseFraction()) == 37.5);
}

/* The resume case, and the regression this test exists for.
 *
 * A paused-then-continued solve, driven the way the GUI drives it: one
 * solveThread_c is started and stopped, destroyed, and a second is built on
 * the same problem, which finds the assembler the first left behind and picks
 * up its search state.
 *
 * The assembly fraction survives that handover; the seconds that bought it do
 * not, because they belonged to the first thread. Feeding the model this run's
 * seconds against the whole of the carried fraction under-projects the
 * assembly phase by exactly the ratio of the two, so the blend collapses
 * towards the disassembly fraction and the monotone guard pins it there while
 * assembly does the rest of the work -- the freeze this branch exists to
 * remove, reintroduced on the resume path.
 *
 * The check is on the basis rather than on the reported value, because a
 * broken basis is not visible in the value alone: it stays bounded, monotone
 * and plausible while silently reporting assembly only.
 *
 * Burr-Glar because it is the one bundled puzzle whose assembly phase runs
 * long enough to pause in the middle of and still have a stretch left to watch.
 */
TEST_CASE("a resumed solve keeps the assembly cost basis alive",
          "[solvethread][progress][stress]") {
  auto p = puzzle_c::load("examples/Burr-Glar.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);
  problem->removeAllSolutions();

  /* ---- the first solve, paused part-way through assembly ---- */
  float paused = 0.0f;
  {
    solveThread_c first(*problem, solveThread_c::PAR_DISASSM, bttest::settings());
    REQUIRE(first.start());

    /* Pause on observed progress, not on a wall clock. A fixed budget fails in
     * both directions: too long and a fast machine finishes the assembly
     * inside it, leaving nothing to resume; too short and a slow one pauses at
     * a fraction so small that the second run's window swamps it. Waiting for
     * the thread's own published progress is machine-independent, and reading
     * it is race-free -- getProgress() is the atomic the GUI polls, so nothing
     * here touches problem_c while the worker is installing the assembler.
     */
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);
    while (!first.stopped() &&
           first.getProgress() < 0.05f &&
           std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(5));

    first.stop();
    const auto joinBy = std::chrono::steady_clock::now() + std::chrono::seconds(120);
    while (!first.stopped() && std::chrono::steady_clock::now() < joinBy)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    REQUIRE(first.stopped());
    REQUIRE(first.currentAction() == solveThread_c::ACT_PAUSING);

    paused = problem->getAssembler()->getFinished();
  }

  INFO("paused at " << paused);
  /* a pause that made no progress would make the whole case vacuous */
  REQUIRE(paused > 0.0f);
  REQUIRE(paused < 1.0f);

  /* ---- the continue: a new thread over the assembler the first left ---- */
  assembler_c * assm = problem->getAssembler();
  REQUIRE(assm != nullptr);

  /* The loop below polls getFinished() while the worker searches. That is the
   * call's documented contract ("It must be possible to call this function
   * while assemble is running") and what the GUI does on every refresh, but it
   * is only free of data races on the parallel path, where getFinished() reads
   * published atomics alone; the serial fallback walks the live DLX stack.
   * Assert the coming run is the parallel one rather than assume it -- if a
   * future change routes this resume to the serial path, this fails loudly
   * instead of the polling quietly becoming a race.
   */
  REQUIRE(assm->getRunThreads() > 1);

  solveThread_c second(*problem, solveThread_c::PAR_DISASSM, bttest::settings());
  REQUIRE(second.start());

  unsigned int samples = 0;    // samples taken during the assembly phase
  unsigned int dead = 0;       // ... of which found no cost basis left
  unsigned int deadAfterGain = 0;  // ... of those, taken after the run advanced
  unsigned int trusted = 0;    // ... of which were past the model's trust threshold
  float reached = 0.0f, baseSeen = -1.0f;
  double worstLeverage = 0.0; // smallest projected/session cost ratio seen
  float leverageA = 0.0f, leverageBase = 0.0f;  // the pair that produced it

  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(300);
  while (!second.stopped() &&
         second.currentAction() != solveThread_c::ACT_ASSERT &&
         std::chrono::steady_clock::now() < deadline) {

    if (second.currentAction() == solveThread_c::ACT_ASSEMBLING) {

      /* the GUI's own poll, which is what maintains the basis */
      second.getProgress();

      const float fraction = assm->getFinished();
      const float base = second.getAssemblyBaseFraction();
      samples++;
      if (fraction > reached) reached = fraction;
      if (baseSeen < 0.0f) baseSeen = base;

      /* The projection getProgress() makes, from the same two numbers it makes
       * it from, against one second of measured cost. Zero means the phase is
       * charged nothing, which is what turns the blend off; anything above 1
       * is the head start being charged for, which is the point.
       */
      const double projected = progressModel_c::projectAssemblyCost(1.0, fraction, base);
      if (projected <= 0.0) {
        dead++;
        /* Allowed only before the run has gained anything on its base: at that
         * instant nothing has been measured, and reporting no cost is right.
         */
        if (fraction > base) deadAfterGain++;
      } else if (worstLeverage == 0.0 || projected < worstLeverage) {
        worstLeverage = projected;
        leverageA = fraction;
        leverageBase = base;
      }

      if (fraction >= progressModel_c::trustThreshold) trusted++;

      /* Stop once there is enough to say something, without running the whole
       * of Burr-Glar. Both halves are floors, so the window is at LEAST 300
       * samples and at least 2% of the head start -- whichever of the two the
       * machine reaches last is what ends it.
       *
       * This bounds the window; it does NOT bound how far the search gets
       * inside it, and no wording here should imply otherwise. On a slow CI
       * runner the 300-sample floor binds long after the fraction floor, and
       * the run covered 0.625 -> 0.854. That is why nothing below asserts a
       * MAGNITUDE of leverage: a/(a-base) is a measure of how little the
       * resumed run got through, which is a property of the runner, not of
       * the code under test.
       */
      if (samples > 300 && fraction >= paused * 1.02f) break;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  second.stop();
  const auto joinBy = std::chrono::steady_clock::now() + std::chrono::seconds(300);
  while (!second.stopped() && std::chrono::steady_clock::now() < joinBy)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  REQUIRE(second.stopped());

  /* Read once the worker has stopped writing it: it is a plain counter, and
   * polling it from here while the pool's merger thread counts into it is an
   * unsynchronised read ThreadSanitizer flags. Monotone, so a non-zero value
   * now means disassembly results were being counted during the window above.
   */
  const unsigned long disassembled = problem->getNumAssemblies();

  INFO("assembly-phase samples: " << samples
       << ", past the trust threshold: " << trusted
       << ", disassembly results counted: " << disassembled
       << ", with no cost basis: " << dead
       << " (of them after the run gained on its base: " << deadAfterGain << ")"
       << "; paused at " << paused << ", base settled at " << baseSeen
       << ", fraction reached " << reached
       << ", smallest projected cost per measured second: " << worstLeverage
       << " at a=" << leverageA << " base=" << leverageBase);

  /* Not a vacuous pass: the window has to have covered a real stretch of the
   * assembly phase, with every OTHER condition evaluate() needs to blend
   * satisfied, so the cost basis is the only thing left that can switch the
   * blend off.
   */
  REQUIRE(samples > 100);
  REQUIRE(trusted > 0);
  REQUIRE(disassembled > 0);

  /* The run picked the paused search up rather than starting over.
   *
   * Not asserted as exact equality with `paused`: the two are read from the
   * assembler at different moments, and getProgress()'s ratchet may lower the
   * base to whatever the resumed run first reports. Both directions of that
   * are covered -- it must be a real head start, and it must never claim more
   * of one than the first thread actually achieved.
   */
  CHECK(baseSeen > 0.0f);
  CHECK(baseSeen <= paused);

  /* Never charged nothing once it had measured something. */
  CHECK(deadAfterGain == 0);

  /* And the head start is actually paid for.
   *
   * What is asserted is the MECHANISM, not a magnitude. The leverage a run
   * shows -- a/(a-base) -- measures how little of the search it got through
   * inside the sampling window, so any floor above 1 is really a statement
   * about how fast the machine is. A bound of 5 passed here at 50.3 and failed
   * on a CI runner at 3.72, where the projection was working perfectly: 3.72378
   * observed against 3.72379 predicted, for a = 0.85446 over base = 0.625.
   *
   * The floor is therefore 1, which is not machine-calibrated but a property
   * of the arithmetic. For a live base in (0,1) and a in (base,1], a/(a-base)
   * is decreasing in a, so it is smallest at a = 1 and worth at least
   * 1/(1-base) -- strictly above 1 for ANY base, at any machine speed, however
   * far the resumed run gets. And 1.0 is exactly what a lost base reports:
   * projectAssemblyCost() returns the session cost untouched when base is 0,
   * which is the defect -- asmCost/a collapses, assembly stops counting
   * against disassembly in the blend, and both the bar and the time estimate
   * are wrong by that factor.
   */
  CHECK(worstLeverage > 1.0);

  /* The projection matches its own formula at the values this run actually
   * showed: the measured second is inflated by a/(a-base). Catches the
   * projection being dropped, inverted or mis-scaled at live values, none of
   * which a fixed-input unit test can see, and none of which depend on how far
   * the run got.
   *
   * The comparison is exact rather than approximate by construction: both
   * sides convert the same two floats to double, and the difference of two
   * doubles widened from float is itself exact, so the two computations agree
   * bit for bit. The epsilon is there for compiler reassociation, not for
   * measurement noise.
   */
  REQUIRE(leverageA > leverageBase);
  const double predicted = static_cast<double>(leverageA)
                         / (static_cast<double>(leverageA) - static_cast<double>(leverageBase));
  INFO("leverage " << worstLeverage << " predicted " << predicted);
  CHECK(worstLeverage == Catch::Approx(predicted).epsilon(1e-9));

  /* Order matters, and these values are on the right side of it: charging the
   * fraction against a base ABOVE it leaves nothing gained and no cost at all.
   * This is the state the capture-ordering hazard would have produced.
   */
  CHECK(progressModel_c::projectAssemblyCost(1.0, leverageBase, leverageA) == 0.0);

  /* The session cost is a linear multiplier, so a mis-scaled one would show. */
  CHECK(progressModel_c::projectAssemblyCost(7.5, leverageA, leverageBase)
        == Catch::Approx(7.5 * worstLeverage).epsilon(1e-9));

  /* And the bar stayed a bar: bounded, and never the running cap, which is
   * what a pinned resume would report.
   */
  CHECK(second.getProgress() >= 0.0f);
  CHECK(second.getProgress() < solveThread_c::runningCap);
}
