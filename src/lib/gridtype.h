/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
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

class assembler_c;
class symmetries_c;
class voxel_c;
class problem_c;
class stlExporter_c;
class movementCache_c;
class xmlWriter_c;
class xmlParser_c;

/**
 * This class encapsulates all information required to handle the different grid types.
 *
 * That means this class knows about the capabilities of the different grids. It
 * know how to create assembler disassembler, STL-exporter, or voxelspaces for the
 * different grid, ...
 *
 * This is a factory (see http://en.wikipedia.org/wiki/Factory_method_pattern)
 */
class gridType_c {

  public:

    /// the available grid types
    typedef enum {
      GT_BRICKS,                   ///< cubes
      GT_TRIANGULAR_PRISM,         ///< triangles stacked in Z-direction
      GT_SPHERES,                  ///< tightly packed spheres
      GT_RHOMBIC,                  ///< complicated cut cube to build rhombic dodecahedra
      GT_TETRA_OCTA,               ///< spacegrid for with tetrahedron and octrahera, also a cut cube

      GT_NUM_GRIDS                 ///< always the last entry, the number of different grids
    } gridType;

    /// capabilities of a given grid space
    typedef enum {
      CAP_ASSEMBLE = 1,            ///< the grid has an assembler
      CAP_DISASSEMBLE = 2,         ///< the grid has a disassembler
      CAP_STLEXPORT = 4            ///< the grid can export to SDL
    } capabilities;

  protected:

    /// the grid type of this instance
    gridType type;

    /**
     * The symmetry object for this grid type.
     *
     * As we only need one symmetry object for a grid type we create a local
     * instance here and just return a pointer to it for the application to
     * use
     */
    mutable symmetries_c * sym;

  public:

    /**
     * load from xml node
     */
    gridType_c(xmlParser_c & pars);

    /** copy constructor */
    gridType_c(const gridType_c&);

    /** used to save to XML */
    void save(xmlWriter_c & xml) const;

    /* some specialised constructors */

    /** create a cube grid */
    gridType_c(void);

    /** create a grid of the given type with its standard parameters */
    gridType_c(gridType gt);

    ~gridType_c(void);

    /** get the grid type */
    gridType getType(void) const { return type; }

    unsigned int getCapabilities(void) const;

    /// return a movement cache instance for this grid type
    movementCache_c * getMovementCache(const problem_c * puz) const;

    /// create a new voxel space of this grid type with the given dimensions
    voxel_c * getVoxel(unsigned int x, unsigned int y, unsigned int z, voxel_type init) const;
    /// create a new voxel space of this grid type, which is loaded from the XML node
    voxel_c * getVoxel(xmlParser_c & pars) const;
    /// create a new voxel space of this grid type, which is a copy of the given space
    voxel_c * getVoxel(const voxel_c & orig) const;
    /// create a new voxel space of this grid type, which is a copy of the given space
    voxel_c * getVoxel(const voxel_c * orig) const;

    /**
     * Return a pointer to the symmetry class for this grid.
     * Don't free this instance the ownership stays with the gridType_c class
     * You just use the class and forget about it
     */
    const symmetries_c * getSymmetries(void) const;

    /// return an STL exporter for this grid
    stlExporter_c * getStlExporter(void) const;

    /**
     * Find a suitable assembler for the given problem.
     * This function is different from the above, it is not dependent on the
     * gridtype of the puzzle but on some of the parameters of the puzzle, e.g
     * has the puzzle multipieces, has the puzzle piece count ranges, ...
     * the function tries to find the fastest assembler that can handle
     * the puzzle.
     * because we are not dependent on the gridtype this function is static
     * but it needs to know the puzzle
     */
    static assembler_c * findAssembler(const problem_c * p);

  private:

    // no copying and assigning
    void operator=(const gridType_c&);
};

#endif
