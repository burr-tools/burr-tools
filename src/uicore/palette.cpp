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
#include "palette.h"

#include <cmath>

namespace btui {

  namespace {

    // copied verbatim from src/gui/piececolor.cpp; keep the two in step
    constexpr int COLS = 18;

    const float tr[COLS] = {
      0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.6f,
      0.0f, 0.6f, 0.6f, 0.0f, 0.6f, 0.0f, 0.6f, 1.0f, 1.0f
    };
    const float tg[COLS] = {
      0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.6f, 0.0f,
      0.6f, 0.6f, 0.0f, 1.0f, 1.0f, 0.6f, 0.0f, 0.6f, 0.0f
    };
    const float tb[COLS] = {
      1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.6f, 0.0f, 0.0f,
      0.6f, 0.0f, 0.6f, 0.6f, 0.0f, 1.0f, 1.0f, 0.0f, 0.6f
    };

    constexpr int JITTERS = 53;

    const float jr[JITTERS] = {
       0.0f,
      -0.3f,  0.3f, -0.3f,  0.3f, -0.3f,  0.3f, -0.3f,  0.3f,  0.3f,
      -0.3f, -0.3f,  0.3f,  0.3f, -0.3f, -0.3f,  0.3f,  0.0f,  0.0f,
       0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.3f, -0.3f,
      -0.4f,  0.4f, -0.4f,  0.4f, -0.4f,  0.4f, -0.4f,  0.4f,  0.4f,
      -0.4f, -0.4f,  0.4f,  0.4f, -0.4f, -0.4f,  0.4f,  0.0f,  0.0f,
       0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.4f, -0.4f
    };
    const float jg[JITTERS] = {
       0.0f,
      -0.3f,  0.3f,  0.3f, -0.3f, -0.3f,  0.3f,  0.3f, -0.3f,  0.3f,
      -0.3f,  0.3f, -0.3f,  0.0f,  0.0f,  0.0f,  0.0f,  0.3f, -0.3f,
      -0.3f,  0.3f,  0.0f,  0.0f,  0.3f, -0.3f,  0.0f,  0.0f,
      -0.4f,  0.4f,  0.4f, -0.4f, -0.4f,  0.4f,  0.4f, -0.4f,  0.4f,
      -0.4f,  0.4f, -0.4f,  0.0f,  0.0f,  0.0f,  0.0f,  0.4f, -0.4f,
      -0.4f,  0.4f,  0.0f,  0.0f,  0.4f, -0.4f,  0.0f,  0.0f
    };
    const float jb[JITTERS] = {
       0.0f,
      -0.3f,  0.3f,  0.3f, -0.3f,  0.3f, -0.3f, -0.3f,  0.3f,  0.0f,
       0.0f,  0.0f,  0.0f,  0.3f, -0.3f,  0.3f, -0.3f,  0.3f, -0.3f,
       0.3f, -0.3f,  0.3f, -0.3f,  0.0f,  0.0f,  0.0f,  0.0f,
      -0.4f,  0.4f,  0.4f, -0.4f,  0.4f, -0.4f, -0.4f,  0.4f,  0.0f,
       0.0f,  0.0f,  0.0f,  0.4f, -0.4f,  0.4f, -0.4f,  0.4f, -0.4f,
       0.4f, -0.4f,  0.4f, -0.4f,  0.0f,  0.0f,  0.0f,  0.0f
    };

    /* the table for the first COLS shapes, a formula beyond -- and for any
     * index a view may hand over while its rows go (-1): never outside the
     * table */
    bool inTable(int x) { return x >= 0 && x < COLS; }
    float baseR(int x) { return inTable(x) ? tr[x] : float((1 + std::sin(0.7 * x)) / 2); }
    float baseG(int x) { return inTable(x) ? tg[x] : float((1 + std::sin(1.3 * x + 1.5)) / 2); }
    float baseB(int x) { return inTable(x) ? tb[x] : float((1 + std::sin(3.5 * x + 2.3)) / 2); }

    /* the copy-th jitter entry that keeps every channel inside [0, 1]; when
     * the table runs out there is no jitter any more (legacy getJitter)
     */
    int jitterIndex(int val, int copy) {
      int j = 0;
      while (j < JITTERS) {
        float x = baseR(val) + jr[j];
        if (x < 0 || x > 1) { j++; continue; }
        x = baseG(val) + jg[j];
        if (x < 0 || x > 1) { j++; continue; }
        x = baseB(val) + jb[j];
        if (x < 0 || x > 1) { j++; continue; }
        if (copy == 0)
          break;
        copy--;
        j++;
      }
      return j == JITTERS ? 0 : j;
    }

    float ramp(float v) { return 0.5f + 0.5f * std::fabs(1 - 2 * v); }
  }

  Rgb pieceColor(int shape) {
    return { baseR(shape), baseG(shape), baseB(shape) };
  }

  Rgb pieceColor(int shape, int copy) {
    int j = jitterIndex(shape, copy);
    float r = baseR(shape), g = baseG(shape), b = baseB(shape);
    return { r + jr[j] * 0.5f * ramp(r), g + jg[j] * 0.4f * ramp(g), b + jb[j] * 0.7f * ramp(b) };
  }

  bool prefersWhiteText(int shape) {
    // the integer channels legacy uses: truncation of value*255
    auto ch = [](float f) { return static_cast<unsigned int>(f * 255); };
    Rgb c = pieceColor(shape);
    return 3 * ch(c.r) + 6 * ch(c.g) + ch(c.b) < 1275;
  }
}
