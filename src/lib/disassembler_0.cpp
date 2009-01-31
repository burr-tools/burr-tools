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
#include "puzzle.h"
#include "assembly.h"
#include "movementanalysator.h"

#include "disassemblernode.h"
#include "disassemblerhashes.h"

#include <queue>
#include <vector>

separation_c * disassembler_0_c::checkSubproblem(int pieceCount, const std::vector<unsigned int> & pieces, disassemblerNode_c * st, bool left, bool * ok) {

  separation_c * res = 0;

  if (pieceCount == 1) {
    *ok = true;
  } else if (subProbGroup(st, pieces, left)) {
    *ok = true;
  } else {

    disassemblerNode_c *n;
    std::vector<unsigned int> pn;
    create_new_params(st, &n, pn, pieces, pieceCount, left);
    res = disassemble_rec(pn, n);

    *ok = res || subProbGrouping(pn);
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
separation_c * disassembler_0_c::disassemble_rec(const std::vector<unsigned int> &pieces, disassemblerNode_c * start) {

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

    analyse->init_find0(node, pieces);

    disassemblerNode_c * st;

    while ((st = analyse->find0(node, pieces))) {

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

      for (unsigned int i = 0; i < pieces.size(); i++)
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
      remove = checkSubproblem(part1, pieces, st, false, &remove_ok);

      /* only check the left over part, when the removed part is OK */
      if (remove_ok)
        left = checkSubproblem(part2, pieces, st, true, &left_ok);

      /* if both subproblems are either trivial or solvable, return the
       * result, otherwise return 0
       */
      if (remove_ok && left_ok) {

        /* both subproblems are solvable -> construct tree */
        erg = new separation_c(left, remove, pieces);

        do {
          state_c *s = new state_c(pieces.size());

          for (unsigned int i = 0; i < pieces.size(); i++)
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

disassembler_0_c::disassembler_0_c(const puzzle_c * puz, unsigned int prob) :
  disassembler_a_c(puz, prob) {


}

disassembler_0_c::~disassembler_0_c() {

}

separation_c * disassembler_0_c::disassemble(const assembly_c * assembly) {

  bt_assert(getPiecenumber() == assembly->placementCount());

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
  std::vector<unsigned int> pieces;
  pc = 0;
  for (unsigned int j = 0; j < assembly->placementCount(); j++)
    if (assembly->isPlaced(j)) {
      start->set(pc, assembly->getX(j), assembly->getY(j), assembly->getZ(j), assembly->getTransformation(j));
      pieces.push_back(j);
      pc++;
    }

  /* reset the grouping class */
  groupReset();

  separation_c * s = disassemble_rec(pieces, start);

  return s;
}
