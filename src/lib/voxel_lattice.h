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
#ifndef __VOXEL_LATTICE_H__
#define __VOXEL_LATTICE_H__

#include "voxel.h"

class latticeGridInfo_c;

/**
 * Voxel space of a described grid (BurrTools NG's grids as data, see
 * latticegridinfo.h and design/2026-11-19-ng-merge-plan.md, step G3).
 *
 * The dense array holds NG's voxel coordinates, up to a lattice
 * translation: only the cells for which validCoordinate() holds are
 * voxels. Everything that moves the contents (transform, minimizePiece,
 * resizeInclude) moves them by lattice vectors only, so a shape keeps its
 * voxel kinds wherever it sits, and two shapes are the same when their
 * boxes hold the same and are a lattice vector apart (identicalInBB).
 *
 * The hotspot is always a voxel position, so that every transformation
 * maps it to a whole-numbered position.
 */
class voxel_lattice_c : public voxel_c {

  public:

    voxel_lattice_c(unsigned int x, unsigned int y, unsigned int z, const gridType_c * gt, voxel_type init = 0);
    voxel_lattice_c(xmlParser_c & pars, const gridType_c * gt);
    voxel_lattice_c(const voxel_c & orig);
    voxel_lattice_c(const voxel_c * orig);

    bool transform(unsigned int nr) override;
    void transformPoint(int * x, int * y, int * z, unsigned int trans) const override;
    void getTransformMatrix(unsigned int trans, double m[9]) const override;

    bool getNeighbor(unsigned int idx, unsigned int typ, int x, int y, int z, int * xn, int *yn, int *zn) const override;

    void resizeInclude(int & px, int & py, int & pz) override;
    void minimizePiece(void) override;
    void initHotspot(void) override;

    bool validCoordinate(int x, int y, int z) const override;
    bool identicalInBB(const voxel_c * op, bool includeColors = true) const override;
    bool onGrid(int x, int y, int z) const override;

    void getConnectionFace(int x, int y, int z, int n, double bevel, double offset, std::vector<float> & faceCorners) const override;
    void calculateSize(float * x, float * y, float * z) const override;
    void drawingBounds(float lo[3], float hi[3]) const override;
    void recalcSpaceCoordinates(float * x, float * y, float * z) const override;

    const latticeGridInfo_c & info(void) const { return inf; }

  private:

    const latticeGridInfo_c & inf;

    /* replace the contents by the given voxels (positions and values),
     * shifted by the lattice vector -v, in a space just big enough */
    struct cell_s { int x, y, z; voxel_type v; };
    void rebuild(const std::vector<cell_s> & cells, int vx, int vy, int vz);

    voxel_lattice_c & operator=(const voxel_lattice_c &) = delete;
};

#endif
