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
#include "okhsl.h"

#include <algorithm>
#include <cmath>

namespace btui {

  namespace {

    /* The base colours come from Okhsl, so every hue is drawn at a
     * lightness that looks the same: the legacy table's pure primaries put
     * blue at a quarter of yellow's lightness, and shading them by one
     * factor (the voxel style's checker, the variable voxels) moved yellow
     * a long way and blue hardly at all.
     *
     * Eight anchor hues, in hue order, with the lightness each needs to
     * sit at the same apparent brightness (a syntax-highlighting set made
     * for a light background; on the view's light grey it reads slightly
     * more saturated than on white). The lightness varies because some
     * hues can't be both this light and fully saturated. */
    struct Anchor { float hue, lightness; };
    constexpr int ANCHORS = 8;
    const Anchor lightAnchors[ANCHORS] = {
      {  25.f, 0.50f },  // red
      {  55.f, 0.55f },  // brown
      { 100.f, 0.60f },  // mustard
      { 155.f, 0.50f },  // green
      { 235.f, 0.60f },  // cyan
      { 265.f, 0.50f },  // blue
      { 305.f, 0.50f },  // purple
      { 340.f, 0.55f },  // magenta
    };

    /* the same hues for a dark background (the set's dark-mode companion):
     * lighter throughout, by more where the hue can take it */
    const Anchor darkAnchors[ANCHORS] = {
      {  25.f, 0.55f },  // red
      {  55.f, 0.65f },  // orange
      { 100.f, 0.70f },  // yellow
      { 155.f, 0.65f },  // green
      { 235.f, 0.70f },  // cyan
      { 265.f, 0.55f },  // blue
      { 305.f, 0.60f },  // purple
      { 340.f, 0.65f },  // pink
    };

    bool darkBackground = false;
    struct Okhsl { float hue, lightness; };  // saturation is always 1

    /* the first eight shapes in the order the legacy table had them: blue,
     * green, red, cyan, yellow (mustard here), magenta, then the two the
     * table had no counterpart for */
    const int firstEight[ANCHORS] = { 5, 3, 0, 4, 2, 7, 6, 1 };

    /* Shape x's place on the anchor cycle, 0 to 8: the first eight shapes
     * are the anchors; the next eight the hues halfway between neighbouring
     * anchors; the next sixteen the quarter points, and so on, each level
     * halving the gaps the level before left. Within a level the places are
     * taken with a stride, so that consecutive shapes are far apart in hue. */
    float cyclePosition(int x) {
      if (x < 0) x = 0;
      if (x < ANCHORS)
        return float(firstEight[x]);

      // level 1 has 8 places, level 2 16, level 3 32 ...
      int first = ANCHORS;
      int places = ANCHORS;
      while (x >= first + places) {
        first += places;
        places *= 2;
      }
      const int k = x - first;
      // a stride coprime with the count (a power of two, so any odd one)
      const int stride = ((3 * places) / 8) | 1;
      const int place = (k * stride) % places;
      return (float(place) + 0.5f) * float(ANCHORS) / float(places);
    }

    Okhsl baseHsl(int x) {
      const Anchor * anchors = darkBackground ? darkAnchors : lightAnchors;
      const float t = cyclePosition(x);
      const int i = int(t) % ANCHORS;
      const float f = t - float(int(t));
      const Anchor & a = anchors[i];
      const Anchor & n = anchors[(i + 1) % ANCHORS];
      const float nextHue = n.hue + (i + 1 == ANCHORS ? 360.f : 0.f);
      return { std::fmod(a.hue + f * (nextHue - a.hue), 360.f), a.lightness + f * (n.lightness - a.lightness) };
    }

    Rgb toRgb(const Okhsl & c) { return okhslToSrgb(c.hue, 1.0f, c.lightness); }

    /* 0 to 255, cut off (the FLTK GUI's colours) */
    unsigned int to8(float v) { return (unsigned int)(v * 255); }

    /* a copy of a shape: the shape's hue and lightness nudged, by more for
     * every further copy, so the copies in one assembly can be told apart
     * and still read as the same shape. Copy 0 is the shape's own colour. */
    struct Nudge { float hue, lightness; };
    constexpr int NUDGES = 12;
    const Nudge nudges[NUDGES] = {
      {   0.f,  0.12f }, {   0.f, -0.12f },
      {  12.f,  0.06f }, { -12.f, -0.06f },
      {  12.f, -0.06f }, { -12.f,  0.06f },
      {  12.f,  0.14f }, { -12.f, -0.14f },
      {   0.f,  0.22f }, {   0.f, -0.22f },
      {  20.f,  0.f   }, { -20.f,  0.f   },
    };

    Okhsl copyHsl(int shape, int copy) {
      Okhsl c = baseHsl(shape);
      if (copy > 0) {
        const Nudge & n = nudges[(copy - 1) % NUDGES];
        // past the table the hue drifts a little further each time round
        c.hue += n.hue + 6.f * float((copy - 1) / NUDGES);
        c.lightness = std::min(0.85f, std::max(0.2f, c.lightness + n.lightness));
      }
      return c;
    }
  }

  void setPieceColorsForDarkBackground(bool dark) { darkBackground = dark; }
  bool pieceColorsForDarkBackground(void) { return darkBackground; }

  Rgb pieceColor(int shape) {
    return toRgb(baseHsl(shape));
  }

  Rgb pieceColor(int shape, int copy) {
    return toRgb(copyHsl(shape, copy));
  }

  Rgb8 pieceColor8(int shape) {
    const Rgb c = pieceColor(shape);
    return { to8(c.r), to8(c.g), to8(c.b) };
  }

  Rgb8 pieceColor8(int shape, int copy) {
    const Rgb c = pieceColor(shape, copy);
    return { to8(c.r), to8(c.g), to8(c.b) };
  }

  float relativeLuminance(const Rgb & c) {
    auto lin = [](float v) { return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f); };
    return 0.2126f * lin(c.r) + 0.7152f * lin(c.g) + 0.0722f * lin(c.b);
  }

  Rgb variableTint(const Rgb & c, float toWhite) {
    const float k = toWhite;
    return { c.r + (1 - c.r) * k, c.g + (1 - c.g) * k, c.b + (1 - c.b) * k };
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
