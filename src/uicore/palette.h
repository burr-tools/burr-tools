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
#ifndef BTUI_PALETTE_H
#define BTUI_PALETTE_H

/* The piece colours: S1 blue, S2 green, S3 red, S4 cyan, ... -- a fixed
 * table for the first 18 shapes, then a sine-based sequence -- and the
 * jittered variants that tell several copies of one shape apart.
 *
 * The one table for both GUIs: the legacy src/gui/piececolor.cpp is a thin
 * wrapper over these functions (legacy compiles palette.cpp, as it does
 * puzzlehistory), and the maths is legacy's to the bit. test_ui_palette
 * pins the values.
 */
namespace btui {

  struct Rgb {
    float r, g, b;
  };

  /* The piece colours come in a set for a light background (the default,
   * and all the FLTK GUI has) and one for a dark background: the same hues,
   * lighter. This switches every colour below for the whole program; a GUI
   * that switches it has to redraw what shows piece colours. */
  void setPieceColorsForDarkBackground(bool dark);
  bool pieceColorsForDarkBackground(void);

  /* the base colour of shape number `shape` (0-based) */
  Rgb pieceColor(int shape);

  /* the colour of copy `copy` (0-based) of that shape; copy 0 is close to,
   * but not exactly, the base colour, as in legacy
   */
  Rgb pieceColor(int shape, int copy);

  /* the same colours as 0 to 255 per channel, cut off rather than rounded
   * (legacy pieceColorRi and friends, the FLTK GUI's colours)
   */
  struct Rgb8 {
    unsigned int r, g, b;
  };
  Rgb8 pieceColor8(int shape);
  Rgb8 pieceColor8(int shape, int copy);

  /* the darker and lighter shade of a channel the voxel style alternates
   * between (legacy darkPieceColor / lightPieceColor)
   */
  inline float darkShade(float f) { return float(f * 0.9); }
  inline float lightShade(float f) { return float(1 - 0.9 * (1 - f)); }

  /* The look of a variable voxel, in both GUIs: the same hue as the fixed
   * voxels next to it, a good deal lighter (variableTint), and in the 3D
   * views see-through at kVariableAlpha, drawn as one layer with the
   * nearest-surface rule: variable voxels hide each other, show the fixed
   * voxels and the background through themselves, and are hidden by the
   * fixed voxels in front of them. "Less there" than a fixed voxel, while
   * still a region of the piece's colour. */
  Rgb variableTint(const Rgb & c);
  inline constexpr float kVariableAlpha = 0.5f;

  /* the 2D editor's chequerboard: a milder pair than the voxel style's,
   * so that the variable cells' lighter tint stands out from it */
  inline float editorDarkShade(float f) { return float(f * 0.95); }
  inline float editorLightShade(float f) { return float(1 - 0.95 * (1 - f)); }

  /* WCAG 2 relative luminance of an sRGB colour, and the contrast ratio of
   * two (1 to 21) */
  float relativeLuminance(const Rgb & c);
  float contrastRatio(const Rgb & a, const Rgb & b);

  /* Whether text on this colour -- the shape's -- is white rather than
   * black: whichever contrasts more, by WCAG's measure, so the better of
   * the two (always at least 4.5:1) is the one drawn. Legacy's weighted
   * channel sum (contrastPieceColor) chose white on red, magenta and some
   * greens at 3 to 4:1. One rule for every chip, list and the 3D view's
   * tag. */
  bool prefersWhiteText(const Rgb & background);
  bool prefersWhiteText(int shape);
}

#endif
