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
#ifndef BTUI_SOLVER_H
#define BTUI_SOLVER_H

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class assembly_c;
class problem_c;
class solution_c;

/* The Solver workspace's rules (C19, C20, C21), without its cards or its
 * thread: what the search options ask of the solver, which run buttons a
 * state allows, which solutions the list shows and in what order, what each
 * Manage item removes, the pieces' names and display states, the player's
 * arithmetic and the one key map of the list, the solution slider and the
 * move seekbar. Legacy mainWindow_c does the same in cb_BtnCont,
 * updateInterface, cb_DeleteSolutions and their neighbours; where the spec
 * differs it is said where it is decided. The answers to the spec's open
 * questions OQ-25 to OQ-33 are in design/2026-11-13-qtgui-solver-workspace.md.
 *
 * Nothing here runs a search or reads a clock: the caller passes what the
 * solver thread and the problem say, and the time that passed.
 */
namespace btui {

  // --- the search options (C19 §2, §3) -------------------------------------

  /* "Sort found solutions by": the order the solver keeps its solutions in,
   * and so which it keeps at the limit (OQ-27) */
  enum class SearchSort { Id, Pieces, Level, Moves };

  struct SolverOptions {
    bool disassemble = false;         ///< "Assemblies and disassembly"
    bool dropDisassemblies = false;   ///< "Don't keep the disassemblies"; implies disassemble
    bool justCount = false;
    bool keepMirror = false;
    bool keepRotated = false;
    bool thoroughRotation = false;
    SearchSort sort = SearchSort::Moves;
    int keepEvery = 1;                ///< legacy "Drop": keep every n-th solution (OQ-26)
    int limit = 100;

    bool operator==(const SolverOptions &) const = default;
  };

  constexpr int maxSolverCount = 100000000;   ///< legacy's bounds of Drop and Limit

  /* the disassembly is checked: "Assemblies and disassembly", or its
   * "Don't keep" sub-switch (AC-C19-02) */
  bool checksDisassembly(const SolverOptions & o);
  /* Level and Moves need the disassembly; without it the choice falls back
   * to the search order (AC-C19-03) */
  bool searchSortAvailable(const SolverOptions & o, SearchSort s);
  SearchSort effectiveSort(const SolverOptions & o);
  /* the Search options rows that are not at their defaults ("· n changed") */
  int changedOptions(const SolverOptions & o);
  /* the options with Keep every and Limit inside the solver's bounds and
   * "Don't keep" switching the check on */
  SolverOptions normalised(SolverOptions o);

  /* what solveThread_c is given: its PAR_* flags, its sort method (SRT_*),
   * the live sort (problem_c::sortSolutions method, -1 none) and the
   * limits. PAR_REDUCE always, as legacy. */
  struct SolverParameters {
    int flags = 0;
    int sortMethod = 0;
    int liveSort = -1;
    unsigned int limit = 100;
    unsigned int drop = 1;
  };
  SolverParameters solverParameters(const SolverOptions & o);

  // --- the run (C19 §4) ----------------------------------------------------

  enum class RunState {
    Idle,       ///< nothing run, or the problem was changed since
    Preparing,  ///< the thread builds the placements
    Ready,      ///< prepared, not searched (Prepare)
    Running,    ///< the thread searches
    Stopping,   ///< Stop pressed, the thread winds down
    Paused,     ///< stopped part way; Continue resumes
    Finished,   ///< searched to the end
  };

  /* What the solver thread and the problem say. `action` is the thread's
   * solveThread_c::ACT_* while one runs for this problem, -1 otherwise. */
  struct RunFacts {
    bool problem = false;          ///< a problem is selected
    bool solvable = false;         ///< it has pieces and a result, the grid an assembler
    bool threadHere = false;       ///< the thread works on this problem
    bool threadElsewhere = false;  ///< it works on another one
    int action = -1;
    int solveState = 0;            ///< problem_c's solveState_e (SS_UNSOLVED…)
    bool canContinue = false;      ///< problem_c::canStartSolving() while SS_SOLVING
    bool hasAssembler = false;     ///< a prepared or stopped search is in memory
    bool prepared = false;         ///< the last run of it was Prepare
    bool busy = false;             ///< an analysis runs, or the file's solutions are being read
  };

  RunState runState(const RunFacts & f);

  /* the Run row's and the explorer's buttons (C19 table; OQ-29) */
  struct RunButtons {
    bool prepare = false, start = false, cont = false, stop = false;
    bool placements = false, step = false;
    bool operator==(const RunButtons &) const = default;
  };
  RunButtons runButtons(const RunFacts & f);

  /* the Activity cell (legacy's words, mainWindow_c::updateInterface) */
  enum class Activity {
    None, Preparing, Optimising, Assembling, Disassembling, Counting, Stopping,
    Paused, Ready, Finished, Partial, Error,
  };
  Activity activity(const RunFacts & f, bool justCount);

  /* legacy timeToString: a number of seconds in the largest fitting unit */
  struct TimeText {
    enum class Unit { Seconds, Minutes, Hours, Days, Months, Years, Millennia, Ages };
    Unit unit = Unit::Seconds;
    double value = 0;              ///< whole for seconds, one decimal otherwise
    bool operator==(const TimeText &) const = default;
  };
  TimeText timeText(double seconds);
  /* time left from the time used and the progress: legacy's
   * used / progress - used, shown once the progress passes about 6 %
   * (C19 §4); nothing before. */
  std::optional<double> timeLeft(double usedSeconds, double progress);
  constexpr double timeLeftAfter = 0.06;

  // --- the solutions (C20) -------------------------------------------------

  /* the DA column and the Disassembly tile */
  enum class DaState { Stored, LevelOnly, Cannot, NotChecked };

  /* a saved solution as the list reads it */
  struct SolutionFacts {
    int index = 0;                 ///< in the problem's saved list
    unsigned int assembly = 0;     ///< its assembly number: the search order
    int pieces = 0;                ///< the pieces it places
    DaState da = DaState::NotChecked;
    std::vector<unsigned int> level;   ///< moves to remove each piece in turn; empty when unknown
    std::string levelText;         ///< "3.1.1"; empty when unknown
    int moves = -1;                ///< the disassembly's sum of moves, -1 when unknown
    bool operator==(const SolutionFacts &) const = default;
  };

  /* Every saved solution of the problem; call it under its lockSolutions().
   * `cannot` are assembly numbers an analysis found cannot be taken apart:
   * the library keeps no mark for that. */
  std::vector<SolutionFacts> solutionFacts(const problem_c & prob, const std::vector<unsigned int> & cannot = {});

  /* the list's sort (C20): the column headers and the Sort-by segment */
  enum class ListKey { Number, Level, Moves, Pieces };
  struct ListSort {
    ListKey key = ListKey::Number;
    bool descending = false;
    bool operator==(const ListSort &) const = default;
  };
  /* a click on a key: the active one reverses; another starts in its own
   * direction, ascending for Number and "best first" (descending) for the
   * others */
  ListSort clickSort(ListSort current, ListKey key);
  /* Level and Moves are offered once some solution has disassembly
   * information */
  bool listKeyAvailable(const std::vector<SolutionFacts> & all, ListKey key);

  /* The solutions the list shows, as positions into `all`: with the
   * disassembly checked only those that can be taken apart (stored or level
   * only), else all; in the sort's order, ties by assembly number. A sort by
   * Level or Moves that is not available sorts by Number. */
  std::vector<int> shownSolutions(const std::vector<SolutionFacts> & all, bool onlyDisassemblable, ListSort sort);

  /* One saved solution's facts, `index` its place in the saved list; what
   * solutionFacts reads for each */
  SolutionFacts solutionFactsOf(const solution_c & s, int index, const std::vector<unsigned int> & cannot = {});

  /* The list's solutions kept from one read to the next (C20), for a solve
   * that adds thousands while the list shows them. A read takes the facts of
   * only the solutions it has not seen, known by their assembly numbers,
   * drops the ones gone, sorts the new ones among themselves and merges them
   * into the rows in one pass from the end: the old rows move back while the
   * new ones fill in. The rows are shownSolutions' every time. Everything is
   * read anew when the facts of a solution already read may have changed (a
   * disassembly analysed or deleted: solution_c::changeCount, or `cannot`),
   * for another problem, and while two solutions share an assembly number;
   * the rows are sorted anew when the filter or the sort in effect changes.
   */
  class SolutionList {
  public:

    /* what a read changed in the rows */
    struct Change {
      bool reset = false;            ///< read or sorted anew: compare the rows by position
      std::vector<int> removed;      ///< rows gone, ascending, numbered as the rows were
      std::vector<int> inserted;     ///< rows new, ascending, numbered as the rows are now
      bool none(void) const { return !reset && removed.empty() && inserted.empty(); }
    };

    /* what the facts of the solutions read depend on besides the list: the
     * problem, the changes inside its solutions, the analysis's verdicts */
    struct Basis {
      const void * owner = nullptr;
      unsigned long long changes = 0;
      std::vector<unsigned int> cannot;
      bool operator==(const Basis &) const = default;
    };

    /* a list of `count` solutions: the i-th's assembly number, and its facts
     * (asked only for the ones not seen) */
    struct Source {
      unsigned int count = 0;
      std::function<unsigned int(unsigned int)> assembly;
      std::function<SolutionFacts(unsigned int)> facts;
    };

    /* the problem's saved solutions; under its lockSolutions() */
    Change read(const problem_c & prob, const std::vector<unsigned int> & cannot, bool onlyDisassemblable, ListSort sort);
    Change read(const Source & src, const Basis & basis, bool onlyDisassemblable, ListSort sort);
    /* nothing kept: the next read reads everything */
    Change clear(void);

    /* every solution read, in no particular order; `index` is its place in
     * the saved list at the last read */
    const std::vector<SolutionFacts> & all(void) const { return m_all; }
    /* the rows: positions into all() */
    const std::vector<int> & shown(void) const { return m_shown; }

  private:

    Change readAll(const Source & src, const Basis & basis, bool onlyDisassemblable, ListSort sort);
    /* the solutions not seen by the last walk gone, from all() and the rows */
    void dropUnseen(Change & ch);

    bool m_valid = false;
    bool m_keyed = false;            ///< the assembly numbers are unique
    Basis m_basis;
    bool m_only = false;
    ListKey m_key = ListKey::Number;   ///< the sort in effect
    bool m_descending = false;
    std::vector<SolutionFacts> m_all;
    std::vector<int> m_shown;
    std::unordered_map<unsigned int, int> m_ids;   ///< assembly number -> position in m_all
    std::vector<unsigned int> m_seen;              ///< per m_all: the walk that saw it
    unsigned int m_walk = 0;
  };

  /* Manage ▸ Remove assemblies (OQ-25): the saved indices each removes,
   * ascending. Before and After are the rows above and below the selected
   * row as the list shows them (`shown`, `selectedRow` in it); All and
   * Without a disassembly reach the hidden ones too, as legacy does. The
   * hover preview tints exactly the shown ones among them. */
  enum class RemoveKind { At, Before, After, WithoutDisassembly, All };
  std::vector<int> toRemove(const std::vector<SolutionFacts> & all, const std::vector<int> & shown,
                            int selectedRow, RemoveKind kind);

  /* After removing, the row to select: the selected one where it is now,
   * or the one that took its place, else the new last; -1 for an empty
   * list. `removedAbove` counts the removed rows shown above it. */
  int rowAfterRemoval(int selectedRow, int removedAbove, int newCount);

  // --- the pieces (C20 piece display, C21) ---------------------------------

  /* a piece the assembly places: "S3", or "S5.1", "S5.2" for a shape with
   * more than one copy in the problem (legacy PieceVisibility) */
  struct PieceChip {
    std::string id;
    int shape = 0;                 ///< the shape's position
    int copy = 0;                  ///< which copy of it
    int piece = 0;                 ///< the piece's number in the assembly
    bool operator==(const PieceChip &) const = default;
  };
  std::vector<PieceChip> pieceChips(const problem_c & prob, const assembly_c * assembly);
  /* every piece the problem may hold, placed or not (the explorer's list) */
  std::vector<PieceChip> pieceChips(const problem_c & prob);

  /* Export Solution to STL (legacy stlExportSolution_c): the shapes the
   * assembly places, each once, ascending; and the file a shape is written
   * to -- its name, or "S3" without one, with every character but letters,
   * digits, '-' and '_' made '_', and ".stl" */
  std::vector<int> placedShapes(const problem_c & prob, const assembly_c * assembly);
  std::string solutionStlName(const std::string & shapeName, int shape);

  enum class PieceState { Solid, Wireframe, Hidden };
  /* a click: solid -> wireframe -> hidden -> solid */
  PieceState nextPieceState(PieceState s);

  // --- the player (C20) ----------------------------------------------------

  constexpr double playMovesPerSecond = 1.2;
  constexpr double playSpeeds[] = { 0.5, 1, 2, 4, 8 };
  constexpr double seekStep = 0.05;

  /* playing for `seconds` at `speed` from `pos`; never past the end */
  double playedTo(double pos, double total, double seconds, double speed);
  /* one move back or forward, to whole moves: 14.2 back is 14, forward 15 */
  double stepMove(double pos, double total, int direction);

  // --- the keys (overview/04 §8, C20) ---------------------------------------

  enum class KeyContext { List, Slider, Seekbar };
  enum class SolverKey { Space, Enter, Left, Right, Up, Down, Home, End, PageUp, PageDown, Delete, Backspace, Escape };
  enum class SolverAction {
    None, TogglePlay, MoveBack, MoveForward, ToAssembled, ToDisassembled,
    Previous, Next, First, Last, PageBack, PageForward, Remove, Pause,
  };
  SolverAction solverKey(KeyContext context, SolverKey key, bool shift);
  constexpr int solverPage = 8;
  /* the row a solution move lands on in a list of `count`; the row itself
   * for any other action, -1 for an empty list */
  int rowAfter(SolverAction a, int row, int count);
}

#endif
