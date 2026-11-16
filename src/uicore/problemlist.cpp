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
#include "problemlist.h"

#include "../lib/problem.h"
#include "../lib/puzzle.h"
#include "../lib/voxel.h"

#include <algorithm>
#include <set>
#include <stdint.h>

namespace btui {

  namespace {
    bool isProblem(const puzzle_c & p, int i) {
      return i >= 0 && unsigned(i) < p.getNumberOfProblems();
    }

    bool isShape(const puzzle_c & p, int s) {
      return s >= 0 && unsigned(s) < p.getNumberOfShapes();
    }

    bool isColour(const puzzle_c & p, int c) {
      return c >= 1 && unsigned(c) <= p.colorNumber();
    }

    /* A piece of the problem: a shape it can hold as a piece. Only a file can
     * make the result a piece too; that part may still be changed, to take it
     * out. */
    bool canBePiece(const puzzle_c & p, int i, int shape) {
      return isProblem(p, i) && isShape(p, shape) &&
             (!isGoalShape(p, i, shape) || pieceCount(p, i, shape).inPuzzle());
    }

    /* Keep the group counts within the piece's new copies. A count that
     * covered every copy keeps doing so, the case the file's short form
     * (one group for the whole piece) stands for. */
    void followGroups(problem_c & pr, unsigned shape, int oldMax, int newMax) {
      if (newMax <= 0)
        return;
      const unsigned part = pr.getPartIdForShape(shape);
      std::vector<std::pair<unsigned short, int>> counts;
      for (unsigned g = 0; g < pr.getNumberOfPartGroups(part); g++)
        counts.emplace_back(pr.getPartGroupId(part, g), pr.getPartGroupCount(part, g));
      for (const auto & [group, count] : counts) {
        const int now = count == oldMax ? newMax : std::min(count, newMax);
        if (now != count)
          pr.setPartGroup(part, group, (unsigned short)now);
      }
    }

    uint32_t colourPair(unsigned piece, unsigned result) {
      return (piece - 1) << 16 | (result - 1);
    }
  }

  std::string problemId(int position) {
    return "P" + std::to_string(position + 1);
  }

  // --- the list ----------------------------------------------------------------

  int newProblem(puzzle_c & p) {
    const unsigned k = p.addProblem();
    problem_c * pr = p.getProblem(k);
    for (unsigned c = 1; c <= p.colorNumber(); c++)
      pr->allowPlacement(c, c);
    return int(k);   // no result: the user chooses it (product decision 2026-11-14)
  }

  int duplicateProblem(puzzle_c & p, int i) {
    if (!isProblem(p, i))
      return -1;
    const std::string label = p.getProblem(unsigned(i))->getName();
    const unsigned k = p.addProblem(p.getProblem(unsigned(i)));
    p.getProblem(k)->setName(label);
    return int(k);
  }

  bool canDeleteProblem(const puzzle_c & p, int i) {
    return isProblem(p, i) && p.getNumberOfProblems() > 1;
  }

  int deleteProblem(puzzle_c & p, int i) {
    if (!canDeleteProblem(p, i))
      return -1;
    p.removeProblem(unsigned(i));
    return std::min(i, int(p.getNumberOfProblems()) - 1);
  }

  bool moveProblem(puzzle_c & p, int from, int to) {
    if (!isProblem(p, from))
      return false;
    to = std::clamp(to, 0, int(p.getNumberOfProblems()) - 1);
    for (int k = from; k < to; k++)
      p.exchangeProblems(unsigned(k), unsigned(k) + 1);
    for (int k = from; k > to; k--)
      p.exchangeProblems(unsigned(k), unsigned(k) - 1);
    return from != to;
  }

  std::string cleanLabel(const std::string & label) {
    std::string clean;
    clean.reserve(label.size());
    for (const char c : label)
      if ((unsigned char)c >= 0x20 && c != 0x7f)
        clean += c;
    return clean;
  }

  bool renameProblem(puzzle_c & p, int i, const std::string & label) {
    if (!isProblem(p, i))
      return false;
    const std::string clean = cleanLabel(label);
    problem_c * pr = p.getProblem(unsigned(i));
    if (pr->getName() == clean)
      return false;
    pr->setName(clean);
    return true;
  }

  bool problemHasSolutions(const puzzle_c & p, int i) {
    if (!isProblem(p, i))
      return false;
    const problem_c * pr = p.getProblem(unsigned(i));
    const solveState_e st = pr->getSolveState();
    return pr->getNumberOfSavedSolutions() > 0 || st == SS_SOLVING || st == SS_SOLVED;
  }

  int problemSolutionCount(const puzzle_c & p, int i) {
    return isProblem(p, i) ? int(p.getProblem(unsigned(i))->getNumberOfSavedSolutions()) : 0;
  }

  void clearProblemSolutions(puzzle_c & p, int i) {
    if (isProblem(p, i))
      p.getProblem(unsigned(i))->removeAllSolutions();
  }

  bool shapeEditCostsSolutions(const puzzle_c & p, int shape) {
    for (int k = 0; k < int(p.getNumberOfProblems()); k++)
      if ((shape < 0 || p.getProblem(unsigned(k))->usesShape(unsigned(shape))) && problemHasSolutions(p, k))
        return true;
    return false;
  }

  // --- what a problem asks for ---------------------------------------------------

  ProblemKind problemKind(const puzzle_c &, int) {
    return ProblemKind::Assembly;
  }

  std::vector<GoalSlot> goalSlots(const puzzle_c & p, int i) {
    if (!isProblem(p, i))
      return {};
    const problem_c * pr = p.getProblem(unsigned(i));
    switch (problemKind(p, i)) {
      case ProblemKind::Assembly:
        return { GoalSlot{ GoalRole::Result, pr->resultValid() ? int(pr->getResultId()) : -1 } };
    }
    return {};
  }

  bool isGoalShape(const puzzle_c & p, int i, int shape) {
    const std::vector<GoalSlot> goal = goalSlots(p, i);
    return std::any_of(goal.begin(), goal.end(), [&](const GoalSlot & s) { return s.shape == shape; });
  }

  std::vector<SectionInfo> puzzleSections(const puzzle_c & p, int i) {
    switch (problemKind(p, i)) {
      case ProblemKind::Assembly:
        return {
          { PuzzleSection::SelectedPiece, SectionScope::Piece },
          { PuzzleSection::Groups, SectionScope::Problem },
          { PuzzleSection::ColourRules, SectionScope::Problem },
        };
    }
    return {};
  }

  // --- the result ----------------------------------------------------------------

  bool setResult(puzzle_c & p, int i, int shape) {
    if (!isProblem(p, i) || !isShape(p, shape))
      return false;
    problem_c * pr = p.getProblem(unsigned(i));
    if (pr->resultValid() && pr->getResultId() == unsigned(shape))
      return false;
    pr->setShapeMaximum(unsigned(shape), 0);
    pr->setResultId(unsigned(shape));
    return true;
  }

  // --- piece counts --------------------------------------------------------------

  PieceCount pieceCount(const puzzle_c & p, int i, int shape) {
    if (!isProblem(p, i) || !isShape(p, shape))
      return {};
    const problem_c * pr = p.getProblem(unsigned(i));
    return { int(pr->getShapeMinimum(unsigned(shape))), int(pr->getShapeMaximum(unsigned(shape))) };
  }

  bool setPieceCount(puzzle_c & p, int i, int shape, int min, int max) {
    if (!canBePiece(p, i, shape))
      return false;
    min = std::clamp(min, 0, maxPieceCount);
    max = std::clamp(std::max(min, max), 0, maxPieceCount);
    const PieceCount was = pieceCount(p, i, shape);
    if (was.min == min && was.max == max)
      return false;
    problem_c * pr = p.getProblem(unsigned(i));
    pr->setShapeMaximum(unsigned(shape), unsigned(max));
    if (max > 0) {
      pr->setShapeMinimum(unsigned(shape), unsigned(min));
      followGroups(*pr, unsigned(shape), was.max, max);
    }
    return true;
  }

  bool stepPiece(puzzle_c & p, int i, int shape, int delta) {
    const PieceCount c = pieceCount(p, i, shape);
    const int max = std::max(0, c.max + delta);
    const int min = c.isRange() ? std::min(c.min, max) : max;
    return setPieceCount(p, i, shape, min, max);
  }

  bool collapseRange(puzzle_c & p, int i, int shape) {
    const PieceCount c = pieceCount(p, i, shape);
    return setPieceCount(p, i, shape, c.max, c.max);
  }

  bool setRangeForAll(puzzle_c & p, int i, int min, int max) {
    if (!isProblem(p, i))
      return false;
    min = std::max(min, 0);
    max = std::max({ max, min, 1 });
    bool changed = false;
    for (int s = 0; s < int(p.getNumberOfShapes()); s++)
      if (pieceCount(p, i, s).inPuzzle())
        changed = setPieceCount(p, i, s, min, max) || changed;
    return changed;
  }

  bool addOneOfEach(puzzle_c & p, int i) {
    if (!isProblem(p, i))
      return false;
    bool changed = false;
    for (int s = 0; s < int(p.getNumberOfShapes()); s++)
      changed = stepPiece(p, i, s, 1) || changed;
    return changed;
  }

  bool removeAllPieces(puzzle_c & p, int i) {
    if (!isProblem(p, i))
      return false;
    bool changed = false;
    for (int s = 0; s < int(p.getNumberOfShapes()); s++)
      changed = setPieceCount(p, i, s, 0, 0) || changed;
    return changed;
  }

  // --- groups --------------------------------------------------------------------

  std::vector<Group> problemGroups(const puzzle_c & p, int i) {
    std::vector<Group> out;
    if (!isProblem(p, i))
      return out;
    const problem_c * pr = p.getProblem(unsigned(i));
    for (int s = 0; s < int(p.getNumberOfShapes()); s++) {
      if (pr->getShapeMaximum(unsigned(s)) == 0)
        continue;
      const unsigned part = pr->getPartIdForShape(unsigned(s));
      for (unsigned g = 0; g < pr->getNumberOfPartGroups(part); g++) {
        const int id = pr->getPartGroupId(part, g);
        const int count = pr->getPartGroupCount(part, g);
        if (id == 0 || count == 0)
          continue;
        auto at = std::lower_bound(out.begin(), out.end(), id, [](const Group & a, int b) { return a.id < b; });
        if (at == out.end() || at->id != id)
          at = out.insert(at, Group{ id, {} });
        at->members.push_back({ s, count });
      }
    }
    return out;
  }

  int pieceGroupCount(const puzzle_c & p, int i, int shape, int group) {
    if (!pieceCount(p, i, shape).inPuzzle())
      return 0;
    const problem_c * pr = p.getProblem(unsigned(i));
    const unsigned part = pr->getPartIdForShape(unsigned(shape));
    for (unsigned g = 0; g < pr->getNumberOfPartGroups(part); g++)
      if (pr->getPartGroupId(part, g) == group)
        return pr->getPartGroupCount(part, g);
    return 0;
  }

  int nextGroup(const puzzle_c & p, int i) {
    const std::vector<Group> groups = problemGroups(p, i);
    if (groups.empty())
      return 1;
    if (groups.back().id < maxGroupId)
      return groups.back().id + 1;
    // the top id is taken (only a file does that): the first one free
    int id = 1;
    for (const Group & g : groups) {
      if (g.id != id)
        return id;
      id++;
    }
    return id <= maxGroupId ? id : 0;
  }

  bool setPieceGroupCount(puzzle_c & p, int i, int shape, int group, int count) {
    const PieceCount c = pieceCount(p, i, shape);
    if (!c.inPuzzle() || group <= 0 || group > maxGroupId)
      return false;
    count = std::clamp(count, 0, c.max);
    if (pieceGroupCount(p, i, shape, group) == count)
      return false;
    problem_c * pr = p.getProblem(unsigned(i));
    pr->setPartGroup(pr->getPartIdForShape(unsigned(shape)), (unsigned short)group, (unsigned short)count);
    return true;
  }

  bool deleteGroup(puzzle_c & p, int i, int group) {
    const std::vector<Group> groups = problemGroups(p, i);
    const auto gone = std::find_if(groups.begin(), groups.end(), [&](const Group & g) { return g.id == group; });
    if (gone == groups.end())
      return false;
    problem_c * pr = p.getProblem(unsigned(i));
    const auto setCount = [&](int shape, int id, int count) {
      pr->setPartGroup(pr->getPartIdForShape(unsigned(shape)), (unsigned short)id, (unsigned short)count);
    };
    for (const GroupMember & m : gone->members)
      setCount(m.shape, group, 0);
    for (auto g = gone + 1; g != groups.end(); ++g)
      for (const GroupMember & m : g->members) {
        setCount(m.shape, g->id, 0);
        setCount(m.shape, g->id - 1, m.count);
      }
    return true;
  }

  // --- colour rules --------------------------------------------------------------

  bool colourRule(const puzzle_c & p, int i, int pieceColour, int resultColour) {
    return isProblem(p, i) && isColour(p, pieceColour) && isColour(p, resultColour) &&
           p.getProblem(unsigned(i))->placementAllowed(unsigned(pieceColour), unsigned(resultColour));
  }

  bool setColourRule(puzzle_c & p, int i, int pieceColour, int resultColour, bool allowed) {
    if (!isProblem(p, i) || !isColour(p, pieceColour) || !isColour(p, resultColour) ||
        colourRule(p, i, pieceColour, resultColour) == allowed)
      return false;
    problem_c * pr = p.getProblem(unsigned(i));
    if (allowed)
      pr->allowPlacement(unsigned(pieceColour), unsigned(resultColour));
    else
      pr->disallowPlacement(unsigned(pieceColour), unsigned(resultColour));
    return true;
  }

  bool setColourPreset(puzzle_c & p, int i, ColourPreset preset) {
    if (!isProblem(p, i))
      return false;
    std::set<uint32_t> rules;
    const unsigned n = p.colorNumber();
    switch (preset) {
      case ColourPreset::EachOnItself:
        for (unsigned c = 1; c <= n; c++)
          rules.insert(colourPair(c, c));
        break;
      case ColourPreset::AllowAll:
        for (unsigned a = 1; a <= n; a++)
          for (unsigned b = 1; b <= n; b++)
            rules.insert(colourPair(a, b));
        break;
      case ColourPreset::Clear:
        break;
    }
    problem_c * pr = p.getProblem(unsigned(i));
    if (pr->getColorConstraints() == rules)
      return false;
    pr->setColorConstraints(std::move(rules));
    return true;
  }

  // --- the summary ---------------------------------------------------------------

  ProblemStats problemStats(const puzzle_c & p, int i) {
    ProblemStats st;
    if (!isProblem(p, i))
      return st;
    const problem_c * pr = p.getProblem(unsigned(i));

    bool fixedCounts = true;
    for (unsigned k = 0; k < pr->getNumberOfParts(); k++) {
      const long long filled = pr->getPartShape(k)->countState(voxel_c::VX_FILLED);
      const long long min = pr->getPartMinimum(k);
      const long long max = pr->getPartMaximum(k);
      st.piecesMin += min;
      st.piecesMax += max;
      st.voxelsMin += filled * min;
      st.voxelsMax += filled * max;
      fixedCounts = fixedCounts && min == max;
    }

    if (pr->maxHolesDefined()) {
      st.holeLimit = true;
      st.maxHoles = int(pr->getMaxHoles());
    }

    if (!pr->resultValid())
      return st;

    const voxel_c * result = p.getShape(pr->getResultId());
    st.resultFixed = result->countState(voxel_c::VX_FILLED);
    const long long variable = result->countState(voxel_c::VX_VARIABLE);
    st.resultTotal = st.resultFixed + variable;

    st.fit = st.voxelsMin > st.resultTotal ? ProblemFit::TooLarge
           : st.voxelsMax < st.resultFixed ? ProblemFit::TooSmall : ProblemFit::Fits;

    if (fixedCounts && variable == 0) {
      st.holesKnown = true;
      st.holes = st.resultFixed - st.voxelsMin;
    }
    return st;
  }

  bool clearHoleLimit(puzzle_c & p, int i) {
    if (!isProblem(p, i) || !p.getProblem(unsigned(i))->maxHolesDefined())
      return false;
    p.getProblem(unsigned(i))->setMaxHolesInvalid();
    return true;
  }
}
