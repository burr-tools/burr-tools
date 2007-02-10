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
#ifndef __PRINT_H__
#define __PRINT_H__

/* this module contains printing routines for the classes,
 * they print function is not part of the classes because they are not
 * required there
 * they are in fact quite interface specific
 */

class voxel_c;
class puzzle_c;
class assembly_c;
class separation_c;

void print(const voxel_c * v, char base = 'a');
void print(const puzzle_c * p);
void print(const assembly_c * a, const puzzle_c * p, unsigned int prob);
void print(const separation_c * s, const assembly_c * a, const puzzle_c * p, unsigned int prob);

#endif
