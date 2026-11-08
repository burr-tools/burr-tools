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
#include "vecmath.h"

namespace btui {

  Quat quatFromRows(Vec3 r, Vec3 u, Vec3 n) {
    // R(row, col): row 0 = r, row 1 = u, row 2 = n
    const float m00 = r.x, m01 = r.y, m02 = r.z;
    const float m10 = u.x, m11 = u.y, m12 = u.z;
    const float m20 = n.x, m21 = n.y, m22 = n.z;
    const float tr = m00 + m11 + m22;
    Quat q;
    if (tr > 0) {
      float s = std::sqrt(tr + 1.0f) * 2;
      q = { 0.25f * s, (m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s };
    } else if (m00 > m11 && m00 > m22) {
      float s = std::sqrt(1.0f + m00 - m11 - m22) * 2;
      q = { (m21 - m12) / s, 0.25f * s, (m01 + m10) / s, (m02 + m20) / s };
    } else if (m11 > m22) {
      float s = std::sqrt(1.0f + m11 - m00 - m22) * 2;
      q = { (m02 - m20) / s, (m01 + m10) / s, 0.25f * s, (m12 + m21) / s };
    } else {
      float s = std::sqrt(1.0f + m22 - m00 - m11) * 2;
      q = { (m10 - m01) / s, (m02 + m20) / s, (m12 + m21) / s, 0.25f * s };
    }
    return normalize(q);
  }

  Mat4 Mat4::rotation(Quat q) {
    Mat4 r;
    for (int c = 0; c < 3; c++) {
      Vec3 e{ c == 0 ? 1.0f : 0.0f, c == 1 ? 1.0f : 0.0f, c == 2 ? 1.0f : 0.0f };
      Vec3 col = rotate(q, e);
      r(0, c) = col.x;
      r(1, c) = col.y;
      r(2, c) = col.z;
    }
    return r;
  }

  Mat4 Mat4::perspective(float fovYRad, float aspect, float zNear, float zFar) {
    Mat4 r;
    const float f = 1.0f / std::tan(fovYRad / 2);
    r.m.fill(0);
    r(0, 0) = f / aspect;
    r(1, 1) = f;
    r(2, 2) = (zFar + zNear) / (zNear - zFar);
    r(2, 3) = 2 * zFar * zNear / (zNear - zFar);
    r(3, 2) = -1;
    return r;
  }

  Mat4 Mat4::orthographic(float halfW, float halfH, float zNear, float zFar) {
    Mat4 r;
    r(0, 0) = 1.0f / halfW;
    r(1, 1) = 1.0f / halfH;
    r(2, 2) = -2.0f / (zFar - zNear);
    r(2, 3) = -(zFar + zNear) / (zFar - zNear);
    return r;
  }

  Mat4 operator*(const Mat4 & a, const Mat4 & b) {
    Mat4 r;
    for (int row = 0; row < 4; row++)
      for (int col = 0; col < 4; col++) {
        float s = 0;
        for (int k = 0; k < 4; k++)
          s += a(row, k) * b(k, col);
        r(row, col) = s;
      }
    return r;
  }

  Vec3 transformPoint(const Mat4 & m, Vec3 p) {
    float x = m(0, 0) * p.x + m(0, 1) * p.y + m(0, 2) * p.z + m(0, 3);
    float y = m(1, 0) * p.x + m(1, 1) * p.y + m(1, 2) * p.z + m(1, 3);
    float z = m(2, 0) * p.x + m(2, 1) * p.y + m(2, 2) * p.z + m(2, 3);
    float w = m(3, 0) * p.x + m(3, 1) * p.y + m(3, 2) * p.z + m(3, 3);
    if (std::fabs(w) < 1e-12f) w = 1e-12f;
    return { x / w, y / w, z / w };
  }
}
