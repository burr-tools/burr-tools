/* The cube grid's chamfered STL surface by per-vertex lookup: every
 * grid vertex is one of 12 face-connected cases (the 8 cells about it,
 * up to the cube's 48 symmetries, cells touching only at an edge or a
 * corner handled as separate bodies); each case's chamfer surface
 * inside the vertex's dual cell, for the r/g regime in force, is a
 * table of planar polygons (and non-planar hole patches with their
 * triangulation) whose vertex coordinates are affine in the gap g and
 * the bevel r. The tables (cubetable.h with interior chamfers,
 * cubetable_nofill.h without) are generated offline from the
 * case-by-case solutions, see the note at the top of each table; this
 * code only evaluates, transforms, welds and triangulates. No booleans,
 * no tracing, no searching. */
#ifndef CUBEMESH_H
#define CUBEMESH_H

#include <vector>
#include <string>

namespace cubeMesh {

  struct vec3 { double x, y, z; };

  /* the shape: nx*ny*nz cells, filled[x + nx*(y + ny*z)] != 0 for a
   * filled cell. Cell (x,y,z) occupies [x,x+1]x[y,y+1]x[z,z+1]. Output:
   * outward-wound triangles, three vertices each, in cell units. The
   * gap g is per body (bodies end up 2g apart), r the bevel leg.
   * Returns false with a message when the parameters are outside what
   * the table covers (r/g outside its regimes, or g + r too large for
   * the dual-cell construction). fills selects the variant: with the
   * interior (concave) chamfers, or without. */
  bool generate(int nx, int ny, int nz, const std::vector<char> & filled,
                double g, double r, bool fills, std::vector<vec3> & tris,
                std::string & err);

  /* the same, keeping the polygons (index loops into verts) as well as
   * the triangles - for tests and for a consolidation pass */
  struct mesh_s {
    std::vector<vec3> verts;
    std::vector<std::vector<int> > polys;   /* loops, after consolidation: whole faces; a face with holes is several loops */
    std::vector<int> polyFace;              /* per loop: its face (the outer loop of a face comes first) */
    std::vector<int> tris;                  /* index triples into verts */
  };
  bool generate(int nx, int ny, int nz, const std::vector<char> & filled,
                double g, double r, bool fills, mesh_s & out, std::string & err);
}

#endif
