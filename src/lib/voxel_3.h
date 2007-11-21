/* Burr Solver
 * Copyright (C) 2003-2007  Andreas Röver
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

#include "voxel.h"

class voxel_3_c : public voxel_c {

  public:

    voxel_3_c(unsigned int x, unsigned int y, unsigned int z, const gridType_c * gt, voxel_type init = 0, voxel_type outs = VX_EMPTY) : voxel_c(x, y, z, gt, init, outs) {}
    voxel_3_c(const xml::node & node, const gridType_c * gt) : voxel_c(node, gt) {}
    voxel_3_c(const voxel_c & orig) : voxel_c(orig) { }
    voxel_3_c(const voxel_c * orig) : voxel_c(orig) { }

    void transformPoint(int * x, int * y, int * z, unsigned int trans) const;
    virtual bool transform(unsigned int nr);

    virtual void getHotspot(unsigned char trans, int * x, int * y, int * z) const;
    virtual void getBoundingBox(unsigned char trans, int * x1, int * y1, int * z1, int * x2 = 0, int * y2 = 0, int * z2 = 0) const;
    bool getNeighbor(unsigned int idx, unsigned int typ, int x, int y, int z, int * xn, int *yn, int *zn) const;

    void scale(unsigned int amount);
    bool scaleDown(unsigned char by, bool action);
    void resizeInclude(int & px, int & py, int & pz);
    void minimizePiece(void);

    virtual bool validCoordinate(int x, int y, int z) const;
};

#endif
