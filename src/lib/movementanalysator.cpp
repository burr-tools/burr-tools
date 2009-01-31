/* Burr Solver
 * Copyright (C) 2003-2009  Andreas Röver
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
#include "movementanalysator.h"

#include "bt_assert.h"
#include "movementcache.h"
#include "problem.h"
#include "disassemblernode.h"
#include "voxel.h"
#include "disassemblerhashes.h"
#include "gridtype.h"

#include <string.h>

/* so, this isn't the function as described by Bill but rather a
 * bit optimized. For each pair of 2 different pieces and for
 * each of the three dimensions I do the following:
 *  - check the intersection area area in this direction
 *  - if it's empty the pieces do not interlock and the matrix
 *    is initialized to infinity (32000)
 *  - if we have an intersection we check each column inside this area
 *    and find the shortest distance the first piece follows
 *    the second and the second piece follows the first
 */
void movementAnalysator_c::prepare(void) {

  unsigned int * idx = matrix;

  int idxCol = cache->numDirections();
  int idxRow = cache->numDirections() * (piecenumber- pieces->size());

  for (unsigned int j = 0; j < pieces->size(); j++) {
    for (unsigned int i = 0; i < pieces->size(); i++) {
      if (i != j)
        cache->getMoValue(searchnode->getX(j) - searchnode->getX(i),
                          searchnode->getY(j) - searchnode->getY(i),
                          searchnode->getZ(j) - searchnode->getZ(i),
                          searchnode->getTrans(i), searchnode->getTrans(j),
                          (*pieces)[i], (*pieces)[j], idx);

      // the diagonals are always zero and will stay that for ever they are initialized
      // to that value in the init function so only the other values need
      idx += idxCol;
    }
    idx += idxRow;
  }

  /* having a look at this algorithm in more detail
   * it comes out that the first pass has lots to do, the 2nd pass
   * is much cheaper (usually a few more corrections sometimes
   * event zero) and then it finished
   *
   * so I change this: alsways look, if a change done leads to other necessary
   * changes, and only if that is the case do another loop
   */

  /* second part of Bills algorithm. */

  unsigned int dirs = cache->numDirections();
  unsigned int rowStep = dirs*piecenumber;
  unsigned int size = dirs*pieces->size();

  bool again = false;

  for (unsigned int d = 0; d < dirs; d++) {
    do {
      again = false;

#if 0
      // this is just for commentaty reasons it show the same algorithmus as below
      // just a bit more understandable

      for (unsigned int y = 0; y < pieces->size(); y++)
        for (unsigned int x = 0; x < pieces->size(); x++) {
          unsigned int min = matrix[x*dirs+d] + matrix[y*piecenumber*dirs+d];

          for (unsigned int i = 1; i < pieces->size(); i++) {
            unsigned int l = matrix[(x + i*piecenumber)*dirs+d] + matrix[(i + y*piecenumber)*dirs+d];
            if (l < min) min = l;
          }

          if (min < matrix[(x + y*piecenumber)*dirs+d]) {
            matrix[(x + y*piecenumber)*dirs+d] = min;
            if (!again) {
              for (unsigned int i = 0; i < y; i++)
                if (min + matrix[(y + i*piecenumber)*dirs+d] < matrix[(x + i*piecenumber)*dirs+d]) {
                  again = true;
                  break;
                }

              if (!again)
                for (unsigned int i = 0; i < x; i++)
                  if (matrix[(i + x*piecenumber)*dirs+d] + min < matrix[(i + y*piecenumber)*dirs+d]) {
                    again = true;
                    break;
                  }
            }
          }
        }
#endif

      unsigned int * pos1 = matrix + d;           // y * piecenumber;
      unsigned int idx, i;

      for (unsigned int y = 0; y < size; y+=dirs) {
        unsigned int * pos2 = matrix + d;           // x

        for (unsigned int x = 0; x < size; x+=dirs) {
          unsigned int min = *pos2 + *pos1;

          for (i = dirs, idx = rowStep; i < size; i+=dirs, idx += rowStep) {
            unsigned int l = pos2[idx] + pos1[i];
            if (l < min) min = l;
          }

          if (min < pos1[x]) {
            pos1[x] = min;

            if (!again) {

              unsigned int * pos3 = matrix + d;

              for (i = 0; i < y; i+=dirs) {
                if (min + pos3[y] < pos3[x]) {
                  again = true;
                  break;
                }
                pos3 += rowStep;
              }

              if (!again) {

                pos3 = matrix + d + piecenumber*x;

                for (i = 0; i < x; i+=dirs)
                  if (pos3[i] + min < pos1[i]) {
                    again = true;
                    break;
                  }
              }
            }
          }
          pos2+=dirs;
        }

        pos1 += rowStep;
      }
    } while (again > 0);
  }
}

/*
 * suppose you want to move piece x y units into one direction, if you hit another piece
 * on your way and this piece can be moved then it may be nice to also move this piece
 *
 * so this function adjusts the movement of other pieces so that one piece can be moved
 * the requested number of units.
 *
 * in the worst case when no movement in the selected direction is possible all values are
 * set to the same value meaning the whole puzzle is moved
 *
 * to distinguish "good" and "bad" moves the function returns true, if less maxPieces
 * have to be moved, this value should not be larger than halve of the pieces in the puzzle
 */
bool movementAnalysator_c::checkmovement(unsigned int maxPieces, unsigned int nextstep) {

  /* we count the number of pieces that need to be moved, if this number
   * gets bigger than halve of the pieces of the current problem we
   * stop and return that this movement is rubbish
   */
  unsigned int moved_pieces = 1;
  bool check[piecenumber];

  /* initialize the movement matrix. We want to move 'nextpiece' 'nextstep' units
   * into the current direction, so we initialize the matrix with all
   * zero except for our piece
   */
  for (int i = 0; i < next_pn; i++) {
    movement[i] = 0;
    check[i] = false;
  }
  movement[nextpiece] = nextstep;
  check[nextpiece] = true;

  bool finished;
  unsigned int nd = nextdir >> 1;
  unsigned int dirs = cache->numDirections();
  bt_assert(nd < dirs);
  unsigned int rowIdx = (piecenumber-next_pn)*dirs;
  unsigned int rowIdx2 = piecenumber*dirs;
  unsigned int rowIdx3 = (piecenumber*next_pn-1)*dirs;

  // the idea here is the following, if we want to move
  // a piece the matrix tells us if we can do that with respecto to
  // another piece, if we can't that other piece must be moved as well
  // and with that new moved piece we need to check that piece, too
  //
  // the comments are only in the first part the second is the same
  // just for the other directions
  if (nextdir & 1) {

    do {

      finished = true;
      unsigned int * idx = matrix + nd;

      // go over all pieces
      for (int i = 0; i < next_pn; i++)
      {
        // if the piece needs to be checked
        if (check[i])
        {
          // check it against all other pieces
          for (int j = 0; j < next_pn; j++)
          {
            // if it is another piece that is still stationary (if it is already
            // moving it moves by the same amount as the other piece, so there
            // will be no problems here
            if ((i != j) && (movement[j] == 0)) {
              // if the requested movement is more than the matrix alows
              // we must also move the new piece

              if (movement[i] > *idx) {  // idx points to matrix[(j+piecenumber*i)*dirs+nd]
                // count the number of moved pieces, if there are more
                // than halve, we bail out because it doesn't make sense
                // to move more than that amount
                moved_pieces++;
                if (moved_pieces > maxPieces)
                  return false;

                // to we move that new piece by the same amount
                // as the first piece and we also need to check
                // that new piece
                movement[j] = nextstep;
                check[j] = true;
                finished = false;
              }
            }
            idx += dirs;
          }
          // the current piece is now checked, so we don't need to do that again
          check[i] = false;
          idx += rowIdx;
        }
        else
        {
          idx += rowIdx2;
        }
      }
    } while (!finished);

  } else {

    do {

      finished = true;
      unsigned int * idx = matrix + nd;

      for (int i = 0; i < next_pn; i++)
      {
        if (check[i])
        {
          for (int j = 0; j < next_pn; j++) {
            if ((i != j) && (movement[j] == 0)) {
              if (movement[i] > *idx) {  // idx should point to matrix[(i + piecenumber * j)*dirs+nr]
                moved_pieces++;
                if (moved_pieces > maxPieces)
                  return false;

                movement[j] = nextstep;
                check[j] = true;
                finished = false;
              }
            }
            idx += rowIdx2;
          }
          check[i] = false;
          idx -= rowIdx3;
        }
        else
        {
          idx += dirs;
        }
      }

    } while (!finished);
  }

  return true;
}

movementAnalysator_c::movementAnalysator_c(const problem_c * puz) :
  piecenumber(puz->pieceNumber()), maxstep((unsigned int) -1) {

  cache = puz->getGridType()->getMovementCache(puz);

  /* allocate the necessary arrays */
  movement = new unsigned int[piecenumber];

  matrix = new unsigned int[cache->numDirections() * piecenumber * piecenumber];
  memset(matrix, 0, cache->numDirections() * piecenumber * piecenumber * sizeof(unsigned int));

  /* create the weights array */
  weights = new int[puz->pieceNumber()];
  unsigned int pc = 0;
  for (unsigned int i = 0; i < puz->shapeNumber(); i++) {
    for (unsigned int j = 0; j < puz->getShapeMax(i); j++)
      weights[pc++] = puz->getShapeShape(i)->getWeight();
  }

  nextstate = -1;

  nodes = new countingNodeHash();
}

movementAnalysator_c::~movementAnalysator_c() {

  delete [] movement;
  delete [] matrix;

  delete cache;
  delete [] weights;
  delete nodes;
}

static int max(int a, int b) { if (a > b) return a; else return b; }

/* creates a new node with the information from the movement array
 * where the pieces move by amount in direction nextdir
 * staring point is the searchnode
 */
disassemblerNode_c * movementAnalysator_c::newNode(unsigned int amount) {

  // calculate the weight of the all the stationary and all the
  // moving pieces
  int moveWeight = 0;
  int stilWeight = 0;
  int nd = nextdir;

  for (unsigned int i = 0; i < pieces->size(); i++) {
    if (movement[i]) {
      bt_assert(amount == movement[i]);

      moveWeight = max(moveWeight, weights[(*pieces)[i]]);

    } else {
      stilWeight = max(stilWeight, weights[(*pieces)[i]]);
    }
  }

  /* we need to invert the movement direction, when the
   * weight of the currently moved pieces is bigger than
   * those of stationary pieces
   */
  if (stilWeight < moveWeight) {

    // stationary pieces become moved, moved piece become stationary
    for (unsigned int i = 0; i < pieces->size(); i++)
      if (movement[i])
        movement[i] = 0;
      else
        movement[i] = amount;

    // and the direction changes to the opposite direction
    nd ^= 1;
  }

  disassemblerNode_c * n = new disassemblerNode_c(pieces->size(), searchnode, nd, amount);

  /* create a new state with the pieces moved */
  for (unsigned int i = 0; i < pieces->size(); i++) {

    if (movement[i]) {

      if (movement[i] == 30000) {

        int mx, my, mz;

        cache->getDirection(nd >> 1, &mx, &my, &mz);

        if (nd & 1) {
          mx = -mx;
          my = -my;
          mz = -mz;
        }

        n->set(i, mx, my, mz, 0xff);

      } else {

        int mx, my, mz;

        cache->getDirection(nd >> 1, &mx, &my, &mz);

        mx *= movement[i];
        my *= movement[i];
        mz *= movement[i];

        if (nd & 1) {
          mx = -mx;
          my = -my;
          mz = -mz;
        }

        n->set(i, searchnode, mx, my, mz);
      }

    } else {

      n->set(i, searchnode, 0, 0, 0);

    }
  }

  return n;
}

/* creates a new node that contains the merged movements of the given 2 nodes
 * merged movement means that a piece is moved the maximum amount specified in
 * both nodes. But only one direction is allowed, so if one piece moves this
 * way and another piece that way 0 i sreturned
 * the function also returns zero, if the new node would be identical to n1 or n0
 * also the amount must be identical in both nodes, so if piece a moves 1 unit
 * in node n0 and andother piece move 2 units in node n1 0 is returned
 */
disassemblerNode_c * movementAnalysator_c::newNodeMerge(const disassemblerNode_c *n0, const disassemblerNode_c *n1) {

  // assert that direction are along the same axis
  bt_assert((nextdir | 1) == (n0->getDirection() | 1));
  bt_assert((nextdir | 1) == (n1->getDirection() | 1));

  bool invert0 = (nextdir != n0->getDirection());
  bool invert1 = (nextdir != n1->getDirection());

  // both nodes need to have the same movement amount, if not return 0
  int amount = n0->getAmount();
  if (amount != n1->getAmount()) return 0;

  /* we need to make sure the new node is different from n0 and n1
   */
  bool different0 = false;
  bool different1 = false;
  int moved = 0;
  bool move0, move1;

  for (int i = 0; i < next_pn; i++) {

    // calculate the movement of the merged node by first finding out if the
    // piece has been moved within one node
    move0 = ((n0->getX(i) != searchnode->getX(i)) ||
             (n0->getY(i) != searchnode->getY(i)) ||
             (n0->getZ(i) != searchnode->getZ(i))) ^ invert0;
    move1 = ((n1->getX(i) != searchnode->getX(i)) ||
             (n1->getY(i) != searchnode->getY(i)) ||
             (n1->getZ(i) != searchnode->getZ(i))) ^ invert1;

    // and if it has been moved in one of them, it needs
    // to be moved in the new node
    if (move0 || move1) {
      movement[i] = amount;
      moved++;
    } else
      movement[i] = 0;

    // the new node differs from the old one if there was a movement cause by the other node which
    // was not available in the first one
    different0 |= (move1 && !move0);
    different1 |= (move0 && !move1);
  }

  // if no or all pieces are moved, exit, this created degenerated nodes
  if (moved == 0 || moved == next_pn) return 0;

  // if the new node is equal to n0 or n1, exit
  if (!different0 || !different1) return 0;

  return newNode(amount);
}


void movementAnalysator_c::init_find(disassemblerNode_c * nd, const std::vector<unsigned int> & pcs) {

  /* initialize the state machine for the find routine
   */
  nextdir = 0;
  nextpiece = 0;
  nextstep = 1;
  nextstate = 0;
  next_pn = pcs.size();

  searchnode = nd;
  pieces = &pcs;

  /* when a new search has been started we need to first calculate
   * the movement matrices, this is a table that contains one 2 dimensional
   * matrix for each of the 6 directions where movement is possible
   *
   * the matrices contains possible movement of one piece if other pieces
   * are not moved. So a one in column 2 row 4 means that piece nr. 2 can
   * be moved one unit it we fix piece nr. 4
   *
   * the algorithm used here is describes in Bill Cutlers booklet
   * "Computer Analysis of All 6 Piece Burrs"
   */
  prepare();
}

/* at first we check if movement is possible at all in the current direction, if so
 * the next thing to do is to check if something can be removed, and finally we look for longer
 * movements in the actual direction
 */
disassemblerNode_c * movementAnalysator_c::find(void) {

  disassemblerNode_c * n = 0;

  // repeat until we either find a movement or have checked everything
  while (!n) {

    switch (nextstate) {
      case 0:
        // check, if a single piece can be removed
        if (checkmovement(1, 30000))
          n = newNode(30000);

        nextpiece++;
        if (nextpiece >= next_pn) {
          nextpiece = 0;
          nextdir++;
          if (nextdir >= 2*cache->numDirections()) {
            nextstate++;
            nextdir = 0;
          }
        }
        break;
      case 1:
        // check, if a group of pieces can be removed
        if (checkmovement(next_pn/2, 30000))
          n = newNode(30000);

        nextpiece++;
        if (nextpiece >= next_pn) {
          nextpiece = 0;
          nextdir++;
          if (nextdir >= 2*cache->numDirections()) {
            nextstate++;
            nextdir = 0;
            nodes->clear();
          }
        }
        break;
      case 2:
        // check, if pieces can be moved
        if ((nextstep <= maxstep) && checkmovement(next_pn/2, nextstep)) {
          n = newNode(nextstep);
          bt_assert(n);

          // we need to merge the gained node with all already found
          // nodes with the same step and if that leads to valid new nodes
          // we also need to return those

          // but first we check, if we have this node already found (maybe via a merger)
          // and if so we delete it
          if (nodes->insert(n)) {
            delete n;
            n = 0;

          } else {

            nextstate = 99;
            state99node = n;
            nodes->initScan();
            state99nextState = 2;
          }

          // if we can move something, we try larger steps
          nextstep++;

        } else {

          // if not, lets try the next piece
          nextstep = 1;
          nextpiece++;
          if (nextpiece >= next_pn) {
            nextpiece = 0;
            nextdir++;
            nodes->clear();
            if (nextdir >= 2*cache->numDirections()) {
              nextstate++;
            }
          }
        }
        break;

      case 99:

        // this is a special state that takes the last found node and creates mergers with all
        // the already found nodes.
        // a merger is a new node that contains the movement of one node AND the movement of
        // the 2nd node at the same time. Of course both nodes need to point into the same
        // direction and in both nodes the pieces need to be moved by
        // the same amount
        //
        // This is needed because when moving groups of pieces and both pieces are independent of
        // one another the code above alone wont find movements where both pieces are moved at
        // the same time but rather one after the other

        {
          const disassemblerNode_c * nd2 = nodes->nextScan();

          if (nd2) {
            n = newNodeMerge(state99node, nd2);

            // if the node is valid check if we already know that node, if so
            // delete it
            if (n && nodes->insert(n)) {
              delete n;
              n = 0;
            }

          } else
            nextstate = state99nextState;
        }

        break;

      default:
        // endstate, do nothing
        return 0;
    }
  }

  return n;
}

void movementAnalysator_c::completeFind(disassemblerNode_c * searchnode, const std::vector<unsigned int> & pieces, std::vector<disassemblerNode_c*> * result) {

  init_find(searchnode, pieces);

  for (unsigned int i = 0; i < result->size(); i++)
    delete (*result)[i];
  result->clear();

  disassemblerNode_c * nd;

  maxstep = 1;

  std::vector<disassemblerNode_c*> toremove;

  while ((nd = find()) != 0) {
    for (unsigned int i = 0; i < result->size(); i++) {
      if (*(*result)[i] == *nd) {
        toremove.push_back(nd);
        nd = 0;
        break;
      }
    }

    if (nd)
      result->push_back(nd);
  }

  maxstep = (unsigned int)-1;

  for (unsigned int i = 0; i < toremove.size(); i++)
    delete toremove[i];
}

