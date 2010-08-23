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
#include "modifiers.h"
#include "polyhedron.h"
#include <set>
#include <map>
#include "../lib/voxel.h"
#include "../lib/stl.h"

using namespace std;

const float Epsilon=1.0e-5;

void faceList_c::addFace(long voxel, int face)
{
  if (containsFace(voxel, face)) return;

  struct face f;
  f.voxel = voxel;
  f.faceNum = face;

  faces.push_back(f);
}

void faceList_c::removeFace(long voxel, int face)
{
  for (unsigned int i = 0; i < faces.size(); i++)
    if (faces[i].voxel == voxel && faces[i].faceNum == face)
    {
      faces.erase(faces.begin()+i);
      return;
    }
}

bool faceList_c::containsFace(long voxel, int face) const
{
  for (unsigned int i = 0; i < faces.size(); i++)
    if (faces[i].voxel == voxel && faces[i].faceNum == face)
    {
      return true;
    }

  return false;
}

/* this routine attempts to find a good quad or triangle in which to
 * fill part of the hole.  It does so by comparing the normals of the
 * border faces against the potential triangles and quads
 */
static int findBestTriOrQuad(vector<Vertex*> vs, int &offset)
{
  int vsize = vs.size();

  if (vsize<4)
  {
    offset=0;
    return vsize;
  }

  int good_quad = vsize;

  for (offset = 0; offset < vsize; offset++)
  {
    // get vertex of the potential polygon
    Vector3Df v0 = vs[offset]->position();
    Vector3Df v1 = vs[(offset+1)%vsize]->position();
    Vector3Df v2 = vs[(offset+2)%vsize]->position();

    // calculate normal for this potential polygon
    Vector3Df n = (v1-v0)^(v2-v0);

    if (n.squaredModule() < Epsilon*Epsilon)
    {
      // hole has string of segments that are colinear
      // best filled w/ a triangle fan
      return 0;
    }

    n.normalize();

    // get normals for neighboring faces and calculate angle again
    Vector3Df n0 = vs[(offset+1)%vsize]->edge()->face()->normal();
    Vector3Df n1 = vs[(offset+2)%vsize]->edge()->face()->normal();

    float angle0=fabs((n * n0)-1.0);
    float angle1=fabs((n * n1)-1.0);

    Vector3Df v3=vs[(offset+3)%vsize]->position();

    // calculate distance from 4th point to plane given by first 3 points
    // it is planar if distance is 0 (or close to it)
    float dist = fabs(n * (v3-v0));

    if (angle0 < Epsilon && angle1 < Epsilon) // really good poly
    {
      if (dist < Epsilon) // found a good quad
      {
        return 4;
      }
      else
      {
        return 3;
      }
    }
    else if (dist<Epsilon) // found a good quad
    {
      good_quad = offset;
    }
  }

  if (good_quad<vsize)
  {
    offset = good_quad;
    return 4;
  }

  // couldn't find a really good candidate, best off using tri-fan filling
  return 0;
}

// this routine tries to find the best set of tris and quads to cap the edge list

static void findOptimizedFaces(Polyhedron &poly, const vector<Vertex*>& corners)
{
  vector<Vertex*> working_set;

  // the hole is given in reverse order, so reverse it into our working set
  for (vector<Vertex*>::const_reverse_iterator rit = corners.rbegin(); rit < corners.rend(); ++rit)
  {
    working_set.push_back(*rit);
  }

  // finished when no polygons are left
  while (working_set.size()>2)
  {
    int offset;
    int ret = findBestTriOrQuad(working_set,offset);
    int old_size = working_set.size();
    vector<int> pts;

    if (ret)
    {
      // create points list of new face
      for (int j = 0; j < ret; j++)
      {
        pts.push_back(working_set[(offset+j) % old_size]->index());
      }
      poly.addFace(pts);

      if (ret == 4)
      {
        if (((offset+2)%old_size)>((offset+1)%old_size))
        {
          working_set.erase(working_set.begin()+((offset+2)%old_size));
          working_set.erase(working_set.begin()+((offset+1)%old_size));
        }
        else
        {
          working_set.erase(working_set.begin()+((offset+1)%old_size));
          working_set.erase(working_set.begin()+((offset+2)%old_size));
        }
      }
      else
      {
        working_set.erase(working_set.begin()+(offset+1)%old_size);
      }
    }
    else // couldn't create a good quad or triangle, fill w/ triangle-fan
    {
      // calculate center of the hole, which for this case is just the average

      Vector3Df center;
      for (unsigned int i=0; i<working_set.size(); i++)
      {
        center += working_set[i]->position();
      }

      center /= (float) working_set.size();
      Vertex *v = poly.addVertex(center);

      // for an n-sided hole, add n-triangles
      for (unsigned int j = 0; j < working_set.size(); j++)
      {
        pts.push_back(working_set[j]->index());
        pts.push_back(working_set[(j+1)%working_set.size()]->index());
        pts.push_back(v->index());
        poly.addFace(pts);

        pts.clear();
      }
      return;
    }
  }
}

/* this routine attempts to simplify the mesh, reducing bevelled and offset
 * faces into less polygons, filling in the 'grooves' in the surface
 */
void fillPolyhedronHoles(Polyhedron & poly, bool fillOutsides)
{
  set<Face*> faces_to_remove;


  for (Polyhedron::face_iterator fit = poly.fBegin(); fit != poly.fEnd(); ++fit)
  {
    // start at 'real' face (non-bevelled and non-offset)
    if (((*fit)->_flags & (FF_OFFSET_FACE | FF_BEVEL_FACE)) == 0)
    {
      Face::edge_circulator ei = (*fit)->begin();
      Face::edge_circulator sentinel = ei;
      Face *f = *fit;
      set<Face*> faces;

      // iterate through all edges of the starting face
      do
      {
        HalfEdge *edge = (*ei);
        uint32_t newflag = 0;
        if ((*ei)->twin()) // must check - as we can erase faces as we go
        {
          /* traverse faces connected to this face, making list of all
           * bevelled and offset faces between starting face and next
           * 'real' face
           */
          do
          {
            if (    (f->_flags & (FF_OFFSET_FACE | FF_BEVEL_FACE))
                && !(f->_flags & FF_PROCESSED_FACE))
            {
              f->_flags |= FF_PROCESSED_FACE;
              newflag |= f->_flags;
              if (fillOutsides)
              {
                faces.insert(f);
              }
            }
            f = edge->twin()->face();
            edge=edge->twin()->next()->next();
          } while (f->_flags & (FF_OFFSET_FACE | FF_BEVEL_FACE));
        }

        // reduce multiple faces into single face
        if (faces.size()>1)
        {
          // construct master list of faces to be removed
          for (set<Face*>::const_iterator sit = faces.begin(); sit != faces.end(); ++sit)
          {
            faces_to_remove.insert(*sit);
          }

          // construct face to replace facets we're removing
          vector<int> face4(4);
          face4[0] = (*ei)->dst()->index();
          face4[1] = (*ei)->src()->index();
          edge = edge->prev()->prev();
          face4[2] = edge->dst()->index();
          face4[3] = edge->src()->index();

          f = poly.addFace(face4);   // add new one
          f->_flags = newflag;

        }

        faces.clear();
        ei++;
      }
      while (ei != sentinel);
    }
  }

  // erase old faces
  if (faces_to_remove.size())
  {
    eraseFaces(&poly, faces_to_remove);
    faces_to_remove.clear();
  }

  // remove any unprocessed faces that have detached edges

  while (1)
  {
    for (Polyhedron::face_iterator fit = poly.fBegin(); fit != poly.fEnd(); ++fit)
    {
      if (((*fit)->_flags & FF_PROCESSED_FACE) == 0)
      {
        if ((*fit)->_flags&FF_BEVEL_FACE) // beveled corners - remove detached...
        {
          Face::edge_circulator ei = (*fit)->begin();
          Face::edge_circulator sentinel = ei;
          do
          {
            if ((*ei)->twin() == 0)
            {
              break;
            }
            ei++;
          }
          while (ei != sentinel);

          if ((*ei)->twin() == 0)
          {
            faces_to_remove.insert(*fit);
          }
        }
        else if ((*fit)->_flags & FF_OFFSET_FACE) // untouched offsets are holes
        {
          faces_to_remove.insert(*fit);
        }
      }
    }

    if (faces_to_remove.size())
    {
      eraseFaces(&poly, faces_to_remove);  // erase old faces
      faces_to_remove.clear();
    }
    else
    {
      break;
    }
  }

  /* construct a list of unpaired edges - these are holes that we need to cap
   * keep track of their src vertex, as that is needed for addFaces...
   * mate up any matched pairs that might have been disconnected due to removal
   * (similar to what finalize does)
   */

  map<pair<Vertex*,Vertex*>,HalfEdge*> conn;
  map<pair<Vertex*,Vertex*>,HalfEdge*>::iterator cit;
  vector<Vertex*> pts_list;
  int pairs_fixed = 0;

  for (Polyhedron::edge_iterator eit = poly.eBegin(); eit != poly.eEnd(); eit++)
  {
    if ((*eit)->twin() == 0)
    {
      pair<Vertex*,Vertex*> idx((*eit)->dst(),(*eit)->prev()->dst());
      cit = conn.find(idx);
      if (cit == conn.end())
      {
        pair<Vertex*,Vertex*> idx2 ((*eit)->prev()->dst(),(*eit)->dst());
        conn.insert(pair<pair<Vertex*,Vertex*>,HalfEdge*>(idx2,(*eit)));
      }
      else
      {
        cit->second->twin(*eit);
        (*eit)->twin(cit->second);
        pairs_fixed++;
        conn.erase(cit);
      }
    }
  }

  map<Vertex*,HalfEdge*> seams;
  set<Vertex*> handled;

  // now need to reduce the structure to just Vertex & HalfEdge
  for (cit=conn.begin(); cit!=conn.end(); ++cit)
  {
    pair<Vertex*,Vertex*> vpr = (*cit).first;
    seams.insert(pair<Vertex*,HalfEdge*>(vpr.first,(*cit).second));
  }

  for (map<Vertex*,HalfEdge*>::iterator i=seams.begin(); i!=seams.end(); ++i)
  {
    Vertex *v = (*i).first;
    HalfEdge *e = (*i).second;

    // iterate around half edges until a loop is found
    while (handled.find(v) == handled.end())
    {
      handled.insert(v);
      pts_list.push_back(v);
      v->edge(e);
      v = e->dst();
      e = seams.find(v)->second;
    }

    if (pts_list.size())
    {
      // find best capping for hole
      findOptimizedFaces(poly,pts_list);
      pts_list.clear();
    }
  }

  // connect up any unmatched edge pairs created during capping
  poly.finalize();

}

void scalePolyhedron(Polyhedron & poly, float val)
{
   for (Polyhedron::vertex_iterator it = poly.vBegin(); it != poly.vEnd(); it++)
   {
      (*it)->position((*it)->position()*val);
   }
}

static void joinTubePairs(Polyhedron & poly, Face *inside, Face *outside, float holeSize)
{
  unsigned int i;
  Vector3Df center(0,0,0);
  std::vector<int> corners;
  std::vector<int> tube_corners;
  set<Face*> faces_to_remove;
  if (inside->size() != outside->size())
  {
    return;
  }

  // calculate center of inside face
  // by averaging all the vertices of the face

  Face::const_edge_circulator ei = inside->begin();
  Face::const_edge_circulator sentinel=ei;
  do
  {
    center += (*ei)->dst()->position();
    ei++;
  }
  while (ei!=sentinel);

  center /= (float)inside->size();
  ei = inside->begin();
  sentinel=ei;

  // generate "hole" vertices on inside face by interpolating each vertex
  // towards the center of the face

  do
  {
    Vertex *v = poly.addVertex((*ei)->dst()->position()*holeSize+center*(1.0-holeSize));
    tube_corners.push_back(v->index());
    corners.push_back(v->index());
    corners.push_back((*ei)->dst()->index());
    ei++;
  }
  while (ei!=sentinel);

  // create new faces for the inside to create the hole

  for (i = 0; i < corners.size(); i+=2)
  {
    vector<int> face4(4);
    face4[0] = corners[i];
    face4[1] = corners[i+1];
    face4[2] = corners[(i+3)%corners.size()];
    face4[3] = corners[(i+2)%corners.size()];

    Face *f = poly.addFace(face4);   // add new one
    f->_flags = inside->_flags;
  }

  // repeat the same idea for the outside face
  // generate the center (average) of the outside face

  ei=outside->begin();
  sentinel=ei;
  center.set(0,0,0);

  do
  {
    center += (*ei)->dst()->position();
    ei++;
  }
  while (ei!=sentinel);

  center /= (float)inside->size();
  ei=outside->begin();
  sentinel=ei;
  corners.clear();

  // generate outside "hole" vertices by interpolating vertex towards center

  do
  {
    Vertex *v = poly.addVertex((*ei)->dst()->position()*holeSize+center*(1.0-holeSize));
    tube_corners.push_back(v->index());
    corners.push_back(v->index());
    corners.push_back((*ei)->dst()->index());
    ei++;
  }
  while (ei!=sentinel);

  // create new faces for the outside to create the hole

  for (i = 0; i < corners.size(); i+=2)
  {
    vector<int> face4(4);
    face4[0] = corners[i];
    face4[1] = corners[i+1];
    face4[2] = corners[(i+3)%corners.size()];
    face4[3] = corners[(i+2)%corners.size()];

    Face *f = poly.addFace(face4);   // add new one
    f->_flags = outside->_flags;
  }
  // since edge iterators are arbitrary to the face, need to find closest
  // pair between inside and outside hole

  const unsigned int tube_size = tube_corners.size()/2;
  double max_dist=1e99;
  int closest=-1;

  for (i = tube_size; i < tube_size*2; i++)
  {
    Vector3Df  temp = poly.vertex(tube_corners[i])->position()-
                      poly.vertex(tube_corners[0])->position();
    double temp_dist = temp.squaredModule();

    if (temp_dist < max_dist)
    {
      closest=i;
      max_dist = temp_dist;
    }
  }

  // connect the inside and outside holes with an N-sided tube

  for (i = 0; i < tube_size; i++)
  {
    vector<int> face4(4);
    face4[0] = tube_corners[i];
    face4[1] = tube_corners[(i+1)%tube_size];
    face4[2] = tube_corners[tube_size+((tube_size+closest-(1+i))%tube_size)];
    face4[3] = tube_corners[tube_size+((tube_size+closest-i)%tube_size)];

    Face *f = poly.addFace(face4);   // add new one
    f->_flags = outside->_flags|inside->_flags;
  }

  // remove original faces
  faces_to_remove.insert(inside);
  faces_to_remove.insert(outside);
  eraseFaces(&poly,faces_to_remove);
  faces_to_remove.clear();
}

void joinPolyhedronInverse(Polyhedron & poly, const Polyhedron & inv, const faceList_c & holes, float holeSize)
{
  int vertexOffset = poly.numVertices();

  for (int i = 0; i < inv.numVertices(); i++)
  {
    poly.addVertex(inv.vertex(i)->position());
  }

  for (Polyhedron::const_face_iterator fit = inv.fBegin(); fit != inv.fEnd(); ++fit)
  {
    Face::const_edge_circulator ei = (*fit)->begin();
    Face::const_edge_circulator sentinel=ei;
    Face *fp;

    std::vector<int> corners;

    do
    {
      corners.insert(corners.begin(), vertexOffset + (*ei)->src()->index());
      ei++;
    }
    while (ei != sentinel);

    fp = poly.addFace(corners);
    fp->_flags |= FF_INSIDE_FACE;
    fp->_fb_index = (*fit)->_fb_index;
    fp->_fb_face = (*fit)->_fb_face;
    if (holes.containsFace(fp->_fb_index, fp->_fb_face))
    {
      for (Polyhedron::face_iterator fit2 = poly.fBegin(); fit2 != poly.fEnd(); ++fit2)
      {
        if ((*fit2)->_fb_index == fp->_fb_index &&
            (*fit2)->_fb_face  == fp->_fb_face && fp != (*fit2))
        {
          joinTubePairs(poly,fp,*fit2, holeSize);
        }
      }
    }
  }
}
