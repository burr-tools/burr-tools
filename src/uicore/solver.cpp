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
#include "solver.h"

#include "../lib/assembly.h"
#include "../lib/disassembly.h"
#include "../lib/problem.h"
#include "../lib/solution.h"
#include "../lib/solvethread.h"

#include <algorithm>
#include <cmath>

namespace btui {

  // --- the search options --------------------------------------------------

  bool checksDisassembly(const SolverOptions & o) {
    return o.disassemble || o.dropDisassemblies;
  }

  bool searchSortAvailable(const SolverOptions & o, SearchSort s) {
    switch (s) {
      case SearchSort::Id:
      case SearchSort::Pieces:
        return true;
      case SearchSort::Level:
      case SearchSort::Moves:
        return checksDisassembly(o);
    }
    return false;
  }

  SearchSort effectiveSort(const SolverOptions & o) {
    return searchSortAvailable(o, o.sort) ? o.sort : SearchSort::Id;
  }

  int changedOptions(const SolverOptions & o) {
    const SolverOptions d;
    return int(o.justCount != d.justCount) + int(o.keepMirror != d.keepMirror) + int(o.keepRotated != d.keepRotated)
         + int(o.thoroughRotation != d.thoroughRotation) + int(o.sort != d.sort) + int(o.keepEvery != d.keepEvery)
         + int(o.limit != d.limit);
  }

  SolverOptions normalised(SolverOptions o) {
    o.keepEvery = std::clamp(o.keepEvery, 1, maxSolverCount);
    o.limit = std::clamp(o.limit, 1, maxSolverCount);
    if (o.dropDisassemblies)
      o.disassemble = true;
    return o;
  }

  SolverParameters solverParameters(const SolverOptions & opt) {
    const SolverOptions o = normalised(opt);
    SolverParameters p;
    p.flags = solveThread_c::PAR_REDUCE;
    if (o.keepMirror) p.flags |= solveThread_c::PAR_KEEP_MIRROR;
    if (o.keepRotated) p.flags |= solveThread_c::PAR_KEEP_ROTATIONS;
    if (o.dropDisassemblies) p.flags |= solveThread_c::PAR_DROP_DISASSEMBLIES;
    if (checksDisassembly(o)) p.flags |= solveThread_c::PAR_DISASSM;
    if (o.justCount) p.flags |= solveThread_c::PAR_JUST_COUNT;
    if (o.thoroughRotation) p.flags |= solveThread_c::PAR_COMPLETE_ROTATIONS;
    switch (effectiveSort(o)) {
      case SearchSort::Id:     p.sortMethod = solveThread_c::SRT_UNSORT; break;
      case SearchSort::Pieces: p.sortMethod = solveThread_c::SRT_UNSORT; p.liveSort = 3; break;
      case SearchSort::Level:  p.sortMethod = solveThread_c::SRT_LEVEL; break;
      case SearchSort::Moves:  p.sortMethod = solveThread_c::SRT_COMPLETE_MOVES; break;
    }
    p.limit = unsigned(o.limit);
    p.drop = unsigned(o.keepEvery);
    return p;
  }

  // --- the run ---------------------------------------------------------------

  RunState runState(const RunFacts & f) {
    if (!f.problem)
      return RunState::Idle;
    if (f.threadHere)
      switch (f.action) {
        case solveThread_c::ACT_PREPARATION:
        case solveThread_c::ACT_REDUCE:
          return RunState::Preparing;
        case solveThread_c::ACT_ASSEMBLING:
        case solveThread_c::ACT_DISASSEMBLING:
          return RunState::Running;
        case solveThread_c::ACT_WAIT_TO_STOP:
          return RunState::Stopping;
        case solveThread_c::ACT_PAUSING:
          return f.prepared ? RunState::Ready : RunState::Paused;
        case solveThread_c::ACT_FINISHED:
          return RunState::Finished;
        default:
          return RunState::Idle;       // an error: the run is over
      }
    switch (f.solveState) {
      case SS_SOLVING: return f.prepared ? RunState::Ready : RunState::Paused;
      case SS_SOLVED:  return RunState::Finished;
      default:         return RunState::Idle;
    }
  }

  RunButtons runButtons(const RunFacts & f) {
    RunButtons b;
    const RunState s = runState(f);
    // nothing runs without a problem, whatever else is said
    const bool free = f.problem && !f.threadHere && !f.threadElsewhere && !f.busy;
    switch (s) {
      case RunState::Idle:
      case RunState::Finished:
      case RunState::Ready:
      case RunState::Paused:
        b.prepare = b.start = free && f.solvable;
        // OQ-29: Continue runs a stopped search, and a prepared one too
        b.cont = free && f.solvable && (s == RunState::Paused || s == RunState::Ready) && f.canContinue;
        // legacy: the placement browser and Step need the assembler, no thread;
        // Step also an unfinished search
        b.placements = free && f.hasAssembler;
        b.step = b.placements && f.solveState != SS_SOLVED;
        break;
      case RunState::Preparing:
      case RunState::Running:
        b.stop = f.threadHere;
        break;
      case RunState::Stopping:
        break;
    }
    return b;
  }

  Activity activity(const RunFacts & f, bool justCount) {
    if (!f.problem)
      return Activity::None;
    if (f.threadHere)
      switch (f.action) {
        case solveThread_c::ACT_PREPARATION:   return Activity::Preparing;
        case solveThread_c::ACT_REDUCE:        return Activity::Optimising;
        case solveThread_c::ACT_ASSEMBLING:    return justCount ? Activity::Counting : Activity::Assembling;
        case solveThread_c::ACT_DISASSEMBLING: return justCount ? Activity::Counting : Activity::Disassembling;
        case solveThread_c::ACT_WAIT_TO_STOP:  return Activity::Stopping;
        case solveThread_c::ACT_PAUSING:       return f.prepared ? Activity::Ready : Activity::Paused;
        case solveThread_c::ACT_FINISHED:      return Activity::Finished;
        default:                               return Activity::Error;
      }
    switch (f.solveState) {
      case SS_SOLVING: return f.prepared ? Activity::Ready : Activity::Paused;
      case SS_SOLVED:  return Activity::Finished;
      case SS_UNKNOWN: return Activity::Partial;
      default:         return Activity::None;
    }
  }

  TimeText timeText(double t) {
    using U = TimeText::Unit;
    const double minute = 60, hour = 60 * minute, day = 24 * hour, month = 30 * day, year = 365.2422 * day;
    auto oneDecimal = [](double v) { return std::round(v * 10) / 10; };
    if (!(t >= 0)) t = 0;
    if (t < minute)               return { U::Seconds, std::floor(t) };
    if (t < hour)                 return { U::Minutes, oneDecimal(t / minute) };
    if (t < day)                  return { U::Hours, oneDecimal(t / hour) };
    if (t < month)                return { U::Days, oneDecimal(t / day) };
    if (t < year)                 return { U::Months, oneDecimal(t / month) };
    if (t < year * 1000)          return { U::Years, oneDecimal(t / year) };
    if (t < year * 1000 * 1000)   return { U::Millennia, oneDecimal(t / (year * 1000)) };
    return { U::Ages, 0 };
  }

  std::optional<double> timeLeft(double used, double progress) {
    if (!(progress >= timeLeftAfter) || progress > 1 || !(used >= 0))
      return std::nullopt;
    return std::max(0.0, used / progress - used);
  }

  // --- the solutions -----------------------------------------------------------

  std::vector<SolutionFacts> solutionFacts(const problem_c & prob, const std::vector<unsigned int> & cannot) {
    std::vector<SolutionFacts> out;
    const unsigned n = prob.getNumberOfSavedSolutions();
    out.reserve(n);
    for (unsigned i = 0; i < n; i++) {
      const solution_c * s = prob.getSavedSolution(i);
      SolutionFacts f;
      f.index = int(i);
      f.assembly = s->getAssemblyNumber();
      if (const assembly_c * a = s->getAssembly())
        for (unsigned k = 0; k < a->placementCount(); k++)
          f.pieces += a->isPlaced(k) ? 1 : 0;
      if (const disassembly_c * d = s->getDisassemblyInfo()) {
        f.da = s->getDisassembly() ? DaState::Stored : DaState::LevelOnly;
        // the sequences' lengths are protected; movesText() says them, "3.1.1"
        f.levelText = d->movesText();
        unsigned n = 0;
        bool digits = false;
        for (char ch : f.levelText) {
          if (ch >= '0' && ch <= '9') {
            n = n * 10 + unsigned(ch - '0');
            digits = true;
          } else if (digits) {
            f.level.push_back(n);
            n = 0;
            digits = false;
          }
        }
        if (digits)
          f.level.push_back(n);
        f.moves = int(d->sumMoves());
      } else if (std::find(cannot.begin(), cannot.end(), f.assembly) != cannot.end())
        f.da = DaState::Cannot;
      out.push_back(std::move(f));
    }
    return out;
  }

  ListSort clickSort(ListSort current, ListKey key) {
    if (current.key == key)
      return { key, !current.descending };
    return { key, key != ListKey::Number };
  }

  bool listKeyAvailable(const std::vector<SolutionFacts> & all, ListKey key) {
    if (key == ListKey::Number || key == ListKey::Pieces)
      return true;
    return std::any_of(all.begin(), all.end(), [](const SolutionFacts & f) { return f.moves >= 0; });
  }

  namespace {

    /* disassembly_c::compare: sequence by sequence, a missing one as 0 */
    int compareLevel(const std::vector<unsigned> & a, const std::vector<unsigned> & b) {
      const size_t n = std::max(a.size(), b.size());
      for (size_t i = 0; i < n; i++) {
        const unsigned x = i < a.size() ? a[i] : 0, y = i < b.size() ? b[i] : 0;
        if (x != y)
          return x < y ? -1 : 1;
      }
      return 0;
    }

    /* < 0 when a comes first in ascending order. Unknown levels and moves
     * sort below every known one, so "best first" puts them last. */
    int compareBy(const SolutionFacts & a, const SolutionFacts & b, ListKey key) {
      switch (key) {
        case ListKey::Number:
          return 0;
        case ListKey::Level: {
          const bool ka = a.moves >= 0, kb = b.moves >= 0;
          if (ka != kb)
            return ka ? 1 : -1;
          return compareLevel(a.level, b.level);
        }
        case ListKey::Moves:
          return a.moves < b.moves ? -1 : a.moves > b.moves ? 1 : 0;
        case ListKey::Pieces:
          return a.pieces - b.pieces;
      }
      return 0;
    }

    bool canBeTakenApart(const SolutionFacts & f) {
      return f.da == DaState::Stored || f.da == DaState::LevelOnly;
    }
  }

  std::vector<int> shownSolutions(const std::vector<SolutionFacts> & all, bool onlyDisassemblable, ListSort sort) {
    std::vector<int> rows;
    rows.reserve(all.size());
    for (size_t i = 0; i < all.size(); i++)
      if (!onlyDisassemblable || canBeTakenApart(all[i]))
        rows.push_back(int(i));
    const ListKey key = listKeyAvailable(all, sort.key) ? sort.key : ListKey::Number;
    const bool desc = key == sort.key ? sort.descending : false;
    std::stable_sort(rows.begin(), rows.end(), [&](int x, int y) {
      const SolutionFacts & a = all[size_t(x)];
      const SolutionFacts & b = all[size_t(y)];
      // the key in the sort's direction, the number of a tie always ascending
      // (by Number, the number itself is the key)
      int c = compareBy(a, b, key);
      if (c != 0)
        return desc ? c > 0 : c < 0;
      if (key == ListKey::Number && desc)
        return a.assembly > b.assembly;
      return a.assembly < b.assembly;
    });
    return rows;
  }

  std::vector<int> toRemove(const std::vector<SolutionFacts> & all, const std::vector<int> & shown,
                            int selectedRow, RemoveKind kind) {
    std::vector<int> out;
    const bool selected = selectedRow >= 0 && selectedRow < int(shown.size());
    auto index = [&](int row) { return all[size_t(shown[size_t(row)])].index; };
    switch (kind) {
      case RemoveKind::At:
        if (selected)
          out.push_back(index(selectedRow));
        break;
      case RemoveKind::Before:
        if (selected)
          for (int r = 0; r < selectedRow; r++)
            out.push_back(index(r));
        break;
      case RemoveKind::After:
        if (selected)
          for (int r = selectedRow + 1; r < int(shown.size()); r++)
            out.push_back(index(r));
        break;
      case RemoveKind::WithoutDisassembly:
        for (const SolutionFacts & f : all)
          if (!canBeTakenApart(f))
            out.push_back(f.index);
        break;
      case RemoveKind::All:
        for (const SolutionFacts & f : all)
          out.push_back(f.index);
        break;
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
  }

  int rowAfterRemoval(int selectedRow, int removedAbove, int newCount) {
    if (newCount <= 0)
      return -1;
    return std::clamp(selectedRow - removedAbove, 0, newCount - 1);
  }

  // --- the pieces ------------------------------------------------------------------

  namespace {
    std::vector<PieceChip> chipsOf(const problem_c & prob, const assembly_c * assembly) {
      std::vector<PieceChip> out;
      int piece = 0;
      for (unsigned p = 0; p < prob.getNumberOfParts(); p++) {
        const unsigned copies = prob.getPartMaximum(p);
        const int shape = int(prob.getShapeIdOfPart(p));
        for (unsigned q = 0; q < copies; q++, piece++) {
          if (assembly && (unsigned(piece) >= assembly->placementCount() || !assembly->isPlaced(unsigned(piece))))
            continue;
          PieceChip c;
          c.id = "S" + std::to_string(shape + 1);
          if (copies > 1)
            c.id += "." + std::to_string(q + 1);
          c.shape = shape;
          c.copy = int(q);
          c.piece = piece;
          out.push_back(std::move(c));
        }
      }
      return out;
    }
  }

  std::vector<PieceChip> pieceChips(const problem_c & prob, const assembly_c * assembly) {
    if (!assembly)
      return {};
    return chipsOf(prob, assembly);
  }

  std::vector<PieceChip> pieceChips(const problem_c & prob) {
    return chipsOf(prob, nullptr);
  }

  std::vector<int> placedShapes(const problem_c & prob, const assembly_c * assembly) {
    std::vector<int> out;
    for (const PieceChip & c : pieceChips(prob, assembly))
      out.push_back(c.shape);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
  }

  std::string solutionStlName(const std::string & shapeName, int shape) {
    std::string out = shapeName.empty() ? "S" + std::to_string(shape + 1) : shapeName;
    for (char & c : out) {
      const unsigned char u = static_cast<unsigned char>(c);
      const bool keep = (u >= '0' && u <= '9') || (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') || c == '-' || c == '_';
      if (!keep)
        c = '_';
    }
    return out + ".stl";
  }

  PieceState nextPieceState(PieceState s) {
    switch (s) {
      case PieceState::Solid:     return PieceState::Wireframe;
      case PieceState::Wireframe: return PieceState::Hidden;
      case PieceState::Hidden:    return PieceState::Solid;
    }
    return PieceState::Solid;
  }

  // --- the player ------------------------------------------------------------------

  double playedTo(double pos, double total, double seconds, double speed) {
    if (!(total > 0))
      return 0;
    const double to = pos + std::max(0.0, seconds) * playMovesPerSecond * speed;
    return std::clamp(to, 0.0, total);
  }

  double stepMove(double pos, double total, int direction) {
    if (!(total > 0))
      return 0;
    // a hair's tolerance, so 14.0000001 steps back to 14 and not 13 later
    const double eps = 1e-6;
    double to = pos;
    if (direction < 0)
      to = std::ceil(pos - eps) - 1;
    else if (direction > 0)
      to = std::floor(pos + eps) + 1;
    return std::clamp(to, 0.0, total);
  }

  // --- the keys ----------------------------------------------------------------------

  SolverAction solverKey(KeyContext context, SolverKey key, bool shift) {
    using A = SolverAction;
    switch (key) {
      case SolverKey::Space:
      case SolverKey::Enter:     return A::TogglePlay;
      case SolverKey::Left:      return shift ? A::ToAssembled : A::MoveBack;
      case SolverKey::Right:     return shift ? A::ToDisassembled : A::MoveForward;
      case SolverKey::Up:        return A::Previous;
      case SolverKey::Down:      return A::Next;
      case SolverKey::Home:      return context == KeyContext::Seekbar ? A::ToAssembled : A::First;
      case SolverKey::End:       return context == KeyContext::Seekbar ? A::ToDisassembled : A::Last;
      case SolverKey::PageUp:    return A::PageBack;
      case SolverKey::PageDown:  return A::PageForward;
      case SolverKey::Delete:
      case SolverKey::Backspace: return context == KeyContext::List ? A::Remove : A::None;
      case SolverKey::Escape:    return A::Pause;
    }
    return A::None;
  }

  int rowAfter(SolverAction a, int row, int count) {
    if (count <= 0)
      return -1;
    int to = row;
    switch (a) {
      case SolverAction::Previous:    to = row - 1; break;
      case SolverAction::Next:        to = row + 1; break;
      case SolverAction::First:       to = 0; break;
      case SolverAction::Last:        to = count - 1; break;
      case SolverAction::PageBack:    to = row - solverPage; break;
      case SolverAction::PageForward: to = row + solverPage; break;
      case SolverAction::None:
      case SolverAction::TogglePlay:
      case SolverAction::MoveBack:
      case SolverAction::MoveForward:
      case SolverAction::ToAssembled:
      case SolverAction::ToDisassembled:
      case SolverAction::Remove:
      case SolverAction::Pause:
        break;
    }
    return std::clamp(to, 0, count - 1);
  }
}
