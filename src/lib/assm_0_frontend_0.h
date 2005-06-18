/* Burr Solver
 * Copyright (C) 2003-2005  Andreas Röver
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


#ifndef __ASSEMBLER_0_FRONTENT_0_H__
#define __ASSEMBLER_0_FRONTENT_0_H__

#include "assembler_0.h"

#include "voxel.h"
#include "puzzle.h"

class assm_0_frontend_0_c : public assembler_0_c {

private:

  /* voxelindex is the invers of the function column. it returns
   * the index (not x, y, z) of a given column in the matrix
   */
  int * voxelindex;

  /* get's called when a solution is found. this function
   * then assembles the solution inside assm and calles the
   * callback function with assm as parameter
   */
  bool solution(void);

  /* this function creates the matrix for the search function
   * because we need to know how many nodes we need to allocate the
   * arrays with the right size, we add a parameter. if this is true
   * the function will not access the array but only count the number
   * of nodes used. this number is returned
   */
  unsigned long countNodes(puzzle_c * puz, unsigned int resultnum);
  void prepare(puzzle_c * puz, int res_filles, int res_vari, unsigned int resultnum);

public:

  assm_0_frontend_0_c() : assembler_0_c() {}
  ~assm_0_frontend_0_c();
};

#endif
