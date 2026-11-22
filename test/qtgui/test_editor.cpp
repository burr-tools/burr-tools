/* Qt Test cases for the voxel editor: the controller's strokes, tools,
 * planes and sizes, the composed edit cursors, and the 2D grid item's hit
 * testing, painting and pointer input.
 */
#include "test_editor.h"

#include "app.h"
#include "coloursmodel.h"
#include "editcursor.h"
#include "systempointer.h"
#include "iconprovider.h"
#include "editorcontroller.h"
#include "settingscontroller.h"
#include "statuscontroller.h"
#include "theme.h"
#include "viewportcontroller.h"
#include "voxelgriditem.h"

#include "../../src/lib/puzzle.h"
#include "../../src/lib/voxel.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QGuiApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>
#include <QTest>

#include <algorithm>
#include <memory>
#include <vector>

/* A time budget (AC-C08-08's 16 ms and the like): checked against the
 * code as built for use, only reported in a coverage or sanitizer build,
 * whose counters and checks make the hot loops several times slower than
 * any user sees them. */
#ifdef BURRTOOLS_INSTRUMENTED_BUILD
#define VERIFY_BUDGET(ms, budget)   qInfo("%.2f ms against a %.0f ms budget: not checked in an instrumented build", double(ms), double(budget))
#else
#define VERIFY_BUDGET(ms, budget)   QVERIFY2((ms) < (budget), qPrintable(QString::number(ms) + QStringLiteral(" ms")))
#endif

namespace {

  /* The quick tools' keys in Qt's terms (C06, overview/04 §6): Alt picks
   * Variable and Ctrl erases; on macOS Command picks Variable and Option
   * erases, which Qt calls Control and Alt there */
  const bool kMac = btui::currentPlatform() == btui::Platform::Mac;
  const Qt::KeyboardModifiers kVariable = kMac ? Qt::ControlModifier : Qt::AltModifier;
  const Qt::KeyboardModifiers kErase = kMac ? Qt::AltModifier : Qt::ControlModifier;

  /* an App with one empty brick shape of the given size, selected */
  struct EditorFixture {
    QTemporaryDir dir;
    std::unique_ptr<App> app;

    explicit EditorFixture(unsigned x = 4, unsigned y = 4, unsigned z = 3,
                           gridType_c::gridType type = gridType_c::GT_BRICKS) {
      app = std::make_unique<App>(dir.filePath(QStringLiteral("s.rc")), dir.filePath(QStringLiteral("l.rc")));
      app->document()->newDocument(int(type));
      app->document()->editStructure(0, [&](puzzle_c & p) { p.addShape(x, y, z); return true; });
      app->shapes()->select(0);
    }

    EditorController * editor(void) const { return app->editor(); }
    const voxel_c & shape(int i = 0) const { return *app->document()->session().puzzle().getShape(unsigned(i)); }
    int state(int x, int y, int z) const { return shape().getState(unsigned(x), unsigned(y), unsigned(z)); }
    int filled(void) const { return int(shape().countState(voxel_c::VX_FILLED)); }
    int variable(void) const { return int(shape().countState(voxel_c::VX_VARIABLE)); }

    /* click a cell of the current layer */
    void click(int col, int row, Qt::KeyboardModifiers m = Qt::NoModifier, bool right = false) {
      editor()->press(col, row, right, m);
      editor()->release();
    }
  };

  struct InputGrid : VoxelGridItem {
    using VoxelGridItem::mousePressEvent;
    using VoxelGridItem::mouseMoveEvent;
    using VoxelGridItem::mouseReleaseEvent;
    using VoxelGridItem::hoverEnterEvent;
    using VoxelGridItem::hoverMoveEvent;
    using VoxelGridItem::hoverLeaveEvent;
  };

  // a hover at an item point; with no window, the scene's point too
  void hover(InputGrid & g, QEvent::Type t, QPointF p) {
    QHoverEvent e(t, p, p, p);
    if (t == QEvent::HoverEnter)
      g.hoverEnterEvent(&e);
    else if (t == QEvent::HoverMove)
      g.hoverMoveEvent(&e);
    else
      g.hoverLeaveEvent(&e);
  }

  QMouseEvent mouse(QEvent::Type t, QPointF p, Qt::MouseButton b = Qt::LeftButton) {
    const Qt::MouseButtons held = t == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::MouseButtons(b);
    return QMouseEvent(t, p, p, t == QEvent::MouseMove ? Qt::NoButton : b, held, Qt::NoModifier);
  }
}

void TestEditor::aStrokeIsOneUndoStep() {
  // T-C07-4 / AC-C08-05: a drag across 4 cells, one undo step
  EditorFixture f;
  DocumentController * d = f.app->document();
  QSignalSpy state(d, &DocumentController::stateChanged);
  QVERIFY(f.editor()->press(0, 1, false, Qt::NoModifier));
  for (int c = 1; c < 4; c++)
    f.editor()->dragTo(c, 1);
  QCOMPARE(f.filled(), 4);                 // shown while the stroke goes on
  QVERIFY(f.editor()->stroking());
  f.editor()->release();
  QVERIFY(!f.editor()->stroking());
  QVERIFY(state.count() > 0);
  QVERIFY(d->canUndo());
  d->undo();
  QCOMPARE(f.filled(), 0);
  d->redo();
  QCOMPARE(f.filled(), 4);
  // a stroke that changes nothing is no step: undo goes back past the drag
  f.click(0, 1);
  d->undo();
  QCOMPARE(f.filled(), 0);
}

void TestEditor::anAreaIsPlacedOnReleaseAsOneStep() {
  // legacy's rubber band: the press and the drag span an area, previewed and
  // placed when the button goes up, one undo step; the right button erases
  // an area; Esc (cancelArea) or another shape drops it unplaced
  EditorFixture f(4, 4, 3);
  EditorController * e = f.editor();
  e->setAreaMode(true);
  QSignalSpy preview(e, &EditorController::previewChanged);
  QVERIFY(e->press(0, 0, false, Qt::NoModifier));
  e->dragTo(2, 1);
  QVERIFY(e->areaDragging());
  QCOMPARE(f.filled(), 0);                               // nothing until the release
  QCOMPARE(int(e->preview().size()), 6);                 // the 3D ghost shows the area
  QVERIFY(preview.count() >= 2);
  QVERIFY(f.app->status()->cursorText().contains(QStringLiteral("3 × 2")));
  e->release();
  QVERIFY(!e->areaDragging());
  QCOMPARE(f.filled(), 6);
  QCOMPARE(f.state(2, 1, 0), int(voxel_c::VX_FILLED));
  f.app->document()->undo();
  QCOMPARE(f.filled(), 0);
  f.app->document()->redo();

  // Mirror X in an area too; the right button erases what it covers
  e->setMirror(1);
  e->press(0, 3, false, Qt::NoModifier);
  e->release();
  QCOMPARE(f.filled(), 8);                               // (0,3) and its mirror (3,3)
  e->setMirror(0);
  e->press(0, 0, true, Qt::NoModifier);
  e->dragTo(3, 3);
  e->release();
  QCOMPARE(f.filled(), 0);

  // dropped: Esc, or the selection moving to another shape
  e->press(0, 0, false, Qt::NoModifier);
  e->dragTo(1, 1);
  e->cancelArea();
  QVERIFY(!e->areaDragging());
  QVERIFY(e->preview().empty());
  e->release();                                          // the button going up afterwards: nothing
  QCOMPARE(f.filled(), 0);
  f.app->shapes()->addShape();                           // selects it
  f.app->shapes()->select(0);
  e->press(0, 0, false, Qt::NoModifier);
  e->dragTo(1, 1);
  f.app->shapes()->select(1);
  QVERIFY(!e->areaDragging());
  e->release();
  QCOMPARE(f.filled(), 0);
  QCOMPARE(int(f.shape(1).countState(voxel_c::VX_FILLED)), 0);

  // Area off: a drag paints each cell it passes, as before
  e->setAreaMode(false);
  f.app->shapes()->select(0);
  e->press(0, 0, false, Qt::NoModifier);
  QCOMPARE(f.filled(), 1);                               // at once
  e->dragTo(1, 0);
  e->release();
  QCOMPARE(f.filled(), 2);
}

void TestEditor::areaModeIsRememberedAndSeededFromLegacy() {
  // legacy keeps it in .burrtools.rc ("rubberband"): the first start takes it over
  QTemporaryDir dir;
  {
    QFile legacy(dir.filePath(QStringLiteral("l.rc")));
    QVERIFY(legacy.open(QIODevice::WriteOnly));
    legacy.write("rubberband = true\n");
  }
  {
    App app(dir.filePath(QStringLiteral("s.rc")), dir.filePath(QStringLiteral("l.rc")));
    QVERIFY(app.editor()->areaMode());
    app.editor()->setAreaMode(false);
    QVERIFY(!app.settings()->areaMode());
  }
  App again(dir.filePath(QStringLiteral("s.rc")), dir.filePath(QStringLiteral("l.rc")));
  QVERIFY(!again.editor()->areaMode());
}

void TestEditor::anUndoDuringADragEndsIt() {
  // Ctrl+Z with the button still down: the stroke so far is undone, and the
  // rest of the drag paints nothing (no undo step per cell)
  EditorFixture f;
  DocumentController * d = f.app->document();
  f.editor()->press(0, 0, false, Qt::NoModifier);
  f.editor()->dragTo(1, 0);
  d->undo();
  QCOMPARE(f.filled(), 0);
  QVERIFY(!f.editor()->stroking());
  f.editor()->dragTo(2, 0);
  f.editor()->release();
  QCOMPARE(f.filled(), 0);
  d->redo();
  QCOMPARE(f.filled(), 2);
}

void TestEditor::anEditDuringADragEndsItsStroke() {
  // PR 181 review: another edit with the button still down (a shortcut's
  // weight change) ends the stroke; the rest of the drag paints nothing,
  // rather than going on as an edit per cell that merges only by time
  EditorFixture f;
  DocumentController * d = f.app->document();
  f.editor()->press(0, 0, false, Qt::NoModifier);
  f.editor()->dragTo(1, 0);
  d->editStructure(0, [](puzzle_c & p) { p.getShape(0)->setWeight(2); return true; });
  QVERIFY(!f.editor()->stroking());
  f.editor()->dragTo(2, 0);
  f.editor()->release();
  QCOMPARE(f.filled(), 2);
  d->undo();                                   // the weight
  QCOMPARE(f.filled(), 2);
  d->undo();                                   // the stroke so far, one step
  QCOMPARE(f.filled(), 0);
}

void TestEditor::theFirstStrokeAfterOpeningIsUnsavedAndUndoableAtOnce() {
  // PR 181 review: the title's * waited for the release; and with no step
  // before it, Ctrl+Z was off for the whole first drag after opening
  EditorFixture f;
  DocumentController * d = f.app->document();
  const QString file = f.dir.filePath(QStringLiteral("s.xmpuzzle"));
  d->saveAsFile(QUrl::fromLocalFile(file));
  QVERIFY(d->loadPath(file));
  f.app->shapes()->select(0);
  QVERIFY(!d->canUndo());
  QVERIFY(!d->modified());
  QSignalSpy state(d, &DocumentController::stateChanged);
  f.editor()->press(0, 0, false, Qt::NoModifier);
  QVERIFY(f.editor()->stroking());
  QVERIFY(d->modified());
  QVERIFY(d->canUndo());
  QVERIFY(!state.isEmpty());
  const qsizetype once = state.count();
  f.editor()->dragTo(1, 0);
  f.editor()->dragTo(2, 0);
  QCOMPARE(state.count(), once);               // once a stroke, not once a cell
  d->undo();
  QCOMPARE(f.filled(), 0);
  QVERIFY(!f.editor()->stroking());
  QVERIFY(!d->modified());
  f.editor()->dragTo(3, 0);
  f.editor()->release();
  QCOMPARE(f.filled(), 0);
}

void TestEditor::theRightButtonErasesWhateverTheToolAndModifiers() {
  // AC-C08-RC
  for (int tool = EditorController::Fixed; tool <= EditorController::Paint; tool++) {
    EditorFixture f;
    f.click(1, 1);
    QCOMPARE(f.filled(), 1);
    f.editor()->setTool(EditorController::Tool(tool));
    f.click(1, 1, Qt::ShiftModifier, true);
    QCOMPARE(f.filled(), 0);
  }
}

void TestEditor::theQuickToolIsCapturedAtThePress() {
  // AC-C08-10: Alt places variable voxels for the whole stroke, whatever is held later
  EditorFixture f;
  f.editor()->press(0, 0, false, kVariable);
  QCOMPARE(f.editor()->toolInEffect(), EditorController::Variable);
  f.editor()->setModifiers(kErase);
  f.editor()->dragTo(1, 0);
  f.editor()->release();
  QCOMPARE(f.variable(), 2);
  QCOMPARE(f.editor()->tool(), EditorController::Fixed);
  // Ctrl erases (Control on macOS)
  f.click(0, 0, kErase);
  QCOMPARE(f.variable(), 1);
  f.click(2, 0, Qt::ShiftModifier);
  QCOMPARE(f.filled(), 1);
}

void TestEditor::theToolInEffectFollowsTheModifiersHeldOverTheGrid() {
  EditorFixture f;
  EditorController * e = f.editor();
  QSignalSpy tool(e, &EditorController::toolChanged);
  e->setModifiers(Qt::ShiftModifier | kVariable);
  QCOMPARE(e->toolInEffect(), EditorController::Fixed);       // not over the grid: the tool
  e->hover(1, 2);
  QCOMPARE(e->toolInEffect(), EditorController::Variable);    // Alt beats Shift
  QVERIFY(e->momentary());
  QVERIFY(tool.count() > 0);
  e->setModifiers(Qt::NoModifier);
  QCOMPARE(e->toolInEffect(), EditorController::Fixed);
  e->setTool(EditorController::Paint);
  e->setModifiers(Qt::ShiftModifier);
  QCOMPARE(e->toolInEffect(), EditorController::Fixed);
  e->hover(-1, -1);
  QCOMPARE(e->toolInEffect(), EditorController::Paint);
  QVERIFY(!e->momentary());
}

void TestEditor::theHoverPreviewIsWhatAClickWouldDo() {
  // AC-C08-10 / AC-C06-11: the cell under the pointer, the tool in effect
  // with the modifiers held, Mirror and Span; none off the grid or mid-stroke
  EditorFixture f(4, 4, 3);
  EditorController * e = f.editor();
  QSignalSpy changed(e, &EditorController::previewChanged);
  QVERIFY(e->preview().empty());
  e->hover(1, 2);
  QVERIFY(changed.count() > 0);
  std::vector<btui::GhostCell> p = e->preview();
  QCOMPARE(p.size(), size_t(1));
  QVERIFY((p[0].cell == btui::Cell3{ 1, 2, 0 }));
  QVERIFY(p[0].kind == btui::GhostKind::Place);

  // Mirror X: the copy too
  changed.clear();
  e->toggleMirror(0);
  QVERIFY(changed.count() > 0);
  QCOMPARE(e->preview().size(), size_t(2));
  e->toggleMirror(0);

  // a voxel there: Ctrl held (Erase) previews erasing it, at once
  f.click(1, 2);
  e->hover(-1, -1);
  e->hover(1, 2);
  QVERIFY(e->preview().empty());              // Fixed over the same fixed voxel: no change
  changed.clear();
  e->setModifiers(kErase);
  QVERIFY(changed.count() > 0);
  p = e->preview();
  QCOMPARE(p.size(), size_t(1));
  QVERIFY(p[0].kind == btui::GhostKind::Erase);
  // Alt (Variable) would change it: replaced
  e->setModifiers(kVariable);
  QVERIFY(e->preview()[0].kind == btui::GhostKind::Replace);
  e->setModifiers(Qt::NoModifier);

  // the layer moves the cell; a stroke and leaving the grid clear it
  e->setLayer(1);
  QVERIFY((e->preview()[0].cell == btui::Cell3{ 1, 2, 1 }));
  e->press(0, 0, false, Qt::NoModifier);
  QVERIFY(e->preview().empty());
  e->release();
  changed.clear();
  e->hover(-1, -1);
  QVERIFY(changed.count() > 0);
  QVERIFY(e->preview().empty());
}

namespace {

  /* a 3x3x3 brick shape seen from +Z, framed: the view's middle (160, 120)
   * is over cell column (1, 1) */
  struct Edit3dFixture : EditorFixture {
    ViewportController * v;
    Edit3dFixture() : EditorFixture(3, 3, 3) {
      v = app->viewport();
      v->setViewportSize(320, 240);
      v->setProjection(QStringLiteral("orthographic"));
      v->frame(1);                       // the mesh built, the camera framed
      v->camera().setOrientation(btui::Camera::lookFrom({ 0, 0, 1 }, 0));
    }
    void fill(int x, int y, int z, int state = voxel_c::VX_FILLED) {
      app->document()->editStructure(0, [&](puzzle_c & p) {
        p.getShape(0)->setState(unsigned(x), unsigned(y), unsigned(z), state);
        return true;
      });
      v->frame(1);                       // the pick mesh follows
    }
    /* where the middle of a cell column (x, y) is on the view, at height z */
    QPointF at(float x, float y, float z = 1) const {
      const btui::Vec3 p = v->camera().project({ x + 0.5f, y + 0.5f, z });
      return { p.x, p.y };
    }
    void click3d(Qt::KeyboardModifiers m = Qt::NoModifier, float x = 160, float y = 120,
                 Qt::MouseButton b = Qt::LeftButton) {
      v->pointerPress(b, x, y, m);
      v->pointerRelease(b, x, y);
    }
  };
}

void TestEdit3d::aClickEditsWithAQuickToolOrInEditMode() {
  // AC-C06-06 / 07 / 12, T-C06-2
  Edit3dFixture f;
  f.fill(1, 1, 1);
  f.click3d();                                           // no modifier, no Edit mode: nothing
  QCOMPARE(f.filled(), 1);
  QVERIFY(!f.editor()->viewportAction(Qt::NoModifier).has_value());

  f.click3d(Qt::ShiftModifier);                          // Fixed on top of the voxel
  QCOMPARE(f.state(1, 1, 2), int(voxel_c::VX_FILLED));
  f.app->document()->undo();                             // one step
  QCOMPARE(f.state(1, 1, 2), int(voxel_c::VX_EMPTY));
  QCOMPARE(f.state(1, 1, 1), int(voxel_c::VX_FILLED));

  f.click3d(kVariable);                                  // Variable
  QCOMPARE(f.state(1, 1, 2), int(voxel_c::VX_VARIABLE));
  f.click3d(kErase);                                     // Erase: the voxel under the pointer
  QCOMPARE(f.state(1, 1, 2), int(voxel_c::VX_EMPTY));

  // Edit mode: a plain click applies the tool, a modifier still overrides it
  f.editor()->setEditMode(true);
  f.editor()->setTool(EditorController::Erase);
  QCOMPARE(int(*f.editor()->viewportAction(Qt::NoModifier)), int(btui::Tool::Erase));
  QCOMPARE(int(*f.editor()->viewportAction(Qt::ShiftModifier)), int(btui::Tool::Fixed));
  f.click3d(Qt::ShiftModifier);
  QCOMPARE(f.state(1, 1, 2), int(voxel_c::VX_FILLED));
  f.click3d();
  QCOMPARE(f.state(1, 1, 2), int(voxel_c::VX_EMPTY));

  // Mirror X: both copies, one step (T-C06-4)
  f.editor()->setTool(EditorController::Fixed);
  f.editor()->setMirror(1);
  const QPointF c01 = f.at(0, 1);
  f.click3d(Qt::NoModifier, float(c01.x()), float(c01.y()));   // over column (0, 1): nothing there but the slab
  f.editor()->setMirror(0);
  QCOMPARE(f.state(0, 1, 0) + f.state(2, 1, 0), 2 * int(voxel_c::VX_FILLED));
  f.app->document()->undo();                             // both, one step
  QCOMPARE(f.state(0, 1, 0) + f.state(2, 1, 0), 0);
}

void TestEdit3d::quickClicksAreAStepEach() {
  // C06 Commit: one undo step per click, however quickly they follow (not a stroke's coalescing)
  Edit3dFixture f;
  f.fill(1, 1, 1);
  f.click3d(Qt::ShiftModifier);
  f.v->frame(1);
  f.click3d(Qt::ShiftModifier);
  QCOMPARE(f.state(1, 1, 2), int(voxel_c::VX_FILLED));
  QCOMPARE(f.filled(), 2);
  f.click3d(kErase);
  QCOMPARE(f.filled(), 1);
  f.app->document()->undo();
  QCOMPARE(f.filled(), 2);
  f.app->document()->undo();
  QCOMPARE(f.filled(), 1);
  QCOMPARE(f.state(1, 1, 1), int(voxel_c::VX_FILLED));
}

void TestEdit3d::dragsAndOtherButtonsNeverEdit() {
  // AC-C06-12: past 4 dp a press navigates; middle and right never edit
  Edit3dFixture f;
  f.fill(1, 1, 1);
  f.v->pointerPress(Qt::LeftButton, 160, 120, Qt::ShiftModifier);
  f.v->pointerMove(170, 120);
  f.v->pointerRelease(Qt::LeftButton, 170, 120);
  QCOMPARE(f.filled(), 1);
  f.click3d(Qt::ShiftModifier, 160, 120, Qt::MiddleButton);
  f.click3d(Qt::ShiftModifier, 160, 120, Qt::RightButton);
  QCOMPARE(f.filled(), 1);
}

void TestEdit3d::theSlabIsA2dGridInTheView() {
  // product decision 2026-10-27: off every voxel, the active layer's slab
  // is the 2D grid -- in an empty shape too
  Edit3dFixture f;
  QVERIFY(f.v->editorVisible() && f.v->displayLayerSlab());
  f.editor()->setLayer(1);
  QCOMPARE(int(f.v->targetAt(160, 120, Qt::ShiftModifier).source), int(btui::TargetSource::Slab));
  f.click3d(Qt::ShiftModifier);
  QCOMPARE(f.state(1, 1, 1), int(voxel_c::VX_FILLED));
  // over a column with nothing in it: the slab's cell
  const QPointF c21 = f.at(2, 1);
  btui::Target3d t = f.v->targetAt(float(c21.x()), float(c21.y()), Qt::ShiftModifier);
  QCOMPARE(int(t.source), int(btui::TargetSource::Slab));
  QVERIFY((t.cell == btui::Cell3{ 2, 1, 1 }));
  // a voxel below the layer: the pointer is on it, so its face decides (2026-10-28)
  f.fill(2, 1, 0);
  f.editor()->setLayer(2);
  t = f.v->targetAt(float(c21.x()), float(c21.y()), Qt::ShiftModifier);
  QCOMPARE(int(t.source), int(btui::TargetSource::Face));
  QVERIFY((t.cell == btui::Cell3{ 2, 1, 1 }));
  f.editor()->setLayer(1);
  // the voxel in the layer: its face is the target
  QCOMPARE(int(f.v->targetAt(160, 120, Qt::ShiftModifier).source), int(btui::TargetSource::Face));
}

void TestEdit3d::anEmptyShapeWithoutTheSlabStartsAtTheOrigin() {
  Edit3dFixture f;
  f.v->setDisplayLayerSlab(false);
  QCOMPARE(int(f.v->targetAt(10, 10, Qt::ShiftModifier).source), int(btui::TargetSource::Origin));
  f.click3d(Qt::ShiftModifier, 10, 10);                  // anywhere in the view
  QCOMPARE(f.state(0, 0, 0), int(voxel_c::VX_FILLED));
  QCOMPARE(f.filled(), 1);
  // not empty any more: off the voxels, nothing
  f.v->frame(1);
  f.click3d(Qt::ShiftModifier, 10, 10);
  QCOMPARE(f.filled(), 1);
}

void TestEdit3d::aFaceWithNothingBeyondIsOutlinedNotEdited() {
  // T-C06-2: the target outside the grid -- no ghost, the face dashed in danger, no change
  Edit3dFixture f;
  f.fill(1, 1, 2);                                       // the top layer
  f.v->pointerHover(160, 120, Qt::ShiftModifier);
  QCOMPARE(int(f.editor()->target3d().source), int(btui::TargetSource::Rejected));
  const SceneFrame fr = f.v->frame(1);
  QVERIFY(fr.ghostFaces.empty());
  QCOMPARE(fr.ghostLines.size(), size_t(4));              // the square's four sides
  for (const btui::LineSeg & l : fr.ghostLines) {
    QVERIFY(l.dashDp > 0);
    QCOMPARE(l.widthDp, 2.5f);
  }
  f.click3d(Qt::ShiftModifier);
  QCOMPARE(f.filled(), 1);
}

void TestEdit3d::hoveringShowsTheGhostAndNamesTheTarget() {
  // AC-C06-08 / 10 / 11: the ghost and the cursor's tool the moment a
  // modifier is held, none without; the status line names the target
  Edit3dFixture f;
  f.fill(1, 1, 1);
  f.v->pointerHover(160, 120, Qt::NoModifier);
  QVERIFY(f.editor()->preview().empty());
  QCOMPARE(f.v->editCursorTool(), -1);
  QVERIFY(!f.editor()->quickHeld3d());

  QSignalSpy tool(f.editor(), &EditorController::toolChanged);
  f.v->pointerHover(160, 120, Qt::ShiftModifier);        // as the modifier filter reports a key
  QVERIFY(tool.count() > 0);
  QVERIFY(f.editor()->quickHeld3d());
  QCOMPARE(f.v->editCursorTool(), int(EditorController::Fixed));
  std::vector<btui::GhostCell> p = f.editor()->preview();
  QCOMPARE(p.size(), size_t(1));
  QVERIFY((p[0].cell == btui::Cell3{ 1, 1, 2 }));
  QVERIFY(!f.v->frame(1).ghostFaces.empty());
  QVERIFY(f.app->status()->cursorText().contains(QStringLiteral("X1 Y1 Z2")));

  f.v->pointerHover(160, 120, kErase);                   // Erase: the voxel itself
  p = f.editor()->preview();
  QVERIFY(p.size() == 1 && p[0].kind == btui::GhostKind::Erase && (p[0].cell == btui::Cell3{ 1, 1, 1 }));
  QCOMPARE(f.v->editCursorTool(), int(EditorController::Erase));

  // the slab's cell names its layer
  const QPointF c01 = f.at(0, 1);
  f.v->pointerHover(float(c01.x()), float(c01.y()), Qt::ShiftModifier);
  QVERIFY(f.app->status()->cursorText().contains(QStringLiteral("active layer")));

  // a drag hides the ghost; leaving clears it
  f.v->pointerPress(Qt::LeftButton, 160, 120, Qt::NoModifier);
  f.v->pointerMove(180, 120);
  QVERIFY(f.editor()->preview().empty());
  f.v->pointerRelease(Qt::LeftButton, 180, 120);
  f.v->pointerHover(160, 120, Qt::ShiftModifier);
  QVERIFY(!f.editor()->preview().empty());
  f.v->pointerLeave();
  QVERIFY(f.editor()->preview().empty());
  QVERIFY(f.v->frame(1).ghostFaces.empty());
  QVERIFY(f.app->status()->cursorText().isEmpty());
}

void TestEdit3d::theGhostTakesEachToolsLook() {
  // C06 "Ghost preview": Fixed and Variable place cubes in the drawing
  // colour at 55 %; a voxel they would change gets a faint "replace" at
  // 22 %; Erase shows danger at 50 %; Paint the drawing colour at 50 %.
  // The drawing colour is a palette colour's, or Default's -- the shape's
  Edit3dFixture f;
  f.fill(1, 1, 1);
  auto ghost = [&] {
    const SceneFrame fr = f.v->frame(1);
    return fr.ghostFaces.empty() ? btui::Rgba8{} : fr.ghostFaces.front().color;
  };
  auto rgb = [](btui::Rgba8 c) { return QColor(c.r, c.g, c.b); };

  f.v->pointerHover(160, 120, kErase);
  QCOMPARE(rgb(ghost()), Theme::instance()->danger());
  QCOMPARE(int(ghost().a), 128);

  f.v->pointerHover(160, 120, Qt::ShiftModifier);       // Default: the shape's own colour
  const btui::Rgba8 place = ghost();
  QCOMPARE(int(place.a), 140);
  QCOMPARE(rgb(place), f.app->viewport()->shapeColor());

  // a palette colour drawn: the ghost takes it
  f.app->colours()->addColour();
  f.app->colours()->select(1);
  QCOMPARE(f.editor()->drawColour(), 1);
  f.v->pointerHover(160, 120, Qt::ShiftModifier);
  QCOMPARE(rgb(ghost()), f.app->colours()->colourOf(1));
  QCOMPARE(rgb(ghost()), f.editor()->cursorColour());   // the edit cursor's, one lookup

  // Paint over the voxel, in Edit mode: its new colour at 50 %
  f.editor()->setEditMode(true);
  f.editor()->setTool(EditorController::Paint);
  f.v->pointerHover(160, 120, Qt::NoModifier);
  QCOMPARE(int(f.editor()->preview().at(0).kind), int(btui::GhostKind::Paint));
  QCOMPARE(rgb(ghost()), f.app->colours()->colourOf(1));
  QCOMPARE(int(ghost().a), 128);
  f.v->pointerLeave();
  f.editor()->setEditMode(false);

  // Variable over the fixed voxel, from the 2D grid: a faint "replace"
  f.editor()->setTool(EditorController::Variable);
  f.editor()->setLayer(1);
  f.editor()->hover(1, 1);
  QCOMPARE(int(f.editor()->preview().at(0).kind), int(btui::GhostKind::Replace));
  QCOMPARE(int(ghost().a), 56);
  f.editor()->hover(-1, -1);
  QVERIFY(f.v->frame(1).ghostFaces.empty());

  // an empty shape with the slab hidden: the status line says where the first voxel goes
  f.app->document()->editStructure(0, [](puzzle_c & p) { p.getShape(0)->setAll(voxel_c::VX_EMPTY); return true; });
  f.v->setDisplayLayerSlab(false);
  f.v->frame(1);
  f.v->pointerHover(30, 30, Qt::ShiftModifier);
  QVERIFY2(f.app->status()->cursorText().contains(QStringLiteral("origin")), qPrintable(f.app->status()->cursorText()));
  f.v->pointerLeave();
}

void TestEdit3d::theFocusedViewsKeysTurnPanAndZoom() {
  // WCAG 2.1.1: every mouse gesture of the view by keys, through the command table
  Edit3dFixture f;
  ViewportController * v = f.v;
  CommandController * c = f.app->commands();
  const btui::Quat start = v->camera().orientation();
  QCOMPARE(c->keyInScope(QStringLiteral("viewport"), Qt::Key_Left, Qt::NoModifier), QStringLiteral("view.orbitLeft"));
  QCOMPARE(c->keyInScope(QStringLiteral("viewport"), Qt::Key_Up, Qt::ShiftModifier), QStringLiteral("view.panUp"));
  QCOMPARE(c->keyInScope(QStringLiteral("viewport"), Qt::Key_Plus, Qt::NoModifier), QStringLiteral("view.zoomIn"));
  QVERIFY(c->keyInScope(QStringLiteral("viewport"), Qt::Key_O, Qt::NoModifier).isEmpty());   // the window's

  // 24 steps of 15 degrees: once round
  c->trigger(QStringLiteral("view.orbitRight"));
  QVERIFY(btui::angleBetween(start, v->camera().orientation()) > 0.2f);
  for (int i = 1; i < 24; i++)
    c->trigger(QStringLiteral("view.orbitRight"));
  QVERIFY(btui::angleBetween(start, v->camera().orientation()) < 1e-3f);
  c->trigger(QStringLiteral("view.orbitUp"));
  c->trigger(QStringLiteral("view.orbitDown"));
  QVERIFY(btui::angleBetween(start, v->camera().orientation()) < 1e-3f);

  const btui::Mat4 before = v->camera().viewMatrix();
  c->trigger(QStringLiteral("view.panLeft"));
  QVERIFY(v->camera().viewMatrix()(0, 3) != before(0, 3));
  const float zoom = v->camera().zoom();
  c->trigger(QStringLiteral("view.zoomIn"));
  while (v->tick(16)) {}
  QVERIFY(v->camera().zoom() > zoom);

  // the cube's faces and turns, animated to their end
  c->trigger(QStringLiteral("view.top"));
  while (v->tick(16)) {}
  QVERIFY(btui::angleBetween(v->camera().orientation(), btui::Camera::lookFrom({ 0, 1, 0 }, 0)) < 0.05f ||
          std::fabs(btui::rotate(v->camera().orientation(), btui::Vec3{ 0, 1, 0 }).z - 1.0f) < 1e-3f);
  c->trigger(QStringLiteral("view.front"));
  while (v->tick(16)) {}
  QVERIFY(std::fabs(btui::rotate(v->camera().orientation(), btui::Vec3{ 0, 0, 1 }).z - 1.0f) < 1e-3f);
  c->trigger(QStringLiteral("view.turnRight"));
  while (v->tick(16)) {}
  QVERIFY(std::fabs(btui::rotate(v->camera().orientation(), btui::Vec3{ 0, 0, 1 }).z) < 1e-3f);
}

void TestEdit3d::singleKeyShortcutsCanBeTurnedOff() {
  // WCAG 2.1.4: the window's one-character keys are marked, so the shell
  // can switch them with Settings ▸ General ▸ Single-key shortcuts
  Edit3dFixture f;
  QVERIFY(f.app->settings()->singleKeyShortcuts());
  QStringList single;
  for (const QVariant & e : f.app->commands()->shortcuts()) {
    const QVariantMap m = e.toMap();
    if (m.value(QStringLiteral("singleKey")).toBool())
      single << m.value(QStringLiteral("key")).toString();
  }
  single.sort();
  QCOMPARE(single, QStringList({ QStringLiteral("editor.tool.erase"), QStringLiteral("editor.tool.fixed"),
                                 QStringLiteral("editor.tool.paint"), QStringLiteral("editor.tool.variable"),
                                 QStringLiteral("view.edit3d"), QStringLiteral("view.fit"), QStringLiteral("view.orbit"),
                                 QStringLiteral("view.pan") }));
  f.app->settings()->setSingleKeyShortcuts(false);
  QVERIFY(!f.app->settings()->singleKeyShortcuts());
  // the shortcuts list says they are off
  bool said = false;
  for (const QVariant & g : f.app->commands()->shortcutHelp())
    for (const QVariant & r : g.toMap().value(QStringLiteral("rows")).toList())
      said |= r.toMap().value(QStringLiteral("action")).toString().contains(QStringLiteral("off (Settings"));
  QVERIFY(said);
  f.app->settings()->setSingleKeyShortcuts(true);
}

void TestEditor::mirrorAndSpanExpandEveryPlacement() {
  // AC-C07-03 / 04 / 06, through the controller
  EditorFixture f(4, 4, 3);
  f.editor()->toggleMirror(0);
  f.click(0, 1);
  QCOMPARE(f.state(0, 1, 0), int(voxel_c::VX_FILLED));
  QCOMPARE(f.state(3, 1, 0), int(voxel_c::VX_FILLED));
  QCOMPARE(f.filled(), 2);
  f.editor()->toggleMirror(0);
  f.editor()->toggleSpan(2);
  QCOMPARE(f.editor()->span(), 4);
  f.click(2, 2);
  QCOMPARE(f.filled(), 2 + 3);             // the whole Z line
  // and to Erase: the span removes the line again
  f.editor()->setTool(EditorController::Erase);
  f.click(2, 2);
  QCOMPARE(f.filled(), 2);
  f.editor()->setSpan(0);
  f.editor()->setMirror(9);                // only X Y Z bits
  QCOMPARE(f.editor()->mirror(), 1);
}

void TestEditor::paintUsesTheDrawingColourAndLeavesEmptyCells() {
  // T-C07-1 / AC-C07-02
  EditorFixture f;
  f.app->colours()->addColour();
  f.app->colours()->addColour();
  f.app->colours()->select(2);
  f.click(0, 0);
  QCOMPARE(int(f.shape().getColor(0, 0, 0)), 2);
  f.app->colours()->select(1);
  f.editor()->setTool(EditorController::Paint);
  f.click(0, 0);
  f.click(1, 0);
  QCOMPARE(int(f.shape().getColor(0, 0, 0)), 1);
  QCOMPARE(f.state(0, 0, 0), int(voxel_c::VX_FILLED));
  QCOMPARE(f.state(1, 0, 0), int(voxel_c::VX_EMPTY));
  QVERIFY(f.editor()->cursorColour().isValid());
  f.app->colours()->select(0);
  QVERIFY(!f.editor()->cursorColour().isValid());   // Default: the theme's tones
}

void TestEditor::theCursorReadoutNamesThePlanesAxes() {
  // T-C08-4 / AC-C08-06
  EditorFixture f(4, 3, 5);
  EditorController * e = f.editor();
  e->hover(2, 1);
  QCOMPARE(f.app->status()->cursorText(), QStringLiteral("Cursor X2 Y1 · Z0"));
  e->setPlane(EditorController::XZ);
  e->setLayer(2);
  e->hover(3, 4);
  QCOMPARE(f.app->status()->cursorText(), QStringLiteral("Cursor X3 Z4 · Y2"));
  e->setPlane(EditorController::YZ);
  e->hover(0, 1);
  QCOMPARE(f.app->status()->cursorText(), QStringLiteral("Cursor Z0 Y1 · X2"));
  e->hover(-1, -1);
  QVERIFY(f.app->status()->cursorText().isEmpty());
  // a click in YZ reaches the voxel the mapping names
  f.click(4, 2);
  QCOMPARE(f.state(2, 2, 4), int(voxel_c::VX_FILLED));
}

void TestEditor::gridSizeStepsAndIsBlockedBeforeCuttingVoxels() {
  // T-C02-1 / AC-C02-02 / 04 / 06 / 07
  EditorFixture f(4, 3, 4);
  EditorController * e = f.editor();
  QCOMPARE(e->sizeX(), 4);
  e->setSize(0, 5);
  QCOMPARE(int(f.shape().getX()), 5);
  QCOMPARE(e->sizeX(), 5);
  f.app->document()->undo();
  QCOMPARE(e->sizeX(), 4);
  f.click(3, 0);
  QSignalSpy flash(e, &EditorController::flashRequested);
  e->setSize(0, 2);
  QCOMPARE(flash.count(), 1);
  QVERIFY(flash.first().first().toString().startsWith(QStringLiteral("Blocked:")));
  QCOMPARE(int(f.shape().getX()), 4);
  // the layer follows a shrink along its axis
  e->setLayer(3);
  e->setSize(2, 2);
  QCOMPARE(e->layerCount(), 2);
  QCOMPARE(e->layer(), 1);
  // clamped to 1 ... max
  e->setSize(1, 0);
  QCOMPARE(e->sizeY(), 1);
}

void TestEditor::applyToAllResizesEveryShape() {
  // AC-C02-03
  EditorFixture f(2, 2, 2);
  f.app->document()->editStructure(0, [](puzzle_c & p) { p.addShape(5, 1, 1); return true; });
  f.app->shapes()->select(0);
  EditorController * e = f.editor();
  e->setApplyToAll(true);
  e->setSize(0, 3);
  QCOMPARE(int(f.shape(0).getX()), 3);
  QCOMPARE(int(f.shape(1).getX()), 6);
  f.app->document()->undo();                 // one step for all
  QCOMPARE(int(f.shape(1).getX()), 5);
}

void TestEditor::onlyBricksHaveTheXzAndYzPlanes() {
  // all but the prisms (product decision 2026-10-28)
  EditorFixture f(4, 4, 4);
  EditorController * e = f.editor();
  QVERIFY(e->allPlanes());
  e->setPlane(EditorController::XZ);
  QCOMPARE(e->plane(), EditorController::XZ);
  f.app->document()->newDocument(gridType_c::GT_SPHERES);
  f.app->document()->editStructure(0, [](puzzle_c & p) { p.addShape(4, 4, 4); return true; });
  f.app->shapes()->select(0);
  QVERIFY(e->allPlanes());
  e->setPlane(EditorController::YZ);
  QCOMPARE(e->plane(), EditorController::YZ);
  // a YZ click: the cell's voxel has X the layer
  e->setLayer(2);
  f.click(0, 0);
  QCOMPARE(f.state(2, 0, 0), int(voxel_c::VX_FILLED));
  // prisms: XY only, and a plane they lack falls back
  f.app->document()->newDocument(gridType_c::GT_TRIANGULAR_PRISM);
  f.app->document()->editStructure(0, [](puzzle_c & p) { p.addShape(4, 4, 4); return true; });
  f.app->shapes()->select(0);
  QVERIFY(!e->allPlanes());
  QCOMPARE(e->plane(), EditorController::XY);
  e->setPlane(EditorController::YZ);
  QCOMPARE(e->plane(), EditorController::XY);
  QVERIFY(!e->planeAvailable(EditorController::YZ));
}

void TestEditor::layerDotsFollowTheVoxels() {
  // T-C08-3 / AC-C08-02
  EditorFixture f(3, 3, 3);
  EditorController * e = f.editor();
  QCOMPARE(e->layerDots(), QList<bool>({ false, false, false }));
  QSignalSpy dots(e, &EditorController::layerDotsChanged);
  e->setLayer(2);
  f.click(1, 1);
  QVERIFY(dots.count() > 0);
  QCOMPARE(e->layerDots(), QList<bool>({ false, false, true }));
  e->setPlane(EditorController::XZ);
  QCOMPARE(e->layerDots(), QList<bool>({ false, true, false }));
  e->layerStep(+5);
  QCOMPARE(e->layer(), 2);
  e->layerStep(-1);
  QCOMPARE(e->layer(), 1);
}

void TestEditor::editsThatChangeNoGeometryKeepTheMesh() {
  // performance guide: a rename, a weight or a colour (piece view) does not rebuild the mesh
  EditorFixture f;
  ViewportController * v = f.app->viewport();
  v->setViewportSize(320, 240);
  f.click(0, 0);
  const quint64 before = v->frame(1).meshRevision;
  f.app->shapes()->renameShape(0, QStringLiteral("Arm"));
  f.app->shapes()->changeWeight(0, +1);
  f.app->colours()->addColour();
  QCOMPARE(v->frame(1).meshRevision, before);
  QCOMPARE(v->shapeLabel(), QStringLiteral("Arm"));
  v->setColourView(QStringLiteral("voxel"));
  const quint64 voxelView = v->frame(1).meshRevision;
  f.app->colours()->addColour();
  QVERIFY(v->frame(1).meshRevision != voxelView);   // the colours show in the voxel view
  const quint64 painted = v->frame(1).meshRevision;
  f.click(1, 0);
  QVERIFY(v->frame(1).meshRevision != painted);
}

void TestEditor::theCursorsAreComposedPerToolColourAndTheme() {
  // PR-20 / glyph catalog ▸ Cursors, on the bundled arrow (the fallback)
  for (qreal scale : { 1.0, 1.5, 2.0 }) {
    const auto c = editcursor::compose(btui::Tool::Fixed, QColor(Qt::red), false, scale, editcursor::Pointer::Bundled);
    QCOMPARE(c.image.width(), qRound(32 * scale));
    QCOMPARE(c.image.devicePixelRatio(), scale);
    QCOMPARE(c.hotspot, QPoint(1, 1));
  }
  // the badge's inside: the drawing colour, or for Default the theme's opposite tone
  auto inside = [](btui::Tool t, const QColor & colour, bool dark) {
    return editcursor::compose(t, colour, dark, 1, editcursor::Pointer::Bundled).image.pixelColor(15, 3);
  };
  QCOMPARE(inside(btui::Tool::Fixed, QColor(Qt::red), false), QColor(Qt::red));
  QCOMPARE(inside(btui::Tool::Fixed, QColor(), false), QColor(Qt::white));
  QVERIFY(inside(btui::Tool::Fixed, QColor(), true).lightness() < 40);
  QVERIFY(inside(btui::Tool::Variable, QColor(), false).lightness() > 240);
  // Erase and Paint: the icon alone, hotspot at its centre, something drawn there
  for (btui::Tool t : { btui::Tool::Erase, btui::Tool::Paint }) {
    const auto c = editcursor::compose(t, QColor(Qt::blue), false, 1, editcursor::Pointer::Bundled);
    QCOMPARE(c.hotspot, QPoint(16, 16));
    int drawn = 0;
    for (int y = 0; y < 32; y++)
      for (int x = 0; x < 32; x++)
        drawn += c.image.pixelColor(x, y).alpha() > 0 ? 1 : 0;
    QVERIFY2(drawn > 60, qPrintable(QString::number(drawn)));
  }
  QVERIFY(!editcursor::cursor(btui::Tool::Paint, QColor(Qt::blue), true, 1).pixmap().isNull());
}

void TestEditor::theCursorsDrawOnTheSystemsArrow() {
  // PR-20, OQ-17: Fixed and Variable are the system's own arrow with the
  // badge at its upper right, the hotspot the arrow's; Erase and Paint
  // follow the system's pointer size
  const double size = systempointer::sizeFactor();
  QVERIFY(size > 0);
  const systempointer::Arrow os = systempointer::arrow(1);
  if (os.image.isNull()) {
#if defined(Q_OS_MACOS)
    // AppKit draws NSCursor's image only with an application of its own,
    // which the offscreen platform does not make
    if (QGuiApplication::platformName() == QLatin1String("offscreen"))
      QSKIP("macOS offscreen: NSCursor has no image without an NSApplication");
    QFAIL("the system's arrow could not be read");
#elif defined(Q_OS_WIN)
    QFAIL("the system's arrow could not be read");
#else
    QSKIP("no Xcursor theme with an arrow here");
#endif
  }
  QVERIFY(QRect(QPoint(0, 0), os.image.size()).contains(os.hotspot));
  // the same arrow, larger on a screen of twice the density
  const systempointer::Arrow os2 = systempointer::arrow(2);
  QCOMPARE(os2.image.devicePixelRatio(), 2.0);
  // (an Xcursor theme pads each size its own way)
  QVERIFY(std::abs(os2.image.width() - 2 * os.image.width()) <= std::max(2, os.image.width() / 2));

  auto red = [](QColor c) { return c.alpha() > 200 && c.red() > 180 && c.green() < 90 && c.blue() < 90; };
  for (const btui::Tool tool : { btui::Tool::Fixed, btui::Tool::Variable }) {
    const auto c = editcursor::compose(tool, QColor(Qt::red), false, 1);
    QCOMPARE(c.hotspot, os.hotspot);
    QVERIFY(c.image.width() >= os.image.width() && c.image.height() >= os.image.height());
    // the arrow's own pixels, unchanged where the badge is not
    QCOMPARE(c.image.pixelColor(os.hotspot + QPoint(1, 3)), os.image.pixelColor(os.hotspot + QPoint(1, 3)));
    // the badge right of the arrow's leftmost column, in the drawing colour
    int badge = 0, minX = c.image.width();
    for (int y = 0; y < c.image.height(); y++)
      for (int x = 0; x < c.image.width(); x++)
        if (red(c.image.pixelColor(x, y))) {
          badge++;
          minX = std::min(minX, x);
        }
    QVERIFY2(tool == btui::Tool::Variable || badge > 30, qPrintable(QString::number(badge)));
    QVERIFY(badge > 0);
    QVERIFY(minX > os.hotspot.x() + 3);
  }
  const auto erase = editcursor::compose(btui::Tool::Erase, QColor(Qt::blue), false, 1);
  QCOMPARE(erase.image.width(), int(std::lround(32 * size)));
  QCOMPARE(erase.hotspot, QPoint(int(std::lround(16 * size)), int(std::lround(16 * size))));
  QVERIFY(!editcursor::cursor(btui::Tool::Fixed, QColor(Qt::red), false, 1).pixmap().isNull());
  // composed once for the same arrow, though macOS draws a new image of it on every call
  QCOMPARE(editcursor::cursor(btui::Tool::Fixed, QColor(Qt::red), false, 1).pixmap().cacheKey(),
           editcursor::cursor(btui::Tool::Fixed, QColor(Qt::red), false, 1).pixmap().cacheKey());
}

void TestEditor::theEditButtonShowsTheCursorsDrawing() {
  // C06 "The Edit button": the same drawing as the cursor, its badge or
  // drop in the drawing colour (Default: the theme's contrast); served as
  // image://editglyph/<tool>?color=&dark=&ink=
  auto count = [](const QImage & img, auto pred) {
    int n = 0;
    for (int y = 0; y < img.height(); y++)
      for (int x = 0; x < img.width(); x++)
        n += pred(img.pixelColor(x, y));
    return n;
  };
  auto red = [](QColor c) { return c.alpha() > 200 && c.red() > 180 && c.green() < 90 && c.blue() < 90; };
  auto any = [](QColor c) { return c.alpha() > 8; };
  /* a drawing's line weight in pixels: its inked area over half its edge,
   * as a line of width w and length l has area w*l and edge 2*l */
  auto weight = [](const QImage & img) {
    auto inked = [&](int x, int y) {
      return x >= 0 && y >= 0 && x < img.width() && y < img.height() && img.pixelColor(x, y).alpha() > 127;
    };
    int area = 0, edge = 0;
    for (int y = 0; y < img.height(); y++)
      for (int x = 0; x < img.width(); x++)
        if (inked(x, y)) {
          area++;
          edge += !inked(x - 1, y) || !inked(x + 1, y) || !inked(x, y - 1) || !inked(x, y + 1);
        }
    return edge ? 2.0 * area / edge : 0.0;
  };
  IconProvider icons;
  double lightest = 1e9, heaviest = 0;
  for (const char * name : { "eye", "pan", "orbit", "fit" }) {
    QSize s;
    const double w = weight(icons.requestImage(QString::fromLatin1(name) + QStringLiteral("?color=000000"), &s, QSize(80, 80)));
    lightest = std::min(lightest, w);
    heaviest = std::max(heaviest, w);
  }
  for (const btui::Tool tool : { btui::Tool::Fixed, btui::Tool::Variable, btui::Tool::Erase, btui::Tool::Paint }) {
    const QImage plain = editcursor::glyph(tool, QColor(), false, 44);
    QCOMPARE(plain.size(), QSize(44, 44));
    QVERIFY(count(plain, any) > 100);
    QCOMPARE(count(plain, red), 0);
    const QImage img = editcursor::glyph(tool, QColor(220, 30, 30), false, 44);
    bool left = false, right = false, top = false, bottom = false;
    for (int i = 0; i < 44; i++) {
      left |= img.pixelColor(0, i).alpha() > 8;
      right |= img.pixelColor(43, i).alpha() > 8;
      top |= img.pixelColor(i, 0).alpha() > 8;
      bottom |= img.pixelColor(i, 43).alpha() > 8;
    }
    if (tool == btui::Tool::Fixed || tool == btui::Tool::Variable) {
      // fitted: it reaches two opposite edges of the square, not the 32 px cursor's corner
      QVERIFY((left && right) || (top && bottom));
    } else {
      /* the eraser and the bucket are drawn as the toolbar's icons are: 20 of
       * 22 px, lines as heavy as theirs (they were the cursor's drawing
       * blown up to the square, twice as heavy), in the button's tint */
      QVERIFY(!left && !right && !top && !bottom);
      const double w = weight(editcursor::glyph(tool, QColor(), false, 88));
      QVERIFY2(w > lightest * 0.8 && w < heaviest * 1.25,
               qPrintable(QStringLiteral("%1: %2 px, the icons %3..%4").arg(int(tool)).arg(w).arg(lightest).arg(heaviest)));
      const QImage tinted = editcursor::glyph(tool, QColor(), false, 44, QColor(220, 30, 30));
      QVERIFY(count(tinted, red) > 40);
      QCOMPARE(count(tinted, [&](QColor c) { return c.alpha() > 200 && !red(c); }), 0);
    }
    if (tool != btui::Tool::Erase)                    // the eraser has no colour of its own
      QVERIFY2(count(img, red) > 4, qPrintable(QString::number(int(tool))));
  }
  // the provider reads the tool, the colour and the theme from its id
  EditGlyphProvider provider;
  QSize size;
  const QImage served = provider.requestImage(QStringLiteral("3?color=dc1e1e&dark=0"), &size, QSize(40, 40));
  QCOMPARE(size, QSize(40, 40));
  QVERIFY(count(served, red) > 4);
  QCOMPARE(served, editcursor::glyph(btui::Tool::Paint, QColor(0xdc, 0x1e, 0x1e), false, 40));
  // and the button's tint, which the eraser's and the bucket's lines take
  QCOMPARE(provider.requestImage(QStringLiteral("2?color=&dark=0&ink=dc1e1e"), &size, QSize(40, 40)),
           editcursor::glyph(btui::Tool::Erase, QColor(), false, 40, QColor(0xdc, 0x1e, 0x1e)));
  QVERIFY(provider.requestImage(QStringLiteral("0?color=&dark=1"), &size, QSize(40, 40))
          != provider.requestImage(QStringLiteral("0?color=&dark=0"), &size, QSize(40, 40)));
}

void TestEditor::theGridHitsItsCellsAndPaintsTheirVoxels() {
  // AC-C08-03 / 04 / 07
  EditorFixture f(4, 3, 2);
  InputGrid g;
  g.setSize(QSizeF(360, 500));
  g.setEditor(f.editor());
  QCOMPARE(g.cellSize(), 72);              // T-C08-2: floor(min(290/4, 440/3))
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 4; c++)
      QCOMPARE(g.cellAt(g.cellCentre(c, r)), QPoint(c, r));
  QCOMPARE(g.cellAt(QPointF(1, 1)), QPoint(-1, -1));
  // row 0 at the bottom
  QVERIFY(g.cellCentre(0, 0).y() > g.cellCentre(0, 2).y());

  f.click(0, 0);                                        // Default: the shape's colour
  f.click(1, 0, kVariable);                             // variable
  QImage img(360, 500, QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::transparent);
  {
    QPainter p(&img);
    g.paint(&p);
  }
  const QColor shapeColour = ShapesModel::chipColor(0);
  QCOMPARE(img.pixelColor(g.cellCentre(0, 0).toPoint()), shapeColour);
  const QColor varCentre = img.pixelColor(g.cellCentre(1, 0).toPoint());
  QVERIFY(varCentre != shapeColour);                    // hatched, not solid
  QCOMPARE(img.pixelColor(g.cellCentre(2, 0).toPoint()), Theme::instance()->panel());

  // another voxel type: cells of its own shape, hit at their centres
  f.app->document()->newDocument(gridType_c::GT_SPHERES);
  f.app->document()->editStructure(0, [](puzzle_c & p) { p.addShape(3, 3, 2); return true; });
  f.app->shapes()->select(0);
  QVERIFY(g.cellAt(g.cellCentre(1, 1)) == QPoint(1, 1));
  QCOMPARE(g.cellCentre(1, 0), QPointF(-1, -1));       // x+y+z odd: no cell
}

void TestEditor::theGridPaintsAStrokeWithoutGaps() {
  // C08: a fast drag fills every cell it crosses, one undo step; the right button erases
  EditorFixture f(8, 2, 1);
  InputGrid g;
  g.setSize(QSizeF(600, 300));
  g.setEditor(f.editor());
  auto press = mouse(QEvent::MouseButtonPress, g.cellCentre(0, 0));
  g.mousePressEvent(&press);
  QVERIFY(press.isAccepted());
  auto move = mouse(QEvent::MouseMove, g.cellCentre(7, 0));      // one jump across the row
  g.mouseMoveEvent(&move);
  auto release = mouse(QEvent::MouseButtonRelease, g.cellCentre(7, 0));
  g.mouseReleaseEvent(&release);
  QCOMPARE(f.filled(), 8);
  f.app->document()->undo();
  QCOMPARE(f.filled(), 0);
  f.app->document()->redo();

  auto rpress = mouse(QEvent::MouseButtonPress, g.cellCentre(3, 0), Qt::RightButton);
  g.mousePressEvent(&rpress);
  auto rrelease = mouse(QEvent::MouseButtonRelease, g.cellCentre(3, 0), Qt::RightButton);
  g.mouseReleaseEvent(&rrelease);
  QCOMPARE(f.filled(), 7);

  // a press beside the cells is left alone
  auto outside = mouse(QEvent::MouseButtonPress, QPointF(2, 2));
  g.mousePressEvent(&outside);
  QVERIFY(!outside.isAccepted());
}

void TestEditor::theOtherButtonMidStrokeLeavesTheStrokeAlone() {
  // PR 181 review: a left drag, the right button clicked during it: the
  // right release does not end the left stroke, which paints on
  EditorFixture f(6, 1, 1);
  InputGrid g;
  g.setSize(QSizeF(600, 200));
  g.setEditor(f.editor());
  auto press = mouse(QEvent::MouseButtonPress, g.cellCentre(0, 0));
  g.mousePressEvent(&press);
  auto move = mouse(QEvent::MouseMove, g.cellCentre(1, 0));
  g.mouseMoveEvent(&move);
  auto rpress = mouse(QEvent::MouseButtonPress, g.cellCentre(1, 0), Qt::RightButton);
  g.mousePressEvent(&rpress);
  QVERIFY(!rpress.isAccepted());                        // mid-stroke: ignored
  auto rrelease = mouse(QEvent::MouseButtonRelease, g.cellCentre(1, 0), Qt::RightButton);
  g.mouseReleaseEvent(&rrelease);
  auto on = mouse(QEvent::MouseMove, g.cellCentre(5, 0));
  g.mouseMoveEvent(&on);
  QCOMPARE(f.filled(), 6);                              // the stroke went on
  auto release = mouse(QEvent::MouseButtonRelease, g.cellCentre(5, 0));
  g.mouseReleaseEvent(&release);
  f.app->document()->undo();                            // and is one step
  QCOMPARE(f.filled(), 0);
}

void TestEditor::theViewIsReadyInTimeAfterAStrokeSegment() {
  // AC-C08-08's second half: after a stroke segment, the 3D view's next
  // frame -- the edit, the mesh rebuilt, the frame put together -- within
  // 16 ms, on a 16 x 16 x 16 shape half filled (the render itself is the
  // GPU's, measured in the performance guide)
  EditorFixture f(16, 16, 16);
  f.app->document()->editStructure(0, [](puzzle_c & p) {
    voxel_c * v = p.getShape(0);
    for (unsigned x = 0; x < 16; x++)
      for (unsigned y = 0; y < 16; y++)
        for (unsigned z = 0; z < 16; z++)
          if ((x + y + z) % 2 == 0)
            v->setState(x, y, z, (x + y) % 5 == 0 ? voxel_c::VX_VARIABLE : voxel_c::VX_FILLED);
    return true;
  });
  ViewportController * v = f.app->viewport();
  v->setViewportSize(800, 600);
  v->frame(1);
  std::vector<qint64> ns;
  f.editor()->setLayer(8);
  f.editor()->press(1, 0, false, Qt::NoModifier);
  quint64 rev = v->frame(1).meshRevision;
  for (int i = 2; i <= 10; i++) {
    QElapsedTimer t;
    t.start();
    f.editor()->dragTo(i, (i + 1) % 2);               // one segment: an empty cell filled
    const SceneFrame fr = v->frame(1);                // the next frame builds the mesh
    ns.push_back(t.nsecsElapsed());
    QVERIFY(fr.mesh && !fr.mesh->empty());
    QVERIFY2(fr.meshRevision != rev, "the mesh was rebuilt");
    rev = fr.meshRevision;
  }
  f.editor()->release();
  const double ms = double(*std::min_element(ns.begin(), ns.end())) / 1e6;
  qInfo("stroke segment to 3D frame, 16^3 half full: %.2f ms (best of 9)", ms);
  VERIFY_BUDGET(ms, 16.0);

  // and a 3D hover after an edit: the pick mesh made again, the target found
  std::vector<qint64> picks;
  for (int i = 0; i < 5; i++) {
    // an empty cell (the sum of its coordinates odd) filled: the mesh changes
    const quint64 before = v->frame(1).meshRevision;
    f.editor()->press(3 + i, i % 2 == 0 ? 6 : 5, false, Qt::NoModifier);
    f.editor()->release();
    QVERIFY(v->frame(1).meshRevision != before);
    QElapsedTimer t;
    t.start();
    const btui::Target3d target = v->targetAt(400, 300, Qt::ShiftModifier);
    picks.push_back(t.nsecsElapsed());
    QVERIFY(target.source != btui::TargetSource::None);
  }
  const double pickMs = double(*std::min_element(picks.begin(), picks.end())) / 1e6;
  qInfo("edit to 3D hover target, 16^3 half full: %.2f ms (best of 5)", pickMs);
  VERIFY_BUDGET(pickMs, 16.0);
}

void TestEditor::theGridByKeyboard() {
  // WCAG 2.1.1 (overview/04 section 5): focused, the arrows move a cell
  // cursor -- the hover, so readout and ghost follow -- Space applies the
  // tool, Shift+Space the Fixed quick tool, Delete erases; one step each
  EditorFixture f(3, 3, 1);
  InputGrid g;
  g.setSize(QSizeF(400, 300));
  g.setEditor(f.editor());
  QFocusEvent in(QEvent::FocusIn, Qt::TabFocusReason);
  QCoreApplication::sendEvent(&g, &in);
  QCOMPARE(f.editor()->hoverCol(), 1);                  // the middle cell first
  QCOMPARE(f.editor()->hoverRow(), 1);
  auto key = [&](int k, Qt::KeyboardModifiers m = Qt::NoModifier) {
    QKeyEvent e(QEvent::KeyPress, k, m);
    QCoreApplication::sendEvent(&g, &e);
    return e.isAccepted();
  };
  QVERIFY(key(Qt::Key_Right));
  QCOMPARE(f.editor()->hoverCol(), 2);
  QVERIFY(key(Qt::Key_Up));
  QCOMPARE(f.editor()->hoverRow(), 2);                  // rows count upward
  key(Qt::Key_Right);                                   // the edge: stays
  QCOMPARE(f.editor()->hoverCol(), 2);
  QVERIFY(!f.editor()->preview().empty());              // the 3D ghost follows
  f.editor()->setTool(EditorController::Variable);
  QVERIFY(key(Qt::Key_Space));
  QCOMPARE(f.state(2, 2, 0), int(voxel_c::VX_VARIABLE));
  QVERIFY(key(Qt::Key_Space, Qt::ShiftModifier));       // the Fixed quick tool
  QCOMPARE(f.state(2, 2, 0), int(voxel_c::VX_FILLED));
  QVERIFY(key(Qt::Key_Delete));                         // erases, whatever the tool
  QCOMPARE(f.state(2, 2, 0), int(voxel_c::VX_EMPTY));
  f.app->document()->undo();                            // one step each
  QCOMPARE(f.state(2, 2, 0), int(voxel_c::VX_FILLED));
  QVERIFY(!key(Qt::Key_A));                             // others go on to the window
  QFocusEvent out(QEvent::FocusOut, Qt::TabFocusReason);
  QCoreApplication::sendEvent(&g, &out);
  QCOMPARE(f.editor()->hoverCol(), -1);
}

void TestEditor::theFramesRepeatedHoverAsksTheKeyboard() {
  // Qt Quick repeats the hover at a still pointer after a frame, with
  // QGuiApplication::keyboardModifiers(), which on Windows still holds a
  // modifier just released: the quick tool would come back on until the
  // pointer moved. A hover where the last one was takes the keyboard's state.
  if (QGuiApplication::queryKeyboardModifiers() != Qt::NoModifier)
    QSKIP("a modifier is held on this keyboard");
  EditorFixture f(4, 4, 2);
  InputGrid g;
  g.setSize(QSizeF(400, 400));
  g.setEditor(f.editor());
  const QPointF p = g.cellCentre(1, 1);
  hover(g, QEvent::HoverEnter, p);
  QHoverEvent held(QEvent::HoverMove, p + QPointF(1, 0), p + QPointF(1, 0), p, kErase);
  g.hoverMoveEvent(&held);                           // a move: the event's keys
  QCOMPARE(f.editor()->toolInEffect(), EditorController::Erase);
  QHoverEvent repeat(QEvent::HoverMove, p + QPointF(1, 0), p + QPointF(1, 0), p + QPointF(1, 0), kErase);
  g.hoverMoveEvent(&repeat);                         // the frame's repeat, stale keys
  QCOMPARE(f.editor()->toolInEffect(), f.editor()->tool());
  // and the grid's own key release, which carries the released key on Windows
  f.editor()->setModifiers(kErase);
  QKeyEvent release(QEvent::KeyRelease, kMac ? Qt::Key_Alt : Qt::Key_Control, kErase);
  QCoreApplication::sendEvent(&g, &release);
  QCOMPARE(f.editor()->toolInEffect(), f.editor()->tool());
}

void TestEditor::theKeysAndThePointerTakeTheHighlightInTurn() {
  // PR 181: with the pointer resting on the grid, the arrows' cell is the
  // one highlighted -- Qt Quick's repeated hover at the still pointer does
  // not put it back -- until the pointer moves; the arrows then go on from
  // the pointer's cell
  EditorFixture f(4, 4, 2);
  InputGrid g;
  g.setSize(QSizeF(400, 400));
  g.setEditor(f.editor());
  auto at = [&] { return QPoint(f.editor()->hoverCol(), f.editor()->hoverRow()); };
  auto key = [&](int k, Qt::KeyboardModifiers m = Qt::NoModifier) {
    QKeyEvent e(QEvent::KeyPress, k, m);
    QCoreApplication::sendEvent(&g, &e);
    return e.isAccepted();
  };
  // the keys' cell: the middle one, from a first Tab in
  QFocusEvent in(QEvent::FocusIn, Qt::TabFocusReason);
  QFocusEvent out(QEvent::FocusOut, Qt::TabFocusReason);
  QCoreApplication::sendEvent(&g, &in);
  const QPoint middle = at();
  QVERIFY(middle != QPoint(0, 0));
  QCoreApplication::sendEvent(&g, &out);
  const QPointF rest = g.cellCentre(0, 0);
  hover(g, QEvent::HoverEnter, rest);
  QCOMPARE(at(), QPoint(0, 0));

  // Tab in again: the keys' cell shown, the pointer resting elsewhere
  QCoreApplication::sendEvent(&g, &in);
  QCOMPARE(at(), middle);
  hover(g, QEvent::HoverMove, rest);                 // the frame's repeat
  QCOMPARE(at(), middle);
  QVERIFY(key(Qt::Key_Right));
  hover(g, QEvent::HoverMove, rest);
  QCOMPARE(at(), middle + QPoint(1, 0));
  QVERIFY(key(Qt::Key_Space));                       // applied where it is shown
  QCOMPARE(f.state(middle.x() + 1, middle.y(), 0), int(voxel_c::VX_FILLED));
  QCOMPARE(f.state(0, 0, 0), int(voxel_c::VX_EMPTY));

  // the pointer moves: the highlight is its again
  hover(g, QEvent::HoverMove, g.cellCentre(3, 3));
  QCOMPARE(at(), QPoint(3, 3));
  // and the arrows go on from there, not from where they left off
  QVERIFY(key(Qt::Key_Down));
  QCOMPARE(at(), QPoint(3, 2));
  hover(g, QEvent::HoverMove, g.cellCentre(3, 3));   // the repeat, at the moved pointer
  QCOMPARE(at(), QPoint(3, 2));

  // a layer step keeps (and shows) the keys' cell
  QVERIFY(key(Qt::Key_Up, Qt::ShiftModifier));
  QCOMPARE(f.editor()->layer(), 1);
  hover(g, QEvent::HoverMove, g.cellCentre(3, 3));
  QCOMPARE(at(), QPoint(3, 2));

  // keys the grid leaves to the window take nothing
  hover(g, QEvent::HoverMove, g.cellCentre(1, 3));
  QVERIFY(!key(Qt::Key_A));
  QCOMPARE(at(), QPoint(1, 3));

  // focus away while the keys hold it: the highlight back under the pointer
  QVERIFY(key(Qt::Key_Left));
  QCOMPARE(at(), QPoint(0, 3));
  QCoreApplication::sendEvent(&g, &out);
  QCOMPARE(at(), QPoint(1, 3));

  // a click: the pointer's, and the keys go on from the clicked cell
  QCoreApplication::sendEvent(&g, &in);
  auto press = mouse(QEvent::MouseButtonPress, g.cellCentre(2, 1));
  g.mousePressEvent(&press);
  auto release = mouse(QEvent::MouseButtonRelease, g.cellCentre(2, 1));
  g.mouseReleaseEvent(&release);
  QCOMPARE(at(), QPoint(2, 1));
  QVERIFY(key(Qt::Key_Right));
  QCOMPARE(at(), QPoint(3, 1));
  hover(g, QEvent::HoverLeave, g.cellCentre(2, 1));
}

void TestEditor::a32By32GridRedrawsInTime() {
  // AC-C08-08: a 32 x 32 plane redrawn in under 8 ms -- half fixed, a
  // quarter variable (hatched, the dearest), at a 20 dp cell
  EditorFixture f(32, 32, 1);
  f.app->document()->editStructure(0, [](puzzle_c & p) {
    voxel_c * v = p.getShape(0);
    for (unsigned x = 0; x < 32; x++)
      for (unsigned y = 0; y < 32; y++)
        if ((x + y) % 4 != 3)
          v->setState(x, y, 0, (x + y) % 4 == 2 ? voxel_c::VX_VARIABLE : voxel_c::VX_FILLED);
    return true;
  });
  InputGrid g;
  g.setSize(QSizeF(32 * 20 + 70, 32 * 20 + 60));
  g.setEditor(f.editor());
  QCOMPARE(g.cellSize(), 20);
  QImage img(int(g.width()), int(g.height()), QImage::Format_ARGB32_Premultiplied);
  std::vector<qint64> ns;
  for (int i = 0; i < 9; i++) {
    img.fill(Qt::transparent);
    QPainter p(&img);
    QElapsedTimer t;
    t.start();
    g.paint(&p);
    ns.push_back(t.nsecsElapsed());
  }
  // the fastest of 9: a busy machine (a parallel build) slows some runs,
  // never the paint itself
  const double ms = double(*std::min_element(ns.begin(), ns.end())) / 1e6;
  qInfo("32 x 32 grid: %.2f ms (best of 9)", ms);
  // next to the test binary, for a look
  img.save(QCoreApplication::applicationDirPath() + QStringLiteral("/render-grid-32.png"));
  VERIFY_BUDGET(ms, 8.0);
}
