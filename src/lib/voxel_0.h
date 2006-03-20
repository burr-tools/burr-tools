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
#ifndef __VOXEL_0_H__
#define __VOXEL_0_H__

#include "voxel.h"
#include "symmetries.h"

class voxel_0_c : public voxel_c {

  protected:

    voxel_0_c(unsigned int x, unsigned int y, unsigned int z, const gridType_c * gt, voxel_type init = 0, voxel_type outs = VX_EMPTY) : voxel_c(x, y, z, gt, init, outs) {}
    voxel_0_c(const xml::node & node, const gridType_c * gt) : voxel_c(node, gt) {}
    voxel_0_c(const voxel_c & orig, unsigned int transformation = 0) : voxel_c(orig) {
      transform(transformation);
    }
    voxel_0_c(const voxel_c * orig, unsigned int transformation = 0) : voxel_c(orig) {
      transform(transformation);
    }

    /* the factory functions need to be friend to use the constructors */
    friend voxel_c * gridType_c::getVoxel(unsigned int x, unsigned int y, unsigned int z, voxel_type init, voxel_type outs) const;
    friend voxel_c * gridType_c::getVoxel(const xml::node & node) const;
    friend voxel_c * gridType_c::getVoxel(const voxel_c & orig, unsigned int transformation = 0) const;
    friend voxel_c * gridType_c::getVoxel(const voxel_c * orig, unsigned int transformation = 0) const;
    friend symmetries_t symmetries_0_c::symmetryCalcuation(const voxel_c *pp) const;

  public:

    void rotatex(int by = 1);
    void rotatey(int by = 1);
    void rotatez(int by = 1);

};

#endif
