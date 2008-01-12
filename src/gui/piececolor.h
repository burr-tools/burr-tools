/* Burr Solver
 * Copyright (C) 2003-2008  Andreas Röver
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
#ifndef __PIECECOLOR_H__
#define __PIECECOLOR_H__

#include <FL/fl_draw.H>

/* this module provides the colours for the pieces. The first few colours are defined within
 * a table, the following colours use a function that hopefully delivers ever changing
 * nice colour values
 */

/* the following 3 functions return the base colour of piece x */
float pieceColorR(int x);
float pieceColorG(int x);
float pieceColorB(int x);

unsigned int pieceColorRi(int x);
unsigned int pieceColorGi(int x);
unsigned int pieceColorBi(int x);

/* if there is more than one instance of the same shape, you can get
 * the colour with this function. It slightly changes the colour
 * from piece to piece so that the pieces all have a similar but distinguishable
 * colour
 */
float pieceColorR(int x, int sub);
float pieceColorG(int x, int sub);
float pieceColorB(int x, int sub);

unsigned int pieceColorRi(int x, int sub);
unsigned int pieceColorGi(int x, int sub);
unsigned int pieceColorBi(int x, int sub);

/* the pieces are drawn with chequered colours, these 2 functions can be used to
 * achieve that effect
 */
float darkPieceColor(float f);
float lightPieceColor(float f);

/* two macros to directly get the fltk color */
#define fltkSubPieceColor(x,y) fl_rgb_color(pieceColorRi(x, y), pieceColorGi(x, y), pieceColorBi(x, y))
#define fltkPieceColor(x) fl_rgb_color(pieceColorRi(x), pieceColorGi(x), pieceColorBi(x))

/* a function that returns an fltk color (either black or white) with a big contrast
 * to the piece color
 */
Fl_Color contrastPieceColor(int x);

#endif
