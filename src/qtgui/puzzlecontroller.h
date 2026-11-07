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
#ifndef BTQT_PUZZLECONTROLLER_H
#define BTQT_PUZZLECONTROLLER_H

#include "documentcontroller.h"
#include "rowsmodel.h"

#include "../uicore/problemlist.h"

#include <QColor>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <functional>
#include <map>
#include <optional>

class LayoutController;
class ShapesModel;

/* The Puzzle workspace (C16, C17; contract section 10): which problem and
 * which piece are selected, what its cards list, and every change to a
 * problem. What a change does to the puzzle is btui::problemlist's; here it
 * becomes one undo step, and a warning first when it would cost a problem
 * its solutions.
 *
 * The lists are RowsModels, worked out together after each change, while
 * the workspace shows: an edit in Entities only marks them stale.
 */
class PuzzleController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("owned by App")

  // the problem selected, -1 in a file without problems
  Q_PROPERTY(int problem READ problem WRITE selectProblem NOTIFY problemChanged FINAL)
  Q_PROPERTY(QString problemIdText READ problemIdText NOTIFY stateChanged FINAL)
  Q_PROPERTY(QString problemLabel READ problemLabel NOTIFY stateChanged FINAL)
  // the 3D view's tag: the result by its id and colour
  Q_PROPERTY(QString resultIdText READ resultIdText NOTIFY stateChanged FINAL)
  Q_PROPERTY(QColor resultColor READ resultColor NOTIFY stateChanged FINAL)
  Q_PROPERTY(QColor resultTextColor READ resultTextColor NOTIFY stateChanged FINAL)
  Q_PROPERTY(bool canDeleteProblem READ canDeleteProblem NOTIFY stateChanged FINAL)
  // the 3D view's name for assistive tech: what the scene holds
  Q_PROPERTY(QString sceneName READ sceneName NOTIFY stateChanged FINAL)
  // the shape selected in the lists, -1 for none: it need not be in the problem
  Q_PROPERTY(int selectedPiece READ selectedPiece WRITE selectPiece NOTIFY selectedPieceChanged FINAL)

  Q_PROPERTY(RowsModel * problems READ problems CONSTANT FINAL)
  Q_PROPERTY(RowsModel * goal READ goal CONSTANT FINAL)
  Q_PROPERTY(RowsModel * pieces READ pieces CONSTANT FINAL)
  Q_PROPERTY(RowsModel * library READ library CONSTANT FINAL)
  Q_PROPERTY(RowsModel * groups READ groups CONSTANT FINAL)
  Q_PROPERTY(RowsModel * pieceGroups READ pieceGroups CONSTANT FINAL)
  Q_PROPERTY(RowsModel * sections READ sections CONSTANT FINAL)
  Q_PROPERTY(RowsModel * colours READ colours CONSTANT FINAL)
  Q_PROPERTY(RowsModel * rules READ rules CONSTANT FINAL)

  // the session's view choices (overview/03)
  Q_PROPERTY(QString piecesView READ piecesView WRITE setPiecesView NOTIFY viewChanged FINAL)
  Q_PROPERTY(bool allPiecesOpen READ allPiecesOpen WRITE setAllPiecesOpen NOTIFY viewChanged FINAL)

  // the selected piece's card
  Q_PROPERTY(bool pieceShown READ pieceShown NOTIFY stateChanged FINAL)
  Q_PROPERTY(QString pieceIdText READ pieceIdText NOTIFY stateChanged FINAL)
  Q_PROPERTY(QString pieceName READ pieceName NOTIFY stateChanged FINAL)
  Q_PROPERTY(QString pieceDetail READ pieceDetail NOTIFY stateChanged FINAL)
  Q_PROPERTY(QColor pieceColor READ pieceColor NOTIFY stateChanged FINAL)
  Q_PROPERTY(QColor pieceTextColor READ pieceTextColor NOTIFY stateChanged FINAL)
  Q_PROPERTY(int pieceMin READ pieceMin NOTIFY stateChanged FINAL)
  Q_PROPERTY(int pieceMax READ pieceMax NOTIFY stateChanged FINAL)
  Q_PROPERTY(bool pieceRange READ pieceRange NOTIFY stateChanged FINAL)
  Q_PROPERTY(int maxCount READ maxCount CONSTANT FINAL)

  // the summary strip
  Q_PROPERTY(bool fits READ fits NOTIFY stateChanged FINAL)
  Q_PROPERTY(QString summaryTitle READ summaryTitle NOTIFY stateChanged FINAL)
  Q_PROPERTY(QString summaryDetail READ summaryDetail NOTIFY stateChanged FINAL)
  Q_PROPERTY(QString holeLimitText READ holeLimitText NOTIFY stateChanged FINAL)

public:

  PuzzleController(DocumentController * doc, ShapesModel * shapes, LayoutController * layout, QObject * parent = nullptr);

  int problem(void) const { return m_problem; }
  QString problemIdText(void) const { return m_view.problemIdText; }
  QString problemLabel(void) const { return m_view.problemLabel; }
  QString resultIdText(void) const { return m_view.resultIdText; }
  QColor resultColor(void) const { return m_view.resultColor; }
  QColor resultTextColor(void) const { return m_view.resultTextColor; }
  bool canDeleteProblem(void) const { return m_view.canDeleteProblem; }
  QString sceneName(void) const { return m_view.sceneName; }
  int selectedPiece(void) const { return m_piece; }

  RowsModel * problems(void) const { return m_problems; }
  RowsModel * goal(void) const { return m_goal; }
  RowsModel * pieces(void) const { return m_pieces; }
  RowsModel * library(void) const { return m_library; }
  RowsModel * groups(void) const { return m_groups; }
  RowsModel * pieceGroups(void) const { return m_pieceGroups; }
  RowsModel * sections(void) const { return m_sections; }
  RowsModel * colours(void) const { return m_colours; }
  RowsModel * rules(void) const { return m_rules; }

  QString piecesView(void) const { return m_piecesView; }
  void setPiecesView(const QString & v);
  bool allPiecesOpen(void) const { return m_allPiecesOpen; }
  void setAllPiecesOpen(bool open);

  bool pieceShown(void) const { return m_view.pieceShown; }
  QString pieceIdText(void) const { return m_view.pieceIdText; }
  QString pieceName(void) const { return m_view.pieceName; }
  QString pieceDetail(void) const { return m_view.pieceDetail; }
  QColor pieceColor(void) const { return m_view.pieceColor; }
  QColor pieceTextColor(void) const { return m_view.pieceTextColor; }
  int pieceMin(void) const { return m_view.pieceMin; }
  int pieceMax(void) const { return m_view.pieceMax; }
  bool pieceRange(void) const { return m_view.pieceRange; }
  int maxCount(void) const;

  bool fits(void) const { return m_view.fits; }
  QString summaryTitle(void) const { return m_view.summaryTitle; }
  QString summaryDetail(void) const { return m_view.summaryDetail; }
  QString holeLimitText(void) const { return m_view.holeLimitText; }

  /* the status bar's sentence, legacy StatProblemInfo's (C16); empty
   * without a result */
  QString statusText(void) const;

  // --- selection ---------------------------------------------------------

  /* clamped to the list; -1 only when there are no problems */
  Q_INVOKABLE void selectProblem(int row);
  Q_INVOKABLE void selectProblemBy(int delta);
  /* -1 selects nothing */
  Q_INVOKABLE void selectPiece(int shape);

  // --- the problems list -------------------------------------------------

  Q_INVOKABLE void newProblem(void);
  Q_INVOKABLE void duplicateProblem(int row);
  /* asks first when the problem holds solutions */
  Q_INVOKABLE void deleteProblem(int row);
  Q_INVOKABLE void moveProblem(int from, int to);
  Q_INVOKABLE void renameProblem(int row, const QString & label);

  // --- the selected problem ----------------------------------------------
  // Each is one undo step, and removes the problem's solutions: the session's
  // one warning comes first (DocumentController::mayCostSolutions).

  Q_INVOKABLE void setResult(int shape);
  /* + / − on a tile, one more from the library, one fewer in a menu */
  Q_INVOKABLE void stepPiece(int shape, int delta);
  /* a fixed count; 0 removes the piece */
  Q_INVOKABLE void setCount(int shape, int count);
  Q_INVOKABLE void setRange(int shape, int min, int max);
  /* One bound of a range, the other kept on its side of it. While a number
   * is typed (holdCounts ... releaseCounts) the other bound is the one the
   * typing began with: the 1 on the way to 15 must not lower Min for good. */
  Q_INVOKABLE void setPieceMin(int shape, int min);
  Q_INVOKABLE void setPieceMax(int shape, int max);
  Q_INVOKABLE void holdCounts(int shape);
  Q_INVOKABLE void releaseCounts(void);
  /* the switch: on offers Min and Max, starting at 0 ... the count; off is
   * a fixed count of the maximum */
  Q_INVOKABLE void setRangeOn(int shape, bool on);
  Q_INVOKABLE void setRangeForAll(int min, int max);
  Q_INVOKABLE void addOneOfEach(void);
  Q_INVOKABLE void removeAllPieces(void);

  /* how many copies of the shape may be in the group */
  Q_INVOKABLE void setGroupCount(int shape, int group, int count);
  /* a new group, holding all of the shape's copies */
  Q_INVOKABLE void newGroup(int shape);
  /* how many copies may be in the group, how many the piece has */
  Q_INVOKABLE int pieceGroupCount(int shape, int group) const;
  Q_INVOKABLE int copiesOf(int shape) const;
  Q_INVOKABLE void deleteGroup(int group);

  /* colours by id, as the rows of `rules` give them */
  Q_INVOKABLE void toggleRule(int pieceColour, int resultColour);
  /* 0 each on itself, 1 allow all, 2 clear */
  Q_INVOKABLE void setRulePreset(int preset);

  Q_INVOKABLE void clearHoleLimit(void);

  // --- the right card's sections ------------------------------------------

  Q_INVOKABLE void setSectionOpen(const QString & key, bool open);
  /* from the collapsed rail: the card opens at this section */
  Q_INVOKABLE void showSection(const QString & key);

  /* What is being typed goes in, before anything moves under it (a field
   * answers typingEnding). */
  Q_INVOKABLE void flushTyping(void);

  /* The lists, now: the workspace does it when it shows them. */
  void refresh(void);

signals:

  void problemChanged(void);
  void selectedPieceChanged(void);
  void stateChanged(void);
  void viewChanged(void);

  /* the card should bring this section into view */
  void sectionRequested(const QString & key);
  void renameRequested(int row);
  /* a message for the status bar (App flashes it) */
  void flashRequested(const QString & text);
  /* a change the screen shows, for assistive tech to say */
  void announceRequested(const QString & text);
  /* a field holding typed text commits it (keep) or drops it */
  void typingEnding(bool keep);

private:

  /* what the cards show besides the lists */
  struct View {
    QString problemIdText, problemLabel, resultIdText, sceneName;
    QColor resultColor, resultTextColor;
    bool canDeleteProblem = false;
    bool pieceShown = false;
    QString pieceIdText, pieceName, pieceDetail;
    QColor pieceColor, pieceTextColor;
    int pieceMin = 0, pieceMax = 0;
    bool pieceRange = false;
    bool fits = false;
    QString summaryTitle, summaryDetail, holeLimitText;
    bool operator==(const View &) const = default;
  };

  using Edit = std::function<bool(puzzle_c &)>;

  /* The selection around one undo step: an undo goes back to what it finds
   * here, a redo to what it leaves. */
  class Step {
    public:
      explicit Step(PuzzleController * c, unsigned int coalesceKey = 0);
      ~Step(void);
      Step(const Step &) = delete;
      Step & operator=(const Step &) = delete;
    private:
      PuzzleController * m_c;
  };

  /* one undo step; f(puzzle_c &) says whether it changed anything */
  bool apply(const Edit & f, puzzleHistory_c::actionKind_e kind = puzzleHistory_c::AK_PROBLEM_STRUCTURAL);
  /* the same for a change to what the selected problem asks for: its
   * solutions go with it. A warning comes first only for a change that would
   * do something. */
  void change(const Edit & f);
  /* ... of one piece's count: a burst of them is one undo step */
  void changeCount(int shape, const Edit & f);
  void changeAs(const Edit & f, puzzleHistory_c::actionKind_e kind, unsigned int coalesceKey);
  bool wouldCostSolutions(int problem, const Edit & f) const;
  btui::PieceCount counts(int shape) const;
  void followShapes(int from, int to);
  void stale(void);
  void select(int problem);

  QString voxelsText(int n) const;
  QString rangeText(long long min, long long max) const;
  QString sectionTitle(int section) const;
  QString sectionKey(int section) const;

  DocumentController * m_doc;
  ShapesModel * m_shapes;
  LayoutController * m_layout;

  RowsModel * m_problems;
  RowsModel * m_goal;
  RowsModel * m_pieces;
  RowsModel * m_library;
  RowsModel * m_groups;
  RowsModel * m_pieceGroups;
  RowsModel * m_sections;
  RowsModel * m_colours;
  RowsModel * m_rules;

  int m_problem = -1;
  int m_problemShown = -1;         ///< the problem QML last heard of (problemChanged)
  int m_piece = -1;
  /* the shape whose Min and Max stay on show though they are equal: the
   * switch was turned on, and stepping must not take them away */
  int m_rangeHeld = -1;
  /* the counts a typed number started from (holdCounts) */
  struct HeldCounts {
    int shape;
    btui::PieceCount counts;
  };
  std::optional<HeldCounts> m_heldCounts;
  QString m_piecesView = QStringLiteral("chips");
  bool m_allPiecesOpen = true;
  std::map<QString, bool> m_sectionOpen;
  bool m_stale = true;
  View m_view;
};

#endif
