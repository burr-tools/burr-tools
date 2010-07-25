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

#include "../halfedge/polyhedron.h"
#include "../halfedge/modifiers.h"

#define Epsilon 1.0e-5

Polyhedron * stlExporter_0_c::getMesh(const voxel_c & v, const faceList_c & holes) const
{
  if (v.countState(voxel_c::VX_VARIABLE)) throw stlException_c("Shapes with variable voxels cannot be exported");
  if (cube_scale < Epsilon) throw stlException_c("Unit size too small");
  if (shrink < 0) throw stlException_c("Offset cannot be negative");
  if (bevel < 0) throw stlException_c("Bevel cannot be negative");
  if (cube_scale < (2*bevel + 2*shrink)) throw stlException_c("Unit size too small for given bevel and offset");

  Polyhedron * poly = v.getMesh(bevel/cube_scale, shrink/cube_scale);

  if (!leaveGroovesInside)
  {
    fillPolyhedronHoles(*poly, leaveGroovesOutside ? 0 : 1);
  }

  scalePolyhedron(*poly, cube_scale);

  if (hole > Epsilon)
  {
    // TODO when wall thickness is too big, skip this and make the shape solid
    // problem is how to fins out when it becomes too big

    Polyhedron * holePoly = v.getMesh(0, (hole+shrink)/cube_scale);

    scalePolyhedron(*holePoly, cube_scale);

    if (smoothVoid)
    {
      fillPolyhedronHoles(*holePoly, 0);
    }

    joinPolyhedronInverse(*poly, *holePoly);

    delete holePoly;
  }

  return poly;
}


const char * stlExporter_0_c::getParameterName(unsigned int idx) const
{
  switch (idx)
  {
    case 0: return "Unit Size";
    case 1: return "Bevel";
    case 2: return "Offset";
    case 3: return "Wall Thickness";
    case 4: return "Leave inside grooves";
    case 5: return "Leave outside grooved";
    case 6: return "Remove grooves in void";
    default: return 0;
  }
}

double stlExporter_0_c::getParameter(unsigned int idx) const
{
  switch (idx)
  {
    case 0: return cube_scale;
    case 1: return bevel;
    case 2: return shrink;
    case 3: return hole;
    case 4: return leaveGroovesInside ? 1 : 0;
    case 5: return leaveGroovesOutside ? 1 : 0;
    case 6: return smoothVoid ? 1 : 0;
    default: return 0;
  }
}

void stlExporter_0_c::setParameter(unsigned int idx, double value)
{
  switch (idx)
  {
    case 0: cube_scale = value; return;
    case 1: bevel = value; return;
    case 2: shrink = value; return;
    case 3: hole = value; return;
    case 4: leaveGroovesInside  = (value != 0); return;
    case 5: leaveGroovesOutside = (value != 0); return;
    case 6: smoothVoid = (value != 0); return;
    default: return;
  }
}

const char * stlExporter_0_c::getParameterTooltip(unsigned int idx) const
{
  switch (idx)
  {
    case 0: return " Basic unit size of the voxel ";
    case 1: return " Size of the bevel at the edges ";
    case 2: return " By how much should faces be inset into the voxel ";
    case 3: return " Thickness of the wall, 0 means the piece is completely filled ";
    case 4: return " Leave the construction grooves on the inside of the generated shape ";
    case 5: return " Leave the construction grooves on the outside of the generated shape ";
    case 6: return " Remove the grooves in the insiede void ";

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
    default:
      return PAR_TYP_POS_DOUBLE;
    case 4:
    case 5:
    case 6:
      return PAR_TYP_SWITCH;
  }
}

