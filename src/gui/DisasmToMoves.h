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

/* this class takes a disassembly tree and generates relativ piecepositions
 * for all peaces at each step of disassembly
 */
#include <../lib/disassembly.h>

/* this is an abstract class used to give thet piece positions to the voxel
 * space widget
 */
class PiecePositions {

public:

  virtual float getX(unsigned int piece) = 0;
  virtual float getY(unsigned int piece) = 0;
  virtual float getZ(unsigned int piece) = 0;
  virtual float getA(unsigned int piece) = 0;
};

 
class DisasmToMoves : public PiecePositions {

  const separation_c * tree;
  unsigned int size;
  float * moves;
  int doRecursive(const separation_c * tree, int step, float weight, int cx, int cy, int cz);

public:

  DisasmToMoves(const separation_c * tr, unsigned int sz) : tree(tr), size(sz) {
    moves = new float[tr->getPieceNumber()*4];
  }

  virtual ~DisasmToMoves() { delete [] moves; }
  
  /* sets the moves for the step, if the value is not integer you
   * get a intermediate of the necessary move (for animation)
   */
  void setStep(float step);

  virtual float getX(unsigned int piece) {
    assert(piece < tree->getPieceNumber());
    return moves[4*piece+0];
  }
  virtual float getY(unsigned int piece) {
    assert(piece < tree->getPieceNumber());
    return moves[4*piece+1];
  }
  virtual float getZ(unsigned int piece) {
    assert(piece < tree->getPieceNumber());
    return moves[4*piece+2];
  }
  virtual float getA(unsigned int piece) {
    assert(piece < tree->getPieceNumber());
    return moves[4*piece+3];
  }
};

#endif
