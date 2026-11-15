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
#ifndef BTQT_SOLVERCONTROLLER_H
#define BTQT_SOLVERCONTROLLER_H

#include "documentcontroller.h"
#include "rowsmodel.h"

#include "../uicore/solutionscene.h"
#include "../uicore/solver.h"

#include <QColor>
#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class LayoutController;
class PuzzleController;
class SettingsController;
class assembly_c;
class disasmToMoves_c;
class problem_c;
class separation_c;
class solveThread_c;

/* The Solver workspace (C19, C20, C21): its problem and search options, the
 * one solver thread and its progress, the problem's solutions as the list and
 * the slider show them, the disassembly player, the piece display, Manage
 * (remove, delete and analyse disassemblies) and the 3D scene of the
 * selected solution at the player's position. Its rules are btui::solver's;
 * the answers legacy gives to the spec's open questions are in
 * design/2026-11-13-qtgui-solver-workspace.md.
 *
 * Like legacy mainWindow_c::update() it polls the thread on a timer: the
 * thread fills the problem's solutions under its lock, and this reads them
 * only under that lock. While a solve or an analysis runs the document is
 * busy (DocumentController::setBusy): nothing may change or replace the
 * puzzle. Results change it through editWithoutUndo -- not an undo step,
 * only unsaved (overview/03).
 */
class SolverController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("owned by App")

public:

  enum RunStateId { Idle, Preparing, Ready, Running, Stopping, Paused, Finished };
  Q_ENUM(RunStateId)
  /* what the list's area shows instead of rows (C20 empty states) */
  enum EmptyState { NotEmpty, Searching, NoAssemblies, NoneDisassemblable, NeverRun, JustCounted, NoProblem,
                    NotFinished,   ///< prepared or stopped before the first assembly
                    NoneKept };    ///< assemblies found, none kept (removed, or Keep every)
  Q_ENUM(EmptyState)
  /* what the player shows (C20 alternate states) */
  enum PlayerState { Playable, NothingToPlay, NotChecked, LevelOnly, NoneStored, CannotDisassemble,
                     NoDisassembler };   ///< the grid cannot take pieces apart at all
  Q_ENUM(PlayerState)

private:

  // --- the problem (C19 §1) --------------------------------------------------
  Q_PROPERTY(int problem READ problem WRITE selectProblem NOTIFY problemChanged FINAL)
  Q_PROPERTY(QString problemIdText READ problemIdText NOTIFY problemChanged FINAL)
  Q_PROPERTY(QString problemLabel READ problemLabel NOTIFY problemChanged FINAL)
  Q_PROPERTY(QString problemDetail READ problemDetail NOTIFY problemChanged FINAL)

  // --- what to check and the search options (C19 §2, §3) ----------------------
  Q_PROPERTY(bool disassemble READ disassemble WRITE setDisassemble NOTIFY optionsChanged FINAL)
  Q_PROPERTY(bool dropDisassemblies READ dropDisassemblies WRITE setDropDisassemblies NOTIFY optionsChanged FINAL)
  Q_PROPERTY(bool justCount READ justCount WRITE setJustCount NOTIFY optionsChanged FINAL)
  Q_PROPERTY(bool keepMirror READ keepMirror WRITE setKeepMirror NOTIFY optionsChanged FINAL)
  Q_PROPERTY(bool keepRotated READ keepRotated WRITE setKeepRotated NOTIFY optionsChanged FINAL)
  Q_PROPERTY(bool thoroughRotation READ thoroughRotation WRITE setThoroughRotation NOTIFY optionsChanged FINAL)
  Q_PROPERTY(int searchSort READ searchSort WRITE setSearchSort NOTIFY optionsChanged FINAL)
  Q_PROPERTY(int keepEvery READ keepEvery WRITE setKeepEvery NOTIFY optionsChanged FINAL)
  Q_PROPERTY(int limit READ limit WRITE setLimit NOTIFY optionsChanged FINAL)
  Q_PROPERTY(int maxCount READ maxCount CONSTANT FINAL)
  Q_PROPERTY(int changedOptions READ changedOptions NOTIFY optionsChanged FINAL)
  Q_PROPERTY(bool optionsOpen READ optionsOpen WRITE setOptionsOpen NOTIFY optionsChanged FINAL)
  // the grid can take pieces apart (legacy CAP_DISASSEMBLE): the radio card's
  Q_PROPERTY(bool canDisassemble READ canDisassemble NOTIFY problemChanged FINAL)

  // --- the run (C19 §4) ----------------------------------------------------------
  Q_PROPERTY(int runState READ runState NOTIFY runChanged FINAL)
  Q_PROPERTY(bool solving READ solving NOTIFY runChanged FINAL)
  Q_PROPERTY(bool canPrepare READ canPrepare NOTIFY runChanged FINAL)
  Q_PROPERTY(bool canStart READ canStart NOTIFY runChanged FINAL)
  Q_PROPERTY(bool canContinue READ canContinue NOTIFY runChanged FINAL)
  Q_PROPERTY(bool canStop READ canStop NOTIFY runChanged FINAL)
  Q_PROPERTY(double progress READ progress NOTIFY runChanged FINAL)
  Q_PROPERTY(QString progressText READ progressText NOTIFY runChanged FINAL)
  Q_PROPERTY(QString activityText READ activityText NOTIFY runChanged FINAL)
  Q_PROPERTY(QString assembliesText READ assembliesText NOTIFY runChanged FINAL)
  Q_PROPERTY(QString solutionsText READ solutionsText NOTIFY runChanged FINAL)
  Q_PROPERTY(QString timeUsedText READ timeUsedText NOTIFY runChanged FINAL)
  Q_PROPERTY(QString timeLeftText READ timeLeftText NOTIFY runChanged FINAL)

  // --- the step-by-step explorer (C19 §5) ------------------------------------------
  Q_PROPERTY(bool canPlacements READ canPlacements NOTIFY runChanged FINAL)
  Q_PROPERTY(bool canMovements READ canMovements NOTIFY selectionChanged FINAL)
  Q_PROPERTY(bool canStep READ canStep NOTIFY runChanged FINAL)
  // the docked drawer (C21): "", "placements" or "movements"
  Q_PROPERTY(QString explorer READ explorer NOTIFY explorerChanged FINAL)
  // Placements: each piece, how many places the search has for it, placed now
  Q_PROPERTY(RowsModel * placements READ placements CONSTANT FINAL)
  Q_PROPERTY(bool assemblerStateShown READ assemblerStateShown NOTIFY sceneChanged FINAL)
  Q_PROPERTY(QString assemblerCaption READ assemblerCaption NOTIFY sceneChanged FINAL)

  // --- the solutions (C20) ------------------------------------------------------------
  Q_PROPERTY(RowsModel * solutions READ solutions CONSTANT FINAL)
  Q_PROPERTY(int shownCount READ shownCount NOTIFY solutionsChanged FINAL)
  Q_PROPERTY(int savedCount READ savedCount NOTIFY solutionsChanged FINAL)
  Q_PROPERTY(int emptyState READ emptyState NOTIFY solutionsChanged FINAL)
  Q_PROPERTY(QString emptyTitle READ emptyTitle NOTIFY solutionsChanged FINAL)
  Q_PROPERTY(QString emptyText READ emptyText NOTIFY solutionsChanged FINAL)
  Q_PROPERTY(QString view READ view WRITE setView NOTIFY viewChanged FINAL)
  Q_PROPERTY(int sortKey READ sortKey NOTIFY solutionsChanged FINAL)
  Q_PROPERTY(bool sortDescending READ sortDescending NOTIFY solutionsChanged FINAL)
  Q_PROPERTY(bool levelSortable READ levelSortable NOTIFY solutionsChanged FINAL)
  Q_PROPERTY(int selectedRow READ selectedRow WRITE selectRow NOTIFY selectionChanged FINAL)
  Q_PROPERTY(int selectedNumber READ selectedNumber NOTIFY selectionChanged FINAL)
  Q_PROPERTY(QString selectedLevel READ selectedLevel NOTIFY selectionChanged FINAL)
  Q_PROPERTY(QString selectedMoves READ selectedMoves NOTIFY selectionChanged FINAL)
  Q_PROPERTY(int selectedPieces READ selectedPieces NOTIFY selectionChanged FINAL)
  Q_PROPERTY(int selectedDa READ selectedDa NOTIFY selectionChanged FINAL)

  // --- the player (C20) ---------------------------------------------------------------
  Q_PROPERTY(int playerState READ playerState NOTIFY selectionChanged FINAL)
  Q_PROPERTY(double totalMoves READ totalMoves NOTIFY selectionChanged FINAL)
  Q_PROPERTY(double position READ position WRITE seek NOTIFY positionChanged FINAL)
  Q_PROPERTY(QString moveText READ moveText NOTIFY positionChanged FINAL)
  Q_PROPERTY(bool playing READ playing NOTIFY positionChanged FINAL)
  Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY viewChanged FINAL)

  // --- Manage (C20) -------------------------------------------------------------------
  Q_PROPERTY(bool canManage READ canManage NOTIFY runChanged FINAL)
  Q_PROPERTY(bool canAnalyse READ canAnalyse NOTIFY runChanged FINAL)
  Q_PROPERTY(bool analysing READ analysing NOTIFY runChanged FINAL)
  Q_PROPERTY(QString analysingText READ analysingText NOTIFY runChanged FINAL)
  Q_PROPERTY(int previewRemove READ previewRemove WRITE setPreviewRemove NOTIFY solutionsChanged FINAL)

  // --- the piece display (C20) and the scene (C21) ------------------------------------
  Q_PROPERTY(RowsModel * pieces READ pieces CONSTANT FINAL)
  Q_PROPERTY(bool xray READ xray WRITE setXray NOTIFY sceneChanged FINAL)
  Q_PROPERTY(QString sceneMessage READ sceneMessage NOTIFY sceneChanged FINAL)
  Q_PROPERTY(QString tagText READ tagText NOTIFY selectionChanged FINAL)

public:

  SolverController(SettingsController * settings, DocumentController * doc, LayoutController * layout,
                   PuzzleController * puzzle, QObject * parent = nullptr);
  ~SolverController() override;

  int problem(void) const { return m_problem; }
  QString problemIdText(void) const;
  QString problemLabel(void) const;
  QString problemDetail(void) const;

  bool disassemble(void) const { return m_options.disassemble; }
  void setDisassemble(bool v);
  bool dropDisassemblies(void) const { return m_options.dropDisassemblies; }
  void setDropDisassemblies(bool v);
  bool justCount(void) const { return m_options.justCount; }
  void setJustCount(bool v);
  bool keepMirror(void) const { return m_options.keepMirror; }
  void setKeepMirror(bool v);
  bool keepRotated(void) const { return m_options.keepRotated; }
  void setKeepRotated(bool v);
  bool thoroughRotation(void) const { return m_options.thoroughRotation; }
  void setThoroughRotation(bool v);
  int searchSort(void) const { return int(m_options.sort); }
  void setSearchSort(int v);
  int keepEvery(void) const { return m_options.keepEvery; }
  void setKeepEvery(int v);
  int limit(void) const { return m_options.limit; }
  void setLimit(int v);
  int maxCount(void) const { return btui::maxSolverCount; }
  int changedOptions(void) const { return btui::changedOptions(m_options); }
  bool optionsOpen(void) const { return m_optionsOpen; }
  void setOptionsOpen(bool v);
  bool canDisassemble(void) const;
  /* a sort-by choice is offered (Level and Moves need the disassembly) */
  Q_INVOKABLE bool searchSortAvailable(int sort) const;
  /* back to the defaults, the What to check choice kept */
  Q_INVOKABLE void resetOptions(void);

  int runState(void) const { return int(btui::runState(m_facts)); }
  bool solving(void) const { return bool(m_thread); }
  bool canPrepare(void) const { return m_buttons.prepare; }
  bool canStart(void) const { return m_buttons.start; }
  bool canContinue(void) const { return m_buttons.cont; }
  bool canStop(void) const { return m_buttons.stop; }
  double progress(void) const { return m_progress; }
  QString progressText(void) const;
  QString activityText(void) const { return m_activityText; }
  QString assembliesText(void) const { return m_assembliesText; }
  QString solutionsText(void) const { return m_solutionsText; }
  QString timeUsedText(void) const { return m_timeUsedText; }
  QString timeLeftText(void) const { return m_timeLeftText; }

  bool canPlacements(void) const { return m_buttons.placements; }
  bool canMovements(void) const;
  bool canStep(void) const { return m_buttons.step; }
  bool assemblerStateShown(void) const { return m_assemblerState != nullptr; }
  QString explorer(void) const { return m_explorer; }
  RowsModel * placements(void) const { return m_placements; }
  /* a Placements or Movements button: opens its drawer, or shuts it */
  Q_INVOKABLE void toggleExplorer(const QString & kind);
  Q_INVOKABLE void closeExplorer(void);
  QString assemblerCaption(void) const { return m_assemblerCaption; }

  RowsModel * solutions(void) const { return m_rows; }
  int shownCount(void) const { return int(m_list.shown().size()); }
  int savedCount(void) const { return int(m_list.all().size()); }
  int emptyState(void) const { return int(m_empty); }
  QString emptyTitle(void) const;
  QString emptyText(void) const;
  QString view(void) const { return m_view; }
  void setView(const QString & v);
  int sortKey(void) const { return int(m_sort.key); }
  bool sortDescending(void) const { return m_sort.descending; }
  bool levelSortable(void) const { return btui::listKeyAvailable(m_list.all(), btui::ListKey::Level); }
  int selectedRow(void) const { return m_row; }
  int selectedNumber(void) const;
  QString selectedLevel(void) const;
  QString selectedMoves(void) const;
  int selectedPieces(void) const;
  int selectedDa(void) const;

  int playerState(void) const;
  double totalMoves(void) const { return m_total; }
  double position(void) const { return m_position; }
  QString moveText(void) const;
  bool playing(void) const { return m_playing; }
  double speed(void) const { return m_speed; }
  void setSpeed(double v);

  bool canManage(void) const;
  bool canAnalyse(void) const;
  bool analysing(void) const { return m_analysis != nullptr; }
  QString analysingText(void) const;
  int previewRemove(void) const { return m_preview; }
  void setPreviewRemove(int kind);

  RowsModel * pieces(void) const { return m_pieces; }
  bool xray(void) const { return m_xray; }
  void setXray(bool v);
  QString sceneMessage(void) const;
  QString tagText(void) const;

  /* the status bar's sentence (C21) */
  QString statusText(void) const;

  // --- the 3D scene, for ViewportController ------------------------------------------
  /* the selected solution (or the assembler's state) at the player's position */
  const btui::ShapeMesh * sceneMesh(void) const { return m_sceneMesh.get(); }
  std::shared_ptr<const btui::ShapeMesh> sceneMeshShared(void) const { return m_sceneMesh; }
  quint64 sceneRevision(void) const { return m_sceneRevision; }
  const std::vector<btui::HullEdge> & wire(void) const { return m_wire; }
  const std::vector<btui::Rgba8> & wireColours(void) const { return m_wireColours; }
  btui::Vec3 sceneCentre(void) const { return m_scene.centre; }
  float sceneRadius(void) const { return m_scene.radius; }
  /* a new solution or problem: the camera frames it (not every move) */
  quint64 framingRevision(void) const { return m_framing; }
  /* how far a piece travels out of the assembly, in world units: the 3D
   * view's clip planes take it in (C21) */
  float sceneReach(void) const { return m_reach; }

  /* Stop a running solve and an analysis and wait for both: before the
   * document goes (quit) or is written while they run (the discard dialog's
   * Save). Their results so far are kept. */
  void stopAndWait(void);

  // --- actions -------------------------------------------------------------------------

  Q_INVOKABLE void selectProblem(int row);
  Q_INVOKABLE void prepare(void);
  Q_INVOKABLE void start(void);
  Q_INVOKABLE void resume(void);
  Q_INVOKABLE void stop(void);
  Q_INVOKABLE void step(void);

  Q_INVOKABLE void selectRow(int row);
  /* the solution slider's drag: the selection at once, the scene at most
   * once a frame (AC-C20-02) */
  Q_INVOKABLE void scrubTo(int row);
  Q_INVOKABLE void clickSort(int key);
  /* "Show the assemblies": What to check back to Assemblies */
  Q_INVOKABLE void showAssemblies(void);

  Q_INVOKABLE void togglePlay(void);
  /* start, back, play, fwd, end (C20 transport) */
  Q_INVOKABLE void transport(const QString & which);
  Q_INVOKABLE void seek(double position);
  /* the Solver keyboard model: context list, slider or seekbar; key a
   * Qt::Key; true when the key was the map's */
  Q_INVOKABLE bool key(const QString & context, int key, bool shift);

  Q_INVOKABLE void cyclePiece(int row);
  Q_INVOKABLE void showAllPieces(void);

  /* Manage ▸ Remove assemblies (kind: btui::RemoveKind): the count, the
   * rows previewed; removeSolutions after the user confirmed */
  Q_INVOKABLE int removeCount(int kind) const;
  Q_INVOKABLE void removeSolutions(int kind);
  /* Manage ▸ Disassemblies */
  Q_INVOKABLE void deleteDisassembly(bool all);
  /* which: current, all, missing */
  Q_INVOKABLE void analyse(const QString & which);
  /* Export to STL: the shapes the selected solution places */
  Q_INVOKABLE QList<int> selectedShapes(void) const;

signals:

  void problemChanged(void);
  void optionsChanged(void);
  void runChanged(void);
  void solutionsChanged(void);
  void selectionChanged(void);
  void positionChanged(void);
  void viewChanged(void);
  void sceneChanged(void);
  void explorerChanged(void);
  void flashRequested(const QString & text);
  void announceRequested(const QString & text);
  void messageRequested(const QString & title, const QString & text);
  /* a solve could not place a piece: the shape, to show in Entities */
  void shapeRequested(int shape);

private:

  problem_c * currentProblem(void) const;
  btui::SolverOptions optionsFor(int problem) const;
  /* the selected solution's facts, null for none (m_row may lag a refresh) */
  const btui::SolutionFacts * selectedFacts(void) const;
  /* everything from the problem and the thread again; once per tick */
  void poll(void);
  void refreshRun(void);
  void refreshSolutions(bool force);
  void refreshSelection(void);
  /* the thread has ended; `results` when it may have changed the problem
   * (stopped or finished), so the file reads unsaved */
  void retireThread(bool results);
  /* the rows from the list as it is (the preview's column too) */
  void rebuildRows(void);
  /* the piece chips from m_chips and m_states */
  void refreshChips(void);
  /* problems edited (Structure: added, removed or moved), or rebuilt (undo,
   * redo, another document): what was kept per problem position no longer
   * names the same problem. Options follow their problem through a
   * structure edit; a rebuild forgets them */
  enum class Forget { Edited, Structure, Rebuilt };
  void forgetProblemState(Forget how);
  void selectRowNow(int row, bool deferScene);
  /* a row as a screen reader hears it when the keys move the selection */
  QString rowName(int row) const;
  void finishAnalysis(void);
  void run(bool fresh, bool prepareOnly);
  void setOptions(const btui::SolverOptions & o);
  void buildScene(void);
  void composeScene(void);
  void stopPlaying(void);
  void setPosition(double pos);
  void tick(void);
  void documentChanged(void);
  void adoptPuzzleProblem(void);
  void updateBusy(void);

  SettingsController * m_settings;
  DocumentController * m_doc;
  LayoutController * m_layout;
  PuzzleController * m_puzzle;

  int m_problem = -1;
  /* the document changed while another workspace was shown: refreshed
   * when the Solver is (an Entities stroke need not rebuild this scene) */
  bool m_stale = false;
  bool shown(void) const;
  btui::SolverOptions m_options;
  /* per problem, this session; keyed by the problem itself, which an edit
   * that adds, removes or moves problems keeps (an undo rebuilds them) */
  std::map<const problem_c *, btui::SolverOptions> m_optionsOf;
  bool m_optionsOpen = false;

  std::unique_ptr<solveThread_c> m_thread;
  int m_threadProblem = -1;
  std::map<int, bool> m_prepared;                   ///< the last run of a problem was Prepare
  std::map<int, std::vector<unsigned>> m_cannot;    ///< per problem: assemblies found not to come apart
  QTimer m_poll;
  QTimer m_sceneTimer;                              ///< scrubTo's one build a frame
  /* what refreshSolutions last read, to skip a poll that would find the same */
  struct Signature {
    unsigned saved = 0;
    unsigned long long revision = 0, changes = 0;   ///< the list's and its solutions' changes
    unsigned long assemblies = 0, solutions = 0;
    int state = -1;
    size_t cannot = 0;
    bool threadHere = false;
    bool operator==(const Signature &) const = default;
  };
  Signature m_signature;
  std::map<int, double> m_floor;                    ///< per problem: the progress shown, never going back
  float m_reach = 0;
  btui::VoxelStyle m_builtStyle = btui::VoxelStyle::Flat;
  btui::ShapeColours m_builtColours;
  bool m_sceneBuilt = false;

  btui::RunFacts m_facts;
  btui::RunButtons m_buttons;
  double m_progress = 0;
  QString m_activityText, m_assembliesText, m_solutionsText, m_timeUsedText, m_timeLeftText;
  int m_lastState = -1;

  /* the solutions read, kept from one read to the next: a refresh during a
   * solve reads only the new ones and inserts their rows */
  btui::SolutionList m_list;
  /* the list is read at most every kListMs while a solve runs, every
   * kListSlowMs past kManySolutions; not while another workspace shows */
  QElapsedTimer m_listClock;
  bool m_listStale = false;
  bool listDue(void);
  int m_maxMoves = 0;                               ///< the rows' longest disassembly, for movesFraction
  RowsModel::Row rowOf(const btui::SolutionFacts & f, bool doomed) const;
  btui::ListSort m_sort;
  EmptyState m_empty = NoProblem;
  RowsModel * m_rows;
  int m_row = -1;
  unsigned int m_selectedAssembly = 0;
  bool m_hasSelection = false;
  QString m_view = QStringLiteral("list");
  QString m_explorer;
  void keepExplorer(void);
  RowsModel * m_placements = nullptr;
  void refreshPlacements(void);
  int m_preview = -1;

  // the player
  std::unique_ptr<disasmToMoves_c> m_moves;
  double m_total = 0;
  double m_position = 0;
  bool m_playing = false;
  double m_speed = 1;
  QTimer m_play;
  QElapsedTimer m_playClock;

  // the scene
  btui::SolutionScene m_scene;
  std::unique_ptr<assembly_c> m_assemblerState;
  QString m_assemblerCaption;
  std::shared_ptr<btui::ShapeMesh> m_sceneMesh;
  quint64 m_sceneRevision = 0;
  quint64 m_framing = 0;
  std::vector<btui::HullEdge> m_wire;
  std::vector<btui::Rgba8> m_wireColours;
  bool m_xray = true;
  RowsModel * m_pieces;
  std::vector<btui::PieceChip> m_chips;
  std::map<std::string, btui::PieceState> m_states;   ///< by piece id, kept across solutions

  // the background analysis (Manage ▸ Analyse)
  struct Analysis;
  std::unique_ptr<Analysis> m_analysis;
};

#endif
