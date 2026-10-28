/* Tests for the PuzzleSolver3D importer (src/lib/ps3dloader): what it
 * reads, and that a damaged or foreign file is refused -- never read past
 * a line's end, nor a size the grid cannot take, nor a puzzle without a
 * piece -- as File > Import's user picks any file at all.
 */
#include <catch2/catch_test_macros.hpp>

#include "../src/lib/problem.h"
#include "../src/lib/ps3dloader.h"
#include "../src/lib/puzzle.h"
#include "../src/lib/voxel.h"

#include <sstream>
#include <string>

namespace {
  std::unique_ptr<puzzle_c> load(const std::string & text) {
    std::istringstream in(text);
    return loadPuzzlerSolver3D(&in);
  }
}

TEST_CASE("a PuzzleSolver3D file: its pieces, its result, their voxels", "[ps3d]") {
  // two layers (z) side by side on each line, X filled, a blank empty
  const auto p = load("PIECE 2,1,2\n"
                      "X  X\n"
                      "RESULT 2,2,1\n"
                      "XX\n"
                      "X \n");
  REQUIRE(p);
  REQUIRE(p->getNumberOfShapes() == 2);
  const voxel_c & piece = *p->getShape(0);
  CHECK(piece.getX() == 2);
  CHECK(piece.getZ() == 2);
  CHECK(piece.getState(0, 0, 0) == voxel_c::VX_FILLED);
  CHECK(piece.getState(1, 0, 0) == voxel_c::VX_EMPTY);
  CHECK(piece.getState(0, 0, 1) == voxel_c::VX_FILLED);     // the second layer from column 3
  CHECK(piece.getState(1, 0, 1) == voxel_c::VX_EMPTY);
  const problem_c & pr = *p->getProblem(0);
  REQUIRE(pr.resultValid());
  CHECK(pr.getResultId() == 1);
  CHECK(p->getShape(1)->countState(voxel_c::VX_FILLED) == 3);
  CHECK(pr.getShapeMinimum(0) == 1);
}

TEST_CASE("a line cut short reads its missing cells as empty", "[ps3d]") {
  // editors trim trailing blanks: "X" stands for "X " in a 2-wide shape
  const auto p = load("PIECE 2,2,1\nX\n X\nRESULT 1,1,1\nX\n");
  REQUIRE(p);
  CHECK(p->getShape(0)->getState(0, 0, 0) == voxel_c::VX_FILLED);
  CHECK(p->getShape(0)->getState(1, 0, 0) == voxel_c::VX_EMPTY);
  CHECK(p->getShape(0)->getState(1, 1, 0) == voxel_c::VX_FILLED);
  // an empty line: the whole row empty
  const auto q = load("PIECE 3,2,1\n\nXXX\nRESULT 1,1,1\nX\n");
  REQUIRE(q);
  CHECK(q->getShape(0)->countState(voxel_c::VX_FILLED) == 3);
}

TEST_CASE("a damaged or foreign file is refused", "[ps3d]") {
  CHECK_FALSE(load(""));                                     // nothing
  CHECK_FALSE(load("hello, world\nnot a puzzle\n"));         // no piece
  CHECK_FALSE(load("RESULT 1,1,1\nX\n"));                    // a result, no piece
  CHECK_FALSE(load("PIECE 2,3,1\nXX\n"));                    // ends inside a shape
  CHECK_FALSE(load("PIECE 501,1,1\n"));                      // too big for the grid
  CHECK_FALSE(load("PIECE -2,1,1\nX\n"));                    // a size below 1
  CHECK_FALSE(load("PIECE 0,1,1\n\n"));
  CHECK_FALSE(load("PIECE a,b,c\nX\n"));                     // a size that does not read
  CHECK_FALSE(load("PIECE 2,1,1\nX,\n"));                    // legacy: a comma is not a cell
}
