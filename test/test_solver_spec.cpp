/* solverSpec_t (src/lib/solver/solverspec.h): a spec built from a problem
 * answers every accessor as the problem_c does, for every problem of every
 * example file, and is a snapshot the document's edits do not reach. */

#include <catch2/catch_test_macros.hpp>

#include "lib/gridtype.h"
#include "lib/problem.h"
#include "lib/puzzle.h"
#include "lib/symmetries.h"
#include "lib/voxel.h"
#include "lib/solver/solverspec.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace {

std::vector<std::filesystem::path> exampleFiles(void) {
  std::vector<std::filesystem::path> files;
  for (const auto & e : std::filesystem::directory_iterator("examples"))
    if (e.path().extension() == ".xmpuzzle")
      files.push_back(e.path());
  std::sort(files.begin(), files.end());
  return files;
}

/* a copy holds the same cells, colours, hotspot and symmetries, but is not the same object */
void checkSameShape(const voxel_c * copy, const voxel_c * orig) {
  REQUIRE(copy != nullptr);
  REQUIRE(orig != nullptr);
  CHECK(copy != orig);
  CHECK(copy->getGridType() == orig->getGridType());
  REQUIRE(copy->getX() == orig->getX());
  REQUIRE(copy->getY() == orig->getY());
  REQUIRE(copy->getZ() == orig->getZ());
  for (unsigned int i = 0; i < orig->getXYZ(); i++)
    REQUIRE(copy->get(i) == orig->get(i));
  CHECK(copy->getHx() == orig->getHx());
  CHECK(copy->getHy() == orig->getHy());
  CHECK(copy->getHz() == orig->getHz());
  CHECK(copy->selfSymmetries() == orig->selfSymmetries());
  CHECK(copy->countState(voxel_c::VX_FILLED) == orig->countState(voxel_c::VX_FILLED));
}

void checkSpecMatchesProblem(const solverSpec_t & spec, const problem_c & problem) {

  const puzzle_c & puzzle = problem.getPuzzle();

  CHECK(spec.getGridType() == puzzle.getGridType());
  CHECK(spec.getGridType()->getSymmetries() == puzzle.getGridType()->getSymmetries());
  CHECK(spec.colorNumber() == puzzle.colorNumber());
  CHECK(spec.getRotationModel().has_value() == puzzle.getRotationModel().has_value());
  if (spec.getRotationModel() && puzzle.getRotationModel()) {
    CHECK(spec.getRotationModel()->play == puzzle.getRotationModel()->play);
    CHECK(spec.getRotationModel()->chamfer == puzzle.getRotationModel()->chamfer);
  }

  REQUIRE(spec.getNumberOfParts() == problem.getNumberOfParts());
  CHECK(spec.getNumberOfPieces() == problem.getNumberOfPieces());

  for (unsigned int part = 0; part < problem.getNumberOfParts(); part++) {
    INFO("part " << part);
    CHECK(spec.getPartMinimum(part) == problem.getPartMinimum(part));
    CHECK(spec.getPartMaximum(part) == problem.getPartMaximum(part));
    CHECK(spec.getShapeIdOfPart(part) == problem.getShapeIdOfPart(part));
    checkSameShape(spec.getPartShape(part), problem.getPartShape(part));
    REQUIRE(spec.getNumberOfPartGroups(part) == problem.getNumberOfPartGroups(part));
    for (unsigned int g = 0; g < problem.getNumberOfPartGroups(part); g++) {
      CHECK(spec.getPartGroupId(part, g) == problem.getPartGroupId(part, g));
      CHECK(spec.getPartGroupCount(part, g) == problem.getPartGroupCount(part, g));
    }
  }

  for (unsigned int piece = 0; piece < problem.getNumberOfPieces(); piece++)
    CHECK(spec.getPartIdToPieceId(piece) == problem.getPartIdToPieceId(piece));

  REQUIRE(spec.resultValid() == problem.resultValid());
  if (problem.resultValid()) {
    CHECK(spec.getResultId() == problem.getResultId());
    checkSameShape(spec.getResultShape(), getResultShape(problem));
    CHECK(getResultShape(spec) == spec.getResultShape());
  }

  for (unsigned int pc = 0; pc <= puzzle.colorNumber(); pc++)
    for (unsigned int res = 0; res <= puzzle.colorNumber(); res++)
      CHECK(spec.placementAllowed(pc, res) == problem.placementAllowed(pc, res));

  REQUIRE(spec.maxHolesDefined() == problem.maxHolesDefined());
  if (problem.maxHolesDefined())
    CHECK(spec.getMaxHoles() == problem.getMaxHoles());

  /* exactly the shapes the problem uses are copied, indexed by shape id */
  REQUIRE(spec.shapes.size() == puzzle.getNumberOfShapes());
  for (unsigned int s = 0; s < puzzle.getNumberOfShapes(); s++) {
    INFO("shape " << s);
    CHECK((spec.shapes[s] != nullptr) == problem.usesShape(s));
  }
}

}

TEST_CASE("solverSpec_t answers as the problem_c it was built from, for every example", "[solverspec]") {

  const std::vector<std::filesystem::path> files = exampleFiles();
  REQUIRE(files.size() > 10);

  unsigned int problems = 0;
  for (const auto & path : files) {
    /* both readings of rhombic and tetra-octa files: NG's grids and the native ones */
    for (cutCubeGrids_e cut : { cutCubeGrids_e::ng, cutCubeGrids_e::native }) {
      INFO(path.string() << (cut == cutCubeGrids_e::ng ? " (ng)" : " (native)"));
      std::unique_ptr<puzzle_c> puzzle = puzzle_c::load(path, cut);
      REQUIRE(puzzle != nullptr);
      for (unsigned int p = 0; p < puzzle->getNumberOfProblems(); p++) {
        INFO("problem " << p);
        std::shared_ptr<const solverSpec_t> spec = makeSolverSpec(*puzzle->getProblem(p));
        REQUIRE(spec != nullptr);
        checkSpecMatchesProblem(*spec, *puzzle->getProblem(p));
        problems++;
      }
    }
  }
  CHECK(problems >= 2 * files.size());
}

TEST_CASE("solverSpec_t is a snapshot: edits of the document do not reach it", "[solverspec]") {

  std::unique_ptr<puzzle_c> puzzle = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(puzzle != nullptr);
  problem_c * problem = puzzle->getProblem(0);
  REQUIRE(problem->getNumberOfParts() > 0);

  std::shared_ptr<const solverSpec_t> spec = makeSolverSpec(*problem);
  const unsigned int filled = spec->getPartShape(0)->countState(voxel_c::VX_FILLED);
  const unsigned int max0 = spec->getPartMaximum(0);

  /* empty the first part's shape and change its count in the document */
  voxel_c * shape = problem->getPartShape(0);
  for (unsigned int i = 0; i < shape->getXYZ(); i++)
    shape->setState(i, voxel_c::VX_EMPTY);
  problem->setShapeMaximum(problem->getShapeIdOfPart(0), max0 + 3);

  CHECK(spec->getPartShape(0)->countState(voxel_c::VX_FILLED) == filled);
  CHECK(spec->getPartMaximum(0) == max0);

  /* and it outlives the puzzle: it shares the grid */
  const gridType_c::gridType type = spec->getGridType()->getType();
  puzzle.reset();
  CHECK(spec->getGridType()->getType() == type);
  CHECK(spec->getPartShape(0)->getGridType() == spec->getGridType());
}

TEST_CASE("solverSpec_t of a problem without a result or with a hole limit", "[solverspec]") {

  std::unique_ptr<puzzle_c> puzzle = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(puzzle != nullptr);
  problem_c * problem = puzzle->getProblem(0);

  problem->setMaxHoles(2);
  checkSpecMatchesProblem(*makeSolverSpec(*problem), *problem);

  problem->setMaxHolesInvalid();
  problem->clearResult();
  checkSpecMatchesProblem(*makeSolverSpec(*problem), *problem);
}
