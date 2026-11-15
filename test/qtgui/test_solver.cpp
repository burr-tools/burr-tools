/* Qt Test cases for the Solver workspace: runs through SolverController, the
 * document's busy gate while a solve reads the puzzle, the list's filter and
 * sort, Manage, the background analysis, the player, the keys, the piece
 * display and the assembler's state.
 */
#include "test_solver.h"

#include "app.h"
#include "layoutcontroller.h"
#include "solvercontroller.h"
#include "statuscontroller.h"
#include "stlexportcontroller.h"
#include "viewportcontroller.h"

#include "../../src/lib/problem.h"
#include "../../src/lib/puzzle.h"
#include "../../src/lib/solution.h"
#include "../../src/lib/voxel.h"
#include "../../src/uicore/problemlist.h"
#include "../../src/uicore/solver.h"

#include <QAbstractItemModelTester>
#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

namespace {

  constexpr int kSolveTimeout = 60000;

  /* an App in the Solver workspace on Pelikan Burr, its stored results gone */
  struct SolverFixture {
    QTemporaryDir dir;
    std::unique_ptr<App> app;

    explicit SolverFixture(bool clear = true) {
      app = std::make_unique<App>(dir.filePath(QStringLiteral("s.rc")), dir.filePath(QStringLiteral("l.rc")));
      app->settings()->setWorkerThreads(1);
      if (!app->document()->loadPath(QStringLiteral("examples/PelikanBurr.xmpuzzle")))
        return;
      app->layout()->setWorkspace(LayoutController::Solver);
      if (clear) {
        app->document()->editResults([](puzzle_c & p) { p.getProblem(0)->removeAllSolutions(); });
        solver()->resetOptions();
        solver()->setDisassemble(false);
      }
      solver()->selectProblem(0);
    }

    SolverController * solver(void) const { return app->solver(); }
    const problem_c & problem(void) const { return *app->document()->session().puzzle().getProblem(0); }

    /* a run to its end, as the Start button does */
    bool solve(bool disassemble) {
      solver()->setDisassemble(disassemble);
      solver()->start();
      if (!solver()->solving())
        return false;
      return QTest::qWaitFor([&] { return !solver()->solving(); }, kSolveTimeout);
    }
  };
}

void TestSolver::aStoredResultOpensWithItsWayOfChecking() {
  // the file's solution was found with the disassembly: the list shows it so
  SolverFixture f(false);
  QVERIFY(f.solver()->disassemble());
  QCOMPARE(f.solver()->shownCount(), 1);
  QCOMPARE(f.solver()->runState(), int(SolverController::Finished));
  QCOMPARE(f.solver()->selectedRow(), 0);
  QCOMPARE(f.solver()->playerState(), int(SolverController::Playable));
  QVERIFY(f.solver()->totalMoves() > 0);
}

void TestSolver::aRunFindsTheSolutionsAndMarksTheFileUnsaved() {
  // AC-C19-04, J14
  SolverFixture f;
  QCOMPARE(f.solver()->emptyState(), int(SolverController::NeverRun));
  QVERIFY(f.solver()->canStart());
  QVERIFY(!f.solver()->canContinue());
  QVERIFY(f.solve(false));
  QCOMPARE(f.solver()->runState(), int(SolverController::Finished));
  QCOMPARE(f.solver()->savedCount(), 12);
  QCOMPARE(f.solver()->shownCount(), 12);
  QCOMPARE(f.solver()->assembliesText(), QStringLiteral("12"));
  QCOMPARE(f.solver()->solutionsText(), QStringLiteral("—"));      // Assemblies only
  QCOMPARE(f.solver()->progressText(), QStringLiteral("100.0 %"));
  QVERIFY(f.app->document()->modified());
  QVERIFY(!f.app->document()->busy());
  // results are not an undo step (overview/03)
  QVERIFY(!f.app->document()->canUndo());

  // Start again begins afresh; the disassembly keeps the one solution
  QVERIFY(f.solve(true));
  QCOMPARE(f.solver()->shownCount(), 1);
  QCOMPARE(f.solver()->solutionsText(), QStringLiteral("1"));
}

void TestSolver::theDocumentIsBusyWhileASolveRuns() {
  SolverFixture f;
  QSignalSpy busy(f.app->document(), &DocumentController::busyChanged);
  f.solver()->setDisassemble(true);
  f.solver()->start();
  QVERIFY(f.solver()->solving());
  QVERIFY(f.app->document()->busy());
  // nothing else may start meanwhile
  QVERIFY(!f.solver()->canStart());
  QVERIFY(!f.solver()->canPrepare());
  QVERIFY(!f.solver()->canManage());
  QVERIFY(QTest::qWaitFor([&] { return !f.solver()->solving(); }, kSolveTimeout));
  QVERIFY(!f.app->document()->busy());
  QCOMPARE(busy.count(), 2);
}

void TestSolver::nothingChangesThePuzzleWhileBusy() {
  // legacy threadStopped(): edits, strokes, undo, redo, New, Open, Save refuse
  SolverFixture f;
  DocumentController * d = f.app->document();
  QVERIFY(d->editStructure(0, [](puzzle_c & p) { p.getShape(0)->setName("renamed"); return true; }));
  QVERIFY(d->canUndo());
  QSignalSpy refused(d, &DocumentController::busyRefused);
  QSignalSpy newFile(d, &DocumentController::newFileTypeRequested);
  QSignalSpy saveAs(d, &DocumentController::saveAsRequested);
  d->setBusy(true);

  QVERIFY(!d->editStructure(0, [](puzzle_c & p) { p.getShape(0)->setName("again"); return true; }));
  QCOMPARE(QString::fromStdString(d->session().puzzle().getShape(0)->getName()), QStringLiteral("renamed"));
  d->beginStroke(puzzleHistory_c::AK_ENTITIES_GRID_PAINT, 0);
  QVERIFY(!d->strokeEdit(0, [](puzzle_c & p) { p.getShape(0)->setState(0, voxel_c::VX_EMPTY); return true; }));
  d->endStroke();
  d->undo();
  QCOMPARE(QString::fromStdString(d->session().puzzle().getShape(0)->getName()), QStringLiteral("renamed"));
  d->requestNew();
  QCOMPARE(newFile.count(), 0);
  d->requestSaveAs();
  QCOMPARE(saveAs.count(), 0);
  QVERIFY(!d->mayCostSolutions(true, [] {}));
  QVERIFY(refused.count() >= 6);
  QCOMPARE(refused.first().first().toString(), QStringLiteral("Stop the solver first"));

  // the solver's own results still go in
  d->editResults([](puzzle_c &) {});
  d->setBusy(false);
  QVERIFY(d->editStructure(0, [](puzzle_c & p) { p.getShape(0)->setName("again"); return true; }));
}

void TestSolver::theListShowsWhatComesApartWhenTheDisassemblyIsChecked() {
  // AC-C20-03
  SolverFixture f;
  QVERIFY(f.solve(false));
  QCOMPARE(f.solver()->shownCount(), 12);
  f.solver()->setDisassemble(true);
  QCOMPARE(f.solver()->shownCount(), 0);
  QCOMPARE(f.solver()->emptyState(), int(SolverController::NoneDisassemblable));
  QVERIFY(f.solver()->emptyText().contains(QStringLiteral("12 assemblies were found")));
  f.solver()->showAssemblies();
  QVERIFY(!f.solver()->disassemble());
  QCOMPARE(f.solver()->shownCount(), 12);
}

void TestSolver::theHeadersSortAndReverse() {
  SolverFixture f;
  QVERIFY(f.solve(false));
  RowsModel * rows = f.solver()->solutions();
  QCOMPARE(rows->value(0, QStringLiteral("number")).toInt(), 1);
  // Level and Moves wait for disassembly information; a click on them does nothing
  QVERIFY(!f.solver()->levelSortable());
  f.solver()->clickSort(int(btui::ListKey::Moves));
  QCOMPARE(f.solver()->sortKey(), int(btui::ListKey::Number));
  f.solver()->clickSort(int(btui::ListKey::Number));
  QVERIFY(f.solver()->sortDescending());
  QCOMPARE(rows->value(0, QStringLiteral("number")).toInt(), 12);
  // the selection is the same solution wherever it moved
  f.solver()->selectRow(0);
  const int number = f.solver()->selectedNumber();
  f.solver()->clickSort(int(btui::ListKey::Number));
  QCOMPARE(f.solver()->selectedNumber(), number);
  QCOMPARE(f.solver()->selectedRow(), 11);
}

void TestSolver::removingIsNotAnUndoStepAndKeepsTheSelection() {
  // OQ-25, AC-C20-05
  SolverFixture f;
  QVERIFY(f.solve(false));
  f.solver()->selectRow(4);
  const int number = f.solver()->selectedNumber();
  QCOMPARE(f.solver()->removeCount(int(btui::RemoveKind::Before)), 4);
  QCOMPARE(f.solver()->removeCount(int(btui::RemoveKind::After)), 7);
  f.solver()->removeSolutions(int(btui::RemoveKind::Before));
  QCOMPARE(f.solver()->shownCount(), 8);
  QCOMPARE(f.solver()->selectedNumber(), number);
  QCOMPARE(f.solver()->selectedRow(), 0);
  QVERIFY(!f.app->document()->canUndo());
  QVERIFY(f.app->document()->modified());

  // At: the next one takes its place
  f.solver()->selectRow(2);
  const int next = f.solver()->solutions()->value(3, QStringLiteral("number")).toInt();
  f.solver()->removeSolutions(int(btui::RemoveKind::At));
  QCOMPARE(f.solver()->shownCount(), 7);
  QCOMPARE(f.solver()->selectedNumber(), next);

  f.solver()->removeSolutions(int(btui::RemoveKind::All));
  QCOMPARE(f.solver()->savedCount(), 0);
  QCOMPARE(f.solver()->selectedRow(), -1);
  QCOMPARE(f.solver()->playerState(), int(SolverController::NothingToPlay));
}

void TestSolver::theRemovePreviewMarksExactlyItsRows() {
  SolverFixture f;
  QVERIFY(f.solve(false));
  f.solver()->selectRow(9);
  f.solver()->setPreviewRemove(int(btui::RemoveKind::After));
  RowsModel * rows = f.solver()->solutions();
  int marked = 0;
  for (int r = 0; r < rows->count(); r++)
    if (rows->value(r, QStringLiteral("willDelete")).toBool()) {
      QVERIFY(r > 9);
      marked++;
    }
  QCOMPARE(marked, 2);
  f.solver()->setPreviewRemove(-1);
  for (int r = 0; r < rows->count(); r++)
    QVERIFY(!rows->value(r, QStringLiteral("willDelete")).toBool());
}

void TestSolver::analysingStoresDisassembliesInTheBackground() {
  // A A DA (OQ-25); "cannot be disassembled" marked (C20 DA column)
  SolverFixture f;
  QVERIFY(f.solve(false));
  QVERIFY(f.solver()->canAnalyse());
  f.solver()->analyse(QStringLiteral("all"));
  QVERIFY(f.solver()->analysing());
  QVERIFY(f.app->document()->busy());
  QVERIFY(!f.solver()->canManage());
  QVERIFY(QTest::qWaitFor([&] { return !f.solver()->analysing(); }, kSolveTimeout));
  QVERIFY(!f.app->document()->busy());
  int stored = 0, cannot = 0;
  RowsModel * rows = f.solver()->solutions();
  for (int r = 0; r < rows->count(); r++) {
    const int da = rows->value(r, QStringLiteral("da")).toInt();
    stored += da == int(btui::DaState::Stored) ? 1 : 0;
    cannot += da == int(btui::DaState::Cannot) ? 1 : 0;
  }
  QCOMPARE(stored, 1);
  QCOMPARE(cannot, 11);
  QVERIFY(f.solver()->levelSortable());
  // with the disassembly checked, the one that comes apart
  f.solver()->setDisassemble(true);
  QCOMPARE(f.solver()->shownCount(), 1);

  // D A DA keeps the assemblies, and the level (legacy removeDisassembly)
  f.solver()->setDisassemble(false);
  f.solver()->deleteDisassembly(true);
  QCOMPARE(f.solver()->savedCount(), 12);
  int levelOnly = 0;
  for (int r = 0; r < rows->count(); r++)
    levelOnly += rows->value(r, QStringLiteral("da")).toInt() == int(btui::DaState::LevelOnly) ? 1 : 0;
  QCOMPARE(levelOnly, 1);
}

void TestSolver::aSolveAddsRowsInPlaceAndSparesAHiddenList() {
  // a 5x8 domino tiling's 3,818 solutions with their disassemblies
  QTemporaryDir dir;
  App app(dir.filePath(QStringLiteral("s.rc")), dir.filePath(QStringLiteral("l.rc")));
  app.settings()->setWorkerThreads(1);
  QVERIFY(app.document()->loadPath(QStringLiteral("test/data/dominoes-5x8.xmpuzzle")));
  app.layout()->setWorkspace(LayoutController::Solver);
  SolverController * s = app.solver();
  s->selectProblem(0);
  s->setDisassemble(true);
  s->setLimit(s->maxCount());
  RowsModel * rows = s->solutions();
  QAbstractItemModelTester tester(rows, QAbstractItemModelTester::FailureReportingMode::QtTest);

  // while another workspace shows, the polls leave the list unread; the end
  // of the run reads it once
  app.layout()->setWorkspace(LayoutController::Puzzle);
  s->start();
  QVERIFY(s->solving());
  while (s->solving()) {
    QCOMPARE(rows->count(), 0);
    QTest::qWait(20);
  }
  QCOMPARE(rows->count(), 3818);
  app.layout()->setWorkspace(LayoutController::Solver);
  QCOMPARE(rows->count(), 3818);

  // shown and sorted by moves, best first: each read puts its new rows in
  // where they belong, Qt's model tester following every insertion
  s->clickSort(int(btui::ListKey::Moves));
  QCOMPARE(s->sortKey(), int(btui::ListKey::Moves));
  auto inOrder = [&] {
    for (int r = 1; r < rows->count(); r++) {
      const int m0 = rows->value(r - 1, QStringLiteral("moves")).toInt(), m1 = rows->value(r, QStringLiteral("moves")).toInt();
      const int n0 = rows->value(r - 1, QStringLiteral("number")).toInt(), n1 = rows->value(r, QStringLiteral("number")).toInt();
      if (m0 < m1 || (m0 == m1 && n0 > n1))
        return false;
    }
    return true;
  };
  QVERIFY(inOrder());
  QSignalSpy reset(rows, &QAbstractItemModel::modelReset);
  s->start();
  while (s->solving()) {
    QTest::qWait(20);
    QCOMPARE(rows->count(), s->shownCount());
    QVERIFY(inOrder());
  }
  QCOMPARE(rows->count(), 3818);
  QVERIFY(inOrder());
  QCOMPARE(reset.count(), 0);
}

void TestSolver::thePlayerStepsPlaysAndSeeks() {
  // AC-C20-04, OQ-30
  SolverFixture f;
  QVERIFY(f.solve(true));
  SolverController * s = f.solver();
  QCOMPARE(s->playerState(), int(SolverController::Playable));
  const double total = s->totalMoves();
  QVERIFY(total > 2);
  QCOMPARE(s->moveText(), QStringLiteral("Move 0.0 / %1").arg(int(total)));
  const quint64 scene = s->sceneRevision();
  s->transport(QStringLiteral("fwd"));
  QCOMPARE(s->position(), 1.0);
  QVERIFY(s->sceneRevision() > scene);
  s->seek(2.4);
  s->transport(QStringLiteral("back"));
  QCOMPARE(s->position(), 2.0);
  s->transport(QStringLiteral("end"));
  QCOMPARE(s->position(), total);
  // play from the end starts again from the assembly, and runs
  s->setSpeed(8);
  s->togglePlay();
  QVERIFY(s->playing());
  QVERIFY(QTest::qWaitFor([&] { return s->position() > 0.5; }, 5000));
  s->seek(1.0);                                   // dragging pauses
  QVERIFY(!s->playing());
  // a new solution starts assembled
  s->transport(QStringLiteral("start"));
  QCOMPARE(s->position(), 0.0);
}

void TestSolver::theKeyMapMovesThroughSolutionsAndMoves() {
  // AC-C20-07
  SolverFixture f;
  QVERIFY(f.solve(false));
  SolverController * s = f.solver();
  s->selectRow(0);
  QVERIFY(s->key(QStringLiteral("list"), Qt::Key_Down, false));
  QCOMPARE(s->selectedRow(), 1);
  QVERIFY(s->key(QStringLiteral("slider"), Qt::Key_PageDown, false));
  QCOMPARE(s->selectedRow(), 9);
  QVERIFY(s->key(QStringLiteral("list"), Qt::Key_End, false));
  QCOMPARE(s->selectedRow(), 11);
  QVERIFY(s->key(QStringLiteral("seekbar"), Qt::Key_Home, false));   // the moves', not the list's
  QCOMPARE(s->selectedRow(), 11);
  QVERIFY(s->key(QStringLiteral("list"), Qt::Key_Home, false));
  QCOMPARE(s->selectedRow(), 0);
  // Delete is the card's (it asks first); a letter is nobody's
  QVERIFY(!s->key(QStringLiteral("list"), Qt::Key_Delete, false));
  QVERIFY(!s->key(QStringLiteral("list"), Qt::Key_A, false));
  // Space with nothing stored says so
  QSignalSpy flash(s, &SolverController::flashRequested);
  QVERIFY(s->key(QStringLiteral("list"), Qt::Key_Space, false));
  QCOMPARE(flash.count(), 1);
  QVERIFY(!s->playing());
}

void TestSolver::aChipCyclesSolidWireframeHidden() {
  // AC-C20-06, AC-C21-01, AC-C21-02
  SolverFixture f(false);
  SolverController * s = f.solver();
  RowsModel * chips = s->pieces();
  QCOMPARE(chips->count(), 7);
  const size_t solid = s->sceneMesh()->opaque.size();
  QVERIFY(s->wire().empty());
  s->cyclePiece(0);
  QCOMPARE(chips->value(0, QStringLiteral("pieceState")).toInt(), int(btui::PieceState::Wireframe));
  QVERIFY(!s->wire().empty());
  QVERIFY(s->sceneMesh()->opaque.size() < solid);
  s->cyclePiece(0);
  QCOMPARE(chips->value(0, QStringLiteral("pieceState")).toInt(), int(btui::PieceState::Hidden));
  QVERIFY(s->wire().empty());
  // the view draws the wire as the ghosts' lines (X-ray), or depth-tested
  s->cyclePiece(1);
  SceneFrame fr = f.app->viewport()->frame(1);
  QVERIFY(!fr.ghostLines.empty());
  s->setXray(false);
  fr = f.app->viewport()->frame(1);
  QVERIFY(fr.ghostLines.empty());
  QVERIFY(!fr.lines.empty());
  // states stay with the piece's id while browsing; Show all resets
  s->showAllPieces();
  for (int r = 0; r < chips->count(); r++)
    QCOMPARE(chips->value(r, QStringLiteral("pieceState")).toInt(), int(btui::PieceState::Solid));
}

void TestSolver::prepareThenStepShowsTheAssemblersState() {
  // AC-C19-05, AC-C21-03, OQ-29
  SolverFixture f;
  SolverController * s = f.solver();
  s->prepare();
  QVERIFY(QTest::qWaitFor([&] { return !s->solving(); }, kSolveTimeout));
  QCOMPARE(s->runState(), int(SolverController::Ready));
  QVERIFY(s->canStep());
  QVERIFY(s->canPlacements());
  QVERIFY(s->canContinue());
  for (int i = 0; i < 3; i++)
    s->step();
  QVERIFY(s->assemblerStateShown());
  QVERIFY(s->assemblerCaption().startsWith(QStringLiteral("Assembler state")));
  s->toggleExplorer(QStringLiteral("placements"));
  QCOMPARE(s->explorer(), QStringLiteral("placements"));
  QCOMPARE(s->placements()->count(), int(f.problem().getNumberOfPieces()));
  // Continue runs the prepared search to its end
  s->resume();
  QVERIFY(QTest::qWaitFor([&] { return !s->solving(); }, kSolveTimeout));
  QCOMPARE(s->runState(), int(SolverController::Finished));
  QVERIFY(s->explorer().isEmpty());               // the search it browsed is gone
  // choosing a solution leaves the assembler's state
  s->selectRow(0);
  QVERIFY(!s->assemblerStateShown());
}

void TestSolver::theMovementsDrawerClosesWithItsDisassembly() {
  // AC-C19-05
  SolverFixture f(false);
  SolverController * s = f.solver();
  QVERIFY(s->canMovements());
  s->toggleExplorer(QStringLiteral("movements"));
  QCOMPARE(s->explorer(), QStringLiteral("movements"));
  s->deleteDisassembly(false);
  QVERIFY(s->explorer().isEmpty());
}

void TestSolver::optionsArePerProblemAndCountTheirChanges() {
  // AC-C19-03, decision 4 of the plan
  SolverFixture f;
  SolverController * s = f.solver();
  QCOMPARE(s->changedOptions(), 0);
  s->setKeepMirror(true);
  s->setLimit(50);
  QCOMPARE(s->changedOptions(), 2);
  // Level and Moves need the disassembly
  QVERIFY(!s->searchSortAvailable(int(btui::SearchSort::Level)));
  s->setDisassemble(true);
  QVERIFY(s->searchSortAvailable(int(btui::SearchSort::Level)));
  s->setDropDisassemblies(true);
  s->setDisassemble(false);
  QVERIFY(!s->dropDisassemblies());              // the sub-switch goes with its card

  f.app->document()->editStructure(-1, [](puzzle_c & p) {
    p.addProblem();
    return true;
  });
  s->selectProblem(1);
  QCOMPARE(s->changedOptions(), 0);
  s->selectProblem(0);
  QCOMPARE(s->changedOptions(), 2);
  s->resetOptions();
  QCOMPARE(s->changedOptions(), 0);
}

void TestSolver::aSolutionExportsEachPlacedShapeOnce() {
  // OQ-33: legacy stlExportSolution_c, one file per shape the solution places
  SolverFixture f(false);
  const QList<int> shapes = f.solver()->selectedShapes();
  QVERIFY(!shapes.isEmpty());
  StlExportController * stl = f.app->stl();
  stl->setSolutionShapes(shapes);
  stl->begin();
  QVERIFY(shapes.contains(stl->shape()));
  QTemporaryDir out;
  const QUrl folder = QUrl::fromLocalFile(out.path());
  QCOMPARE(stl->existingSolutionFiles(folder), 0);
  QSignalSpy exported(stl, &StlExportController::exported);
  QCOMPARE(stl->exportSolution(folder), int(shapes.size()));
  QCOMPARE(exported.count(), 1);
  QCOMPARE(QDir(out.path()).entryList({ QStringLiteral("*.stl") }).size(), shapes.size());
  QCOMPARE(stl->existingSolutionFiles(folder), int(shapes.size()));
  stl->end();
  QVERIFY(stl->solutionShapes().isEmpty());       // the plain Export ▸ STL again
}

namespace {

  /* a new brick puzzle: a result of `result` voxels in a row, one piece of
   * `piece` voxels in a row (or a `side` x `side` square result) */
  struct Made {
    QTemporaryDir dir;
    std::unique_ptr<App> app;
    Made(int gridType, int resultX, int resultY, int piece) {
      app = std::make_unique<App>(dir.filePath(QStringLiteral("s.rc")), dir.filePath(QStringLiteral("l.rc")));
      app->settings()->setWorkerThreads(1);
      app->document()->newDocument(gridType);
      app->document()->editStructure(0, [&](puzzle_c & p) {
        const unsigned r = p.addShape(unsigned(resultX), unsigned(resultY), 1);
        p.getShape(r)->setAll(voxel_c::VX_FILLED);
        const unsigned s = p.addShape(unsigned(piece), 1, 1);
        p.getShape(s)->setAll(voxel_c::VX_FILLED);
        const int i = btui::newProblem(p);
        btui::setResult(p, i, int(r));      // a new problem has none (2026-11-14)
        btui::setPieceCount(p, i, int(s), 1, 1);
        return true;
      });
      app->layout()->setWorkspace(LayoutController::Solver);
      app->solver()->selectProblem(0);
    }
  };

  /* what the run says when it ends in an error */
  QString errorOf(Made & m) {
    QSignalSpy said(m.app->solver(), &SolverController::messageRequested);
    m.app->solver()->start();
    if (!QTest::qWaitFor([&] { return !m.app->solver()->solving(); }, kSolveTimeout))
      return QStringLiteral("(still solving)");
    return said.isEmpty() ? QString() : said.last().at(1).toString();
  }
}

void TestSolver::theSolversErrorsAreSaidInItsWords() {
  // legacy mainWindow_c::update()'s messages, as the run ends
  {
    Made m(int(gridType_c::GT_BRICKS), 2, 1, 3);
    QCOMPARE(errorOf(m), QStringLiteral("Pieces contain 1 units too many"));
    QVERIFY(!m.app->document()->busy());
  }
  {
    Made m(int(gridType_c::GT_BRICKS), 3, 1, 1);
    QCOMPARE(errorOf(m), QStringLiteral("Pieces contain 2 units less than required"));
  }
  {
    // four voxels each, but the bar fits nowhere in the square: legacy then
    // shows that shape in Entities
    Made m(int(gridType_c::GT_BRICKS), 2, 2, 4);
    QSignalSpy shown(m.app->solver(), &SolverController::shapeRequested);
    QCOMPARE(errorOf(m), QStringLiteral("Piece 2 can be placed nowhere within the result"));
    QCOMPARE(shown.count(), 1);
    QCOMPARE(m.app->layout()->workspace(), int(LayoutController::Entities));
    QCOMPARE(m.app->shapes()->selected(), 1);
  }
}

void TestSolver::aGridWithoutADisassemblerRefusesTheCheck() {
  // spheres have an assembler and no disassembler (legacy CAP_DISASSEMBLE)
  Made m(int(gridType_c::GT_SPHERES), 2, 1, 2);
  SolverController * s = m.app->solver();
  QVERIFY(!s->canDisassemble());
  s->setDisassemble(true);
  QSignalSpy said(s, &SolverController::messageRequested);
  s->start();
  QVERIFY(!s->solving());
  QCOMPARE(said.count(), 1);
  QVERIFY(said.first().at(1).toString().contains(QStringLiteral("doesn't have a disassembler")));
  // the assemblies alone run, and nothing can be analysed
  s->setDisassemble(false);
  s->start();
  QVERIFY(QTest::qWaitFor([&] { return !s->solving(); }, kSolveTimeout));
  QVERIFY(s->savedCount() > 0);
  QVERIFY(!s->canAnalyse());
}

void TestSolver::aProblemWithoutAResultDoesNotRun() {
  QTemporaryDir dir;
  App app(dir.filePath(QStringLiteral("s.rc")), dir.filePath(QStringLiteral("l.rc")));
  app.document()->newDocument(int(gridType_c::GT_BRICKS));
  app.document()->editStructure(0, [](puzzle_c & p) {
    p.addShape(1, 1, 1);
    p.addProblem();                        // no result
    return true;
  });
  app.layout()->setWorkspace(LayoutController::Solver);
  SolverController * s = app.solver();
  s->selectProblem(0);
  QVERIFY(!s->canStart());
  QCOMPARE(s->problemDetail(), QStringLiteral("No result · 0 piece shapes"));
  QSignalSpy said(s, &SolverController::messageRequested);
  s->start();                              // the button is off; the call says why
  QCOMPARE(said.count(), 1);
  QCOMPARE(said.first().at(1).toString(), QStringLiteral("A result shape must be defined"));
  QCOMPARE(s->sceneMessage(), QStringLiteral("Run the solver to see solutions here"));
  QCOMPARE(s->statusText(), QStringLiteral("Problem <b>P1</b> — not solved yet"));
}

void TestSolver::everySearchOptionCountsAsAChange() {
  SolverFixture f;
  SolverController * s = f.solver();
  s->setJustCount(true);
  s->setKeepRotated(true);
  s->setThoroughRotation(true);
  s->setKeepEvery(3);
  s->setSearchSort(int(btui::SearchSort::Pieces));
  QCOMPARE(s->changedOptions(), 5);
  s->setSearchSort(99);                    // no such sort: nothing changes
  QCOMPARE(s->searchSort(), int(btui::SearchSort::Pieces));
  QVERIFY(s->justCount() && s->keepRotated() && s->thoroughRotation());
  QCOMPARE(s->keepEvery(), 3);
  // Just count: counted, not kept, and the list says so
  QVERIFY(f.solve(false));
  QCOMPARE(s->savedCount(), 0);
  QCOMPARE(s->emptyState(), int(SolverController::JustCounted));
  QCOMPARE(s->assembliesText(), QStringLiteral("12"));
}

void TestSolver::theRestOfTheKeyMap() {
  // the stored solution plays: Enter, Return and Space toggle, the arrows step
  SolverFixture f(false);
  SolverController * s = f.solver();
  QVERIFY(s->key(QStringLiteral("list"), Qt::Key_Right, false));
  QCOMPARE(s->position(), 1.0);
  QVERIFY(s->key(QStringLiteral("seekbar"), Qt::Key_Left, false));
  QCOMPARE(s->position(), 0.0);
  QVERIFY(s->key(QStringLiteral("list"), Qt::Key_Right, true));
  QCOMPARE(s->position(), s->totalMoves());
  QVERIFY(s->key(QStringLiteral("list"), Qt::Key_Left, true));
  QCOMPARE(s->position(), 0.0);
  QVERIFY(s->key(QStringLiteral("list"), Qt::Key_Return, false));
  QVERIFY(s->playing());
  QVERIFY(s->key(QStringLiteral("list"), Qt::Key_Escape, false));
  QVERIFY(!s->playing());
  QVERIFY(s->key(QStringLiteral("slider"), Qt::Key_Enter, false));
  QVERIFY(s->playing());
  s->togglePlay();
  QVERIFY(!s->playing());
  QVERIFY(s->key(QStringLiteral("list"), Qt::Key_Up, false));
  QVERIFY(s->key(QStringLiteral("list"), Qt::Key_PageUp, false));
  QCOMPARE(s->selectedRow(), 0);
  QVERIFY(!s->key(QStringLiteral("list"), Qt::Key_Backspace, false));   // the card's: it asks first
  // the speeds snap to the player's own
  s->setSpeed(3);
  QCOMPARE(s->speed(), 2.0);
  s->setSpeed(100);
  QCOMPARE(s->speed(), 8.0);
}

void TestSolver::stopPausesAndContinueFinishes() {
  // OQ-29: Stop pauses a search that Continue takes up where it was
  SolverFixture f;
  SolverController * s = f.solver();
  s->setDisassemble(true);
  s->start();
  QVERIFY(s->canStop());
  s->stop();
  QVERIFY(QTest::qWaitFor([&] { return !s->solving(); }, kSolveTimeout));
  if (s->runState() == int(SolverController::Paused)) {
    QVERIFY(s->canContinue());
    QCOMPARE(s->activityText(), QStringLiteral("stopped"));
    s->resume();
    QVERIFY(QTest::qWaitFor([&] { return !s->solving(); }, kSolveTimeout));
  }
  QCOMPARE(s->runState(), int(SolverController::Finished));
  QCOMPARE(s->shownCount(), 1);
}

void TestSolver::escPausesOnlyWhilePlaying() {
  // Esc pauses the player; otherwise it is not the player's (a menu or the view takes it)
  SolverFixture f(false);
  SolverController * s = f.solver();
  QVERIFY(!s->key(QStringLiteral("list"), Qt::Key_Escape, false));
  s->togglePlay();
  QVERIFY(s->playing());
  QVERIFY(s->key(QStringLiteral("list"), Qt::Key_Escape, false));
  QVERIFY(!s->playing());
  QVERIFY(!s->key(QStringLiteral("list"), Qt::Key_Escape, false));
}

void TestSolver::theEmptyListSaysWhy() {
  // C20's empty states: prepared but not run, everything removed
  SolverFixture f;
  SolverController * s = f.solver();
  QSignalSpy scene(s, &SolverController::sceneChanged);
  s->prepare();
  QVERIFY(QTest::qWaitFor([&] { return !s->solving(); }, kSolveTimeout));
  QCOMPARE(s->emptyState(), int(SolverController::NotFinished));
  QCOMPARE(s->emptyTitle(), QStringLiteral("Search not finished"));
  QVERIFY(s->sceneMessage().contains(QStringLiteral("not finished")));
  QVERIFY(scene.count() > 0);                     // the view's message follows
  s->resume();
  QVERIFY(QTest::qWaitFor([&] { return !s->solving(); }, kSolveTimeout));
  QCOMPARE(s->emptyState(), int(SolverController::NotEmpty));
  s->removeSolutions(int(btui::RemoveKind::All));
  QCOMPARE(s->emptyState(), int(SolverController::NoneKept));
  QVERIFY(s->emptyText().startsWith(QStringLiteral("12 assemblies were found")));
}

void TestSolver::anEditForgetsTheAssemblersState() {
  // the assembler stepped through belongs to the puzzle before the edit
  SolverFixture f;
  SolverController * s = f.solver();
  s->prepare();
  QVERIFY(QTest::qWaitFor([&] { return !s->solving(); }, kSolveTimeout));
  s->step();
  QVERIFY(s->assemblerStateShown());
  s->toggleExplorer(QStringLiteral("placements"));
  QCOMPARE(s->explorer(), QStringLiteral("placements"));
  QVERIFY(f.app->document()->editStructure(0, [](puzzle_c & p) { p.getShape(0)->setName("renamed"); return true; }));
  QVERIFY(!s->assemblerStateShown());
  QVERIFY(s->explorer().isEmpty());
}

void TestSolver::optionsFollowTheirProblemAndAnUndoForgetsThem() {
  // options are kept per problem for the session; a structure edit keeps them
  // with their problem, an undo rebuilds the problems and forgets them
  SolverFixture f;
  SolverController * s = f.solver();
  DocumentController * d = f.app->document();
  s->setKeepMirror(true);
  QVERIFY(d->editStructure(-1, [](puzzle_c & p) { p.addProblem(); return true; }));
  QCOMPARE(s->changedOptions(), 1);
  s->selectProblem(1);
  s->setLimit(50);
  s->selectProblem(0);
  QCOMPARE(s->changedOptions(), 1);
  QVERIFY(s->keepMirror());
  d->undo();                                      // the added problem goes
  QCOMPARE(s->problem(), 0);
  QCOMPARE(s->changedOptions(), 0);
}

void TestSolver::scrubbingSelectsAtOnceAndDrawsOnceAFrame() {
  // the slider's drag: title and tiles at once, the scene on the next frame
  SolverFixture f;
  QVERIFY(f.solve(false));
  SolverController * s = f.solver();
  s->selectRow(0);
  const quint64 scene = s->sceneRevision();
  s->scrubTo(3);
  s->scrubTo(5);
  QCOMPARE(s->selectedRow(), 5);
  QCOMPARE(s->sceneRevision(), scene);
  QVERIFY(QTest::qWaitFor([&] { return s->sceneRevision() > scene; }, 2000));
  const quint64 drawn = s->sceneRevision();
  QTest::qWait(100);
  QCOMPARE(s->sceneRevision(), drawn);            // once for both moves
}

void TestSolver::theExplorerClosesWhenTheSelectionCannotShowIt() {
  // the Movements drawer shows the selected disassembly: one without closes it
  SolverFixture f;
  QVERIFY(f.solve(false));
  SolverController * s = f.solver();
  s->analyse(QStringLiteral("all"));
  QVERIFY(QTest::qWaitFor([&] { return !s->analysing(); }, kSolveTimeout));
  RowsModel * rows = s->solutions();
  int stored = -1;
  for (int r = 0; r < rows->count() && stored < 0; r++)
    if (rows->value(r, QStringLiteral("da")).toInt() == int(btui::DaState::Stored))
      stored = r;
  QVERIFY(stored >= 0);
  s->selectRow(stored);
  QVERIFY(s->canMovements());
  s->toggleExplorer(QStringLiteral("movements"));
  QCOMPARE(s->explorer(), QStringLiteral("movements"));
  s->selectRow(stored == 0 ? 1 : 0);
  QVERIFY(!s->canMovements());
  QVERIFY(s->explorer().isEmpty());
}

void TestSolver::quittingStopsTheSolverFirst() {
  // Quit while a solve runs: the solver stops, then the usual question
  SolverFixture f;
  SolverController * s = f.solver();
  DocumentController * d = f.app->document();
  QSignalSpy refused(d, &DocumentController::busyRefused);
  s->setDisassemble(true);
  s->start();
  QVERIFY(s->solving());
  d->requestQuit();
  QVERIFY(!s->solving());
  QVERIFY(!d->busy());
  QCOMPARE(refused.count(), 0);
}

void TestSolver::anUndoThatCostsSolutionsAsksOnceASession() {
  // the history keeps no solutions: an undo or redo that would drop some asks
  // first, under the session's one warning; Cancel leaves everything as it was
  SolverFixture f;
  DocumentController * d = f.app->document();
  QSignalSpy asked(d, &DocumentController::solutionsWarningRequested);
  const unsigned int shape = f.problem().getShapeIdOfPart(0);
  const unsigned int max = f.problem().getShapeMaximum(shape);
  const auto setMax = [&](unsigned int m) {
    return d->editStructure(-1, [&](puzzle_c & p) { p.getProblem(0)->setShapeMaximum(shape, m); return true; });
  };
  QVERIFY(setMax(max + 1));
  QVERIFY(setMax(max));                           // back as it was, two steps on

  d->undo();                                      // nothing at stake: no question
  d->redo();
  QCOMPARE(asked.count(), 0);
  QCOMPARE(f.problem().getShapeMaximum(shape), max);

  QVERIFY(f.solve(false));
  QCOMPARE(int(f.problem().getNumberOfSavedSolutions()), 12);
  d->undo();
  QCOMPARE(asked.count(), 1);
  QCOMPARE(f.problem().getShapeMaximum(shape), max);         // nothing yet
  d->resolveSolutionsWarning(false);
  QCOMPARE(int(f.problem().getNumberOfSavedSolutions()), 12);
  QCOMPARE(f.solver()->shownCount(), 12);
  QVERIFY(!d->solutionsWarned());

  d->undo();
  QCOMPARE(asked.count(), 2);
  d->resolveSolutionsWarning(true);
  QCOMPARE(f.problem().getShapeMaximum(shape), max + 1);
  QCOMPARE(int(f.problem().getNumberOfSavedSolutions()), 0);

  // told once: a redo, a solve and an undo that costs them just go
  d->redo();
  QVERIFY(f.solve(false));
  d->undo();
  QCOMPARE(asked.count(), 2);
  QCOMPARE(f.problem().getShapeMaximum(shape), max + 1);
  QCOMPARE(int(f.problem().getNumberOfSavedSolutions()), 0);
}
