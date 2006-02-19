/* Burr Solver
 * Copyright (C) 2003-2006  Andreas Röver
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

/* this module provides the colors for the pieces. The first few colors are defined within
 * a table, the following colors use a function that hopefully delivers ever changeing
 * nice color values
 */

/* the following 3 functions return the base color of piece x */
float pieceColorR(int x);
float pieceColorG(int x);
float pieceColorB(int x);

/* if there is more than one instance of the same shape, you can get
 * the color with this function. It slightly changes the color
 * from piece to piece so that the pieces all have a similar but distinuishable
 * color
 */
float pieceColorR(int x, int sub);
float pieceColorG(int x, int sub);
float pieceColorB(int x, int sub);

/* the pieces are drawn with checkerd colors, these 2 functions can be used to
 * achieve that effect
 */
float darkPieceColor(float f);
float lightPieceColor(float f);

#endif
