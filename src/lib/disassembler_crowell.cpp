/* BurrTools
 *
 * Andrew Crowell take-apart BFS. Same structure as disassembler_0_c with a
 * per-node successor cap matching CheckAvailMoves Nkeep.
 * Feature map (implemented vs not, and how to extend): crowell_solver.h
 */
#include "disassembler_crowell.h"

#include "bt_assert.h"

#include "disassemblernode.h"
#include "disassemblerhashes.h"

#include <queue>
#include <vector>

separation_c * disassembler_crowell_c::disassemble_rec(const std::vector<unsigned int> &pieces, disassemblerNode_c * start) {

  std::queue<disassemblerNode_c *> openlist[2];
  nodeHash closed[3];

  int curListFront = 0;
  int newListFront = 1;
  int oldFront = 0;
  int curFront = 1;
  int newFront = 2;

  closed[curFront].insert(start);
  openlist[curListFront].push(start);

  while (!openlist[curListFront].empty()) {

    if (aborted())
      return 0;

    disassemblerNode_c * node = openlist[curListFront].front();
    openlist[curListFront].pop();

    init_find(node, pieces);

    disassemblerNode_c * st;
    int added = 0;

    while ((st = find()) && !aborted()) {

      if (closed[oldFront].contains(st) || closed[curFront].contains(st) || closed[newFront].insert(st)) {

        if (st->decRefCount())
          delete st;

        continue;
      }

      if (!st->is_separation()) {

        if (added < MAX_SUCCESSORS)
          openlist[newListFront].push(st);
        added++;

        if (st->decRefCount())
          delete st;

        /* Fortran Nkeep: stop expanding this node after 100 new neighbours. */
        if (added >= MAX_SUCCESSORS)
          break;

        continue;
      }

      separation_c * res = checkSubproblems(st, pieces);

      if (st->decRefCount())
        delete st;

      return res;
    }

    if (openlist[curListFront].empty()) {

      curListFront = 1 - curListFront;
      newListFront = 1 - newListFront;

      closed[oldFront].clear();

      oldFront = curFront;
      curFront = newFront;
      newFront = (newFront + 1) % 3;
    }
  }

  return 0;
}
