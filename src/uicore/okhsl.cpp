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

/* The conversion below is Björn Ottosson's reference implementation of
 * Okhsl (https://bottosson.github.io/posts/colorpicker/ and
 * https://bottosson.github.io/posts/gamutclipping/), reformatted:
 *
 * Copyright (c) 2021 Björn Ottosson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
 * NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "okhsl.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace btui {

  namespace {

    constexpr float kPi = 3.14159265358979323846f;

    struct Lab { float L, a, b; };
    struct LC { float L, C; };
    struct ST { float S, T; };

    Rgb oklabToLinearSrgb(Lab c) {
      const float l_ = c.L + 0.3963377774f * c.a + 0.2158037573f * c.b;
      const float m_ = c.L - 0.1055613458f * c.a - 0.0638541728f * c.b;
      const float s_ = c.L - 0.0894841775f * c.a - 1.2914855480f * c.b;

      const float l = l_ * l_ * l_;
      const float m = m_ * m_ * m_;
      const float s = s_ * s_ * s_;

      return {
        +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s,
        -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s,
        -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s,
      };
    }

    float srgbTransfer(float a) {
      return 0.0031308f >= a ? 12.92f * a : 1.055f * std::pow(a, 0.4166666666666667f) - 0.055f;
    }

    // the maximum saturation S = C/L of a hue that fits in sRGB; a and b normalised
    float computeMaxSaturation(float a, float b) {
      float k0, k1, k2, k3, k4, wl, wm, ws;

      if (-1.88170328f * a - 0.80936493f * b > 1) {
        // red goes below zero first
        k0 = +1.19086277f; k1 = +1.76576728f; k2 = +0.59662641f; k3 = +0.75515197f; k4 = +0.56771245f;
        wl = +4.0767416621f; wm = -3.3077115913f; ws = +0.2309699292f;
      } else if (1.81444104f * a - 1.19445276f * b > 1) {
        // green
        k0 = +0.73956515f; k1 = -0.45954404f; k2 = +0.08285427f; k3 = +0.12541070f; k4 = +0.14503204f;
        wl = -1.2684380046f; wm = +2.6097574011f; ws = -0.3413193965f;
      } else {
        // blue
        k0 = +1.35733652f; k1 = -0.00915799f; k2 = -1.15130210f; k3 = -0.50559606f; k4 = +0.00692167f;
        wl = -0.0041960863f; wm = -0.7034186147f; ws = +1.7076147010f;
      }

      float S = k0 + k1 * a + k2 * b + k3 * a * a + k4 * a * b;

      // one step of Halley's method
      const float k_l = +0.3963377774f * a + 0.2158037573f * b;
      const float k_m = -0.1055613458f * a - 0.0638541728f * b;
      const float k_s = -0.0894841775f * a - 1.2914855480f * b;

      {
        const float l_ = 1.f + S * k_l;
        const float m_ = 1.f + S * k_m;
        const float s_ = 1.f + S * k_s;

        const float l = l_ * l_ * l_;
        const float m = m_ * m_ * m_;
        const float s = s_ * s_ * s_;

        const float l_dS = 3.f * k_l * l_ * l_;
        const float m_dS = 3.f * k_m * m_ * m_;
        const float s_dS = 3.f * k_s * s_ * s_;

        const float l_dS2 = 6.f * k_l * k_l * l_;
        const float m_dS2 = 6.f * k_m * k_m * m_;
        const float s_dS2 = 6.f * k_s * k_s * s_;

        const float f  = wl * l     + wm * m     + ws * s;
        const float f1 = wl * l_dS  + wm * m_dS  + ws * s_dS;
        const float f2 = wl * l_dS2 + wm * m_dS2 + ws * s_dS2;

        S = S - f * f1 / (f1 * f1 - 0.5f * f * f2);
      }

      return S;
    }

    LC findCusp(float a, float b) {
      const float S_cusp = computeMaxSaturation(a, b);
      const Rgb rgb_at_max = oklabToLinearSrgb({ 1, S_cusp * a, S_cusp * b });
      const float L_cusp = std::cbrt(1.f / std::max(std::max(rgb_at_max.r, rgb_at_max.g), rgb_at_max.b));
      const float C_cusp = L_cusp * S_cusp;
      return { L_cusp, C_cusp };
    }

    // where the line L = L0 (1 - t) + t L1, C = t C1 leaves the sRGB gamut
    float findGamutIntersection(float a, float b, float L1, float C1, float L0, LC cusp) {
      float t;
      if (((L1 - L0) * cusp.C - (cusp.L - L0) * C1) <= 0.f) {
        // lower half
        t = cusp.C * L0 / (C1 * cusp.L + cusp.C * (L0 - L1));
      } else {
        // upper half: the triangle first, then one step of Halley's method
        t = cusp.C * (L0 - 1.f) / (C1 * (cusp.L - 1.f) + cusp.C * (L0 - L1));

        const float dL = L1 - L0;
        const float dC = C1;

        const float k_l = +0.3963377774f * a + 0.2158037573f * b;
        const float k_m = -0.1055613458f * a - 0.0638541728f * b;
        const float k_s = -0.0894841775f * a - 1.2914855480f * b;

        const float l_dt = dL + dC * k_l;
        const float m_dt = dL + dC * k_m;
        const float s_dt = dL + dC * k_s;

        {
          const float L = L0 * (1.f - t) + t * L1;
          const float C = t * C1;

          const float l_ = L + C * k_l;
          const float m_ = L + C * k_m;
          const float s_ = L + C * k_s;

          const float l = l_ * l_ * l_;
          const float m = m_ * m_ * m_;
          const float s = s_ * s_ * s_;

          const float ldt = 3 * l_dt * l_ * l_;
          const float mdt = 3 * m_dt * m_ * m_;
          const float sdt = 3 * s_dt * s_ * s_;

          const float ldt2 = 6 * l_dt * l_dt * l_;
          const float mdt2 = 6 * m_dt * m_dt * m_;
          const float sdt2 = 6 * s_dt * s_dt * s_;

          const float r = 4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s - 1;
          const float r1 = 4.0767416621f * ldt - 3.3077115913f * mdt + 0.2309699292f * sdt;
          const float r2 = 4.0767416621f * ldt2 - 3.3077115913f * mdt2 + 0.2309699292f * sdt2;

          const float u_r = r1 / (r1 * r1 - 0.5f * r * r2);
          float t_r = -r * u_r;

          const float g = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s - 1;
          const float g1 = -1.2684380046f * ldt + 2.6097574011f * mdt - 0.3413193965f * sdt;
          const float g2 = -1.2684380046f * ldt2 + 2.6097574011f * mdt2 - 0.3413193965f * sdt2;

          const float u_g = g1 / (g1 * g1 - 0.5f * g * g2);
          float t_g = -g * u_g;

          const float bb = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s - 1;
          const float b1 = -0.0041960863f * ldt - 0.7034186147f * mdt + 1.7076147010f * sdt;
          const float b2 = -0.0041960863f * ldt2 - 0.7034186147f * mdt2 + 1.7076147010f * sdt2;

          const float u_b = b1 / (b1 * b1 - 0.5f * bb * b2);
          float t_b = -bb * u_b;

          t_r = u_r >= 0.f ? t_r : FLT_MAX;
          t_g = u_g >= 0.f ? t_g : FLT_MAX;
          t_b = u_b >= 0.f ? t_b : FLT_MAX;

          t += std::min(t_r, std::min(t_g, t_b));
        }
      }

      return t;
    }

    // the toe that maps Oklab L to the perceptual lightness L_r, and back
    float toeInv(float x) {
      constexpr float k_1 = 0.206f;
      constexpr float k_2 = 0.03f;
      constexpr float k_3 = (1.f + k_1) / (1.f + k_2);
      return (x * x + k_1 * x) / (k_3 * (x + k_2));
    }

    ST toST(LC cusp) {
      return { cusp.C / cusp.L, cusp.C / (1 - cusp.L) };
    }

    // a smooth approximation of the cusp, with S_mid < S_max and T_mid < T_max
    ST getSTMid(float a_, float b_) {
      const float S = 0.11516993f + 1.f / (
        +7.44778970f + 4.15901240f * b_
        + a_ * (-2.19557347f + 1.75198401f * b_
          + a_ * (-2.13704948f - 10.02301043f * b_
            + a_ * (-4.24894561f + 5.38770819f * b_ + 4.69891013f * a_))));

      const float T = 0.11239642f + 1.f / (
        +1.61320320f - 0.68124379f * b_
        + a_ * (+0.40370612f + 0.90148123f * b_
          + a_ * (-0.27087943f + 0.61223990f * b_
            + a_ * (+0.00299215f - 0.45399568f * b_ - 0.14661872f * a_))));

      return { S, T };
    }

    struct Cs { float C_0, C_mid, C_max; };

    Cs getCs(float L, float a_, float b_) {
      const LC cusp = findCusp(a_, b_);

      const float C_max = findGamutIntersection(a_, b_, L, 1, L, cusp);
      const ST ST_max = toST(cusp);

      // compensates for the curved part of the gamut
      const float k = C_max / std::min((L * ST_max.S), (1 - L) * ST_max.T);

      float C_mid;
      {
        const ST ST_mid = getSTMid(a_, b_);
        // a soft minimum instead of the sharp triangle, for a smooth chroma
        const float C_a = L * ST_mid.S;
        const float C_b = (1.f - L) * ST_mid.T;
        C_mid = 0.9f * k * std::sqrt(std::sqrt(1.f / (1.f / (C_a * C_a * C_a * C_a) + 1.f / (C_b * C_b * C_b * C_b))));
      }

      float C_0;
      {
        // independent of hue: roughly the average S and T
        const float C_a = L * 0.4f;
        const float C_b = (1.f - L) * 0.8f;
        C_0 = std::sqrt(1.f / (1.f / (C_a * C_a) + 1.f / (C_b * C_b)));
      }

      return { C_0, C_mid, C_max };
    }
  }

  Rgb okhslToSrgb(float hueDegrees, float s, float l) {

    if (l >= 1.0f)
      return { 1.f, 1.f, 1.f };
    if (l <= 0.f)
      return { 0.f, 0.f, 0.f };

    const float h = hueDegrees / 360.f;
    const float a_ = std::cos(2.f * kPi * h);
    const float b_ = std::sin(2.f * kPi * h);
    const float L = toeInv(l);

    const Cs cs = getCs(L, a_, b_);
    const float C_0 = cs.C_0;
    const float C_mid = cs.C_mid;
    const float C_max = cs.C_max;

    // C interpolated so that at s = 0 C = 0 with dC/ds = C_0, at s = 0.8
    // C = C_mid, and at s = 1 C = C_max
    const float mid = 0.8f;
    const float mid_inv = 1.25f;

    float C;
    if (s < mid) {
      const float t = mid_inv * s;
      const float k_1 = mid * C_0;
      const float k_2 = (1.f - k_1 / C_mid);
      C = t * k_1 / (1.f - k_2 * t);
    } else {
      const float t = (s - mid) / (1 - mid);
      const float k_0 = C_mid;
      const float k_1 = (1.f - mid) * C_mid * C_mid * mid_inv * mid_inv / C_0;
      const float k_2 = (1.f - (k_1) / (C_max - C_mid));
      C = k_0 + t * k_1 / (1.f - k_2 * t);
    }

    const Rgb rgb = oklabToLinearSrgb({ L, C * a_, C * b_ });
    auto clamp01 = [](float v) { return std::min(1.f, std::max(0.f, v)); };
    return {
      clamp01(srgbTransfer(rgb.r)),
      clamp01(srgbTransfer(rgb.g)),
      clamp01(srgbTransfer(rgb.b)),
    };
  }
}
