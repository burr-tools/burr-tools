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
#include "stl_0.h"

#include "voxel.h"
#include "gridtype.h"
#include "cubepoly.h"
#include "minkmesh.h"

#include "../halfedge/polyhedron.h"
#include "../halfedge/modifiers.h"

#include <string>

#define Epsilon 1.0e-5

/* the cube grid: the chamfer comes from the per-vertex lookup tables in
 * cubemesh.cpp (see cubepoly.h) - the gap step (every body's faces moved
 * in by the offset, bodies 2*offset apart), convex edges bevelled with
 * legs of the bevel size, and the concave edges chamfered as well or not
 * (parameter 5, "Interior chamfers"). Bevel and offset are in output
 * units, the mesher works in cell units and the result is scaled like
 * the other grids' meshes. */
Polyhedron * stlExporter_0_c::getCubeMesh(const voxel_c & v, double scale_y, double scale_z) const
{
  static std::string why;                        /* stlException_c keeps a pointer */
  Polyhedron * poly = cubePolyhedron(v, shrink / cube_scale, bevel / cube_scale, interiorChamfers, why);
  if (!poly) throw stlException_c(why.c_str());
  scalePolyhedron(*poly, cube_scale, scale_y, scale_z);
  return poly;
}

/* the prism, rhombic and tetra-octa grids: the Minkowski construction
 * (minkmesh.h) with the same parameters and the same switch */
Polyhedron * stlExporter_0_c::getMinkowskiMesh(const voxel_c & v, double scale_y, double scale_z) const
{
  static std::string why;
  Polyhedron * poly = minkMesh::polyhedron(v, shrink / cube_scale, bevel / cube_scale, interiorChamfers, why);
  if (!poly) throw stlException_c(why.c_str());
  scalePolyhedron(*poly, cube_scale, scale_y, scale_z);
  return poly;
}

Polyhedron * stlExporter_0_c::getMesh(const voxel_c & v) const
{
  if (v.countState(voxel_c::VX_VARIABLE)) throw stlException_c("Shapes with variable voxels cannot be exported");
  if (cube_scale < Epsilon) throw stlException_c("Unit size too small");
  if (cube_scale_y < 0 || (cube_scale_y > 0 && cube_scale_y < Epsilon)) throw stlException_c("Unit size y too small");
  if (cube_scale_z < 0 || (cube_scale_z > 0 && cube_scale_z < Epsilon)) throw stlException_c("Unit size z too small");
  if (shrink < 0) throw stlException_c("Offset cannot be negative");
  if (bevel < 0) throw stlException_c("Bevel cannot be negative");

  // the unit size in y and z direction defaults to the base unit size
  double scale_y = (cube_scale_y > Epsilon) ? cube_scale_y : cube_scale;
  double scale_z = (cube_scale_z > Epsilon) ? cube_scale_z : cube_scale;

  if (v.getGridType()->getType() == gridType_c::GT_BRICKS)
    return getCubeMesh(v, scale_y, scale_z);
  if (minkMesh::handles(v))
    return getMinkowskiMesh(v, scale_y, scale_z);

  throw stlException_c("No mesher for this grid");
}


const char * stlExporter_0_c::getParameterName(unsigned int idx) const
{
  switch (idx)
  {
    case 0: return "Unit Size";
    case 1: return "Unit Size Y";
    case 2: return "Unit Size Z";
    case 3: return "Bevel";
    case 4: return "Offset";
    case 5: return "Interior chamfers";
    default: return 0;
  }
}

double stlExporter_0_c::getParameter(unsigned int idx) const
{
  switch (idx)
  {
    case 0: return cube_scale;
    case 1: return cube_scale_y;
    case 2: return cube_scale_z;
    case 3: return bevel;
    case 4: return shrink;
    case 5: return interiorChamfers ? 1 : 0;
    default: return 0;
  }
}

void stlExporter_0_c::setParameter(unsigned int idx, double value)
{
  switch (idx)
  {
    case 0: cube_scale = value; return;
    case 1: cube_scale_y = value; return;
    case 2: cube_scale_z = value; return;
    case 3: bevel = value; return;
    case 4: shrink = value; return;
    case 5: interiorChamfers = (value != 0); return;
    default: return;
  }
}

const char * stlExporter_0_c::getParameterTooltip(unsigned int idx) const
{
  switch (idx)
  {
    case 0: return " Basic unit size of the voxel, used for the x direction and as the "
                    "default for the y and z direction ";
    case 1: return " Unit size in the y direction, 0 means same as the basic unit size. "
                    "Bevel and offset stretch along with a changed y unit ";
    case 2: return " Unit size in the z direction, 0 means same as the basic unit size. "
                    "Bevel and offset stretch along with a changed z unit ";
    case 3: return " Size of the bevel at the edges ";
    case 4: return " By how much should faces be inset into the voxel ";
    case 5: return " Chamfer the concave (interior) edges as well as the convex ones ";

    default: return "";
  }
}

stlExporter_c::parameterTypes stlExporter_0_c::getParameterType(unsigned int idx) const
{
  switch(idx)
  {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    default:
      return PAR_TYP_POS_DOUBLE;
    case 5:
      return PAR_TYP_SWITCH;
  }
}

