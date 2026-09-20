#include <catch2/catch_test_macros.hpp>

#include "lib/puzzle.h"
#include "lib/problem.h"
#include "lib/assembler.h"
#include "lib/assembler_0.h"
#include "lib/assembler_1.h"
#include "lib/assembly.h"
#include "lib/disassembler.h"
#include "lib/disassembler_0.h"
#include "lib/disassembly.h"
#include "lib/gridtype.h"
#include "lib/voxel.h"
#include "tools/xml.h"
#include "tools/gzstream.h"

#include <cstdlib>
#include <sstream>
#include <set>
#include <memory>
#include <string>

namespace {

/* RAII environment variable, so a test can drive the runtime toggles that
 * allowing tests to verify fallback behavior under runtime feature toggles (such as BURRTOOLS_NO_SIMD).
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
  try {
    bt_assert(1 == 2);
    FAIL("bt_assert should have thrown assert_exception");
  } catch (const assert_exception & e) {
    CHECK(std::string(e.expr) == "1 == 2");
    CHECK(std::string(e.file).ends_with("test_solver.cpp"));
    CHECK(e.line > 0);
    CHECK(std::string(e.what()) == "1 == 2");
  }

  // Passing assertion does not throw
  CHECK_NOTHROW([&] { bt_assert(2 + 2 == 4); }());
}

TEST_CASE("assert_log correctly records lines", "[assert]") {
  REQUIRE(assert_log != nullptr);
  unsigned int initialLines = assert_log->lines();
  bt_assert_line("first assert log entry");
  bt_assert_line("second assert log entry");
  CHECK(assert_log->lines() == initialLines + 2);
  CHECK(std::string(assert_log->line(initialLines)) == "first assert log entry");
  CHECK(std::string(assert_log->line(initialLines + 1)) == "second assert log entry");
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

  int serial = 0;
  {
    assembler_0_c assm(*problem);
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    assm.assemble([&serial](std::unique_ptr<assembly_c>) -> bool { serial++; return true; });
  }
  REQUIRE(serial > 0);

  /* Guard the premise rather than trusting the chosen puzzle: keepRotations
   * forces avoidTransformedAssemblies off, so it must yield strictly more
   * assemblies. If the two agree, symmetry breaking is not active here any
   * more and this case has stopped covering the concurrent path.
   */
  {
    int keptRotations = 0;
    assembler_0_c assm(*problem);
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, true, false) == assembler_c::ERR_NONE);
    assm.assemble([&keptRotations](std::unique_ptr<assembly_c>) -> bool { keptRotations++; return true; });
    INFO("symmetry breaking must be active for this test to be meaningful");
    REQUIRE(keptRotations > serial);
  }

  /* Reload so the parallel run starts with cold caches: the lazy fills are
   * first-touch, so a parallel run after a serial run on the same puzzle
   * object races on already-warm caches and reports nothing.
   */
  auto pFresh = puzzle_c::load("examples/Bermuda.xmpuzzle");
  REQUIRE(pFresh != nullptr);
  auto problemFresh = pFresh->getProblem(0);
  REQUIRE(problemFresh != nullptr);

  int parallel = 0;
  assembler_0_c assm(*problemFresh);
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
  assm.assemble([&parallel](std::unique_ptr<assembly_c>) -> bool { parallel++; return true; });

  CHECK(parallel == serial);
}

/* Records a canonical fingerprint of every assembly, so two runs can be
 * compared as multisets instead of by count alone. Equal counts are much
 * weaker than an equal set: one lost assembly plus one duplicated assembly
 * leaves the count untouched, and both are failure modes the parallel search
 * can actually produce.
 */
class RecordingAssemblerCallback : public assembler_cb {
public:
  std::multiset<std::string> fingerprints;

  bool assembly(std::unique_ptr<assembly_c> a) override {
    std::string s;
    for (unsigned int i = 0; i < a->placementCount(); i++) {
      if (a->isPlaced(i)) {
        s += std::to_string(a->getTransformation(i)) + ",";
        s += std::to_string(a->getX(i)) + ",";
        s += std::to_string(a->getY(i)) + ",";
        s += std::to_string(a->getZ(i)) + ";";
      } else {
        s += "-;";
      }
    }
    fingerprints.insert(std::move(s));
    return true;
  }
};

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

  /* Guard the premise: not every puzzle qualifies for the SIMD solver (holes,
   * variable voxels and >512 columns all disqualify it), but if *none* of them
   * does then this case has silently stopped comparing anything and is only
   * running DLX twice. Keep at least one SIMD-eligible puzzle in the list.
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

  // Under old code, !running && !abbort && iterations > 0 caused getFinished() to falsely return 1.0f!
  CHECK(assm.getFinished() < 1.0f);
}

