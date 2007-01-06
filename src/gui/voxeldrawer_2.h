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
#ifndef __VOXEL_DRAWER_2_H__
#define __VOXEL_DRAWER_2_H__

#include "voxeldrawer.h"

class gridType_c;

class voxelDrawer_2_c : public voxelDrawer_c {

  private:

    const gridType_c * gt;

  public:

    voxelDrawer_2_c(int x, int y, int w, int h, const gridType_c * g) : voxelDrawer_c(x, y, w, h), gt(g) {}

    void drawShape(const shapeInfo * shape, colorMode colors);
    void drawCursor(unsigned int sx, unsigned int sy, unsigned int sz);

    void gridTypeChanged(void);

    void calculateSize(const voxel_c * shape, float * x, float * y, float * z);

    void drawFrame(const voxel_c * space, int x, int y, int z, float edge);
    void drawNormalVoxel(const voxel_c * space, int x, int y, int z, float alpha, float edge);
    void drawVariableMarkers(const voxel_c * space, int x, int y, int z);
};

#endif
