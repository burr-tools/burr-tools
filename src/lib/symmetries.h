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


// this modules contains just some helper functions for transformations and symmetrie handling

#ifndef __SYMMETRIES_H__
#define __SYMMETRIES_H__

#define NUM_TRANSFORMATIONS 24
#define NUM_TRANSFORMATIONS_MIRROR 48

class voxel_c;

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
typedef unsigned char symmetries_t;

/* this return true, if the symmetry contains the given transformation */
bool symmetrieContainsTransformation(symmetries_t s, unsigned int t);

unsigned char transAdd(unsigned char t1, unsigned char t2);

/* find the first transformation, for a shape with the given transformation that
 * retults in a shape identical to the given transformation
 */
unsigned char minimizeTransformation(symmetries_t s, unsigned char trans);

#define unSymmetric(s) ((s) == 0)

unsigned int countSymmetryIntersection(symmetries_t s1, symmetries_t s2);
bool symmetriesLeft(symmetries_t resultSym, symmetries_t s2);

#define symmetryInvalid() (0xFF)

#define isSymmetryInvalid(s) ((s) == 0xFF)

symmetries_t symmetryCalcuation(const voxel_c * pp);

#endif

