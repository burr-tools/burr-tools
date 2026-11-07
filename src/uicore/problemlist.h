/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef BTUI_PROBLEMLIST_H
#define BTUI_PROBLEMLIST_H

#include <string>
#include <vector>

class problem_c;
class puzzle_c;

/* The Puzzle workspace's operations (C16, C17), without its cards: what the
 * problems list, the result, the piece counts, the groups and the colour
 * rules do to the puzzle, and the totals the summary reads. Legacy
 * mainWindow_c does the same in cb_NewProblem, cb_CopyProblem,
 * cb_ShapeToResult, cb_AddShapeToProblem and their neighbours and in
 * groupsEditor_c; the differences are the spec's and are named where they
 * are made.
 *
 * Each function changes the puzzle it is given and nothing else; the caller
 * makes it one undo step (DocumentSession::edit, AK_PROBLEM_STRUCTURAL).
 * Problems and shapes are given by position, as the GUI's rows are, and a
 * position the GUI can produce but that names nothing -- -1, or one past the
 * end while a view tears down -- changes nothing and does not throw.
 */
namespace btui {

  /* the most copies of a shape a problem can hold: the library's field */
  constexpr int maxPieceCount = 65535;
  constexpr int maxGroupId = 65535;       ///< the file's and the library's 16 bits

  /* "P1" for position 0 */
  std::string problemId(int position);

  // --- the list ----------------------------------------------------------------

  /* An empty problem appended to the list, each colour allowed on itself
   * (legacy cb_NewProblem), its result the first shape (C16; legacy left it
   * unset). Returns its position. */
  int newProblem(puzzle_c & p);

  /* A copy of problem i appended: result, counts, groups, colour rules and
   * label, not its solutions. Returns its position, -1 if i is no problem. */
  int duplicateProblem(puzzle_c & p, int i);

  /* Delete needs a second problem to remain (C16; legacy allowed none). */
  bool canDeleteProblem(const puzzle_c & p, int i);

  /* Delete problem i. Returns the problem to select after it: the one now at
   * i, else the new last one; -1 when nothing was deleted. */
  int deleteProblem(puzzle_c & p, int i);

  /* Move problem `from` to position `to` (clamped), shifting those between.
   * Returns whether anything moved. */
  bool moveProblem(puzzle_c & p, int from, int to);

  /* Set the label, without control characters; empty means none. Returns
   * whether it changed. */
  bool renameProblem(puzzle_c & p, int i, const std::string & label);
  /* A label as a file can hold it: control characters (a pasted newline or
   * tab) go, which XML 1.0 mostly cannot hold. */
  std::string cleanLabel(const std::string & label);

  /* Whether problem i holds what undo cannot bring back: saved solutions or
   * a search state. Any change to what it asks for clears them, after one
   * warning a session (DocumentController::mayCostSolutions). */
  bool problemHasSolutions(const puzzle_c & p, int i);

  /* how many solutions it holds, to say so in that warning */
  int problemSolutionCount(const puzzle_c & p, int i);

  /* Forget them, and the search state (legacy changeProblem). */
  void clearProblemSolutions(puzzle_c & p, int i);

  /* Whether editing the shape costs solutions: a problem that uses it holds
   * some. -1 stands for several shapes: any problem that holds some. */
  bool shapeEditCostsSolutions(const puzzle_c & p, int shape);

  // --- what a problem asks for ---------------------------------------------------

  /* The kinds of problem there are. A kind decides the slots and sections
   * below, so a new one is a new value here and the compiler points at what
   * it must answer; the file format knows only assembly so far. */
  enum class ProblemKind { Assembly };

  ProblemKind problemKind(const puzzle_c & p, int i);

  /* The shapes a problem names in a role of their own: they are never its
   * pieces, and the Result card and the 3D scene's top row list them. */
  enum class GoalRole { Result };

  struct GoalSlot {
    GoalRole role = GoalRole::Result;
    int shape = -1;            ///< -1: none chosen
  };

  std::vector<GoalSlot> goalSlots(const puzzle_c & p, int i);

  /* Whether the shape fills a slot of the problem, and so cannot be a piece. */
  bool isGoalShape(const puzzle_c & p, int i, int shape);

  /* The right card's sections, in order (C17), and what each applies to. The
   * card and its collapsed rail both read this, so a new section is a row
   * here and one QML file. */
  enum class PuzzleSection { SelectedPiece, Groups, ColourRules };
  enum class SectionScope { Piece, Problem };

  struct SectionInfo {
    PuzzleSection section = PuzzleSection::SelectedPiece;
    SectionScope scope = SectionScope::Piece;
  };

  std::vector<SectionInfo> puzzleSections(const puzzle_c & p, int i);

  // --- the result ----------------------------------------------------------------

  /* Make the shape the result; it stops being a piece (legacy
   * cb_ShapeToResult). Returns whether anything changed. */
  bool setResult(puzzle_c & p, int i, int shape);

  // --- piece counts --------------------------------------------------------------

  struct PieceCount {
    int min = 0;
    int max = 0;
    bool inPuzzle(void) const { return max > 0; }
    bool isRange(void) const { return min != max; }
  };

  PieceCount pieceCount(const puzzle_c & p, int i, int shape);

  /* One more copy, or one fewer (delta). A fixed count stays fixed; a range
   * moves its maximum and the minimum follows if it must (C16). At 0 the
   * piece leaves the problem. The result cannot be a piece. Returns whether
   * anything changed. */
  bool stepPiece(puzzle_c & p, int i, int shape, int delta);

  /* Set the count; min above max raises max, and 0 ... 0 removes the piece
   * (C17: min <= max, both >= 0). Group memberships of the whole piece
   * follow it. Returns whether anything changed. */
  bool setPieceCount(puzzle_c & p, int i, int shape, int min, int max);

  /* Range on or off (C17): off is min = max = max; on leaves the numbers,
   * which the two steppers then set apart. Returns whether anything changed. */
  bool collapseRange(puzzle_c & p, int i, int shape);

  /* Every piece in the problem takes this range (legacy bulk range, AC-C16-08):
   * max is at least 1 and never below min. */
  bool setRangeForAll(puzzle_c & p, int i, int min, int max);

  /* One more of every shape but the goal shapes (legacy cb_AddAllShapesToProblem). */
  bool addOneOfEach(puzzle_c & p, int i);

  /* legacy cb_RemoveAllShapesFromProblem */
  bool removeAllPieces(puzzle_c & p, int i);

  // --- groups --------------------------------------------------------------------

  /* The library's groups as they are: a group is a number (1 ...), and a
   * piece has a count in each group it is in. The count says how many of its
   * copies may be members; the disassembler works out which, so the counts
   * of a piece are not shares of a whole and one piece can be in several
   * groups. Pieces left in one clump that are all of a group are not taken
   * apart any further. */
  struct GroupMember {
    int shape = -1;
    int count = 0;
  };

  struct Group {
    int id = 0;
    std::vector<GroupMember> members;    ///< in shape order
  };

  /* the problem's groups by id; a group exists while it has members */
  std::vector<Group> problemGroups(const puzzle_c & p, int i);

  /* how many copies of the shape may be in the group, 0 when none */
  int pieceGroupCount(const puzzle_c & p, int i, int shape, int group);

  /* the id a new group gets */
  int nextGroup(const puzzle_c & p, int i);

  /* Set that count, within the piece's copies; 0 leaves the group. Returns
   * whether anything changed. */
  bool setPieceGroupCount(puzzle_c & p, int i, int shape, int group, int count);

  /* Empty the group; the groups after it move down, so that the ids stay
   * G1 ... Gn. Returns whether anything changed. */
  bool deleteGroup(puzzle_c & p, int i, int group);

  // --- colour rules --------------------------------------------------------------

  /* Colours by id, 1 ... n, as the matrix shows them. Default (0) is not
   * among them: the library lets a voxel of it go anywhere, in either role,
   * and no rule can say otherwise (problem_c::placementAllowed; OQ-22). */
  bool colourRule(const puzzle_c & p, int i, int pieceColour, int resultColour);

  /* Returns whether it changed. */
  bool setColourRule(puzzle_c & p, int i, int pieceColour, int resultColour, bool allowed);

  enum class ColourPreset { EachOnItself, AllowAll, Clear };

  /* the matrix's three buttons. Returns whether anything changed. */
  bool setColourPreset(puzzle_c & p, int i, ColourPreset preset);

  // --- the summary ---------------------------------------------------------------

  enum class ProblemFit {
    NoResult,      ///< nothing to fill
    Fits,          ///< the pieces can fill the result
    TooLarge,      ///< the fewest pieces are more than the result can hold
    TooSmall,      ///< the most pieces do not reach the result's fixed voxels
  };

  struct ProblemStats {
    ProblemFit fit = ProblemFit::NoResult;
    /* wide: 65535 copies of a large shape do not fit an int */
    long long resultFixed = 0; ///< the voxels the result must fill
    long long resultTotal = 0; ///< with the variable ones it may fill
    long long piecesMin = 0;   ///< pieces, fewest and most
    long long piecesMax = 0;
    long long voxelsMin = 0;   ///< the pieces' voxels, fewest and most
    long long voxelsMax = 0;
    /* Derived, never configured (C16): known when every count is fixed and
     * the result has only fixed voxels */
    bool holesKnown = false;
    long long holes = 0;       ///< negative: the pieces exceed the result by that much
    /* the file's limit on holes (legacy's Problem Details field): the search
     * still honours it, so it is shown although nothing sets it (OQ-23) */
    bool holeLimit = false;
    int maxHoles = 0;
  };

  ProblemStats problemStats(const puzzle_c & p, int i);

  /* Lift the file's limit on holes. Returns whether there was one. */
  bool clearHoleLimit(puzzle_c & p, int i);
}

#endif
