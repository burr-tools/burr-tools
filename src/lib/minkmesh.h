/* The chamfered STL surface of the prism, rhombic and tetra-octa grids
 * by Minkowski operations (the Manifold library): the shape's cells are
 * convex hulls, the gap step is an erosion by the grid's offset bit, the
 * chamfer an expansion / erosion by its chamfer bit.
 *
 * Rhombic and tetra-octa, with interior (concave) chamfers:
 *   erode by the cube at 2b, expand by the octahedron at 2b, erode by it
 *   at b, expand by the cube at b, erode by the cuboctahedron at g
 * without:
 *   erode by the cube at b', then expand by the cuboctahedron at b' - g
 *   (erode by g - b' when the gap is the larger, nothing when equal)
 * Prism, with interior chamfers:
 *   erode by the hexagonal prism at 2b, expand by the bipyramid at 2b,
 *   erode by the bipyramid at b, expand by the prism at b - g (erode by
 *   g - b when the gap is the larger, nothing when equal)
 * without:
 *   erode by the prism at b + g, expand by the bipyramid at b
 * with b' = 1.707 b for the cuboctahedron. Faces land g inside their
 * cells, bodies 2g apart, convex edges bevelled with legs b, and with
 * fills the concave edges filled with legs b.
 *
 * Bits: the cube with vertices at (+-1, +-1, +-1); the octahedron with
 * vertices at 1 on the axes (its edges give legs exactly the size on a
 * cube's edges); the cuboctahedron with faces on the (100), (110) and
 * (111) directions, (100) and (110) at 1, (111) at 1.0557; the
 * hexagonal prism and the hexagonal bipyramid aligned to the grid, the
 * bipyramid's faces on the facet directions at 45 degrees. */
#ifndef MINKMESH_H
#define MINKMESH_H

#include <string>

class voxel_c;
class Polyhedron;

namespace minkMesh {

  /* is the grid handled here (prism, rhombic, tetra-octa) */
  bool handles(const voxel_c & v);

  /* the chamfered surface of the shape v as a halfedge Polyhedron in cell
   * units, finalized: gap g, bevel legs r, with or without the interior
   * chamfers. Returns 0 with a message when the construction fails or
   * nothing is left of the shape. */
  Polyhedron * polyhedron(const voxel_c & v, double g, double r, bool fills, std::string & err);
}

#endif
