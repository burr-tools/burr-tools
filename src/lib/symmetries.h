// this modules contains just some helper functions for transformations and symmetrie handling

#ifndef __SYMMETRIES_H__
#define __SYMMETRIES_H__

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
typedef __int64_t symmetries_t;
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

