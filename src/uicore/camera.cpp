/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include "camera.h"

#include <algorithm>

namespace btui {

  namespace {

    float easeInOutCubic(float t) {
      return t < 0.5f ? 4 * t * t * t : 1 - std::pow(-2 * t + 2, 3.0f) / 2;
    }

    float tanHalfFov(void) {
      return std::tan(radians(Camera::kFovYDeg) / 2);
    }
  }

  Camera::Camera(void) : orient(homeOrientation()) {}

  void Camera::setViewport(float widthDp, float heightDp) {
    vw = std::max(1.0f, widthDp);
    vh = std::max(1.0f, heightDp);
  }

  void Camera::setScene(Vec3 c, float r) {
    centre = c;
    radius = std::max(0.5f, r);
  }

  void Camera::setZoom(float v) {
    z = zTarget = std::clamp(v, kMinZoom, kMaxZoom);
  }

  float Camera::fitDistance(void) const {
    const float t = tanHalfFov();
    const float aspect = vw / vh;
    // the sphere must fit the narrower of the two half angles; 10 % margin
    const float halfV = std::atan(t);
    const float halfH = std::atan(t * aspect);
    const float half = std::min(halfV, halfH);
    return 1.1f * radius / std::sin(half);
  }

  float Camera::worldPerDp(void) const {
    return 2 * distance() * tanHalfFov() / vh;
  }

  Mat4 Camera::viewMatrix(void) const {
    return Mat4::translation({ px, py, -distance() }) * Mat4::rotation(orient) * Mat4::translation(-centre);
  }

  void Camera::clipPlanes(float * zNear, float * zFar) const {
    const float d = distance();
    const float r = radius * 1.2f + std::sqrt(px * px + py * py);
    *zNear = std::max(d - r, d * 0.01f);
    *zFar = d + r + 1.0f;
  }

  Mat4 Camera::projectionMatrix(void) const {
    float n, f;
    clipPlanes(&n, &f);
    const float aspect = vw / vh;
    if (proj == Projection::Orthographic) {
      // the same framing at the target's depth as the perspective view
      const float halfH = distance() * tanHalfFov();
      return Mat4::orthographic(halfH * aspect, halfH, n, f);
    }
    return Mat4::perspective(radians(kFovYDeg), aspect, n, f);
  }

  // --- rotation -----------------------------------------------------------

  /* legacy arcBall_c::mapToSphere: each axis scaled to [-1, 1] separately */
  Vec3 Camera::arcballVector(float x, float y) const {
    const float aw = vw > 1 ? 1.0f / ((vw - 1) * 0.5f) : 1.0f;
    const float ah = vh > 1 ? 1.0f / ((vh - 1) * 0.5f) : 1.0f;
    const float tx = x * aw - 1.0f;
    const float ty = 1.0f - y * ah;
    const float len = tx * tx + ty * ty;
    if (len > 1.0f) {
      float n = 1.0f / std::sqrt(len);
      return { tx * n, ty * n, 0 };
    }
    return { tx, ty, std::sqrt(1.0f - len) };
  }

  /* legacy method2_c::spherePoint: scaled by the smaller side, with a
   * hyperbolic sheet outside the sphere */
  Vec3 Camera::dragVector(float x, float y) const {
    const float smin = std::min(vw, vh);
    Vec3 v{ 2.0f * (x - 0.5f * vw) / smin, 2.0f * (0.5f * vh - y) / smin, 0 };
    float d = v.x * v.x + v.y * v.y;
    if (d < 0.75f) {
      v.z = std::sqrt(1.0f - d);
    } else if (d < 3.0f) {
      d = std::sqrt(3.0f) - std::sqrt(d);
      float t = std::max(0.0f, 1.0f - d * d);
      v.z = 1.0f - std::sqrt(t);
    }
    return normalize(v);
  }

  void Camera::beginRotate(float x, float y) {
    cancelAnimation();
    dragging = true;
    dragStartOrient = orient;
    dragStartVec = arcballVector(x, y);
    lastX = x;
    lastY = y;
  }

  void Camera::rotateTo(float x, float y) {
    if (!dragging)
      return;

    if (method == RotationMethod::Arcball) {
      // legacy: the rotation from the press point to here, applied to the
      // orientation at the press, as the quaternion (perp, dot)
      Vec3 en = arcballVector(x, y);
      Vec3 perp = cross(dragStartVec, en);
      if (length(perp) > 1e-5f) {
        Quat q = normalize(Quat{ dot(dragStartVec, en), perp.x, perp.y, perp.z });
        orient = q * dragStartOrient;
      } else {
        orient = dragStartOrient;
      }
    } else {
      // legacy method2_c: incremental, the shortest arc between the last and
      // the current sphere point, applied in view space
      Vec3 v1 = dragVector(lastX, lastY);
      Vec3 v2 = dragVector(x, y);
      float d = dot(v1, v2);
      Quat q;
      if (d > 0.999999f) {
        q = Quat();
      } else if (d < -0.999999f) {
        q = Quat{ 0, 0, 0, -1 };
      } else {
        float div = std::sqrt((d + 1.0f) * 2.0f);
        Vec3 c = cross(v1, v2);
        q = Quat{ div * 0.5f, c.x / div, c.y / div, c.z / div };
      }
      orient = q * orient;
      lastX = x;
      lastY = y;
    }
  }

  void Camera::endRotate(void) {
    dragging = false;
  }

  void Camera::panBy(float dxDp, float dyDp) {
    cancelAnimation();
    const float s = worldPerDp();
    px += dxDp * s;
    py -= dyDp * s;
  }

  void Camera::wheel(float delta) {
    if (anim) {
      // a manual zoom stops a snap where it is, but keeps the eased zoom
      // continuing from the zoom actually shown
      anim = false;
      zTarget = z;
    }
    zTarget = std::clamp(zTarget * std::exp(-delta * 0.0016f), kMinZoom, kMaxZoom);
  }

  bool Camera::tick(float dtMs) {
    bool more = false;

    if (anim) {
      animT = std::min(1.0f, animT + dtMs / kAnimMs);
      const float e = easeInOutCubic(animT);
      orient = slerp(animFrom, animTo, e);
      if (animFrame) {
        z = zTarget = animZoomFrom + (1.0f - animZoomFrom) * e;
        px = animPanXFrom * (1 - e);
        py = animPanYFrom * (1 - e);
      }
      if (animT >= 1.0f) {
        orient = animTo;
        anim = false;
      } else {
        more = true;
      }
    }

    if (z != zTarget) {
      // frame-rate independent easing; snap within 0.15 %
      z += (zTarget - z) * (1.0f - std::exp(-dtMs / kZoomTauMs));
      if (std::fabs(zTarget - z) <= 0.0015f * zTarget)
        z = zTarget;
      else
        more = true;
    }

    return more;
  }

  void Camera::animateTo(Quat target, bool frame) {
    target = normalize(target);
    const bool sameOrient = angleBetween(orient, target) < 1e-4f;
    const bool framed = std::fabs(z - 1.0f) < 1e-4f && std::fabs(px) < 1e-6f && std::fabs(py) < 1e-6f;
    if (sameOrient && (!frame || framed)) {
      anim = false;
      orient = target;
      return;
    }
    dragging = false;
    anim = true;
    animFrame = frame;
    animT = 0;
    animFrom = orient;
    animTo = target;
    animZoomFrom = z;
    animPanXFrom = px;
    animPanYFrom = py;
  }

  void Camera::home(void) {
    animateTo(homeOrientation(), true);
  }

  void Camera::fit(void) {
    animateTo(orient, true);
  }

  void Camera::cancelAnimation(void) {
    if (!anim)
      return;
    anim = false;
    zTarget = z;
  }

  // --- picking ------------------------------------------------------------

  Camera::Ray Camera::rayAt(float x, float y) const {
    const float nx = 2.0f * x / vw - 1.0f;
    const float ny = 1.0f - 2.0f * y / vh;
    const float t = tanHalfFov();
    const float aspect = vw / vh;
    const float d = distance();

    Vec3 o, dir;
    if (proj == Projection::Orthographic) {
      const float halfH = d * t;
      o = { nx * halfH * aspect, ny * halfH, 0 };
      dir = { 0, 0, -1 };
    } else {
      o = { 0, 0, 0 };
      dir = normalize(Vec3{ nx * t * aspect, ny * t, -1 });
    }

    // view = T(pan, -d) R T(-centre)  =>  world = centre + R^T (view - (pan, -d))
    const Quat inv = conjugate(orient);
    Ray r;
    r.origin = centre + rotate(inv, o - Vec3{ px, py, -d });
    r.dir = rotate(inv, dir);
    return r;
  }

  Vec3 Camera::project(Vec3 world) const {
    Vec3 ndc = transformPoint(projectionMatrix() * viewMatrix(), world);
    Vec3 viewPos = transformPoint(viewMatrix(), world);
    return { (ndc.x + 1) * 0.5f * vw, (1 - ndc.y) * 0.5f * vh, viewPos.z };
  }

  // --- helpers ------------------------------------------------------------

  Quat Camera::fromYawPitchRoll(float yawDeg, float pitchDeg, float rollDeg) {
    return Quat::axisAngle({ 0, 0, 1 }, radians(rollDeg)) *
           Quat::axisAngle({ 1, 0, 0 }, radians(pitchDeg)) *
           Quat::axisAngle({ 0, 1, 0 }, radians(yawDeg));
  }

  Quat Camera::lookFrom(Vec3 dir, float yawHintDeg) {
    Vec3 d = normalize(dir);
    // with R = Rx(pitch) Ry(yaw), the world direction toward the viewer is
    // (-cos p sin y, sin p, cos p cos y)
    if (std::fabs(d.y) > 0.9999f) {
      const float yaw = std::round(yawHintDeg / 90.0f) * 90.0f;
      return fromYawPitchRoll(yaw, d.y > 0 ? 90.0f : -90.0f, 0);
    }
    const float yaw = degrees(std::atan2(-d.x, d.z));
    const float pitch = degrees(std::atan2(d.y, std::sqrt(d.x * d.x + d.z * d.z)));
    return fromYawPitchRoll(yaw, pitch, 0);
  }

  float Camera::yawDeg(void) const {
    Vec3 d = viewAxisInWorld(orient, 2);
    if (std::fabs(d.y) > 0.9999f) {
      // looking along Y: the yaw is in where "up" on screen points
      Vec3 u = viewAxisInWorld(orient, 1);
      return degrees(std::atan2(d.y > 0 ? u.x : -u.x, d.y > 0 ? -u.z : u.z));
    }
    return degrees(std::atan2(-d.x, d.z));
  }
}
