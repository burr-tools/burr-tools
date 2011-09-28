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

#include <string.h>

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

#if defined(WIN32) || defined(__APPLE__)
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


void stlExporter_c::write(const char * fname, const voxel_c & v, const faceList_c & holes)
{
  FILE * f;
  unsigned long triangleCount = 0;

  const char * title = basename(fname);

  if (binaryMode)
  {
    f = fopen(fname,"wb");

    if (!f) throw stlException_c("Could not open file");

    int pos = 0;

    for (int i = 0; i < 84; i++)
    {
      if (fwrite(title+pos, 1, 1, f) != 1) throw stlException_c("Could not write file");
      if (title[pos]) pos++;
    }
  }
  else
  {
    f = fopen(fname,"w");

    if (!f) throw stlException_c("Could not open file");

    fprintf(f, "solid %s\n", title);
  }

  // try to generate the polyhedron, there might be problems along the way,
  // like wrong parameters, or things like that, so we need to catch those
  // cases and close the file, if that happens

  Polyhedron * poly = 0;

  try
  {
    poly = getMesh(v, holes);
    if (!poly) throw stlException_c("Something went wrong when generating the STL polyhedron");
  }
  catch (stlException_c & e)
  {
    fclose(f);
    throw e;
  }

  // write out the generated polyhedron
  for(Polyhedron::const_face_iterator it=poly->fBegin(); it!=poly->fEnd(); it++)
  {
    const Face* fc = *it;

    if (fc->hole())
      continue;

    const float * normal = fc->normal().getData();

    Face::const_edge_circulator e = fc->begin();
    Face::const_edge_circulator sentinel = e;
    e++;
    Vector3Df start = (*e)->dst()->position();
    e++;

    do
    {
      const float * v1 = start.getData();
      const float * v2 = (*e)->dst()->position().getData();
      e++;
      const float * v3 = (*e)->dst()->position().getData();

      if (binaryMode)
      {
        // write normal vector
        if (fwrite(normal, 3, 4, f) != 4) throw stlException_c("Could not write file");

        // write the 3 vertices
        if (fwrite(v1, 3, 4, f) != 4) throw stlException_c("Coult not write file");
        if (fwrite(v2, 3, 4, f) != 4) throw stlException_c("Coult not write file");
        if (fwrite(v3, 3, 4, f) != 4) throw stlException_c("Coult not write file");

        // attribute
        int i = 0;
        if (fwrite(&i, 1, 2, f) != 2) throw stlException_c("Coult not write file");

        triangleCount++;
      }
      else
      {
        fprintf(f,"  facet normal %9.4e %9.4e %9.4e\n", normal[0], normal[1], normal[2]);
        fprintf(f,"    outer loop\n");
        fprintf(f,"      vertex %9.4e %9.4e %9.4e\n", v1[0], v1[1], v1[2]);
        fprintf(f,"      vertex %9.4e %9.4e %9.4e\n", v2[0], v2[1], v2[2]);
        fprintf(f,"      vertex %9.4e %9.4e %9.4e\n", v3[0], v3[1], v3[2]);
        fprintf(f,"    endloop\n");
        fprintf(f,"  endfacet\n");
      }
    } while (e != sentinel);
  }

  delete poly;

  if (binaryMode)
  {
    // write out the triangle count into the header
    fseek(f, 80, SEEK_SET);
    if (fwrite(&triangleCount, 1, 4, f) != 4) throw stlException_c("Coult not write file");
  }
  else
  {
    fprintf(f, "endsolid\n");
  }

  fclose(f);
}

