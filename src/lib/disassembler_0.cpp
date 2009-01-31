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
#include "disassembler_0.h"

#include "bt_assert.h"

#include "disassemblernode.h"
#include "disassemblerhashes.h"

#include <queue>
#include <vector>

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

    init_find(node, pieces);

    disassemblerNode_c * st;

    while ((st = find())) {

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

      /* if we get here we can stop, even if we didn't find a solution
       * so we empty the openlist and se stop the currently running
       * search process
       */

      /* nodes inside the closed hashtables are freed automagically */

      /* check the possible sub problems, this function call disassemble rec recursivly */
      return checkSubproblems(st, pieces);
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

