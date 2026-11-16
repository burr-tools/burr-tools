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

  SolutionFacts solutionFactsOf(const solution_c & s, int index, const std::vector<unsigned int> & cannot) {
    SolutionFacts f;
    f.index = index;
    f.assembly = s.getAssemblyNumber();
    if (const assembly_c * a = s.getAssembly())
      for (unsigned k = 0; k < a->placementCount(); k++)
        f.pieces += a->isPlaced(k) ? 1 : 0;
    if (const disassembly_c * d = s.getDisassemblyInfo()) {
      f.da = s.getDisassembly() ? DaState::Stored : DaState::LevelOnly;
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
    } else if (std::binary_search(cannot.begin(), cannot.end(), f.assembly))
      f.da = DaState::Cannot;
    return f;
  }

  std::vector<SolutionFacts> solutionFacts(const problem_c & prob, const std::vector<unsigned int> & cannot) {
    std::vector<SolutionFacts> out;
    const unsigned n = prob.getNumberOfSavedSolutions();
    out.reserve(n);
    for (unsigned i = 0; i < n; i++)
      out.push_back(solutionFactsOf(*prob.getSavedSolution(i), int(i), cannot));
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

    /* the sort in effect: Level and Moves fall back to Number while no
     * solution has them, ascending */
    ListSort sortInEffect(const std::vector<SolutionFacts> & all, ListSort sort) {
      const ListKey key = listKeyAvailable(all, sort.key) ? sort.key : ListKey::Number;
      return { key, key == sort.key ? sort.descending : false };
    }

    /* whether a row comes before b in the sort in effect: the key in the
     * sort's direction, the number of a tie always ascending (by Number, the
     * number itself is the key) */
    bool rowBefore(const SolutionFacts & a, const SolutionFacts & b, ListSort effective) {
      const int c = compareBy(a, b, effective.key);
      if (c != 0)
        return effective.descending ? c > 0 : c < 0;
      if (effective.key == ListKey::Number && effective.descending)
        return a.assembly > b.assembly;
      return a.assembly < b.assembly;
    }
  }

  std::vector<int> shownSolutions(const std::vector<SolutionFacts> & all, bool onlyDisassemblable, ListSort sort) {
    std::vector<int> rows;
    rows.reserve(all.size());
    for (size_t i = 0; i < all.size(); i++)
      if (!onlyDisassemblable || canBeTakenApart(all[i]))
        rows.push_back(int(i));
    const ListSort eff = sortInEffect(all, sort);
    std::stable_sort(rows.begin(), rows.end(), [&](int x, int y) {
      return rowBefore(all[size_t(x)], all[size_t(y)], eff);
    });
    return rows;
  }

  // --- the list kept between reads ------------------------------------------------

  SolutionList::Change SolutionList::read(const problem_c & prob, const std::vector<unsigned int> & cannot,
                                          bool onlyDisassemblable, ListSort sort) {
    Source src;
    src.count = prob.getNumberOfSavedSolutions();
    src.assembly = [&](unsigned i) { return prob.getSavedSolution(i)->getAssemblyNumber(); };
    src.facts = [&](unsigned i) { return solutionFactsOf(*prob.getSavedSolution(i), int(i), cannot); };
    Basis basis;
    basis.owner = &prob;
    basis.changes = solution_c::changeCount();
    basis.cannot = cannot;
    return read(src, basis, onlyDisassemblable, sort);
  }

  SolutionList::Change SolutionList::readChanged(const problem_c & prob, const std::vector<unsigned int> & cannot,
                                                 const std::vector<unsigned int> & assemblies,
                                                 bool onlyDisassemblable, ListSort sort) {
    Source src;
    src.count = prob.getNumberOfSavedSolutions();
    src.assembly = [&](unsigned i) { return prob.getSavedSolution(i)->getAssemblyNumber(); };
    src.facts = [&](unsigned i) { return solutionFactsOf(*prob.getSavedSolution(i), int(i), cannot); };
    Basis basis;
    basis.owner = &prob;
    basis.changes = solution_c::changeCount();
    basis.cannot = cannot;
    return readChanged(src, basis, assemblies, onlyDisassemblable, sort);
  }

  SolutionList::Change SolutionList::readChanged(const Source & src, const Basis & basis,
                                                 const std::vector<unsigned int> & assemblies,
                                                 bool onlyDisassemblable, ListSort sort) {
    if (!m_valid || !m_keyed || basis.owner != m_basis.owner || src.count != m_all.size())
      return readAll(src, basis, onlyDisassemblable, sort);

    // the changed ones' facts again, each found by its number where it stood
    std::vector<int> changed;
    changed.reserve(assemblies.size());
    for (unsigned a : assemblies) {
      const auto it = m_ids.find(a);
      if (it == m_ids.end())
        return readAll(src, basis, onlyDisassemblable, sort);
      SolutionFacts & f = m_all[size_t(it->second)];
      const int index = f.index;
      if (index < 0 || unsigned(index) >= src.count || src.assembly(unsigned(index)) != a)
        return readAll(src, basis, onlyDisassemblable, sort);
      f = src.facts(unsigned(index));
      f.index = index;
      f.assembly = a;
      changed.push_back(it->second);
    }
    m_basis = basis;
    std::sort(changed.begin(), changed.end());
    changed.erase(std::unique(changed.begin(), changed.end()), changed.end());

    // the filter or the sort in effect changed (Moves offered once one has
    // them): the rows anew
    const ListSort eff = sortInEffect(m_all, sort);
    if (onlyDisassemblable != m_only || eff.key != m_key || eff.descending != m_descending) {
      m_only = onlyDisassemblable;
      m_key = eff.key;
      m_descending = eff.descending;
      m_shown = shownSolutions(m_all, onlyDisassemblable, sort);
      Change reset;
      reset.reset = true;
      return reset;
    }

    Change ch;
    // their rows out: the others close up, in their order
    std::vector<char> isChanged(m_all.size(), 0);
    for (int c : changed)
      isChanged[size_t(c)] = 1;
    size_t w = 0;
    for (size_t r = 0; r < m_shown.size(); r++) {
      if (isChanged[size_t(m_shown[r])])
        ch.removed.push_back(int(r));
      else
        m_shown[w++] = m_shown[r];
    }
    m_shown.resize(w);

    // and in again where they belong, merged from the end as read() does
    std::erase_if(changed, [&](int c) { return onlyDisassemblable && !canBeTakenApart(m_all[size_t(c)]); });
    if (changed.empty())
      return ch;
    std::sort(changed.begin(), changed.end(), [&](int x, int y) { return rowBefore(m_all[size_t(x)], m_all[size_t(y)], eff); });
    size_t i = m_shown.size(), j = changed.size();
    w = m_shown.size() + changed.size();
    m_shown.resize(w);
    ch.inserted.resize(changed.size());
    while (j > 0) {
      w--;
      if (i > 0 && rowBefore(m_all[size_t(changed[j - 1])], m_all[size_t(m_shown[i - 1])], eff))
        m_shown[w] = m_shown[--i];
      else {
        m_shown[w] = changed[--j];
        ch.inserted[j] = int(w);
      }
    }
    return ch;
  }

  SolutionList::Change SolutionList::clear(void) {
    Change ch;
    ch.reset = m_valid;
    m_valid = false;
    m_all.clear();
    m_shown.clear();
    m_ids.clear();
    m_seen.clear();
    return ch;
  }

  SolutionList::Change SolutionList::readAll(const Source & src, const Basis & basis, bool onlyDisassemblable, ListSort sort) {
    m_all.clear();
    m_all.reserve(src.count);
    m_ids.clear();
    m_keyed = true;
    for (unsigned i = 0; i < src.count; i++) {
      m_all.push_back(src.facts(i));
      m_all.back().index = int(i);
      m_keyed = m_ids.emplace(m_all.back().assembly, int(i)).second && m_keyed;
    }
    m_seen.assign(m_all.size(), m_walk);
    m_shown = shownSolutions(m_all, onlyDisassemblable, sort);
    const ListSort eff = sortInEffect(m_all, sort);
    m_key = eff.key;
    m_descending = eff.descending;
    m_only = onlyDisassemblable;
    m_basis = basis;
    m_valid = true;
    Change ch;
    ch.reset = true;
    return ch;
  }

  void SolutionList::dropUnseen(Change & ch) {
    // the rows of the ones gone, and the rows closed up
    size_t w = 0;
    for (size_t r = 0; r < m_shown.size(); r++)
      if (m_seen[size_t(m_shown[r])] == m_walk)
        m_shown[w++] = m_shown[r];
      else
        ch.removed.push_back(int(r));
    m_shown.resize(w);
    // all() closed up, the rows and the numbers following
    std::vector<int> moved(m_all.size(), -1);
    size_t k = 0;
    for (size_t i = 0; i < m_all.size(); i++)
      if (m_seen[i] == m_walk) {
        moved[i] = int(k);
        if (k != i) {
          m_all[k] = std::move(m_all[i]);
          m_seen[k] = m_seen[i];
        }
        k++;
      }
    m_all.resize(k);
    m_seen.resize(k);
    for (int & r : m_shown)
      r = moved[size_t(r)];
    m_ids.clear();
    for (size_t i = 0; i < m_all.size(); i++)
      m_ids.emplace(m_all[i].assembly, int(i));
  }

  SolutionList::Change SolutionList::read(const Source & src, const Basis & basis, bool onlyDisassemblable, ListSort sort) {
    if (!m_valid || !m_keyed || !(basis == m_basis))
      return readAll(src, basis, onlyDisassemblable, sort);

    // the walk: every solution's place in the list, the facts of the new ones
    m_walk++;
    const size_t known = m_all.size();
    for (unsigned i = 0; i < src.count; i++) {
      const unsigned a = src.assembly(i);
      const auto it = m_ids.find(a);
      if (it == m_ids.end()) {
        m_ids.emplace(a, int(m_all.size()));
        m_all.push_back(src.facts(i));
        m_all.back().index = int(i);
        m_all.back().assembly = a;
        m_seen.push_back(m_walk);
      } else if (m_seen[size_t(it->second)] == m_walk)
        return readAll(src, basis, onlyDisassemblable, sort);   // two of one number
      else {
        m_seen[size_t(it->second)] = m_walk;
        m_all[size_t(it->second)].index = int(i);
      }
    }

    Change ch;
    std::vector<int> fresh;
    for (size_t i = known; i < m_all.size(); i++)
      fresh.push_back(int(i));
    if (std::any_of(m_seen.begin(), m_seen.begin() + std::ptrdiff_t(known), [&](unsigned s) { return s != m_walk; })) {
      const size_t before = m_all.size();
      dropUnseen(ch);
      // the new ones, all seen, closed up after the ones kept
      const size_t gone = before - m_all.size();
      for (int & f : fresh)
        f -= int(gone);
    }

    // the filter or the sort in effect changed (Moves offered once one has
    // them): the rows anew
    const ListSort eff = sortInEffect(m_all, sort);
    if (onlyDisassemblable != m_only || eff.key != m_key || eff.descending != m_descending) {
      m_only = onlyDisassemblable;
      m_key = eff.key;
      m_descending = eff.descending;
      m_shown = shownSolutions(m_all, onlyDisassemblable, sort);
      Change reset;
      reset.reset = true;
      return reset;
    }

    // the new ones sorted among themselves, then merged from the end: the
    // rows grow by their number, and from the last down each place takes the
    // later of the last old row and the last new one not placed yet
    std::erase_if(fresh, [&](int f) { return onlyDisassemblable && !canBeTakenApart(m_all[size_t(f)]); });
    if (fresh.empty())
      return ch;
    std::sort(fresh.begin(), fresh.end(), [&](int x, int y) { return rowBefore(m_all[size_t(x)], m_all[size_t(y)], eff); });
    size_t i = m_shown.size(), j = fresh.size(), w = m_shown.size() + fresh.size();
    m_shown.resize(w);
    ch.inserted.resize(fresh.size());
    while (j > 0) {
      w--;
      if (i > 0 && rowBefore(m_all[size_t(fresh[j - 1])], m_all[size_t(m_shown[i - 1])], eff))
        m_shown[w] = m_shown[--i];
      else {
        m_shown[w] = fresh[--j];
        ch.inserted[j] = int(w);
      }
    }
    return ch;
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
