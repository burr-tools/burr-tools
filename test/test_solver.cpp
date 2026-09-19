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

