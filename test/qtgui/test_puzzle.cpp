/* Qt Test cases for the Puzzle workspace: the problems, the result, the
 * pieces, groups and colour rules through PuzzleController, the warning before
 * a change costs solutions, the sections the card and its rail share, and the
 * scene's objects and frames.
 */
#include "test_puzzle.h"

#include "app.h"
#include "commandcontroller.h"
#include "editorcontroller.h"
#include "inspectorcontroller.h"
#include "layoutcontroller.h"
#include "offscreenrenderer.h"
#include "puzzlecontroller.h"
#include "shapesmodel.h"
#include "settingscontroller.h"
#include "statuscontroller.h"
#include "test_gpu.h"
#include "theme.h"
#include "viewportcontroller.h"

#include "../../src/lib/problem.h"
#include "../../src/lib/puzzle.h"
#include "../../src/lib/voxel.h"
#include "../../src/uicore/problemlist.h"
#include "../../src/uicore/vectorexport.h"

#include <QFile>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <memory>

namespace {

  /* an App in the Puzzle workspace with bars of 2, 3 and 4 voxels, and a
   * problem whose result is the first */
  struct PuzzleFixture {
    QTemporaryDir dir;
    std::unique_ptr<App> app;

    explicit PuzzleFixture(int shapes = 3) {
      app = std::make_unique<App>(dir.filePath(QStringLiteral("s.rc")), dir.filePath(QStringLiteral("l.rc")));
      app->document()->newDocument(int(gridType_c::GT_BRICKS));
      app->document()->editStructure(0, [&](puzzle_c & p) {
        for (int i = 0; i < shapes; i++) {
          const unsigned s = p.addShape(unsigned(i) + 2, 1, 1);
          for (int v = 0; v < i + 2; v++)
            p.getShape(s)->setState(unsigned(v), voxel_c::VX_FILLED);
        }
        return true;
      });
      app->viewport()->setViewportSize(800, 800);
      app->layout()->setWorkspace(LayoutController::Puzzle);
      puzzle()->newProblem();
      puzzle()->setResult(0);       // a new problem has none (2026-11-14)
      flush();
    }

    PuzzleController * puzzle(void) const { return app->puzzle(); }
    ViewportController * viewport(void) const { return app->viewport(); }
    const puzzle_c & doc(void) const { return app->document()->session().puzzle(); }
    const problem_c & problem(int i = 0) const { return *doc().getProblem(unsigned(i)); }

    /* the scene is built after a burst of changes */
    void flush(void) {
      QCoreApplication::sendPostedEvents();
      viewport()->frame(1);
    }

    /* a problem that holds a solution: Pelikan Burr's */
    void loadSolved(void) {
      QVERIFY(app->document()->loadPath(QStringLiteral("examples/PelikanBurr.xmpuzzle")));
      puzzle()->refresh();
      QVERIFY(problem().getNumberOfSavedSolutions() > 0);
    }

    /* a stroke that changes something: the layer of a filled voxel of the
     * selected shape, the Erase tool, pressed on it */
    QPoint aimAtAVoxel(int shape) {
      const voxel_c & v = *doc().getShape(unsigned(shape));
      EditorController * e = app->editor();
      e->setTool(EditorController::Erase);
      for (unsigned z = 0; z < v.getZ(); z++)
        for (unsigned y = 0; y < v.getY(); y++)
          for (unsigned x = 0; x < v.getX(); x++)
            if (v.getState(x, y, z) != voxel_c::VX_EMPTY) {
              e->setLayer(int(z));
              return QPoint(int(x), int(y));
            }
      return {};
    }

    QVariant object(int row, const char * role) const {
      return viewport()->sceneObjects()->value(row, QString::fromLatin1(role));
    }
    QPointF frameCentre(int row) const {
      const double half = object(row, "frameSize").toDouble() / 2;
      return { object(row, "frameX").toDouble() + half, object(row, "frameY").toDouble() + half };
    }
  };
}

void TestPuzzle::aFileWithoutProblemsSelectsNone() {
  PuzzleFixture f;
  f.app->document()->newDocument(int(gridType_c::GT_BRICKS));
  QCOMPARE(f.puzzle()->problem(), -1);
  QCOMPARE(f.puzzle()->problems()->count(), 0);
  QVERIFY(!f.puzzle()->canDeleteProblem());
  f.puzzle()->selectProblem(3);
  QCOMPARE(f.puzzle()->problem(), -1);
  QCOMPARE(f.puzzle()->statusText(), QString());
  // nothing to act on, and nothing happens
  f.puzzle()->stepPiece(0, 1);
  f.puzzle()->toggleRule(1, 1);
  f.puzzle()->deleteProblem(0);
  QVERIFY(!f.app->document()->modified());
}

void TestPuzzle::theListsFollowTheProblem() {
  PuzzleFixture f;
  PuzzleController * z = f.puzzle();
  QCOMPARE(z->problem(), 0);
  QCOMPARE(z->library()->count(), 2);           // every shape but the result
  QCOMPARE(z->pieces()->count(), 0);

  z->stepPiece(1, 1);
  QCOMPARE(z->pieces()->count(), 1);
  QCOMPARE(z->pieces()->value(0, QStringLiteral("idText")).toString(), QStringLiteral("S2"));
  QCOMPARE(z->pieces()->value(0, QStringLiteral("countText")).toString(), QStringLiteral("×1"));
  QCOMPARE(z->library()->value(0, QStringLiteral("countText")).toString(), QStringLiteral("×1"));

  z->newProblem();
  QCOMPARE(z->problem(), 1);
  QCOMPARE(z->pieces()->count(), 0);
  z->selectProblem(0);
  QCOMPARE(z->pieces()->count(), 1);
}

void TestPuzzle::problemsAreAddedCopiedAndDeletedAsUndoSteps() {
  // AC-C16-01
  PuzzleFixture f;
  PuzzleController * z = f.puzzle();
  z->renameProblem(0, QStringLiteral("first"));
  z->stepPiece(1, 2);
  z->duplicateProblem(0);
  QCOMPARE(z->problems()->count(), 2);
  QCOMPARE(z->problem(), 1);
  QCOMPARE(z->problems()->value(1, QStringLiteral("label")).toString(), QStringLiteral("first"));
  QCOMPARE(btui::pieceCount(f.doc(), 1, 1).max, 2);

  z->deleteProblem(1);
  QCOMPARE(z->problems()->count(), 1);
  QCOMPARE(z->problem(), 0);
  QVERIFY(!z->canDeleteProblem());
  z->deleteProblem(0);
  QCOMPARE(z->problems()->count(), 1);

  f.app->document()->undo();
  QCOMPARE(z->problems()->count(), 2);
  f.app->document()->undo();
  QCOMPARE(z->problems()->count(), 1);
}

void TestPuzzle::aMoveKeepsTheSelectionOnItsProblem() {
  PuzzleFixture f;
  PuzzleController * z = f.puzzle();
  z->newProblem();
  z->newProblem();
  z->renameProblem(0, QStringLiteral("a"));
  z->renameProblem(1, QStringLiteral("b"));
  z->renameProblem(2, QStringLiteral("c"));

  z->selectProblem(0);
  z->moveProblem(0, 2);                          // b c a
  QCOMPARE(z->problem(), 2);
  z->selectProblem(0);                           // b
  z->moveProblem(2, 0);                          // a b c
  QCOMPARE(z->problem(), 1);
  QCOMPARE(z->problems()->value(1, QStringLiteral("label")).toString(), QStringLiteral("b"));
}

void TestPuzzle::theLibraryReadsTheSelectedProblem() {
  // every problem its own counts, in the same words as its tiles
  PuzzleFixture f;
  PuzzleController * z = f.puzzle();
  z->setRange(1, 0, 2);
  z->newProblem();
  z->setCount(2, 3);
  const auto countOf = [&](int shape) {
    for (int i = 0; i < z->library()->count(); i++)
      if (z->library()->value(i, QStringLiteral("shape")).toInt() == shape)
        return z->library()->value(i, QStringLiteral("countText")).toString();
    return QStringLiteral("absent");
  };
  QCOMPARE(countOf(1), QString());
  QCOMPARE(countOf(2), QStringLiteral("×3"));
  z->selectProblem(0);
  QCOMPARE(countOf(1), QStringLiteral("0–2"));
  QCOMPARE(countOf(2), QString());
  QCOMPARE(z->pieces()->value(0, QStringLiteral("countText")).toString(), countOf(1));
  f.app->document()->undo();                     // the second count, of the other problem
  z->selectProblem(1);
  QCOMPARE(countOf(2), QString());
}

void TestPuzzle::aNewProblemHasNoResultUntilOneIsChosen() {
  // product decision 2026-11-14: no shape is taken for the result; the row names it by code and label
  PuzzleFixture f;
  PuzzleController * z = f.puzzle();
  z->newProblem();
  RowsModel * goal = z->goal();
  QCOMPARE(goal->value(0, QStringLiteral("shape")).toInt(), -1);
  QCOMPARE(goal->value(0, QStringLiteral("name")).toString(), QStringLiteral("Please choose a shape"));
  QVERIFY(!f.problem(1).resultValid());
  QCOMPARE(z->library()->count(), 3);            // every shape is a piece to choose from

  z->setResult(1);
  QCOMPARE(goal->value(0, QStringLiteral("name")).toString(), QStringLiteral("S2"));
  f.app->document()->editStructure(-1, [](puzzle_c & p) { p.getShape(1)->setName("Bar"); return true; });
  z->refresh();
  QCOMPARE(goal->value(0, QStringLiteral("name")).toString(), QStringLiteral("S2 Bar"));
}

void TestPuzzle::theResultIsNoPiece() {
  // AC-C16-02
  PuzzleFixture f;
  PuzzleController * z = f.puzzle();
  z->stepPiece(2, 1);
  z->setResult(2);
  QCOMPARE(int(f.problem().getResultId()), 2);
  QCOMPARE(z->pieces()->count(), 0);
  QCOMPARE(z->goal()->value(0, QStringLiteral("idText")).toString(), QStringLiteral("S3"));
  QCOMPARE(z->library()->count(), 2);
  QCOMPARE(z->library()->value(0, QStringLiteral("idText")).toString(), QStringLiteral("S1"));

  // selected, the result has no card: only a hint
  z->selectPiece(2);
  QVERIFY(!z->pieceShown());
  z->selectPiece(0);
  QVERIFY(z->pieceShown());
}

void TestPuzzle::countsStepAndLeaveAtZero() {
  // AC-C16-03, T-C16-3
  PuzzleFixture f;
  PuzzleController * z = f.puzzle();
  for (int i = 0; i < 3; i++)
    z->stepPiece(1, 1);
  QCOMPARE(btui::pieceCount(f.doc(), 0, 1).max, 3);
  for (int i = 0; i < 4; i++)
    z->stepPiece(1, -1);
  QCOMPARE(z->pieces()->count(), 0);

  z->stepPiece(1, 1);
  z->setCount(1, 0);
  QCOMPARE(z->pieces()->count(), 0);
  QVERIFY(f.app->document()->canUndo());
}

void TestPuzzle::aRangeStaysOnShowWhileItsNumbersMeet() {
  // AC-C17-01, AC-C17-02
  PuzzleFixture f;
  PuzzleController * z = f.puzzle();
  z->selectPiece(1);
  z->stepPiece(1, 2);
  QVERIFY(!z->pieceRange());

  z->setRangeOn(1, true);
  QVERIFY(z->pieceRange());
  QCOMPARE(z->pieceMin(), 0);
  QCOMPARE(z->pieceMax(), 2);

  // Min stepped up to Max: still two steppers, not the fixed editor
  z->setRange(1, 2, 2);
  QCOMPARE(z->pieceMin(), 2);
  QVERIFY(z->pieceRange());

  z->setRange(1, 1, 2);
  z->setRangeOn(1, false);
  QVERIFY(!z->pieceRange());
  QCOMPARE(z->pieceMin(), 2);
  QCOMPARE(z->pieceMax(), 2);

  // min <= max, whichever moved
  z->setRangeOn(1, true);
  z->setRange(1, 5, 2);
  QCOMPARE(z->pieceMax(), 5);
}

void TestPuzzle::theSummaryAndStatusSayTheTotals() {
  // AC-C16-06, T-C16-6: bars of 2, 3 and 4 voxels, the result the 2
  PuzzleFixture f;
  PuzzleController * z = f.puzzle();
  z->setResult(2);                               // 4 voxels
  z->stepPiece(1, 1);                            // 3
  f.app->status()->endFlash();                   // "Problem P1 added", from the fixture
  QVERIFY(!z->fits());
  QCOMPARE(z->summaryTitle(), QStringLiteral("Pieces cannot reach the result"));
  QVERIFY(z->summaryDetail().contains(QStringLiteral("Holes: 1")));
  QCOMPARE(f.app->status()->text(),
           QStringLiteral("Problem <b>P1</b> result can contain 4 voxels, pieces (n = 1) contain 3 voxels"));

  z->stepPiece(0, 1);                            // 2 more
  QCOMPARE(z->summaryTitle(), QStringLiteral("Pieces are too large for the result"));
  QVERIFY(z->summaryDetail().contains(QStringLiteral("Pieces exceed the result by 1")));

  z->setRange(1, 0, 1);
  QVERIFY(z->fits());
  QVERIFY(z->summaryDetail().contains(QStringLiteral("Holes depend on the counts")));
  QCOMPARE(f.app->status()->text(),
           QStringLiteral("Problem <b>P1</b> result can contain 4 voxels, pieces (n = 1 - 2) contain 2 - 5 voxels"));
}

void TestPuzzle::aPieceCanBeInSeveralGroups() {
  PuzzleFixture f;
  PuzzleController * z = f.puzzle();
  z->selectPiece(1);
  z->stepPiece(1, 2);
  z->stepPiece(2, 1);

  z->newGroup(1);
  z->newGroup(1);
  z->setGroupCount(2, 2, 1);
  QCOMPARE(z->groups()->count(), 2);
  QCOMPARE(z->pieceGroups()->count(), 2);
  QCOMPARE(z->pieceGroups()->value(0, QStringLiteral("count")).toInt(), 2);
  QCOMPARE(z->pieces()->value(0, QStringLiteral("groupText")).toString(), QStringLiteral("G1, G2"));
  QCOMPARE(z->pieceGroupCount(1, 2), 2);
  QCOMPARE(z->copiesOf(1), 2);

  z->setGroupCount(1, 1, 0);
  QCOMPARE(z->pieces()->value(0, QStringLiteral("groupText")).toString(), QStringLiteral("G2"));
  QCOMPARE(z->groups()->count(), 1);

  z->deleteGroup(2);
  QCOMPARE(z->groups()->count(), 0);
  QCOMPARE(z->sections()->value(1, QStringLiteral("railText")).toString(), QStringLiteral("No groups in this problem"));
  QCOMPARE(z->sections()->value(1, QStringLiteral("summary")).toString(), QString());   // the open section says so
}

void TestPuzzle::theColourMatrixHasNoDefault() {
  // OQ-22; AC-C17-04
  PuzzleFixture f;
  PuzzleController * z = f.puzzle();
  QCOMPARE(z->colours()->count(), 0);
  QCOMPARE(z->rules()->count(), 0);

  f.app->document()->edit(puzzleHistory_c::AK_COLOR_PALETTE, 0, [](puzzle_c & p) {
    p.addColor(10, 20, 30);
    p.addColor(40, 50, 60);
    return true;
  });
  QCOMPARE(z->colours()->count(), 2);
  QCOMPARE(z->colours()->value(0, QStringLiteral("name")).toString(), QStringLiteral("C1"));
  QCOMPARE(z->rules()->count(), 4);

  z->toggleRule(1, 2);
  QCOMPARE(z->rules()->value(1, QStringLiteral("allowed")).toBool(), true);
  QCOMPARE(z->rules()->value(2, QStringLiteral("allowed")).toBool(), false);   // the pair has a direction
  z->setRulePreset(1);
  QCOMPARE(z->rules()->value(2, QStringLiteral("allowed")).toBool(), true);
  z->setRulePreset(2);
  QCOMPARE(z->rules()->value(0, QStringLiteral("allowed")).toBool(), false);
  z->setRulePreset(0);
  QCOMPARE(z->rules()->value(0, QStringLiteral("allowed")).toBool(), true);
  QCOMPARE(z->rules()->value(1, QStringLiteral("allowed")).toBool(), false);
}

void TestPuzzle::theRailsListComeFromTheSectionTable() {
  PuzzleFixture f;
  PuzzleController * z = f.puzzle();
  QCOMPARE(z->sections()->count(), 3);
  const QStringList titles = { QStringLiteral("Selected piece"), QStringLiteral("Groups"), QStringLiteral("Colour rules") };
  for (int i = 0; i < 3; i++)
    QCOMPARE(z->sections()->value(i, QStringLiteral("title")).toString(), titles[i]);

  // a rail entry opens the card, and the section in it
  f.app->layout()->setRightCollapsed(true);
  z->setSectionOpen(QStringLiteral("rules"), false);
  QCOMPARE(z->sections()->value(2, QStringLiteral("open")).toBool(), false);
  QSignalSpy requested(z, &PuzzleController::sectionRequested);
  z->showSection(QStringLiteral("rules"));
  QVERIFY(!f.app->layout()->rightCollapsed());
  QCOMPARE(z->sections()->value(2, QStringLiteral("open")).toBool(), true);
  QCOMPARE(requested.count(), 1);
  QCOMPARE(requested.first().first().toString(), QStringLiteral("rules"));
}

namespace {
  /* the undo history's memory budget for one test, back to what it was after */
  struct SolutionBudget {
    size_t was = puzzleHistory_c::solutionBudget();
    explicit SolutionBudget(size_t bytes) { puzzleHistory_c::setSolutionBudget(bytes); }
    ~SolutionBudget(void) { puzzleHistory_c::setSolutionBudget(was); }
  };
}

void TestPuzzle::aPuzzleChangeKeepsItsSolutionsForUndo() {
  PuzzleFixture f;
  f.loadSolved();
  PuzzleController * z = f.puzzle();
  DocumentController * d = f.app->document();
  const int shape = int(f.problem().getShapeIdOfPart(0));
  const int before = btui::pieceCount(f.doc(), 0, shape).max;
  const int solutions = int(f.problem().getNumberOfSavedSolutions());
  QSignalSpy asked(d, &DocumentController::solutionsWarningRequested);

  // a count change drops them into its undo step, without a word (2026-11-14)
  z->stepPiece(shape, 1);
  QCOMPARE(asked.count(), 0);
  QCOMPARE(btui::pieceCount(f.doc(), 0, shape).max, before + 1);
  QCOMPARE(int(f.problem().getNumberOfSavedSolutions()), 0);
  QVERIFY(d->modified());
  d->undo();
  QCOMPARE(btui::pieceCount(f.doc(), 0, shape).max, before);
  QCOMPARE(int(f.problem().getNumberOfSavedSolutions()), solutions);
  QVERIFY(!d->modified());
  d->redo();
  QCOMPARE(int(f.problem().getNumberOfSavedSolutions()), 0);
  d->undo();
  QCOMPARE(int(f.problem().getNumberOfSavedSolutions()), solutions);

  // the result too
  z->setResult(shape);
  QCOMPARE(asked.count(), 0);
  QCOMPARE(int(f.problem().getNumberOfSavedSolutions()), 0);
  d->undo();
  QCOMPARE(int(f.problem().getNumberOfSavedSolutions()), solutions);
  QVERIFY(!d->solutionsWarned());
}

void TestPuzzle::cancellingTheWarningChangesNothing() {
  PuzzleFixture f;
  f.loadSolved();
  PuzzleController * z = f.puzzle();
  DocumentController * d = f.app->document();
  const int solutions = int(f.problem().getNumberOfSavedSolutions());
  const int shape = int(f.problem().getShapeIdOfPart(0));
  const SolutionBudget budget(puzzleHistory_c::solutionBytesKept());   // no room: it warns
  QSignalSpy asked(d, &DocumentController::solutionsWarningRequested);

  z->setCount(shape, 0);
  QCOMPARE(asked.count(), 1);
  QCOMPARE(asked.first().first().toBool(), true);   // too big to keep
  d->resolveSolutionsWarning(false);
  QVERIFY(btui::pieceCount(f.doc(), 0, shape).inPuzzle());
  QCOMPARE(int(f.problem().getNumberOfSavedSolutions()), solutions);
  QVERIFY(!d->modified());

  d->resolveSolutionsWarning(true);              // no question open: nothing happens
  QVERIFY(!d->modified());
  z->setCount(shape, 0);                         // it asks again, every time
  QCOMPARE(asked.count(), 2);
  d->resolveSolutionsWarning(true);              // going on loses them for good
  QVERIFY(!btui::pieceCount(f.doc(), 0, shape).inPuzzle());
  d->undo();
  QCOMPARE(int(f.problem().getNumberOfSavedSolutions()), 0);
  QVERIFY(d->modified());
}

void TestPuzzle::renamingAndReorderingNeverWarn() {
  PuzzleFixture f;
  f.loadSolved();
  PuzzleController * z = f.puzzle();
  const int solutions = int(f.problem().getNumberOfSavedSolutions());
  z->newProblem();
  QSignalSpy asked(f.app->document(), &DocumentController::solutionsWarningRequested);
  z->renameProblem(0, QStringLiteral("named"));
  z->moveProblem(0, 1);
  QCOMPARE(asked.count(), 0);
  QCOMPARE(int(f.problem(1).getNumberOfSavedSolutions()), solutions);
}

void TestPuzzle::deletingAProblemWithSolutionsWarns() {
  PuzzleFixture f;
  f.loadSolved();
  PuzzleController * z = f.puzzle();
  DocumentController * d = f.app->document();
  z->newProblem();
  const int count = z->problems()->count();
  QSignalSpy asked(d, &DocumentController::solutionsWarningRequested);

  z->deleteProblem(0);
  QCOMPARE(asked.count(), 1);
  QCOMPARE(z->problems()->count(), count);
  d->resolveSolutionsWarning(true);
  QCOMPARE(z->problems()->count(), count - 1);
}

void TestPuzzle::everyVoxelEditRemovesTheSolutionsOfItsProblems() {
  PuzzleFixture f;
  f.loadSolved();
  DocumentController * d = f.app->document();
  f.app->layout()->setWorkspace(LayoutController::Entities);
  const int shape = int(f.problem().getShapeIdOfPart(0));
  f.app->shapes()->select(shape);
  EditorController * e = f.app->editor();
  const QPoint cell = f.aimAtAVoxel(shape);
  const int solutions = int(f.problem().getNumberOfSavedSolutions());
  QSignalSpy asked(d, &DocumentController::solutionsWarningRequested);

  // the undo step keeps them: no warning, and undo brings them back
  QVERIFY(e->press(cell.x(), cell.y(), false, Qt::NoModifier));
  e->dragTo(cell.x() + 1, cell.y());
  e->release();
  QCOMPARE(asked.count(), 0);
  QCOMPARE(int(f.problem().getNumberOfSavedSolutions()), 0);
  QVERIFY(d->modified());
  d->undo();
  QCOMPARE(int(f.problem().getNumberOfSavedSolutions()), solutions);
  QVERIFY(!d->modified());
  QVERIFY(!d->solutionsWarned());
}

void TestPuzzle::solutionsTooBigToKeepWarnEveryTime() {
  PuzzleFixture f;
  f.loadSolved();
  DocumentController * d = f.app->document();
  f.app->layout()->setWorkspace(LayoutController::Entities);
  const int shape = int(f.problem().getShapeIdOfPart(0));
  f.app->shapes()->select(shape);
  EditorController * e = f.app->editor();
  const QPoint cell = f.aimAtAVoxel(shape);
  const int solutions = int(f.problem().getNumberOfSavedSolutions());
  const SolutionBudget budget(puzzleHistory_c::solutionBytesKept());   // no room
  d->setSolutionsWarned(true);                   // that warning is not this one
  QSignalSpy asked(d, &DocumentController::solutionsWarningRequested);

  // a stroke: the press is held, nothing starts
  QVERIFY(e->press(cell.x(), cell.y(), false, Qt::NoModifier));
  e->dragTo(cell.x() + 1, cell.y());
  e->release();
  QCOMPARE(asked.count(), 1);
  QCOMPARE(asked.first().first().toBool(), true);
  QCOMPARE(int(f.problem().getNumberOfSavedSolutions()), solutions);
  QVERIFY(!d->modified());
  d->resolveSolutionsWarning(false);             // cancelled: nothing changed
  QCOMPARE(int(f.problem().getNumberOfSavedSolutions()), solutions);
  QVERIFY(!d->modified());

  // asked again, every time; going on applies the cell and loses them for good
  QVERIFY(e->press(cell.x(), cell.y(), false, Qt::NoModifier));
  e->release();
  QCOMPARE(asked.count(), 2);
  d->resolveSolutionsWarning(true);
  QCOMPARE(int(f.problem().getNumberOfSavedSolutions()), 0);
  QVERIFY(d->canUndo());
  d->undo();
  QCOMPARE(int(f.problem().getNumberOfSavedSolutions()), 0);
  QVERIFY(d->modified());                        // the file still has them

  f.loadSolved();
  f.app->shapes()->select(shape);
  const QPoint again = f.aimAtAVoxel(shape);
  QVERIFY(e->press(again.x(), again.y(), false, Qt::NoModifier));
  e->release();
  QCOMPARE(asked.count(), 3);
  d->resolveSolutionsWarning(false);
}

void TestPuzzle::inspectorGridAndClickEditsRemoveSolutions_data() {
  QTest::addColumn<int>("kind");
  QTest::newRow("grid size") << 0;
  QTest::newRow("inspector") << 1;
  QTest::newRow("3D click") << 2;
}

void TestPuzzle::inspectorGridAndClickEditsRemoveSolutions() {
  QFETCH(int, kind);
  PuzzleFixture f;
  f.loadSolved();
  DocumentController * d = f.app->document();
  f.app->layout()->setWorkspace(LayoutController::Entities);
  const int shape = int(f.problem().getShapeIdOfPart(0));
  f.app->shapes()->select(shape);
  const int solutions = int(f.problem().getNumberOfSavedSolutions());
  QSignalSpy asked(d, &DocumentController::solutionsWarningRequested);
  EditorController * e = f.app->editor();
  switch (kind) {
    case 0:
      e->setSize(0, e->size(0) + 1);
      break;
    case 1:
      // the first operation that changes this shape (a flip of a symmetric one does not)
      for (int task = 0; task < 24 && !d->modified(); task++)
        f.app->inspector()->run(task);
      break;
    case 2: {
      const voxel_c & v = *f.doc().getShape(unsigned(shape));
      btui::Target3d t;
      t.source = btui::TargetSource::Face;
      for (unsigned z = 0; z < v.getZ(); z++)
        for (unsigned y = 0; y < v.getY(); y++)
          for (unsigned x = 0; x < v.getX(); x++)
            if (v.getState(x, y, z) != voxel_c::VX_EMPTY)
              t.cell = { int(x), int(y), int(z) };
      e->setEditMode(true);
      e->setTool(EditorController::Erase);
      QVERIFY(e->click3d(t, Qt::NoModifier));
      break;
    }
  }
  QVERIFY(d->modified());
  QCOMPARE(int(f.problem().getNumberOfSavedSolutions()), 0);
  QCOMPARE(asked.count(), 0);                    // its undo step keeps them
  d->undo();
  QCOMPARE(int(f.problem().getNumberOfSavedSolutions()), solutions);
}

void TestPuzzle::aShapeNoSolvedProblemUsesNeverWarns() {
  PuzzleFixture f;
  f.loadSolved();
  DocumentController * d = f.app->document();
  f.app->layout()->setWorkspace(LayoutController::Entities);
  f.app->shapes()->addShape();                   // no problem uses it
  const int solutions = int(f.problem().getNumberOfSavedSolutions());
  QSignalSpy asked(d, &DocumentController::solutionsWarningRequested);
  EditorController * e = f.app->editor();
  e->setTool(EditorController::Fixed);
  QVERIFY(e->press(0, 0, false, Qt::NoModifier));
  e->release();
  QCOMPARE(asked.count(), 0);
  QCOMPARE(int(f.problem().getNumberOfSavedSolutions()), solutions);
}

void TestPuzzle::undoKeepsTheSolutionsOfOtherProblems() {
  PuzzleFixture f;
  f.loadSolved();
  PuzzleController * z = f.puzzle();
  const int solutions = int(f.problem().getNumberOfSavedSolutions());
  f.app->document()->setSolutionsWarned(true);
  z->newProblem();
  z->stepPiece(0, 1);                            // the new, unsolved problem
  f.app->document()->undo();
  f.app->document()->undo();
  QCOMPARE(int(f.problem().getNumberOfSavedSolutions()), solutions);
}

void TestPuzzle::theHoleLimitShowsAndClears() {
  PuzzleFixture f;
  PuzzleController * z = f.puzzle();
  QCOMPARE(z->holeLimitText(), QString());
  f.app->document()->edit(puzzleHistory_c::AK_PROBLEM_STRUCTURAL, 0, [](puzzle_c & p) {
    p.getProblem(0)->setMaxHoles(3);
    return true;
  });
  QVERIFY(z->holeLimitText().contains(u'3'));
  z->clearHoleLimit();
  QCOMPARE(z->holeLimitText(), QString());
  QVERIFY(!f.problem().maxHolesDefined());
  f.app->document()->undo();
  QVERIFY(f.problem().maxHolesDefined());
}

void TestPuzzle::listsOnlyFollowWhileTheWorkspaceShows() {
  PuzzleFixture f;
  PuzzleController * z = f.puzzle();
  f.app->layout()->setWorkspace(LayoutController::Entities);
  f.app->document()->editStructure(0, [](puzzle_c & p) { p.addShape(2, 2, 2); return true; });
  QCOMPARE(z->library()->count(), 2);            // stale: a paint stroke must cost nothing here
  f.app->layout()->setWorkspace(LayoutController::Puzzle);
  QCOMPARE(z->library()->count(), 3);
}

void TestPuzzle::theSceneHasAnObjectPerCopy() {
  // AC-C18-01, T-C18-1
  PuzzleFixture f;
  PuzzleController * z = f.puzzle();
  z->stepPiece(1, 3);
  z->stepPiece(2, 2);
  z->setRange(2, 0, 2);
  z->selectPiece(1);
  f.flush();

  RowsModel * o = f.viewport()->sceneObjects();
  QCOMPARE(o->count(), 7);
  QCOMPARE(f.object(0, "caption").toString(), QStringLiteral("RESULT · S1"));
  QCOMPARE(f.object(1, "caption").toString(), QStringLiteral("SELECTED · S2"));
  QCOMPARE(f.object(2, "caption").toString(), QStringLiteral("S2 · 1/3"));
  QCOMPARE(f.object(4, "caption").toString(), QStringLiteral("S2 · 3/3"));
  QCOMPARE(f.object(5, "caption").toString(), QStringLiteral("S3 · 1/2 (optional)"));

  // all copies of the selected piece are framed; the others and the result are not
  QVERIFY(f.object(1, "selected").toBool());
  QVERIFY(f.object(3, "selected").toBool());
  QVERIFY(!f.object(0, "selected").toBool());
  QVERIFY(!f.object(5, "selected").toBool());

  SceneFrame fr = f.viewport()->frame(1);
  QVERIFY(fr.mesh);
  QVERIFY(fr.anchored);
  QVERIFY(fr.lines.empty());                     // AC-C18-04: no axes, no bounds
  QCOMPARE(fr.dimAxis, -1);
}

void TestPuzzle::aClickSelectsAPieceNotTheResult() {
  // AC-C18-03, T-C18-3
  PuzzleFixture f;
  PuzzleController * z = f.puzzle();
  z->stepPiece(2, 1);
  f.flush();
  ViewportController * v = f.viewport();

  const QPointF result = f.frameCentre(0);
  v->pointerPress(Qt::LeftButton, float(result.x()), float(result.y()), {});
  v->pointerRelease(Qt::LeftButton, float(result.x()), float(result.y()));
  QCOMPARE(z->selectedPiece(), -1);

  const QPointF piece = f.frameCentre(1);
  v->pointerHover(float(piece.x()), float(piece.y()), {});
  QVERIFY(f.object(1, "hovered").toBool());
  v->pointerHover(float(result.x()), float(result.y()), {});
  QVERIFY(!f.object(0, "hovered").toBool());

  v->pointerPress(Qt::LeftButton, float(piece.x()), float(piece.y()), {});
  v->pointerRelease(Qt::LeftButton, float(piece.x()), float(piece.y()));
  QCOMPARE(z->selectedPiece(), 2);

  // empty space leaves the selection
  v->pointerPress(Qt::LeftButton, 2, 2, {});
  v->pointerRelease(Qt::LeftButton, 2, 2);
  QCOMPARE(z->selectedPiece(), 2);
}

void TestPuzzle::framesStayPutWhileTheObjectsTurn() {
  // AC-C18-02, T-C18-2
  PuzzleFixture f;
  f.puzzle()->stepPiece(1, 2);
  f.puzzle()->stepPiece(2, 1);
  f.flush();
  ViewportController * v = f.viewport();
  const QPointF before = f.frameCentre(2);
  v->pointerPress(Qt::LeftButton, 100, 100, {});
  v->pointerMove(300, 180);
  v->pointerRelease(Qt::LeftButton, 300, 180);
  const QPointF after = f.frameCentre(2);
  QVERIFY(std::abs(before.x() - after.x()) < 1);
  QVERIFY(std::abs(before.y() - after.y()) < 1);
}

void TestPuzzle::panAndZoomMoveTheWholeLayout() {
  // AC-C18-02
  PuzzleFixture f;
  f.puzzle()->stepPiece(1, 2);
  f.flush();
  ViewportController * v = f.viewport();
  const QPointF a0 = f.frameCentre(1), b0 = f.frameCentre(2);
  v->panStep(40, 0);
  const QPointF a1 = f.frameCentre(1), b1 = f.frameCentre(2);
  QVERIFY(a1.x() > a0.x());
  QVERIFY(std::abs((a1.x() - a0.x()) - (b1.x() - b0.x())) < 1);

  const double gap = std::abs(b1.x() - a1.x());
  v->zoomStep(+1);
  QTRY_VERIFY(std::abs(f.frameCentre(2).x() - f.frameCentre(1).x()) > gap);
}

void TestPuzzle::theSceneDrawsEachObjectInItsFrame() {
  // AC-C18-02: anchored, each object is where its frame says, turned or not
  PuzzleFixture f;
  f.puzzle()->stepPiece(1, 2);
  f.puzzle()->stepPiece(2, 1);
  f.flush();
  OffscreenRenderer renderer{ OffscreenRenderer::Backend::Software };
  GPU_OR_SKIP(renderer.isValid());
  const auto check = [&] {
    const SceneFrame frame = f.viewport()->frame(1);
    const QImage img = renderer.render(frame, QSize(800, 800));
    QVERIFY(!img.isNull());
    for (int i = 0; i < f.viewport()->sceneObjects()->count(); i++) {
      const QPoint at = f.frameCentre(i).toPoint();
      QVERIFY2(img.pixelColor(at) != frame.clear, qPrintable(QStringLiteral("nothing drawn at object %1").arg(i)));
    }
  };
  check();
  f.viewport()->orbitStep(35, 20);
  check();
}

void TestPuzzle::everyVoxelTypeDrawsItsScene_data() {
  QTest::addColumn<int>("type");
  QTest::newRow("bricks") << int(gridType_c::GT_BRICKS);
  QTest::newRow("prisms") << int(gridType_c::GT_TRIANGULAR_PRISM);
  QTest::newRow("spheres") << int(gridType_c::GT_SPHERES);
  QTest::newRow("rhombic") << int(gridType_c::GT_RHOMBIC);
  QTest::newRow("tetra-octa") << int(gridType_c::GT_TETRA_OCTA);
}

void TestPuzzle::everyVoxelTypeDrawsItsScene() {
  // AC-C18-05, T-C18-4: each voxel type's scene drawn, every object where its frame is
  QFETCH(int, type);
  PuzzleFixture f;
  f.app->document()->newDocument(type);
  f.app->document()->editStructure(0, [&](puzzle_c & p) {
    for (unsigned k = 0; k < 2; k++) {
      const unsigned i = p.addShape(4 - k, 4 - k, 4 - k);
      voxel_c * s = p.getShape(i);
      for (unsigned z = 0; z < s->getZ(); z++)
        for (unsigned y = 0; y < s->getY(); y++)
          for (unsigned x = 0; x < s->getX(); x++)
            if (s->validCoordinate(int(x), int(y), int(z)))
              s->setState(x, y, z, voxel_c::VX_FILLED);
    }
    return true;
  });
  f.puzzle()->newProblem();
  f.puzzle()->setResult(0);
  f.puzzle()->stepPiece(1, 2);
  f.puzzle()->selectPiece(1);
  f.flush();
  QCOMPARE(f.viewport()->sceneObjects()->count(), 4);      // the result, Selected, two copies
  OffscreenRenderer renderer{ OffscreenRenderer::Backend::Software };
  GPU_OR_SKIP(renderer.isValid());
  const SceneFrame frame = f.viewport()->frame(1);
  const QImage img = renderer.render(frame, QSize(800, 800));
  QVERIFY(!img.isNull());
  for (int i = 0; i < f.viewport()->sceneObjects()->count(); i++) {
    const QPoint at = f.frameCentre(i).toPoint();
    QVERIFY2(img.pixelColor(at) != frame.clear, qPrintable(QStringLiteral("nothing drawn at object %1").arg(i)));
  }
}

void TestPuzzle::theCameraGoesBackToTheShapeInEntities() {
  PuzzleFixture f;
  f.puzzle()->stepPiece(1, 4);
  f.flush();
  f.app->layout()->setWorkspace(LayoutController::Entities);
  f.flush();
  SceneFrame fr = f.viewport()->frame(1);
  QVERIFY(!fr.anchored);
  QCOMPARE(f.viewport()->sceneObjects()->count(), 0);
}

void TestPuzzle::piecesAndProblemsKeysWorkOnlyInPuzzle() {
  PuzzleFixture f;
  CommandController * c = f.app->commands();
  PuzzleController * z = f.puzzle();
  z->selectPiece(1);
  QVERIFY(c->isEnabled(QStringLiteral("pieces.more")));
  QVERIFY(!c->isEnabled(QStringLiteral("pieces.fewer")));
  c->trigger(QStringLiteral("pieces.more"));
  QCOMPARE(z->pieceMax(), 1);
  QVERIFY(c->isEnabled(QStringLiteral("pieces.remove")));
  QCOMPARE(c->keyInScope(QStringLiteral("puzzlePieces"), Qt::Key_Plus, int(Qt::ShiftModifier)), QStringLiteral("pieces.more"));
  QCOMPARE(c->keyInScope(QStringLiteral("puzzlePieces"), Qt::Key_Minus, 0), QStringLiteral("pieces.fewer"));
  QCOMPARE(c->keyInScope(QStringLiteral("problemsList"), Qt::Key_Delete, 0), QStringLiteral("problems.delete"));

  f.app->layout()->setWorkspace(LayoutController::Entities);
  QVERIFY(!c->isEnabled(QStringLiteral("pieces.more")));
  QVERIFY(!c->isEnabled(QStringLiteral("problems.rename")));
}

void TestPuzzle::aChangeThatChangesNothingDoesNotWarn() {
  PuzzleFixture f;
  f.loadSolved();
  const SolutionBudget budget(puzzleHistory_c::solutionBytesKept());   // no room: a change warns
  QSignalSpy asked(f.app->document(), &DocumentController::solutionsWarningRequested);

  // the problem's rules are as they are: each colour on itself or nothing to change
  f.puzzle()->clearHoleLimit();                     // it has none
  const int shape = int(f.problem().getShapeIdOfPart(0));
  const btui::PieceCount c = btui::pieceCount(f.doc(), 0, shape);
  f.puzzle()->setCount(shape, c.max);               // the count it has
  f.puzzle()->setGroupCount(shape, 1, 0);           // in no group yet
  QCOMPARE(asked.count(), 0);
  QVERIFY(!f.app->document()->solutionsWarned());

  // the real one still does
  f.puzzle()->stepPiece(shape, 1);
  QCOMPARE(asked.count(), 1);
  f.app->document()->resolveSolutionsWarning(false);
}

void TestPuzzle::aCancelledRangeLeavesTheSwitchOff() {
  PuzzleFixture f;
  f.loadSolved();
  const SolutionBudget budget(puzzleHistory_c::solutionBytesKept());   // no room: it warns
  const int shape = int(f.problem().getShapeIdOfPart(0));
  f.puzzle()->selectPiece(shape);
  f.puzzle()->setRangeOn(shape, true);
  f.app->document()->resolveSolutionsWarning(false);
  f.puzzle()->refresh();
  QVERIFY(!f.puzzle()->pieceRange());
}

void TestPuzzle::undoAndRedoMoveTheSelection() {
  PuzzleFixture f;
  f.puzzle()->newProblem();                 // P2, selected
  f.puzzle()->selectPiece(1);
  f.puzzle()->stepPiece(1, 1);              // the step, on P2 with S2
  f.puzzle()->selectProblem(0);
  f.puzzle()->selectPiece(2);

  f.app->document()->undo();
  QCOMPARE(f.puzzle()->problem(), 1);
  QCOMPARE(f.puzzle()->selectedPiece(), 1);
  f.puzzle()->selectProblem(0);
  f.app->document()->redo();
  QCOMPARE(f.puzzle()->problem(), 1);

  // a new problem: gone on undo, selected again on redo
  f.puzzle()->selectProblem(0);
  f.puzzle()->newProblem();
  QCOMPARE(f.puzzle()->problem(), 2);
  f.app->document()->undo();
  QCOMPARE(f.puzzle()->problem(), 0);
  f.app->document()->redo();
  QCOMPARE(f.puzzle()->problem(), 2);

  // a voxel edit is no step of this workspace: the selection stays
  f.puzzle()->selectProblem(1);
  f.app->document()->editStructure(0, [](puzzle_c & p) { p.addShape(1, 1, 1); return true; });
  f.puzzle()->selectProblem(0);
  f.app->document()->undo();
  QCOMPARE(f.puzzle()->problem(), 0);
}

void TestPuzzle::undoEndsAHeldRange() {
  PuzzleFixture f;
  f.puzzle()->stepPiece(1, 1);
  f.puzzle()->selectPiece(1);
  f.puzzle()->setRangeOn(1, true);
  QVERIFY(f.puzzle()->pieceRange());
  f.app->document()->undo();
  QVERIFY(!f.puzzle()->pieceRange());
  QCOMPARE(f.puzzle()->pieceMin(), 1);
}

void TestPuzzle::theSelectionFollowsItsShape() {
  PuzzleFixture f(5);
  f.puzzle()->stepPiece(3, 1);
  f.puzzle()->selectPiece(3);
  ShapesModel * shapes = f.app->shapes();

  shapes->deleteShape(1);                   // before it: it moves up
  QCOMPARE(f.puzzle()->selectedPiece(), 2);
  shapes->moveShape(2, 0);                  // itself
  QCOMPARE(f.puzzle()->selectedPiece(), 0);
  shapes->moveShape(1, 3);                  // past it: no change
  QCOMPARE(f.puzzle()->selectedPiece(), 0);
  shapes->moveShape(3, 0);                  // to before it
  QCOMPARE(f.puzzle()->selectedPiece(), 1);
  shapes->deleteShape(1);                   // itself: nothing selected
  QCOMPARE(f.puzzle()->selectedPiece(), -1);
}

void TestPuzzle::undoOfAShapeDeleteBringsTheSelectionBack() {
  PuzzleFixture f(5);
  f.puzzle()->selectPiece(3);
  f.app->shapes()->deleteShape(1);
  QCOMPARE(f.puzzle()->selectedPiece(), 2);
  f.app->document()->undo();
  QCOMPARE(f.puzzle()->selectedPiece(), 3);
  f.app->document()->redo();
  QCOMPARE(f.puzzle()->selectedPiece(), 2);
}

void TestPuzzle::aHeldStepperIsOneUndoStep() {
  PuzzleFixture f;
  f.puzzle()->stepPiece(2, 1);              // an edit that must stay reachable
  QTest::qWait(puzzleHistory_c::PROBLEM_COUNT_COALESCE_MS + 50);
  for (int i = 0; i < 40; i++)
    f.puzzle()->stepPiece(1, 1);
  QCOMPARE(f.puzzle()->copiesOf(1), 40);
  f.app->document()->undo();
  QCOMPARE(f.puzzle()->copiesOf(1), 0);
  QCOMPARE(f.puzzle()->copiesOf(2), 1);
  f.app->document()->undo();
  QCOMPARE(f.puzzle()->copiesOf(2), 0);
}

void TestPuzzle::aTypedNumberDoesNotLowerTheOtherEndForGood() {
  PuzzleFixture f;
  f.puzzle()->selectPiece(1);
  f.puzzle()->setRange(1, 3, 20);
  QTest::qWait(puzzleHistory_c::PROBLEM_COUNT_COALESCE_MS + 50);

  // 15 typed into Max with a pause after the 1
  f.puzzle()->holdCounts(1);
  f.puzzle()->setPieceMax(1, 1);
  QCOMPARE(f.puzzle()->pieceMin(), 1);
  QVERIFY(f.puzzle()->pieceRange());        // Max stays on show: it is in use
  f.puzzle()->setPieceMax(1, 15);
  QCOMPARE(f.puzzle()->pieceMin(), 3);
  QCOMPARE(f.puzzle()->pieceMax(), 15);
  f.puzzle()->releaseCounts();

  // one undo step takes the whole number back
  f.app->document()->undo();
  QCOMPARE(f.puzzle()->pieceMin(), 3);
  QCOMPARE(f.puzzle()->pieceMax(), 20);

  // with the typing over, the other end gives way
  f.puzzle()->setPieceMax(1, 2);
  QCOMPARE(f.puzzle()->pieceMin(), 2);
}

void TestPuzzle::anEditWaitingForAnAnswerGoesWithTheDocument() {
  PuzzleFixture f;
  f.loadSolved();
  const int shape = int(f.problem().getShapeIdOfPart(0));
  f.puzzle()->stepPiece(shape, 1);          // the warning holds it
  f.app->document()->newDocument(int(gridType_c::GT_BRICKS));
  f.app->document()->resolveSolutionsWarning(true);
  QCOMPARE(f.doc().getNumberOfProblems(), 0u);
  QVERIFY(!f.app->document()->session().isModified());
}

void TestPuzzle::twoEditsOfOneGestureShareTheWarning() {
  PuzzleFixture f;
  f.loadSolved();
  const SolutionBudget budget(puzzleHistory_c::solutionBytesKept());   // no room: it warns
  QSignalSpy asked(f.app->document(), &DocumentController::solutionsWarningRequested);
  const int a = int(f.problem().getShapeIdOfPart(0)), b = int(f.problem().getShapeIdOfPart(1));
  const int copiesA = f.puzzle()->copiesOf(a), copiesB = f.puzzle()->copiesOf(b);
  f.puzzle()->stepPiece(a, 1);
  f.puzzle()->stepPiece(b, 1);
  QCOMPARE(asked.count(), 1);
  f.app->document()->resolveSolutionsWarning(true);
  QCOMPARE(f.puzzle()->copiesOf(a), copiesA + 1);
  QCOMPARE(f.puzzle()->copiesOf(b), copiesB + 1);
}

void TestPuzzle::aVectorExportFromPuzzleSpreadsTheObjects() {
  PuzzleFixture f;
  f.puzzle()->stepPiece(1, 1);
  f.puzzle()->stepPiece(2, 1);
  f.flush();
  QTemporaryDir dir;
  const QString path = dir.filePath(QStringLiteral("scene.svg"));
  QVERIFY(f.viewport()->exportVector(QUrl::fromLocalFile(path), int(btui::VectorFormat::SVG)));
  QFile file(path);
  QVERIFY(file.open(QIODevice::ReadOnly));
  const QString svg = QString::fromUtf8(file.readAll());

  // the first coordinate of each polygon: the objects stand apart on the page
  QList<double> xs;
  static const QRegularExpression point(QStringLiteral("points=\"([0-9.]+),"));
  for (auto it = point.globalMatch(svg); it.hasNext(); )
    xs << it.next().captured(1).toDouble();
  QVERIFY(!xs.isEmpty());
  const auto [low, high] = std::minmax_element(xs.begin(), xs.end());
  QVERIFY2(*high - *low > 200, qPrintable(QStringLiteral("x runs from %1 to %2").arg(*low).arg(*high)));
}

void TestPuzzle::theTopCountStops() {
  PuzzleFixture f;
  f.puzzle()->setCount(1, btui::maxPieceCount);
  QCOMPARE(f.puzzle()->copiesOf(1), btui::maxPieceCount);
  const bool couldUndo = f.app->document()->session().canUndo();

  f.puzzle()->stepPiece(1, 1);
  QCOMPARE(f.puzzle()->copiesOf(1), btui::maxPieceCount);
  QCOMPARE(f.app->document()->session().canUndo(), couldUndo);

  f.puzzle()->refresh();
  QVERIFY(f.puzzle()->summaryDetail().contains(QString::number(btui::maxPieceCount)));
  QVERIFY(!f.puzzle()->summaryDetail().contains(QLatin1Char('-')));
}

void TestPuzzle::anOddValueChangesNothing() {
  PuzzleFixture f;
  const auto rules = f.problem().getColorConstraints();
  f.puzzle()->setRulePreset(7);
  QCOMPARE(f.problem().getColorConstraints(), rules);
  f.puzzle()->setPiecesView(QStringLiteral("cards"));
  QCOMPARE(f.puzzle()->piecesView(), QStringLiteral("chips"));
}

void TestPuzzle::shapeColoursDefaultToMutedAndSwitchBack() {
  PuzzleFixture f;
  SettingsController * s = f.app->settings();
  QCOMPARE(s->shapeColours(), QStringLiteral("muted"));
  QVERIFY(ShapesModel::chipColor(2) != QColor(255, 0, 0));

  s->setShapeColours(QStringLiteral("classic"));
  QCOMPARE(ShapesModel::chipColor(2), QColor(255, 0, 0));
  QCOMPARE(ShapesModel::chipColor(0), QColor(0, 0, 255));

  // an unknown word is the default, as in every setting of its kind
  s->setShapeColours(QStringLiteral("neon"));
  QCOMPARE(s->shapeColours(), QStringLiteral("muted"));

  // a reset of its page brings the default back
  s->setShapeColours(QStringLiteral("classic"));
  s->resetSection(QStringLiteral("general"));
  QCOMPARE(s->shapeColours(), QStringLiteral("muted"));
  QVERIFY(ShapesModel::chipColor(2) != QColor(255, 0, 0));
}

void TestPuzzle::everyViewFollowsTheShapeColours() {
  PuzzleFixture f;
  f.puzzle()->stepPiece(1, 1);
  f.puzzle()->selectPiece(1);
  f.flush();
  ShapesModel * shapes = f.app->shapes();
  QSignalSpy rows(shapes, &QAbstractItemModel::dataChanged);
  QSignalSpy scene(f.viewport(), &ViewportController::sceneChanged);

  const auto sceneColours = [&] {
    QSet<quint32> seen;
    const SceneFrame frame = f.viewport()->frame(1);
    for (const auto * list : { &frame.mesh->opaque, &frame.mesh->translucent })
      for (const btui::MeshVertex & v : *list)
        seen.insert(quint32(v.color.r) << 16 | quint32(v.color.g) << 8 | v.color.b);
    return seen;
  };
  const QColor mutedChip = f.puzzle()->pieces()->value(0, QStringLiteral("chipColor")).value<QColor>();
  const QSet<quint32> muted = sceneColours();

  f.app->settings()->setShapeColours(QStringLiteral("classic"));
  QCoreApplication::sendPostedEvents();
  f.flush();

  QVERIFY(rows.count() >= 1);
  QVERIFY(scene.count() >= 1);
  const QColor classicChip = f.puzzle()->pieces()->value(0, QStringLiteral("chipColor")).value<QColor>();
  QVERIFY(classicChip != mutedChip);
  QCOMPARE(classicChip, ShapesModel::chipColor(1));
  QCOMPARE(f.puzzle()->pieceColor(), classicChip);
  QVERIFY(sceneColours() != muted);
}

void TestPuzzle::theThemePicksItsMutedSet() {
  PuzzleFixture f;
  f.app->settings()->setTheme(QStringLiteral("light"));
  const QColor light = ShapesModel::chipColor(2);
  QSignalSpy spy(Theme::instance(), &Theme::shapeColoursChanged);
  f.app->settings()->setTheme(QStringLiteral("dark"));
  QCOMPARE(spy.count(), 1);
  QVERIFY(ShapesModel::chipColor(2) != light);
  QVERIFY(ShapesModel::chipColor(2).lightnessF() > light.lightnessF());

  // classic does not depend on the theme
  f.app->settings()->setShapeColours(QStringLiteral("classic"));
  const QColor classic = ShapesModel::chipColor(2);
  f.app->settings()->setTheme(QStringLiteral("light"));
  QCOMPARE(ShapesModel::chipColor(2), classic);
}

void TestPuzzle::removeAllPiecesEmptiesTheProblemInOneStep() {
  PuzzleFixture f;
  f.puzzle()->setCount(1, 2);
  f.puzzle()->setCount(2, 1);
  QVERIFY(f.puzzle()->pieces()->count() > 0);
  f.puzzle()->removeAllPieces();
  QCOMPARE(f.puzzle()->pieces()->count(), 0);
  f.app->document()->undo();
  QVERIFY(f.puzzle()->pieces()->count() > 0);
}

void TestPuzzle::theMinimumAloneMovesTheRangesFloor() {
  PuzzleFixture f;
  f.puzzle()->setRange(1, 1, 3);
  f.puzzle()->setPieceMin(1, 2);
  f.puzzle()->selectPiece(1);
  QCOMPARE(f.puzzle()->pieceMin(), 2);
  QCOMPARE(f.puzzle()->pieceMax(), 3);
  // above the top it lifts the top with it
  f.puzzle()->setPieceMin(1, 5);
  QCOMPARE(f.puzzle()->pieceMin(), 5);
  QCOMPARE(f.puzzle()->pieceMax(), 5);
}

void TestPuzzle::allPiecesOpensAndShuts() {
  PuzzleFixture f;
  QSignalSpy view(f.puzzle(), &PuzzleController::viewChanged);
  const bool was = f.puzzle()->allPiecesOpen();
  f.puzzle()->setAllPiecesOpen(!was);
  QCOMPARE(f.puzzle()->allPiecesOpen(), !was);
  f.puzzle()->setAllPiecesOpen(!was);      // the same again: no signal
  QCOMPARE(view.count(), 1);
}
