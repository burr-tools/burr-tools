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

void DisasmToMoves::setStep(float step) {

  for (unsigned int i = 0; i < 4 * tree->getPieceNumber(); i++)
    moves[i] = 0;

  int s = int(step);

  doRecursive(tree, s, 1-step+s, 0, 0, 0);
  doRecursive(tree, s+1, step-s, 0, 0, 0);
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

/* return the number of steps this tree requires for disassembly
 */

int DisasmToMoves::doRecursive(const separation_c * tree, int step, float weight, int cx, int cy, int cz) {
  if (!tree) return 0;

  /* we do need to include "=" here because the last move will be the separation and
   * we don't want to display that move as said in the state but rater a bit
   * more adequat for the screen
   */
  if ((step >= 0) && ((unsigned int)step >= tree->getMoves())) {

    /* we can be sure that we have disassembled the current subpuzzle,
     * so we need to display both subparts separated. it is possible that
     * one or both subparts are only one piece. In this case the tree
     * doesn't contain the subtrees
     */

    /* take the last state, in this state the removed pieces have a
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

    /* find out the direction the piece is removed
     */
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
      moves[4*tree->getPieceName(pc)+0] += weight * (dx+cx+((mabs(s->getX(pc))<10000)?(s->getX(pc)):(0)));
      moves[4*tree->getPieceName(pc)+1] += weight * (dy+cy+((mabs(s->getY(pc))<10000)?(s->getY(pc)):(0)));
      moves[4*tree->getPieceName(pc)+2] += weight * (dz+cz+((mabs(s->getZ(pc))<10000)?(s->getZ(pc)):(0)));
      moves[4*tree->getPieceName(pc)+3] += weight * 0;

      steps = 0;
    }

    /* place the left over pieces in the old center */
    if (tree->getLeft())
      steps2 = doRecursive(tree->getLeft(), step - (int)tree->getMoves() - steps, weight, cx, cy, cz);
    else {
      moves[4*tree->getPieceName(pc2)+0] += weight * (cx+s->getX(pc2));
      moves[4*tree->getPieceName(pc2)+1] += weight * (cy+s->getY(pc2));
      moves[4*tree->getPieceName(pc2)+2] += weight * (cz+s->getZ(pc2));
      moves[4*tree->getPieceName(pc2)+3] += weight * 0 * weight;

      steps2 = 0;
    }

    return tree->getMoves() + steps + steps2;
  }

  /* all right the number of steps shows us that we have the task to disassemble */
  const state_c * s = tree->getState(mmax(step, 0));

  for (unsigned int i = 0; i < tree->getPieceNumber(); i++) {
    moves[4*tree->getPieceName(i)+0] += weight * (cx+s->getX(i));
    moves[4*tree->getPieceName(i)+1] += weight * (cy+s->getY(i));
    moves[4*tree->getPieceName(i)+2] += weight * (cz+s->getZ(i));
    moves[4*tree->getPieceName(i)+3] += weight * 1;
  }

  int steps, steps2;

  if (tree->getRemoved())
    steps = doRecursive(tree->getRemoved(), step - tree->getMoves(), 0, 0, 0, 0);
  else {
    steps = 0;
  }

  /* place the left over pieces in the old center */
  if (tree->getLeft())
    steps2 = doRecursive(tree->getLeft(), step - tree->getMoves() - steps, 0, 0, 0, 0);
  else {
    steps2 = 0;
  }

  return tree->getMoves() + steps + steps2;
}


