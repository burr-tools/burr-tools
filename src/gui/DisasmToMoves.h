/* Burr Solver
 * Copyright (C) 2003-2005  Andreas Röver
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


#ifndef __DISASSMTOMOVES_H__
#define __DISASSMTOMOVES_H__

#include <../lib/disassembly.h>

/* this is an abstract class used to give thet piece positions to the voxel
 * space widget
 */
class PiecePositions {

public:

  /* the current position of the piece is returned */
  virtual float getX(unsigned int piece) = 0;
  virtual float getY(unsigned int piece) = 0;
  virtual float getZ(unsigned int piece) = 0;

  /* the alpha value of the piece (0 invisible, 1 opaque) */
  virtual float getA(unsigned int piece) = 0;
};

/* this class takes a disassembly tree and generates relative piecepositions
 * for all pieces at each step of disassembly
 */
class DisasmToMoves : public PiecePositions {

  /* the disassembly tree */
  const separation_c * tree;

  /* size is used to removed pieces from the puzzle, this value controls
   * how far they are move, when they are removed
   */
  unsigned int size;

  /* this array contains the current position and alpha values of all pieces */
  float * moves;

  /* this function walks the tree and sets the piece positions */
  int doRecursive(const separation_c * tree, int step, float weight, int cx, int cy, int cz);

public:

  DisasmToMoves(const separation_c * tr, unsigned int sz);

  virtual ~DisasmToMoves();
  
  /* sets the moves for the step, if the value is not integer you
   * get a intermediate of the necessary move (for animation)
   */
  void setStep(float step);

  /* as the functions are virtual it's no use to declare them inline,
   * speed is of no omportance anyway
   */
  virtual float getX(unsigned int piece);
  virtual float getY(unsigned int piece);
  virtual float getZ(unsigned int piece);
  virtual float getA(unsigned int piece);
};

#endif
