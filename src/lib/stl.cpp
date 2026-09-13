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

#include "stl.h"

#include "../halfedge/polyhedron.h"
#include "../halfedge/vector3.h"
#include "../halfedge/modifiers.h"

#include <string.h>
#include <cmath>
#include <memory>

/** \page STL Surface Tessellation Language
 *
 * STL is a file format allowing to define 3D-Objects using just triangles. The
 * format is used for 3D rapid prototyping machines.
 *
 * See http://en.wikipedia.org/wiki/STL_(file_format) for details in the file format.
 *
 * BurrTools supports text mode STL as well as binary mode STL file export. The working
 * is again similar to all. You have the base class and a derived class for concrete
 * grid types (e.g. one for bricks, one for spheres...)
 *
 * The concrete classes do the grid dependent stuff and add lots of triangles to the file
 */

#if defined(_WIN32) || defined(__APPLE__) || defined(EMSCRIPTEN)
const char * basename(const char * name) {
  const char * res1 = strchr(name, '/');
  const char * res2 = strchr(name, '\\');

  const char * res = res1>res2 ? res1 : res2;

  if (res == 0)
    res = name;
  else
    res++;

  return res;
}
#endif


void stlExporter_c::write(const char * fname, const voxel_c & v)
{
  std::unique_ptr<FILE, int(*)(FILE*)> f(nullptr, &fclose);
  unsigned long triangleCount = 0;

  const char * title = basename(fname);

  if (binaryMode)
  {
    f.reset(fopen(fname,"wb"));

    if (!f) throw stlException_c("Could not open file");

    int pos = 0;

    for (int i = 0; i < 84; i++)
    {
      if (fwrite(title+pos, 1, 1, f.get()) != 1) throw stlException_c("Could not write file");
      if (title[pos]) pos++;
    }
  }
  else
  {
    f.reset(fopen(fname,"w"));

    if (!f) throw stlException_c("Could not open file");

    fprintf(f.get(), "solid %s\n", title);
  }

  // try to generate the polyhedron, there might be problems along the way,
  // like wrong parameters, or things like that
  std::unique_ptr<Polyhedron> poly(getMesh(v));
  if (!poly) throw stlException_c("Something went wrong when generating the STL polyhedron");

  /* connected coplanar faces merged and re-triangulated with fewer,
   * larger triangles: the meshers keep every vertex of a face's outline */
  poly.reset(mergeCoplanarFaces(*poly));

  // write out the generated polyhedron
  for(Polyhedron::const_face_iterator it=poly->fBegin(); it!=poly->fEnd(); ++it)
  {
    const Face* fc = *it;

    if (fc->hole())
      continue;

    Vector3Df normalVector = fc->normal();
    if (std::isnan(normalVector.x()) || std::isnan(normalVector.y()) || std::isnan(normalVector.z()))
      normalVector = Vector3Df(0, 0, 0);
    const float * normal = normalVector.getData();

    Face::const_edge_circulator e = fc->begin();
    Face::const_edge_circulator sentinel = e;
    ++e;
    Vector3Df start = (*e)->dst()->position();
    ++e;

    do
    {
      const float * v1 = start.getData();
      const float * v2 = (*e)->dst()->position().getData();
      ++e;
      const float * v3 = (*e)->dst()->position().getData();

      if (binaryMode)
      {
        // write normal vector
        if (fwrite(normal, 3, 4, f.get()) != 4) throw stlException_c("Could not write file");

        // write the 3 vertices
        if (fwrite(v1, 3, 4, f.get()) != 4) throw stlException_c("Coult not write file");
        if (fwrite(v2, 3, 4, f.get()) != 4) throw stlException_c("Coult not write file");
        if (fwrite(v3, 3, 4, f.get()) != 4) throw stlException_c("Coult not write file");

        // attribute
        int i = 0;
        if (fwrite(&i, 1, 2, f.get()) != 2) throw stlException_c("Coult not write file");

        triangleCount++;
      }
      else
      {
        fprintf(f.get(),"  facet normal %9.4e %9.4e %9.4e\n", normal[0], normal[1], normal[2]);
        fprintf(f.get(),"    outer loop\n");
        fprintf(f.get(),"      vertex %9.4e %9.4e %9.4e\n", v1[0], v1[1], v1[2]);
        fprintf(f.get(),"      vertex %9.4e %9.4e %9.4e\n", v2[0], v2[1], v2[2]);
        fprintf(f.get(),"      vertex %9.4e %9.4e %9.4e\n", v3[0], v3[1], v3[2]);
        fprintf(f.get(),"    endloop\n");
        fprintf(f.get(),"  endfacet\n");
      }
    } while (e != sentinel);
  }

  if (binaryMode)
  {
    // write out the triangle count into the header
    fseek(f.get(), 80, SEEK_SET);
    if (fwrite(&triangleCount, 1, 4, f.get()) != 4) throw stlException_c("Coult not write file");
  }
  else
  {
    fprintf(f.get(), "endsolid\n");
  }
}

