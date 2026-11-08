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
#include "piececolor.h"

#include "../uicore/palette.h"

/* The colours come from src/uicore/palette, the one table both GUIs use,
 * computed exactly as this file did before; these are its legacy names.
 */

float pieceColorR(int x) { return btui::pieceColor(x).r; }
float pieceColorG(int x) { return btui::pieceColor(x).g; }
float pieceColorB(int x) { return btui::pieceColor(x).b; }

unsigned int pieceColorRi(int x) { return btui::pieceColor8(x).r; }
unsigned int pieceColorGi(int x) { return btui::pieceColor8(x).g; }
unsigned int pieceColorBi(int x) { return btui::pieceColor8(x).b; }

float pieceColorR(int x, int sub) { return btui::pieceColor(x, sub).r; }
float pieceColorG(int x, int sub) { return btui::pieceColor(x, sub).g; }
float pieceColorB(int x, int sub) { return btui::pieceColor(x, sub).b; }

unsigned int pieceColorRi(int x, int sub) { return btui::pieceColor8(x, sub).r; }
unsigned int pieceColorGi(int x, int sub) { return btui::pieceColor8(x, sub).g; }
unsigned int pieceColorBi(int x, int sub) { return btui::pieceColor8(x, sub).b; }

float darkPieceColor(float f) { return btui::darkShade(f); }
float lightPieceColor(float f) { return btui::lightShade(f); }

Fl_Color contrastPieceColor(int x) {
  if (3*pieceColorRi(x) + 6*pieceColorGi(x) + pieceColorBi(x) < 1275)
    return fl_rgb_color(255, 255, 255);
  else
    return fl_rgb_color(0, 0, 0);
}
