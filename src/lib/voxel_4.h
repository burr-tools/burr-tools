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
#ifndef __VOXEL_4_H__
#define __VOXEL_4_H__

#include "voxel_0.h"

/**
 * voxel space class for tetra-octa grid.
 *
 * This builds on top of the brick grid as this grid is implemented
 * using the normal bricks. So this class can use many of the brick
 * functions it just has to ensure certain alignments.
 *
 * The emulation is working like this: Imagine a tetrahedron inscribed
 * into a cube so that 2 corners of the tetrahedron are in the opposite
 * 2 top corners of the cube and the other 2 corners of the tetrahedron
 * are in the other bottom 2 corners. This leaves 4 little irregular
 * tetrahedrons to complete the cube. 4 of those smaller tetrahedron
 * can be assembled into an octahedron.
 *
 * There are 2 possible orientations for this cube. The complete
 * space is filled with a checkerboard pattern of those 2 cubes.
 *
 * Each of these cubes is represented as one 3x3x3 cube in the normal
 * cube grid. The regular tetrahedron is the centre cube and the 4
 * irregular tetrahedrons are represented by voxels in the corresponding
 * corners of the 3x3x3 cube
 *
 * To better understand this grid, it is best best is to play a bit
 * with it in the GUI.
 */
class voxel_4_c : public voxel_0_c {

  public:

    voxel_4_c(unsigned int x, unsigned int y, unsigned int z, const gridType_c * gt, voxel_type init = 0) : voxel_0_c(x, y, z, gt, init) {}
    voxel_4_c(xmlParser_c & pars, const gridType_c * gt) : voxel_0_c(pars, gt) {}
    voxel_4_c(const voxel_c & orig) : voxel_0_c(orig) { }
    voxel_4_c(const voxel_c * orig) : voxel_0_c(orig) { }

    virtual bool transform(unsigned int nr);

    bool getNeighbor(unsigned int idx, unsigned int typ, int x, int y, int z, int * xn, int *yn, int *zn) const;

    void scale(unsigned int amount, bool grid);
    bool scaleDown(unsigned char by, bool action);
    void resizeInclude(int & px, int & py, int & pz);
    void minimizePiece(void);

    virtual bool validCoordinate(int x, int y, int z) const;
    bool identicalInBB(const voxel_c * op, bool includeColors = true) const;
    bool onGrid(int x, int y, int z) const;

    virtual bool meshParamsValid(double bevel, double offset) const;
    void getConnectionFace(int x, int y, int z, int n, double bevel, double offset, std::vector<float> & faceCorners) const;
    void calculateSize(float * x, float * y, float * z) const;
    void recalcSpaceCoordinates(float * x, float * y, float * z) const;

  private:

    // no copying and assigning
    void operator=(const voxel_4_c&);
};

#endif
