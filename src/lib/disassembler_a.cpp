/* Burr Solver
 * Copyright (C) 2003-2007  Andreas Röver
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
#include "puzzle.h"
#include "grouping.h"
#include "disassemblernode.h"
#include "movementanalysator.h"
#include "assembly.h"

disassembler_a_c::disassembler_a_c(const puzzle_c * puz, unsigned int prob) :
  disassembler_c(), puzzle(puz), problem(prob) {

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

  analyse = new movementAnalysator_c(puzzle, prob);
}

disassembler_a_c::~disassembler_a_c() {
  delete groups;
  delete [] piece2shape;

  delete analyse;
}

/* create all the necessary parameters for one of the two possible subproblems
 * our current problems divides into
 */
void create_new_params(disassemblerNode_c * st, disassemblerNode_c ** n, std::vector<unsigned int> & pn, const std::vector<unsigned int> & pieces, int part, bool cond) {

  *n = new disassemblerNode_c(part, 0, 0, 0);

  int num = 0;
  int dx, dy, dz;

  dx = dy = dz = 0;

  for (unsigned int i = 0; i < pieces.size(); i++)
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
      pn.push_back(pieces[i]);
      num++;
    }

  bt_assert(num == part);
}

unsigned short disassembler_a_c::subProbGroup(disassemblerNode_c * st, const std::vector<unsigned int> & pn, bool cond) {

  unsigned short group = 0;

  for (unsigned int i = 0; i < pn.size(); i++)
    if (st->is_piece_removed(i) == cond)
      if (puzzle->probGetShapeGroupNumber(problem, piece2shape[pn[i]]) != 1)
        return 0;
      else if (group == 0)
        group = puzzle->probGetShapeGroup(problem, piece2shape[pn[i]], 0);
      else if (group != puzzle->probGetShapeGroup(problem, piece2shape[pn[i]], 0))
        return 0;

  return group;
}

bool disassembler_a_c::subProbGrouping(const std::vector<unsigned int> & pn) {

  groups->newSet();

  for (unsigned int i = 0; i < pn.size(); i++)
    if (!groups->addPieceToSet(piece2shape[pn[i]]))
      return false;

  return true;
}

void disassembler_a_c::prepareForAssembly(const assembly_c * assm) {

  bt_assert(puzzle->probPieceNumber(problem) == assm->placementCount());
  analyse->prepareAssembly(assm);
  groups->reSet();
}

