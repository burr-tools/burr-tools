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
#ifndef __VOXEL_3_H__
#define __VOXEL_3_H__

#include "voxel_0.h"

/**
 * Voxel class for the rhombic grid.
 *
 * This grid builds on top of the cube grid. The rhombic grid is emulated using
 * cubes. So this class uses many functions from the cubes voxel space.
 *
 * This class mainly adds alignment functionality for transformations
 * and the neighbor functionality.
 *
 * The rhombic grid emulation is explained in the user guide with some graphics,
 * here I can place only a textual description which might be hard to understand.
 *
 * It is possible to halve a cube in such a way that you get 2 prisms with a triangle
 * as base face. This triangle has a shape of an rectangular triangle with sides of
 * length 1, 1 and sqrt(2). There are 2*3 = 6 such cuts possible. If you do all those 6
 * cuts on a cube you get 24 little irregular but identical tetrahedras. Those are
 * the building blocks of the rhombic grid.
 *
 * The cube with its 24 tetrahedras is emulated in a 5x5x5 sized cube within the
 * cubic grid. The 24 tetrahedras are located as little diamonds on each of the six
 * faces of the 5x5x5 cube. That makes 6 faces with 4 cubes each = 24 voxels.
 *
 * Have a look at the user guide to see this and play with BurrTools to understand
 * the grid.
 *
 * With this grid it is possible to build rhombic dodecahedrons. The grid needs alignment
 * of 5 in each direction to ensure voxels don't change parity when the shape is rotated.
 * Also translations can only be done in multiples of 5 and so on...
 */
class voxel_3_c : public voxel_0_c {

  public:

    voxel_3_c(unsigned int x, unsigned int y, unsigned int z, const gridType_c * gt, voxel_type init = 0) : voxel_0_c(x, y, z, gt, init) {}
    voxel_3_c(xmlParser_c & pars, const gridType_c * gt) : voxel_0_c(pars, gt) {}
    voxel_3_c(const voxel_c & orig) : voxel_0_c(orig) { }
    voxel_3_c(const voxel_c * orig) : voxel_0_c(orig) { }

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
    void operator=(const voxel_3_c&);
};

#endif
