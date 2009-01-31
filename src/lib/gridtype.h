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
#ifndef __GRID_TYPE_H__
#define __GRID_TYPE_H__

#include "types.h"

#include "bt_assert.h"

#include <xmlwrapp/node.h>

class assembler_c;
class assemblerFrontend_c;
class symmetries_c;
class voxel_c;
class problem_c;
class stlExporter_c;
class movementCache_c;

/* this class encapsulates all information required to handle the different grid types
 */
class gridType_c {

  public:

    typedef enum {
      GT_BRICKS,
      GT_TRIANGULAR_PRISM,
      GT_SPHERES,                  // tightly packed spheres
      GT_RHOMBIC,
    } gridType;

    /* capabilities of a given grid space */
    typedef enum {
      CAP_ASSEMBLE = 1,
      CAP_DISASSEMBLE = 2,
      CAP_STLEXPORT = 4
    } capabilities;

  protected:

    gridType type;

    union {

      /* the parameter for the GT_BRICKS type
       */
      struct {
        bool nothing;
      } brick;

      struct {
        bool nothgin;    // nothing for now
      } triangularPrism;

      struct {
        bool nothing;
      } spheres;

    } parameters;

    symmetries_c * sym;

  public:

    /**
     * load from xml node
     */
    gridType_c(const xml::node & node);

    /* used to save to XML */
    xml::node save(void) const;

    /* some specializes constructors */

    /* create a cube grid */
    gridType_c(void);

    /* create a grid of the given type with its standard parameters */
    gridType_c(gridType gt);

    ~gridType_c(void);

    /* get the grid type */
    gridType getType(void) const { return type; }

    unsigned int getCapabilities(void) const;

    /* these functions return assembler and disassemble for the current space grid
     * if the required functionality is not available, return 0
     */
    assemblerFrontend_c * getAssemblerFrontend(void) const;
    movementCache_c * getMovementCache(const problem_c * puz) const;

    /* voxel spaces have different implementations for rotation, and mirror functions */
    voxel_c * getVoxel(unsigned int x, unsigned int y, unsigned int z, voxel_type init) const;
    voxel_c * getVoxel(const xml::node & node) const;
    voxel_c * getVoxel(const voxel_c & orig) const;
    voxel_c * getVoxel(const voxel_c * orig) const;

    const symmetries_c * getSymmetries(void) const;

    stlExporter_c * getStlExporter(void) const;

    /* this function is different from the above, it is not dependend on the
     * gridtype of the puzzle but on some of the parameters of the puzzle, e.g
     * has the puzzle multipieces, has the puzzle piece count ranges, ...
     * the function tries to find the fastest assembler that can handle
     * the puzzle.
     * because we are not dependend on the gridtype this function is static
     * but it needs to know the puzzle
     */
    static assembler_c * findAssembler(const problem_c * p);
};



#endif
