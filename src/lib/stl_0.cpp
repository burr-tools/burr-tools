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
#include "cubemesh.h"

#include "../halfedge/polyhedron.h"
#include "../halfedge/modifiers.h"

#include <string>
#include <math.h>

#define Epsilon 1.0e-5

/* the cube grid: the chamfer comes from the per-vertex lookup tables in
 * cubemesh.cpp - the gap step (every body's faces moved
 * in by the offset, bodies 2*offset apart), convex edges bevelled with
 * legs of the bevel size, and the concave edges chamfered as well or not
 * (parameter 10, "Interior chamfers"). Bevel and offset are in output
 * units, the mesher works in cell units and the result is scaled like
 * the other grids' meshes. The old mesher's construction grooves do not exist here,
 * so the groove switches have no effect; tubes need the old mesher's
 * one-face-per-voxel-side structure and are refused. */
Polyhedron * stlExporter_0_c::getCubeMesh(const voxel_c & v, const faceList_c & holes, double scale_y, double scale_z) const
{
  static std::string why;                        /* stlException_c keeps a pointer */
  if (!holes.empty()) throw stlException_c("Tubes are not supported by the cube chamfer mesher");
  int nx = v.getX(), ny = v.getY(), nz = v.getZ();
  std::vector<char> filled(nx * ny * nz, 0);
  for (int z = 0; z < nz; z++)
    for (int y = 0; y < ny; y++)
      for (int x = 0; x < nx; x++)
        filled[x + nx * (y + ny * z)] = v.getState(x, y, z) == voxel_c::VX_FILLED ? 1 : 0;
  double g = shrink / cube_scale, r = bevel / cube_scale;
  cubeMesh::mesh_s outer;
  if (!cubeMesh::generate(nx, ny, nz, filled, g, r, interiorChamfers, outer, why))
    throw stlException_c(why.c_str());
  Polyhedron * poly = new Polyhedron();
  vertexList_c vl(poly);
  /* triangles (the STL writer fan-triangulates faces, so the mesher's
   * whole faces, which may be non-convex or have holes, go in as their
   * triangles); the face payload the 3D view uses: bevel faces flagged,
   * axis faces given their voxel and side (getNeighbor numbering: 0..2
   * = -x -y -z, 3..5 = +x +y +z) and the checkerboard bit */
  struct add_s {
    static void faces(Polyhedron * poly, vertexList_c & vl, const cubeMesh::mesh_s & m, const voxel_c & v, double g, bool inside) {
      int nx = v.getX(), ny = v.getY(), nz = v.getZ();
      for (unsigned t = 0; t + 2 < m.tris.size(); t += 3) {
        const cubeMesh::vec3 & a = m.verts[m.tris[t]], & b = m.verts[m.tris[t + 1]], & c = m.verts[m.tris[t + 2]];
        int ia = vl.get(a.x, a.y, a.z);
        int ib = vl.get(b.x, b.y, b.z);
        int ic = vl.get(c.x, c.y, c.z);
        Face * f = inside ? poly->addFace(ia, ic, ib) : poly->addFace(ia, ib, ic);
        double n[3] = { (b.y - a.y) * (c.z - a.z) - (b.z - a.z) * (c.y - a.y),
                        (b.z - a.z) * (c.x - a.x) - (b.x - a.x) * (c.z - a.z),
                        (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x) };
        double ln = sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        int side = -1;
        for (int ax = 0; ax < 3 && ln > 0; ax++) {
          if (n[ax] > (1 - 1e-6) * ln) side = 3 + ax;
          if (n[ax] < -(1 - 1e-6) * ln) side = ax;
        }
        f->_fb_face = -1; f->_fb_index = 0; f->_flags = inside ? FF_INSIDE_FACE : 0;
        if (side < 0) { f->_flags |= FF_BEVEL_FACE; continue; }
        /* the voxel this side belongs to: the face plane is cell + 1 - g
         * (positive side) or cell + g (negative), the other two
         * coordinates from the triangle's centroid */
        double cen[3] = { (a.x + b.x + c.x) / 3, (a.y + b.y + c.y) / 3, (a.z + b.z + c.z) / 3 };
        int ax = side % 3;
        int cell[3];
        for (int q = 0; q < 3; q++) cell[q] = (int)floor(cen[q]);
        cell[ax] = side >= 3 ? (int)floor(cen[ax] + g + 0.5) - 1 : (int)floor(cen[ax] - g + 0.5);
        if (inside) cell[ax] = side >= 3 ? (int)floor(cen[ax] - g + 0.5) : (int)floor(cen[ax] + g + 0.5) - 1;
        if (cell[0] < 0 || cell[1] < 0 || cell[2] < 0 || cell[0] >= nx || cell[1] >= ny || cell[2] >= nz) continue;
        f->_fb_index = v.getIndex(cell[0], cell[1], cell[2]);
        f->_fb_face = side;
        if ((cell[0] + cell[1] + cell[2]) & 1) f->_flags |= FF_COLOR_LIGHT;
      }
    }
  };
  add_s::faces(poly, vl, outer, v, g, false);
  /* the inner void: the same shape with the gap grown by the wall
   * thickness, inverted; left out when the wall would fill it (the
   * mesher's own limit on gap + bevel) */
  double g2 = (hole + shrink) / cube_scale;
  if (hole > Epsilon && g2 + r <= 0.35) {
    /* (bevel 0 and offset 0 are special-cased inside the mesher: the
     * gap step alone, the r/g -> infinity limit, or the plain cells) */
    cubeMesh::mesh_s inner;
    if (!cubeMesh::generate(nx, ny, nz, filled, g2, r, interiorChamfers, inner, why))
      throw stlException_c(why.c_str());
    add_s::faces(poly, vl, inner, v, g2, true);
  }
  poly->finalize();
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

