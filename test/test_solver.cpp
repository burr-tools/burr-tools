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

#include <sstream>
#include <memory>
#include <string>

namespace {

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
