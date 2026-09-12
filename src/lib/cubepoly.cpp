/* see cubepoly.h */
#include "cubepoly.h"
#include "cubemesh.h"
#include "voxel.h"

#include "../halfedge/polyhedron.h"

#include <cmath>
#include <algorithm>
#include <vector>

namespace {

/* the mesher's triangles into the polyhedron (the STL writer
 * fan-triangulates faces, so the mesher's whole faces, which may be
 * non-convex or have holes, go in as their triangles), with the face
 * payload the 3D view uses: bevel faces flagged, axis faces given their
 * voxel and side (getNeighbor numbering: 0..2 = -x -y -z, 3..5 = +x +y
 * +z) and the checkerboard bit */
void addFaces(Polyhedron * poly, vertexList_c & vl, const cubeMesh::mesh_s & m, const voxel_c & v, double g) {
  int nx = v.getX(), ny = v.getY(), nz = v.getZ();
  for (unsigned t = 0; t + 2 < m.tris.size(); t += 3) {
    const cubeMesh::vec3 & a = m.verts[m.tris[t]], & b = m.verts[m.tris[t + 1]], & c = m.verts[m.tris[t + 2]];
    int ia = vl.get(a.x, a.y, a.z);
    int ib = vl.get(b.x, b.y, b.z);
    int ic = vl.get(c.x, c.y, c.z);
    Face * f = poly->addFace(ia, ib, ic);
    double n[3] = { (b.y - a.y) * (c.z - a.z) - (b.z - a.z) * (c.y - a.y),
                    (b.z - a.z) * (c.x - a.x) - (b.x - a.x) * (c.z - a.z),
                    (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x) };
    double ln = sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    int side = -1;
    for (int ax = 0; ax < 3 && ln > 0; ax++) {
      if (n[ax] > (1 - 1e-6) * ln) side = 3 + ax;
      if (n[ax] < -(1 - 1e-6) * ln) side = ax;
    }
    f->_fb_face = -1; f->_fb_index = 0; f->_flags = 0;
    /* the voxel this face belongs to (the 3D view removes it on a click):
     * a side's face plane is cell + 1 - g (positive side) or cell + g
     * (negative), the other two coordinates from the centroid; a bevel
     * or fill belongs to the filled cell its centroid is in or, for a
     * fill (which bulges into the empty cell at a concave edge), the
     * filled cell it sticks out of least */
    double cen[3] = { (a.x + b.x + c.x) / 3, (a.y + b.y + c.y) / 3, (a.z + b.z + c.z) / 3 };
    int cell[3];
    for (int q = 0; q < 3; q++) cell[q] = (int)floor(cen[q]);
    if (side >= 0) {
      int ax = side % 3;
      cell[ax] = side >= 3 ? (int)floor(cen[ax] + g + 0.5) - 1 : (int)floor(cen[ax] - g + 0.5);
    } else {
      f->_flags |= FF_BEVEL_FACE;
      double best = 1e300; int bestCell[3] = { -1, -1, -1 };
      for (int dx = -1; dx <= 1; dx++)
        for (int dy = -1; dy <= 1; dy++)
          for (int dz = -1; dz <= 1; dz++) {
            int cx = cell[0] + dx, cy = cell[1] + dy, cz = cell[2] + dz;
            if (cx < 0 || cy < 0 || cz < 0 || cx >= nx || cy >= ny || cz >= nz) continue;
            if (v.getState(cx, cy, cz) != voxel_c::VX_FILLED) continue;
            double out = 0;
            out = std::max(out, std::max(cx - cen[0], cen[0] - (cx + 1)));
            out = std::max(out, std::max(cy - cen[1], cen[1] - (cy + 1)));
            out = std::max(out, std::max(cz - cen[2], cen[2] - (cz + 1)));
            if (out < best) { best = out; bestCell[0] = cx; bestCell[1] = cy; bestCell[2] = cz; }
          }
      for (int q = 0; q < 3; q++) cell[q] = bestCell[q];
    }
    if (cell[0] < 0 || cell[1] < 0 || cell[2] < 0 || cell[0] >= nx || cell[1] >= ny || cell[2] >= nz) continue;
    f->_fb_index = v.getIndex(cell[0], cell[1], cell[2]);
    if (side < 0) continue;
    f->_fb_face = side;
    if ((cell[0] + cell[1] + cell[2]) & 1) f->_flags |= FF_COLOR_LIGHT;
  }
}

} // namespace

Polyhedron * cubePolyhedron(const voxel_c & v, double g, double r, bool fills, std::string & err)
{
  int nx = v.getX(), ny = v.getY(), nz = v.getZ();
  std::vector<char> filled(nx * ny * nz, 0);
  for (int z = 0; z < nz; z++)
    for (int y = 0; y < ny; y++)
      for (int x = 0; x < nx; x++)
        filled[x + nx * (y + ny * z)] = v.getState(x, y, z) == voxel_c::VX_FILLED ? 1 : 0;
  cubeMesh::mesh_s mesh;
  if (!cubeMesh::generate(nx, ny, nz, filled, g, r, fills, mesh, err)) return 0;
  Polyhedron * poly = new Polyhedron();
  vertexList_c vl(poly);
  addFaces(poly, vl, mesh, v, g);
  poly->finalize();
  return poly;
}
