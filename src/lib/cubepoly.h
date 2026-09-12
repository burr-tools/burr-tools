/* The cube grid's chamfered surface as a halfedge Polyhedron: the lookup
 * mesher's output (cubemesh.h) with the face payload the 3D view and the
 * STL exporter use. Shared by the STL export and the view's STL render
 * style, so both show the same surface. */
#ifndef CUBEPOLY_H
#define CUBEPOLY_H

#include <string>

class voxel_c;
class Polyhedron;

/* the chamfered surface of the cube-grid shape v, in cell units,
 * finalized: gap g (bodies end up 2g apart), bevel legs r, with or
 * without the interior (concave) chamfers. Returns 0 with a message when
 * the mesher refuses the parameters. */
Polyhedron * cubePolyhedron(const voxel_c & v, double g, double r, bool fills, std::string & err);

#endif
