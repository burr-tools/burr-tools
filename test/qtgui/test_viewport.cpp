/* Qt Test cases for the 3D view: the controller's frames and gestures, the
 * view cube's painting, and real renders through an offscreen QRhi.
 *
 * TestViewportItem draws the real 3D view item the same way: Qt Quick
 * renders it through a QQuickRenderControl into the renderer's own QRhi.
 *
 * Render tests draw into a texture with no window, through the same
 * OffscreenRenderer the image export uses: Direct3D 11 on its WARP software
 * rasteriser on Windows (the same pixels on every machine, no GPU needed),
 * OpenGL elsewhere, or what BURRTOOLS_OFFSCREEN_RHI asks for (Linux CI:
 * vulkan, on Mesa's lavapipe). Where no backend can be made they skip, or
 * fail when BURRTOOLS_REQUIRE_GPU_TESTS is set (test_gpu.h). Renders are also
 * written next to the test binary (build/test/qtgui/render-*.png) for a look
 * by eye.
 *
 * They assert on pixels the spec fixes -- the background is the canvas
 * token, the shape covers the centre, the slab tints its layer -- rather
 * than comparing whole images, so driver rounding and anti-aliasing do not
 * make them flaky.
 */
#include "test_viewport.h"

#include "app.h"
#include "offscreenrenderer.h"
#include "pipelinecache.h"
#include "settingscontroller.h"
#include "test_gpu.h"
#include "scenerenderer.h"
#include "theme.h"
#include "viewcubeitem.h"
#include "viewportcontroller.h"
#include "voxelviewport.h"

#include "../../src/lib/puzzle.h"
#include "../../src/lib/voxel.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QFile>
#include <QQuickGraphicsConfiguration>
#include <QQuickGraphicsDevice>
#include <QQuickRenderControl>
#include <QQuickRenderTarget>
#include <QQuickWindow>
#include <rhi/qrhi.h>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <cmath>
#include <memory>

namespace {

  struct AppFixture {
    QTemporaryDir dir;
    std::unique_ptr<App> app;

    AppFixture() {
      app = std::make_unique<App>(dir.filePath(QStringLiteral("s.rc")), dir.filePath(QStringLiteral("l.rc")));
      app->viewport()->setViewportSize(320, 240);
    }

    bool load(const char * file) { return app->document()->loadPath(QString::fromLatin1(file)); }
  };

  /* A renderer without a window and the size to draw at. Software where the
   * platform has a software rasteriser, so the pixels are the same everywhere. */
  struct OffscreenTarget {
    OffscreenRenderer renderer{ OffscreenRenderer::Backend::Software };
    QSize size;

    bool create(QSize s) {
      size = s;
      return renderer.isValid();
    }

    QImage render(const SceneFrame & f) { return renderer.render(f, size); }
  };

  bool near(QColor a, QColor b, int tol = 6) {
    return std::abs(a.red() - b.red()) <= tol && std::abs(a.green() - b.green()) <= tol &&
           std::abs(a.blue() - b.blue()) <= tol;
  }

  /* next to the test binary, in the build tree -- never in the sources */
  void saveForInspection(const QImage & img, const char * name) {
    img.save(QCoreApplication::applicationDirPath() + QStringLiteral("/render-%1.png").arg(QString::fromLatin1(name)));
  }
}

// ---------------------------------------------------------------------------

void TestViewportController::framesFollowTheDisplayOptions() {
  AppFixture f;
  QVERIFY(f.load("examples/PelikanBurr.xmpuzzle"));
  ViewportController * v = f.app->viewport();
  QVERIFY(v->hasShape());

  // defaults (C06): axes, bounds and slab on, dimming off
  SceneFrame fr = v->frame(1);
  QVERIFY(fr.mesh);
  QCOMPARE(fr.dimAxis, -1);
  // 3 axes + 12 dashed boundary edges + 12 slab edges
  QCOMPARE(int(fr.lines.size()), 3 + 12 + 12);
  QCOMPARE(int(fr.overlayFaces.size()), 36);

  v->setDisplayAxes(false);
  v->setDisplayBounds(false);
  fr = v->frame(1);
  QCOMPARE(int(fr.lines.size()), 12);

  v->setDisplayDimOtherLayers(true);
  v->setPlane(1);   // XZ: layers run along Y
  QCOMPARE(v->frame(1).dimAxis, 1);
}

void TestViewportController::slabAndDimOnlyWithTheEditorVisible() {
  // T-C06-9 / AC-C11-08
  AppFixture f;
  QVERIFY(f.load("examples/PelikanBurr.xmpuzzle"));
  ViewportController * v = f.app->viewport();
  v->setDisplayDimOtherLayers(true);
  QVERIFY(!v->frame(1).overlayFaces.empty());

  f.app->layout()->setRightCollapsed(true);
  QVERIFY(!v->editorVisible());
  SceneFrame fr = v->frame(1);
  QVERIFY(fr.overlayFaces.empty());
  QCOMPARE(fr.dimAxis, -1);

  f.app->layout()->setRightCollapsed(false);
  f.app->layout()->toggleFocus3d();
  QVERIFY(v->frame(1).overlayFaces.empty());
  f.app->layout()->toggleFocus3d();
  f.app->layout()->toggleFocus2d();
  QVERIFY(!v->frame(1).overlayFaces.empty());
}

void TestViewportController::displayOptionsPersist() {
  QTemporaryDir dir;
  const QString s = dir.filePath(QStringLiteral("s.rc")), l = dir.filePath(QStringLiteral("l.rc"));
  {
    App app(s, l);
    app.viewport()->setDisplayAxes(false);
    app.viewport()->setProjection(QStringLiteral("orthographic"));
    app.viewport()->setColourView(QStringLiteral("voxel"));
  }
  App app(s, l);
  QCOMPARE(app.viewport()->displayAxes(), false);
  QCOMPARE(app.viewport()->projection(), QStringLiteral("orthographic"));
  QCOMPARE(app.viewport()->colourView(), QStringLiteral("voxel"));
  QCOMPARE(app.viewport()->camera().projection(), btui::Camera::Projection::Orthographic);
}

void TestViewportController::onlyEntitiesShowsTheShape() {
  AppFixture f;
  QVERIFY(f.load("examples/PelikanBurr.xmpuzzle"));
  f.app->layout()->setWorkspace(LayoutController::Puzzle);
  SceneFrame fr = f.app->viewport()->frame(1);
  QVERIFY(!fr.mesh);
  QVERIFY(fr.lines.empty());
}

void TestViewportController::aClickWithinTheSlopDoesNotNavigate() {
  // T-C06-3: press, move 3 dp, release -> a click; no orbit
  AppFixture f;
  QVERIFY(f.load("examples/PelikanBurr.xmpuzzle"));
  ViewportController * v = f.app->viewport();
  const btui::Quat before = v->camera().orientation();
  QSignalSpy clicked(v, &ViewportController::clicked);
  v->pointerPress(Qt::LeftButton, 100, 100, {});
  v->pointerMove(103, 100);
  v->pointerRelease(Qt::LeftButton, 103, 100);
  QCOMPARE(clicked.count(), 1);
  QVERIFY(btui::angleBetween(before, v->camera().orientation()) < 1e-6f);
}

void TestViewportController::dragsOrbitAndPanByMode() {
  // T-C06-7: Orbit mode: left orbits, Shift+left and middle pan;
  // Pan mode: left pans, Shift+left orbits
  for (const char * mode : { "orbit", "pan" }) {
    AppFixture f;
    QVERIFY(f.load("examples/PelikanBurr.xmpuzzle"));
    ViewportController * v = f.app->viewport();
    v->setNavMode(QString::fromLatin1(mode));
    const bool panMode = QByteArray(mode) == "pan";

    auto drag = [&](Qt::MouseButton b, Qt::KeyboardModifiers m) {
      const btui::Quat q0 = v->camera().orientation();
      const float px = v->camera().panX();
      v->pointerPress(b, 100, 100, m);
      v->pointerMove(140, 110);
      v->pointerRelease(b, 140, 110);
      const bool rotated = btui::angleBetween(q0, v->camera().orientation()) > 1e-4f;
      const bool panned = std::fabs(v->camera().panX() - px) > 1e-6f;
      return std::pair{ rotated, panned };
    };

    auto plain = drag(Qt::LeftButton, {});
    QCOMPARE(plain.first, !panMode);
    QCOMPARE(plain.second, panMode);
    auto shifted = drag(Qt::LeftButton, Qt::ShiftModifier);
    QCOMPARE(shifted.first, panMode);
    QCOMPARE(shifted.second, !panMode);
    auto middle = drag(Qt::MiddleButton, {});
    QCOMPARE(middle.first, false);
    QCOMPARE(middle.second, true);
    // the right button is reserved
    auto right = drag(Qt::RightButton, {});
    QCOMPARE(right.first, false);
    QCOMPARE(right.second, false);
  }
}

void TestViewportController::theWheelZoomsAndHonoursReverseScroll() {
  AppFixture f;
  ViewportController * v = f.app->viewport();
  const float z = v->camera().zoomTarget();
  v->wheel(120);                       // away from the user: zoom in
  QVERIFY(v->camera().zoomTarget() > z);
  f.app->settings()->setReverseScroll(true);
  const float z2 = v->camera().zoomTarget();
  v->wheel(120);
  QVERIFY(v->camera().zoomTarget() < z2);
}

void TestViewportController::theCubeDrivesTheCamera() {
  AppFixture f;
  ViewportController * v = f.app->viewport();
  btui::ViewCube::Hit h;
  h.kind = btui::ViewCube::Kind::Region;
  h.dir = { 1, 0, 0 };
  v->cubeClick(h);
  QVERIFY(v->camera().animating());
  for (int i = 0; i < 30; i++)
    v->tick(16);
  QVERIFY(!v->camera().animating());
  QVERIFY(btui::dot(btui::viewAxisInWorld(v->camera().orientation(), 2), btui::Vec3{ 1, 0, 0 }) > 0.9999f);

  h.kind = btui::ViewCube::Kind::Home;
  v->cubeClick(h);
  for (int i = 0; i < 30; i++)
    v->tick(16);
  QVERIFY(btui::angleBetween(v->camera().orientation(), btui::Camera::homeOrientation()) < 1e-3f);
}

void TestViewportController::layerStepsClampAndFollowThePlane() {
  // T-C08-1 (plane/layer part): a 4x3x5 shape has 5/3/4 layers
  AppFixture f;
  puzzle_c & p = f.app->document()->session().puzzle();
  p.addShape(4, 3, 5);
  f.app->shapes()->refresh();
  f.app->shapes()->select(0);
  ViewportController * v = f.app->viewport();
  QCOMPARE(v->layerCount(), 5);
  v->setLayer(4);
  v->setPlane(1);
  QCOMPARE(v->layerCount(), 3);
  QCOMPARE(v->layer(), 2);           // clamped
  v->layerStep(+5);
  QCOMPARE(v->layer(), 2);
  v->layerStep(-1);
  QCOMPARE(v->layer(), 1);
  v->setPlane(2);
  QCOMPARE(v->layerCount(), 4);
}

// ---------------------------------------------------------------------------

void TestViewCubePaint::drawsTheCubeAndHighlightsTheHoveredRegion() {
  for (bool persp : { false, true }) {
    QImage img(int(btui::ViewCube::kWidth), int(btui::ViewCube::kHeight), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    {
      QPainter p(&img);
      btui::ViewCube::Hit hover;
      hover.kind = btui::ViewCube::Kind::Region;
      hover.dir = { 0, 0, 1 };
      ViewCubeItem::paintCube(&p, btui::Camera::homeOrientation(), persp, hover, false);
    }
    // the +Z face centre is highlighted (#4f7df0); somewhere off the cube
    // stays transparent
    const QPointF c = [&] {
      btui::Vec3 s = btui::ViewCube::project(btui::Camera::homeOrientation(), persp, { 0.3f, -0.3f, 1 });
      return QPointF(s.x, s.y);
    }();
    QVERIFY(near(img.pixelColor(c.toPoint()), QColor(0x4f, 0x7d, 0xf0), 10));
    QCOMPARE(img.pixelColor(150, 165).alpha(), 0);
    saveForInspection(img, persp ? "viewcube-perspective" : "viewcube-orthographic");
  }
}

void TestViewCubePaint::showsTheArrowsOnlyFaceAligned() {
  auto paint = [](btui::Quat q) {
    QImage img(int(btui::ViewCube::kWidth), int(btui::ViewCube::kHeight), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    ViewCubeItem::paintCube(&p, q, true, {}, false);
    return img;
  };
  const QPoint rightArrow(int(btui::ViewCube::kCentreX + btui::ViewCube::kArrowOffset + 2), int(btui::ViewCube::kCentreY));
  QVERIFY(paint(btui::Camera::lookFrom({ 0, 0, 1 }, 0)).pixelColor(rightArrow).alpha() > 0);
  QCOMPARE(paint(btui::Camera::homeOrientation()).pixelColor(rightArrow).alpha(), 0);
}

// ---------------------------------------------------------------------------

void TestViewportController::theVoxelStyleSettingRebuildsTheMesh() {
  // Settings ▸ 3D view ▸ Voxel style: Flat (default) has outlined faces,
  // Classic legacy's bevelled checker; switching rebuilds the shape's mesh
  AppFixture f;
  QVERIFY(f.load("examples/PelikanBurr.xmpuzzle"));
  ViewportController * v = f.app->viewport();
  QCOMPARE(f.app->settings()->voxelStyle(), QStringLiteral("flat"));
  auto outlined = [](const SceneFrame & fr) {
    for (const auto & vert : fr.mesh->opaque)
      if (vert.edge[0] < 1.5f || vert.edge[1] < 1.5f || vert.edge[2] < 1.5f)
        return true;
    return false;
  };
  const SceneFrame flat = v->frame(1);
  QVERIFY(flat.mesh);
  QVERIFY(outlined(flat));

  QSignalSpy rebuilt(v, &ViewportController::sceneChanged);
  f.app->settings()->setVoxelStyle(QStringLiteral("legacy"));
  QCOMPARE(rebuilt.count(), 1);
  const SceneFrame classic = v->frame(1);
  QVERIFY(classic.meshRevision != flat.meshRevision);
  QVERIFY(!outlined(classic));
  QVERIFY(classic.mesh->opaque.size() > flat.mesh->opaque.size());   // the bevels

  // another setting leaves the mesh alone
  f.app->settings()->setLighting(false);
  QCOMPARE(rebuilt.count(), 1);
  f.app->settings()->setLighting(true);

  f.app->settings()->resetSection(QStringLiteral("view3d"));
  QCOMPARE(f.app->settings()->voxelStyle(), QStringLiteral("flat"));
  QVERIFY(outlined(v->frame(1)));
}

void TestViewCubePaint::theItemHasRoomForTheAxisLabelsAndHitsThroughIt() {
  // the item is the design's 156 x 170 plus room at the left and the bottom
  // for the axis labels; a press is mapped back into the design's dp
  AppFixture f;
  ViewCubeItem item;
  QCOMPARE(item.implicitWidth(), double(btui::ViewCube::kWidth + btui::ViewCube::kPadLeft));
  QCOMPARE(item.implicitHeight(), double(btui::ViewCube::kHeight + btui::ViewCube::kPadBottom));
  item.setSize(QSizeF(item.implicitWidth(), item.implicitHeight()));
  item.setController(f.app->viewport());
  const btui::Quat q = f.app->viewport()->camera().orientation();
  const bool persp = f.app->viewport()->camera().projection() == btui::Camera::Projection::Perspective;
  // the +Z face's centre, in the design's coordinates, then in the item's
  const btui::Vec3 c = btui::ViewCube::project(q, persp, { 0, 0, 1 });
  const btui::ViewCube::Hit hit = item.hitAt(QPointF(c.x + btui::ViewCube::kPadLeft, c.y));
  QCOMPARE(int(hit.kind), int(btui::ViewCube::Kind::Region));
  QVERIFY(hit.dir == (btui::CubeDir{ 0, 0, 1 }));
  // the same point without the padding is somewhere else on the cube or off it
  QVERIFY(!(item.hitAt(QPointF(c.x, c.y)).dir == (btui::CubeDir{ 0, 0, 1 })) ||
          item.hitAt(QPointF(c.x, c.y)).kind != btui::ViewCube::Kind::Region);
  // the padding itself holds nothing to press
  QCOMPARE(int(item.hitAt(QPointF(4, 4)).kind), int(btui::ViewCube::Kind::None));
}

void TestRender::rendersTheShapeOnTheCanvas() {
  OffscreenTarget t;
  GPU_OR_SKIP(t.create(QSize(320, 240)));
  AppFixture f;
  QVERIFY(f.load("examples/PelikanBurr.xmpuzzle"));
  Theme::instance()->setMode(QStringLiteral("light"));
  QImage img = t.render(f.app->viewport()->frame(1));
  QVERIFY(!img.isNull());
  saveForInspection(img, "pelikan-light");

  // the corners are the canvas token, the centre is the shape (blue S1)
  const QColor canvas = Theme::instance()->canvas();
  QVERIFY(near(img.pixelColor(2, 2), canvas));
  QVERIFY(near(img.pixelColor(317, 237), canvas));
  const QColor mid = img.pixelColor(160, 120);
  QVERIFY(!near(mid, canvas, 20));
  QVERIFY(mid.blue() > mid.red() + 60);
}

void TestRender::lightingChangesTheShading() {
  OffscreenTarget t;
  GPU_OR_SKIP(t.create(QSize(320, 240)));
  AppFixture f;
  QVERIFY(f.load("examples/PelikanBurr.xmpuzzle"));
  SceneFrame lit = f.app->viewport()->frame(1);
  lit.lines.clear();
  lit.overlayFaces.clear();
  SceneFrame flat = lit;
  flat.lighting = false;
  const QImage a = t.render(lit), b = t.render(flat);
  QVERIFY(a.pixelColor(160, 120) != b.pixelColor(160, 120));
}

void TestRender::translucencyBlendsInLinearLight() {
  // Gamma-correct rendering: a white face at 50 % over black is half the
  // light, which sRGB encodes as about 188 (blending the stored sRGB values
  // would give 128). Unlit faces are shaded 0.92 first, as the mock does.
  OffscreenTarget t;
  GPU_OR_SKIP(t.create(QSize(64, 64)));
  auto mesh = std::make_shared<btui::ShapeMesh>();
  const float tri[3][2] = { { -1, -1 }, { 3, -1 }, { -1, 3 } };    // counter-clockwise, covers the view
  for (const auto & p : tri) {
    btui::MeshVertex v;
    v.pos[0] = p[0]; v.pos[1] = p[1]; v.pos[2] = 0.5f;
    v.normal[2] = 1;
    v.color = { 255, 255, 255, 128 };
    mesh->translucent.push_back(v);
  }
  SceneFrame fr;
  fr.clear = Qt::black;
  fr.mesh = mesh;
  fr.meshRevision = 1;
  fr.lighting = false;
  const QImage img = t.render(fr);
  QVERIFY(!img.isNull());
  if (!t.renderer.linearLight())
    QSKIP("the device cannot render to RGBA16F: blended as stored");
  auto lin = [](double c) { return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); };
  auto enc = [](double c) { return c <= 0.0031308 ? c * 12.92 : 1.055 * std::pow(c, 1 / 2.4) - 0.055; };
  const int expected = int(std::lround(255 * enc((128.0 / 255) * lin(0.92))));
  const QColor px = img.pixelColor(32, 32);
  QVERIFY2(std::abs(px.red() - expected) <= 3 && std::abs(px.green() - expected) <= 3,
           qPrintable(QStringLiteral("%1, expected %2").arg(px.red()).arg(expected)));
}

void TestRender::innerVariableVoxelsShowThroughTheOuterOnes() {
  // The flat style: two variable voxels one behind the other, red and green.
  // Every translucent layer is blended, farthest first: the pixel takes
  // both (alpha 1 - (1 - a)^2) and the near voxel's colour leads -- from
  // either side, so the sorting follows the view. Legacy's rule (nearest
  // surface only) gives one layer.
  OffscreenTarget t;
  GPU_OR_SKIP(t.create(QSize(64, 64)));
  AppFixture f;
  puzzle_c & p = f.app->document()->session().puzzle();
  // voxels take colour ids, which run from 1: addColor's index + 1
  const unsigned red = p.addColor(255, 0, 0) + 1, green = p.addColor(0, 255, 0) + 1;
  p.addShape(1, 1, 2);
  voxel_c * v = p.getShape(p.getNumberOfShapes() - 1);
  v->setState(0, 0, 0, voxel_c::VX_VARIABLE);
  v->setState(0, 0, 1, voxel_c::VX_VARIABLE);
  v->setColor(0, 0, 0, green);          // the back, seen from +z
  v->setColor(0, 0, 1, red);
  btui::MeshOptions opt;
  opt.colors = btui::ColorMode::Voxel;
  for (unsigned i = 0; i < p.colorNumber(); i++) {
    unsigned char r, g, b;
    p.getColor(i, &r, &g, &b);
    opt.palette.push_back({ r / 255.0f, g / 255.0f, b / 255.0f });
  }
  auto mesh = std::make_shared<btui::ShapeMesh>(btui::buildShapeMesh(*v, opt));
  QVERIFY(!mesh->translucent.empty());

  auto look = [&](btui::Vec3 from, bool layers) {
    btui::Camera cam;
    cam.setViewport(64, 64);
    cam.setScene((mesh->boundsMin + mesh->boundsMax) * 0.5f, 1.2f);
    cam.setOrientation(btui::Camera::lookFrom(from, 0));
    SceneFrame fr;
    fr.clear = QColor(0, 0, 0, 0);
    fr.view = cam.viewMatrix();
    fr.projection = cam.projectionMatrix();
    fr.mesh = mesh;
    fr.meshRevision = 1;
    fr.lighting = false;
    fr.translucentLayers = layers;
    return t.render(fr).pixelColor(32, 32);
  };
  const double a = opt.variableAlpha;
  const QColor front = look({ 0, 0, 1 }, true);
  QVERIFY2(std::abs(front.alphaF() - (1 - (1 - a) * (1 - a))) < 0.03, qPrintable(QString::number(front.alphaF())));
  QVERIFY2(front.red() > front.green() && front.green() > 0,
           qPrintable(QStringLiteral("red %1 green %2").arg(front.red()).arg(front.green())));
  const QColor back = look({ 0, 0, -1 }, true);
  QVERIFY2(back.green() > back.red() && back.red() > 0,
           qPrintable(QStringLiteral("red %1 green %2").arg(back.red()).arg(back.green())));
  const QColor nearest = look({ 0, 0, 1 }, false);
  QVERIFY2(std::abs(nearest.alphaF() - a) < 0.03, qPrintable(QString::number(nearest.alphaF())));
  QCOMPARE(nearest.green(), 0);
}

void TestRender::theFlatStyleOutlinesEveryVoxelFace() {
  // C06 / the mock: each voxel face stroked darker; Classic (legacy's
  // bevels and checker) has no such lines. Rendered with and without the
  // outline, only the outline pixels change -- and they get darker.
  OffscreenTarget t;
  GPU_OR_SKIP(t.create(QSize(320, 240)));
  AppFixture f;
  QVERIFY(f.load("examples/PelikanBurr.xmpuzzle"));
  Theme::instance()->setMode(QStringLiteral("light"));
  ViewportController * v = f.app->viewport();
  v->setDisplayAxes(false);
  v->setDisplayBounds(false);
  QTRY_VERIFY_WITH_TIMEOUT(!v->animating(), 3000);

  auto compareOutline = [&](int * darker, int * lighter) {
    SceneFrame fr = v->frame(1);
    fr.lines.clear();
    fr.overlayFaces.clear();
    const QImage with = t.render(fr);
    fr.outlineStrength = 0;
    const QImage without = t.render(fr);
    *darker = *lighter = 0;
    for (int y = 0; y < with.height(); y++)
      for (int x = 0; x < with.width(); x++) {
        const QColor a = with.pixelColor(x, y), b = without.pixelColor(x, y);
        const int d = (a.red() + a.green() + a.blue()) - (b.red() + b.green() + b.blue());
        if (d < -12) (*darker)++;
        if (d > 12) (*lighter)++;
      }
    return with;
  };

  int darker = 0, lighter = 0;
  const QImage flat = compareOutline(&darker, &lighter);
  saveForInspection(flat, "pelikan-flat");
  // an 8x8x8 cube shows three faces of 64 voxel faces each: many lines
  QVERIFY2(darker > 1500, qPrintable(QStringLiteral("%1 outline pixels").arg(darker)));
  QCOMPARE(lighter, 0);

  f.app->settings()->setVoxelStyle(QStringLiteral("legacy"));
  QTRY_VERIFY_WITH_TIMEOUT(!v->animating(), 3000);
  saveForInspection(compareOutline(&darker, &lighter), "pelikan-classic");
  QCOMPARE(darker, 0);
  QCOMPARE(lighter, 0);
  f.app->settings()->setVoxelStyle(QStringLiteral("flat"));
}

void TestRender::darkThemeUsesTheDarkCanvas() {
  OffscreenTarget t;
  GPU_OR_SKIP(t.create(QSize(160, 120)));
  AppFixture f;
  Theme::instance()->setMode(QStringLiteral("dark"));
  QImage img = t.render(f.app->viewport()->frame(1));
  QVERIFY(near(img.pixelColor(80, 60), Theme::instance()->canvas()));
  Theme::instance()->setMode(QStringLiteral("light"));
}

void TestRender::theSlabTintsItsLayer() {
  // a flat 6x6x1 plate seen from the front: the slab of its only layer
  // covers it, and with the slab off the pixels differ
  OffscreenTarget t;
  GPU_OR_SKIP(t.create(QSize(320, 240)));
  AppFixture f;
  puzzle_c & p = f.app->document()->session().puzzle();
  p.addShape(6, 6, 2);
  for (unsigned x = 0; x < 6; x++)
    for (unsigned y = 0; y < 6; y++)
      p.getShape(0)->setState(x, y, 0, voxel_c::VX_FILLED);
  f.app->shapes()->refresh();
  f.app->shapes()->select(0);
  ViewportController * v = f.app->viewport();
  v->setLayer(1);                 // the empty upper layer: the slab sits in front of the plate
  v->camera().setOrientation(btui::Camera::lookFrom({ 0, 0, 1 }, 0));
  const QImage with = t.render(v->frame(1));
  v->setDisplayLayerSlab(false);
  const QImage without = t.render(v->frame(1));
  saveForInspection(with, "slab-on");
  const QColor a = with.pixelColor(160, 120), b = without.pixelColor(160, 120);
  QVERIFY(a != b);
  // the tint pulls toward the accent (blue)
  QVERIFY(a.blue() >= b.blue());
}

void TestRender::everyGridTypeRenders() {
  // T-C14 / AC-C18-05 for the Entities view: one example of each grid type
  OffscreenTarget t;
  GPU_OR_SKIP(t.create(QSize(240, 180)));
  const char * files[] = {
    "examples/PelikanBurr.xmpuzzle", "examples/Bermuda.xmpuzzle", "examples/BallRoom.xmpuzzle",
    "examples/12PieceSeparation.xmpuzzle", "examples/FourPieceTetrahedron.xmpuzzle",
  };
  for (const char * file : files) {
    AppFixture f;
    f.app->viewport()->setViewportSize(240, 180);
    QVERIFY2(f.load(file), file);
    SceneFrame fr = f.app->viewport()->frame(1);
    QVERIFY2(fr.mesh && !fr.mesh->empty(), file);
    QImage img = t.render(fr);
    // some pixel of the picture is not canvas
    const QColor canvas = fr.clear;
    int drawn = 0;
    for (int y = 0; y < img.height(); y += 4)
      for (int x = 0; x < img.width(); x += 4)
        if (!near(img.pixelColor(x, y), canvas, 12))
          drawn++;
    QVERIFY2(drawn > 50, file);
    saveForInspection(img, QFileInfo(QString::fromLatin1(file)).baseName().toLatin1().constData());
  }
}

void TestRender::orthographicDiffersFromPerspective() {
  OffscreenTarget t;
  GPU_OR_SKIP(t.create(QSize(320, 240)));
  AppFixture f;
  QVERIFY(f.load("examples/PelikanBurr.xmpuzzle"));
  const QImage persp = t.render(f.app->viewport()->frame(1));
  f.app->viewport()->setProjection(QStringLiteral("orthographic"));
  const QImage ortho = t.render(f.app->viewport()->frame(1));
  saveForInspection(ortho, "pelikan-ortho");
  QVERIFY(persp != ortho);
}

void TestRender::theStlPreviewDrawsTheMeshAndItsInsides() {
  OffscreenTarget t;
  GPU_OR_SKIP(t.create(QSize(320, 240)));
  AppFixture f;
  QVERIFY(f.load("examples/PelikanBurr.xmpuzzle"));
  Theme::instance()->setMode(QStringLiteral("light"));
  StlExportController * stl = f.app->stl();
  stl->setViewportSize(320, 240);
  stl->begin();
  stl->setShape(2);     // a solid piece: the centre of the view is covered

  const QImage solid = t.render(stl->frame(1));
  QVERIFY(!solid.isNull());
  saveForInspection(solid, "stl-preview");
  const QColor canvas = Theme::instance()->canvas();
  QVERIFY(near(solid.pixelColor(2, 2), canvas));
  const QColor mid = solid.pixelColor(160, 120);
  QVERIFY(!near(mid, canvas, 20));
  // one grey: no piece colour
  QVERIFY(std::abs(mid.red() - mid.blue()) < 12);

  stl->setInsides(true);
  const QImage xray = t.render(stl->frame(1));
  saveForInspection(xray, "stl-preview-insides");
  const QColor see = xray.pixelColor(160, 120);
  // see-through: between the solid grey and the canvas
  QVERIFY(!near(see, canvas, 4));
  QVERIFY(see.lightness() > mid.lightness() + 10);
  stl->end();
}

// ---------------------------------------------------------------------------

namespace {

  /* Qt Quick without a window: a QQuickRenderControl drawing into a texture
   * of the offscreen renderer's QRhi, read back after every frame. The
   * window's own colour is magenta, so wherever an item fails to draw, it
   * shows. */
  struct QuickTarget {
    OffscreenRenderer renderer{ OffscreenRenderer::Backend::Software };
    QQuickRenderControl control;
    std::unique_ptr<QQuickWindow> window;
    std::unique_ptr<QRhiTexture> tex;
    std::unique_ptr<QRhiRenderBuffer> depth;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    std::unique_ptr<QRhiTextureRenderTarget> rt;

    ~QuickTarget() {
      // the item's resources go with the window, before the device
      window.reset();
      rt.reset();
      rp.reset();
      depth.reset();
      tex.reset();
    }

    bool create(QSize size) {
      QRhi * rhi = renderer.rhi();
      if (!rhi)
        return false;
      // Qt Quick's RHI renderer (main() chose it) for the device's API
      switch (rhi->backend()) {
        case QRhi::D3D11: QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11); break;
        case QRhi::Vulkan: QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan); break;
        case QRhi::Metal: QQuickWindow::setGraphicsApi(QSGRendererInterface::Metal); break;
        case QRhi::OpenGLES2: QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL); break;
        default: return false;
      }
      window = std::make_unique<QQuickWindow>(&control);
#if QT_CONFIG(vulkan)
      if (QVulkanInstance * vk = renderer.vulkanInstance())
        window->setVulkanInstance(vk);
#endif
      window->setGraphicsDevice(QQuickGraphicsDevice::fromRhi(rhi));
      if (!control.initialize())
        return false;
      tex.reset(rhi->newTexture(QRhiTexture::RGBA8, size, 1, QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
      depth.reset(rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, size, 1));
      if (!tex->create() || !depth->create())
        return false;
      QRhiTextureRenderTargetDescription desc{ QRhiColorAttachment(tex.get()) };
      desc.setDepthStencilBuffer(depth.get());
      rt.reset(rhi->newTextureRenderTarget(desc));
      rp.reset(rt->newCompatibleRenderPassDescriptor());
      rt->setRenderPassDescriptor(rp.get());
      if (!rt->create())
        return false;
      window->setRenderTarget(QQuickRenderTarget::fromRhiRenderTarget(rt.get()));
      window->setGeometry(0, 0, size.width(), size.height());
      window->contentItem()->setSize(size);
      window->setColor(Qt::magenta);
      return true;
    }

    QImage frame(void) {
      QRhi * rhi = renderer.rhi();
      control.polishItems();
      control.beginFrame();
      control.sync();
      control.render();
      QRhiReadbackResult rb;
      QRhiResourceUpdateBatch * u = rhi->nextResourceUpdateBatch();
      u->readBackTexture({ tex.get() }, &rb);
      control.commandBuffer()->resourceUpdate(u);
      control.endFrame();     // an offscreen frame: waits for the readback
      QImage img(reinterpret_cast<const uchar *>(rb.data.constData()), rb.pixelSize.width(), rb.pixelSize.height(),
                 QImage::Format_RGBA8888_Premultiplied);
      if (rhi->isYUpInFramebuffer())
        img = img.flipped(Qt::Vertical);
      return img.copy();
    }
  };

  bool isMagenta(QColor c) {
    return c.red() > 230 && c.green() < 25 && c.blue() > 230;
  }
}

void TestViewportItem::redrawsAtTheFinalSizeAfterTheCardGrows() {
  // AC-C11-07: when a side card collapses, the centre card grows over the
  // 200 ms transition; afterwards the 3D view must show a fresh frame at
  // its final size -- not the old frame stretched, nor the window behind it
  AppFixture f;
  QVERIFY(f.load("examples/PelikanBurr.xmpuzzle"));
  Theme::instance()->setMode(QStringLiteral("light"));
  ViewportController * v = f.app->viewport();
  QTRY_VERIFY_WITH_TIMEOUT(!v->animating(), 3000);

  const QSize full(720, 300);
  QuickTarget t;
  GPU_OR_SKIP(t.create(full));

  auto * view = new VoxelViewport(t.window->contentItem());
  view->setController(v);
  view->setSize(QSizeF(400, 300));
  const QImage before = t.frame();
  QCOMPARE(view->effectiveColorBufferSize(), QSize(400, 300));
  QVERIFY(isMagenta(before.pixelColor(600, 150)));          // the window, right of the view
  QVERIFY(near(before.pixelColor(4, 4), Theme::instance()->canvas()));

  // the width as the animation steps it, one frame each
  for (int w : { 430, 500, 590, 670, 710, 720 }) {
    view->setWidth(w);
    t.frame();
  }
  const QImage after = t.frame();
  saveForInspection(after, "viewport-after-grow");
  QCOMPARE(view->effectiveColorBufferSize(), full);
  QCOMPARE(v->viewportSize(), QSizeF(full));

  // nothing of the window shows through: the view covers its new size
  int holes = 0;
  for (int y = 0; y < after.height(); y += 3)
    for (int x = 0; x < after.width(); x += 3)
      if (isMagenta(after.pixelColor(x, y)))
        holes++;
  QCOMPARE(holes, 0);

  // and the frame is the one a view made at that size draws
  delete view;
  auto * fresh = new VoxelViewport(t.window->contentItem());
  fresh->setController(v);
  fresh->setSize(QSizeF(full));
  const QImage reference = t.frame();
  saveForInspection(reference, "viewport-fresh");
  int differing = 0;
  for (int y = 0; y < after.height(); y++)
    for (int x = 0; x < after.width(); x++)
      if (!near(after.pixelColor(x, y), reference.pixelColor(x, y), 8))
        differing++;
  QVERIFY2(differing <= after.width() * after.height() / 1000,
           qPrintable(QStringLiteral("%1 pixels differ from a fresh render").arg(differing)));
  // the shape really is there, centred in the wider view
  QVERIFY(!near(after.pixelColor(360, 150), Theme::instance()->canvas(), 20));
}

void TestViewportItem::drawsWithTheBestMultisamplingTheDeviceHas() {
  // smooth voxel edges: 8x where the device can, else 4x, 2x, or none
  QCOMPARE(VoxelViewport::bestSampleCount({ 1, 2, 4, 8, 16 }), 8);
  QCOMPARE(VoxelViewport::bestSampleCount({ 1, 4 }), 4);
  QCOMPARE(VoxelViewport::bestSampleCount({ 1, 2 }), 2);
  QCOMPARE(VoxelViewport::bestSampleCount({ 1 }), 1);
  QCOMPARE(VoxelViewport::bestSampleCount({}), 1);

  AppFixture f;
  QuickTarget t;
  GPU_OR_SKIP(t.create(QSize(200, 150)));
  auto * view = new VoxelViewport(t.window->contentItem());
  view->setController(f.app->viewport());
  view->setSize(QSizeF(200, 150));
  const int best = VoxelViewport::bestSampleCount(t.renderer.rhi()->supportedSampleCounts());
  QTRY_COMPARE(view->msaaSamples(), best);
  QCOMPARE(view->sampleCount(), 1);          // the item's texture: the scene multisamples itself
  QVERIFY(!t.frame().isNull());
  QCOMPARE(view->effectiveColorBufferSize(), QSize(200, 150));
}

// ---------------------------------------------------------------------------

namespace {

  /* everything said through Qt's message handler while alive -- a cache
   * that is missing, broken or someone else's must pass without a word */
  struct MessageLog {
    static QStringList & lines(void) { static QStringList l; return l; }
    QtMessageHandler previous;
    MessageLog() {
      lines().clear();
      previous = qInstallMessageHandler([](QtMsgType t, const QMessageLogContext &, const QString & m) {
        if (t != QtDebugMsg && t != QtInfoMsg)
          lines().append(m);
      });
    }
    ~MessageLog() { qInstallMessageHandler(previous); }
  };

  /* one frame of the 3D view's renderer through an offscreen renderer
   * keeping its pipeline cache in `file` */
  struct CachedRender {
    OffscreenRenderer renderer{ OffscreenRenderer::Backend::Software };
    explicit CachedRender(const QString & file) { renderer.setPipelineCacheFile(file); }
    bool draw(const SceneFrame & f) { return !renderer.render(f, QSize(96, 72)).isNull(); }
  };

  /* A Qt Quick window that Qt gives a QRhi of its own (as the main window
   * gets), drawn through a QQuickRenderControl, with the pipeline cache
   * configured as main() configures the main window's. */
  struct CachedWindow {
    std::unique_ptr<QQuickRenderControl> control = std::make_unique<QQuickRenderControl>();
    std::unique_ptr<QQuickWindow> window;
#if QT_CONFIG(vulkan)
    QVulkanInstance vulkan;
#endif
    std::unique_ptr<QRhiTexture> tex;
    std::unique_ptr<QRhiRenderBuffer> depth;
    std::unique_ptr<QRhiRenderPassDescriptor> rp;
    std::unique_ptr<QRhiTextureRenderTarget> rt;

    bool create(const QString & file) {
      // the graphics API the render tests use (OffscreenRenderer's choice)
      QSGRendererInterface::GraphicsApi api = QSGRendererInterface::Direct3D11;
      {
        OffscreenRenderer probe(OffscreenRenderer::Backend::Software);
        if (!probe.isValid())
          return false;
        switch (probe.rhi()->backend()) {
          case QRhi::D3D11: api = QSGRendererInterface::Direct3D11; break;
          case QRhi::Vulkan: api = QSGRendererInterface::Vulkan; break;
          case QRhi::Metal: api = QSGRendererInterface::Metal; break;
          case QRhi::OpenGLES2: api = QSGRendererInterface::OpenGL; break;
          default: return false;
        }
      }
      QQuickWindow::setGraphicsApi(api);
      window = std::make_unique<QQuickWindow>(control.get());
#if QT_CONFIG(vulkan)
      if (api == QSGRendererInterface::Vulkan) {
        vulkan.setExtensions(QRhiVulkanInitParams::preferredInstanceExtensions());
        if (!vulkan.create())
          return false;
        window->setVulkanInstance(&vulkan);
      }
#endif
      QQuickGraphicsConfiguration config;
      config.setPreferSoftwareDevice(true);         // WARP on Windows, as the render tests
      config.setAutomaticPipelineCache(false);
      window->setGraphicsConfiguration(config);
      PipelineCache::configureWindow(window.get(), file);
      if (!control->initialize())
        return false;
      QRhi * rhi = control->rhi();
      const QSize size(96, 72);
      tex.reset(rhi->newTexture(QRhiTexture::RGBA8, size, 1, QRhiTexture::RenderTarget));
      depth.reset(rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, size, 1));
      if (!tex->create() || !depth->create())
        return false;
      QRhiTextureRenderTargetDescription desc{ QRhiColorAttachment(tex.get()) };
      desc.setDepthStencilBuffer(depth.get());
      rt.reset(rhi->newTextureRenderTarget(desc));
      rp.reset(rt->newCompatibleRenderPassDescriptor());
      rt->setRenderPassDescriptor(rp.get());
      if (!rt->create())
        return false;
      window->setRenderTarget(QQuickRenderTarget::fromRhiRenderTarget(rt.get()));
      window->setGeometry(0, 0, size.width(), size.height());
      window->contentItem()->setSize(size);
      return true;
    }

    void frame(void) {
      control->polishItems();
      control->beginFrame();
      control->sync();
      control->render();
      control->endFrame();
    }

    /* Qt Quick writes the cache when the device goes, which needs the
     * window still there to read its configuration */
    ~CachedWindow() {
      rt.reset();
      rp.reset();
      depth.reset();
      tex.reset();
      control.reset();
      window.reset();
    }
  };
}

void TestPipelineCache::theFilesLiveBesideTheSettings() {
  const QString dir = QStringLiteral("C:/somewhere/BurrTools");
  QCOMPARE(PipelineCache::windowFile(dir + QStringLiteral("/.burrtools-qt.rc")), dir + QStringLiteral("/.burrtools-qt.pipelines"));
  QCOMPARE(PipelineCache::offscreenFile(dir + QStringLiteral("/.burrtools-qt.rc")),
           dir + QStringLiteral("/.burrtools-qt.offscreen.pipelines"));
  // a scratch settings file (BURRTOOLS_QT_SETTINGS) takes its caches along
  QCOMPARE(PipelineCache::windowFile(QStringLiteral("/tmp/run/s.rc")), QStringLiteral("/tmp/run/s.pipelines"));
  QVERIFY(PipelineCache::windowFile(QString()).isEmpty());

  // the App's settings decide: the tests' App is on a scratch file, never the user's
  AppFixture f;
  QCOMPARE(QFileInfo(PipelineCache::windowFile(f.app->settings()->file())).absolutePath(),
           QFileInfo(f.dir.filePath(QStringLiteral("s.rc"))).absolutePath());
}

void TestPipelineCache::aWindowLoadsOnlyACacheThatExists() {
  QTemporaryDir dir;
  const QString file = dir.filePath(QStringLiteral("w.pipelines"));
  QQuickWindow w;
  PipelineCache::configureWindow(&w, file);
  QCOMPARE(w.graphicsConfiguration().pipelineCacheSaveFile(), file);
  QVERIFY(w.graphicsConfiguration().pipelineCacheLoadFile().isEmpty());     // nothing to load yet
  QVERIFY(PipelineCache::write(file, QByteArray("x")));
  PipelineCache::configureWindow(&w, file);
  QCOMPARE(w.graphicsConfiguration().pipelineCacheLoadFile(), file);
}

void TestPipelineCache::theOffscreenRendererKeepsItsCache() {
  QTemporaryDir dir;
  const QString file = dir.filePath(QStringLiteral("o.pipelines"));
  AppFixture f;
  QVERIFY(f.load("examples/PelikanBurr.xmpuzzle"));
  const SceneFrame frame = f.app->viewport()->frame(1);
  {
    CachedRender first(file);
    GPU_OR_SKIP(first.renderer.isValid());
    if (!first.renderer.rhi()->isFeatureSupported(QRhi::PipelineCacheDataLoadSave))
      QSKIP("this graphics backend keeps no pipeline cache");
    QVERIFY(!first.renderer.pipelineCacheSeeded());             // no file yet
    QVERIFY(first.draw(frame));
  }
  QVERIFY2(QFileInfo(file).size() > 0, "no cache written");

  // the second start begins with the first one's pipelines
  {
    CachedRender second(file);
    QVERIFY(second.renderer.pipelineCacheSeeded());
    QVERIFY(second.draw(frame));
  }

  // a broken, a cut-short or someone else's file: ignored without a word,
  // the picture drawn, and a good cache written in its place
  const QByteArray good = PipelineCache::read(file);
  QFile self(QCoreApplication::applicationFilePath());
  QVERIFY(self.open(QIODevice::ReadOnly));
  const QByteArray bad[] = { QByteArray("not a pipeline cache"), good.left(good.size() / 2), self.read(200000) };
  for (const QByteArray & b : bad) {
    QVERIFY(PipelineCache::write(file, b));
    MessageLog log;
    {
      CachedRender r(file);
      QVERIFY(!r.renderer.pipelineCacheSeeded());
      QVERIFY(r.draw(frame));
    }
    QVERIFY2(MessageLog::lines().isEmpty(), qPrintable(MessageLog::lines().join(QLatin1Char('\n'))));
    CachedRender again(file);
    QVERIFY(again.renderer.pipelineCacheSeeded());
  }

  // no file at all: the same as an empty cache
  QFile::remove(file);
  MessageLog log;
  CachedRender none(file);
  QVERIFY(!none.renderer.pipelineCacheSeeded());
  QVERIFY(none.draw(frame));
  QVERIFY(MessageLog::lines().isEmpty());
}

void TestPipelineCache::theWindowWritesItsCacheAndTheNextStartLoadsIt() {
  QTemporaryDir dir;
  const QString file = dir.filePath(QStringLiteral("w.pipelines"));
  AppFixture f;
  QVERIFY(f.load("examples/PelikanBurr.xmpuzzle"));
  {
    CachedWindow w;
    GPU_OR_SKIP(w.create(file));
    if (!w.control->rhi()->isFeatureSupported(QRhi::PipelineCacheDataLoadSave))
      QSKIP("this graphics backend keeps no pipeline cache");
    QVERIFY(w.control->rhi()->pipelineCacheData().isEmpty());   // a first start: nothing cached
    auto * view = new VoxelViewport(w.window->contentItem());
    view->setController(f.app->viewport());
    view->setSize(QSizeF(96, 72));
    w.frame();
    w.frame();
  }
  QVERIFY2(QFileInfo(file).size() > 0, "the window wrote no cache");

  // the next start: the device holds the pipelines before drawing anything
  MessageLog log;
  {
    CachedWindow w;
    QVERIFY(w.create(file));
    QVERIFY(!w.control->rhi()->pipelineCacheData().isEmpty());
  }
  // and a broken file is ignored as quietly
  QVERIFY(PipelineCache::write(file, QByteArray("garbage")));
  {
    CachedWindow w;
    QVERIFY(w.create(file));
    QVERIFY(w.control->rhi()->pipelineCacheData().isEmpty());
  }
  QVERIFY2(MessageLog::lines().isEmpty(), qPrintable(MessageLog::lines().join(QLatin1Char('\n'))));
}
