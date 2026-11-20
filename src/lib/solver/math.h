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
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef __SOLVER_MATH_H__
#define __SOLVER_MATH_H__

/** @file
 * Small public linear-algebra value types for the BurrTools core API
 * (design/2026-11-20-core-api.md, section 2): 3D vectors (integer and
 * floating-point), 3x3 physical rotation/linear matrices, and 3D rigid poses.
 *
 * Pure value types with no I/O, GUI, XML, or document dependencies.
 */

#include "../bt_assert.h"

#include <array>
#include <cmath>
#include <compare>
#include <type_traits>

template <typename T>
struct vec3_t {
  T x = T(0);
  T y = T(0);
  T z = T(0);

  constexpr vec3_t() = default;
  constexpr vec3_t(T x_, T y_, T z_) : x(x_), y(y_), z(z_) {}

  constexpr bool operator==(const vec3_t &) const = default;
  constexpr auto operator<=>(const vec3_t &) const = default;

  constexpr vec3_t operator+(const vec3_t & o) const { return { x + o.x, y + o.y, z + o.z }; }
  constexpr vec3_t operator-(const vec3_t & o) const { return { x - o.x, y - o.y, z - o.z }; }
  constexpr vec3_t operator-() const { return { -x, -y, -z }; }
  constexpr vec3_t operator*(T s) const { return { x * s, y * s, z * s }; }

  constexpr vec3_t & operator+=(const vec3_t & o) { x += o.x; y += o.y; z += o.z; return *this; }
  constexpr vec3_t & operator-=(const vec3_t & o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
  constexpr vec3_t & operator*=(T s) { x *= s; y *= s; z *= s; return *this; }

  constexpr T & operator[](int i) {
    bt_assert(i >= 0 && i < 3);
    return i == 0 ? x : (i == 1 ? y : z);
  }
  constexpr const T & operator[](int i) const {
    bt_assert(i >= 0 && i < 3);
    return i == 0 ? x : (i == 1 ? y : z);
  }
};

template <typename T>
constexpr vec3_t<T> operator*(T s, const vec3_t<T> & v) { return { s * v.x, s * v.y, s * v.z }; }

template <typename T>
constexpr T dot(const vec3_t<T> & a, const vec3_t<T> & b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

template <typename T>
constexpr vec3_t<T> cross(const vec3_t<T> & a, const vec3_t<T> & b) {
  return {
    a.y * b.z - a.z * b.y,
    a.z * b.x - a.x * b.z,
    a.x * b.y - a.y * b.x
  };
}

template <typename T>
inline double length(const vec3_t<T> & a) {
  return std::sqrt(static_cast<double>(dot(a, a)));
}

using vec3i_t = vec3_t<int>;
using vec3f_t = vec3_t<float>;
using vec3d_t = vec3_t<double>;

/** Row-major 3x3 matrix in physical 3D space (applied to column vectors: m * v). */
struct mat3d_t {
  std::array<double, 9> m = {
    1.0, 0.0, 0.0,
    0.0, 1.0, 0.0,
    0.0, 0.0, 1.0
  };

  constexpr mat3d_t() = default;
  constexpr explicit mat3d_t(const std::array<double, 9> & entries) : m(entries) {}

  static constexpr mat3d_t identity() { return mat3d_t{}; }

  static mat3d_t fromArray(const double a[9]) {
    mat3d_t out;
    for (int i = 0; i < 9; i++)
      out.m[i] = a[i];
    return out;
  }

  void toArray(double a[9]) const {
    for (int i = 0; i < 9; i++)
      a[i] = m[i];
  }

  constexpr double & at(int r, int c) {
    bt_assert(r >= 0 && r < 3 && c >= 0 && c < 3);
    return m[r * 3 + c];
  }
  constexpr double at(int r, int c) const {
    bt_assert(r >= 0 && r < 3 && c >= 0 && c < 3);
    return m[r * 3 + c];
  }

  constexpr bool operator==(const mat3d_t &) const = default;

  constexpr vec3d_t operator*(const vec3d_t & v) const {
    return {
      m[0] * v.x + m[1] * v.y + m[2] * v.z,
      m[3] * v.x + m[4] * v.y + m[5] * v.z,
      m[6] * v.x + m[7] * v.y + m[8] * v.z
    };
  }

  constexpr mat3d_t operator*(const mat3d_t & o) const {
    mat3d_t r{};
    for (int i = 0; i < 3; i++)
      for (int j = 0; j < 3; j++)
        r.m[i * 3 + j] = m[i * 3 + 0] * o.m[0 * 3 + j]
                       + m[i * 3 + 1] * o.m[1 * 3 + j]
                       + m[i * 3 + 2] * o.m[2 * 3 + j];
    return r;
  }

  constexpr mat3d_t transpose() const {
    return mat3d_t({
      m[0], m[3], m[6],
      m[1], m[4], m[7],
      m[2], m[5], m[8]
    });
  }

  constexpr double determinant() const {
    return m[0] * (m[4] * m[8] - m[5] * m[7])
         - m[1] * (m[3] * m[8] - m[5] * m[6])
         + m[2] * (m[3] * m[7] - m[4] * m[6]);
  }
};

/**
 * Physical 3D rigid/orthogonal pose: p' = rotation * p + translation.
 * Used for piece placements in physical space, meshes, and disassembly frames.
 */
struct pose3d_t {
  mat3d_t rotation = mat3d_t::identity();
  vec3d_t translation{};

  constexpr pose3d_t() = default;
  constexpr pose3d_t(const mat3d_t & r, const vec3d_t & t) : rotation(r), translation(t) {}

  static constexpr pose3d_t identity() { return pose3d_t{}; }

  constexpr bool operator==(const pose3d_t &) const = default;

  constexpr vec3d_t apply(const vec3d_t & p) const {
    return rotation * p + translation;
  }

  /** Compose poses: (a * b).apply(p) == a.apply(b.apply(p)). */
  constexpr pose3d_t operator*(const pose3d_t & o) const {
    return { rotation * o.rotation, rotation * o.translation + translation };
  }

  /** Inverse for orthogonal rotation (rotation^T == rotation^-1). */
  constexpr pose3d_t inverse() const {
    const mat3d_t rt = rotation.transpose();
    return { rt, -(rt * translation) };
  }

  /** Export as column-major 4x4 matrix (OpenGL / QRhi / GPU uniform layout). */
  template <typename FloatT>
  void toMat4ColMajor(FloatT out[16]) const {
    out[0]  = static_cast<FloatT>(rotation.m[0]);
    out[1]  = static_cast<FloatT>(rotation.m[3]);
    out[2]  = static_cast<FloatT>(rotation.m[6]);
    out[3]  = static_cast<FloatT>(0);

    out[4]  = static_cast<FloatT>(rotation.m[1]);
    out[5]  = static_cast<FloatT>(rotation.m[4]);
    out[6]  = static_cast<FloatT>(rotation.m[7]);
    out[7]  = static_cast<FloatT>(0);

    out[8]  = static_cast<FloatT>(rotation.m[2]);
    out[9]  = static_cast<FloatT>(rotation.m[5]);
    out[10] = static_cast<FloatT>(rotation.m[8]);
    out[11] = static_cast<FloatT>(0);

    out[12] = static_cast<FloatT>(translation.x);
    out[13] = static_cast<FloatT>(translation.y);
    out[14] = static_cast<FloatT>(translation.z);
    out[15] = static_cast<FloatT>(1);
  }
};

#endif // __SOLVER_MATH_H__
