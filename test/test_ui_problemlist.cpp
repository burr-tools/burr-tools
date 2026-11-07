/* Tests for btui's problem list: what the Puzzle workspace's cards do to the
 * puzzle (C16, C17), and the totals its summary reads.
 */
#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"

#include "../src/uicore/documentsession.h"
#include "../src/uicore/problemlist.h"

#include "../src/lib/problem.h"
#include "../src/lib/puzzle.h"
#include "../src/lib/voxel.h"

#include <memory>
#include <string>
#include <vector>

using namespace btui;

namespace {

  /* a brick puzzle of n shapes; shape k is a bar of k+1 fixed voxels */
  std::unique_ptr<puzzle_c> shapes(unsigned n) {
    auto p = std::make_unique<puzzle_c>(std::make_unique<gridType_c>(gridType_c::GT_BRICKS));
    for (unsigned k = 0; k < n; k++) {
      const unsigned i = p->addShape(k + 1, 1, 1);
      for (unsigned v = 0; v <= k; v++)
        p->getShape(i)->setState(v, voxel_c::VX_FILLED);
    }
    return p;
  }

  /* shapes(n) with one problem, its result shape 0 */
  std::unique_ptr<puzzle_c> withProblem(unsigned n) {
    auto p = shapes(n);
    newProblem(*p);
    return p;
  }

  std::vector<std::string> names(const puzzle_c & p) {
    std::vector<std::string> out;
    for (unsigned k = 0; k < p.getNumberOfProblems(); k++)
      out.push_back(p.getProblem(k)->getName());
    return out;
  }
}

TEST_CASE("problems are named by position", "[ui][problemlist]") {
  CHECK(problemId(0) == "P1");
  CHECK(problemId(11) == "P12");
}

TEST_CASE("a new problem has the first shape as result and each colour on itself", "[ui][problemlist]") {
  auto p = shapes(2);
  p->addColor(1, 2, 3);
  p->addColor(4, 5, 6);
  const int k = newProblem(*p);
  REQUIRE(k == 0);
  CHECK(p->getProblem(0)->resultValid());
  CHECK(p->getProblem(0)->getResultId() == 0);
  CHECK(p->getProblem(0)->getNumberOfParts() == 0);
  CHECK(colourRule(*p, 0, 1, 1));
  CHECK(colourRule(*p, 0, 2, 2));
  CHECK_FALSE(colourRule(*p, 0, 1, 2));
}

TEST_CASE("a new problem in a puzzle without shapes has no result", "[ui][problemlist]") {
  auto p = shapes(0);
  newProblem(*p);
  CHECK_FALSE(p->getProblem(0)->resultValid());
  CHECK(problemStats(*p, 0).fit == ProblemFit::NoResult);
}

TEST_CASE("duplicating a problem copies all but its solutions", "[ui][problemlist]") {
  auto p = withProblem(3);
  p->addColor(1, 2, 3);
  p->getProblem(0)->setName("first");
  setPieceCount(*p, 0, 1, 1, 2);
  setPieceGroupCount(*p, 0, 1, 1, 2);
  setColourRule(*p, 0, 1, 1, true);

  const int k = duplicateProblem(*p, 0);
  REQUIRE(k == 1);
  CHECK(p->getProblem(1)->getName() == "first");
  CHECK(p->getProblem(1)->getResultId() == 0);
  CHECK(pieceCount(*p, 1, 1).min == 1);
  CHECK(pieceCount(*p, 1, 1).max == 2);
  CHECK(pieceGroupCount(*p, 1, 1, 1) == 2);
  CHECK(colourRule(*p, 1, 1, 1));
  CHECK(duplicateProblem(*p, 5) == -1);
}

TEST_CASE("the last problem cannot be deleted; a delete selects its neighbour", "[ui][problemlist]") {
  auto p = withProblem(1);
  CHECK_FALSE(canDeleteProblem(*p, 0));
  CHECK(deleteProblem(*p, 0) == -1);
  CHECK(p->getNumberOfProblems() == 1);

  newProblem(*p);
  newProblem(*p);
  CHECK(deleteProblem(*p, 2) == 1);
  CHECK(deleteProblem(*p, 0) == 0);
  CHECK(deleteProblem(*p, -1) == -1);
}

TEST_CASE("a problem moves to a position, those between shifting", "[ui][problemlist]") {
  auto p = shapes(1);
  for (const char * n : { "a", "b", "c" })
    p->getProblem(unsigned(newProblem(*p)))->setName(n);

  CHECK(moveProblem(*p, 0, 2));
  CHECK(names(*p) == std::vector<std::string>{ "b", "c", "a" });
  CHECK(moveProblem(*p, 2, 1));
  CHECK(names(*p) == std::vector<std::string>{ "b", "a", "c" });
  CHECK_FALSE(moveProblem(*p, 1, 1));
  CHECK_FALSE(moveProblem(*p, 9, 0));
  CHECK(moveProblem(*p, 0, 99));
  CHECK(names(*p) == std::vector<std::string>{ "a", "c", "b" });
}

TEST_CASE("a problem's label drops control characters and reports a change", "[ui][problemlist]") {
  auto p = withProblem(1);
  CHECK(renameProblem(*p, 0, "a\tb\n"));
  CHECK(p->getProblem(0)->getName() == "ab");
  CHECK_FALSE(renameProblem(*p, 0, "ab"));
  CHECK_FALSE(renameProblem(*p, 3, "x"));
}

TEST_CASE("an assembly problem names its result as its one goal slot", "[ui][problemlist]") {
  auto p = withProblem(2);
  const std::vector<GoalSlot> slots = goalSlots(*p, 0);
  REQUIRE(slots.size() == 1);
  CHECK(slots[0].role == GoalRole::Result);
  CHECK(slots[0].shape == 0);
  CHECK(isGoalShape(*p, 0, 0));
  CHECK_FALSE(isGoalShape(*p, 0, 1));
  CHECK(goalSlots(*p, 4).empty());
}

TEST_CASE("the right card's sections are the piece, then what the problem holds", "[ui][problemlist]") {
  auto p = withProblem(1);
  const std::vector<SectionInfo> s = puzzleSections(*p, 0);
  REQUIRE(s.size() == 3);
  CHECK(s[0].section == PuzzleSection::SelectedPiece);
  CHECK(s[0].scope == SectionScope::Piece);
  CHECK(s[1].section == PuzzleSection::Groups);
  CHECK(s[2].section == PuzzleSection::ColourRules);
  CHECK(s[2].scope == SectionScope::Problem);
}

TEST_CASE("a shape chosen as result stops being a piece", "[ui][problemlist]") {
  auto p = withProblem(3);
  setPieceCount(*p, 0, 2, 2, 2);
  CHECK(setResult(*p, 0, 2));
  CHECK(p->getProblem(0)->getResultId() == 2);
  CHECK_FALSE(pieceCount(*p, 0, 2).inPuzzle());
  CHECK_FALSE(setResult(*p, 0, 2));
  CHECK_FALSE(setResult(*p, 0, 9));
}

TEST_CASE("the result cannot be made a piece", "[ui][problemlist]") {
  auto p = withProblem(2);
  CHECK_FALSE(stepPiece(*p, 0, 0, 1));
  CHECK_FALSE(setPieceCount(*p, 0, 0, 1, 1));
  CHECK_FALSE(pieceCount(*p, 0, 0).inPuzzle());
}

TEST_CASE("a fixed count steps as one number and leaves at 0", "[ui][problemlist]") {
  auto p = withProblem(2);
  CHECK(stepPiece(*p, 0, 1, 1));
  CHECK(stepPiece(*p, 0, 1, 1));
  CHECK(pieceCount(*p, 0, 1).min == 2);
  CHECK(pieceCount(*p, 0, 1).max == 2);
  CHECK(stepPiece(*p, 0, 1, -1));
  CHECK(stepPiece(*p, 0, 1, -1));
  CHECK_FALSE(pieceCount(*p, 0, 1).inPuzzle());
  CHECK(p->getProblem(0)->getNumberOfParts() == 0);
  CHECK_FALSE(stepPiece(*p, 0, 1, -1));
}

TEST_CASE("a range steps its maximum, the minimum following", "[ui][problemlist]") {
  auto p = withProblem(2);
  setPieceCount(*p, 0, 1, 1, 3);
  stepPiece(*p, 0, 1, 1);
  CHECK(pieceCount(*p, 0, 1).min == 1);
  CHECK(pieceCount(*p, 0, 1).max == 4);
  stepPiece(*p, 0, 1, -3);
  CHECK(pieceCount(*p, 0, 1).min == 1);
  CHECK(pieceCount(*p, 0, 1).max == 1);
}

TEST_CASE("min above max raises max; a range collapses to its maximum", "[ui][problemlist]") {
  auto p = withProblem(2);
  CHECK(setPieceCount(*p, 0, 1, 3, 1));
  CHECK(pieceCount(*p, 0, 1).max == 3);
  CHECK(setPieceCount(*p, 0, 1, 0, 2));
  CHECK(pieceCount(*p, 0, 1).isRange());
  CHECK(collapseRange(*p, 0, 1));
  CHECK(pieceCount(*p, 0, 1).min == 2);
  CHECK_FALSE(pieceCount(*p, 0, 1).isRange());
  CHECK_FALSE(collapseRange(*p, 0, 1));
}

TEST_CASE("a range of 0 to N keeps the piece in the problem", "[ui][problemlist]") {
  auto p = withProblem(2);
  setPieceCount(*p, 0, 1, 0, 2);
  CHECK(pieceCount(*p, 0, 1).inPuzzle());
  CHECK(pieceCount(*p, 0, 1).min == 0);
}

TEST_CASE("set range for all gives every piece the range, max at least 1 and not below min", "[ui][problemlist]") {
  auto p = withProblem(4);
  stepPiece(*p, 0, 1, 1);
  stepPiece(*p, 0, 3, 2);

  // AC-C16-08
  CHECK(setRangeForAll(*p, 0, 2, 1));
  CHECK(pieceCount(*p, 0, 1).min == 2);
  CHECK(pieceCount(*p, 0, 1).max == 2);
  CHECK(pieceCount(*p, 0, 3).max == 2);
  CHECK_FALSE(pieceCount(*p, 0, 2).inPuzzle());

  CHECK(setRangeForAll(*p, 0, 0, 0));
  CHECK(pieceCount(*p, 0, 1).min == 0);
  CHECK(pieceCount(*p, 0, 1).max == 1);
}

TEST_CASE("one of each skips the result; remove all empties the problem", "[ui][problemlist]") {
  auto p = withProblem(3);
  CHECK(addOneOfEach(*p, 0));
  CHECK_FALSE(pieceCount(*p, 0, 0).inPuzzle());
  CHECK(pieceCount(*p, 0, 1).max == 1);
  CHECK(pieceCount(*p, 0, 2).max == 1);
  CHECK(removeAllPieces(*p, 0));
  CHECK(p->getProblem(0)->getNumberOfParts() == 0);
  CHECK_FALSE(removeAllPieces(*p, 0));
}

TEST_CASE("a piece can be in several groups, each with its own count", "[ui][problemlist]") {
  auto p = withProblem(3);
  setPieceCount(*p, 0, 1, 2, 2);
  setPieceCount(*p, 0, 2, 1, 1);

  CHECK(nextGroup(*p, 0) == 1);
  CHECK(setPieceGroupCount(*p, 0, 1, 1, 2));
  CHECK(setPieceGroupCount(*p, 0, 1, 2, 2));
  CHECK(setPieceGroupCount(*p, 0, 2, 2, 1));

  const std::vector<Group> g = problemGroups(*p, 0);
  REQUIRE(g.size() == 2);
  CHECK(g[0].id == 1);
  REQUIRE(g[0].members.size() == 1);
  CHECK(g[0].members[0].shape == 1);
  CHECK(g[0].members[0].count == 2);
  CHECK(g[1].id == 2);
  CHECK(g[1].members.size() == 2);
  CHECK(nextGroup(*p, 0) == 3);
}

TEST_CASE("a group count stays within the piece's copies and 0 leaves the group", "[ui][problemlist]") {
  auto p = withProblem(2);
  setPieceCount(*p, 0, 1, 2, 2);
  CHECK(setPieceGroupCount(*p, 0, 1, 1, 9));
  CHECK(pieceGroupCount(*p, 0, 1, 1) == 2);
  CHECK(setPieceGroupCount(*p, 0, 1, 1, 0));
  CHECK(problemGroups(*p, 0).empty());
  CHECK_FALSE(setPieceGroupCount(*p, 0, 0, 1, 1));   // not a piece
  CHECK_FALSE(setPieceGroupCount(*p, 0, 1, 0, 1));   // not a group
}

TEST_CASE("a whole piece's group follows its count; a part of it is capped", "[ui][problemlist]") {
  auto p = withProblem(3);
  setPieceCount(*p, 0, 1, 2, 2);
  setPieceGroupCount(*p, 0, 1, 1, 2);
  stepPiece(*p, 0, 1, 2);
  CHECK(pieceGroupCount(*p, 0, 1, 1) == 4);

  setPieceCount(*p, 0, 2, 3, 3);
  setPieceGroupCount(*p, 0, 2, 2, 2);
  stepPiece(*p, 0, 2, 1);
  CHECK(pieceGroupCount(*p, 0, 2, 2) == 2);
  setPieceCount(*p, 0, 2, 1, 1);
  CHECK(pieceGroupCount(*p, 0, 2, 2) == 1);
}

TEST_CASE("deleting a group empties it and the later groups move down", "[ui][problemlist]") {
  auto p = withProblem(3);
  setPieceCount(*p, 0, 1, 1, 1);
  setPieceCount(*p, 0, 2, 1, 1);
  setPieceGroupCount(*p, 0, 1, 1, 1);
  setPieceGroupCount(*p, 0, 2, 2, 1);

  CHECK(deleteGroup(*p, 0, 1));
  const std::vector<Group> g = problemGroups(*p, 0);
  REQUIRE(g.size() == 1);
  CHECK(g[0].id == 1);
  REQUIRE(g[0].members.size() == 1);
  CHECK(g[0].members[0].shape == 2);
  CHECK_FALSE(deleteGroup(*p, 0, 7));
}

TEST_CASE("colour rules toggle one pair; Default has no rule", "[ui][problemlist]") {
  auto p = withProblem(1);
  p->addColor(1, 2, 3);
  p->addColor(4, 5, 6);

  CHECK_FALSE(colourRule(*p, 0, 1, 2));
  CHECK(setColourRule(*p, 0, 1, 2, true));
  CHECK(colourRule(*p, 0, 1, 2));
  CHECK_FALSE(colourRule(*p, 0, 2, 1));     // the pair has a direction
  CHECK_FALSE(setColourRule(*p, 0, 1, 2, true));
  CHECK(setColourRule(*p, 0, 1, 2, false));

  // OQ-22: the library lets colour 0 go anywhere, so no rule can name it
  CHECK_FALSE(setColourRule(*p, 0, 0, 1, true));
  CHECK_FALSE(setColourRule(*p, 0, 1, 0, true));
  CHECK_FALSE(setColourRule(*p, 0, 3, 1, true));
  CHECK(p->getProblem(0)->placementAllowed(0, 1));
}

TEST_CASE("the colour presets set the whole matrix of one problem", "[ui][problemlist]") {
  auto p = withProblem(1);
  newProblem(*p);
  p->addColor(1, 2, 3);
  p->addColor(4, 5, 6);

  // AC-C17-04: per problem
  CHECK(setColourPreset(*p, 0, ColourPreset::AllowAll));
  CHECK(colourRule(*p, 0, 1, 2));
  CHECK_FALSE(colourRule(*p, 1, 1, 2));
  CHECK_FALSE(setColourPreset(*p, 0, ColourPreset::AllowAll));

  CHECK(setColourPreset(*p, 0, ColourPreset::EachOnItself));
  CHECK(colourRule(*p, 0, 2, 2));
  CHECK_FALSE(colourRule(*p, 0, 2, 1));

  CHECK(setColourPreset(*p, 0, ColourPreset::Clear));
  CHECK_FALSE(colourRule(*p, 0, 1, 1));
}

TEST_CASE("the summary derives the holes from fixed counts", "[ui][problemlist]") {
  auto p = shapes(4);                 // bars of 1, 2, 3, 4 voxels
  newProblem(*p);
  setResult(*p, 0, 3);                // 4 voxels
  stepPiece(*p, 0, 0, 1);             // 1
  stepPiece(*p, 0, 1, 1);             // 2

  // T-C16-6
  ProblemStats st = problemStats(*p, 0);
  CHECK(st.fit == ProblemFit::TooSmall);
  CHECK(st.resultFixed == 4);
  CHECK(st.resultTotal == 4);
  CHECK(st.piecesMin == 2);
  CHECK(st.voxelsMin == 3);
  CHECK(st.holesKnown);
  CHECK(st.holes == 1);

  stepPiece(*p, 0, 1, 1);             // 1 + 2 + 2
  st = problemStats(*p, 0);
  CHECK(st.fit == ProblemFit::TooLarge);
  CHECK(st.holes == -1);
}

TEST_CASE("variable voxels and ranges make the fit a range and the holes unknown", "[ui][problemlist]") {
  auto p = shapes(4);
  p->getShape(3)->setState(3, voxel_c::VX_VARIABLE);    // 3 fixed + 1 variable
  newProblem(*p);
  setResult(*p, 0, 3);
  setPieceCount(*p, 0, 0, 0, 4);

  const ProblemStats st = problemStats(*p, 0);
  CHECK(st.resultFixed == 3);
  CHECK(st.resultTotal == 4);
  CHECK(st.voxelsMin == 0);
  CHECK(st.voxelsMax == 4);
  CHECK(st.fit == ProblemFit::Fits);
  CHECK_FALSE(st.holesKnown);
}

TEST_CASE("a problem without a result still counts its pieces", "[ui][problemlist]") {
  auto p = shapes(2);
  p->addProblem();
  stepPiece(*p, 0, 1, 1);
  const ProblemStats st = problemStats(*p, 0);
  CHECK(st.fit == ProblemFit::NoResult);
  CHECK(st.voxelsMax == 2);
  CHECK_FALSE(st.holesKnown);
}

TEST_CASE("the file's hole limit shows and can be lifted", "[ui][problemlist]") {
  auto p = withProblem(1);
  CHECK_FALSE(problemStats(*p, 0).holeLimit);
  CHECK_FALSE(clearHoleLimit(*p, 0));
  p->getProblem(0)->setMaxHoles(2);
  CHECK(problemStats(*p, 0).holeLimit);
  CHECK(problemStats(*p, 0).maxHoles == 2);
  CHECK(clearHoleLimit(*p, 0));
  CHECK_FALSE(problemStats(*p, 0).holeLimit);
}

TEST_CASE("a position that names nothing changes nothing", "[ui][problemlist]") {
  auto p = withProblem(2);
  for (const int i : { -1, 1, 99 }) {
    CHECK_FALSE(problemHasSolutions(*p, i));
    CHECK_FALSE(setResult(*p, i, 1));
    CHECK_FALSE(stepPiece(*p, i, 1, 1));
    CHECK_FALSE(setRangeForAll(*p, i, 1, 1));
    CHECK_FALSE(addOneOfEach(*p, i));
    CHECK_FALSE(removeAllPieces(*p, i));
    CHECK_FALSE(deleteGroup(*p, i, 1));
    CHECK_FALSE(setColourPreset(*p, i, ColourPreset::Clear));
    CHECK_FALSE(clearHoleLimit(*p, i));
    CHECK(problemStats(*p, i).fit == ProblemFit::NoResult);
    CHECK(problemGroups(*p, i).empty());
    clearProblemSolutions(*p, i);
  }
  CHECK_FALSE(stepPiece(*p, 0, -1, 1));
  CHECK_FALSE(stepPiece(*p, 0, 5, 1));
  CHECK(pieceCount(*p, 0, 5).max == 0);
}

TEST_CASE("an edit of a shape costs the solutions of the problems using it", "[ui][problemlist]") {
  auto p = withProblem(3);
  CHECK_FALSE(shapeEditCostsSolutions(*p, 0));
  CHECK_FALSE(shapeEditCostsSolutions(*p, -1));
}

TEST_CASE("each operation works on every voxel type", "[ui][problemlist]") {
  for (const gridType_c::gridType type : bttest::ALL_GRIDS) {
    auto p = std::make_unique<puzzle_c>(std::make_unique<gridType_c>(type));
    p->addShape(2, 2, 2);
    p->addShape(2, 2, 2);
    newProblem(*p);
    CHECK(stepPiece(*p, 0, 1, 1));
    CHECK(problemStats(*p, 0).piecesMax == 1);
  }
}

TEST_CASE("the totals of a very large count do not overflow", "[ui][problemlist]") {
  auto p = std::make_unique<puzzle_c>(std::make_unique<gridType_c>(gridType_c::GT_BRICKS));
  p->addShape(1, 1, 1);
  p->getShape(0)->setState(0, 0, 0, voxel_c::VX_FILLED);
  const unsigned big = p->addShape(33, 33, 33);
  p->getShape(big)->setAll(voxel_c::VX_FILLED);
  newProblem(*p);
  REQUIRE(setPieceCount(*p, 0, 1, maxPieceCount, maxPieceCount));

  const ProblemStats st = problemStats(*p, 0);
  CHECK(st.voxelsMin == 33LL * 33 * 33 * maxPieceCount);
  CHECK(st.fit == ProblemFit::TooLarge);
  CHECK(st.holes == 1 - 33LL * 33 * 33 * maxPieceCount);
}

TEST_CASE("the group after the last id is the first one free", "[ui][problemlist]") {
  auto p = std::make_unique<puzzle_c>(std::make_unique<gridType_c>(gridType_c::GT_BRICKS));
  p->addShape(1, 1, 1);
  p->addShape(1, 1, 1);
  p->addShape(1, 1, 1);
  newProblem(*p);
  REQUIRE(setPieceCount(*p, 0, 1, 1, 1));
  REQUIRE(setPieceCount(*p, 0, 2, 1, 1));
  CHECK(nextGroup(*p, 0) == 1);
  REQUIRE(setPieceGroupCount(*p, 0, 1, maxGroupId, 1));
  CHECK(nextGroup(*p, 0) == 1);
  REQUIRE(setPieceGroupCount(*p, 0, 2, 1, 1));
  CHECK(nextGroup(*p, 0) == 2);

  // the id past the last would wrap around to 0 in the library's 16 bits
  CHECK_FALSE(setPieceGroupCount(*p, 0, 1, maxGroupId + 1, 1));
  CHECK(problemGroups(*p, 0).size() == 2);
}

TEST_CASE("a result that a file also lists as a piece can be taken out", "[ui][problemlist]") {
  auto p = std::make_unique<puzzle_c>(std::make_unique<gridType_c>(gridType_c::GT_BRICKS));
  p->addShape(1, 1, 1);
  p->addShape(1, 1, 1);
  newProblem(*p);
  REQUIRE(isGoalShape(*p, 0, 0));
  CHECK_FALSE(setPieceCount(*p, 0, 0, 1, 1));       // never by adding it

  p->getProblem(0)->setShapeMaximum(0, 2);          // what such a file gives
  CHECK(removeAllPieces(*p, 0));
  CHECK(p->getProblem(0)->getNumberOfParts() == 0);
  CHECK(isGoalShape(*p, 0, 0));
}
