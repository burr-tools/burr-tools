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
#include "viewcube.h"
#include "camera.h"

#include <algorithm>

namespace btui {

  namespace {

    /* The label frames: side faces read with world +Y up; the top and bottom
     * with +X to the right, as seen from outside the cube. */
    const ViewCube::Face faces[6] = {
      { { 1, 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 }, "+X" },
      { { -1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 }, "-X" },
      { { 0, 1, 0 }, { 1, 0, 0 }, { 0, 0, -1 }, "+Y" },
      { { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, 1 }, "-Y" },
      { { 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 }, "+Z" },
      { { 0, 0, -1 }, { -1, 0, 0 }, { 0, 1, 0 }, "-Z" },
    };

    bool axisAligned(Vec3 v) {
      float m = std::max({ std::fabs(v.x), std::fabs(v.y), std::fabs(v.z) });
      return m > 0.999f;
    }

    int faceIndex(Vec3 n) {
      for (int i = 0; i < 6; i++)
        if (dot(faces[i].n, n) > 0.5f)
          return i;
      return 4;
    }

    float roundTo90(float deg) { return std::round(deg / 90.0f) * 90.0f; }
  }

  const ViewCube::Face & ViewCube::face(int i) { return faces[std::clamp(i, 0, 5)]; }

  Vec3 ViewCube::project(Quat orient, bool perspective, Vec3 p) {
    Vec3 q = rotate(orient, p);
    float s = perspective ? (kEyeDistance - 1.0f) / (kEyeDistance - q.z) : 1.0f;
    return { kCentreX + q.x * s * kHalf, kCentreY - q.y * s * kHalf, q.z };
  }

  bool ViewCube::faceVisible(Quat orient, bool perspective, int f) {
    Vec3 nv = rotate(orient, faces[f].n);
    // perspective: the face centre (at nv) is seen when its normal points at
    // the eye (0, 0, D): (E - nv) . nv > 0  <=>  nv.z > 1 / D
    return perspective ? nv.z > 1.0f / kEyeDistance : nv.z > 1e-4f;
  }

  bool ViewCube::faceAligned(Quat orient) {
    return axisAligned(viewAxisInWorld(orient, 2)) && axisAligned(viewAxisInWorld(orient, 1));
  }

  ViewCube::Hit ViewCube::hitTest(Quat orient, bool perspective, float x, float y) {
    Hit h;

    auto near = [&](float cx, float cy, float r) {
      return (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r;
    };

    if (std::fabs(x - kHomeX) <= kHomeHit && std::fabs(y - kHomeY) <= kHomeHit) {
      h.kind = Kind::Home;
      return h;
    }

    if (faceAligned(orient)) {
      const struct { Arrow a; float x, y; } arrows[] = {
        { Arrow::Up, kCentreX, kCentreY - kArrowOffset },
        { Arrow::Down, kCentreX, kCentreY + kArrowOffset },
        { Arrow::Left, kCentreX - kArrowOffset, kCentreY },
        { Arrow::Right, kCentreX + kArrowOffset, kCentreY },
      };
      for (const auto & a : arrows)
        if (near(a.x, a.y, kArrowHit)) {
          h.kind = Kind::Arrow;
          h.arrow = a.a;
          return h;
        }
      // the hit circles sit slightly outward of the drawn arcs (C13)
      if (near(kRollCcwX, kRollCcwY - 4, kRollHit)) { h.kind = Kind::Roll; h.roll = +1; return h; }
      if (near(kRollCwX + 2, kRollCwY, kRollHit))   { h.kind = Kind::Roll; h.roll = -1; return h; }
    }

    // cast a ray through the point into the cube
    const float X = (x - kCentreX) / kHalf;
    const float Y = (kCentreY - y) / kHalf;
    Vec3 o, d;
    if (perspective) {
      o = { 0, 0, kEyeDistance };
      d = Vec3{ X, Y, 1.0f } - o;
    } else {
      o = { X, Y, 10.0f };
      d = { 0, 0, -1 };
    }
    const Quat inv = conjugate(orient);
    o = rotate(inv, o);
    d = rotate(inv, d);

    float tmin = -1e30f, tmax = 1e30f;
    for (int i = 0; i < 3; i++) {
      if (std::fabs(d[i]) < 1e-9f) {
        if (o[i] < -1 || o[i] > 1)
          return h;
        continue;
      }
      float t1 = (-1 - o[i]) / d[i], t2 = (1 - o[i]) / d[i];
      if (t1 > t2) std::swap(t1, t2);
      tmin = std::max(tmin, t1);
      tmax = std::min(tmax, t2);
    }
    if (tmin > tmax || tmax < 0)
      return h;

    const Vec3 p = o + d * tmin;
    // the face is the axis where the point sits on the surface
    int axis = 0;
    for (int i = 1; i < 3; i++)
      if (std::fabs(p[i]) > std::fabs(p[axis]))
        axis = i;
    Vec3 n{ axis == 0 ? (p.x > 0 ? 1.0f : -1.0f) : 0.0f, axis == 1 ? (p.y > 0 ? 1.0f : -1.0f) : 0.0f,
            axis == 2 ? (p.z > 0 ? 1.0f : -1.0f) : 0.0f };
    const Face & f = faces[faceIndex(n)];
    const float u = dot(p, f.b), v = dot(p, f.t);

    Vec3 dir = n;
    if (std::fabs(u) > kInner) dir += f.b * (u > 0 ? 1.0f : -1.0f);
    if (std::fabs(v) > kInner) dir += f.t * (v > 0 ? 1.0f : -1.0f);

    h.kind = Kind::Region;
    h.dir = { int(std::lround(dir.x)), int(std::lround(dir.y)), int(std::lround(dir.z)) };
    return h;
  }

  std::vector<ViewCube::Patch> ViewCube::regionPatches(CubeDir dir) {
    std::vector<Patch> out;
    const Vec3 d = dir.vec();
    for (int i = 0; i < 6; i++) {
      const Face & f = faces[i];
      if (dot(f.n, d) < 0.5f)
        continue;      // the region does not touch this face
      auto range = [](float c, float * lo, float * hi) {
        if (c > 0.5f)       { *lo = kInner; *hi = 1; }
        else if (c < -0.5f) { *lo = -1; *hi = -kInner; }
        else                { *lo = -kInner; *hi = kInner; }
      };
      Patch p;
      p.face = i;
      range(dot(f.b, d), &p.u0, &p.u1);
      range(dot(f.t, d), &p.v0, &p.v1);
      out.push_back(p);
    }
    return out;
  }

  float ViewCube::yawOf(Quat q) {
    Camera c;
    c.setOrientation(q);
    return c.yawDeg();
  }

  std::optional<Quat> ViewCube::target(const Hit & h, Quat current) {
    switch (h.kind) {

      case Kind::Region:
        return Camera::lookFrom(h.dir.vec(), yawOf(current));

      case Kind::Arrow:
      case Kind::Roll: {
        // legacy viewCube_c::applyNav, on the rows of the current rotation
        const Vec3 r = viewAxisInWorld(current, 0);
        const Vec3 u = viewAxisInWorld(current, 1);
        const Vec3 n = viewAxisInWorld(current, 2);
        Vec3 nr, nu, nn;
        if (h.kind == Kind::Arrow) {
          switch (h.arrow) {
            case Arrow::Up:    nr = r;  nu = -n; nn = u;  break;
            case Arrow::Down:  nr = r;  nu = n;  nn = -u; break;
            case Arrow::Right: nr = -n; nu = u;  nn = r;  break;
            case Arrow::Left:  nr = n;  nu = u;  nn = -r; break;
          }
        } else if (h.roll < 0) {   // clockwise
          nr = u;  nu = -r; nn = n;
        } else {                   // counter-clockwise
          nr = -u; nu = r;  nn = n;
        }
        return quatFromRows(nr, nu, nn);
      }

      default:
        return std::nullopt;
    }
  }

  Quat ViewCube::uprightTarget(CubeDir faceDir, Quat current) {
    const Vec3 n = normalize(faceDir.vec());
    const Face & f = faces[faceIndex(n)];
    const Quat base = Camera::lookFrom(n, yawOf(current));
    // turn the view so the label's baseline points to screen right
    const Vec3 b = rotate(base, f.b);
    const float roll = roundTo90(-degrees(std::atan2(b.y, b.x)));
    return Quat::axisAngle({ 0, 0, 1 }, radians(roll)) * base;
  }

  std::optional<Quat> ViewCube::snapNearest(Quat current) {
    const Vec3 fwd = viewAxisInWorld(current, 2);
    float best = -2;
    CubeDir bestDir;
    for (int x = -1; x <= 1; x++)
      for (int y = -1; y <= 1; y++)
        for (int z = -1; z <= 1; z++) {
          CubeDir d{ x, y, z };
          if (d.count() == 0)
            continue;
          float s = dot(fwd, normalize(d.vec()));
          if (s > best) { best = s; bestDir = d; }
        }
    if (best > 0.97f)
      return Camera::lookFrom(bestDir.vec(), yawOf(current));
    return std::nullopt;
  }
}
