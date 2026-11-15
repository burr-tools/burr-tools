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
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef __LATTICEGRIDINFO_H__
#define __LATTICEGRIDINFO_H__

#include "gridtype.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

/**
 * What the described grids (BurrTools NG's grids as data, see
 * design/2026-11-19-ng-merge-plan.md, step G3) need from NG's grid model,
 * in this code base's terms: dense transformation numbers, integer maps,
 * the product table, and the geometry for drawing.
 *
 * Coordinates: a voxel space of a described grid uses NG's voxel
 * coordinates directly, up to a lattice translation. Every operation that
 * moves the contents of a space (transform, minimizePiece, resizeInclude)
 * moves them by a lattice vector only, so whether a cell is a voxel, and
 * which kind, never depends on where a shape sits in its space.
 *
 * Transformations: numbers 0 .. n-1 are the grid's rotations, in NG's
 * order (0 is the identity), n .. 2n-1 the same rotations followed by NG's
 * mirror (a point reflection), as our native grids order theirs.
 *
 * One instance per described grid type, built on first use and never
 * changed; safe to read from any thread.
 */
class latticeGridInfo_c {

  public:

    /// the info for a described grid type; throws for any other type
    static const latticeGridInfo_c & get(gridType_c::gridType type);

    ~latticeGridInfo_c();

    /// NG's storage id of the grid
    size_t ngId(void) const { return ngGrid; }

    /// rotations without mirror (getNumTransformations)
    unsigned int rotations(void) const { return numRot; }
    /// all transformations (getNumTransformationsMirror)
    unsigned int transformations(void) const { return 2*numRot; }

    /// NG's rotation number of one of ours
    int ngRotation(unsigned int t) const { return trans[t].ng; }

    /// is (x, y, z) a voxel of the grid
    bool isVoxel(int x, int y, int z) const;
    /// the kind of voxel at (x, y, z) (NG's base position index), -1 if none
    int base(int x, int y, int z) const;
    /// column j of the lattice: the translations are its integer combinations
    void latticeColumn(int j, int & x, int & y, int & z) const { x = L[j]; y = L[3+j]; z = L[6+j]; }

    /// is (x, y, z) a translation that maps the grid onto itself
    bool isLatticeVector(int x, int y, int z) const;

    /**
     * Apply transformation t to a voxel position: rotate (and mirror)
     * about the origin, then the transformation's shift. Returns false
     * when the result is not a whole-numbered point, which happens only
     * for points that are not voxels.
     */
    bool apply(unsigned int t, int & x, int & y, int & z) const;

    /**
     * The lattice vector v to subtract from a set of positions whose
     * smallest coordinates are (mx, my, mz) so that all of them become
     * non negative and as small as possible.
     */
    void alignDown(int mx, int my, int mz, int & vx, int & vy, int & vz) const;

    /// first t1, then t2; TND when that is not one of the grid's transformations
    unsigned char product(unsigned char t1, unsigned char t2) const { return mult[t1 * transformations() + t2]; }

    /// the linear part of t in physical space, row major
    void physicalMatrix(unsigned int t, double m[9]) const;

    /// physical position of the voxel coordinates (x, y, z), linear part only
    void toPhysical(float & x, float & y, float & z) const;

    /// how far a voxel's polyhedron reaches past toPhysical() of its position, per axis
    void voxelReach(float lo[3], float hi[3]) const { for (int k = 0; k < 3; k++) { lo[k] = reachLo[k]; hi[k] = reachHi[k]; } }

    /// number of faces (= face neighbours) of the voxel kind at (x, y, z)
    unsigned int faces(int x, int y, int z) const;

    /// the face neighbour n of (x, y, z); false if there is no such face
    bool faceNeighbour(int x, int y, int z, unsigned int n, int & xn, int & yn, int & zn) const;

    /// edge (typ 1) and corner (typ 2) neighbours, without the face neighbours and without (x, y, z)
    const std::vector<std::array<int, 3>> & otherNeighbours(int base, unsigned int typ) const { return typ == 1 ? edgeNb[base] : cornerNb[base]; }

    /**
     * Corners of face n of the voxel at (x, y, z) in physical
     * coordinates, moved for bevel and offset as NG draws them.
     */
    void faceCorners(int x, int y, int z, unsigned int n, double bevel, double offset, std::vector<float> & out) const;

    /// movement directions (NG's movement vectors; the opposite ones are implied)
    unsigned int directions(void) const { return (unsigned int)dirs.size(); }
    const std::array<int, 3> & direction(unsigned int d) const { return dirs[d]; }
    /// the cells, relative to a voxel of kind `base`, that must be empty for it to move one step in direction d
    const std::vector<std::array<int, 3>> & moveChecks(int base, unsigned int d) const { return checks[base][d]; }
    /// how far a check reaches from its voxel, at most, in any coordinate
    int checkReach(void) const { return reach; }
    /**
     * whether the disassembler can work on this grid: it has movement
     * directions, and each is a lattice vector, so moved pieces stay a
     * lattice vector apart
     */
    bool canDisassemble(void) const { return disassemble; }

    /// symmetry groups: NG's group numbers (0..135), as our bitsets of transformations; 0 for one not on this grid
    uint64_t groupSet(unsigned int g) const { return groups[g]; }
    unsigned int numGroups(void) const { return (unsigned int)groups.size(); }

  private:

    explicit latticeGridInfo_c(gridType_c::gridType type);

    size_t ngGrid;
    unsigned int numRot = 0;

    struct trans_s {
      int ng;          // NG's rotation number
      int den;         // common denominator of m and t
      int m[9];        // den * rotation, row major
      int t[3];        // den * shift
      double phys[9];  // the rotation in physical space
    };
    std::vector<trans_s> trans;

    std::vector<unsigned char> mult;
    std::vector<uint64_t> groups;

    // lattice (columns) and voxel to physical
    int L[9];
    float v2p[9];
    float reachLo[3] = {0, 0, 0}, reachHi[3] = {0, 0, 0};

    std::vector<std::vector<std::array<int, 3>>> edgeNb, cornerNb;

    std::vector<std::array<int, 3>> dirs;
    std::vector<std::vector<std::vector<std::array<int, 3>>>> checks;
    int reach = 0;
    bool disassemble = false;

    // NG's grid, kept as an opaque pointer so this header needs none of NG's
    struct ng_s;
    std::unique_ptr<const ng_s> ng;

    latticeGridInfo_c(const latticeGridInfo_c &) = delete;
    latticeGridInfo_c & operator=(const latticeGridInfo_c &) = delete;
};

#endif
