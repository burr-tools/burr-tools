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
#ifndef __MODIFIERS_H__
#define __MODIFIERS_H__

#include <vector>
class Polyhedron;
/** this class contains a list of faces (voxel+facenumer) pairs */
class faceList_c {

  private:

    struct face{
      long voxel;
      int faceNum;
    };

    std::vector<face> faces;

  public:

    faceList_c(void) {}

    void addFace(long voxel, int face);
    void removeFace(long voxel, int face);

    bool containsFace(long voxel, int face) const;

    void clear(void) { faces.clear(); }
};

void scalePolyhedron(Polyhedron & poly, float val);
void fillPolyhedronHoles(Polyhedron &poly, bool fillOutsides);

// inverts the inv polyhedron and adds those faces to poly
void joinPolyhedronInverse(Polyhedron & poly, const Polyhedron & inv, const faceList_c & holes, float holeSize);

#endif
