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
#ifndef __VOXEL_DRAWER_A_H__
#define __VOXEL_DRAWER_A_H__

#include "voxeldrawer.h"

/* voxeldrawer base class for tetrahedron-based voxel drawer
 * this class contains pretty generic edge-detection code for those grids
 * all that is needed is to provide the proper coordinates of the voxels
 */
class voxelDrawer_a_c : public voxelDrawer_c {

  public:

    voxelDrawer_a_c(void) {}

    void drawFrame(const voxel_c * space, int x, int y, int z, float edge);
    void drawNormalVoxel(const voxel_c * space, int x, int y, int z, float alpha, float edge);
    void drawVariableMarkers(const voxel_c * space, int x, int y, int z);
    void drawCursor(const voxel_c * space, int mX1, int mX2, int mY1, int mY2, int mZ, int mode);

  private:

    /* you need to provide the proper coordinated for the voxel
     * at position x;y;z. If there is no voxel at those coordinated
     * retzrn false
     */
    virtual bool getTetrahedron(int x, int y, int z,
        int *x1, int *y1, int *z1,
        int *x2, int *y2, int *z2,
        int *x3, int *y3, int *z3,
        int *x4, int *y4, int *z4) = 0;

    bool getEdgeFaces(int x, int y, int z, int xa, int ya, int za, int xb, int yb, int zb, int *f1, int *f2);
    int getEdgeNeighbors(const voxel_c * space, int x, int y, int z, int face, int edge, int vx[8*3]);
    bool edgeVisible(const voxel_c * space, int x, int y, int z, int face, int edge);

};

#endif
