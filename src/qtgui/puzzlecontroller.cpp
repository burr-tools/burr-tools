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
#include "puzzlecontroller.h"

#include "guarded.h"
#include "layoutcontroller.h"
#include "shapesmodel.h"
#include "theme.h"

#include "../lib/problem.h"
#include "../lib/puzzle.h"
#include "../lib/voxel.h"
#include "../uicore/drawcolours.h"
#include "../uicore/palette.h"
#include "../uicore/problemlist.h"
#include "../uicore/shapelist.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace {

  QString idOfShape(int shape) {
    return QString::fromStdString(btui::shapeId(shape));
  }

  QString groupName(int group) {
    return QStringLiteral("G%1").arg(group);
  }

  int filled(const voxel_c * v) {
    return int(v->countState(voxel_c::VX_FILLED));
  }

  int variable(const voxel_c * v) {
    return int(v->countState(voxel_c::VX_VARIABLE));
  }
}

PuzzleController::PuzzleController(DocumentController * doc, ShapesModel * shapes, LayoutController * layout, QObject * parent) :
  QObject(parent), m_doc(doc), m_shapes(shapes), m_layout(layout)
{
  m_problems = new RowsModel({ "idText", "label", "solutions" }, this);
  m_goal = new RowsModel({ "role", "title", "shape", "idText", "name", "chipColor", "chipTextColor", "detail" }, this);
  m_pieces = new RowsModel({ "shape", "idText", "label", "chipColor", "chipTextColor", "minCount", "maxCount",
                             "countText", "rangeOn", "groupText" }, this);
  m_library = new RowsModel({ "shape", "idText", "label", "chipColor", "used", "countText" }, this);
  // a piece added or taken away is one row made or dropped in the views, not a shift of the rest
  m_pieces->setKeyed(true);
  m_library->setKeyed(true);
  m_groups = new RowsModel({ "group", "name", "members" }, this);
  m_pieceGroups = new RowsModel({ "group", "name", "count" }, this);
  m_sections = new RowsModel({ "key", "title", "summary", "railText", "badge", "open" }, this);
  m_colours = new RowsModel({ "colour", "name", "swatch", "swatchText" }, this);
  m_rules = new RowsModel({ "pieceColour", "resultColour", "pieceName", "resultName", "allowed" }, this);

  connect(doc, &DocumentController::documentReplaced, this, [this] {
    emit typingEnding(false);     // it was meant for the document that went
    m_problem = 0;                // the first, or none (refresh)
    m_piece = -1;
    m_rangeHeld = -1;
    m_heldCounts.reset();
    emit selectedPieceChanged();
    setHoveredPiece(-1);
    stale();
  });
  connect(doc, &DocumentController::aboutToApplyHistory, this, &PuzzleController::flushTyping);
  connect(doc, &DocumentController::historyApplied, this, [this] {
    m_rangeHeld = -1;
    m_heldCounts.reset();
    // back to where the step was made, if it was made here
    const puzzleHistory_c::selection_t s = m_doc->restoredSelection();
    if (s.problem != puzzleHistory_c::NO_PROBLEM) {
      m_problem = int(s.problem);
      const int piece = s.piece == puzzleHistory_c::NO_SHAPE ? -1 : int(s.piece);
      if (piece != m_piece) {
        m_piece = piece;
        emit selectedPieceChanged();
      }
    }
    stale();
  });
  connect(shapes, &ShapesModel::shapesRenumbering, this, [this] { m_doc->beginStep(m_problem, m_piece, 0); });
  connect(shapes, &ShapesModel::shapesRenumbered, this, &PuzzleController::followShapes);
  connect(doc, &DocumentController::puzzleEdited, this, [this] { stale(); });
  if (const Theme * t = Theme::instance())
    connect(t, &Theme::shapeColoursChanged, this, [this] { stale(); });
  connect(layout, &LayoutController::changed, this, [this] {
    if (m_stale && m_layout->workspace() == LayoutController::Puzzle)
      refresh();
  });
  stale();
}

int PuzzleController::maxCount(void) const {
  return btui::maxPieceCount;
}

/* The lists follow the puzzle while the workspace shows; a paint stroke in
 * Entities costs them nothing. A view that needs them sooner calls refresh(). */
void PuzzleController::stale(void) {
  m_stale = true;
  if (m_layout->workspace() == LayoutController::Puzzle)
    refresh();
}

QString PuzzleController::voxelsText(int n) const {
  return n == 1 ? tr("1 voxel") : tr("%1 voxels").arg(n);
}

QString PuzzleController::rangeText(long long min, long long max) const {
  return min == max ? QString::number(min) : QStringLiteral("%1–%2").arg(min).arg(max);
}

QString PuzzleController::sectionKey(int section) const {
  switch (btui::PuzzleSection(section)) {
    case btui::PuzzleSection::SelectedPiece: return QStringLiteral("piece");
    case btui::PuzzleSection::Groups:        return QStringLiteral("groups");
    case btui::PuzzleSection::ColourRules:   return QStringLiteral("rules");
  }
  return {};
}

QString PuzzleController::sectionTitle(int section) const {
  switch (btui::PuzzleSection(section)) {
    case btui::PuzzleSection::SelectedPiece: return tr("Selected piece");
    case btui::PuzzleSection::Groups:        return tr("Groups");
    case btui::PuzzleSection::ColourRules:   return tr("Colour rules");
  }
  return {};
}

void PuzzleController::refresh(void) {
  guarded([&] {
    m_stale = false;
    const puzzle_c & p = m_doc->session().puzzle();
    const int problemCount = int(p.getNumberOfProblems());
    const int shapeCount = int(p.getNumberOfShapes());

    const int problem = problemCount == 0 ? -1 : std::clamp(m_problem, 0, problemCount - 1);
    const int piece = m_piece < shapeCount ? m_piece : -1;
    const bool problemMoved = problem != m_problemShown;
    const bool pieceMoved = piece != m_piece;
    m_problem = m_problemShown = problem;
    m_piece = piece;

    View v;

    std::vector<RowsModel::Row> problems;
    for (int i = 0; i < problemCount; i++) {
      const problem_c * pr = p.getProblem(unsigned(i));
      problems.push_back({ QString::fromStdString(btui::problemId(i)), QString::fromStdString(pr->getName()),
                           btui::problemSolutionCount(p, i) });
    }
    m_problems->setRows(std::move(problems));

    if (problem >= 0) {
      v.problemIdText = QString::fromStdString(btui::problemId(problem));
      v.problemLabel = QString::fromStdString(p.getProblem(unsigned(problem))->getName());
      v.canDeleteProblem = btui::canDeleteProblem(p, problem);
    }

    // --- the goal: the result, and any other shape a problem names ---------
    std::vector<RowsModel::Row> goal;
    for (const btui::GoalSlot & slot : btui::goalSlots(p, problem)) {
      QString role, title;
      switch (slot.role) {
        case btui::GoalRole::Result:
          role = QStringLiteral("result");
          title = tr("Result");
          break;
      }
      if (slot.shape < 0) {
        goal.push_back({ role, title, -1, QString(), tr("Please choose a shape"), QColor(), QColor(),
                         tr("The shape the pieces must fill") });
        continue;
      }
      if (slot.role == btui::GoalRole::Result) {
        v.resultIdText = idOfShape(slot.shape);
        v.resultColor = ShapesModel::chipColor(slot.shape);
        v.resultTextColor = ShapesModel::chipTextColor(slot.shape);
      }
      const voxel_c * s = p.getShape(unsigned(slot.shape));
      const int fixed = filled(s), total = fixed + variable(s);
      const QString name = QString::fromStdString(s->getName());
      // the shape's code and label: the chip beside it is too small to read for some
      goal.push_back({ role, title, slot.shape, idOfShape(slot.shape),
                       name.isEmpty() ? idOfShape(slot.shape) : tr("%1 %2").arg(idOfShape(slot.shape), name),
                       ShapesModel::chipColor(slot.shape), ShapesModel::chipTextColor(slot.shape),
                       tr("%1 · can hold %2").arg(voxelsText(fixed), rangeText(fixed, total)) });
    }
    m_goal->setRows(std::move(goal));

    // --- the pieces, in shape order; the library, all but the goal ---------
    const std::vector<btui::Group> groups = btui::problemGroups(p, problem);
    std::vector<RowsModel::Row> pieces, library;
    for (int s = 0; s < shapeCount; s++) {
      const btui::PieceCount c = btui::pieceCount(p, problem, s);
      if (btui::isGoalShape(p, problem, s) && !c.inPuzzle())
        continue;
      const QString label = QString::fromStdString(p.getShape(unsigned(s))->getName());
      // one reading of the count, for the tile and the library
      const QString countText = !c.inPuzzle() ? QString()
                              : c.isRange() ? rangeText(c.min, c.max) : QStringLiteral("×%1").arg(c.max);
      library.push_back({ s, idOfShape(s), label, ShapesModel::chipColor(s), c.inPuzzle(), countText });
      if (!c.inPuzzle())
        continue;
      QStringList names;
      for (const btui::Group & g : groups)
        for (const btui::GroupMember & m : g.members)
          if (m.shape == s)
            names << groupName(g.id);
      pieces.push_back({ s, idOfShape(s), label, ShapesModel::chipColor(s), ShapesModel::chipTextColor(s), c.min, c.max, countText,
                         c.isRange() || m_rangeHeld == s, names.join(QStringLiteral(", ")) });
    }
    m_pieces->setRows(std::move(pieces));
    m_library->setRows(std::move(library));

    // --- groups --------------------------------------------------------------
    std::vector<RowsModel::Row> groupRows;
    for (const btui::Group & g : groups) {
      QVariantList members;
      for (const btui::GroupMember & m : g.members)
        members.append(QVariantMap{ { QStringLiteral("idText"), idOfShape(m.shape) },
                                    { QStringLiteral("count"), m.count },
                                    { QStringLiteral("chipColor"), ShapesModel::chipColor(m.shape) },
                                    { QStringLiteral("chipTextColor"), ShapesModel::chipTextColor(m.shape) } });
      groupRows.push_back({ g.id, tr("Group %1").arg(g.id), members });
    }
    m_groups->setRows(std::move(groupRows));

    // --- the selected piece ----------------------------------------------------
    std::vector<RowsModel::Row> pieceGroups;
    const btui::PieceCount selected = btui::pieceCount(p, problem, piece);
    if (piece >= 0 && problem >= 0 && (!btui::isGoalShape(p, problem, piece) || selected.inPuzzle())) {
      const voxel_c * s = p.getShape(unsigned(piece));
      const btui::PieceCount c = selected;
      const QString name = QString::fromStdString(s->getName());
      const int fixed = filled(s), vari = variable(s);
      v.pieceShown = true;
      v.pieceIdText = idOfShape(piece);
      v.pieceName = name.isEmpty() ? tr("Piece") : name;
      v.pieceDetail = tr("%1 · %2 fixed · %3 variable").arg(voxelsText(fixed + vari)).arg(fixed).arg(vari);
      v.pieceColor = ShapesModel::chipColor(piece);
      v.pieceTextColor = ShapesModel::chipTextColor(piece);
      v.pieceMin = c.min;
      v.pieceMax = c.max;
      v.pieceRange = c.isRange() || m_rangeHeld == piece;
      for (const btui::Group & g : groups)
        pieceGroups.push_back({ g.id, groupName(g.id), btui::pieceGroupCount(p, problem, piece, g.id) });
    }
    m_pieceGroups->setRows(std::move(pieceGroups));

    // --- colour rules: no row or column for Default (OQ-22) -------------------
    const int colours = int(p.colorNumber());
    std::vector<RowsModel::Row> colourRows, rules;
    for (int c = 1; c <= colours; c++) {
      const btui::Rgb8 rgb = btui::colourRgb(p, c);
      const QColor swatch(rgb.r, rgb.g, rgb.b);
      const bool white = btui::prefersWhiteText(btui::Rgb{ float(swatch.redF()), float(swatch.greenF()), float(swatch.blueF()) });
      colourRows.push_back({ c, QString::fromStdString(btui::colourName(c)), swatch,
                             white ? QColor(Qt::white) : QColor(Qt::black) });
    }
    for (int a = 1; a <= colours; a++)
      for (int b = 1; b <= colours; b++)
        rules.push_back({ a, b, QString::fromStdString(btui::colourName(a)), QString::fromStdString(btui::colourName(b)),
                          btui::colourRule(p, problem, a, b) });
    m_colours->setRows(std::move(colourRows));
    m_rules->setRows(std::move(rules));

    // --- the sections, for the card and its rail --------------------------------
    std::vector<RowsModel::Row> sections;
    for (const btui::SectionInfo & info : btui::puzzleSections(p, problem)) {
      const QString key = sectionKey(int(info.section));
      /* summary: the header's, empty where the open section says it itself;
       * railText: what the rail's tooltip says, the section being out of sight */
      QString summary, railText, badge;
      switch (info.section) {
        case btui::PuzzleSection::SelectedPiece:
          summary = railText = v.pieceIdText;
          badge = v.pieceIdText;
          break;
        case btui::PuzzleSection::Groups:
          railText = groups.empty() ? tr("No groups in this problem")
                                    : groups.size() == 1 ? tr("1 group") : tr("%1 groups").arg(groups.size());
          summary = groups.empty() ? QString() : railText;
          badge = groups.empty() ? QString() : QString::number(groups.size());
          break;
        case btui::PuzzleSection::ColourRules:
          railText = colours == 0 ? tr("No colours in this puzzle")
                                  : colours == 1 ? tr("1 colour") : tr("%1 colours").arg(colours);
          summary = colours == 0 ? QString() : railText;
          break;
      }
      const auto open = m_sectionOpen.find(key);
      sections.push_back({ key, sectionTitle(int(info.section)), summary, railText, badge,
                           open == m_sectionOpen.end() || open->second });
    }
    m_sections->setRows(std::move(sections));

    // --- the summary strip -------------------------------------------------------
    const btui::ProblemStats st = btui::problemStats(p, problem);
    v.fits = st.fit == btui::ProblemFit::Fits;
    switch (st.fit) {
      case btui::ProblemFit::NoResult: v.summaryTitle = tr("Choose a result"); break;
      case btui::ProblemFit::Fits:     v.summaryTitle = tr("Pieces can fill the result"); break;
      case btui::ProblemFit::TooLarge: v.summaryTitle = tr("Pieces are too large for the result"); break;
      case btui::ProblemFit::TooSmall: v.summaryTitle = tr("Pieces cannot reach the result"); break;
    }
    if (problem >= 0) {
      const QString holes = !st.holesKnown ? tr("Holes depend on the counts")
                          : st.holes >= 0 ? tr("Holes: %1").arg(st.holes)
                          : tr("Pieces exceed the result by %1").arg(-st.holes);
      const QString pieceTotals = tr("Pieces n = %1, %2").arg(rangeText(st.piecesMin, st.piecesMax),
                                                              tr("%1 voxels").arg(rangeText(st.voxelsMin, st.voxelsMax)));
      v.summaryDetail = st.fit == btui::ProblemFit::NoResult
                      ? pieceTotals
                      : tr("Result %1 · %2 · %3").arg(tr("%1 voxels").arg(rangeText(st.resultFixed, st.resultTotal)), pieceTotals, holes);
      const QString problemName = v.problemLabel.isEmpty() ? v.problemIdText : tr("%1 %2").arg(v.problemIdText, v.problemLabel);
      const QString count = st.piecesMax == 1 ? tr("1 piece") : tr("%1 pieces").arg(rangeText(st.piecesMin, st.piecesMax));
      v.sceneName = v.resultIdText.isEmpty() ? tr("%1: no result, %2").arg(problemName, count)
                                             : tr("%1: result %2, %3").arg(problemName, v.resultIdText, count);
      if (st.holeLimit)
        v.holeLimitText = st.maxHoles == 1 ? tr("This file limits the search to 1 hole")
                                           : tr("This file limits the search to %1 holes").arg(st.maxHoles);
    }

    const bool viewChanged = !(v == m_view);
    // a change of fit, by an edit here: a move to another problem reads as that problem
    if (v.summaryTitle != m_view.summaryTitle && !problemMoved && !m_view.summaryTitle.isEmpty())
      emit announceRequested(v.summaryTitle);
    m_view = std::move(v);
    if (problemMoved)
      emit problemChanged();
    if (pieceMoved)
      emit selectedPieceChanged();
    if (viewChanged)
      emit stateChanged();
  });
}

QString PuzzleController::statusText(void) const {
  return guarded([&]() -> QString {
    const puzzle_c & p = m_doc->session().puzzle();
    const btui::ProblemStats st = btui::problemStats(p, m_problem);
    if (st.fit == btui::ProblemFit::NoResult)
      return {};
    const auto spaced = [](long long a, long long b) {
      return a == b ? QString::number(a) : QStringLiteral("%1 - %2").arg(a).arg(b);
    };
    // the bar's language is the legacy sentence's, as StatusController::shapeText's
    return QStringLiteral("Problem <b>P%1</b> result can contain %2 voxels, pieces (n = %3) contain %4 voxels")
      .arg(m_problem + 1).arg(spaced(st.resultFixed, st.resultTotal), spaced(st.piecesMin, st.piecesMax),
                              spaced(st.voxelsMin, st.voxelsMax));
  });
}

// --- selection -----------------------------------------------------------------

void PuzzleController::select(int problem) {
  const int n = m_problems->count();
  const int want = n == 0 ? -1 : std::clamp(problem, 0, n - 1);
  if (want == m_problem)
    return;
  flushTyping();
  m_problem = m_problemShown = want;
  m_rangeHeld = -1;
  emit problemChanged();
  refresh();
}

void PuzzleController::selectProblem(int row) {
  if (m_stale)
    refresh();
  select(row);
}

void PuzzleController::selectProblemBy(int delta) {
  selectProblem(std::max(0, m_problem + delta));
}

void PuzzleController::selectPiece(int shape) {
  if (shape == m_piece)
    return;
  flushTyping();
  m_piece = shape;
  m_rangeHeld = -1;
  emit selectedPieceChanged();
  refresh();
}

void PuzzleController::setHoveredPiece(int shape) {
  if (shape == m_hovered)
    return;
  m_hovered = shape;
  emit hoveredPieceChanged();
}

void PuzzleController::setPiecesView(const QString & v) {
  if (v == m_piecesView || (v != QLatin1String("chips") && v != QLatin1String("table")))
    return;
  m_piecesView = v;
  emit viewChanged();
}

void PuzzleController::setAllPiecesOpen(bool open) {
  if (open == m_allPiecesOpen)
    return;
  m_allPiecesOpen = open;
  emit viewChanged();
}

// --- changes -------------------------------------------------------------------

void PuzzleController::flushTyping(void) {
  emit typingEnding(true);
}

PuzzleController::Step::Step(PuzzleController * c, unsigned int coalesceKey) : m_c(c) {
  m_c->m_doc->beginStep(m_c->m_problem, m_c->m_piece, coalesceKey);
}

PuzzleController::Step::~Step(void) {
  m_c->m_doc->endStep(m_c->m_problem, m_c->m_piece);
}

bool PuzzleController::apply(const Edit & f, puzzleHistory_c::actionKind_e kind) {
  flushTyping();
  return guarded([&] {
    return m_doc->edit(kind, DocumentController::Change::Problem, -1, m_shapes->selected(), f);
  });
}

/* The edit tried on a copy, which holds no solutions: nothing to warn of
 * for a change that changes nothing. */
bool PuzzleController::wouldCostSolutions(int problem, const Edit & f) const {
  const puzzle_c & p = m_doc->session().puzzle();
  if (!btui::problemHasSolutions(p, problem))
    return false;
  puzzle_c copy(&p);
  return f(copy);
}

/* A change to what a problem asks for removes its solutions, into the undo
 * step (DocumentController::editProblems): undo brings them back. It warns
 * only when they are too big for the history to keep. */
void PuzzleController::changeAs(const Edit & f, puzzleHistory_c::actionKind_e kind, unsigned int coalesceKey) {
  flushTyping();
  guarded([&] {
    const int problem = m_problem;
    const auto clearing = [this, problem, f, kind, coalesceKey] {
      Step step(this, coalesceKey);
      const Edit change = [&](puzzle_c & p) {
        if (!f(p))
          return false;
        btui::clearProblemSolutions(p, problem);
        return true;
      };
      if (problem < 0) {
        apply(change, kind);
        return;
      }
      flushTyping();
      guarded([&] { m_doc->editProblems(kind, m_shapes->selected(), { unsigned(problem) }, change); });
    };
    const std::vector<unsigned int> problems = problem < 0 ? std::vector<unsigned int>() : std::vector<unsigned int>{ unsigned(problem) };
    if (m_doc->mayDropProblemSolutions(wouldCostSolutions(problem, f), problems, clearing))
      clearing();
  });
}

void PuzzleController::change(const Edit & f) {
  changeAs(f, puzzleHistory_c::AK_PROBLEM_STRUCTURAL, 0);
}

void PuzzleController::changeCount(int shape, const Edit & f) {
  const btui::PieceCount before = counts(shape);
  changeAs(f, puzzleHistory_c::AK_PROBLEM_COUNT, unsigned(shape));
  const btui::PieceCount now = counts(shape);
  if (now.min != before.min || now.max != before.max)
    emit announceRequested(tr("%1: %2").arg(idOfShape(shape), now.inPuzzle() ? rangeText(now.min, now.max) : tr("not in this puzzle")));
}

btui::PieceCount PuzzleController::counts(int shape) const {
  return guarded([&] { return btui::pieceCount(m_doc->session().puzzle(), m_problem, shape); });
}

/* The shapes were renumbered: the selection stays with its shape. */
void PuzzleController::followShapes(int from, int to) {
  const auto follow = [&](int shape) {
    if (from < 0 || shape < 0)
      return shape;
    if (to < 0)
      return shape == from ? -1 : shape > from ? shape - 1 : shape;
    if (shape == from)
      return to;
    if (from < shape && shape <= to)
      return shape - 1;
    if (to <= shape && shape < from)
      return shape + 1;
    return shape;
  };
  const int piece = follow(m_piece);
  m_rangeHeld = follow(m_rangeHeld);
  if (piece != m_piece) {
    m_piece = piece;
    emit selectedPieceChanged();
    stale();
  }
  m_doc->endStep(m_problem, m_piece);
}

void PuzzleController::newProblem(void) {
  Step step(this);
  int added = -1;
  if (apply([&](puzzle_c & p) { added = btui::newProblem(p); return true; })) {
    select(added);
    emit flashRequested(tr("Problem %1 added").arg(QString::fromStdString(btui::problemId(added))));
  }
}

void PuzzleController::duplicateProblem(int row) {
  Step step(this);
  int added = -1;
  if (apply([&](puzzle_c & p) { added = btui::duplicateProblem(p, row); return added >= 0; })) {
    select(added);
    emit flashRequested(tr("Problem %1 duplicated as %2").arg(QString::fromStdString(btui::problemId(row)),
                                                              QString::fromStdString(btui::problemId(added))));
  }
}

void PuzzleController::deleteProblem(int row) {
  guarded([&] {
    const puzzle_c & p = m_doc->session().puzzle();
    if (!btui::canDeleteProblem(p, row))
      return;
    const auto remove = [this, row] {
      Step step(this);
      const int selected = m_problem;
      int next = -1;
      if (!apply([&](puzzle_c & q) { next = btui::deleteProblem(q, row); return next >= 0; }))
        return;
      select(selected == row ? next : selected > row ? selected - 1 : selected);
      emit flashRequested(tr("Problem %1 deleted").arg(QString::fromStdString(btui::problemId(row))));
    };
    if (m_doc->mayCostSolutions(btui::problemHasSolutions(p, row), remove))
      remove();
  });
}

void PuzzleController::moveProblem(int from, int to) {
  Step step(this);
  const int selected = m_problem;
  if (!apply([&](puzzle_c & p) { return btui::moveProblem(p, from, to); }))
    return;
  // the selection stays with its problem, the rows between shifting
  const int n = m_problems->count();
  const int target = std::clamp(to, 0, n - 1);
  int follow = selected;
  if (selected == from)
    follow = target;
  else if (from < selected && selected <= target)
    follow = selected - 1;
  else if (target <= selected && selected < from)
    follow = selected + 1;
  select(follow);
  emit flashRequested(tr("Order changed — problems renumbered P1…P%1").arg(n));
}

void PuzzleController::renameProblem(int row, const QString & label) {
  Step step(this);
  apply([&](puzzle_c & p) { return btui::renameProblem(p, row, label.toStdString()); });
}

void PuzzleController::setResult(int shape) {
  const int problem = m_problem;
  change([=](puzzle_c & p) { return btui::setResult(p, problem, shape); });
}

void PuzzleController::stepPiece(int shape, int delta) {
  const int problem = m_problem;
  changeCount(shape, [=](puzzle_c & p) { return btui::stepPiece(p, problem, shape, delta); });
}

void PuzzleController::setCount(int shape, int count) {
  if (m_rangeHeld == shape)
    m_rangeHeld = -1;
  const int problem = m_problem;
  changeCount(shape, [=](puzzle_c & p) { return btui::setPieceCount(p, problem, shape, count, count); });
}

/* A range on show stays on show when its two ends meet: they are being
 * edited, and the field in use must not go away. */
void PuzzleController::setRange(int shape, int min, int max) {
  if (counts(shape).isRange())
    m_rangeHeld = shape;
  const int problem = m_problem;
  changeCount(shape, [=](puzzle_c & p) { return btui::setPieceCount(p, problem, shape, min, max); });
}

void PuzzleController::holdCounts(int shape) {
  m_heldCounts = HeldCounts{ shape, counts(shape) };
}

void PuzzleController::releaseCounts(void) {
  m_heldCounts.reset();
}

void PuzzleController::setPieceMin(int shape, int min) {
  const btui::PieceCount c = m_heldCounts && m_heldCounts->shape == shape ? m_heldCounts->counts : counts(shape);
  setRange(shape, min, std::max(min, c.max));
}

void PuzzleController::setPieceMax(int shape, int max) {
  const btui::PieceCount c = m_heldCounts && m_heldCounts->shape == shape ? m_heldCounts->counts : counts(shape);
  setRange(shape, std::min(c.min, max), max);
}

void PuzzleController::setRangeOn(int shape, bool on) {
  const btui::PieceCount c = counts(shape);
  if (!on) {
    m_rangeHeld = -1;
    if (c.isRange()) {
      const int problem = m_problem;
      change([=](puzzle_c & p) { return btui::collapseRange(p, problem, shape); });
    } else {
      refresh();
    }
    return;
  }
  /* from 0 when it can: an optional piece is what a range is mostly for
   * (C17; legacy's min=0). The steppers stay on show once it went ahead. */
  if (c.inPuzzle() && !c.isRange()) {
    const int problem = m_problem;
    change([=](puzzle_c & p) { return btui::setPieceCount(p, problem, shape, 0, c.max); });
    if (counts(shape).isRange())    // not when a warning holds it
      m_rangeHeld = shape;
  } else {
    m_rangeHeld = shape;
    refresh();
  }
}

void PuzzleController::setRangeForAll(int min, int max) {
  const int problem = m_problem;
  change([=](puzzle_c & p) { return btui::setRangeForAll(p, problem, min, max); });
}

void PuzzleController::addOneOfEach(void) {
  const int problem = m_problem;
  change([=](puzzle_c & p) { return btui::addOneOfEach(p, problem); });
}

void PuzzleController::removeAllPieces(void) {
  const int problem = m_problem;
  change([=](puzzle_c & p) { return btui::removeAllPieces(p, problem); });
}

void PuzzleController::setGroupCount(int shape, int group, int count) {
  const int problem = m_problem;
  changeCount(shape, [=](puzzle_c & p) { return btui::setPieceGroupCount(p, problem, shape, group, count); });
}

void PuzzleController::newGroup(int shape) {
  const int group = guarded([&] { return btui::nextGroup(m_doc->session().puzzle(), m_problem); });
  if (group <= 0) {
    emit flashRequested(tr("Every group number is in use"));
    return;
  }
  const int problem = m_problem;
  const int count = counts(shape).max;
  change([=](puzzle_c & p) { return btui::setPieceGroupCount(p, problem, shape, group, count); });
}

int PuzzleController::pieceGroupCount(int shape, int group) const {
  return guarded([&] { return btui::pieceGroupCount(m_doc->session().puzzle(), m_problem, shape, group); });
}

int PuzzleController::copiesOf(int shape) const {
  return guarded([&] { return btui::pieceCount(m_doc->session().puzzle(), m_problem, shape).max; });
}

void PuzzleController::deleteGroup(int group) {
  const int problem = m_problem;
  change([=](puzzle_c & p) { return btui::deleteGroup(p, problem, group); });
}

void PuzzleController::toggleRule(int pieceColour, int resultColour) {
  const int problem = m_problem;
  change([=](puzzle_c & p) {
    return btui::setColourRule(p, problem, pieceColour, resultColour, !btui::colourRule(p, problem, pieceColour, resultColour));
  });
}

void PuzzleController::setRulePreset(int preset) {
  if (preset < 0 || preset > 2)
    return;
  const int problem = m_problem;
  const btui::ColourPreset which = preset == 0 ? btui::ColourPreset::EachOnItself
                                 : preset == 1 ? btui::ColourPreset::AllowAll : btui::ColourPreset::Clear;
  change([=](puzzle_c & p) { return btui::setColourPreset(p, problem, which); });
}

void PuzzleController::clearHoleLimit(void) {
  const int problem = m_problem;
  change([=](puzzle_c & p) { return btui::clearHoleLimit(p, problem); });
}

// --- sections ------------------------------------------------------------------

void PuzzleController::setSectionOpen(const QString & key, bool open) {
  m_sectionOpen[key] = open;
  refresh();
}

void PuzzleController::showSection(const QString & key) {
  m_sectionOpen[key] = true;
  m_layout->setRightCollapsed(false);
  refresh();
  emit sectionRequested(key);
}
