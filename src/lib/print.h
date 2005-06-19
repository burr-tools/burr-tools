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


/* this module contains printing routines for the classes,
 * they print function is not part of the classes bacause they are not
 * required there
 * they are in fact quite interface specific
 */

#include "voxel.h"
#include "puzzle.h"
#include "disassembly.h"

void print(const assemblyVoxel_c * v);
void print(const pieceVoxel_c * v);
void print(const voxel_c * v);
void print(const puzzle_c * p);
void print(const state_c * s, const assemblyVoxel_c *start, const separation_c * s, unsigned int piecenumber);
void print(const separation_c * s, const assemblyVoxel_c * start);
