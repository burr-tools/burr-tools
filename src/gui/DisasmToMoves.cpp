#include "DisasmToMoves.h"

void DisasmToMoves::setStep(float step) {

  for (int i = 0; i < piecenumber; i++) {
    moves[4*i+0] = 0;
    moves[4*i+1] = 0;
    moves[4*i+2] = 0;
    moves[4*i+3] = 0;
  }

  int size = dis->getStart()->getX();
  if (dis->getStart()->getY() > size) size = dis->getStart()->getY();
  if (dis->getStart()->getZ() > size) size = dis->getStart()->getZ();

  size = 3 * size / 2;
  
  int s = int(step);

  doRecursive(dis->getTree(), s, 1-step+s, 0, 0, 0, size);
  doRecursive(dis->getTree(), s+1, step-s, 0, 0, 0, size);
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

int DisasmToMoves::doRecursive(const separation_c * tree, int step, float weight, int cx, int cy, int cz, int remove) {
  if (!tree) return 0;

  /* we do need to include "=" here because the last move will be the separation and
   * we don't want to display that move as said in the state but rater a bit
   * more adequat for the screen
   */
  if (step >= tree->getMoves()) {

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
    int pc, pc2;

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

    if (s->getX(pc) >  10000) dx = remove;
    if (s->getX(pc) < -10000) dx = - remove;
    if (s->getY(pc) >  10000) dy = remove;
    if (s->getY(pc) < -10000) dy = - remove;
    if (s->getZ(pc) >  10000) dz = remove;
    if (s->getZ(pc) < -10000) dz = - remove;

    int steps, steps2;

    /* place the removed pieces with the new center */
    if (tree->getRemoved())
      steps = doRecursive(tree->getRemoved(), step - tree->getMoves(), weight, cx+dx, cy+dy, cz+dz, remove/*-3*/);
    else {
      moves[4*tree->getPieceName(pc)-4] += (dx+cx+((mabs(s->getX(pc))<10000)?(s->getX(pc)):(0))) * weight;
      moves[4*tree->getPieceName(pc)-3] += (dy+cy+((mabs(s->getY(pc))<10000)?(s->getY(pc)):(0))) * weight;
      moves[4*tree->getPieceName(pc)-2] += (dz+cz+((mabs(s->getZ(pc))<10000)?(s->getZ(pc)):(0))) * weight;
      moves[4*tree->getPieceName(pc)-1] += 0 * weight;

      steps = 0;
    }

    /* place the left over pieces in the old center */
    if (tree->getLeft())
      steps2 = doRecursive(tree->getLeft(), step - tree->getMoves() - steps, weight, cx, cy, cz, remove/*-3*/);
    else {
      moves[4*tree->getPieceName(pc2)-4] += (cx+s->getX(pc2)) * weight;
      moves[4*tree->getPieceName(pc2)-3] += (cy+s->getY(pc2)) * weight;
      moves[4*tree->getPieceName(pc2)-2] += (cz+s->getZ(pc2)) * weight;
      moves[4*tree->getPieceName(pc2)-1] += 0 * weight;

      steps2 = 0;
    }

    return tree->getMoves() + steps + steps2;
  }

  /* all right the number of steps shows us that we have the task to disassemble */
  const state_c * s = tree->getState(mmax(step, 0));

  for (int i = 0; i < tree->getPieceNumber(); i++) {
    moves[4*tree->getPieceName(i)-4] += (s->getX(i)+cx) * weight;
    moves[4*tree->getPieceName(i)-3] += (s->getY(i)+cy) * weight;
    moves[4*tree->getPieceName(i)-2] += (s->getZ(i)+cz) * weight;
    moves[4*tree->getPieceName(i)-1] += 1 * weight;
  }

  int steps, steps2;

  if (tree->getRemoved())
    steps = doRecursive(tree->getRemoved(), step - tree->getMoves(), 0, 0, 0, 0, 0);
  else {
    steps = 0;
  }

  /* place the left over pieces in the old center */
  if (tree->getLeft())
    steps2 = doRecursive(tree->getLeft(), step - tree->getMoves() - steps, 0, 0, 0, 0, 0);
  else {
    steps2 = 0;
  }

  return tree->getMoves() + steps + steps2;
}


