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


// this modules contains just some helper functions for transformations and symmetrie handling

#ifndef __SYMMETRIES_H__
#define __SYMMETRIES_H__

#define NUM_TRANSFORMATIONS 24
#define NUM_TRANSFORMATIONS_MIRROR 48

/* one piece can have 48 symmetries. 24 rotational and another 24 rotational with mirroring.
 * the mirroring is possible to avoid finding mirrored solutions.
 * All the symmetries are enumbered. The first 24 are the not mirrored. the other 24 are
 * first mirrored along the x axins (-x -> x). The the piece is rotated around the x axis by
 * the value contained inside this array at the position of the symmetry number or symmetry number
 * minus 24. The the piece is rotated around y and finally around z.
 * If the resulting piece is identical to the original untransformed than the piece has the
 * symmetry or the given rotation number
 */
int rotx(unsigned int p);
int roty(unsigned int p);
int rotz(unsigned int p);

/* this type is used to collect all the symmetries that a piece can have. For each symmetry
 * the corresponding bit is set
 */
#ifdef WIN32
typedef unsigned long long symmetries_t;
#else
#include <sys/types.h>
typedef u_int64_t symmetries_t;
#endif

/* this return true, if the symmetry contains the given transformation */
bool symmetrieContainsTransformation(symmetries_t s, unsigned int t);

/* returns the number of rotations that are contained in this symmetry */
unsigned int numSymmetries(symmetries_t s);

/* this function returns a new symmetry class that contains both symmetries
 * at once
 */
symmetries_t multiplySymmetries(symmetries_t s1, symmetries_t s2);

#endif

