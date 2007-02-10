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
#ifndef __DISASSEMBLER_H__
#define __DISASSEMBLER_H__

class separation_c;
class assembly_c;

/* this class implements a burr disassembler. The interface is simple:
 * 1) construct the class with whatever parameters the concrete subclass requires
 * 2) call diassemble for each assembly found and evaluate the result
 *
 * some subclasses may be able to handle several assemblies, others may only
 * disassembler one, that depends on the concrete disassembler you use
 */
class disassembler_c {

public:

  disassembler_c(void) {}
  virtual ~disassembler_c(void) {}

  /* because we can only have or don't have a disassembly sequence
   * we don't need the same complicated callback interface. The function
   * returns either the disassembly sequence or a null pointer.
   * you need to take care of freeing the disassembly sequence after
   * doing with it whatever you want
   */
  virtual separation_c * disassemble(const assembly_c * assembly) { return 0; }

};

#endif
