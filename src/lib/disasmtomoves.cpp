/* Burr Solver
 * Copyright (C) 2003-2007  Andreas Röver
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
#include "disasmtomoves.h"

#include "../lib/disassembly.h"

disasmToMoves_c::disasmToMoves_c(const separation_c * tr, unsigned int sz, unsigned int max) : tree(tr), size(sz), maxPieceName(max) {

  moves = new float[maxPieceName*4];
  mv = new bool[maxPieceName];
}

disasmToMoves_c::~disasmToMoves_c() {
  delete [] moves;
  delete [] mv;
}

void disasmToMoves_c::setStep(float step, bool fadeOut, bool center_active) {

  int s = int(step);
  float frac = step - s;

  // a temporary array, used to save the 2nd placement for the interpolation */
  float * moves2 = new float[maxPieceName*4];

  for (unsigned int i = 0; i < 4 * maxPieceName; i++) {
    moves[i] = moves2[i] = 0;
  }

  /* what we do is go twice through the tree and linearly interpolate between
   * the 2 states that we have in in the two nodes that we are currently in between
   *
   * this is done with the weight value (1-frac and frac)
   */
  if (tree) {

    /* get the 2 possible positions between we have to interpolate */
    doRecursive(tree, s  , moves, center_active, 0, 0, 0);
    doRecursive(tree, s+1, moves2, center_active, 0, 0, 0);

    // interpolate and check, which piece moves right now
    for (unsigned int i = 0; i < maxPieceName; i++) {
      mv[i] = ((moves[4*i+0] != moves2[4*i+0]) || (moves[4*i+1] != moves2[4*i+1]) || (moves[4*i+2] != moves2[4*i+2]));
      moves[4*i+0] = (1-frac)*moves[4*i+0] + frac*moves2[4*i+0];
      moves[4*i+1] = (1-frac)*moves[4*i+1] + frac*moves2[4*i+1];
      moves[4*i+2] = (1-frac)*moves[4*i+2] + frac*moves2[4*i+2];
      moves[4*i+3] = (1-frac)*moves[4*i+3] + frac*moves2[4*i+3];
    }

    if (!fadeOut)
      for (unsigned int i = 0; i < maxPieceName; i++)
        if (moves[4*i+3] > 0) moves[4*i+3] = 1;

  }

  delete [] moves2;
}

float disasmToMoves_c::getX(unsigned int piece) {
  bt_assert(piece < maxPieceName);
  return moves[4*piece+0];
}
float disasmToMoves_c::getY(unsigned int piece) {
  bt_assert(piece < maxPieceName);
  return moves[4*piece+1];
}
float disasmToMoves_c::getZ(unsigned int piece) {
  bt_assert(piece < maxPieceName);
  return moves[4*piece+2];
}
float disasmToMoves_c::getA(unsigned int piece) {
  bt_assert(piece < maxPieceName);
  return moves[4*piece+3];
}
bool disasmToMoves_c::moving(unsigned int piece) {
  bt_assert(piece < maxPieceName);
  return mv[piece];
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
 *    cx, cy, cz are the centre to display the current tree
 */
int disasmToMoves_c::doRecursive(const separation_c * tree, int step, float * array, bool center_active, int cx, int cy, int cz) {

  bt_assert(tree);

  /* first check, if we are inside this tree node, this is the case when
   * the number of steps is between 0 and the number of steps in this node
   *
   * we do need to include "=" here because the last move will be the separation and
   * we don't want to display that move as said in the state but rather a bit
   * more adequate for the screen
   *
   * in the state the removed part would be removed by 10000 units
   */
  if ((step >= 0) && ((unsigned int)step >= tree->getMoves())) {

    /* so, this is the path for after the current node, the first thing
     * is to find out in which directions the pieces that are removed
     * are removed, then define the new centre for the removed
     * part and call the subtrees
     *
     * we can be sure that we have disassembled the current subpuzzle,
     * so we need to display both subparts separated. It is possible that
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

    /* place the removed pieces with the new centre */
    if (tree->getRemoved()) {

      /* if we use the center_active option we need to keep the removed part stationary
       * and place the pieces where they belong,
       *
       * otherwise we place the removed part somewhere out of the way
       */
      if (center_active)
        steps = doRecursive(tree->getRemoved(), step - (int)tree->getMoves(), array, center_active,
            tree->getState(tree->getMoves()-1)->getX(pc) + cx - tree->getRemoved()->getState(0)->getX(0),
            tree->getState(tree->getMoves()-1)->getY(pc) + cy - tree->getRemoved()->getState(0)->getY(0),
            tree->getState(tree->getMoves()-1)->getZ(pc) + cz - tree->getRemoved()->getState(0)->getZ(0));
      else
        steps = doRecursive(tree->getRemoved(), step - (int)tree->getMoves(), array, center_active, cx+dx, cy+dy, cz+dz);

    } else {

      /* if there is no removed tree, the pieces need to vanish */
      if (array)
        for (unsigned int p = 0; p < tree->getPieceNumber(); p++)
          if (s->pieceRemoved(p)) {
            array[4*tree->getPieceName(p)+0] += dx+cx+((mabs(s->getX(p))<10000)?(s->getX(p)):(0));
            array[4*tree->getPieceName(p)+1] += dy+cy+((mabs(s->getY(p))<10000)?(s->getY(p)):(0));
            array[4*tree->getPieceName(p)+2] += dz+cz+((mabs(s->getZ(p))<10000)?(s->getZ(p)):(0));
            array[4*tree->getPieceName(p)+3] += 0;
          }

      steps = 0;
    }

    /* place the left over pieces in the old centre */
    if (tree->getLeft()) {

      /* if we use center_active switch, we first display the removed part and need to move the left
       * over part out of the way, this is done via the d. values in the negative direction of the
       * removal of the pieces
       *
       * if we don't use the center_active option, the left over part stays in the middle
       */
      if (center_active && (step - (int)tree->getMoves() < steps) && (tree->getRemoved()))
        steps2 = doRecursive(tree->getLeft(), step - (int)tree->getMoves() - steps, array, center_active, cx-dx, cy-dy, cz-dz);
      else
        steps2 = doRecursive(tree->getLeft(), step - (int)tree->getMoves() - steps, array, center_active, cx, cy, cz);

      /* if the steps tell us that we are currenlty animating the removed part
       * and there actually _is_ a removed animation, we hide all
       * pieces that are not removed
       */
      if (array && center_active && (step - (int)tree->getMoves() < steps) && (tree->getRemoved())) {
        for (unsigned int p = 0; p < tree->getPieceNumber(); p++)
          if (!s->pieceRemoved(p)) {
            array[4*tree->getPieceName(p)+3] = 0;
          }
      }

    } else {

      if (array)
        for (unsigned int p = 0; p < tree->getPieceNumber(); p++)
          if (!s->pieceRemoved(p)) {
            array[4*tree->getPieceName(p)+0] += cx+s->getX(p);
            array[4*tree->getPieceName(p)+1] += cy+s->getY(p);
            array[4*tree->getPieceName(p)+2] += cz+s->getZ(p);
            array[4*tree->getPieceName(p)+3] += 0;
          }

      steps2 = 0;
    }

    return tree->getMoves() + steps + steps2;
  }

  /* all right the number of steps shows us that we have the task to disassemble
   * this node, so get the state and place the pieces at the right position
   *
   * we also have to place the pieces at their initial position, when we are
   * before the current node
   */
  const state_c * s = tree->getState(mmax(step, 0));

  if (array)
    for (unsigned int i = 0; i < tree->getPieceNumber(); i++) {
      array[4*tree->getPieceName(i)+0] += cx+s->getX(i);
      array[4*tree->getPieceName(i)+1] += cy+s->getY(i);
      array[4*tree->getPieceName(i)+2] += cz+s->getZ(i);
      array[4*tree->getPieceName(i)+3] += 1;
    }

  int steps  = tree->getRemoved() ? doRecursive(tree->getRemoved(), step - tree->getMoves()        , 0, center_active, 0, 0, 0) : 0;
  int steps2 = tree->getLeft()    ? doRecursive(tree->getLeft()   , step - tree->getMoves() - steps, 0, center_active, 0, 0, 0) : 0;

  return tree->getMoves() + steps + steps2;
}
