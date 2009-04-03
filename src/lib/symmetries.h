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
#ifndef __SYMMETRIES_H__
#define __SYMMETRIES_H__

#include <inttypes.h>

// this modules contains just some helper functions for transformations and symmetry handling

/// this define is used for undefined transformations in the transformation multiplication matrix
#define TND (unsigned char)(-1)

/** \page transformationDetails Transformations and Symmetries in BurrTools
 *
 * A transformation rotates and mirrors a shape inside its spacegrid. That means the
 * transformed shapes again resides completely within the grid. Transformations are
 * normally completely opaque to the user. They are simply a number that corresponds
 * to one posible rotation. Some things are rules though
 *
 * - The lower transformation numbers don't contains mirror transformations
 * - The upper transformations all contain a mirror
 *
 * BurrTools uses a concept that I call Symmetries. Throughout the sources the word
 * symmetry can mean either of 2 possible things:
 *
 * - A transformation that will reorient a shape onto itself
 * - or A list of all transformations that with the above property, sometimes refered to as symmetry group
 *
 * For each shape we can calculate such a list of transformations. There are some
 * things to keep in mind:
 *
 * - when you transform the shape the list may change. That is the symmetry list is not
 *   orientation independent
 * - BurrTools doesn't handle the list, but it knows all possible symmetry lists and has
 *   enumbered them and only uses those numbers wherever it is possible
 */


/**
 * This type holds the symmetry list entry number. Right now spheres are the greatest user
 * with 241 entries
 */
typedef uint8_t symmetries_t;

/* some macros for the symmeties_t type */

/**
 * find out if a piece with the symmetry group s is completely unsymmetric (a bit like prime)
 */
#define unSymmetric(s) ((s) == 0)

/**
 * There is a value for an invalid symmetry group that can be used to signify uncalculated
 * symmetry groups
 */
#define symmetryInvalid() (0xFF)

/**
 * check, if the given symmetry group is the invalid symmetry group
 */
#define isSymmetryInvalid(s) ((s) == 0xFF)

class voxel_c;

/**
 * this class contains all kinds of functions to handle symmetry groups
 *
 * This is an absract base class only the derived classes actually do something.
 * If you need an instance of this class you should always use the corresponding
 * gridType_c function
 */
class symmetries_c {

  public:

    symmetries_c(void) {}
    virtual ~symmetries_c(void) {}

    /**
     * Get the number of possible transformations for a voxel space.
     * This number does not include mirror transformations
     */
    virtual unsigned int getNumTransformations(void) const = 0;

    /**
     * Get the number of possible transformations for a voxel space.
     * This time the mirror transformations are included. Normally this value
     * is twice the number you get when calling getNumTransformations
     */
    virtual unsigned int getNumTransformationsMirror(void) const = 0;

    /**
     * This return true, if the symmetry contains the given transformation
     *
     * A a symmetry group contains a list of transformations. This functions
     * tells you whether the given symmetry group s contains transformation t
     */
    virtual bool symmetrieContainsTransformation(symmetries_t s, unsigned int t) const = 0;

    /**
     * When you apply all the transformations to the shape with a given symmetry
     * s for which this function returns true, you get all possible
     * orientations that that shape can have
     */
    virtual bool isTransformationUnique(symmetries_t s, unsigned int t) const = 0;

    /**
     * returns the transformation that results, when you first carry out transformation t1
     * and then transformation t2
     *
     * the function might return TND, when the new required transformation doesn't exist
     */
    virtual unsigned char transAdd(unsigned char t1, unsigned char t2) const = 0;

    /**
     * Find the first transformation, for a shape with the given symmetry group that
     * results in a shape identical to the given transformation.
     *
     * This functionality is used when rotating an assembly. The new transformation is
     * found out by using transAdd but then we might end up with a high transformation
     * number even though the symmetries of the shape would say otherwise.
     *
     * An example. Suppose you have a cube in orientation 0. Now you transform that
     * cube by transformation 1 resulting in orientation 1 for the cube. But a cube in
     * orientation 1, well in _any_ orientation looks identical so we can just as well
     * say we leave the cube in orientation 0
     */
    virtual unsigned char minimizeTransformation(symmetries_t s, unsigned char trans) const = 0;

    /**
     * This counts how much 'overlap' there is in symmetry between the given result and the shape.
     * The value is small, when the 2 shapes have less symmetries in common
     */
    virtual unsigned int countSymmetryIntersection(symmetries_t resultSym, symmetries_t s2) const = 0;

    /**
     * Do the result and the shape share common symmetries?
     * This is a bit like countSymmetryIntersection() > 1, only faster
     */
    virtual bool symmetriesLeft(symmetries_t resultSym, symmetries_t s2) const = 0;

    /**
     * returns true, if the symmetry group given contains a mirror transformation, meaning
     * the shape that symmetry belongs to can be mirrored by using just rotations
     */
    virtual bool symmetryContainsMirror(symmetries_t sym) const = 0;

    /** calculate the symmetry group for the given voxel space */
    virtual symmetries_t calculateSymmetry(const voxel_c * pp) const = 0;

    /**
     * find out if the shape has an unknown symmetry group. This was especially useful
     * when this feature with the symmetry groups was introduced and not all of the
     * symmetry groups were known. Nowadays there should be very little chance
     * of finding new symmetry groups. I still leave this extra check in just in case
     */
    virtual bool symmetryKnown(const voxel_c * pp) const = 0;

  private:

    // no copying and assigning
    symmetries_c(const symmetries_c&);
    void operator=(const symmetries_c&);
};

#endif
