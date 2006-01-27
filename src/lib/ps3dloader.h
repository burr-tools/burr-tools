/* Burr Solver
 * Copyright (C) 2003-2006  Andreas Röver
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


/* this module just contains a function that tries to load puzzles saved with the
 * puzzle solver 3d
 */

#ifndef __PS3DLOADER_H__
#define __PS3DLOADER_H__

#include "puzzle.h"

#include <iostream>

/* either return a puzzle, or nil, when failed */
puzzle_c * loadPuzzlerSolver3D(std::istream * str);


#endif
