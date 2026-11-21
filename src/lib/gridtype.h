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
#include "solver_config.h"
#include "cutcubegrids.h"
#include "solver/math.h"
#include "solver/shape.h"

#include <memory>
#include <mutex>
#include <optional>
#include <vector>

class assembler_c;
class symmetries_c;
class voxel_c;
struct solverSpec_t;
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
      GT_TRIANGULAR_PRISM,         ///< triangles stacked in Z-direction: read only, converted to GT_DESCRIBED_TRIANGULAR_PRISM (C3)
      GT_SPHERES,                  ///< tightly packed spheres
      GT_RHOMBIC,                  ///< complicated cut cube to build rhombic dodecahedra
      GT_TETRA_OCTA,               ///< spacegrid for with tetrahedron and octrahera, also a cut cube

      /* Described grids: BurrTools NG's grids as data (src/lib/latticegrid,
       * design/2026-11-19-ng-merge-plan.md), one value per grid. Each maps to
       * NG's storage id in latticeGridInfo_c. GT_DESCRIBED_BRICKS, a copy
       * of a native grid, exists to cross-check the described grids against
       * the native ones; the GUIs do not offer it. */
      GT_HEXAGONAL_PRISM,          ///< hexagons stacked in Z-direction (NG grid 3)
      GT_CAIRO_PRISM,              ///< Cairo pentagons stacked in Z-direction (NG grid 5)
      GT_RHOMBIC_TETRAHEDRA,       ///< NG's rhombic grid, each of our rhombic voxels is two of these (NG grid 1)
      GT_OCTAHEDRAL_TETRAHEDRA,    ///< NG's tetrahedra and octahedra, the cells of GT_TETRA_OCTA (NG grid 2)
      GT_DESCRIBED_BRICKS,         ///< cubes as a described grid, for cross-checks (NG grid 0)
      GT_DESCRIBED_TRIANGULAR_PRISM, ///< triangular prisms (NG grid 4), the grid of every triangular prism puzzle since C3

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
    mutable std::unique_ptr<symmetries_c> sym;
    /** creates sym once (getSymmetries() may run on several threads); a
     * copy gets a fresh flag, as it gets no sym */
    mutable std::once_flag symOnce;

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

    /** is this one of the described grids (NG's grids as data) */
    static bool isDescribed(gridType t) { return t >= GT_HEXAGONAL_PRISM && t < GT_NUM_GRIDS; }
    bool isDescribed(void) const { return isDescribed(type); }

    /** the grids a user can create a puzzle on in the GUIs: bricks,
     *  spheres, and triangular, hexagonal and Cairo prisms (NG's grids,
     *  which every GUI edits); rhombic and tetra-octa on the grids `cut`
     *  says (the FLTK GUI keeps the native ones). The cross-check grid
     *  GT_DESCRIBED_BRICKS is never offered. */
    static bool offeredForNewPuzzles(gridType t, cutCubeGrids_e cut = cutCubeGrids_e::ng) {
      if (t == GT_BRICKS || t == GT_SPHERES || t == GT_DESCRIBED_TRIANGULAR_PRISM ||
          t == GT_HEXAGONAL_PRISM || t == GT_CAIRO_PRISM) return true;
      if (cut != cutCubeGrids_e::ng) return t == GT_RHOMBIC || t == GT_TETRA_OCTA;
      return t == GT_RHOMBIC_TETRAHEDRA || t == GT_OCTAHEDRAL_TETRAHEDRA;
    }

    unsigned int getCapabilities(void) const;

    /// return a movement cache instance for this grid type
    std::unique_ptr<movementCache_c> getMovementCache(std::shared_ptr<const solverSpec_t> puz) const;

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

    /** \name Cell-address (n, k) and placement operations (B6) */
    //@{
    /** Number of cell kinds K inside one repeating lattice cell (k in [0, K)). */
    unsigned int kindCount(void) const;

    /** Exact bijection from cell address (n, k) to the grid's voxel coordinates. */
    vec3i_t cellToVoxel(cellAddr_t c) const;

    /** Exact inverse from voxel coordinates to (n, k); nullopt if not a valid cell. */
    std::optional<cellAddr_t> voxelToCell(vec3i_t v) const;

    /** Convert a lattice translation vector dn to a voxel-coordinate delta (always onGrid). */
    vec3i_t latticeToVoxelDelta(vec3i_t dn) const;

    /** Exact inverse from a voxel-coordinate delta to a lattice vector; nullopt if not onGrid. */
    std::optional<vec3i_t> voxelDeltaToLattice(vec3i_t dv) const;

    /**
     * Transform a single cell (n, k) by grid turn g (0 <= g < numTransformationsMirror).
     * Always succeeds for periodic grids; for spheres, returns nullopt when
     * the rotated cell does not land on the FCC packing relative to the origin.
     */
    std::optional<cellAddr_t> transformCell(cellAddr_t c, unsigned int turn) const;

    /**
     * Transform a set of cells by grid turn g in place. For spheres, turns
     * about the first cell so any valid shape rotation succeeds regardless of
     * where the shape sits in space; returns false if any cell leaves the grid.
     */
    bool transformCells(std::vector<cellEntry_t> & cells, unsigned int turn) const;

    /**
     * Transform an arbitrary grid voxel point p relative to a reference voxel
     * point ref by grid turn g, returning T_g(p) - T_g(ref) in voxel coordinates.
     * Used to map legacy hotspots across turns identically to voxel_c::transform.
     */
    std::optional<vec3i_t> transformVoxelOffset(vec3i_t p, vec3i_t ref, unsigned int turn) const;

    /** Physical 3x3 rotation/mirror matrix for turn g. */
    mat3d_t physicalTransformMatrix(unsigned int turn) const;
    //@}

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
    static std::unique_ptr<assembler_c> findAssembler(std::shared_ptr<const solverSpec_t> p, const solverSettings_t & settings);

  public:

    // no assigning
    gridType_c & operator=(const gridType_c &) = delete;
};

#endif
