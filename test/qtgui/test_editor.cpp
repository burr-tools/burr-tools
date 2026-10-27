/* Qt Test cases for the voxel editor: the controller's strokes, tools,
 * planes and sizes, the composed edit cursors, and the 2D grid item's hit
 * testing, painting and pointer input.
 */
#include "test_editor.h"

#include "app.h"
#include "editcursor.h"
#include "editorcontroller.h"
#include "statuscontroller.h"
#include "theme.h"
#include "viewportcontroller.h"
#include "voxelgriditem.h"

#include "../../src/lib/puzzle.h"
#include "../../src/lib/voxel.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <memory>
#include <vector>

namespace {

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
  };

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
  f.editor()->press(0, 0, false, Qt::AltModifier);
  QCOMPARE(f.editor()->toolInEffect(), EditorController::Variable);
  f.editor()->setModifiers(Qt::ControlModifier);
  f.editor()->dragTo(1, 0);
  f.editor()->release();
  QCOMPARE(f.variable(), 2);
  QCOMPARE(f.editor()->tool(), EditorController::Fixed);
  // Ctrl erases (Control on macOS)
  f.click(0, 0, btui::currentPlatform() == btui::Platform::Mac ? Qt::MetaModifier : Qt::ControlModifier);
  QCOMPARE(f.variable(), 1);
  f.click(2, 0, Qt::ShiftModifier);
  QCOMPARE(f.filled(), 1);
}

void TestEditor::theToolInEffectFollowsTheModifiersHeldOverTheGrid() {
  EditorFixture f;
  EditorController * e = f.editor();
  QSignalSpy tool(e, &EditorController::toolChanged);
  e->setModifiers(Qt::ShiftModifier | Qt::AltModifier);
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
  EditorFixture f(4, 4, 4);
  EditorController * e = f.editor();
  QVERIFY(e->allPlanes());
  e->setPlane(EditorController::XZ);
  QCOMPARE(e->plane(), EditorController::XZ);
  f.app->document()->newDocument(gridType_c::GT_SPHERES);
  f.app->document()->editStructure(0, [](puzzle_c & p) { p.addShape(4, 4, 4); return true; });
  f.app->shapes()->select(0);
  QVERIFY(!e->allPlanes());
  QCOMPARE(e->plane(), EditorController::XY);   // fell back
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
  // PR-20 / glyph catalog ▸ Cursors
  for (qreal scale : { 1.0, 1.5, 2.0 }) {
    const auto c = editcursor::compose(btui::Tool::Fixed, QColor(Qt::red), false, scale);
    QCOMPARE(c.image.width(), qRound(32 * scale));
    QCOMPARE(c.image.devicePixelRatio(), scale);
    QCOMPARE(c.hotspot, QPoint(1, 1));
  }
  // the badge's inside: the drawing colour, or for Default the theme's opposite tone
  auto inside = [](btui::Tool t, const QColor & colour, bool dark) {
    return editcursor::compose(t, colour, dark, 1).image.pixelColor(15, 3);
  };
  QCOMPARE(inside(btui::Tool::Fixed, QColor(Qt::red), false), QColor(Qt::red));
  QCOMPARE(inside(btui::Tool::Fixed, QColor(), false), QColor(Qt::white));
  QVERIFY(inside(btui::Tool::Fixed, QColor(), true).lightness() < 40);
  QVERIFY(inside(btui::Tool::Variable, QColor(), false).lightness() > 240);
  // Erase and Paint: the icon alone, hotspot at its centre, something drawn there
  for (btui::Tool t : { btui::Tool::Erase, btui::Tool::Paint }) {
    const auto c = editcursor::compose(t, QColor(Qt::blue), false, 1);
    QCOMPARE(c.hotspot, QPoint(16, 16));
    int drawn = 0;
    for (int y = 0; y < 32; y++)
      for (int x = 0; x < 32; x++)
        drawn += c.image.pixelColor(x, y).alpha() > 0 ? 1 : 0;
    QVERIFY2(drawn > 60, qPrintable(QString::number(drawn)));
  }
  QVERIFY(!editcursor::cursor(btui::Tool::Paint, QColor(Qt::blue), true, 1).pixmap().isNull());
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
  f.click(1, 0, Qt::AltModifier);                       // variable
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
  QVERIFY2(ms < 8.0, qPrintable(QString::number(ms) + QStringLiteral(" ms")));
}
