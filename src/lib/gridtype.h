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
#ifndef __GRID_TYPE_H__
#define __GRID_TYPE_H__

#include "types.h"

#include <xmlwrapp/node.h>

class assembler_0_c;
class disassembler_c;
class symmetries_c;
class voxel_c;
class puzzle_c;

/* this class encapsulates all information required to handle the different grid types
 */
class gridType_c {

  public:

    typedef enum {
      GT_BRICKS,
//      GT_TRIANGULAR_PRISM,
//      GT_RHOMBIC_DODECAEDER,
    } gridType;

  protected:

    gridType type;

    union {

      /* the parameter for the GT_BRICKS type
       */
      struct {
        bool y_differs_x;  // length of y-axis is different from x axis
        bool z_differs_x;
        bool z_differs_y;

        bool axy_ortho;    // y axis is in a rectangle to x axis
        bool axz_ortho;
        bool ayz_ortho;

        bool axy_differs_axz;  // the angle between axes x and y is different from the angle between axes x and z
        bool axy_differs_ayz;
        bool ayz_differs_ayz;
      } brick;

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

    /* get the grid type */
    gridType getType(void) { return type; }

    /* these functions return assembler and disassemble for the current space grid
     * if the requied functionality is not available, return 0
     */
    assembler_0_c * getAssembler(void) const;
    disassembler_c * getDisassembler(const puzzle_c * puz, unsigned int prob) const;

    /* voxel spaces have different implementatios for rotation, and mirror functions */
    voxel_c * getVoxel(unsigned int x, unsigned int y, unsigned int z, voxel_type init, voxel_type outs) const;
    voxel_c * getVoxel(const xml::node & node) const;
    voxel_c * getVoxel(const voxel_c & orig, unsigned int transformation = 0) const;
    voxel_c * getVoxel(const voxel_c * orig, unsigned int transformation = 0) const;

    const symmetries_c * getSymmetries(void) const;

    /* sometimes it might be possible to convert from the current grid
     * to anothe e.g. hexagonal to triangular prisms
     */
#if 0
    converter_c * getConveter(gridType target);
#endif
};



#endif