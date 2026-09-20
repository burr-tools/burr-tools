#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "lib/puzzle.h"
#include "lib/solvethread.h"
#include "lib/voxel.h"
#include "lib/problem.h"
#include "lib/assembler.h"
#include "lib/assembler_0.h"
#include "lib/assembler_1.h"
#include "lib/assembly.h"
#include "lib/disassembler.h"
#include "lib/disassembler_0.h"
#include "lib/disassembly.h"
#include "lib/gridtype.h"
#include "lib/progressmodel.h"
#include "lib/solvethread.h"
#include "lib/voxel.h"
#include "tools/xml.h"
#include "tools/gzstream.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <set>
#include <chrono>
#include <memory>
#include <thread>
#include <string>
#include <thread>
#include <vector>

namespace {

/* RAII helper to temporarily set or unset environment variables during a test,
 * allowing tests to verify fallback behavior under runtime feature toggles
 * (such as BURRTOOLS_NO_SIMD).
 */
class ScopedEnv {
public:
  ScopedEnv(const char * name, const char * value) : name_(name) {
    const char * old = getenv(name);
    had_ = (old != nullptr);
    if (had_) old_ = old;
    set(name, value);
  }
  ~ScopedEnv() {
    set(name_.c_str(), had_ ? old_.c_str() : nullptr);
  }
private:
  /* setenv/unsetenv are POSIX; MinGW and MSVC have _putenv_s instead, where
   * assigning an empty value is what removes the variable
   */
  static void set(const char * name, const char * value) {
#ifdef WIN32
    _putenv_s(name, value ? value : "");
#else
    if (value) setenv(name, value, 1);
    else unsetenv(name);
#endif
  }

  std::string name_;
  std::string old_;
  bool had_;
};

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

SolveResult solvePuzzle(const char * path, unsigned int problemIdx = 0, bool disassemble = true) {
  auto p = puzzle_c::load(path);
  REQUIRE(p != nullptr);

  REQUIRE(problemIdx < p->getNumberOfProblems());
  problem_c * problem = p->getProblem(problemIdx);
  REQUIRE(problem != nullptr);

  const gridType_c * gt = problem->getPuzzle().getGridType();
  REQUIRE(gt != nullptr);

  std::unique_ptr<assembler_c> assm = gt->findAssembler(*problem);
  REQUIRE(assm != nullptr);

  REQUIRE(assm->createMatrix(false, false, false) == assembler_c::ERR_NONE);

  std::unique_ptr<disassembler_c> disasm;
  if (disassemble && (gt->getCapabilities() & gridType_c::CAP_DISASSEMBLE)) {
    disasm = std::make_unique<disassembler_0_c>(*problem);
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

  // Test assembler_0_c: calls assemble with lambda overload directly on derived assembler_0_c
  {
    assembler_0_c assm(*problem);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    int count = 0;
    assm.assemble([&](std::unique_ptr<assembly_c>) -> bool {
      count++;
      return false; // Return false to stop immediately
    });
    CHECK(count == 1);
  }

  // Test assembler_1_c: calls assemble with lambda overload directly on derived assembler_1_c
  {
    assembler_1_c assm(*problem);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
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

  auto assm = std::make_unique<assembler_0_c>(*problem);
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
    assembler_0_c assm(*problem);
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    disassembler_0_c disasm(*problem);
    TestAssemblerCallback cb(&disasm);
    assm.assemble(&cb);
    assemblies_1 = cb.assemblies;
    solutions_1 = cb.solutions;
    CHECK(assemblies_1 == 12);
    CHECK(solutions_1 == 1);
  }

  {
    assembler_0_c assm(*problem);
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    disassembler_0_c disasm(*problem);
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
    assembler_0_c assm(*problem);
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    TestAssemblerCallback cb;
    assm.assemble(&cb);
    total_expected = cb.assemblies;
    REQUIRE(total_expected == 12);
  }

  // Two-phase solve: stop after 5 assemblies, then continue
  {
    assembler_0_c assm(*problem);
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

    int phase1_count = 0;
    class StoppingCallback : public assembler_cb {
    public:
      int count = 0;
      assembler_0_c & a;
      StoppingCallback(assembler_0_c & assm) : a(assm) {}
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

TEST_CASE("Parallel assembler on small 2/3 piece problem", "[assembler][parallel][small]") {
  auto p = puzzle_c::load("examples/DemoMirrorParadox.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  if (assembler_0_c::canHandle(*problem)) {
    assembler_0_c assm1(*problem);
    assm1.setNumThreads(1);
    REQUIRE(assm1.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    TestAssemblerCallback cb1;
    assm1.assemble(&cb1);

    assembler_0_c assm4(*problem);
    assm4.setNumThreads(4);
    REQUIRE(assm4.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    TestAssemblerCallback cb4;
    assm4.assemble(&cb4);

    CHECK(cb4.assemblies == cb1.assemblies);
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

/* An interrupted parallel search must not save itself as a resumable position.
 *
 * Before this was handled, stopping a parallel solve left pos == 0, save()
 * wrote "nothing searched yet" next to an already-populated solution list, and
 * continuing re-reported every assembly found before the stop. The contract
 * now is: such a state is refused on restore with a distinct error, so the
 * caller resets rather than double counting.
 */
TEST_CASE("Parallel assembler: an interrupted search is not restored as resumable",
          "[assembler][parallel][resume]") {
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  assembler_0_c assm(*problem);
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

  /* stop from the callback on the first assembly, which aborts the workers
   * part way through the task set
   */
  int seen = 0;
  assm.assemble([&seen](std::unique_ptr<assembly_c>) -> bool {
    seen++;
    return false;
  });
  REQUIRE(seen == 1);
  REQUIRE(assm.getFinished() < 1.0f);

  /* what the interrupted search would write into the .xmpuzzle */
  std::string state;
  {
    std::ostringstream str;
    xmlWriter_c xml(str);
    assm.save(xml);
    state = str.str();
  }

  /* a fresh assembler must refuse it rather than silently starting over */
  assembler_0_c restored(*problem);
  REQUIRE(restored.createMatrix(false, false, false) == assembler_c::ERR_NONE);

  std::string payload = extractAssemblerContent(state);
  CHECK(restored.setPosition(payload.c_str(), assemblerVersionOf(state).c_str())
        == assembler_c::ERR_CAN_NOT_RESTORE_INTERRUPTED);
}

/* The leading flag added to the save payload must not break ordinary restore.
 *
 * Deliberately a not-yet-started assembler rather than a finished one: a
 * finished search is SS_SOLVED and never serialised, and feeding a completed
 * position (pos == piecenumber + 1) to setPosition() trips a pre-existing
 * out-of-bounds read, since rows/columns are sized piecenumber while the
 * integrity loop runs to pos. That is a separate bug from this change.
 */
TEST_CASE("Parallel assembler: the interrupted flag does not break normal restore",
          "[assembler][parallel][resume]") {
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  assembler_0_c assm(*problem);
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

  std::string state;
  {
    std::ostringstream str;
    xmlWriter_c xml(str);
    assm.save(xml);
    state = str.str();
  }

  assembler_0_c restored(*problem);
  REQUIRE(restored.createMatrix(false, false, false) == assembler_c::ERR_NONE);
  std::string payload = extractAssemblerContent(state);
  CHECK(restored.setPosition(payload.c_str(), assemblerVersionOf(state).c_str())
        == assembler_c::ERR_NONE);
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

  assembler_0_c assm(*problem);
  assm.setNumThreads(1000000);
  CHECK(assm.getNumThreads() <= 256);

  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
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
    assembler_0_c assm(*problem);
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
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
    assembler_0_c assm(*problem);
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, true, false) == assembler_c::ERR_NONE);
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
  assembler_0_c assm(*problemFresh);
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
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
    assembler_1_c assm(*problem);
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    serial = std::move(cb.fingerprints);
  }
  CHECK(serial.size() == 96);

  {
    RecordingAssemblerCallback cb;
    assembler_1_c assm(*problem);
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
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
    assembler_1_c assm(*problem);
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    serial = std::move(cb.fingerprints);
  }
  REQUIRE_FALSE(serial.empty());

  RecordingAssemblerCallback cb;
  assembler_1_c assm(*problem);
  assm.setNumThreads(64);          // targetTasks = 256, unreachable at depth 1
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
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
    assembler_1_c assm(*problem);
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    serial = std::move(cb.fingerprints);
  }
  REQUIRE(serial.size() > 1);

  assembler_1_c assm(*problem);
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

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

TEST_CASE("Parallel assembler 1 stops promptly when aborted", "[assembler][parallel]") {
  auto p = puzzle_c::load("examples/CubeInCage.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  assembler_1_c assm(*problem);
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

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
    assembler_1_c assm(*problem);
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
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
    assembler_1_c assm(*problem);
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, true, false) == assembler_c::ERR_NONE);
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
  assembler_1_c assm(*problemFresh);
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
  assm.assemble(&cb);

  CHECK(cb.fingerprints == serial);
}

/* Same contract as the assembler_0 case: a parallel Huang search that was
 * stopped part way saves no usable resume point, because
 * generateTasksAtDepth() has reset the master back to the root, so it must be
 * refused on restore rather than silently starting over and re-reporting.
 */
TEST_CASE("Parallel assembler 1: an interrupted search is not restored as resumable",
          "[assembler][parallel][resume]") {
  auto p = puzzle_c::load("examples/CubeInCage.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  assembler_1_c assm(*problem);
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

  int seen = 0;
  assm.assemble([&seen](std::unique_ptr<assembly_c>) -> bool { seen++; return false; });
  REQUIRE(seen == 1);
  REQUIRE(assm.getFinished() < 1.0f);

  std::string state;
  {
    std::ostringstream str;
    xmlWriter_c xml(str);
    assm.save(xml);
    state = str.str();
  }

  assembler_1_c restored(*problem);
  REQUIRE(restored.createMatrix(false, false, false) == assembler_c::ERR_NONE);
  std::string payload = extractAssemblerContent(state);
  CHECK(restored.setPosition(payload.c_str(), assemblerVersionOf(state).c_str())
        == assembler_c::ERR_CAN_NOT_RESTORE_INTERRUPTED);
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
  ScopedEnv env("BURRTOOLS_NO_SIMD", "1");
  auto p = puzzle_c::load("examples/CubeInCage.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  assembler_1_c assm(*problem);
  assm.setNumThreads(1);            // serial: a resumable stop
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

  int seen = 0;
  assm.assemble([&seen](std::unique_ptr<assembly_c>) -> bool { seen++; return false; });
  REQUIRE(seen == 1);
  REQUIRE(assm.getFinished() < 1.0f);

  std::string state;
  {
    std::ostringstream str;
    xmlWriter_c xml(str);
    assm.save(xml);
    state = str.str();
  }

  assembler_1_c restored(*problem);
  REQUIRE(restored.createMatrix(false, false, false) == assembler_c::ERR_NONE);
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

  assembler_1_c assm(*problem);
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
  assm.assemble([](std::unique_ptr<assembly_c>) -> bool { return true; });
  REQUIRE(assm.getFinished() >= 1.0f);

  /* a fresh assembler on the same problem must start at 0, not inherit a
   * finished-looking fraction
   */
  assembler_1_c again(*problem);
  again.setNumThreads(4);
  REQUIRE(again.createMatrix(false, false, false) == assembler_c::ERR_NONE);
  CHECK(again.getFinished() < 1.0f);
}

/* Cross-check the SIMD exact-cover path against classical DLX on the whole
 * regression corpus.
 *
 * This matters for two reasons beyond the comparison itself. canUseSimd()
 * returns true by default, so without this the regression suite exercises
 * *only* the SIMD path, and the DLX implementations in assembler_0.cpp /
 * assembler_1.cpp became untested in CI at the same moment they became the
 * fallback for every case SIMD refuses. And a divergence between the two is
 * exactly the class of bug that produces wrong solve results rather than a
 * crash.
 *
 * Each puzzle is solved twice on the same inputs -- once with the SIMD solver
 * enabled, once with BURRTOOLS_NO_SIMD=1 forcing DLX -- and the two must agree
 * on assembly count, solution count and disassembly move level.
 */
TEST_CASE("SIMD and DLX solvers agree across the regression corpus",
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
  };

  bool tookDifferentPaths = false;

  for (const auto & c : cases) {
    INFO("puzzle: " << c.path);

    SolveResult simd;
    {
      ScopedEnv env("BURRTOOLS_NO_SIMD", nullptr);
      simd = solvePuzzle(c.path, c.prob, c.disassemble);
    }

    SolveResult dlx;
    {
      ScopedEnv env("BURRTOOLS_NO_SIMD", "1");
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

/* An interrupted SIMD search must not save itself as a resumable position.
 *
 * Before this was handled, simdSearch() kept its search state inside the solver,
 * so an aborted run left pos == 0. When saved to an .xmpuzzle, it was
 * indistinguishable from a fresh unstarted search, so resuming it replayed
 * the entire search and duplicated all found assemblies.
 */
TEST_CASE("SIMD assembler: an interrupted search is not restored as resumable",
          "[assembler][simd][resume]") {
  auto p = puzzle_c::load("examples/Bermuda.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  assembler_0_c assm(*problem);
  assm.setNumThreads(1);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

  int seen = 0;
  assm.assemble([&seen](std::unique_ptr<assembly_c>) -> bool {
    seen++;
    return false;
  });
  REQUIRE(seen == 1);
  REQUIRE(assm.getFinished() < 1.0f);

  std::string state;
  {
    std::ostringstream str;
    xmlWriter_c xml(str);
    assm.save(xml);
    state = str.str();
  }

  assembler_0_c restored(*problem);
  REQUIRE(restored.createMatrix(false, false, false) == assembler_c::ERR_NONE);

  std::string payload = extractAssemblerContent(state);
  CHECK(restored.setPosition(payload.c_str(), assemblerVersionOf(state).c_str())
        == assembler_c::ERR_CAN_NOT_RESTORE_INTERRUPTED);
}

TEST_CASE("assembler 1: getFinished does not report 100% before starting a restored search",
          "[assembler][huang][resume]") {
  auto p = puzzle_c::load("examples/CubeInCage.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  assembler_1_c assm(*problem);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

  // Take 1 step in the search: search is NOT finished yet
  assm.debug_step(1);
  REQUIRE(assm.getIterations() > 0);

  // A partially searched assembler must not report finished: after one
  // debug step the run token is unstopped and stacks are non-empty, so the
  // progress estimate has to stay below 1.0f (a stale completion flag or an
  // "idle means done" shortcut would falsely return 1.0f here).
  CHECK(assm.getFinished() < 1.0f);
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

  solveThread_c thread(*problem, solveThread_c::PAR_REDUCE | solveThread_c::PAR_JUST_COUNT);
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
  ScopedEnv noSimd("BURRTOOLS_NO_SIMD", forceDlx ? "1" : nullptr);

  auto p = puzzle_c::load(path);
  REQUIRE(p != nullptr);
  problem_c * problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  std::unique_ptr<assembler_c> assm = problem->getPuzzle().getGridType()->findAssembler(*problem);
  REQUIRE(assm != nullptr);
  REQUIRE(assm->createMatrix(false, false, false) == assembler_c::ERR_NONE);

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
TEST_CASE("Assembler 1 SIMD search agrees with DLX on a reduced matrix", "[solver][simd][reduce]") {
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

TEST_CASE("Parallel assembler 0 SIMD: completed search does not replay on second assemble or on restore", "[solver][replay][simd]") {
  ScopedEnv noSimd("BURRTOOLS_NO_SIMD", nullptr);
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  problem_c * problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  std::unique_ptr<assembler_c> assm = std::make_unique<assembler_0_c>(*problem);
  REQUIRE(assm != nullptr);
  REQUIRE(assm->createMatrix(false, false, false) == assembler_c::ERR_NONE);

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
    assm->save(writer);
  }
  xmlParser_c parser(ss);
  parser.nextTag();
  parser.require(xmlParser_c::START_TAG, "assembler");
  std::string version = parser.getAttributeValue("version");
  std::string state = parser.nextText();

  std::unique_ptr<assembler_c> assm2 = std::make_unique<assembler_0_c>(*problem);
  REQUIRE(assm2 != nullptr);
  REQUIRE(assm2->createMatrix(false, false, false) == assembler_c::ERR_NONE);
  assembler_c::errState err = assm2->setPosition(state.c_str(), version.c_str());
  CHECK(err == assembler_c::ERR_NONE);
  CHECK(assm2->getFinished() == 1.0f);

  TestAssemblerCallback cb3(nullptr);
  assm2->assemble(&cb3);
  CHECK(cb3.assemblies == 0);
}

TEST_CASE("Single-threaded assembler 0 SIMD: completed search does not replay on second assemble or on restore", "[solver][replay][simd]") {
  ScopedEnv threads("BURRTOOLS_THREADS", "1");
  ScopedEnv noSimd("BURRTOOLS_NO_SIMD", nullptr);
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  problem_c * problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  std::unique_ptr<assembler_c> assm = std::make_unique<assembler_0_c>(*problem);
  REQUIRE(assm != nullptr);
  REQUIRE(assm->createMatrix(false, false, false) == assembler_c::ERR_NONE);

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
    assm->save(writer);
  }
  xmlParser_c parser(ss);
  parser.nextTag();
  parser.require(xmlParser_c::START_TAG, "assembler");
  std::string version = parser.getAttributeValue("version");
  std::string state = parser.nextText();

  std::unique_ptr<assembler_c> assm2 = std::make_unique<assembler_0_c>(*problem);
  REQUIRE(assm2 != nullptr);
  REQUIRE(assm2->createMatrix(false, false, false) == assembler_c::ERR_NONE);
  assembler_c::errState err = assm2->setPosition(state.c_str(), version.c_str());
  CHECK(err == assembler_c::ERR_NONE);
  CHECK(assm2->getFinished() == 1.0f);

  TestAssemblerCallback cb3(nullptr);
  assm2->assemble(&cb3);
  CHECK(cb3.assemblies == 0);
}

TEST_CASE("Parallel assembler 1 SIMD: completed search does not replay on second assemble or on restore", "[solver][replay][simd]") {
  ScopedEnv noSimd("BURRTOOLS_NO_SIMD", nullptr);
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  problem_c * problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  std::unique_ptr<assembler_c> assm = std::make_unique<assembler_1_c>(*problem);
  REQUIRE(assm != nullptr);
  REQUIRE(assm->createMatrix(false, false, false) == assembler_c::ERR_NONE);

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
    assm->save(writer);
  }
  xmlParser_c parser(ss);
  parser.nextTag();
  parser.require(xmlParser_c::START_TAG, "assembler");
  std::string version = parser.getAttributeValue("version");
  std::string state = parser.nextText();

  std::unique_ptr<assembler_c> assm2 = std::make_unique<assembler_1_c>(*problem);
  REQUIRE(assm2 != nullptr);
  REQUIRE(assm2->createMatrix(false, false, false) == assembler_c::ERR_NONE);
  assembler_c::errState err = assm2->setPosition(state.c_str(), version.c_str());
  CHECK(err == assembler_c::ERR_NONE);
  CHECK(assm2->getFinished() == 1.0f);

  TestAssemblerCallback cb3(nullptr);
  assm2->assemble(&cb3);
  CHECK(cb3.assemblies == 0);
}

TEST_CASE("Single-threaded assembler 1 SIMD: completed search does not replay on second assemble or on restore", "[solver][replay][simd]") {
  ScopedEnv threads("BURRTOOLS_THREADS", "1");
  ScopedEnv noSimd("BURRTOOLS_NO_SIMD", nullptr);
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  problem_c * problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  std::unique_ptr<assembler_c> assm = std::make_unique<assembler_1_c>(*problem);
  REQUIRE(assm != nullptr);
  REQUIRE(assm->createMatrix(false, false, false) == assembler_c::ERR_NONE);

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
    assm->save(writer);
  }
  xmlParser_c parser(ss);
  parser.nextTag();
  parser.require(xmlParser_c::START_TAG, "assembler");
  std::string version = parser.getAttributeValue("version");
  std::string state = parser.nextText();

  std::unique_ptr<assembler_c> assm2 = std::make_unique<assembler_1_c>(*problem);
  REQUIRE(assm2 != nullptr);
  REQUIRE(assm2->createMatrix(false, false, false) == assembler_c::ERR_NONE);
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
 * colCount/weight, and assembler_0's upDown) are push_back-only and are never
 * cleared between calls, so a second createMatrix followed by a second
 * assemble() on the same instance is untested here and likely unsound. Do
 * not copy this pattern into a test that calls assemble() twice.
 */
TEST_CASE("a freshly prepared assembler does not report itself finished",
          "[assembler][progress]") {
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  SECTION("assembler_0") {
    assembler_0_c assm(*problem);
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    CHECK(assm.getFinished() < 1.0f);

    TestAssemblerCallback cb;
    assm.assemble(&cb);
    CHECK(assm.getFinished() == 1.0f);

    /* preparing again must restart, not inherit the completed state */
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    CHECK(assm.getFinished() < 1.0f);
  }

  SECTION("assembler_1") {
    assembler_1_c assm(*problem);
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    CHECK(assm.getFinished() < 1.0f);

    TestAssemblerCallback cb;
    assm.assemble(&cb);
    CHECK(assm.getFinished() == 1.0f);

    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    CHECK(assm.getFinished() < 1.0f);
  }

  /* The 4-thread sections above always take the parallel path, which sets
   * totalTasks and therefore never touches the total == 0 fallback in
   * getFinished(). A single-threaded run falls to simdSearch()/iterative()
   * instead (assemble() routes to the parallel path only when threads > 1),
   * which is exactly the path that under-reported completion before the
   * searchComplete check was hoisted above the total > 0 branch.
   */
  SECTION("assembler_0 single-threaded") {
    assembler_0_c assm(*problem);
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    CHECK(assm.getFinished() < 1.0f);

    TestAssemblerCallback cb;
    assm.assemble(&cb);
    CHECK(assm.getFinished() == 1.0f);
  }

  SECTION("assembler_1 single-threaded") {
    assembler_1_c assm(*problem);
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
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
 */
ProgressTrace traceSolveStoppingAfter(assembler_c & assm, assembler_cb & cb,
                                      std::chrono::milliseconds budget,
                                      std::chrono::milliseconds interval) {
  ProgressTrace t;
  std::atomic<bool> done{false};
  std::thread sampler([&]{
    while (!done.load(std::memory_order_relaxed)) {
      t.samples.push_back(assm.getFinished());
      std::this_thread::sleep_for(interval);
    }
  });
  std::thread stopper([&]{
    auto deadline = std::chrono::steady_clock::now() + budget;
    while (!done.load(std::memory_order_relaxed) &&
           std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    assm.stop();
  });
  assm.assemble(&cb);
  done.store(true, std::memory_order_relaxed);
  sampler.join();
  stopper.join();
  return t;
}

}

/* The reported regression was not that progress stopped, but that it moved in
 * coarse steps with long stalls -- on Burr-Glar the bar held 99.06% for 32 s of
 * a 315 s solve, and the derived time estimate predicted 2.7 s remaining while
 * 30.3 s were left. A task contributed nothing at all until it completed, so
 * the curve was a staircase of at most one step per task.
 */
TEST_CASE("parallel assembly progress advances smoothly",
          "[assembler][parallel][progress]") {
  auto p = puzzle_c::load("examples/Burr-Glar.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  assembler_0_c assm(*problem);
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

  TestAssemblerCallback cb;
  ProgressTrace t = traceSolveStoppingAfter(assm, cb, std::chrono::milliseconds(3000),
                                            std::chrono::milliseconds(20));

  /* drop the terminal sample of the stopped run before asserting on shape */
  REQUIRE(t.samples.size() > 1);
  t.samples.pop_back();

  INFO("samples: " << t.samples.size()
       << " distinct: " << t.distinctValues()
       << " longest plateau: " << t.longestPlateauFraction());

  REQUIRE(t.samples.size() > 20);
  CHECK(t.inRange());
  CHECK(t.monotone());
  CHECK(t.longestPlateauFraction() < 0.5);
  CHECK(*std::max_element(t.samples.begin(), t.samples.end()) < 1.0f);
  CHECK(t.distinctValues() > t.samples.size() / 10);
}

/* The brief for these progress tests names examples/HexSticks.xmpuzzle, but
 * that puzzle has parts with a piece count > 1, which assembler_0_c::
 * canHandle() rejects outright (ERR_PUZZLE_UNHANDABLE) -- as it does
 * CubeInCage and the other multi-count examples. Re-verified on this base.
 * PelikanBurr.xmpuzzle, already used throughout this file, is a single-count
 * puzzle both assemblers accept, so it is used here instead.
 */
TEST_CASE("parallel assembly progress is monotone and ends at 1.0",
          "[assembler][parallel][progress]") {
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  assembler_0_c assm(*problem);
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

  TestAssemblerCallback cb;
  ProgressTrace t = traceSolve(assm, cb);

  CHECK(t.inRange());
  CHECK(t.monotone());
  CHECK(assm.getFinished() == 1.0f);
}

/* assembler_0_c grants this exact type (declared at global scope, matching the
 * friend declaration in assembler_0.h -- an unnamed-namespace version would be
 * a distinct type and would NOT be granted friendship) access to
 * generateSubtreeTasks() and SubtreeTask::share, so the share-conservation
 * invariant can be asserted directly instead of only inferred from
 * getFinished() end-to-end behaviour.
 */
struct SubtreeTaskShareTestAccess {
  /* Mirrors the targetTasks/maxDepth formula parallelMultiSearch uses.
   * Returns prunedShare plus the sum of every live task's share via the
   * return value, and the raw prunedShare via `outPrunedShare` -- callers must
   * check both: the sum alone can pass vacuously if pruning stops happening
   * entirely (a prunedShare of 0 folded into live shares that already summed
   * to 1 on their own says nothing about whether the leak this exists to
   * catch is still fixed). Pruned subtrees (colCount reaching 0, or the holes
   * budget running out) are dropped by generateSubtreeTasks without ever
   * becoming a task; their share must still be folded into prunedShare, or the
   * sum falls short of 1.0.
   */
  static double prunedPlusLiveShare(assembler_0_c & assm, unsigned int workers,
                                    float & outPrunedShare) {
    unsigned int targetTasks = std::max(16u, workers * 4);
    unsigned int maxDepth = std::min(assm.piecenumber > 1 ? assm.piecenumber - 1 : 1u, 3u);

    std::vector<assembler_0_c::SubtreeTask> tasks;
    float prunedShare = 0.0f;
    assm.generateSubtreeTasks(tasks, targetTasks, maxDepth, prunedShare);
    outPrunedShare = prunedShare;

    double sum = prunedShare;
    for (const auto & task : tasks) sum += task.share;
    return sum;
  }
};

/* DiagonalCube.xmpuzzle was picked by probing every assembler_0_c-compatible
 * example puzzle directly: at the shallow depth (<=3) and task budget (16)
 * generateSubtreeTasks actually uses, most puzzles never prune (a dead column
 * or a holes-budget miss needs specific structure to show up this early).
 * DiagonalCube does -- about half its search tree is pruned within the first
 * three placements -- so this test would have proven nothing on a puzzle that
 * never exercises the pruning path.
 */
TEST_CASE("pruned subtree shares are folded into completedShare, not dropped",
          "[assembler][progress]") {
  auto p = puzzle_c::load("examples/DiagonalCube.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  assembler_0_c assm(*problem);
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

  float prunedShare = 0.0f;
  double sum = SubtreeTaskShareTestAccess::prunedPlusLiveShare(assm, 4, prunedShare);

  INFO("prunedShare: " << prunedShare << " sum: " << sum);

  /* Guards against this test going vacuous: without it, a future drift that
   * makes DiagonalCube (or the pruning heuristics) stop pruning entirely would
   * still pass the sum check below, silently ceasing to guard the leak this
   * test exists to catch.
   */
  CHECK(prunedShare > 0.0);
  CHECK(sum == Catch::Approx(1.0).margin(1e-6));
}

/* A solve aborted on the parallel path leaves totalTasks and completedShare
 * holding that run's numbers, and totalTasks is what selects the task-based
 * branch of getFinished(). Without assemble() clearing them for a non-parallel
 * run, a resume with one thread reports the abandoned run's constant share for
 * the whole of the serial search.
 *
 * Two observations that must differ, taken from the main thread with nothing
 * else alive, so it is deterministic and needs no sampling thread.
 *
 * DiagonalCube rather than PelikanBurr, and the difference is this base's, not
 * a preference. Here a task the run token cut short is NOT counted as
 * complete (completedTasks and completedShare are only updated by
 * finishTask() when the task actually finished), so on a puzzle that prunes
 * nothing an abort landing on the first assembly leaves completedShare at
 * exactly 0 and there is no stale value to leak. DiagonalCube prunes about
 * half its tree during task generation, and that pruned share is seeded into
 * completedShare before any worker starts -- so the stale value is non-zero
 * and deterministic whenever the abort lands.
 */
TEST_CASE("parallel progress state does not leak into a serial resume",
          "[assembler][parallel][progress]") {
  auto p = puzzle_c::load("examples/DiagonalCube.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  assembler_0_c assm(*problem);
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

  /* a parallel run, stopped from the callback so the abort is deterministic */
  int parallelAssemblies = 0;
  assm.assemble([&](std::unique_ptr<assembly_c>) -> bool {
    parallelAssemblies++;
    return false;
  });
  REQUIRE(parallelAssemblies == 1);

  const float stale = assm.getFinished();
  INFO("stale parallel share: " << stale);
  REQUIRE(stale > 0.0f);
  REQUIRE(stale < 1.0f);

  /* resume single-threaded and stop it the same way */
  assm.setNumThreads(1);
  int serialAssemblies = 0;
  assm.assemble([&](std::unique_ptr<assembly_c>) -> bool {
    serialAssemblies++;
    return false;
  });
  REQUIRE(serialAssemblies == 1);

  INFO("progress after the serial resume: " << assm.getFinished());
  CHECK(assm.getFinished() != stale);
}

/* The share accumulator is only seeded -- to prunedTaskShare -- when a fresh
 * task list is generated, at the top of parallelMultiSearch, on that thread,
 * before any worker exists. A resumed run finds parallelTasks already
 * holding the pool remainder a prior pause drained back into it, skips
 * generation entirely, and therefore skips the seed too: completedShare is
 * simply left alone, carrying forward exactly what the paused run had
 * already accumulated. The workers need no reset discipline of their own.
 *
 * This asserts the property that matters to the user: resuming a paused solve
 * does not throw away the progress already made. If completedShare were
 * reset on every call, the resumed run would restart the accumulator at 0
 * (or the pruned share alone) and the bar would jump backwards on every
 * pause.
 *
 * Burr-Glar because the seed has to be interesting: it must contain completed
 * tasks, not just pruned ones. Every other bundled puzzle assembler_0_c
 * accepts finishes in milliseconds -- measured on PelikanBurr, a pause after
 * five assemblies completes no task at all and leaves the accumulator at 0,
 * so it cannot tell a seeded resume from an unseeded one. The second phase
 * only needs to get as far as the seeding, so its budget is tiny.
 */
TEST_CASE("a resumed parallel run picks the share up where it left off",
          "[assembler][parallel][progress][resume]") {
  auto p = puzzle_c::load("examples/Burr-Glar.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  assembler_0_c assm(*problem);
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

  TestAssemblerCallback cb;
  traceSolveStoppingAfter(assm, cb, std::chrono::milliseconds(1000),
                          std::chrono::milliseconds(50));

  const float paused = assm.getFinished();
  INFO("paused at " << paused);

  /* the seed has to carry completed tasks for this to say anything; a pause
   * that completed none would pass vacuously
   */
  REQUIRE(paused > 0.0f);
  REQUIRE(paused < 1.0f);

  /* resume, and stop again almost immediately: all this run has to do is seed */
  traceSolveStoppingAfter(assm, cb, std::chrono::milliseconds(50),
                          std::chrono::milliseconds(10));

  INFO("resumed at " << assm.getFinished());
  CHECK(assm.getFinished() >= paused);
}

/* Forces the DLX back end for the length of a TEST_CASE.
 *
 * assembler_1_c has two parallel back ends and only the DLX one can report how
 * far into a task a worker has got -- the SIMD solver has no such hook, so it
 * keeps whole-task granularity. Which one runs is decided by canUseSimd(), and
 * on this tree the SIMD back end has grown to cover every bundled example that
 * runs for long enough to sample: the puzzle the reference implementation used
 * to reach the DLX path (HexSticks) is now taken by SIMD, as is Burr-Glar.
 * Measured on every example and problem: only DemoMirrorParadox,
 * DemoPieceGenerator and PiecesOfEight still reach the DLX path by default,
 * and the longest of those runs 171 ms.
 *
 * So rather than pick a puzzle by what canUseSimd() happens to reject today --
 * which is exactly the choice that silently stopped testing anything when the
 * SIMD coverage widened -- the path is selected explicitly, by the environment
 * variable canUseSimd() already honours.
 */
namespace {

struct ScopedNoSimd {
  bool hadValue;
  std::string oldValue;

  ScopedNoSimd() {
    const char * existing = std::getenv("BURRTOOLS_NO_SIMD");
    hadValue = (existing != nullptr);
    if (hadValue) oldValue = existing;
#ifdef _WIN32
    _putenv_s("BURRTOOLS_NO_SIMD", "1");
#else
    setenv("BURRTOOLS_NO_SIMD", "1", 1);
#endif
  }

  ~ScopedNoSimd() {
#ifdef _WIN32
    _putenv_s("BURRTOOLS_NO_SIMD", hadValue ? oldValue.c_str() : "");
#else
    if (hadValue) setenv("BURRTOOLS_NO_SIMD", oldValue.c_str(), 1);
    else unsetenv("BURRTOOLS_NO_SIMD");
#endif
  }
};

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

  assembler_1_c assm(*problem);
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

  TestAssemblerCallback cb;
  ProgressTrace t = traceSolve(assm, cb);

  INFO("samples: " << t.samples.size()
       << " distinct: " << t.distinctValues()
       << " longest plateau: " << t.longestPlateauFraction());

  REQUIRE(t.samples.size() > 1);
  CHECK(t.inRange());

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
   * wide, against the microseconds of a whole slot walk it replaces.
   */
  CHECK(t.monotone());

  CHECK(assm.getFinished() == 1.0f);
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

  assembler_1_c assm(*problem);
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

  TestAssemblerCallback cb;
  ProgressTrace t = traceSolveStoppingAfter(assm, cb, std::chrono::milliseconds(3000),
                                            std::chrono::milliseconds(20));

  /* drop the terminal sample of the stopped run before asserting on shape */
  REQUIRE(t.samples.size() > 1);
  t.samples.pop_back();

  INFO("samples: " << t.samples.size()
       << " distinct: " << t.distinctValues()
       << " longest plateau: " << t.longestPlateauFraction());

  REQUIRE(t.samples.size() > 20);
  CHECK(t.inRange());

  /* see the note on the handoff window in the test above */
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

  /* The two assertions about the SHAPE of the curve -- how long it can stand
   * still and how many values it takes -- need the search to be running at a
   * realistic node rate, because a worker publishes once every 4096 nodes and
   * that is what sets how often the curve can move at all. This build does
   * ~12.2M nodes in the budget, so a worker publishes every ~4 ms against a
   * 20 ms sampling interval, and the curve has room to be smooth.
   *
   * A ThreadSanitizer build does ~253K -- 48x slower -- which stretches the
   * publication interval to ~195 ms and makes the sampled curve a picture of
   * the instrumentation rather than of the progress code: measured there,
   * 22 distinct values of 114 with a 37% plateau, against 115 of 117 and 2.6%
   * here. The case still runs under the sanitizer, and everything above this
   * point is still asserted there, because what the sanitizer is for is the
   * cross-thread publication these samples drive. Only the shape is left to
   * builds that can produce one, and a build that cannot says so rather than
   * passing quietly.
   */
  if (assm.getIterations() > 2000000) {
    CHECK(t.longestPlateauFraction() < 0.5);
    CHECK(t.distinctValues() > t.samples.size() / 10);
  } else {
    WARN("search too slow for the shape assertions: " << assm.getIterations()
         << " nodes, " << t.distinctValues() << " distinct of " << t.samples.size()
         << ", longest plateau " << t.longestPlateauFraction());
  }
}

/* getFinished() has to switch progress sources BACK when a run is not
 * parallel, not merely switch them over when it is. A solve aborted on the
 * parallel path leaves completedTasks and totalTasks holding that run's
 * numbers; if the flag selecting the task-based source is never cleared, a
 * resume with one thread reports that abandoned run's ratio -- a constant,
 * wrong number -- for the whole of the serial search.
 *
 * Clearing the flag alone is not enough, which is why this test was written
 * first: getFinished() has two task-ratio branches gated on two pieces of
 * state, and clearing only the flag moves the stale value from the first
 * branch to the second. It failed with bit-identical values either way.
 *
 * The check is made after both runs have been stopped, from the main thread
 * with nothing else alive, so it is deterministic and adds no sampling thread:
 * the stale ratio is exactly the value getFinished() would keep returning, and
 * the serial estimate reproduces it only by coincidence.
 */
TEST_CASE("Huang progress leaves the task-based source when a run is not parallel",
          "[assembler][parallel][progress]") {
  ScopedNoSimd dlxPath;

  auto p = puzzle_c::load("examples/HexSticks.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  assembler_1_c assm(*problem);
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

  /* a parallel run, stopped from the callback so the abort is deterministic.
   * Tasks that had already drained when the abort landed are counted, so
   * completedTasks is non-zero and the stale ratio is too.
   */
  int parallelAssemblies = 0;
  assm.assemble([&](std::unique_ptr<assembly_c>) -> bool {
    parallelAssemblies++;
    return false;
  });
  REQUIRE(parallelAssemblies == 1);

  const float stale = assm.getFinished();
  INFO("stale parallel ratio: " << stale);
  REQUIRE(stale > 0.0f);
  REQUIRE(stale < 1.0f);

  /* resume single-threaded and stop it the same way */
  assm.setNumThreads(1);
  int serialAssemblies = 0;
  assm.assemble([&](std::unique_ptr<assembly_c>) -> bool {
    serialAssemblies++;
    return false;
  });
  REQUIRE(serialAssemblies == 1);

  INFO("progress after the serial resume: " << assm.getFinished());
  CHECK(assm.getFinished() != stale);
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

  assembler_1_c assm(*problem);
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

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
 * in worker-seconds: wall time times this number. It has to be the width the
 * run will ACTUALLY have, not the configured one -- a resumed search runs
 * serially on both engines however many threads are set, and charging it for
 * four over-weights the whole phase by four.
 */
TEST_CASE("getRunThreads reports the width the next run will really have",
          "[assembler][parallel][progress]") {

  SECTION("assembler_0") {
    auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
    REQUIRE(p != nullptr);
    auto problem = p->getProblem(0);
    REQUIRE(problem != nullptr);

    assembler_0_c assm(*problem);
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

    /* fresh: the parallel path, so the configured width */
    CHECK(assm.getRunThreads() == 4);

    /* one thread configured is a serial run whatever the state */
    assm.setNumThreads(1);
    CHECK(assm.getRunThreads() == 1);

    /* a resumed search is routed to the serial path even with four threads
     * configured. Restoring a saved position is how a resume actually reaches
     * the assembler: the GUI writes it into the puzzle file on pause and
     * setPosition() rebuilds the partial DLX stack from it.
     * "0 1 0 (0 0)(0 0)" is the interrupted flag clear, pos=1, 0 iterations
     * and two empty (row column) pairs -- the smallest string that leaves pos
     * non-zero without covering anything.
     */
    assm.setNumThreads(4);
    REQUIRE(assm.setPosition("0 1 0 (0 0)(0 0)", "1.5") == assembler_c::ERR_NONE);
    CHECK(assm.getRunThreads() == 1);
  }

  SECTION("assembler_1") {
    /* The serial run below has to be the DLX one for the stated mechanism to
     * be the real one: simdSearch() never touches next_row_stack, so with the
     * SIMD back end the final check would pass on rows being left non-empty by
     * the solution callback instead -- right answer, wrong reason, and it
     * would stop holding on a puzzle with no assemblies. PelikanBurr rather
     * than HexSticks because the DLX serial path has to run to completion
     * here: 5272 nodes against 1.8M.
     */
    ScopedNoSimd dlxPath;

    auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
    REQUIRE(p != nullptr);
    auto problem = p->getProblem(0);
    REQUIRE(problem != nullptr);

    assembler_1_c assm(*problem);
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

    CHECK(assm.getRunThreads() == 4);

    assm.setNumThreads(1);
    CHECK(assm.getRunThreads() == 1);

    /* run the serial path to completion: next_row_stack is drained, so the
     * stacks are no longer at the initial state the task generator needs and
     * the next call cannot go parallel however many threads are configured
     */
    TestAssemblerCallback cb;
    assm.assemble(&cb);
    REQUIRE(cb.assemblies > 0);
    assm.setNumThreads(4);
    CHECK(assm.getRunThreads() == 1);
  }
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
 */
TEST_CASE("solve thread reports monotone whole-solve progress",
          "[solvethread][progress]") {
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  /* the shipped examples are saved already solved, and solveThread_c asserts
   * solveState == SS_UNSOLVED on the way in
   */
  problem->removeAllSolutions();

  solveThread_c thread(*problem, solveThread_c::PAR_DISASSM);

  /* nothing has started yet, so there is nothing to report and no assembler
   * to read it from */
  CHECK(thread.getProgress() == 0.0f);

  std::vector<float> samples;
  REQUIRE(thread.start());

  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);
  while (!thread.stopped() &&
         thread.currentAction() != solveThread_c::ACT_ASSERT &&
         std::chrono::steady_clock::now() < deadline) {
    samples.push_back(thread.getProgress());
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  REQUIRE(thread.currentAction() == solveThread_c::ACT_FINISHED);

  /* The final sample may have been taken in the window between the loop's
   * check and the worker setting ACT_FINISHED, so it is allowed to be 1.0;
   * drop it before asserting that a running solve never reports completion.
   */
  REQUIRE(samples.size() > 1);
  samples.pop_back();

  INFO("samples: " << samples.size()
       << " first: " << samples.front()
       << " last: " << samples.back());

  for (size_t i = 0; i < samples.size(); i++) {
    CHECK(samples[i] >= 0.0f);
    CHECK(samples[i] < 1.0f);
    if (i) CHECK(samples[i] >= samples[i-1]);
  }

  /* a bar that never moves is monotone and bounded too */
  CHECK(*std::max_element(samples.begin(), samples.end()) > samples.front());

  CHECK(thread.getProgress() == 1.0f);
  CHECK(thread.getProgress() == 1.0f);  // idempotent, the GUI polls repeatedly
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

  solveThread_c thread(*problem, solveThread_c::PAR_DISASSM);
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
    solveThread_c first(*problem, solveThread_c::PAR_DISASSM);
    REQUIRE(first.start());

    /* A wall-clock budget rather than a progress trigger, because the only
     * way to watch the fraction from here is to poll problem_c for the
     * assembler while the worker is still installing it -- an unsynchronised
     * read that ThreadSanitizer flags, and one the production GUI does not
     * make either. Long enough that the parallel run completes whole tasks;
     * REQUIRE(paused > 0) below keeps a pause that achieved nothing from
     * passing vacuously. Same pattern and budget as the sibling resume test.
     */
    const auto pauseAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
    while (!first.stopped() && std::chrono::steady_clock::now() < pauseAt)
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

  solveThread_c second(*problem, solveThread_c::PAR_DISASSM);
  REQUIRE(second.start());

  unsigned int samples = 0;    // samples taken during the assembly phase
  unsigned int dead = 0;       // ... of which found no cost basis left
  unsigned int deadAfterGain = 0;  // ... of those, taken after the run advanced
  unsigned int trusted = 0;    // ... of which were past the model's trust threshold
  float reached = 0.0f, baseSeen = -1.0f;
  double worstLeverage = 0.0; // smallest projected/session cost ratio seen

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
      }

      if (fraction >= progressModel_c::trustThreshold) trusted++;

      if (samples > 300 && fraction >= paused + 0.005f) break;
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
       << ", smallest projected cost per measured second: " << worstLeverage);

  /* Not a vacuous pass: the window has to have covered a real stretch of the
   * assembly phase, with every OTHER condition evaluate() needs to blend
   * satisfied, so the cost basis is the only thing left that can switch the
   * blend off.
   */
  REQUIRE(samples > 100);
  REQUIRE(trusted > 0);
  REQUIRE(disassembled > 0);

  /* The run picked the paused search up rather than starting over: the basis
   * IS the fraction the first thread reached.
   */
  CHECK(baseSeen == paused);

  /* Never charged nothing once it had measured something. */
  CHECK(deadAfterGain == 0);

  /* And the head start is actually paid for. The first thread bought ~84% of
   * the search and its seconds died with it; this one measures only the sliver
   * past that, so every measured second has to be charged as many times over
   * as the sliver is small -- a/(a-base), which over this window is tens. At a
   * leverage of 1 the phase would be charged what this run alone spent, which
   * is the defect: asmCost/a collapses, assembly stops counting against
   * disassembly in the blend, and both the bar and the time estimate are wrong
   * by that factor.
   */
  CHECK(worstLeverage > 5.0);
}
