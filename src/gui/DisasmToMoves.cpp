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


#include "DisasmToMoves.h"

DisasmToMoves::DisasmToMoves(const separation_c * tr, unsigned int sz) : tree(tr), size(sz) {
  moves = new float[tr->getPieceNumber()*4];
}

DisasmToMoves::~DisasmToMoves() {
  delete [] moves;
}

void DisasmToMoves::setStep(float step) {

  int s = int(step);
  float frac = step - s;

  /* what we do is go twice through the tree and linearly interpolate between
   * the 2 states that we have in in the two nodes that we are currently in between
   *
   * this is done with the weight value (1-frac and frac)
   */
  for (unsigned int i = 0; i < 4 * tree->getPieceNumber(); i++)  moves[i] = 0;

  if (tree) {
    if (frac != 1) doRecursive(tree, s  , 1-frac, 0, 0, 0);
    if (frac != 0) doRecursive(tree, s+1,   frac, 0, 0, 0);
  }
}

float DisasmToMoves::getX(unsigned int piece) {
  bt_assert(piece < tree->getPieceNumber());
  return moves[4*piece+0];
}
float DisasmToMoves::getY(unsigned int piece) {
  bt_assert(piece < tree->getPieceNumber());
  return moves[4*piece+1];
}
float DisasmToMoves::getZ(unsigned int piece) {
  bt_assert(piece < tree->getPieceNumber());
  return moves[4*piece+2];
}
float DisasmToMoves::getA(unsigned int piece) {
  bt_assert(piece < tree->getPieceNumber());
  return moves[4*piece+3];
}

static int mabs(int a) {
  if (a > 0)
    return a;
  else
    return -a;
}

static int mmax(int a, int b) {
  if (a>b)
    return a;
  else
    return b;
}

/* this is the core function that walks through the tree, let's see if I can
 * describe what's going on in here
 *
 * let's start with the parameters:
 *    tree is the current subtree to walk through
 *    step is the step to show inside this tree, if step is negative or bigger than
 *                the steps required for this tree we are somewhere outside the tree
 *    weight is used for the linear interpolation it is a value between 0 and 1 including
 *                values are multiplied by this value and then the 2 end points are added
 *    cx, cy, cz are the center to display the current tree
 */
int DisasmToMoves::doRecursive(const separation_c * tree, int step, float weight, int cx, int cy, int cz) {

  bt_assert(tree);

  /* first check, if we are inside this tree node, this is the case when
   * the number of steps is between 0 and the number of steps in this node
   *
   * we do need to include "=" here because the last move will be the separation and
   * we don't want to display that move as said in the state but rater a bit
   * more adequat for the screen
   *
   * in the state the removed part would be removed by 10000 units
   */
  if ((step >= 0) && ((unsigned int)step >= tree->getMoves())) {

    /* so, this is the path for after the current node, the first thing
     * is to find out in which directions the pieces that are removed
     * are removed, then define the new center for the removed
     * part and call the subtrees
     *
     * we can be sure that we have disassembled the current subpuzzle,
     * so we need to display both subparts separated. it is possible that
     * one or both subparts are only one piece. In this case the tree
     * doesn't contain the subtrees
     *
     * take the last state, in this state the removed pieces have a
     * distance grater 1000
     */
    const state_c * s = tree->getState(tree->getMoves());

    /* find one of the removed pieces and one of the left pieces */
    unsigned int pc, pc2;

    for (pc = 0; pc < tree->getPieceNumber(); pc++)
      if (s->pieceRemoved(pc))
        break;

    for (pc2 = 0; pc2 < tree->getPieceNumber(); pc2++)
      if (!s->pieceRemoved(pc2))
        break;

    /* find out the direction the piece is removed */
    int dx, dy, dz;

    dx = dy = dz = 0;

    if (s->getX(pc) >  10000) dx = size;
    if (s->getX(pc) < -10000) dx = - size;
    if (s->getY(pc) >  10000) dy = size;
    if (s->getY(pc) < -10000) dy = - size;
    if (s->getZ(pc) >  10000) dz = size;
    if (s->getZ(pc) < -10000) dz = - size;

    int steps, steps2;

    /* place the removed pieces with the new center */
    if (tree->getRemoved())
      steps = doRecursive(tree->getRemoved(), step - (int)tree->getMoves(), weight, cx+dx, cy+dy, cz+dz);
    else {
      for (unsigned int p = 0; p < tree->getPieceNumber(); p++)
        if (s->pieceRemoved(p)) {
          moves[4*tree->getPieceName(p)+0] += weight * (dx+cx+((mabs(s->getX(p))<10000)?(s->getX(p)):(0)));
          moves[4*tree->getPieceName(p)+1] += weight * (dy+cy+((mabs(s->getY(p))<10000)?(s->getY(p)):(0)));
          moves[4*tree->getPieceName(p)+2] += weight * (dz+cz+((mabs(s->getZ(p))<10000)?(s->getZ(p)):(0)));
          moves[4*tree->getPieceName(p)+3] += weight * 0;
        }

      steps = 0;
    }

    /* place the left over pieces in the old center */
    if (tree->getLeft())
      steps2 = doRecursive(tree->getLeft(), step - (int)tree->getMoves() - steps, weight, cx, cy, cz);
    else {

      for (unsigned int p = 0; p < tree->getPieceNumber(); p++)
        if (!s->pieceRemoved(p)) {
          moves[4*tree->getPieceName(p)+0] += weight * (cx+s->getX(p));
          moves[4*tree->getPieceName(p)+1] += weight * (cy+s->getY(p));
          moves[4*tree->getPieceName(p)+2] += weight * (cz+s->getZ(p));
          moves[4*tree->getPieceName(p)+3] += weight * 0 * weight;
        }

      steps2 = 0;
    }

    return tree->getMoves() + steps + steps2;
  }

  /* all right the number of steps shows us that we have the task to disassemble
   * this node, so get the state and place the pieces ad the right position
   *
   * we also have to place the pieces at their initial position, when we are
   * bevore the current node
   */
  const state_c * s = tree->getState(mmax(step, 0));

  for (unsigned int i = 0; i < tree->getPieceNumber(); i++) {
    moves[4*tree->getPieceName(i)+0] += weight * (cx+s->getX(i));
    moves[4*tree->getPieceName(i)+1] += weight * (cy+s->getY(i));
    moves[4*tree->getPieceName(i)+2] += weight * (cz+s->getZ(i));
    moves[4*tree->getPieceName(i)+3] += weight * 1;
  }

  int steps  = tree->getRemoved() ? doRecursive(tree->getRemoved(), step - tree->getMoves()        , 0, 0, 0, 0) : 0;
  int steps2 = tree->getLeft()    ? doRecursive(tree->getLeft()   , step - tree->getMoves() - steps, 0, 0, 0, 0) : 0;

  return tree->getMoves() + steps + steps2;
}



