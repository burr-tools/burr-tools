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
 *  - pieces: defines which pieces of the assembly voxel space are actually really present
 *            in the current subproblem
 *  - start: the start position of each piece
 *
 * A lof of stuff happens automatically, e.g the disassemblerNode_c class is reference counted
 * and the nodeHashs will automatically decrease that count and free the nodes
 */
separation_c * disassembler_0_c::disassemble_rec(const std::vector<unsigned int> &pieces, disassemblerNode_c * start) {

  // openlist is a list of nodes that need to be analyzed. They wew found
  // as neighbours of nodes that were analyzed and not yet known
  std::queue<disassemblerNode_c *> openlist[2];
  // closed nodes are nodes that were analyzed. We don't want to keep all
  // those nodes around because we only need to keep them, when they could
  // be on a shortest path. That means we only keep the nodes, when they
  // lead to a node from the open list
  //
  // But we need to keep the nodes long enough to be sure we don't walk back
  // in our movement tree. So we organize the closed nodes in 3 fronts. All nodes
  // on each front have the same distance from the root node. old is one step less
  // than current which again is one step less than new front away from root
  //
  // when we start analysing a new front all nodes in the open list are also in the
  // current front. The new front is empty. now we take one node adfter the other from
  // the open list, analyze them. When the new found nodes are in oldfront, then they
  // are a step back towards root and can be dropped. When they are in the current
  // front, then they are a step sideways to another node with the same distance and
  // can be dropped, when the new node is in new front, it is a step away from the
  // root, but we already do know a way to that node and we can drop this node.
  // Only when the new node is unknown it is something interesting. We insert the
  // new node into the new front.
  //
  // That way we slowly empty the openList and build up a new open list with nodes
  // that we didn't know about before
  //
  // Once the current open list is empty, we have completely analyzed the current front
  // that means we don't need the old front any more because we can no longer reach those
  // nodes directly by analyzing a node from the open list because those nodes are now at
  // least 2 steps away, so we release them. The reference counting will make sure no longer
  // needed nodes will get freed.
  // The current front will becom the old front, the new front the current front and we
  // open up a new empty new front. The new built up open list will be used to get
  // the new nodes to analyze and an other open list will be started.
  nodeHash closed[3];

  // setup the fronts and the open List indices
  int curListFront = 0;
  int newListFront = 1;
  int oldFront = 0;
  int curFront = 1;
  int newFront = 2;

  // insert the start node
  closed[curFront].insert(start);
  openlist[curListFront].push(start);

  /* while there are nodes left we should look at */
  while (!openlist[curListFront].empty()) {

    /* remove the node from the open list and start examining */
    disassemblerNode_c * node = openlist[curListFront].front();
    openlist[curListFront].pop();

    // initialize a movement analysis for the current node
    init_find(node, pieces);

    disassemblerNode_c * st;

    while ((st = find())) {

      /* check the different fronts and also try to insert into the new
       * front, if it is known in either front, ...
       */
      if (closed[oldFront].contains(st) || closed[curFront].contains(st) || closed[newFront].insert(st)) {

        /* the new node is already here. We have found a new longer or equal long way to that
         * node, so we can safely delete the new node and continue to the next
         *
         * we use the reference count mechanism of the node class, so if the node
         * isn't use anywhere else, we can delete it here
         */
        if (st->decRefCount())
          delete st;

        continue;
      }

      // when we get here the new found node was not known before

      if (!st->is_separation()) {

        /* the new node is no solution so insert the node into
         * the open list for later examination and go on to the next node
         */
        openlist[newListFront].push(st);

        // we need to dec-ref-count because we will overwrite st in the next step
        // and st hold one count of the node, once we get to use boost smart
        // ponters this here will become simpler
        if (st->decRefCount())
          delete st;

        continue;
      }

      /* when we get here the new found node is a solution */

      /* check the possible sub problems, this function call disassemble rec recursivly */
      separation_c * res = checkSubproblems(st, pieces);

      if (st->decRefCount())
        delete st;

      return res;

      /* nodes inside the closed hashtables are freed automagically */
    }

    // if the current front is completely checked, open up the new front
    if (openlist[curListFront].empty()) {

      // toggle the 2 lists of the fronts
      curListFront = 1 - curListFront;
      newListFront = 1 - newListFront;

      // free the oldFront nodes
      closed[oldFront].clear();

      // circle the fronts
      oldFront = curFront;
      curFront = newFront;
      newFront = (newFront + 1) % 3;
    }
  }

  // we have not found a node that separated the problem, so return 0
  return 0;

  // the nodes inside the hashtables are freed automatically
}

