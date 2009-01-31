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
#ifndef __ASSEMBLER_FRONTENT_3_H__
#define __ASSEMBLER_FRONTENT_3_H__

#include "assembler.h"


/* the assembler frontend for the rhombic grid */
class assemblerFrontend_3_c : public assemblerFrontend_c {

public:

  bool pieceFits(int x, int y, int z) const;

  assemblerFrontend_3_c(void) {}

};

#endif
