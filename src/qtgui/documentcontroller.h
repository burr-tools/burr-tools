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
#ifndef BTQT_DOCUMENTCONTROLLER_H
#define BTQT_DOCUMENTCONTROLLER_H

#include "../uicore/documentsession.h"

#include <QObject>
#include <QString>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

#include <functional>
#include <utility>
#include <vector>

/* The document as the GUI sees it: file name, modified state, undo/redo,
 * and the New / Open / Import / Save / Save as / Quit flows.
 *
 * Every question for the user is asked by QML with an OS-native dialog. The
 * controller asks for one by emitting a ...Requested() signal and carries on
 * when QML calls back -- resolveDiscard(), openFile(), saveAsFile(),
 * newDocument() -- so a flow such as "New with unsaved changes -> Save ->
 * pick a name -> choose the voxel type" is a short state machine here, and
 * tests drive it by calling the same continuation methods.
 */
class DocumentController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("owned by App")

  Q_PROPERTY(QString fileName READ fileName NOTIFY fileChanged FINAL)
  Q_PROPERTY(QString filePath READ filePath NOTIFY fileChanged FINAL)
  Q_PROPERTY(QString windowTitle READ windowTitle NOTIFY stateChanged FINAL)
  Q_PROPERTY(QUrl folder READ folder NOTIFY fileChanged FINAL)
  Q_PROPERTY(bool modified READ modified NOTIFY stateChanged FINAL)
  Q_PROPERTY(bool canUndo READ canUndo NOTIFY stateChanged FINAL)
  Q_PROPERTY(bool canRedo READ canRedo NOTIFY stateChanged FINAL)
  Q_PROPERTY(int gridType READ gridType NOTIFY documentReplaced FINAL)
  Q_PROPERTY(QString gridTypeName READ gridTypeName NOTIFY documentReplaced FINAL)
  Q_PROPERTY(QString comment READ comment NOTIFY stateChanged FINAL)
  Q_PROPERTY(bool flowPending READ flowPending NOTIFY stateChanged FINAL)
  // the warning that edits remove solutions has been answered, this session
  Q_PROPERTY(bool solutionsWarned READ solutionsWarned WRITE setSolutionsWarned NOTIFY solutionsWarnedChanged FINAL)
  // a solve or an analysis reads the puzzle: nothing may change or replace it
  Q_PROPERTY(bool busy READ busy NOTIFY busyChanged FINAL)

public:

  explicit DocumentController(QObject * parent = nullptr);

  btui::DocumentSession & session(void) { return m_session; }
  const btui::DocumentSession & session(void) const { return m_session; }

  QString fileName(void) const;
  QString filePath(void) const;
  QString windowTitle(void) const;
  QUrl folder(void) const;
  bool modified(void) const { return m_session.isModified(); }
  bool canUndo(void) const { return m_session.canUndo(); }
  bool canRedo(void) const { return m_session.canRedo(); }
  int gridType(void) const;
  QString gridTypeName(void) const;
  QString comment(void) const;
  bool flowPending(void) const { return m_pending != Pending::None; }
  bool solutionsWarned(void) const { return m_solutionsWarned; }
  void setSolutionsWarned(bool warned);

  /* While a solve or an analysis reads the puzzle (SolverController), every
   * path that would change or replace it refuses -- the edits, strokes,
   * undo and redo, New, Open, Import, Convert, Save -- as legacy's
   * threadStopped() does, with one message (busyRefused). Solver results
   * change it through session().editWithoutUndo(), which is not gated. */
  bool busy(void) const { return m_busy; }
  void setBusy(bool busy);
  /* true, and the message, while busy: the caller does nothing */
  bool refuseWhileBusy(void);
  /* what stops the work that makes the document busy, and waits for it
   * (SolverController::stopAndWait): Quit runs it before it asks about
   * unsaved changes, so a Save there writes a puzzle nothing else writes */
  void setBusyStopper(std::function<void()> stop) { m_stopBusy = std::move(stop); }

  /* Any edit removes the solutions it makes wrong, and they leave the file
   * with the next save. They can be found again, so one warning a session
   * says so, and from then on edits just go ahead. An edit asks here first,
   * saying whether it costs solutions: true means go on now; false means the
   * warning is up and `retry` runs if the user goes on. */
  bool mayCostSolutions(bool costs, std::function<void()> retry);
  /* A voxel edit of these shapes asks here instead (editVoxels), saying
   * whether it drops solutions. Its undo step keeps them, so it goes on
   * without a word -- unless they are too big for the history's memory
   * budget (DocumentSession::keepsSolutionsOf): then they would be lost for
   * good, and the warning says so every time. */
  bool mayDropSolutions(bool drops, const std::vector<unsigned int> & shapes, std::function<void()> retry);
  /* The same for a change to what these problems ask for (editProblems). */
  bool mayDropProblemSolutions(bool drops, const std::vector<unsigned int> & problems, std::function<void()> retry);
  /* the answer to solutionsWarningRequested() */
  Q_INVOKABLE void resolveSolutionsWarning(bool goOn);

  /* The official voxel type names in the File ▸ New order (spec C14). The
   * legacy dialog's "Tetrahedra-Octahera" is corrected.
   */
  Q_INVOKABLE static QString gridTypeDisplayName(int type);

  // --- starting a flow (menu commands) -----------------------------------

  Q_INVOKABLE void requestNew(void);
  Q_INVOKABLE void requestOpen(void);
  Q_INVOKABLE void requestImport(void);
  Q_INVOKABLE void requestQuit(void);
  /* open a file named by the OS or the command line; still asks about
   * unsaved changes first */
  Q_INVOKABLE void requestOpenPath(const QString & path);

  Q_INVOKABLE void save(void);
  Q_INVOKABLE void requestSaveAs(void);

  Q_INVOKABLE void undo(void);
  Q_INVOKABLE void redo(void);

  Q_INVOKABLE void setComment(const QString & text);

  /* Convert to another grid type (Convert dialog). The converted puzzle
   * replaces this one; false, with a message, when the conversion fails.
   */
  Q_INVOKABLE bool convertTo(int gridType);

  // --- continuations (QML reports the user's answer) ---------------------

  enum DiscardChoice { Cancel = 0, Save = 1, Discard = 2 };
  Q_ENUM(DiscardChoice)

  Q_INVOKABLE void resolveDiscard(int choice);
  Q_INVOKABLE void newDocument(int gridType);
  Q_INVOKABLE void openFile(const QUrl & file);
  Q_INVOKABLE void importFile(const QUrl & file);
  Q_INVOKABLE void saveAsFile(const QUrl & file);
  /* the user closed a file or type dialog without choosing */
  Q_INVOKABLE void cancelFlow(void);

  /* Load directly, without the unsaved-changes question: the command line
   * at start-up. Returns whether it loaded.
   */
  bool loadPath(const QString & path);

  /* The puzzle files named on the command line, opened by
   * loadStartupFiles(): Main.qml calls it after the window's first frame,
   * before it makes the workspace. Like legacy, the first that loads wins;
   * the others are not tried. */
  void setStartupFiles(const QStringList & files);
  Q_INVOKABLE void loadStartupFiles(void);
  bool startupFilesPending(void) const { return !m_startupFiles.isEmpty(); }
  bool startupFileLoaded(void) const { return m_startupLoaded; }

  /* A dialog changes the puzzle's shapes or problems in place: f(puzzle_c &)
   * makes the change and returns whether it changed anything. A change is
   * one undo step (legacy AK_ENTITIES_STRUCTURAL), and the views are told;
   * no change, neither. If f throws, the step is recorded and the views
   * told all the same (DocumentSession::edit), and the exception goes on.
   * -1 keeps the current shape selection where possible.
   */
  template <class F> bool editStructure(int selectedShape, F && f) {
    return edit(puzzleHistory_c::AK_ENTITIES_STRUCTURAL, selectedShape, std::forward<F>(f));
  }

  /* The solver's results change the puzzle (SolverController): a finished
   * run, removed solutions, deleted or analysed disassemblies. Not an undo
   * step and not gated by busy -- the solver is what makes it busy -- but
   * unsaved, and the views that count solutions are told (resultsChanged). */
  template <class F> void editResults(F && f) {
    m_session.editWithoutUndo(std::forward<F>(f));
    emit resultsChanged();
    emit stateChanged();
  }

  /* What an edit changed, so that each view refreshes only what shows it
   * (puzzleEdited): a rename does not rebuild the 3D view's mesh. */
  enum class Change {
    Structure,    ///< shapes or problems added, removed, reordered; anything else
    Voxels,       ///< the voxels or the size of a shape (shape -1: of several)
    Label,        ///< a shape's name or weight
    Palette,      ///< the colour list
    Problem,      ///< what a problem holds, no shape's voxels (the 3D view of Entities stays)
  };
  Q_ENUM(Change)

  /* the change an undo step of a kind makes */
  static Change changeOf(puzzleHistory_c::actionKind_e kind);

  /* The same as one undo step of another kind: the colour list
   * (AK_COLOR_PALETTE), the voxel editor's grid size. The change is the
   * kind's (changeOf); the second form names it, and the shape it is to. */
  template <class F> bool edit(puzzleHistory_c::actionKind_e kind, int selectedShape, F && f) {
    return edit(kind, changeOf(kind), selectedShape, selectedShape, std::forward<F>(f));
  }

  template <class F> bool edit(puzzleHistory_c::actionKind_e kind, Change change, int shape, int selectedShape, F && f) {
    return tellEdit(change, shape, selectedShape, [&] {
      return m_session.edit(kind, sessionShape(selectedShape), std::forward<F>(f));
    });
  }

  /* An edit of the voxels of shapes (DocumentSession::editVoxels): as
   * edit(), and the problems using a shape it changed lose their solutions
   * -- to the undo step, which brings them back. shape: the one changed,
   * -1 for several. */
  template <class F> bool editVoxels(puzzleHistory_c::actionKind_e kind, int shape, int selectedShape,
                                     const std::vector<unsigned int> & shapes, F && f) {
    return tellEdit(Change::Voxels, shape, selectedShape, [&] {
      return m_session.editVoxels(kind, sessionShape(selectedShape), shapes, std::forward<F>(f));
    });
  }

  /* A change to what problems ask for (DocumentSession::editProblems):
   * as edit(), and those problems lose their solutions -- to the undo
   * step, which brings them back. */
  template <class F> bool editProblems(puzzleHistory_c::actionKind_e kind, int selectedShape,
                                       const std::vector<unsigned int> & problems, F && f) {
    return tellEdit(Change::Problem, -1, selectedShape, [&] {
      return m_session.editProblems(kind, sessionShape(selectedShape), problems, std::forward<F>(f));
    });
  }

private:

  static unsigned int sessionShape(int shape) { return shape < 0 ? puzzleHistory_c::NO_SHAPE : unsigned(shape); }

  /* run an edit of the session (g), and tell the views what it changed */
  template <class G> bool tellEdit(Change change, int shape, int selectedShape, G && g) {
    if (refuseWhileBusy())
      return false;
    const bool wasStroke = m_session.inStroke();
    bool changed;
    try {
      changed = g();
    } catch (...) {
      if (wasStroke)
        emit strokeInterrupted();
      edited(selectedShape, change, shape);
      throw;
    }
    if (wasStroke)
      emit strokeInterrupted();  // the drag goes on, but its stroke is over
    if (changed)
      edited(selectedShape, change, shape);
    else if (wasStroke)
      emit stateChanged();       // the stroke it ended may be a step
    return changed;
  }

public:

  /* The Puzzle workspace's selection around the next undo step
   * (puzzleHistory_c::beginStep, endStep), and where the undo or redo just
   * applied puts it (historyApplied). */
  void beginStep(int problem, int piece, unsigned int coalesceKey);
  void endStep(int problem, int piece);
  puzzleHistory_c::selection_t restoredSelection(void) const { return m_restored; }

  /* A stroke of the voxel editor (DocumentSession::beginStroke): each
   * strokeEdit() is shown at once (puzzleEdited, Voxels), the undo step is
   * taken at endStroke(). The stroke's first change makes the document
   * unsaved and undoable at once (stateChanged), not at the release. */
  void beginStroke(puzzleHistory_c::actionKind_e kind, int shape);
  template <class F> bool strokeEdit(int shape, F && f) {
    if (refuseWhileBusy())
      return false;
    const bool clean = !m_session.strokeChanged();
    bool changed;
    try {
      changed = m_session.strokeEdit(std::forward<F>(f));
    } catch (...) {
      emit puzzleEdited(shape, Change::Voxels, shape);
      if (clean && m_session.strokeChanged())
        emit stateChanged();
      throw;
    }
    if (changed)
      emit puzzleEdited(shape, Change::Voxels, shape);
    if (clean && m_session.strokeChanged())
      emit stateChanged();       // the title's *, Undo enabled
    return changed;
  }
  void endStroke(void);

signals:

  void fileChanged(void);
  void stateChanged(void);
  /* a different puzzle object: everything showing the old one must reset */
  void documentReplaced(void);
  /* undo/redo changed the puzzle; tab and shape say where (puzzleHistory_c) */
  void historyApplied(int tab, int selectedShape);
  /* An open stroke was ended by something else than endStroke(): an undo,
   * a redo or another edit while the button is still down. What it painted
   * is a step of its own; the drag that made it must paint no more. */
  void strokeInterrupted(void);
  /* an undo or redo is about to apply: what is being typed goes in first */
  void aboutToApplyHistory();
  /* Shapes or problems changed in place (not undo/redo): what changed, and
   * to which shape (-1: several, or none). selectedShape is the selection
   * the change asks for; -1 = no preference. */
  void puzzleEdited(int selectedShape, DocumentController::Change change, int shape);

  void solutionsWarnedChanged(void);
  void busyChanged(void);
  /* editResults(): a problem's solutions or solve state changed */
  void resultsChanged(void);
  /* a change was refused while busy: the status bar says why */
  void busyRefused(const QString & text);
  /* an edit would cost solutions, the first this session; or tooBigToKeep,
   * a voxel edit whose solutions the undo history has no room for: QML
   * warns, then resolveSolutionsWarning() */
  void solutionsWarningRequested(bool tooBigToKeep);

  void confirmDiscardRequested(const QString & action);
  void newFileTypeRequested(void);
  void openFileRequested(void);
  void importFileRequested(void);
  void saveAsRequested(void);
  void quitApproved(void);
  void messageRequested(const QString & title, const QString & text);
  /* loadStartupFiles() ran (whether a file loaded or not) */
  void startupFilesLoaded(void);

private:

  enum class Pending { None, New, Open, Import, Quit, OpenPath };

  void start(Pending p, const char * action);
  void proceed(void);
  /* edit() changed the puzzle: the views repaint */
  void edited(int selectedShape, Change change, int shape);
  void finishLoad(const btui::DocumentSession::LoadResult & r, const QString & path);
  void setPending(Pending p);
  /* the edit waits for the warning (mayCostSolutions, mayDropSolutions, mayDropProblemSolutions) */
  bool holdForWarning(std::function<void()> retry, bool tooBigToKeep);

  btui::DocumentSession m_session;
  Pending m_pending = Pending::None;
  bool m_busy = false;
  std::function<void()> m_stopBusy;
  bool m_solutionsWarned = false;
  std::function<void()> m_heldEdit;      ///< the edit the solutions warning holds
  bool m_heldTooBig = false;             ///< ... warned of as too big to keep (mayDropSolutions)
  bool m_dropAnswered = false;           ///< that warning's held edit is running: it goes ahead
  puzzleHistory_c::selection_t m_restored; ///< what the last undo or redo selected
  QString m_pendingPath;
  bool m_continueAfterSaveAs = false;
  QStringList m_startupFiles;
  bool m_startupLoaded = false;
};

#endif
