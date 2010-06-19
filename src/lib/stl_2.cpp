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
#include "stl_2.h"

#include "voxel_2.h"

#define Epsilon 1.0e-5

Polyhedron * stlExporter_2_c::getMesh(const voxel_c & v) const
{
  if (v.countState(voxel_c::VX_VARIABLE)) throw stlException_c("Shapes with variable voxels cannot be exported");
  if (sphere_rad < Epsilon) throw stlException_c("Sphere size too small");
  if (offset < 0) throw stlException_c("Offset cannot be negative");
  if (round < 0) throw stlException_c("Curvature radius cannot be negative");
  if (offset > sphere_rad) throw stlException_c("Offset must be smaller than sphere radius");
  if (round > 1) throw stlException_c("The curvature radius is relative and must be between 0 and 1");
  if (connection_rad < 0 || connection_rad > 1)
    throw stlException_c("The connection radius is relative and must be between 0 and 1");
  if (inner_rad < 0 || inner_rad >= sphere_rad)
    throw stlException_c("The inner radius cannot be negative and must be less than the sphere radius");
  if (hole_diam < 0 || hole_diam > 0.333334*sphere_rad)
      throw stlException_c("The hole diameter cannot be negative or greater than 1/3 of the sphere radius");

  const voxel_2_c * v2 = dynamic_cast<const voxel_2_c*>(&v);
  // check cast, otherwise a non voxel_2_c class was given, which is wrong in this case
  bt_assert(v2);

  return v2->getMesh(sphere_rad, connection_rad, round, offset, (int)recursion, inner_rad, (square_hole?-1:1)*hole_diam);
}


const char * stlExporter_2_c::getParameterName(unsigned int idx) const
{
  switch (idx)
  {
    case 0: return "Sphere radius";
    case 1: return "Connection radius";
    case 2: return "Curvature radius";
    case 3: return "Offset";
    case 4: return "Recursions";
    case 5: return "Inner radius";
    case 6: return "Hole diameter";
    case 7: return "Square Hole";
    default: return 0;
  }
}

double stlExporter_2_c::getParameter(unsigned int idx) const
{
  switch (idx)
  {
    case 0: return sphere_rad;
    case 1: return connection_rad;
    case 2: return round;
    case 3: return offset;
    case 4: return recursion;
    case 5: return inner_rad;
    case 6: return hole_diam;
    case 7: return square_hole;
    default: return 0;
  }
}

void stlExporter_2_c::setParameter(unsigned int idx, double value)
{
  switch (idx)
  {
    case 0: sphere_rad = value; return;
    case 1: connection_rad = value; return;
    case 2: round = value; return;
    case 3: offset = value; return;
    case 4: recursion = value; return;
    case 5: inner_rad = value; return;
    case 6: hole_diam = value; return;
    case 7: square_hole = value; return;
    default: return;
  }
}

const char * stlExporter_2_c::getParameterTooltip(unsigned int idx) const
{
  switch (idx)
  {
    case 0: return " Base Radius of the spheres, defines how far apart they are ";
    case 1: return " Radius of the connection cylinder between the spheres, 1 is the maximum, 0 means no connection ";
    case 2: return " Define the radius of the curve that creates the transition between sphere surface and connection cylinder 1 is maximum ";
    case 3: return " The radius of the outer surface of the sphere is decreased by this value to make the spheres a bit smaller ";
    case 4: return " How many levels of recursion, a value of 3 or 4 is enough, be careful with bigger values as it will take ages to generate ";
    case 5: return " The radius of the void inside the sphere, must be smaller than the sphere radius ";
    case 6: return " The radius of the holes inside the sphere wall that connect inside and outside ";
    case 7: return " When tagged the holes will be square, otherwise round ";
    default: return "";
  }
}

stlExporter_c::parameterTypes stlExporter_2_c::getParameterType(unsigned int idx) const
{
  switch (idx)
  {
    case 0:
    case 1:
    case 2:
    case 3:
    case 5:
    case 6:
      return stlExporter_c::PAR_TYP_POS_DOUBLE;
    case 4:
      return stlExporter_c::PAR_TYP_POS_INTEGER;
    case 7:
      return stlExporter_c::PAR_TYP_SWITCH;
    default:
      return stlExporter_c::PAR_TYP_DOUBLE;
  }
}


