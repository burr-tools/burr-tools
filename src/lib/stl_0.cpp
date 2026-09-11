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

#include "../halfedge/polyhedron.h"
#include "../halfedge/modifiers.h"

#include <string>
#include <math.h>

#define Epsilon 1.0e-5

/* the cube grid: the chamfer comes from the per-vertex lookup tables in
 * cubemesh.cpp (see cubepoly.h) - the gap step (every body's faces moved
 * in by the offset, bodies 2*offset apart), convex edges bevelled with
 * legs of the bevel size, and the concave edges chamfered as well or not
 * (parameter 10, "Interior chamfers"). Bevel and offset are in output
 * units, the mesher works in cell units and the result is scaled like
 * the other grids' meshes. The old mesher's construction grooves do not
 * exist here, so the groove switches have no effect; tubes need the old
 * mesher's one-face-per-voxel-side structure and are refused. */
Polyhedron * stlExporter_0_c::getCubeMesh(const voxel_c & v, const faceList_c & holes, double scale_y, double scale_z) const
{
  static std::string why;                        /* stlException_c keeps a pointer */
  if (!holes.empty()) throw stlException_c("Tubes are not supported by the cube chamfer mesher");
  double g = shrink / cube_scale, r = bevel / cube_scale;
  /* the inner void: the same shape with the gap grown by the wall
   * thickness, inverted; left out when the wall would fill it (the
   * mesher's own limit on gap + bevel) */
  double g2 = (hole + shrink) / cube_scale;
  Polyhedron * poly = cubePolyhedron(v, g, r, interiorChamfers, (hole > Epsilon && g2 + r <= 0.35) ? g2 : 0, why);
  if (!poly) throw stlException_c(why.c_str());
  scalePolyhedron(*poly, cube_scale, scale_y, scale_z);
  return poly;
}

Polyhedron * stlExporter_0_c::getMesh(const voxel_c & v, const faceList_c & holes) const
{
  if (v.countState(voxel_c::VX_VARIABLE)) throw stlException_c("Shapes with variable voxels cannot be exported");
  if (cube_scale < Epsilon) throw stlException_c("Unit size too small");
  if (cube_scale_y < 0 || (cube_scale_y > 0 && cube_scale_y < Epsilon)) throw stlException_c("Unit size y too small");
  if (cube_scale_z < 0 || (cube_scale_z > 0 && cube_scale_z < Epsilon)) throw stlException_c("Unit size z too small");
  if (shrink < 0) throw stlException_c("Offset cannot be negative");
  if (bevel < 0) throw stlException_c("Bevel cannot be negative");
  if (tubes > 1) throw stlException_c("Tubes size too large");

  // the unit size in y and z direction defaults to the base unit size
  double scale_y = (cube_scale_y > Epsilon) ? cube_scale_y : cube_scale;
  double scale_z = (cube_scale_z > Epsilon) ? cube_scale_z : cube_scale;

  if (v.getGridType()->getType() == gridType_c::GT_BRICKS)
    return getCubeMesh(v, holes, scale_y, scale_z);

  if (!v.meshParamsValid(bevel/cube_scale, shrink/cube_scale)) throw stlException_c("Bevel and offset are not valid");

  Polyhedron * poly = v.getMesh(bevel/cube_scale, shrink/cube_scale);

  if (!leaveGroovesInside)
  {
    fillPolyhedronHoles(*poly, leaveGroovesOutside ? 0 : 1);
  }

  scalePolyhedron(*poly, cube_scale, scale_y, scale_z);

  // we create inside void, when wall thickness is more than zero and not too
  // big to fill out the complete internal void (or better to let the
  // generated internal polygon become degenerated
  if ((hole > Epsilon) && v.meshParamsValid(0, (hole+shrink)/cube_scale))
  {
    Polyhedron * holePoly = v.getMesh(0, (hole+shrink)/cube_scale);

    scalePolyhedron(*holePoly, cube_scale, scale_y, scale_z);

    if (smoothVoid)
    {
      fillPolyhedronHoles(*holePoly, 0);
    }

    joinPolyhedronInverse(*poly, *holePoly, holes, tubes);

    delete holePoly;
  }

  return poly;
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
    case 5: return "Wall Thickness";
    case 6: return "Tubes size";
    case 7: return "Leave inside grooves";
    case 8: return "Leave outside grooved";
    case 9: return "Remove grooves in void";
    case 10: return "Interior chamfers";
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
    case 5: return hole;
    case 6: return tubes;
    case 7: return leaveGroovesInside ? 1 : 0;
    case 8: return leaveGroovesOutside ? 1 : 0;
    case 9: return smoothVoid ? 1 : 0;
    case 10: return interiorChamfers ? 1 : 0;
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
    case 5: hole = value; return;
    case 6: tubes = value; return;
    case 7: leaveGroovesInside  = (value != 0); return;
    case 8: leaveGroovesOutside = (value != 0); return;
    case 9: smoothVoid = (value != 0); return;
    case 10: interiorChamfers = (value != 0); return;
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
    case 5: return " Thickness of the wall, 0 means the piece is completely filled ";
    case 6: return " The size of the tubes that connect the inner void with the outside world. "
                    "The size is relative to the face size. Biggest value 1 ";
    case 7: return " Leave the construction grooves on the inside of the generated shape ";
    case 8: return " Leave the construction grooves on the outside of the generated shape ";
    case 9: return " Remove the grooves in the insiede void ";
    case 10: return " Chamfer the concave (interior) edges as well as the convex ones (cube grid) ";

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
    case 5:
    case 6:
    default:
      return PAR_TYP_POS_DOUBLE;
    case 7:
    case 8:
    case 9:
    case 10:
      return PAR_TYP_SWITCH;
  }
}

