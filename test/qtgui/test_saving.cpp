/* Qt Test cases for saving what the Entities workspace edits: every voxel
 * type, the 2D grid in each plane, the 3D view, the inspector, several
 * shapes and the problems that use them, each saved, opened again in a
 * fresh App and compared with the puzzle as it was before the save.
 */
#include "test_saving.h"

#include "app.h"
#include "coloursmodel.h"
#include "editorcontroller.h"
#include "inspectorcontroller.h"
#include "shapesmodel.h"
#include "viewportcontroller.h"
#include "voxelgriditem.h"

#include "../../src/lib/problem.h"
#include "../../src/lib/puzzle.h"
#include "../../src/lib/voxel.h"
#include "../../src/uicore/commands.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace {

  /* The whole puzzle as lines of text, so a failed comparison names what
   * differs: the type, the colours, each shape's size, label, weight and
   * every voxel that is not empty, each problem's result and parts. */
  QStringList describe(const puzzle_c & p) {
    QStringList out;
    out << QStringLiteral("type %1").arg(int(p.getGridType()->getType()));
    for (unsigned int c = 0; c < p.colorNumber(); c++) {
      unsigned char r, g, b;
      p.getColor(c, &r, &g, &b);
      out << QStringLiteral("colour %1: %2 %3 %4").arg(c + 1).arg(r).arg(g).arg(b);
    }
    for (unsigned int s = 0; s < p.getNumberOfShapes(); s++) {
      const voxel_c & v = *p.getShape(s);
      out << QStringLiteral("S%1 %2x%3x%4 '%5' weight %6").arg(s + 1).arg(v.getX()).arg(v.getY()).arg(v.getZ())
                 .arg(QString::fromStdString(v.getName())).arg(v.getWeight());
      for (unsigned int i = 0; i < v.getXYZ(); i++)
        if (v.getState(i) != voxel_c::VX_EMPTY)
          out << QStringLiteral("  S%1 voxel %2: state %3 colour %4").arg(s + 1).arg(i).arg(v.getState(i)).arg(v.getColor(i));
    }
    for (unsigned int i = 0; i < p.getNumberOfProblems(); i++) {
      const problem_c & q = *p.getProblem(i);
      out << QStringLiteral("P%1 result %2").arg(i + 1).arg(q.resultValid() ? int(q.getResultId()) : -1);
      for (unsigned int part = 0; part < q.getNumberOfParts(); part++)
        out << QStringLiteral("  P%1 part S%2 %3..%4").arg(i + 1).arg(q.getShapeIdOfPart(part) + 1)
                   .arg(q.getPartMinimum(part)).arg(q.getPartMaximum(part));
    }
    return out;
  }

  int filledCells(const puzzle_c & p) {
    int n = 0;
    for (unsigned int s = 0; s < p.getNumberOfShapes(); s++)
      n += int(p.getShape(s)->countState(voxel_c::VX_FILLED) + p.getShape(s)->countState(voxel_c::VX_VARIABLE));
    return n;
  }

  /* an App with a new puzzle of one type and one empty shape, three cells
   * or more along each axis (rhombic and tetra-octa cells span several
   * units), selected; it saves into its own folder and opens again */
  struct SaveFixture {
    QTemporaryDir dir;
    std::unique_ptr<App> app;

    explicit SaveFixture(gridType_c::gridType type) {
      makeApp();
      app->document()->newDocument(int(type));
      const unsigned n = type == gridType_c::GT_RHOMBIC ? 15 : type == gridType_c::GT_TETRA_OCTA ? 12 : 4;
      app->document()->editStructure(0, [&](puzzle_c & p) { p.addShape(n, n, n); return true; });
      app->shapes()->select(0);
      EditorController * e = app->editor();
      e->setTool(EditorController::Fixed);
      e->setMirror(0);
      e->setSpan(0);
    }

    void makeApp(void) {
      app = std::make_unique<App>(dir.filePath(QStringLiteral("s.rc")), dir.filePath(QStringLiteral("l.rc")));
    }

    const puzzle_c & puzzle(void) const { return app->document()->session().puzzle(); }
    EditorController * editor(void) const { return app->editor(); }
    QString path(void) const { return dir.filePath(QStringLiteral("saved.xmpuzzle")); }

    /* Save as, then open the file in a new App; what was on screen before
     * the save, to compare with puzzle() */
    QStringList saveAndReopen(void) {
      const QStringList before = describe(puzzle());
      app->document()->saveAsFile(QUrl::fromLocalFile(path()));
      [&] {
        QVERIFY2(QFile::exists(path()), "the file is written");
        QVERIFY2(!app->document()->modified(), "the save leaves the document clean");
      }();
      app.reset();
      makeApp();
      [&] {
        QVERIFY2(app->document()->loadPath(path()), "the file opens again");
        QVERIFY(!app->document()->modified());
        QVERIFY(!app->document()->canUndo());
      }();
      return before;
    }
  };

  /* the cells (col, row) the 2D grid draws in its current layer */
  std::vector<QPoint> layerCells(const VoxelGridItem & g) {
    std::vector<QPoint> cells;
    for (int row = 0; row < 24; row++)
      for (int col = 0; col < 24; col++)
        if (g.cellCentre(col, row).x() >= 0)
          cells.push_back(QPoint(col, row));
    return cells;
  }

  void click(EditorController * e, QPoint cell) {
    QVERIFY(e->press(cell.x(), cell.y(), false, Qt::NoModifier));
    e->release();
  }

  // Ctrl erases (Option on macOS)
  const Qt::KeyboardModifiers kErase = btui::currentPlatform() == btui::Platform::Mac ? Qt::AltModifier : Qt::ControlModifier;

  void addTypes(void) {
    QTest::addColumn<int>("type");
    QTest::newRow("bricks") << int(gridType_c::GT_BRICKS);
    QTest::newRow("prism") << int(gridType_c::GT_TRIANGULAR_PRISM);
    QTest::newRow("spheres") << int(gridType_c::GT_SPHERES);
    QTest::newRow("rhombic") << int(gridType_c::GT_RHOMBIC);
    QTest::newRow("tetra-octa") << int(gridType_c::GT_TETRA_OCTA);
  }
}

void TestSaving::edits2dInEveryPlaneAreSaved_data() {
  addTypes();
}

void TestSaving::edits2dInEveryPlaneAreSaved() {
  // the 2D grid, in each plane the type has and in a layer of its own:
  // a Fixed click, a Variable click, a stroke in a new colour, Paint over
  // the first voxel, a placement erased again, a mirrored placement and an
  // area -- all of it saved and read back as it was
  QFETCH(int, type);
  SaveFixture f{ gridType_c::gridType(type) };
  EditorController * e = f.editor();
  VoxelGridItem g;
  g.setSize(QSizeF(480, 360));
  g.setEditor(e);

  int layer = 0;
  for (const EditorController::Plane plane : { EditorController::XY, EditorController::XZ, EditorController::YZ }) {
    if (!e->planeAvailable(int(plane)))
      continue;
    e->setPlane(plane);
    e->setLayer(layer++);
    const std::vector<QPoint> cells = layerCells(g);
    QVERIFY2(cells.size() >= 6, "the layer has room for the edits");
    const int before = filledCells(f.puzzle());

    e->setTool(EditorController::Fixed);
    f.app->colours()->select(0);
    click(e, cells[0]);
    e->setTool(EditorController::Variable);
    click(e, cells[1]);
    f.app->colours()->addColour();                   // a colour of the puzzle's, drawn with at once
    e->setTool(EditorController::Fixed);
    QVERIFY(e->press(cells[2].x(), cells[2].y(), false, Qt::NoModifier));
    e->dragTo(cells[3].x(), cells[3].y());
    e->release();
    e->setTool(EditorController::Paint);
    click(e, cells[0]);
    e->setTool(EditorController::Fixed);
    click(e, cells[4]);
    e->setTool(EditorController::Erase);
    click(e, cells[4]);
    e->setTool(EditorController::Fixed);
    e->toggleMirror(0);
    click(e, cells[5]);
    e->toggleMirror(0);
    QVERIFY2(filledCells(f.puzzle()) > before, "each plane's edits place voxels");
  }
  // an area (legacy's rubber band) in the last plane
  e->setAreaMode(true);
  const std::vector<QPoint> cells = layerCells(g);
  QVERIFY(e->press(cells.front().x(), cells.front().y(), false, Qt::NoModifier));
  e->dragTo(cells[cells.size() / 2].x(), cells[cells.size() / 2].y());
  e->release();
  e->setAreaMode(false);
  QVERIFY(f.app->document()->modified());

  const QStringList before = f.saveAndReopen();
  QCOMPARE(describe(f.puzzle()), before);
}

void TestSaving::edits3dAreSaved_data() {
  addTypes();
}

void TestSaving::edits3dAreSaved() {
  // the 3D view: Shift-clicks on the slab and beyond a face, the Erase
  // quick tool, Edit mode's Variable on another cell of the slab, Paint in
  // a new colour, and the slab moved to another layer -- saved and read back
  QFETCH(int, type);
  SaveFixture f{ gridType_c::gridType(type) };
  ViewportController * v = f.app->viewport();
  EditorController * e = f.editor();
  v->setViewportSize(320, 240);
  v->setProjection(QStringLiteral("orthographic"));
  v->frame(1);
  v->camera().setOrientation(btui::Camera::lookFrom({ 0, 0, 1 }, 0));

  // points over the slab, the one nearest the view's middle first
  auto slabPoints = [&] {
    std::vector<QPointF> pts;
    for (int y = 40; y < 200; y += 4)
      for (int x = 60; x < 260; x += 4)
        if (v->targetAt(float(x), float(y), Qt::ShiftModifier).source == btui::TargetSource::Slab)
          pts.push_back(QPointF(x, y));
    std::sort(pts.begin(), pts.end(), [](QPointF a, QPointF b) {
      return std::hypot(a.x() - 160, a.y() - 120) < std::hypot(b.x() - 160, b.y() - 120);
    });
    return pts;
  };
  auto clickAt = [&](QPointF at, Qt::KeyboardModifiers m) {
    v->pointerPress(Qt::LeftButton, float(at.x()), float(at.y()), m);
    v->pointerRelease(Qt::LeftButton, float(at.x()), float(at.y()));
    v->frame(1);                                      // the pick mesh follows
  };

  std::vector<QPointF> pts = slabPoints();
  QVERIFY2(!pts.empty(), "no point of the view is over the slab");
  const QPointF first = pts.front();
  clickAt(first, Qt::ShiftModifier);                  // on the slab
  clickAt(first, Qt::ShiftModifier);                  // beyond the face
  QCOMPARE(filledCells(f.puzzle()), 2);
  clickAt(first, kErase);                             // the Erase quick tool takes the front one
  QCOMPARE(filledCells(f.puzzle()), 1);

  // Edit mode, Variable, a cell of the slab away from the first
  e->setEditMode(true);
  e->setTool(EditorController::Variable);
  pts = slabPoints();
  QPointF away(-1, -1);
  for (const QPointF & p : pts)
    if (std::hypot(p.x() - first.x(), p.y() - first.y()) > 40) {
      away = p;
      break;
    }
  QVERIFY2(away.x() >= 0, "the slab has a second cell in view");
  clickAt(away, Qt::NoModifier);
  QCOMPARE(filledCells(f.puzzle()), 2);

  // Paint the first column's front voxel in a new colour
  f.app->colours()->addColour();
  e->setTool(EditorController::Paint);
  clickAt(first, Qt::NoModifier);

  // the slab one layer up: a voxel there
  e->setLayer(1);
  v->frame(1);
  e->setTool(EditorController::Fixed);
  pts = slabPoints();
  const QPointF upper = pts.empty() ? QPointF(-1, -1) : pts.front();
  if (upper.x() >= 0) {                              // the layer below may hide it
    const int was = filledCells(f.puzzle());
    clickAt(upper, Qt::NoModifier);
    QCOMPARE(filledCells(f.puzzle()), was + 1);
  }
  e->setEditMode(false);
  QVERIFY(filledCells(f.puzzle()) >= 2);

  const QStringList before = f.saveAndReopen();
  QCOMPARE(describe(f.puzzle()), before);
  // a voxel in the new colour is among them
  bool coloured = false;
  for (const QString & line : before)
    coloured = coloured || (line.contains(QStringLiteral(" voxel ")) && !line.endsWith(QStringLiteral("colour 0")));
  QVERIFY2(coloured, "Paint took the new colour");
}

void TestSaving::transformsGridSizeAndScaleAreSaved_data() {
  addTypes();
}

void TestSaving::transformsGridSizeAndScaleAreSaved() {
  // the inspector's flip, rotation and nudge, the grid made larger, then
  // ×2 where the type scales and To origin -- each a change of the shape's
  // voxels or size that the file must hold
  QFETCH(int, type);
  SaveFixture f{ gridType_c::gridType(type) };
  EditorController * e = f.editor();
  InspectorController * in = f.app->inspector();
  VoxelGridItem g;
  g.setSize(QSizeF(480, 360));
  g.setEditor(e);
  e->setLayer(1);
  const std::vector<QPoint> cells = layerCells(g);
  QVERIFY(cells.size() >= 3);
  click(e, cells[1]);
  e->setTool(EditorController::Variable);
  click(e, cells[2]);
  const QStringList start = describe(f.puzzle());

  e->setSize(0, e->size(0) + 1);                     // one step wider
  QVERIFY(f.puzzle().getShape(0)->getX() > 4u || type != int(gridType_c::GT_BRICKS));
  int ran = 0;
  for (const char * kind : { "flip", "rotate", "nudge" })
    for (const QVariant & b : in->transformButtons()) {
      const QVariantMap m = b.toMap();
      if (m.value(QStringLiteral("kind")).toString() != QLatin1String(kind))
        continue;
      const int task = m.value(QStringLiteral("task")).toInt();
      if (!in->allowed(task))
        continue;
      in->run(task);
      ran++;
      break;
    }
  QVERIFY2(ran >= 2, "a flip and a rotation ran");
  const QVariantMap tasks = in->tasks();
  if (in->scaleShown())
    in->run(tasks.value(QStringLiteral("scale2")).toInt());
  in->run(tasks.value(QStringLiteral("origin")).toInt());
  QVERIFY(describe(f.puzzle()) != start);

  const QStringList before = f.saveAndReopen();
  QCOMPARE(describe(f.puzzle()), before);
}

void TestSaving::eachShapeKeepsItsOwnEdits() {
  // several pieces, each edited while selected: voxels, grid sizes, a
  // label, a weight; a duplicate, a reorder and a delete renumber them;
  // the file holds each shape's own, in the list's order
  SaveFixture f{ gridType_c::GT_BRICKS };
  EditorController * e = f.editor();
  ShapesModel * s = f.app->shapes();
  VoxelGridItem g;
  g.setSize(QSizeF(480, 360));
  g.setEditor(e);

  s->addShape();
  s->addShape();
  QCOMPARE(int(f.puzzle().getNumberOfShapes()), 3);
  // shape i gets i + 1 voxels in layer i, and its own label
  for (int i = 0; i < 3; i++) {
    s->select(i);
    QCOMPARE(s->selected(), i);
    e->setLayer(i);
    const std::vector<QPoint> cells = layerCells(g);
    for (int k = 0; k <= i; k++)
      click(e, cells[size_t(k)]);
    s->renameShape(i, QStringLiteral("piece %1").arg(i + 1));
  }
  s->select(1);
  e->setSize(2, e->size(2) + 2);                     // S2 two layers deeper
  s->changeWeight(2, +2);                            // S3 weight 3
  s->duplicateShape(0);                              // S4: a copy of S1
  s->renameShape(3, QStringLiteral("copy of piece 1"));
  s->moveShape(3, 1);                                // the copy becomes S2
  s->deleteShape(2);                                 // piece 2 goes; piece 3 is S3

  const puzzle_c & p = f.puzzle();
  QCOMPARE(int(p.getNumberOfShapes()), 3);
  QCOMPARE(QString::fromStdString(p.getShape(0)->getName()), QStringLiteral("piece 1"));
  QCOMPARE(QString::fromStdString(p.getShape(1)->getName()), QStringLiteral("copy of piece 1"));
  QCOMPARE(QString::fromStdString(p.getShape(2)->getName()), QStringLiteral("piece 3"));
  QCOMPARE(p.getShape(2)->getWeight(), 3);
  QVERIFY(p.getShape(1)->identicalInBB(p.getShape(0)));
  QCOMPARE(int(p.getShape(2)->countState(voxel_c::VX_FILLED)), 3);

  // and S3 edited after the reorder lands in S3
  s->select(2);
  e->setLayer(0);
  click(e, layerCells(g)[5]);
  QCOMPARE(int(p.getShape(2)->countState(voxel_c::VX_FILLED)), 4);

  const QStringList before = f.saveAndReopen();
  QCOMPARE(describe(f.puzzle()), before);
}

void TestSaving::problemsFollowTheirShapesThroughASave() {
  // a problem's result and pieces follow their shapes through a reorder
  // and a delete (OQ-16), and the file says so
  SaveFixture f{ gridType_c::GT_BRICKS };
  ShapesModel * s = f.app->shapes();
  s->addShape();
  s->addShape();
  s->addShape();
  const char * labels[] = { "result", "piece a", "piece b", "unused" };
  for (int i = 0; i < 4; i++)
    s->renameShape(i, QString::fromLatin1(labels[i]));
  f.app->document()->editStructure(0, [](puzzle_c & p) {
    problem_c & q = *p.getProblem(p.addProblem());
    q.setResultId(0);
    q.setShapeMaximum(1, 2);
    q.setShapeMinimum(1, 1);
    q.setShapeMaximum(2, 1);
    q.setShapeMinimum(2, 1);
    return true;
  });
  s->moveShape(0, 2);                                // result to S3
  s->deleteShape(3);                                 // "unused", now S4

  auto labelOf = [&](unsigned int shape) { return QString::fromStdString(f.puzzle().getShape(shape)->getName()); };
  auto check = [&] {
    const problem_c & q = *f.puzzle().getProblem(0);
    QCOMPARE(labelOf(q.getResultId()), QStringLiteral("result"));
    QCOMPARE(int(q.getNumberOfParts()), 2);
    for (unsigned int part = 0; part < 2; part++) {
      const QString l = labelOf(q.getShapeIdOfPart(part));
      QVERIFY2(l == QLatin1String("piece a") || l == QLatin1String("piece b"), qPrintable(l));
      QCOMPARE(int(q.getPartMaximum(part)), l == QLatin1String("piece a") ? 2 : 1);
      QCOMPARE(int(q.getPartMinimum(part)), 1);
    }
  };
  check();
  const QStringList before = f.saveAndReopen();
  QCOMPARE(describe(f.puzzle()), before);
  check();
}

void TestSaving::undoneEditsAreNotSavedAndASaveIsClean() {
  // the file holds the puzzle as undo left it; a save leaves the document
  // clean, the next edit makes it modified again, an undo past the save too
  SaveFixture f{ gridType_c::GT_BRICKS };
  EditorController * e = f.editor();
  VoxelGridItem g;
  g.setSize(QSizeF(480, 360));
  g.setEditor(e);
  const std::vector<QPoint> cells = layerCells(g);
  click(e, cells[0]);
  click(e, cells[1]);
  click(e, cells[2]);
  f.app->document()->undo();                         // the third voxel is not saved
  QCOMPARE(filledCells(f.puzzle()), 2);

  f.app->document()->saveAsFile(QUrl::fromLocalFile(f.path()));
  QVERIFY(!f.app->document()->modified());
  click(e, cells[3]);
  QVERIFY(f.app->document()->modified());
  f.app->document()->undo();
  QVERIFY2(!f.app->document()->modified(), "back where the save was");
  f.app->document()->undo();
  QVERIFY2(f.app->document()->modified(), "before the save");
  f.app->document()->redo();
  f.app->document()->save();                         // to the same file, without asking
  QVERIFY(!f.app->document()->modified());

  const QStringList before = f.saveAndReopen();
  QCOMPARE(describe(f.puzzle()), before);
  QCOMPARE(filledCells(f.puzzle()), 2);
}
