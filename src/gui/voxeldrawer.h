/* Burr Solver
 * Copyright (C) 2003-2008  Andreas Röver
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
#ifndef __VOXEL_DRAWER_H__
#define __VOXEL_DRAWER_H__

#include "voxeldrawer.h"

class voxel_c;

class voxelDrawer_c {

  public:

    voxelDrawer_c(void) {}
    virtual ~voxelDrawer_c(void) {}

    virtual void recalcSpaceCoordinates(float * x, float * y, float * z);

    virtual void drawCursor(const voxel_c * space, int mX1, int mX2, int mY1, int mY2, int mZ, int mode) = 0;


    virtual void calculateSize(const voxel_c * shape, float * x, float * y, float * z) = 0;

    virtual void drawFrame(const voxel_c * space, int x, int y, int z, float edge) = 0;
    virtual void drawNormalVoxel(const voxel_c * space, int x, int y, int z, float alpha, float edge) = 0;
    virtual void drawVariableMarkers(const voxel_c * space, int x, int y, int z) = 0;
};

/* some openGL helper functions for drawing some complexer things */
void drawGridTriangle(double x0, double y0, double z0,
    double v1x, double v1y, double v1z,
    double v2x, double v2y, double v2z, int diag);
void drawGridRect(double x0, double y0, double z0,
    double v1x, double v1y, double v1z,
    double v2x, double v2y, double v2z, int diag);
void drawTriangle(double x1, double y1, double z1,
    double x2, double y2, double z2,
    double x3, double y3, double z3, double px, double py, double pz);

/* checks, if the given coordinate xyz is inside the defined region. mode is a bitmask with
 * the bits defined in gridEditor_c
 */
bool inRegion(int x, int y, int z, int x1, int x2, int y1, int y2, int z1, int z2, int sx, int sy, int sz, int mode);

#endif
