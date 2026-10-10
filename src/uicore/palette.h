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
 * A copy of the legacy src/gui/piececolor.cpp maths without its FLTK
 * header, so both GUIs colour a given piece identically. The two must change
 * together; test_ui_palette pins the values.
 */
namespace btui {

  struct Rgb {
    float r, g, b;
  };

  /* the base colour of shape number `shape` (0-based) */
  Rgb pieceColor(int shape);

  /* the colour of copy `copy` (0-based) of that shape; copy 0 is close to,
   * but not exactly, the base colour, as in legacy
   */
  Rgb pieceColor(int shape, int copy);

  /* the darker and lighter shade of a channel the voxel style alternates
   * between (legacy darkPieceColor / lightPieceColor)
   */
  inline float darkShade(float f) { return f * 0.9f; }
  inline float lightShade(float f) { return 1.0f - 0.9f * (1.0f - f); }

  /* true when white text reads better than black on the shape's colour
   * (legacy contrastPieceColor)
   */
  bool prefersWhiteText(int shape);
}

#endif
