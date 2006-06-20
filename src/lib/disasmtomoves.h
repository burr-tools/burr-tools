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
#ifndef __DISASSMTOMOVES_H__
#define __DISASSMTOMOVES_H__

class separation_c;

/* this is an abstract class used to give that piece positions to the voxel
 * space widget
 */
class piecePositions_c {

public:

  /* the current position of the piece is returned */
  virtual float getX(unsigned int piece) = 0;
  virtual float getY(unsigned int piece) = 0;
  virtual float getZ(unsigned int piece) = 0;

  /* the alpha value of the piece (0 invisible, 1 opaque) */
  virtual float getA(unsigned int piece) = 0;

  /* piece moving at this time */
  virtual bool moving(unsigned int piece) = 0;
};

/* this class takes a disassembly tree and generates relative piecepositions
 * for all pieces at each step of disassembly
 */
class disasmToMoves_c : public piecePositions_c {

  /* the disassembly tree */
  const separation_c * tree;

  /* size is used to removed pieces from the puzzle, this value controls
   * how far they are move, when they are removed
   */
  unsigned int size;

  /* this array contains the current position and alpha values of all pieces */
  float * moves;

  /* this array contains the information, if a piece is currently moving, or not */
  bool * mv;

  /* this function walks the tree and sets the piece positions */
  int doRecursive(const separation_c * tree, int step, float * array, bool center_active, int cx, int cy, int cz);

public:

  /* create class. sz is used when the pieces are removed from the
   * assembled puzzle. The larger the further away the pieces will be
   * moved
   */
  disasmToMoves_c(const separation_c * tr, unsigned int sz);

  virtual ~disasmToMoves_c();

  /* sets the moves for the step, if the value is not integer you
   * get a intermediate of the necessary move (for animation)
   * if fade out is true, pieces face out, when they are removed from the
   * rest of the puzzle
   * if center_active if true, the group of pieces that currently is
   * worked on is always in the middle of the display, other groups are invisible
   */
  void setStep(float step, bool fadeOut = true, bool center_active = false);

  /* as the functions are virtual it's no use to declare them inline,
   * speed is of no importance anyway
   */
  virtual float getX(unsigned int piece);
  virtual float getY(unsigned int piece);
  virtual float getZ(unsigned int piece);
  virtual float getA(unsigned int piece);
  virtual bool moving(unsigned int piece);
};

#endif
