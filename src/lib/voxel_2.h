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
#ifndef __VOXEL_2_H__
#define __VOXEL_2_H__

#include "voxel.h"

/**
 * Voxel class for the sphere grid.
 *
 * The sphere grid is something special. It is the only grid where
 * transformations can fail.
 *
 * The grid is done by placing the spheres in a 3-dimensional checker board
 * pattern. That results in a regular placement of spheres.
 *
 * The other possible embedding would be the hexagonal planes, but this
 * embedding is much more irregular.
 *
 * The sphere grid is a lot of transformations but depending on the actual
 * shape only some of them are actually doable, others would result in spheres
 * ending up outside the grid positions. Those transformations will not be
 * done.
 *
 * An other irregularity is the neighbor function, which only returns first
 * grade neighbors (face touching in the other grids). The other neighbors
 * don't make sense.
 */
class voxel_2_c : public voxel_c {

  public:

    voxel_2_c(unsigned int x, unsigned int y, unsigned int z, const gridType_c * gt, voxel_type init = 0) : voxel_c(x, y, z, gt, init) {}
    voxel_2_c(xmlParser_c & pars, const gridType_c * gt) : voxel_c(pars, gt) {}
    voxel_2_c(const voxel_c & orig) : voxel_c(orig) { }
    voxel_2_c(const voxel_c * orig) : voxel_c(orig) { }

    void transformPoint(int * x, int * y, int * z, unsigned int trans) const;
    bool transform(unsigned int nr);
    void minimizePiece(void);

    bool getNeighbor(unsigned int idx, unsigned int typ, int x, int y, int z, int * xn, int *yn, int *zn) const;

    void initHotspot(void);

    void resizeInclude(int & px, int & py, int & pz);

    virtual bool validCoordinate(int x, int y, int z) const;
    bool onGrid(int x, int y, int z) const;

  private:

    // no copying and assigning
    void operator=(const voxel_2_c&);
};

#endif
