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
#ifndef BTUI_VECMATH_H
#define BTUI_VECMATH_H

#include <array>
#include <cmath>

/* The little linear algebra the 3D view needs, without a toolkit: 3-vectors,
 * unit quaternions for orientations, and column-major 4x4 matrices laid out
 * the way GPU uniform buffers expect them.
 *
 * Conventions used by the whole viewport: right-handed, view space looks
 * down -Z with +X right and +Y up, so a world point p relative to the camera
 * target appears at rotate(orientation, p) in view space.
 */
namespace btui {

  constexpr float kPi = 3.14159265358979323846f;
  inline float radians(float deg) { return deg * kPi / 180.0f; }
  inline float degrees(float rad) { return rad * 180.0f / kPi; }

  struct Vec3 {
    float x = 0, y = 0, z = 0;

    constexpr Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(Vec3 o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vec3 operator-(Vec3 o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vec3 operator-() const { return { -x, -y, -z }; }
    Vec3 operator*(float s) const { return { x * s, y * s, z * s }; }
    Vec3 & operator+=(Vec3 o) { x += o.x; y += o.y; z += o.z; return *this; }
    float operator[](int i) const { return i == 0 ? x : (i == 1 ? y : z); }
  };

  inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
  inline Vec3 cross(Vec3 a, Vec3 b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
  inline float length(Vec3 a) { return std::sqrt(dot(a, a)); }
  inline Vec3 normalize(Vec3 a) { float l = length(a); return l > 1e-12f ? a * (1.0f / l) : Vec3(0, 0, 0); }

  /* A rotation; (w, x, y, z), kept normalised by the operations below. */
  struct Quat {
    float w = 1, x = 0, y = 0, z = 0;

    static Quat axisAngle(Vec3 axis, float rad) {
      Vec3 a = normalize(axis);
      float s = std::sin(rad / 2);
      return { std::cos(rad / 2), a.x * s, a.y * s, a.z * s };
    }
  };

  inline Quat normalize(Quat q) {
    float n = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    if (n < 1e-12f) return Quat();
    return { q.w / n, q.x / n, q.y / n, q.z / n };
  }

  /* a * b: first b, then a */
  inline Quat operator*(Quat a, Quat b) {
    return normalize(Quat{
      a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
      a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
      a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
      a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w });
  }

  inline Quat conjugate(Quat q) { return { q.w, -q.x, -q.y, -q.z }; }

  inline Vec3 rotate(Quat q, Vec3 v) {
    Vec3 u{ q.x, q.y, q.z };
    Vec3 t = cross(u, v) * 2.0f;
    return v + t * q.w + cross(u, t);
  }

  inline float quatDot(Quat a, Quat b) { return a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z; }

  /* spherical interpolation along the shortest path */
  inline Quat slerp(Quat a, Quat b, float t) {
    float d = quatDot(a, b);
    if (d < 0) { b = { -b.w, -b.x, -b.y, -b.z }; d = -d; }
    float s0, s1;
    if (d > 0.9995f) {
      s0 = 1 - t; s1 = t;
    } else {
      float th = std::acos(d), st = std::sin(th);
      s0 = std::sin((1 - t) * th) / st;
      s1 = std::sin(t * th) / st;
    }
    return normalize(Quat{ s0 * a.w + s1 * b.w, s0 * a.x + s1 * b.x, s0 * a.y + s1 * b.y, s0 * a.z + s1 * b.z });
  }

  /* The angle of the rotation that takes a to b, in radians. Computed from
   * the relative rotation with atan2 rather than acos(dot): for two equal
   * float quaternions the dot product rounds to just below 1, and acos turns
   * that rounding into an angle of about 0.04 degrees. */
  inline float angleBetween(Quat a, Quat b) {
    Quat r = conjugate(a) * b;
    float v = std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z);
    return 2.0f * std::atan2(v, std::fabs(r.w));
  }

  /* The rotation whose matrix has these rows (the view's right, up and
   * toward-the-viewer directions expressed in world coordinates).
   */
  Quat quatFromRows(Vec3 r, Vec3 u, Vec3 n);

  /* the rows of a rotation's matrix: index 0 right, 1 up, 2 toward the viewer */
  inline Vec3 viewAxisInWorld(Quat q, int i) {
    Vec3 e{ i == 0 ? 1.0f : 0.0f, i == 1 ? 1.0f : 0.0f, i == 2 ? 1.0f : 0.0f };
    return rotate(conjugate(q), e);
  }

  /* Column-major 4x4 matrix: m[col*4 + row]. */
  struct Mat4 {
    std::array<float, 16> m{ 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 };

    float & operator()(int row, int col) { return m[size_t(col * 4 + row)]; }
    float operator()(int row, int col) const { return m[size_t(col * 4 + row)]; }

    static Mat4 translation(Vec3 t) { Mat4 r; r(0, 3) = t.x; r(1, 3) = t.y; r(2, 3) = t.z; return r; }
    static Mat4 rotation(Quat q);
    static Mat4 scale(float s) { Mat4 r; r(0, 0) = r(1, 1) = r(2, 2) = s; return r; }

    /* OpenGL-style clip space: z in [-1, 1]. The renderer corrects for the
     * graphics API with QRhi's clipSpaceCorrMatrix(). */
    static Mat4 perspective(float fovYRad, float aspect, float zNear, float zFar);
    static Mat4 orthographic(float halfW, float halfH, float zNear, float zFar);
  };

  Mat4 operator*(const Mat4 & a, const Mat4 & b);

  /* transform a point (w = 1) and divide by w */
  Vec3 transformPoint(const Mat4 & m, Vec3 p);
}

#endif
