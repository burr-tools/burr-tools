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
#include "shapecolours.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace btui {

  namespace {

    constexpr double kChroma = 0.5;              // of the table's own
    // the table's OKLCH lightness, its darkest to its lightest (S7, S5)
    constexpr double kTableDark = 0.3087, kTableLight = 0.9680;
    // the band it is spread over: 3:1 against the panel at the near end
    constexpr double kOnDarkLow = 0.56, kOnDarkHigh = 0.86;
    constexpr double kOnLightLow = 0.38, kOnLightHigh = 0.66;

    double toLinear(double c) { return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); }

    double toGamma(double c) {
      c = std::clamp(c, 0.0, 1.0);
      return c <= 0.0031308 ? c * 12.92 : 1.055 * std::pow(c, 1 / 2.4) - 0.055;
    }

    struct Lab {
      double l, a, b;
    };

    Lab toOklab(const Rgb & c) {
      const double r = toLinear(c.r), g = toLinear(c.g), b = toLinear(c.b);
      const double l = std::cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
      const double m = std::cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
      const double s = std::cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
      return { 0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
               1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
               0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s };
    }

    /* linear sRGB, which may lie outside 0 ... 1 */
    std::array<double, 3> fromOklab(const Lab & c) {
      const double l = std::pow(c.l + 0.3963377774 * c.a + 0.2158037573 * c.b, 3);
      const double m = std::pow(c.l - 0.1055613458 * c.a - 0.0638541728 * c.b, 3);
      const double s = std::pow(c.l - 0.0894841775 * c.a - 1.2914855480 * c.b, 3);
      return { 4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
               -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
               -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s };
    }
  }

  Rgb mutedColour(const Rgb & classic, bool dark) {
    const Lab c = toOklab(classic);
    const double low = dark ? kOnDarkLow : kOnLightLow, high = dark ? kOnDarkHigh : kOnLightHigh;
    const double spread = (c.l - kTableDark) / (kTableLight - kTableDark);
    const double l = low + std::clamp(spread, 0.0, 1.0) * (high - low);
    const double hue = std::atan2(c.b, c.a);

    // into sRGB by chroma alone, so the hue and the lightness stay
    double chroma = std::hypot(c.a, c.b) * kChroma;
    std::array<double, 3> rgb = fromOklab({ l, chroma * std::cos(hue), chroma * std::sin(hue) });
    const auto inGamut = [&] {
      return std::all_of(rgb.begin(), rgb.end(), [](double v) { return v >= -0.0005 && v <= 1.0005; });
    };
    while (!inGamut() && chroma > 0.001) {
      chroma *= 0.98;
      rgb = fromOklab({ l, chroma * std::cos(hue), chroma * std::sin(hue) });
    }
    return { float(toGamma(rgb[0])), float(toGamma(rgb[1])), float(toGamma(rgb[2])) };
  }

  Rgb ShapeColours::of(int shape) const {
    const Rgb c = pieceColor(shape);
    return palette == ShapePalette::Muted ? mutedColour(c, dark) : c;
  }

  Rgb ShapeColours::of(int shape, int copy) const {
    const Rgb c = pieceColor(shape, copy);
    return palette == ShapePalette::Muted ? mutedColour(c, dark) : c;
  }

  bool ShapeColours::whiteText(int shape) const {
    return prefersWhiteText(of(shape));
  }
}
