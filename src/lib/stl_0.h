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
#ifndef __STL_0_H__
#define __STL_0_H__

#include "stl.h"

/** stl exporter for cubes */
class stlExporter_0_c : public stlExporter_c {

  public:

    /* cubes: it exports the cube grid, whose pieces can make rotation
     * moves, so bevel and offset can come from a model (followModel) */
    explicit stlExporter_0_c(bool cubes = false) : bevel(0.3), cube_scale(10), cube_scale_y(0), cube_scale_z(0), shrink(0.01),
      interiorChamfers(true), cubes(cubes) {}

    Polyhedron * getMesh(const voxel_c & v) const override;
    unsigned int numParameters(void) const override { return 6; }
    const char * getParameterName(unsigned int idx) const override;
    double getParameter(unsigned int idx) const override;
    void setParameter(unsigned int idx, double value) override;
    const char * getParameterTooltip(unsigned int idx) const override;
    parameterTypes getParameterType(unsigned int idx) const override;
    bool followModel(const rotationGeometry::model_t & model) override;
    bool followsModel(void) const override { return following.has_value(); }
    std::optional<rotationGeometry::model_t> currentModel(void) const override;

  private:

    double bevel;
    double cube_scale;
    double cube_scale_y;  // unit size in y direction, 0 = same as cube_scale
    double cube_scale_z;  // unit size in z direction, 0 = same as cube_scale
    double shrink;
    bool interiorChamfers;   // chamfer the concave edges too
    bool cubes;
    /* the model bevel and offset follow (followModel), until set by hand */
    std::optional<rotationGeometry::model_t> following;

    void applyModel(void);

    /* the cube grid: the per-vertex lookup mesher (cubepoly.h); the
     * other grids: the Minkowski mesher (minkmesh.h); scale_y/scale_z as
     * in getMesh */
    Polyhedron * getCubeMesh(const voxel_c & v, double scale_y, double scale_z) const;
    Polyhedron * getMinkowskiMesh(const voxel_c & v, double scale_y, double scale_z) const;

private:

  // no copying and assigning
  stlExporter_0_c(const stlExporter_0_c&);
  void operator=(const stlExporter_0_c&);

};

#endif
