/* Burr Solver
 * Copyright (C) 2003  Andreas Röver
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

#ifndef __DISASSEMBLY_H__
#define __DISASSEMBLY_H__

#include "voxel.h"

#include <vector>
#include <deque>

/* defines one step in the separation process,
 * - either one move you have to do or
 * - one state of relative piece positions on your way
 */
class state_c {

  int *dx, *dy, *dz;
  int piecenumber;

public:

  state_c(int pn) : piecenumber(pn) {
    dx = new int[pn];
    dy = new int[pn];
    dz = new int[pn];
    assert(dx && dy && dz);
  }

  ~state_c() {
    delete [] dx;
    delete [] dy;
    delete [] dz;
  }

  void set(int piece, int x, int y, int z) {
    assert(piece < piecenumber);
    dx[piece] = x;
    dy[piece] = y;
    dz[piece] = z;
  }

  int getPiecenumber(void) const {
    return piecenumber;
  }

  int getX(int i) const {
    assert(i < piecenumber);
    return dx[i];
  }
  int getY(int i) const {
    assert(i < piecenumber);
    return dy[i];
  }
  int getZ(int i) const {
    assert(i < piecenumber);
    return dz[i];
  }

  bool pieceRemoved(int i) const {
    return (dx[i] > 10000) || (dx[i] < -10000) ||
      (dy[i] > 10000) || (dy[i] < -10000) ||
      (dz[i] > 10000) || (dz[i] < -10000);
  }

  void print(voxel_c * start, voxel_type * pieces) const;

};

/* a list of states that lead to a separation of the
 * puzzle into 2 parts
 */
class separation_c {

  /* number of moves necessary to separate the current puzzle
   * into 2 parts
   */
  int moves;

  /* number of pieces that are in this subsection of the puzzle
   */
  int piecenumber;

  /* this array is here to identify the piecenumber for the given pieces
   * the state is for
   */
  voxel_type * pieces;

  /* vector with all the states that finally lead to 2 separate
   * subpuzzles one state represents the  movement of
   * all the pieces inside this subpuzzle relative to it's
   * position in the completely assembled puzzle
   *
   * the last state will move all the pieces, that can be removed
   * a long way out of the puzzle (more than 10000 units)
   *
   * the first state is the beginning state, for this partition, e.g
   * for the root node the first state represents the assembles puzzle
   * with all values 0
   */
  std::deque <state_c *> movements;

  /* the 2 parts the puzzle gets divided with the
   * last move. if one of this parts consists of only
   * one piece there will be a null pointer
   */
  separation_c * removed, *left;


public:

  separation_c(separation_c * r, separation_c * l, int pn, voxel_type * pcs) : moves(0), piecenumber(pn), removed(r), left(l) {
    pieces = new voxel_type[pn];

    for (int i = 0; i < pn; i++)
      pieces[i] = pcs[i];
  }

  ~separation_c() {
    delete removed;
    delete left;
    for (int i = 0; i < moves; i++)
      delete movements[i];
    delete [] pieces;
  }

  int getMoves(void) const {
    return moves - 1;
  }

  int sumMoves(void) const {
    int erg = moves - 1;
    if (removed)
      erg += removed->sumMoves();
    if (left)
      erg += left->sumMoves();

    return erg;
  }

  const state_c * getState(int num) const {
    assert(num < moves);
    return movements[num];
  }

  const separation_c * getLeft(void) const { return left; }
  const separation_c * getRemoved(void) const { return removed; }

  void addstate(state_c *st) {
    assert(st->getPiecenumber() == piecenumber);
    moves++;
    movements.push_front(st);
  }

  int getPieceNumber(void) const { return piecenumber; }

  voxel_type getPieceName(int num) const {
    assert(num < piecenumber);
    return pieces[num];
  }

  void print(voxel_c * start) const;

};

/* defines an assembly from the assembled position
 * and all the steps necessary to completely disassemble
 */
class disassembly_c {

private:

  /* the disassembly tree */
  separation_c * tree;

  /* this is the assembled puzzle, needed for printing
   * the states graphically
   */
  voxel_c * start;

public:

  disassembly_c(voxel_c * st, separation_c *t) {
    tree = t;
    start = new voxel_c(st);
  }

  ~disassembly_c() {
    delete tree;
    delete start;
  }

  /* number of moves bevore the puzzle get's
   * separated
   */
  int firstlevel(void) const { return tree->getMoves(); }
  int sumlevel(void) const { return tree->sumMoves(); }

  void print(void) const;

  const voxel_c * getStart(void) const { return start; }

  const separation_c * getTree(void) const { return tree; }
};

#endif
