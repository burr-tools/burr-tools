/* Tests for btui::ViewCube: the regions, snapping, the 90-degree and roll
 * steps and the upright double-click of spec C13 (test plan T-C13-*).
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "../src/uicore/camera.h"
#include "../src/uicore/viewcube.h"

using namespace btui;
using Catch::Matchers::WithinAbs;

namespace {

  /* where a cube-space point lands, for aiming the hit tests */
  Vec2 at(Quat q, bool persp, Vec3 p) {
    Vec3 s = ViewCube::project(q, persp, p);
    return { s.x, s.y };
  }

  ViewCube::Hit hitAt(Quat q, bool persp, Vec3 p) {
    Vec2 s = at(q, persp, p);
    return ViewCube::hitTest(q, persp, s.x, s.y);
  }

  Quat faceOn(Vec3 n) { return Camera::lookFrom(n, 0); }

  float angleDeg(Quat a, Quat b) { return degrees(angleBetween(a, b)); }
}

TEST_CASE("the axis indicator's labels stay inside the item at every orientation", "[ui][viewcube]") {
  // The labels sit 33 dp from the indicator's origin, a 12 dp box round
  // each; ViewCubeItem paints the design shifted right by kPadLeft in an
  // item kItemWidth x kItemHeight. Whichever way an axis points, its label
  // must not be cut off (it was, at the left and the bottom).
  const float reach = 33 + 6;
  for (float yaw = 0; yaw < 360; yaw += 15)
    for (float pitch = -90; pitch <= 90; pitch += 15)
      for (float roll : { 0.0f, 90.0f, 180.0f, 270.0f }) {
        const Quat q = Camera::fromYawPitchRoll(yaw, pitch, roll);
        for (int i = 0; i < 3; i++) {
          const Vec3 v = rotate(q, Vec3{ i == 0 ? 1.0f : 0.0f, i == 1 ? 1.0f : 0.0f, i == 2 ? 1.0f : 0.0f });
          // as painted: screen x is v.x, screen y is -v.y, from the origin
          const float x = ViewCube::kAxisX + v.x * 33, y = ViewCube::kAxisY - v.y * 33;
          INFO("yaw " << yaw << " pitch " << pitch << " roll " << roll << " axis " << i);
          CHECK(x - 6 >= -ViewCube::kPadLeft);
          CHECK(x + 6 <= ViewCube::kWidth);
          CHECK(y - 6 >= 0);
          CHECK(y + 6 <= ViewCube::kItemHeight);
        }
      }
  CHECK(reach <= ViewCube::kAxisX + ViewCube::kPadLeft);
  CHECK(ViewCube::kAxisY + reach <= ViewCube::kItemHeight);
}

TEST_CASE("face centre, edge strip and corner regions follow the 0.56 threshold", "[ui][viewcube]") {
  // T-C13-1, on the +Z face seen face-on, in both projections
  for (bool persp : { false, true }) {
    Quat q = faceOn({ 0, 0, 1 });
    INFO("perspective " << persp);
    CHECK(hitAt(q, persp, { 0, 0, 1 }).dir == CubeDir{ 0, 0, 1 });
    CHECK(hitAt(q, persp, { 0.5f, -0.5f, 1 }).dir == CubeDir{ 0, 0, 1 });
    CHECK(hitAt(q, persp, { 0.8f, 0, 1 }).dir == CubeDir{ 1, 0, 1 });
    CHECK(hitAt(q, persp, { 0, -0.8f, 1 }).dir == CubeDir{ 0, -1, 1 });
    CHECK(hitAt(q, persp, { -0.8f, 0.8f, 1 }).dir == CubeDir{ -1, 1, 1 });
    CHECK(hitAt(q, persp, { 0, 0, 1 }).kind == ViewCube::Kind::Region);
  }
}

TEST_CASE("an oblique view hits the faces it shows", "[ui][viewcube]") {
  for (bool persp : { false, true }) {
    Quat q = Camera::homeOrientation();   // shows +X, +Y and +Z
    INFO("perspective " << persp);
    CHECK(hitAt(q, persp, { 1, 0, 0 }).dir == CubeDir{ 1, 0, 0 });
    CHECK(hitAt(q, persp, { 0, 1, 0 }).dir == CubeDir{ 0, 1, 0 });
    CHECK(hitAt(q, persp, { 0, 0, 1 }).dir == CubeDir{ 0, 0, 1 });
    CHECK(hitAt(q, persp, { 0.95f, 0.95f, 0.95f }).dir == CubeDir{ 1, 1, 1 });
    // away from the cube there is nothing
    CHECK(ViewCube::hitTest(q, persp, 150, 165).kind == ViewCube::Kind::None);
  }
}

TEST_CASE("every region snaps to its direction; top and bottom end square", "[ui][viewcube]") {
  // T-C13-2: 6 faces, 12 edges, 8 corners
  int n = 0;
  for (int x = -1; x <= 1; x++)
    for (int y = -1; y <= 1; y++)
      for (int z = -1; z <= 1; z++) {
        CubeDir d{ x, y, z };
        if (d.count() == 0)
          continue;
        n++;
        ViewCube::Hit h;
        h.kind = ViewCube::Kind::Region;
        h.dir = d;
        auto t = ViewCube::target(h, Camera::homeOrientation());
        REQUIRE(t);
        INFO(x << "," << y << "," << z);
        CHECK_THAT(dot(viewAxisInWorld(*t, 2), normalize(d.vec())), WithinAbs(1.0, 1e-5));
        if (d.count() == 1 && y != 0)
          CHECK(ViewCube::faceAligned(*t));
      }
  CHECK(n == 26);
}

TEST_CASE("arrows and roll controls exist only when face-aligned", "[ui][viewcube]") {
  // T-C13-4
  Quat aligned = faceOn({ 0, 0, 1 });
  REQUIRE(ViewCube::faceAligned(aligned));
  auto right = ViewCube::hitTest(aligned, true, ViewCube::kCentreX + ViewCube::kArrowOffset, ViewCube::kCentreY);
  CHECK(right.kind == ViewCube::Kind::Arrow);
  CHECK(right.arrow == ViewCube::Arrow::Right);
  auto ccw = ViewCube::hitTest(aligned, true, ViewCube::kRollCcwX, ViewCube::kRollCcwY);
  CHECK(ccw.kind == ViewCube::Kind::Roll);
  CHECK(ccw.roll == +1);
  auto cw = ViewCube::hitTest(aligned, true, ViewCube::kRollCwX, ViewCube::kRollCwY);
  CHECK(cw.roll == -1);

  Quat oblique = Camera::homeOrientation();
  CHECK_FALSE(ViewCube::faceAligned(oblique));
  CHECK(ViewCube::hitTest(oblique, true, ViewCube::kCentreX + ViewCube::kArrowOffset, ViewCube::kCentreY).kind
        != ViewCube::Kind::Arrow);
  CHECK(ViewCube::hitTest(oblique, true, ViewCube::kHomeX, ViewCube::kHomeY).kind == ViewCube::Kind::Home);
}

TEST_CASE("the right arrow brings the right-hand face to the front", "[ui][viewcube]") {
  Quat q = faceOn({ 0, 0, 1 });
  Vec3 rightFace = viewAxisInWorld(q, 0);
  ViewCube::Hit h;
  h.kind = ViewCube::Kind::Arrow;
  h.arrow = ViewCube::Arrow::Right;
  Quat t = *ViewCube::target(h, q);
  CHECK_THAT(dot(viewAxisInWorld(t, 2), rightFace), WithinAbs(1.0, 1e-5));

  // and the left arrow undoes it
  h.arrow = ViewCube::Arrow::Left;
  Quat back = *ViewCube::target(h, t);
  CHECK(angleDeg(back, q) < 1e-3f);
}

TEST_CASE("four steps in one direction return to the start", "[ui][viewcube]") {
  // T-C13-4 / AC-C13-06
  Quat start = faceOn({ 1, 0, 0 });
  for (auto mk : { std::pair{ ViewCube::Kind::Arrow, 0 }, std::pair{ ViewCube::Kind::Arrow, 1 },
                   std::pair{ ViewCube::Kind::Roll, 1 }, std::pair{ ViewCube::Kind::Roll, -1 } }) {
    ViewCube::Hit h;
    h.kind = mk.first;
    if (mk.first == ViewCube::Kind::Arrow)
      h.arrow = mk.second ? ViewCube::Arrow::Up : ViewCube::Arrow::Left;
    else
      h.roll = mk.second;
    Quat q = start;
    for (int i = 0; i < 4; i++) {
      q = *ViewCube::target(h, q);
      CHECK(ViewCube::faceAligned(q));
    }
    CHECK(angleDeg(q, start) < 1e-3f);
  }
}

TEST_CASE("roll turns about the view axis in the stated direction", "[ui][viewcube]") {
  // T-C13-10: counter-clockwise moves what was on top to the left
  Quat q = faceOn({ 0, 0, 1 });
  ViewCube::Hit h;
  h.kind = ViewCube::Kind::Roll;
  h.roll = +1;
  Quat t = *ViewCube::target(h, q);
  CHECK_THAT(dot(viewAxisInWorld(t, 2), viewAxisInWorld(q, 2)), WithinAbs(1.0, 1e-5));
  Vec3 topOnScreen = rotate(t, viewAxisInWorld(q, 1));
  CHECK_THAT(topOnScreen.x, WithinAbs(-1.0, 1e-5));
}

TEST_CASE("double-clicking a face makes its label upright from any roll", "[ui][viewcube]") {
  // T-C13-11 / AC-C13-13: the label's baseline ends horizontal, left to right
  for (int f = 0; f < 6; f++) {
    const auto & face = ViewCube::face(f);
    CubeDir d{ int(face.n.x), int(face.n.y), int(face.n.z) };
    for (float roll : { 0.0f, 90.0f, 180.0f, -90.0f, 37.0f }) {
      Quat start = Quat::axisAngle({ 0, 0, 1 }, radians(roll)) * Camera::homeOrientation();
      Quat t = ViewCube::uprightTarget(d, start);
      Vec3 baseline = rotate(t, face.b);
      INFO(face.label << " from roll " << roll);
      CHECK_THAT(baseline.x, WithinAbs(1.0, 1e-4));
      CHECK_THAT(dot(viewAxisInWorld(t, 2), face.n), WithinAbs(1.0, 1e-5));
    }
  }
}

TEST_CASE("region patches cover each touched face", "[ui][viewcube]") {
  // T-C13-8: a face centre one patch, an edge two, a corner three
  CHECK(ViewCube::regionPatches({ 0, 0, 1 }).size() == 1);
  CHECK(ViewCube::regionPatches({ 1, 0, 1 }).size() == 2);
  CHECK(ViewCube::regionPatches({ 1, 1, 1 }).size() == 3);

  auto p = ViewCube::regionPatches({ 0, 0, 1 }).front();
  CHECK(p.face == 4);
  CHECK_THAT(p.u0, WithinAbs(-ViewCube::kInner, 1e-6));
  CHECK_THAT(p.u1, WithinAbs(ViewCube::kInner, 1e-6));

  // on the +Z face the +X edge strip is at the face's right (+b = +X)
  for (auto q : ViewCube::regionPatches({ 1, 0, 1 }))
    if (q.face == 4) {
      CHECK_THAT(q.u0, WithinAbs(ViewCube::kInner, 1e-6));
      CHECK_THAT(q.u1, WithinAbs(1.0, 1e-6));
    }
}

TEST_CASE("perspective makes the near edges longer than the far ones", "[ui][viewcube]") {
  // T-C13-6
  Quat q = Camera::homeOrientation();
  auto len = [&](bool persp, Vec3 a, Vec3 b) {
    Vec3 pa = ViewCube::project(q, persp, a), pb = ViewCube::project(q, persp, b);
    return std::hypot(pa.x - pb.x, pa.y - pb.y);
  };
  // two parallel X edges: the one nearest the viewer and the farthest
  const float nearP = len(true, { -1, 1, 1 }, { 1, 1, 1 }), farP = len(true, { -1, -1, -1 }, { 1, -1, -1 });
  const float nearO = len(false, { -1, 1, 1 }, { 1, 1, 1 }), farO = len(false, { -1, -1, -1 }, { 1, -1, -1 });
  CHECK(nearP > farP * 1.1f);
  CHECK_THAT(nearO, WithinAbs(farO, 1e-3));
}

TEST_CASE("only the faces turned to the eye are visible", "[ui][viewcube]") {
  for (bool persp : { false, true }) {
    Quat q = Camera::homeOrientation();
    INFO("perspective " << persp);
    CHECK(ViewCube::faceVisible(q, persp, 0));    // +X
    CHECK(ViewCube::faceVisible(q, persp, 2));    // +Y
    CHECK(ViewCube::faceVisible(q, persp, 4));    // +Z
    CHECK_FALSE(ViewCube::faceVisible(q, persp, 1));
    CHECK_FALSE(ViewCube::faceVisible(q, persp, 3));
    CHECK_FALSE(ViewCube::faceVisible(q, persp, 5));
  }
}

TEST_CASE("the arrows and roll controls stay clear of the cube", "[ui][viewcube]") {
  // T-C13-12: >= 6 dp from the face-on silhouette in both projections
  for (bool persp : { false, true }) {
    Quat q = faceOn({ 0, 0, 1 });
    float maxX = 0;
    for (int i = 0; i < 8; i++) {
      Vec3 c{ (i & 1) ? 1.0f : -1.0f, (i & 2) ? 1.0f : -1.0f, (i & 4) ? 1.0f : -1.0f };
      maxX = std::max(maxX, ViewCube::project(q, persp, c).x - ViewCube::kCentreX);
    }
    // the right arrow's near tip is 6 dp inside its centre
    CHECK(ViewCube::kArrowOffset - 6 - maxX >= 6);
  }
}

TEST_CASE("after dragging the cube, a nearby view snaps", "[ui][viewcube]") {
  Quat nearFace = Quat::axisAngle({ 0, 1, 0 }, radians(5)) * faceOn({ 0, 0, 1 });
  auto s = ViewCube::snapNearest(nearFace);
  REQUIRE(s);
  CHECK_THAT(dot(viewAxisInWorld(*s, 2), Vec3{ 0, 0, 1 }), WithinAbs(1.0, 1e-5));

  Quat between = Camera::fromYawPitchRoll(20, 10, 0);
  CHECK_FALSE(ViewCube::snapNearest(between));
}
