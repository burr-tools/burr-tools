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


#ifndef __DISASSMTOMOVES_H__
#define __DISASSMTOMOVES_H__

/* this class takes a disassembly tree and generates relativ piecepositions
 * for all peaces at each step of disassembly
 */
#include <../lib/disassembly.h>

 
class DisasmToMoves {

  const disassembly_c * dis;
  float * moves;
  int piecenumber;

  int doRecursive(const separation_c * tree, int step, float weight, int cx, int cy, int cz, int remove);

public:

  DisasmToMoves(const disassembly_c * disasm, float * mv, int piecen) : dis(disasm), moves(mv), piecenumber(piecen) {}
  
  /* sets the moves for the step, if the value is not integer you
   * get a intermediate of the necessary move (for animation)
   */
  void setStep(float step);
};

#endif
