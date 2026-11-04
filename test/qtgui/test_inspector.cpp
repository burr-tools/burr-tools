/* Qt Test cases for the inspector under the Shapes list: its sections, the
 * Transform buttons per voxel type, running an operation as one undo step,
 * what is blocked and why, Apply to all shapes, and the 3D preview of a
 * Transform button.
 */
#include "test_inspector.h"

#include "app.h"
#include "editorcontroller.h"
#include "inspectorcontroller.h"
#include "viewportcontroller.h"

#include "../../src/lib/puzzle.h"
#include "../../src/lib/voxel.h"
#include "../../src/uicore/shapeops.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

namespace {

  // the brick buttons' legacy tasks (btui::opsDescriptor)
  constexpr int kNudgeXPos = 0, kNudgeXNeg = 1, kRotateXPos = 7, kRotateYPos = 8, kFlipX = 12;

  /* an App with brick shapes of 3x3x3, the first selected */
  struct InspectorFixture {
    QTemporaryDir dir;
    std::unique_ptr<App> app;

    explicit InspectorFixture(int shapes = 1, gridType_c::gridType type = gridType_c::GT_BRICKS) {
      app = std::make_unique<App>(dir.filePath(QStringLiteral("s.rc")), dir.filePath(QStringLiteral("l.rc")));
      app->document()->newDocument(int(type));
      app->document()->editStructure(0, [&](puzzle_c & p) {
        for (int i = 0; i < shapes; i++)
          p.addShape(3, 3, 3);
        return true;
      });
      app->shapes()->select(0);
    }

    InspectorController * inspector(void) const { return app->inspector(); }
    const voxel_c & shape(int i = 0) const { return *app->document()->session().puzzle().getShape(unsigned(i)); }
    int state(int x, int y, int z, int i = 0) const { return shape(i).getState(unsigned(x), unsigned(y), unsigned(z)); }
    void set(int x, int y, int z, int i = 0, int state = voxel_c::VX_FILLED) {
      app->document()->editStructure(-1, [&](puzzle_c & p) {   // the selection stays
        p.getShape(unsigned(i))->setState(unsigned(x), unsigned(y), unsigned(z), state);
        return true;
      });
    }
  };
}

void TestInspector::theSectionsRememberWhetherTheyAreOpen() {
  // AC-C03-01: every section closed until opened (user decision 2026-10-29); kept in the settings
  QTemporaryDir dir;
  const QString s = dir.filePath(QStringLiteral("s.rc")), l = dir.filePath(QStringLiteral("l.rc"));
  {
    App app(s, l);
    InspectorController * i = app.inspector();
    QVERIFY(!i->transformOpen());
    QVERIFY(!i->fitOpen());
    QVERIFY(!i->repairOpen());
    QSignalSpy changed(i, &InspectorController::sectionsChanged);
    i->setFitOpen(true);
    i->setFitOpen(false);
    i->setRepairOpen(true);
    QCOMPARE(changed.count(), 3);
    i->setRepairOpen(true);
    QCOMPARE(changed.count(), 3);
  }
  App app(s, l);
  QVERIFY(!app.inspector()->transformOpen());
  QVERIFY(!app.inspector()->fitOpen());
  QVERIFY(app.inspector()->repairOpen());
}

void TestInspector::eachVoxelTypeHasItsButtons() {
  // AC-C14-03: bricks 3 + 6 + 6 in a matrix with ×n and Minimize; spheres
  // the two-block layout without ×n; a new document's type takes over
  InspectorFixture f;
  InspectorController * i = f.inspector();
  QCOMPARE(i->transformButtons().size(), 15);
  QVERIFY(!i->pads());
  QVERIFY(i->scaleShown());
  const QVariantMap first = i->transformButtons().first().toMap();
  QCOMPARE(first.value(QStringLiteral("kind")).toString(), QStringLiteral("flip"));
  QVERIFY(!first.value(QStringLiteral("id")).toString().isEmpty());
  QVERIFY(!first.value(QStringLiteral("tip")).toString().isEmpty());

  // QML's task numbers (`tasks`) are the enum's
  const QVariantMap tasks = i->tasks();
  QCOMPARE(tasks.size(), 13);
  QCOMPARE(tasks.value(QStringLiteral("prune")).toInt(), int(InspectorController::Prune));
  QCOMPARE(tasks.value(QStringLiteral("minimize")).toInt(), int(InspectorController::Minimize));
  QCOMPARE(tasks.value(QStringLiteral("fillHoles")).toInt(), int(InspectorController::FillHoles));
  QCOMPARE(tasks.value(QStringLiteral("clearOuter")).toInt(), int(InspectorController::ClearOuter));

  QSignalSpy typeChanged(i, &InspectorController::typeChanged);
  f.app->document()->newDocument(int(gridType_c::GT_SPHERES));
  QCOMPARE(typeChanged.count(), 1);
  QVERIFY(i->pads());
  QVERIFY(!i->scaleShown());
  QCOMPARE(i->transformButtons().size(), int(btui::opsDescriptor(gridType_c::GT_SPHERES).buttons.size()));
}

void TestInspector::anOperationIsOneUndoStep() {
  // AC-C04-01: each press one step, undone at once
  InspectorFixture f;
  f.set(0, 1, 1);
  f.set(1, 1, 1);
  f.inspector()->run(kNudgeXPos);
  QCOMPARE(f.state(2, 1, 1), int(voxel_c::VX_FILLED));
  QCOMPARE(f.state(0, 1, 1), int(voxel_c::VX_EMPTY));
  f.inspector()->run(kFlipX);
  f.inspector()->run(kRotateYPos);          // quick presses are separate steps (AK_ENTITIES_STRUCTURAL)
  f.app->document()->undo();
  f.app->document()->undo();
  QCOMPARE(f.state(2, 1, 1), int(voxel_c::VX_FILLED));
  QCOMPARE(f.state(1, 1, 1), int(voxel_c::VX_FILLED));
  f.app->document()->undo();
  QCOMPARE(f.state(0, 1, 1), int(voxel_c::VX_FILLED));
  QCOMPARE(f.state(2, 1, 1), int(voxel_c::VX_EMPTY));
}

void TestInspector::aNudgeOutOfTheGridIsBlockedWithItsReason() {
  // OQ-7: the spec blocks what legacy would cut off, and says so
  InspectorFixture f;
  f.set(2, 1, 1);
  QSignalSpy flash(f.inspector(), &InspectorController::flashRequested);
  const bool couldUndo = f.app->document()->canUndo();
  QVERIFY(f.inspector()->allowed(kNudgeXPos));       // a Transform button is checked when pressed
  f.inspector()->run(kNudgeXPos);
  QCOMPARE(f.state(2, 1, 1), int(voxel_c::VX_FILLED));
  QCOMPARE(flash.count(), 1);
  QCOMPARE(flash.first().first().toString(), QStringLiteral("Blocked: the piece would leave the grid"));
  QCOMPARE(f.app->document()->canUndo(), couldUndo);
  f.app->document()->undo();                         // the set() above, not a blocked step
  QCOMPARE(f.state(2, 1, 1), int(voxel_c::VX_EMPTY));

  f.set(2, 1, 1);
  f.inspector()->run(kNudgeXNeg);                    // the other way is free
  QCOMPARE(f.state(1, 1, 1), int(voxel_c::VX_FILLED));
}

void TestInspector::fitAndScaleFollowApplyToAll() {
  // user decision P6: Fit & scale's operations honour Apply to all shapes; Transform stays per shape
  InspectorFixture f(2);
  f.set(2, 2, 2, 0);
  f.set(1, 2, 2, 1);
  f.app->editor()->setApplyToAll(true);
  f.inspector()->run(InspectorController::Origin);
  QCOMPARE(f.state(0, 0, 0, 0), int(voxel_c::VX_FILLED));
  QCOMPARE(f.state(0, 0, 0, 1), int(voxel_c::VX_FILLED));
  f.app->document()->undo();                         // both, one step
  QCOMPARE(f.state(2, 2, 2, 0), int(voxel_c::VX_FILLED));
  QCOMPARE(f.state(1, 2, 2, 1), int(voxel_c::VX_FILLED));

  f.inspector()->run(kNudgeXNeg);
  QCOMPARE(f.state(1, 2, 2, 0), int(voxel_c::VX_FILLED));
  QCOMPARE(f.state(1, 2, 2, 1), int(voxel_c::VX_FILLED));   // the other shape left as it was

  f.app->editor()->setApplyToAll(false);
  f.inspector()->run(InspectorController::Origin);
  QCOMPARE(f.state(0, 0, 0, 0), int(voxel_c::VX_FILLED));
  QCOMPARE(f.state(1, 2, 2, 1), int(voxel_c::VX_FILLED));
}

void TestInspector::theStatesFollowTheShape() {
  // AC-C03-03 / 04: disabled with the reason; worked out again once edits pause
  InspectorFixture f;
  InspectorController * i = f.inspector();
  QVERIFY(!i->allowed(InspectorController::Prune));
  QCOMPARE(i->reason(InspectorController::Prune), QStringLiteral("The shape is empty"));
  QCOMPARE(i->shape(), 0);

  QSignalSpy state(i, &InspectorController::stateChanged);
  for (int x = 0; x < 2; x++)
    for (int y = 0; y < 2; y++)
      for (int z = 0; z < 2; z++)
        f.set(x, y, z);
  QCOMPARE(state.count(), 0);                        // not per edit
  QTRY_COMPARE(state.count(), 1);
  QVERIFY(i->allowed(InspectorController::Prune));
  QVERIFY(i->allowed(InspectorController::Minimize));
  QCOMPARE(i->minimizeFactor(), 2);                  // "Reduce resolution ÷2"

  f.inspector()->run(InspectorController::Minimize);
  QCOMPARE(int(f.shape().countState(voxel_c::VX_FILLED)), 1);
  QTRY_VERIFY(!i->allowed(InspectorController::Minimize));
  QCOMPARE(i->reason(InspectorController::Minimize), QStringLiteral("Already at minimum resolution"));

  // a new selection shows its own states at once
  f.app->shapes()->addShape();               // selected
  QTRY_COMPARE(i->shape(), 1);
  QVERIFY(!i->allowed(InspectorController::Prune));
  const int before = state.count();
  f.app->shapes()->select(0);
  QVERIFY(state.count() > before);
  QCOMPARE(i->shape(), 0);
  QVERIFY(i->allowed(InspectorController::Prune));
}

void TestInspector::repairSaysWhatItChanged() {
  // AC-C05-01 / 04: Fill holes counts; Outer ▸ Variable changes the shell
  InspectorFixture f;
  f.app->document()->editStructure(0, [](puzzle_c & p) {
    voxel_c * v = p.getShape(0);
    for (unsigned x = 0; x < 3; x++)
      for (unsigned y = 0; y < 3; y++)
        for (unsigned z = 0; z < 3; z++)
          if (x != 1 || y != 1 || z != 1)
            v->setState(x, y, z, voxel_c::VX_FILLED);
    return true;
  });
  QSignalSpy flash(f.inspector(), &InspectorController::flashRequested);
  f.inspector()->run(InspectorController::FillHoles);
  QCOMPARE(f.state(1, 1, 1), int(voxel_c::VX_FILLED));
  QCOMPARE(flash.last().first().toString(), QStringLiteral("Filled 1 interior voxel"));
  f.inspector()->run(InspectorController::FillHoles);
  QCOMPARE(flash.last().first().toString(), QStringLiteral("No interior holes found"));

  f.inspector()->run(InspectorController::VariableOuter);
  QCOMPARE(flash.last().first().toString(), QStringLiteral("26 outer voxels updated"));
  QCOMPARE(f.state(0, 0, 0), int(voxel_c::VX_VARIABLE));
  QCOMPARE(f.state(1, 1, 1), int(voxel_c::VX_FILLED));
}

void TestInspector::aTransformButtonPreviewsItsResult() {
  // user decision P6 (legacy previewTransform): hovering a Transform button
  // shows the result as a ghost and the hint; leaving clears it
  InspectorFixture f;
  f.set(1, 1, 1);
  f.set(2, 1, 1);
  ViewportController * v = f.app->viewport();
  v->setViewportSize(320, 240);
  InspectorController * i = f.inspector();

  i->preview(kRotateXPos);
  QVERIFY(v->transformPreviewShown());
  SceneFrame fr = v->frame(1);
  QVERIFY(!fr.ghostFaces.empty());
  QVERIFY(!fr.hintLines.empty());

  // a blocked nudge: the hint, but no ghost
  i->preview(kNudgeXPos);
  fr = v->frame(1);
  QVERIFY(fr.ghostFaces.empty());
  QVERIFY(!fr.hintLines.empty());

  // pressed while previewed: the preview follows the shape -- here to the
  // grid's edge, where the next −X nudge is blocked
  i->preview(kNudgeXNeg);
  QVERIFY(!v->frame(1).ghostFaces.empty());
  i->run(kNudgeXNeg);
  QVERIFY(v->transformPreviewShown());
  fr = v->frame(1);
  QVERIFY(fr.ghostFaces.empty());
  QVERIFY(!fr.hintLines.empty());

  i->clearPreview();
  QVERIFY(!v->transformPreviewShown());
  fr = v->frame(1);
  QVERIFY(fr.ghostFaces.empty());
  QVERIFY(fr.hintLines.empty());

  // a button that is not a transform previews nothing
  i->preview(InspectorController::Prune);
  QVERIFY(!v->transformPreviewShown());
}
