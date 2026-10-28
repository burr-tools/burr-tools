/* Qt Test cases for editing every voxel type end to end: the 2D grid in
 * each of the type's planes, by pointer and keyboard, and the 3D view's
 * clicks, through the App's controllers and the grid item.
 */
#include "test_voxeltypes.h"

#include "app.h"
#include "theme.h"
#include "editorcontroller.h"
#include "viewportcontroller.h"
#include "voxelgriditem.h"

#include "../../src/lib/puzzle.h"
#include "../../src/lib/voxel.h"
#include "../../src/uicore/commands.h"

#include <QCoreApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <cmath>
#include <memory>

namespace {

  /* an App with one empty shape of the type, three cells or more along
   * each axis (rhombic and tetra-octa cells span several units), selected */
  struct TypeFixture {
    QTemporaryDir dir;
    std::unique_ptr<App> app;

    explicit TypeFixture(gridType_c::gridType type) {
      app = std::make_unique<App>(dir.filePath(QStringLiteral("s.rc")), dir.filePath(QStringLiteral("l.rc")));
      app->document()->newDocument(int(type));
      const unsigned n = type == gridType_c::GT_RHOMBIC ? 15 : type == gridType_c::GT_TETRA_OCTA ? 12 : 4;
      app->document()->editStructure(0, [&](puzzle_c & p) { p.addShape(n, n, n); return true; });
      app->shapes()->select(0);
      EditorController * e = app->editor();
      e->setTool(EditorController::Fixed);
      e->setMirror(0);
      e->setSpan(0);
    }

    EditorController * editor(void) const { return app->editor(); }
    const voxel_c & shape(void) const { return *app->document()->session().puzzle().getShape(0); }
    int filled(void) const {
      return int(shape().countState(voxel_c::VX_FILLED) + shape().countState(voxel_c::VX_VARIABLE));
    }
  };

  struct InputGrid : VoxelGridItem {
    using VoxelGridItem::mousePressEvent;
    using VoxelGridItem::mouseReleaseEvent;
  };

  void addTypes(void) {
    QTest::addColumn<int>("type");
    QTest::newRow("bricks") << int(gridType_c::GT_BRICKS);
    QTest::newRow("prism") << int(gridType_c::GT_TRIANGULAR_PRISM);
    QTest::newRow("spheres") << int(gridType_c::GT_SPHERES);
    QTest::newRow("rhombic") << int(gridType_c::GT_RHOMBIC);
    QTest::newRow("tetra-octa") << int(gridType_c::GT_TETRA_OCTA);
  }

  // Ctrl erases (Option on macOS)
  const Qt::KeyboardModifiers kErase = btui::currentPlatform() == btui::Platform::Mac ? Qt::AltModifier : Qt::ControlModifier;
}

void TestVoxelTypes::paintsEveryPlaneByPointerAndKeys_data() {
  addTypes();
}

void TestVoxelTypes::paintsEveryPlaneByPointerAndKeys() {
  // C08 for every type (C14): in each plane it has, a click on a cell of
  // layer 1 places a voxel in that layer of that plane -- its layer dot
  // alone lights -- and the keyboard's cursor moves, places and erases
  QFETCH(int, type);
  const auto t = gridType_c::gridType(type);
  TypeFixture f(t);
  EditorController * e = f.editor();
  InputGrid g;
  g.setSize(QSizeF(480, 360));
  g.setEditor(e);

  int planes = 0;
  for (const EditorController::Plane plane : { EditorController::XY, EditorController::XZ, EditorController::YZ }) {
    if (!e->planeAvailable(int(plane))) {
      QVERIFY(t == gridType_c::GT_TRIANGULAR_PRISM && plane != EditorController::XY);
      continue;
    }
    planes++;
    e->setPlane(plane);
    QCOMPARE(int(e->plane()), int(plane));
    e->setLayer(1);
    QCOMPARE(e->layer(), 1);

    // the first cell the layer draws
    QPoint cell(-1, -1);
    for (int row = 0; row < 16 && cell.x() < 0; row++)
      for (int col = 0; col < 16 && cell.x() < 0; col++)
        if (g.cellCentre(col, row).x() >= 0)
          cell = QPoint(col, row);
    QVERIFY2(cell.x() >= 0, "the layer has no cell");
    QCOMPARE(g.cellAt(g.cellCentre(cell.x(), cell.y())), cell);

    const int before = f.filled();
    QMouseEvent press(QEvent::MouseButtonPress, g.cellCentre(cell.x(), cell.y()), g.cellCentre(cell.x(), cell.y()),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    g.mousePressEvent(&press);
    QMouseEvent release(QEvent::MouseButtonRelease, g.cellCentre(cell.x(), cell.y()), g.cellCentre(cell.x(), cell.y()),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    g.mouseReleaseEvent(&release);
    QCOMPARE(f.filled(), before + 1);
    const QList<bool> dots = e->layerDots();
    for (int l = 0; l < dots.size(); l++)
      QCOMPARE(dots.at(l), l == 1);

    // the keyboard: focused, the cursor is on a cell; the arrows move it off the voxel
    QFocusEvent in(QEvent::FocusIn, Qt::TabFocusReason);
    QCoreApplication::sendEvent(&g, &in);
    QVERIFY(e->hoverCol() >= 0);
    auto key = [&](int k) {
      QKeyEvent ev(QEvent::KeyPress, k, Qt::NoModifier);
      QCoreApplication::sendEvent(&g, &ev);
      return ev.isAccepted();
    };
    for (const int k : { Qt::Key_Right, Qt::Key_Up, Qt::Key_Left, Qt::Key_Down, Qt::Key_Right })
      if (QPoint(e->hoverCol(), e->hoverRow()) == cell)
        key(k);
      else
        break;
    QVERIFY(QPoint(e->hoverCol(), e->hoverRow()) != cell);
    QVERIFY(g.cellCentre(e->hoverCol(), e->hoverRow()).x() >= 0);   // a cell of the type
    QVERIFY(key(Qt::Key_Space));
    QCOMPARE(f.filled(), before + 2);
    QVERIFY(key(Qt::Key_Delete));
    QCOMPARE(f.filled(), before + 1);
    QFocusEvent out(QEvent::FocusOut, Qt::TabFocusReason);
    QCoreApplication::sendEvent(&g, &out);

    // three steps, each undone on its own
    for (int i = 0; i < 3; i++)
      f.app->document()->undo();
    QCOMPARE(f.filled(), before);
  }
  QCOMPARE(planes, t == gridType_c::GT_TRIANGULAR_PRISM ? 1 : 3);
}

void TestVoxelTypes::theHoveredCellIsOutlined_data() {
  addTypes();
}

void TestVoxelTypes::theHoveredCellIsOutlined() {
  // C08 "hover outline": a 2 dp accent line inside the cell under the
  // pointer, in the cell's own shape (a circle for spheres), and gone when
  // the pointer leaves
  QFETCH(int, type);
  TypeFixture f{ gridType_c::gridType(type) };
  InputGrid g;
  g.setSize(QSizeF(480, 360));
  g.setEditor(f.editor());
  QPoint cell(-1, -1);
  for (int row = 0; row < 16 && cell.x() < 0; row++)
    for (int col = 0; col < 16 && cell.x() < 0; col++)
      if (g.cellCentre(col, row).x() >= 0)
        cell = QPoint(col, row);
  QVERIFY(cell.x() >= 0);
  const QPointF c = g.cellCentre(cell.x(), cell.y());
  const QColor accent = Theme::instance()->accent();
  // accent pixels within 40 dp of the cell's centre
  auto accentNear = [&] {
    QImage img(int(g.width()), int(g.height()), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    g.paint(&p);
    p.end();
    int n = 0;
    for (int y = std::max(0, int(c.y()) - 40); y < std::min(img.height(), int(c.y()) + 40); y++)
      for (int x = std::max(0, int(c.x()) - 40); x < std::min(img.width(), int(c.x()) + 40); x++) {
        const QColor q = img.pixelColor(x, y);
        n += std::abs(q.red() - accent.red()) < 12 && std::abs(q.green() - accent.green()) < 12 &&
             std::abs(q.blue() - accent.blue()) < 12;
      }
    return n;
  };
  const int none = accentNear();
  f.editor()->hover(cell.x(), cell.y());
  const int hovered = accentNear();
  QVERIFY2(hovered > none + 20, qPrintable(QStringLiteral("%1 accent pixels, %2 without").arg(hovered).arg(none)));
  // the pointer gone: no outline
  f.editor()->hover(-1, -1);
  QCOMPARE(accentNear(), none);
}

void TestVoxelTypes::editsIn3dOnTheSlabAndOnAFace_data() {
  addTypes();
}

void TestVoxelTypes::editsIn3dOnTheSlabAndOnAFace() {
  // C06 for every type: Shift-click on the active layer's slab places a
  // voxel there; Shift-click on it places the next one beyond its face; the
  // Erase quick tool takes the voxel under the pointer; one step each
  QFETCH(int, type);
  TypeFixture f{ gridType_c::gridType(type) };
  ViewportController * v = f.app->viewport();
  v->setViewportSize(320, 240);
  v->setProjection(QStringLiteral("orthographic"));
  v->frame(1);
  v->camera().setOrientation(btui::Camera::lookFrom({ 0, 0, 1 }, 0));
  QCOMPARE(f.editor()->layer(), 0);

  // the point over a cell of the slab nearest the view's middle: a voxel at
  // the grid's edge has sloping faces (spheres) whose far side is outside,
  // where a Fixed press is rightly refused
  QPointF at(-1, -1);
  double best = 1e9;
  for (int y = 40; y < 200; y += 4)
    for (int x = 60; x < 260; x += 4)
      if (v->targetAt(float(x), float(y), Qt::ShiftModifier).source == btui::TargetSource::Slab) {
        const double d = std::hypot(x - 160.0, y - 120.0);
        if (d < best) {
          best = d;
          at = QPointF(x, y);
        }
      }
  QVERIFY2(at.x() >= 0, "no point of the view is over the slab");
  auto click = [&](Qt::KeyboardModifiers m) {
    v->pointerPress(Qt::LeftButton, float(at.x()), float(at.y()), m);
    v->pointerRelease(Qt::LeftButton, float(at.x()), float(at.y()));
    v->frame(1);                                      // the pick mesh follows
  };

  click(Qt::ShiftModifier);
  QCOMPARE(f.filled(), 1);
  QVERIFY(f.editor()->layerDots().value(0));
  QCOMPARE(int(v->targetAt(float(at.x()), float(at.y()), Qt::ShiftModifier).source), int(btui::TargetSource::Face));
  click(Qt::ShiftModifier);                           // beyond the face under the pointer
  QCOMPARE(f.filled(), 2);
  click(kErase);                                      // the voxel under the pointer
  QCOMPARE(f.filled(), 1);
  for (int i = 0; i < 3; i++)
    f.app->document()->undo();
  QCOMPARE(f.filled(), 0);
}
