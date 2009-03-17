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
#include "disassembler_a.h"

#include "bt_assert.h"
#include "problem.h"
#include "grouping.h"
#include "disassemblernode.h"
#include "movementanalysator.h"
#include "assembly.h"
#include "disassembly.h"

disassembler_a_c::disassembler_a_c(const problem_c * puz) :
  disassembler_c(), puzzle(puz) {

  /* initialize the grouping class */
  groups = new grouping_c();
  for (unsigned int i = 0; i < puz->shapeNumber(); i++)
    for (unsigned int j = 0; j < puz->getShapeGroupNumber(i); j++)
      groups->addPieces(puz->getShape(i),
                        puz->getShapeGroup(i, j),
                        puz->getShapeGroupCount(i, j));

  /* initialize piece 2 shape transformation */
  piece2shape = new unsigned short[puz->pieceNumber()];
  int p = 0;
  for (unsigned int i = 0; i < puz->shapeNumber(); i++)
    for (unsigned int j = 0; j < puz->getShapeMax(i); j++)
      piece2shape[p++] = i;

  analyse = new movementAnalysator_c(puzzle);
}

disassembler_a_c::~disassembler_a_c() {
  delete groups;
  delete [] piece2shape;

  delete analyse;
}

/* create all the necessary parameters for one of the two possible subproblems
 * our current problems divides into
 */
void create_new_params(const disassemblerNode_c * st, disassemblerNode_c ** n, std::vector<unsigned int> & pn, const std::vector<unsigned int> & pieces, int part, bool cond) {

  *n = new disassemblerNode_c(part);

  int num = 0;
  int dx, dy, dz;

  dx = dy = dz = 0;

  for (unsigned int i = 0; i < pieces.size(); i++)
    if (st->is_piece_removed(i) == cond) {
      // we take the data from the node before the current, because the current node contains
      // the disassembled positions, which are not interesting, we want to have the position
      // before the puzzle falls apart but just take the halve of the pieces that matter
      (*n)->set(num,
                st->getComefrom()->getX(i),
                st->getComefrom()->getY(i),
                st->getComefrom()->getZ(i),
                st->getComefrom()->getTrans(i));
      pn.push_back(pieces[i]);
      num++;
    }

  bt_assert(num == part);
}

separation_c * disassembler_a_c::checkSubproblem(int pieceCount, const std::vector<unsigned int> & pieces, const disassemblerNode_c * st, bool left, bool * ok) {

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

    if (n->decRefCount())
      delete n;

    *ok = res || subProbGrouping(pn);
  }

  return res;
}

separation_c * disassembler_a_c::checkSubproblems(const disassemblerNode_c * st, const std::vector<unsigned int> &pieces) {

  /* if we get here we have found a node that separated the puzzle into
   * 2 pieces. So we recursively solve the subpuzzles and create a tree
   * with them that needs to be returned
   */
  separation_c * erg = 0;

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

    const disassemblerNode_c * st2 = st;

    do {
      state_c *s = new state_c(pieces.size());

      for (unsigned int i = 0; i < pieces.size(); i++)

        if (st2->is_piece_removed(i)) {

          /* when the piece is removed in here there must be a
           * predecessor node
           */
          bt_assert(st2->getComefrom());

          s->set(i, st2->getComefrom()->getX(i) + 20000*st2->getX(i),
              st2->getComefrom()->getY(i) + 20000*st2->getY(i),
              st2->getComefrom()->getZ(i) + 20000*st2->getZ(i));

        } else
          s->set(i, st2->getX(i), st2->getY(i), st2->getZ(i));

        erg->addstate(s);

        st2 = st2->getComefrom();
    } while (st2);

  } else {

    /* one of the subproblems was unsolvable in this case the whole
     * puzzle is unsolvable, so we can as well stop here
     */
    if (left) delete left;
    if (remove) delete remove;
  }

  return erg;
}

unsigned short disassembler_a_c::subProbGroup(const disassemblerNode_c * st, const std::vector<unsigned int> & pn, bool cond) {

  unsigned short group = 0;

  for (unsigned int i = 0; i < pn.size(); i++)
  {
    if (st->is_piece_removed(i) == cond)
    {
      if (puzzle->getShapeGroupNumber(piece2shape[pn[i]]) != 1)
      {
        return 0;
      }
      else if (group == 0)
      {
        group = puzzle->getShapeGroup(piece2shape[pn[i]], 0);
      }
      else if (group != puzzle->getShapeGroup(piece2shape[pn[i]], 0))
      {
        return 0;
      }
    }
  }

  return group;
}

bool disassembler_a_c::subProbGrouping(const std::vector<unsigned int> & pn) {

  groups->newSet();

  for (unsigned int i = 0; i < pn.size(); i++)
    if (!groups->addPieceToSet(piece2shape[pn[i]]))
      return false;

  return true;
}

separation_c * disassembler_a_c::disassemble(const assembly_c * assembly) {

  bt_assert(puzzle->pieceNumber() == assembly->placementCount());
  groups->reSet();

  disassemblerNode_c * start = new disassemblerNode_c(assembly);

  if (start->getPiecenumber() < 2) {
    delete start;
    return 0;
  }

  /* create pieces field. This field contains the
   * names of all present pieces. Because at the start
   * all pieces are still there we fill the array
   * with all the numbers
   */
  std::vector<unsigned int> pieces;
  for (unsigned int j = 0; j < assembly->placementCount(); j++)
    if (assembly->isPlaced(j))
      pieces.push_back(j);

  separation_c * s = disassemble_rec(pieces, start);

  if (start->decRefCount())
    delete start;

  return s;
}

