/* Burr Solver
 * Copyright (C) 2003-2008  Andreas Röver
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
#include "disassembler_0.h"

#include "disassembly.h"
#include "bt_assert.h"
#include "movementcache_0.h"
#include "puzzle.h"
#include "grouping.h"
#include "assembly.h"

#include "disassemblernode.h"
#include "disassemblerhashes.h"

#include <queue>
#include <vector>

void disassembler_0_c::init_find(disassemblerNode_c * nd, int piecenumber, voxel_type * pieces) {

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
  prepare(piecenumber, pieces, nd);

  /* initialize the state machine for the find routine
   */
  nextdir = 0;
  nextpiece = 0;
  nextstep = 1;
  nextstate = 0;
  next_pn = piecenumber;
}

/* at first we check if movement is possible at all in the current direction, if so
 * the next thing to do is to check if something can be removed, and finally we look for longer
 * movements in the actual direction
 */
disassemblerNode_c * disassembler_0_c::find(disassemblerNode_c * searchnode, const int * weights) {

  disassemblerNode_c * n = 0;

  static countingNodeHash nodes;

  // repeat until we either find a movement or have checked everything
  while (!n) {

    switch (nextstate) {
      case 0:
        // check, if a single piece can be removed
        if (checkmovement(1, nextdir, next_pn, nextpiece, 30000))
          n = newNode(next_pn, nextdir, searchnode, weights, 30000);

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
        if (checkmovement(next_pn/2, nextdir, next_pn, nextpiece, 30000))
          n = newNode(next_pn, nextdir, searchnode, weights, 30000);

        nextpiece++;
        if (nextpiece >= next_pn) {
          nextpiece = 0;
          nextdir++;
          if (nextdir >= 2*cache->numDirections()) {
            nextstate++;
            nextdir = 0;
            nodes.clear();
          }
        }
        break;
      case 2:
        // check, if pieces can be moved
        if (checkmovement(next_pn/2, nextdir, next_pn, nextpiece, nextstep)) {
          n = newNode(next_pn, nextdir, searchnode, weights, nextstep);
          bt_assert(n);

          // we need to merge the gained node with all already found
          // nodes with the same step and if that leads to valid new nodes
          // we also need to return those

          // but first we check, if we have this node already found (maybe via a merger)
          // and if so we delete it
          if (nodes.insert(n)) {
            delete n;
            n = 0;

          } else {

            nextstate = 99;
            state99node = n;
            nodes.initScan();
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
            nodes.clear();
            if (nextdir >= 2*cache->numDirections()) {
              return 0;
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
          const disassemblerNode_c * nd2 = nodes.nextScan();

          if (nd2) {
            n = newNodeMerge(state99node, nd2, searchnode, next_pn, nextdir, weights);

            // if the node is valid check if we already know that node, if so
            // delete it
            if (n && nodes.insert(n)) {
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

/* create all the necessary parameters for one of the two possible subproblems
 * our current problems divides into
 */
static void create_new_params(disassemblerNode_c * st, disassemblerNode_c ** n, voxel_type ** pn, int ** nw, int piecenumber, voxel_type * pieces, const int * weights, int part, bool cond) {

  *n = new disassemblerNode_c(part, 0, 0, 0);
  *pn = new voxel_type[part];
  *nw = new int[part];

  int num = 0;
  int dx, dy, dz;

  dx = dy = dz = 0;

  for (int i = 0; i < piecenumber; i++)
    if (st->is_piece_removed(i) == cond) {
      if (num == 0) {
        /* find the direction, the first piece was moved out of the puzzle
         * and shift it back along this axis */
        if ((st->getX(i) > 10000) || (st->getX(i) < -10000)) dx = st->getX(i);
        if ((st->getY(i) > 10000) || (st->getY(i) < -10000)) dy = st->getY(i);
        if ((st->getZ(i) > 10000) || (st->getZ(i) < -10000)) dz = st->getZ(i);
      }
      (*n)->set(num,
                st->getX(i) - dx,
                st->getY(i) - dy,
                st->getZ(i) - dz,
                st->getTrans(i));
      (*pn)[num] = pieces[i];
      (*nw)[num] = weights[i];
      num++;
    }

  bt_assert(num == part);
}

unsigned short disassembler_0_c::subProbGroup(disassemblerNode_c * st, voxel_type * pn, bool cond, int piecenumber) {

  unsigned short group = 0;

  for (int i = 0; i < piecenumber; i++)
    if (st->is_piece_removed(i) == cond)
      if (puzzle->probGetShapeGroupNumber(problem, piece2shape[pn[i]]) != 1)
        return 0;
      else if (group == 0)
        group = puzzle->probGetShapeGroup(problem, piece2shape[pn[i]], 0);
      else if (group != puzzle->probGetShapeGroup(problem, piece2shape[pn[i]], 0))
        return 0;

  return group;
}

bool disassembler_0_c::subProbGrouping(voxel_type * pn, int piecenumber) {

  groups->newSet();

  for (int i = 0; i < piecenumber; i++)
    if (!groups->addPieceToSet(piece2shape[pn[i]]))
      return false;

  return true;
}

separation_c * disassembler_0_c::checkSubproblem(int pieceCount, voxel_type * pieces, int piecenumber, disassemblerNode_c * st, bool left, bool * ok, const int * weights) {

  separation_c * res = 0;

  if (pieceCount == 1) {
    *ok = true;
  } else if (subProbGroup(st, pieces, left, piecenumber)) {
    *ok = true;
  } else {

    disassemblerNode_c *n;
    voxel_type * pn;
    int * nw;
    create_new_params(st, &n, &pn, &nw, piecenumber, pieces, weights, pieceCount, left);
    res = disassemble_rec(pieceCount, pn, n, nw);

    *ok = res || subProbGrouping(pn, pieceCount);

    delete [] pn;
    delete [] nw;
  }

  return res;
}

/* this is a breadth first search function that analyses the movement of
 * an assembled problem. When the problem falls apart into 2 pieces the function
 * calls itself recursively. It returns null if the problem can not be taken apart
 * completely and otherwise the disassembly tree
 *
 * the parameters are:
 *  - piecenumber: the number of pieces of the current problem. Because we can have
 *                 subproblems this number is not identical to the number of pieces
 *                 in the assembly voxel space
 *  - pieces: defines which pieces of the assembly voxel space are actually really present
 *            in the current subproblem
 *  - start: the start position of each piece
 *
 * the function takes over the ownership of the node and pieces. They are deleted at the end
 * of the function, so you must allocate them with new
 */
separation_c * disassembler_0_c::disassemble_rec(int piecenumber, voxel_type * pieces, disassemblerNode_c * start, const int * weights) {

  std::queue<disassemblerNode_c *> openlist[2];
  nodeHash closed[3];

  int curListFront = 0;
  int newListFront = 1;
  int oldFront = 0;
  int curFront = 1;
  int newFront = 2;

  closed[curFront].insert(start);
  openlist[curListFront].push(start);

  separation_c * erg = 0;

  /* the algorithm works with 3 sets of nodes. All nodes in one set do have the same distance
   * from the start node. The 3 sets are the current frontier (cf), the new frontier (nf) and the line
   * behind the current frontier (of).
   * The nodes in the cf list are taken one by one and examined and all neighbour nodes are generated. Now
   * there are 3 possibilities: 1) the node is one way further from the start, then the new node belongs to
   * nf, if the node has the same distance from the start as the currently examined node, if belongs into the
   * cf list and if it is nearer to the start is must be in of.
   * How do we find out where it belongs? By checking, if that node is already there. We check if the node is
   * in of or in cf or in nf, only if it is nowhere to be found, we add it to nf.
   * After all nodes in cf are checked, all nodes in of are dropped, cf becomes of, and nf becomes cf. nf is empty.
   * And we restart.
   * "Dropping" of the old frontier doesn't mean the nodes are deleted. They can only be deleted, when they are not
   * part of the used shortest way to one of the nodes in cf. This is done by reference counting. So dropping
   * means, check if there is an other node using a node, if not we can delete the node. This deletion may even
   * result in more deletion as this might drop the reference counter of another node to 0
   * This reference counter is increased for each node whose comefrom pointer points to that node. The counter
   * is also increased by one as long as it is inside one of the frontier lists.
   */

  /* while there are nodes left we should look at and we have not found a solution */
  while (!openlist[curListFront].empty()) {

    /* remove the node from the open list and start examining */
    disassemblerNode_c * node = openlist[curListFront].front();
    openlist[curListFront].pop();

    /* if the current list is now empty, we need to toggle everything after we have finished
     * searching this node
     */

    init_find(node, piecenumber, pieces);

    disassemblerNode_c * st;

    while ((st = find(node, weights))) {

      /* check all closed nodelists and also insert the node into the newFront list
       * insert has the same return value as contains, but also inserts the node
       * when it is not yet inside the hashtable
       */
      if (closed[oldFront].contains(st) || closed[curFront].contains(st) || closed[newFront].insert(st)) {

        /* the new node is already here. We have found a new longer way to that
         * node, so we can safely delete the new node and continue to the next
         *
         * we use the reference count mechanism of the node class, so if the node
         * isn't use anywhere else, we can delete it here
         */
        if (st->decRefCount())
          delete st;

        continue;
      }

      if (!st->is_separation()) {

        /* the new node is no solution so insert the
         * new state into the known state table
         * and the open list for later examination and go on to the next node
         */
        openlist[newListFront].push(st);
        continue;
      }

      /* if we get here we have found a node that separated the puzzle into
       * 2 pieces. So we recursively solve the subpuzzles and create a tree
       * with them that needs to be returned
       */

      /* count the pieces in both parts */
      int part1 = 0, part2 = 0;

      for (int i = 0; i < piecenumber; i++)
        if (st->is_piece_removed(i))
          part2++;
        else
          part1++;

      /* each subpart must contain at least 1 piece,
       * otherwise there is something wrong
       */
      bt_assert((part1 > 0) && (part2 > 0));

      separation_c * left, *remove;
      bool left_ok = false;
      bool remove_ok = false;
      left = remove = 0;

      /* all right, the following thing come twice, maybe I should
       * put it into a function, anyway:
       * if the subproblem to check has only one piece, it's solved
       * if all the pieces belong to the same group, we can stop
       * else try to disassemble, if that fails, try to
       * group the involved pieces into an identical group
       */
      remove = checkSubproblem(part1, pieces, piecenumber, st, false, &remove_ok, weights);

      /* only check the left over part, when the removed part is OK */
      if (remove_ok)
        left = checkSubproblem(part2, pieces, piecenumber, st, true, &left_ok, weights);

      /* if both subproblems are either trivial or solvable, return the
       * result, otherwise return 0
       */
      if (remove_ok && left_ok) {

        /* both subproblems are solvable -> construct tree */
        erg = new separation_c(left, remove, piecenumber, pieces);

        do {
          state_c *s = new state_c(piecenumber);

          for (int i = 0; i < piecenumber; i++)
            s->set(i, st->getX(i), st->getY(i), st->getZ(i));

          erg->addstate(s);

          st = (disassemblerNode_c*)st->getComefrom();
        } while (st);

      } else {

        /* one of the subproblems was unsolvable in this case the whole
         * puzzle is unsolvable, so we can as well stop here
         */
        if (left) delete left;
        if (remove) delete remove;
      }

      /* if we get here we can stop, even if we didn't find a solution
       * so we empty the openlist and se stop the currently running
       * search process
       */

      /* nodes inside the closed hashtables are freed automagically */

      return erg;
    }

    // if the current front is completely checked, open up the new front
    if (openlist[curListFront].empty()) {

      // toggle the 2 lists of the fronts
      curListFront = 1 - curListFront;
      newListFront = 1 - newListFront;

      // free the oldFront nodes
      closed[oldFront].clear();

      oldFront = curFront;
      curFront = newFront;
      newFront = (newFront + 1) % 3;
    }
  }

  // the nodes inside the hashtables are freed automatically
  return 0;
}

disassembler_0_c::disassembler_0_c(movementCache_c * c, const puzzle_c * puz, unsigned int prob) :
  disassembler_a_c(c, puz, prob),
  puzzle(puz), problem(prob) {

  /* initialize the grouping class */
  groups = new grouping_c();
  for (unsigned int i = 0; i < puz->probShapeNumber(prob); i++)
    for (unsigned int j = 0; j < puz->probGetShapeGroupNumber(prob, i); j++)
      groups->addPieces(puz->probGetShape(prob, i),
                        puz->probGetShapeGroup(prob, i, j),
                        puz->probGetShapeGroupCount(prob, i, j));

  /* initialize piece 2 shape transformation */
  piece2shape = new unsigned short[puz->probPieceNumber(prob)];
  int p = 0;
  for (unsigned int i = 0; i < puz->probShapeNumber(prob); i++)
    for (unsigned int j = 0; j < puz->probGetShapeMax(prob, i); j++)
      piece2shape[p++] = i;

  /* create the weights array */
  weights = new int[puz->probPieceNumber(prob)];
  for (unsigned int i = 0; i < puz->probPieceNumber(prob); i++)
    weights[i] = puzzle->probGetShapeShape(prob, piece2shape[i])->getWeight();
}

disassembler_0_c::~disassembler_0_c() {

  delete groups;
  delete [] piece2shape;
  delete [] weights;
}

separation_c * disassembler_0_c::disassemble(const assembly_c * assembly) {

  bt_assert(assembly->placementCount() == puzzle->probPieceNumber(problem));

  /* create the first node with the start state
   * here all pieces are at position (0; 0; 0)
   */
  unsigned int pc = 0;
  for (unsigned int j = 0; j < assembly->placementCount(); j++)
    if (assembly->isPlaced(j))
      pc++;

  if (pc < 2)
    return 0;

  disassemblerNode_c * start = new disassemblerNode_c(pc, 0, 0, 0);

  /* create pieces field. This field contains the
   * names of all present pieces. Because at the start
   * all pieces are still there we fill the array
   * with all the numbers
   */
  voxel_type * pieces = new voxel_type[pc];
  pc = 0;
  for (unsigned int j = 0; j < assembly->placementCount(); j++)
    if (assembly->isPlaced(j)) {
      start->set(pc, assembly->getX(j), assembly->getY(j), assembly->getZ(j), assembly->getTransformation(j));
      pieces[pc] = j;
      pc++;
    }

  /* reset the grouping class */
  groups->reSet();

  separation_c * s = disassemble_rec(pc, pieces, start, weights);

  delete [] pieces;

  return s;
}
