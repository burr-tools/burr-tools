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

#include <algorithm>
#include <cmath>

namespace btui {

  namespace {

    // the legacy tables (src/gui/piececolor.cpp now wraps these)
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
     * table.
     *
     * The arithmetic below is legacy's to the bit, double where it used
     * double, so the FLTK GUI's piececolor wrappers return what they always
     * did; test_ui_palette pins it.
     */
    bool inTable(int x) { return x >= 0 && x < COLS; }
    double sineR(int x) { return 1 + std::sin(0.7 * x); }
    double sineG(int x) { return 1 + std::sin(1.3 * x + 1.5); }
    double sineB(int x) { return 1 + std::sin(3.5 * x + 2.3); }
    float baseR(int x) { return inTable(x) ? tr[x] : float(sineR(x) / 2); }
    float baseG(int x) { return inTable(x) ? tg[x] : float(sineG(x) / 2); }
    float baseB(int x) { return inTable(x) ? tb[x] : float(sineB(x) / 2); }

    /* 0 to 255: the table's float times 255, the formula's double, cut off */
    unsigned int base8(const float * table, double (*sine)(int), int x) {
      return inTable(x) ? (unsigned int)(table[x] * 255) : (unsigned int)(255 * sine(x) / 2);
    }

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

    float ramp(float v) { return float(0.5 + 0.5 * std::fabs(1 - 2 * v)); }

    /* a channel of copy `copy`: its jitter, scaled down near the ends */
    double jittered(float v, float jitter, double scale) { return v + jitter * scale * ramp(v); }

    struct Jittered {
      double r, g, b;
    };

    Jittered jitteredColor(int shape, int copy) {
      const int j = jitterIndex(shape, copy);
      return { jittered(baseR(shape), jr[j], 0.5), jittered(baseG(shape), jg[j], 0.4),
               jittered(baseB(shape), jb[j], 0.7) };
    }
  }

  Rgb pieceColor(int shape) {
    return { baseR(shape), baseG(shape), baseB(shape) };
  }

  Rgb pieceColor(int shape, int copy) {
    const Jittered c = jitteredColor(shape, copy);
    return { float(c.r), float(c.g), float(c.b) };
  }

  Rgb8 pieceColor8(int shape) {
    return { base8(tr, sineR, shape), base8(tg, sineG, shape), base8(tb, sineB, shape) };
  }

  Rgb8 pieceColor8(int shape, int copy) {
    const Jittered c = jitteredColor(shape, copy);
    return { (unsigned int)(c.r * 255), (unsigned int)(c.g * 255), (unsigned int)(c.b * 255) };
  }

  float relativeLuminance(const Rgb & c) {
    auto lin = [](float v) { return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f); };
    return 0.2126f * lin(c.r) + 0.7152f * lin(c.g) + 0.0722f * lin(c.b);
  }

  float contrastRatio(const Rgb & a, const Rgb & b) {
    const float la = relativeLuminance(a), lb = relativeLuminance(b);
    return (std::max(la, lb) + 0.05f) / (std::min(la, lb) + 0.05f);
  }

  bool prefersWhiteText(const Rgb & background) {
    return contrastRatio(background, { 1, 1, 1 }) > contrastRatio(background, { 0, 0, 0 });
  }

  bool prefersWhiteText(int shape) {
    return prefersWhiteText(pieceColor(shape));
  }
}
