/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
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

#include "disassembly.h"
#include "disassemblernode.h"

disasmToMoves_c::disasmToMoves_c(const separation_c * tr, unsigned int sz, unsigned int max)
  : tree(tr ? std::make_unique<separation_c>(tr) : nullptr),
    size(sz),
    moves(max * 4, 0.0f),
    orients(max, 0),
    rotAngle(max, 0.0f),
    rotAxisX(max, 0.0f),
    rotAxisY(max, 0.0f),
    rotAxisZ(max, 0.0f),
    rotPivotX(max, 0.0f),
    rotPivotY(max, 0.0f),
    rotPivotZ(max, 0.0f),
    mv(max, false),
    maxPieceName(max) {
}

disasmToMoves_c::~disasmToMoves_c() = default;

void disasmToMoves_c::setStep(float step, bool fadeOut, bool center_active) {

  int s = int(step);
  float frac = step - s;

  // a temporary array, used to save the 2nd placement for the interpolation */
  std::vector<float> moves2(maxPieceName * 4, 0.0f);
  std::vector<unsigned int> orients2(maxPieceName, 0);

  std::fill(moves.begin(), moves.end(), 0.0f);
  std::fill(orients.begin(), orients.end(), 0);
  std::fill(rotAngle.begin(), rotAngle.end(), 0.0f);
  std::fill(rotAxisX.begin(), rotAxisX.end(), 0.0f);
  std::fill(rotAxisY.begin(), rotAxisY.end(), 0.0f);
  std::fill(rotAxisZ.begin(), rotAxisZ.end(), 0.0f);
  std::fill(rotPivotX.begin(), rotPivotX.end(), 0.0f);
  std::fill(rotPivotY.begin(), rotPivotY.end(), 0.0f);
  std::fill(rotPivotZ.begin(), rotPivotZ.end(), 0.0f);

  if (tree) {

    doRecursive(tree.get(), s  , moves.data(), orients.data(), center_active, 0, 0, 0);
    doRecursive(tree.get(), s+1, moves2.data(), orients2.data(), center_active, 0, 0, 0);

    /* Look up rotation metadata on the destination state inside the active node.
     * doRecursive only fills positions; fetch rotation arrival from the tree state
     * that corresponds to step s+1 when we are inside a single separation node.
     * Simpler approach: if orientation changes, find the state via a helper walk.
     */
    for (unsigned int i = 0; i < maxPieceName; i++) {
      bool rotating = (orients[i] != orients2[i]);
      mv[i] = ((moves[4*i+0] != moves2[4*i+0]) || (moves[4*i+1] != moves2[4*i+1]) || (moves[4*i+2] != moves2[4*i+2]) || rotating);

      if (rotating) {
        /* Keep start hotspot + start mesh; drive a continuous OpenGL tumble
         * about the recorded pivot. At frac==1 the next integer step will
         * load the end state fully. */
        float angle = frac * 90.0f;
        unsigned int axis = 0, sense = 0;
        int pvx = 0, pvy = 0, pvz = 0;
        bool havePivot = findRotationArrival(s + 1, i, &pvx, &pvy, &pvz, &axis, &sense);

        if (havePivot) {
          if (sense != 0) angle = -angle;
          rotAngle[i] = angle;
          rotAxisX[i] = (axis == 0) ? 1.0f : 0.0f;
          rotAxisY[i] = (axis == 1) ? 1.0f : 0.0f;
          rotAxisZ[i] = (axis == 2) ? 1.0f : 0.0f;
          rotPivotX[i] = (float)pvx * 0.5f + 0.5f;
          rotPivotY[i] = (float)pvy * 0.5f + 0.5f;
          rotPivotZ[i] = (float)pvz * 0.5f + 0.5f;
        } else if (frac >= 0.5f) {
          /* Fallback without pivot metadata: snap */
          moves[4*i+0] = moves2[4*i+0];
          moves[4*i+1] = moves2[4*i+1];
          moves[4*i+2] = moves2[4*i+2];
          moves[4*i+3] = moves2[4*i+3];
          orients[i] = orients2[i];
        }
        /* else keep start moves/orients; rotAngle already set when havePivot */
      } else {
        moves[4*i+0] = (1-frac)*moves[4*i+0] + frac*moves2[4*i+0];
        moves[4*i+1] = (1-frac)*moves[4*i+1] + frac*moves2[4*i+1];
        moves[4*i+2] = (1-frac)*moves[4*i+2] + frac*moves2[4*i+2];
        moves[4*i+3] = (1-frac)*moves[4*i+3] + frac*moves2[4*i+3];
      }
    }

    if (!fadeOut)
      for (unsigned int i = 0; i < maxPieceName; i++)
        if (moves[4*i+3] > 0) moves[4*i+3] = 1;

  }
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
unsigned int disasmToMoves_c::getTrans(unsigned int piece) {
  bt_assert(piece < maxPieceName);
  return orients[piece];
}

bool disasmToMoves_c::getRotationAnim(unsigned int piece,
                                      float * angleDeg,
                                      float * axisX, float * axisY, float * axisZ,
                                      float * pivotX, float * pivotY, float * pivotZ) {
  bt_assert(piece < maxPieceName);
  if (rotAngle[piece] == 0)
    return false;
  *angleDeg = rotAngle[piece];
  *axisX = rotAxisX[piece];
  *axisY = rotAxisY[piece];
  *axisZ = rotAxisZ[piece];
  *pivotX = rotPivotX[piece];
  *pivotY = rotPivotY[piece];
  *pivotZ = rotPivotZ[piece];
  return true;
}

bool disasmToMoves_c::findRotationArrival(int step, unsigned int pieceName,
                                          int * pvx, int * pvy, int * pvz,
                                          unsigned int * axis, unsigned int * sense) const {
  if (!tree || step < 0)
    return false;
  return findRotationArrivalRec(tree.get(), step, pieceName, pvx, pvy, pvz, axis, sense);
}

bool disasmToMoves_c::findRotationArrivalRec(const separation_c * t, int step, unsigned int pieceName,
                                             int * pvx, int * pvy, int * pvz,
                                             unsigned int * axis, unsigned int * sense) const {
  /* States of this node occupy global steps [0, getMoves()] relative to this subtree root */
  if (step >= 0 && (unsigned int)step <= t->getMoves()) {
    const state_c * st = t->getState((unsigned int)step);
    if (st->isRotationArrival()) {
      /* Compound rotations store one primary rotPiece but share pivot/axis/sense.
       * Any piece in this separation that is actually turning (caller already
       * checked orientation change) uses the same tumble. */
      for (unsigned int k = 0; k < t->getPieceNumber(); k++) {
        if (t->getPieceName(k) != pieceName)
          continue;
        *pvx = st->getRotPivotX();
        *pvy = st->getRotPivotY();
        *pvz = st->getRotPivotZ();
        *axis = st->getRotAxis();
        *sense = st->getRotSense();
        return true;
      }
    }
    return false;
  }

  if (step < (int)t->getMoves())
    return false;

  int sub = step - (int)t->getMoves();

  /* Subtree lengths match doRecursive: removed first, then left */
  int removedLen = 0;
  if (t->getRemoved()) {
    if (findRotationArrivalRec(t->getRemoved(), sub, pieceName, pvx, pvy, pvz, axis, sense))
      return true;
    /* need length of removed subtree in steps — sumSteps of that separation */
    removedLen = (int)t->getRemoved()->sumSteps();
  }

  if (t->getLeft())
    return findRotationArrivalRec(t->getLeft(), sub - removedLen, pieceName, pvx, pvy, pvz, axis, sense);

  return false;
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
int disasmToMoves_c::doRecursive(const separation_c * tree, int step, float * array, unsigned int * orientsOut, bool center_active, int cx, int cy, int cz) {

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
        steps = doRecursive(tree->getRemoved(), step - (int)tree->getMoves(), array, orientsOut, center_active,
            tree->getState(tree->getMoves()-1)->getX(pc) + cx - tree->getRemoved()->getState(0)->getX(0),
            tree->getState(tree->getMoves()-1)->getY(pc) + cy - tree->getRemoved()->getState(0)->getY(0),
            tree->getState(tree->getMoves()-1)->getZ(pc) + cz - tree->getRemoved()->getState(0)->getZ(0));
      else
        steps = doRecursive(tree->getRemoved(), step - (int)tree->getMoves(), array, orientsOut, center_active, cx+dx, cy+dy, cz+dz);

    } else {

      const state_c * s2 = tree->getState(tree->getMoves()-1);

      /* if there is no removed tree, the pieces need to vanish */
      if (array)
        for (unsigned int p = 0; p < tree->getPieceNumber(); p++)
          if (s->pieceRemoved(p)) {
            array[4*tree->getPieceName(p)+0] += dx+cx+((mabs(s->getX(p))<10000)?(s->getX(p)):(s2->getX(p)));
            array[4*tree->getPieceName(p)+1] += dy+cy+((mabs(s->getY(p))<10000)?(s->getY(p)):(s2->getY(p)));
            array[4*tree->getPieceName(p)+2] += dz+cz+((mabs(s->getZ(p))<10000)?(s->getZ(p)):(s2->getZ(p)));
            array[4*tree->getPieceName(p)+3] += 0;
            if (orientsOut)
              orientsOut[tree->getPieceName(p)] = s2->getOrient(p);
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
        steps2 = doRecursive(tree->getLeft(), step - (int)tree->getMoves() - steps, array, orientsOut, center_active, cx-dx, cy-dy, cz-dz);
      else
        steps2 = doRecursive(tree->getLeft(), step - (int)tree->getMoves() - steps, array, orientsOut, center_active, cx, cy, cz);

      /* if the steps tell us that we are currently animating the removed part
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
            if (orientsOut)
              orientsOut[tree->getPieceName(p)] = s->getOrient(p);
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
      if (orientsOut)
        orientsOut[tree->getPieceName(i)] = s->getOrient(i);
    }

  int steps  = tree->getRemoved() ? doRecursive(tree->getRemoved(), step - tree->getMoves()        , 0, 0, center_active, 0, 0, 0) : 0;
  int steps2 = tree->getLeft()    ? doRecursive(tree->getLeft()   , step - tree->getMoves() - steps, 0, 0, center_active, 0, 0, 0) : 0;

  return tree->getMoves() + steps + steps2;
}







fixedPositions_c::fixedPositions_c(const disassemblerNode_c * nd, const std::vector<unsigned int> & pc, unsigned int pcs)
  : pieces(pcs), x(pcs, 0), y(pcs, 0), z(pcs, 0), visible(pcs, false) {

  for (unsigned int p = 0; p < pc.size(); p++) {

    unsigned int pi = pc[p];

    bt_assert(pi < pieces);

    x[pi] = (int)nd->getX(p);
    y[pi] = (int)nd->getY(p);
    z[pi] = (int)nd->getZ(p);

    visible[pi] = true;
  }
}

fixedPositions_c::fixedPositions_c(const fixedPositions_c * nd)
  : pieces(nd->pieces), x(nd->x), y(nd->y), z(nd->z), visible(nd->visible) {
}

fixedPositions_c::~fixedPositions_c(void) = default;

float fixedPositions_c::getX(unsigned int piece) { bt_assert(piece < pieces); return x[piece]; }
float fixedPositions_c::getY(unsigned int piece) { bt_assert(piece < pieces); return y[piece]; }
float fixedPositions_c::getZ(unsigned int piece) { bt_assert(piece < pieces); return z[piece]; }
float fixedPositions_c::getA(unsigned int piece) { return visible[piece] ? 1 : 0; }
bool fixedPositions_c::moving(unsigned int /*piece*/) { return false; }

