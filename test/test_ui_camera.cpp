/* Tests for btui::Camera: framing, navigation, the eased wheel zoom and the
 * 360 ms snaps of spec C06 / C13. Time only moves through tick(), so every
 * animation is checked frame by frame.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "../src/uicore/camera.h"

#include <set>

using namespace btui;
using Catch::Matchers::WithinAbs;

namespace {

  Camera framed(void) {
    Camera c;
    c.setViewport(800, 600);
    c.setScene({ 2, 1.5f, 2 }, 3);
    return c;
  }

  float angleDeg(Quat a, Quat b) { return degrees(angleBetween(a, b)); }

  void runFor(Camera & c, float ms) {
    for (float t = 0; t < ms; t += 16)
      c.tick(16);
  }
}

TEST_CASE("the scene sphere fits the view at zoom 1", "[ui][camera]") {
  Camera c = framed();
  // every point of the bounding sphere's silhouette lands inside the view
  for (Vec3 dir : { Vec3{ 1, 0, 0 }, Vec3{ 0, 1, 0 }, Vec3{ 0, 0, 1 }, Vec3{ -1, -1, 0 } }) {
    Vec3 p = c.project(c.sceneCentre() + normalize(dir) * c.sceneRadius());
    CHECK(p.x >= 0);
    CHECK(p.x <= 800);
    CHECK(p.y >= 0);
    CHECK(p.y <= 600);
  }
  // and the target is in the middle
  Vec3 m = c.project(c.sceneCentre());
  CHECK_THAT(m.x, WithinAbs(400, 0.01));
  CHECK_THAT(m.y, WithinAbs(300, 0.01));
}

TEST_CASE("the clip planes hold the whole scene at any zoom", "[ui][camera]") {
  Camera c = framed();
  for (float z : { 0.3f, 1.0f, 4.0f }) {
    c.setZoom(z);
    float n, f;
    c.clipPlanes(&n, &f);
    INFO("zoom " << z);
    CHECK(n > 0);
    CHECK(n < c.distance() - c.sceneRadius());
    CHECK(f > c.distance() + c.sceneRadius());
  }
}

TEST_CASE("a picking ray passes through the point it was cast from", "[ui][camera]") {
  for (auto proj : { Camera::Projection::Perspective, Camera::Projection::Orthographic }) {
    Camera c = framed();
    c.setProjection(proj);
    c.panBy(30, -20);
    for (auto [x, y] : { std::pair{ 400.0f, 300.0f }, std::pair{ 120.0f, 80.0f }, std::pair{ 700.0f, 550.0f } }) {
      auto r = c.rayAt(x, y);
      // a point along the ray near the scene projects back to (x, y)
      Vec3 p = r.origin + r.dir * (c.distance() * 0.9f);
      Vec3 s = c.project(p);
      INFO("projection " << int(proj) << " at " << x << "," << y);
      CHECK_THAT(s.x, WithinAbs(x, 0.05));
      CHECK_THAT(s.y, WithinAbs(y, 0.05));
    }
  }
}

TEST_CASE("panning moves the content with the pointer", "[ui][camera]") {
  Camera c = framed();
  Vec3 before = c.project(c.sceneCentre());
  c.panBy(50, 20);
  Vec3 after = c.project(c.sceneCentre());
  CHECK_THAT(after.x - before.x, WithinAbs(50, 0.05));
  CHECK_THAT(after.y - before.y, WithinAbs(20, 0.05));
}

TEST_CASE("both rotation methods turn the view and a still drag does nothing", "[ui][camera]") {
  for (auto m : { Camera::RotationMethod::Drag, Camera::RotationMethod::Arcball }) {
    Camera c = framed();
    c.setRotationMethod(m);
    Quat start = c.orientation();
    c.beginRotate(400, 300);
    c.rotateTo(400, 300);
    CHECK(angleDeg(c.orientation(), start) < 1e-3f);
    c.rotateTo(500, 300);
    c.endRotate();
    INFO("method " << int(m));
    CHECK(angleDeg(c.orientation(), start) > 5);

    // a horizontal drag turns about the screen's vertical axis: the view's
    // up direction (in view space) is unchanged
    Vec3 upBefore = rotate(start, viewAxisInWorld(start, 1));
    Vec3 upAfter = rotate(c.orientation(), viewAxisInWorld(start, 1));
    CHECK_THAT(dot(upBefore, upAfter), WithinAbs(1.0, 1e-3));
  }
}

TEST_CASE("a manual orbit keeps a roll the view cube made", "[ui][camera]") {
  Camera c = framed();
  // rolled 90 degrees: a horizontal drag must still move the model
  // horizontally on screen (C13 "no jump", rolled screen frame)
  c.setOrientation(Quat::axisAngle({ 0, 0, 1 }, radians(90)) * Camera::homeOrientation());
  Vec3 worldUpOnScreenBefore = rotate(c.orientation(), { 0, 1, 0 });
  c.beginRotate(400, 300);
  c.rotateTo(420, 300);
  c.endRotate();
  Vec3 worldUpOnScreenAfter = rotate(c.orientation(), { 0, 1, 0 });
  // the drag turned about the screen's vertical: the world up vector's
  // screen-vertical component is unchanged
  CHECK_THAT(worldUpOnScreenAfter.y, WithinAbs(worldUpOnScreenBefore.y, 1e-3));
}

TEST_CASE("one wheel notch zooms smoothly over several frames", "[ui][camera]") {
  // T-C06-13: delta -100 sampled every 40 ms gives at least 5 distinct
  // increasing values converging on about x1.17 within about 400 ms
  Camera c = framed();
  c.wheel(-100);
  CHECK_THAT(c.zoomTarget(), WithinAbs(std::exp(0.16f), 1e-4));
  std::set<float> seen;
  float last = c.zoom();
  for (int i = 0; i < 10; i++) {
    c.tick(40);
    CHECK(c.zoom() >= last);
    last = c.zoom();
    seen.insert(c.zoom());
  }
  CHECK(seen.size() >= 5);
  // converged to within 0.3 % of the target ("about 400 ms")
  CHECK_THAT(c.zoom(), Catch::Matchers::WithinRel(std::exp(0.16f), 0.003f));
}

TEST_CASE("wheel notches accumulate and the zoom is clamped", "[ui][camera]") {
  Camera c = framed();
  c.wheel(-100);
  c.wheel(-100);
  CHECK_THAT(c.zoomTarget(), WithinAbs(std::exp(0.32f), 1e-4));
  for (int i = 0; i < 200; i++)
    c.wheel(-500);
  CHECK(c.zoomTarget() == Camera::kMaxZoom);
  for (int i = 0; i < 200; i++)
    c.wheel(500);
  CHECK(c.zoomTarget() == Camera::kMinZoom);
}

TEST_CASE("the zoom easing does not depend on the frame rate", "[ui][camera]") {
  Camera a = framed(), b = framed();
  a.wheel(-200);
  b.wheel(-200);
  for (int i = 0; i < 12; i++) a.tick(25);    // 300 ms in 25 ms frames
  for (int i = 0; i < 30; i++) b.tick(10);    // 300 ms in 10 ms frames
  CHECK_THAT(a.zoom(), WithinAbs(b.zoom(), 1e-3));
}

TEST_CASE("Home animates for 360 ms and ends at the default view, framed", "[ui][camera]") {
  // T-C13-3
  Camera c = framed();
  c.setOrientation(Camera::fromYawPitchRoll(100, -40, 30));
  c.setZoom(2.5f);
  c.panBy(40, 40);

  c.home();
  REQUIRE(c.animating());
  c.tick(120);
  // intermediate frames differ from both ends
  CHECK(angleDeg(c.orientation(), Camera::homeOrientation()) > 1);
  CHECK(c.zoom() < 2.5f);
  CHECK(c.zoom() > 1.0f);
  c.tick(240);
  CHECK_FALSE(c.animating());
  CHECK(angleDeg(c.orientation(), Camera::homeOrientation()) < 1e-2f);
  CHECK_THAT(c.zoom(), WithinAbs(1.0, 1e-5));
  CHECK_THAT(c.panX(), WithinAbs(0.0, 1e-5));
  CHECK_THAT(c.panY(), WithinAbs(0.0, 1e-5));
}

TEST_CASE("Fit keeps the orientation", "[ui][camera]") {
  Camera c = framed();
  Quat q = Camera::fromYawPitchRoll(70, 10, 0);
  c.setOrientation(q);
  c.setZoom(3);
  c.fit();
  runFor(c, 400);
  CHECK(angleDeg(c.orientation(), q) < 1e-2f);
  CHECK_THAT(c.zoom(), WithinAbs(1.0, 1e-5));
}

TEST_CASE("manual input stops a snap where it is, without a jump", "[ui][camera]") {
  // T-C13-7
  Camera c = framed();
  c.animateTo(Camera::fromYawPitchRoll(90, 0, 0), false);
  c.tick(150);
  Quat mid = c.orientation();

  c.beginRotate(400, 300);   // an orbit starts mid-animation
  CHECK_FALSE(c.animating());
  CHECK(angleDeg(c.orientation(), mid) < 1e-4f);
  c.rotateTo(400, 300);
  CHECK(angleDeg(c.orientation(), mid) < 1e-3f);
  c.endRotate();

  // a wheel during a snap also stops it and keeps the shown zoom
  c.animateTo(Camera::homeOrientation(), true);
  c.tick(100);
  float z = c.zoom();
  c.wheel(-10);
  CHECK_FALSE(c.animating());
  CHECK(c.zoomTarget() > z);
}

TEST_CASE("the animation eases in and out", "[ui][camera]") {
  Camera c = framed();
  Quat from = c.orientation();
  Quat to = Camera::fromYawPitchRoll(Camera::kHomeYawDeg + 90, Camera::kHomePitchDeg, 0);
  const float total = angleDeg(from, to);
  c.animateTo(to, false);
  c.tick(36);                                   // 10 %
  float early = angleDeg(from, c.orientation()) / total;
  c.tick(144);                                  // 50 %
  float half = angleDeg(from, c.orientation()) / total;
  CHECK(early < 0.05f);                         // cubic: 4 * 0.1^3 = 0.004
  CHECK_THAT(half, WithinAbs(0.5, 0.02));
}

TEST_CASE("lookFrom faces the given direction; top and bottom stay square", "[ui][camera]") {
  for (Vec3 d : { Vec3{ 1, 0, 0 }, Vec3{ 0, 0, -1 }, Vec3{ 1, 1, 0 }, Vec3{ -1, 1, 1 } }) {
    Quat q = Camera::lookFrom(d, 0);
    Vec3 n = viewAxisInWorld(q, 2);
    INFO(d.x << "," << d.y << "," << d.z);
    CHECK_THAT(dot(n, normalize(d)), WithinAbs(1.0, 1e-5));
    // no roll: the view's right axis is horizontal
    CHECK_THAT(viewAxisInWorld(q, 0).y, WithinAbs(0.0, 1e-5));
  }
  // straight down: the yaw hint is rounded to 90 degrees and the screen axes
  // are world axes
  Quat top = Camera::lookFrom({ 0, 1, 0 }, 37);
  Vec3 r = viewAxisInWorld(top, 0), u = viewAxisInWorld(top, 1);
  CHECK(std::max({ std::fabs(r.x), std::fabs(r.y), std::fabs(r.z) }) > 0.9999f);
  CHECK(std::max({ std::fabs(u.x), std::fabs(u.y), std::fabs(u.z) }) > 0.9999f);
}

TEST_CASE("yawDeg reads back the yaw, also when looking straight down", "[ui][camera]") {
  for (float yaw : { -120.0f, -32.0f, 0.0f, 45.0f, 170.0f }) {
    for (float pitch : { 0.0f, 30.0f, 90.0f, -90.0f }) {
      Camera c;
      c.setOrientation(Camera::fromYawPitchRoll(yaw, pitch, 0));
      INFO("yaw " << yaw << " pitch " << pitch);
      CHECK_THAT(c.yawDeg(), WithinAbs(yaw, 0.05));
    }
  }
}
