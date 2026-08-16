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

  faceList_c::face f = {voxel, face};

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
  uint32_t flags=0;
  uint8_t color = 0;

  // the hole is given in reverse order, so reverse it into our working set
  for (vector<Vertex*>::const_reverse_iterator rit = corners.rbegin(); rit < corners.rend(); ++rit)
  {
    working_set.push_back(*rit);
    flags |= (*rit)->edge()->face()->_flags;
    if (color == 0 && (*rit)->edge()->face()->_color != 0)
      color = (*rit)->edge()->face()->_color;
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
      Face *f = poly.addFace(pts);
      f->_flags = flags;
      f->_fb_face = -1;
      f->_color = color;

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
        Face *f = poly.addFace(pts);
	f->_flags = flags;
	f->_fb_face = -1;
        f->_color = color;

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
      uint8_t color = (*fit)->_color;

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
              f->_color = color;
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
	  // calculate angle between starting and ending face
	  Vector3Df n0 = f->normal();
	  Vector3Df n1 = (*fit)->normal();
	  float angle=fabs((n0 * n1)-1.0);

          // construct face to replace facets we're removing
          vector<int> face4(4);
          face4[0] = (*ei)->dst()->index();
          face4[1] = (*ei)->src()->index();
          edge = edge->prev()->prev();
          face4[2] = edge->dst()->index();
          face4[3] = edge->src()->index();

          f = poly.addFace(face4);   // add new one

	  if (angle<Epsilon) // if face is co-planar, mask wireframe bit
	    f->_flags = newflag & (~FF_WIREFRAME);
	  else
	    f->_flags = newflag;
	  f->_fb_face = -1; // prevent tubes being connected here
          f->_color = color;
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
    f->_fb_index = inside->_fb_index;
    f->_fb_face = inside->_fb_face;
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
    f->_fb_index = outside->_fb_index;
    f->_fb_face = outside->_fb_face;
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
    f->_fb_index = outside->_fb_index;
    f->_fb_face = outside->_fb_face;
  }

  // remove original faces
  faces_to_remove.insert(inside);
  faces_to_remove.insert(outside);
  eraseFaces(&poly,faces_to_remove);
  faces_to_remove.clear();
}

void joinPolyhedronInverse(Polyhedron & poly, const Polyhedron & inv, const faceList_c & holes, float holeSize)
{
  std::vector<std::pair<Face*,Face*> > face_pairs;
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
            (*fit2)->_fb_face  == fp->_fb_face &&
            fp != (*fit2) && fp->_fb_face!=-1) // no bevel/offset faces
        {
          face_pairs.push_back(pair<Face *,Face *>(fp,*fit2));
        }
      }
    }
  }
  for (std::vector<int>::size_type i=0; i<face_pairs.size(); i++)
    joinTubePairs(poly,face_pairs[i].first,face_pairs[i].second, holeSize);
  face_pairs.clear();
}

/* ------------------------------------------------------------------ */
/* merging of coplanar faces                                          */
/* ------------------------------------------------------------------ */

/* a 2d point used during the retriangulation of merged face groups */
struct point2D_s
{
  double x, y;
};

/* twice the signed area of the triangle a, b, c */
static double triArea2(const point2D_s & a, const point2D_s & b, const point2D_s & c)
{
  return (b.x-a.x)*(c.y-a.y) - (b.y-a.y)*(c.x-a.x);
}

static bool samePoint(const point2D_s & a, const point2D_s & b)
{
  return a.x == b.x && a.y == b.y;
}

/* is p inside the closed ccw triangle a, b, c */
static bool pointInTriangle(const point2D_s & p, const point2D_s & a, const point2D_s & b, const point2D_s & c, double eps)
{
  return triArea2(a, b, p) >= -eps && triArea2(b, c, p) >= -eps && triArea2(c, a, p) >= -eps;
}

/* do the open segments a-b and c-d properly cross one another */
static bool segmentsCross(const point2D_s & a, const point2D_s & b, const point2D_s & c, const point2D_s & d)
{
  double d1 = triArea2(c, d, a);
  double d2 = triArea2(c, d, b);
  double d3 = triArea2(a, b, c);
  double d4 = triArea2(a, b, d);

  return ((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) &&
         ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0));
}

/* triangulate the weakly simple ccw polygon given by poly (indices into
 * pts) by ear clipping. The emitted triangles are appended to tris as
 * index triples. Returns false, when no ear can be clipped any more even
 * though more than 2 corners are left
 */
static bool earClip(std::vector<int> poly, const std::vector<point2D_s> & pts, std::vector<int> & tris, double eps, double fatEps)
{
  unsigned int i = 0;

  while (poly.size() > 2)
  {
    bool clipped = false;

    // first look for an ear with a decent area, only when there is none
    // accept a sliver, that keeps nearly collinear corners from producing
    // degenerate triangles when there is a better choice
    for (int pass = 0; pass < 2 && !clipped; pass++)
    {
    double minArea = pass == 0 ? fatEps : eps;

    for (unsigned int tries = 0; tries < poly.size(); tries++, i++)
    {
      unsigned int ia = poly[(i  ) % poly.size()];
      unsigned int ib = poly[(i+1) % poly.size()];
      unsigned int ic = poly[(i+2) % poly.size()];

      const point2D_s & a = pts[ia];
      const point2D_s & b = pts[ib];
      const point2D_s & c = pts[ic];

      // the ear tip must be strictly convex
      if (triArea2(a, b, c) <= minArea)
        continue;

      // no other corner of the polygon may lie within the ear
      bool blocked = false;

      for (unsigned int j = 0; j < poly.size(); j++)
      {
        const point2D_s & p = pts[poly[j]];

        if (samePoint(p, a) || samePoint(p, b) || samePoint(p, c))
          continue;

        if (pointInTriangle(p, a, b, c, eps))
        {
          blocked = true;
          break;
        }
      }

      if (blocked)
        continue;

      tris.push_back(ia);
      tris.push_back(ib);
      tris.push_back(ic);

      poly.erase(poly.begin() + ((i+1) % poly.size()));
      clipped = true;
      break;
    }
    }

    if (!clipped)
      return false;
  }

  return true;
}

/* copy one face of the source polyhedron into the destination */
static void copyFace(Polyhedron * dst, vertexList_c & vl, const Face * f)
{
  std::vector<int> corners;

  Face::const_edge_circulator e = f->begin();
  Face::const_edge_circulator sentinel = e;

  do
  {
    const Vector3Df & p = (*e)->dst()->position();
    corners.push_back(vl.get(p.x(), p.y(), p.z()));
    e++;
  } while (e != sentinel);

  Face * f2 = dst->addFace(corners);

  f2->_flags = f->_flags;
  f2->_color = f->_color;
  f2->_fb_index = f->_fb_index;
  f2->_fb_face = f->_fb_face;
}

/* try to merge one group of connected coplanar faces into fewer, larger
 * triangles and add those to the destination polyhedron. Returns false
 * when anything goes wrong, in that case nothing has been added
 */
static bool mergeGroup(Polyhedron * dst, vertexList_c & vl, const std::vector<const Face *> & group, const std::set<const Face *> & inGroup)
{
  // collect the boundary edges of the group. An edge is on the boundary
  // when the face on the other side is not part of the group. The map is
  // keyed by the vertex index to keep everything deterministic
  std::multimap<int, std::pair<const Vertex *, const Vertex *> > boundary;

  for (unsigned int i = 0; i < group.size(); i++)
  {
    Face::const_edge_circulator e = group[i]->begin();
    Face::const_edge_circulator sentinel = e;

    do
    {
      const HalfEdge * he = *e;
      const HalfEdge * tw = he->twin();

      if (!tw || !tw->face() || tw->face()->hole() || inGroup.find(tw->face()) == inGroup.end())
      {
        const Vertex * src = he->prev()->dst();
        boundary.insert(std::make_pair(src->index(), std::make_pair(src, he->dst())));
      }

      e++;
    } while (e != sentinel);
  }

  // assemble the boundary edges into closed loops
  std::vector<std::vector<const Vertex *> > loops;

  while (!boundary.empty())
  {
    std::vector<const Vertex *> loop;

    const Vertex * start = boundary.begin()->second.first;
    const Vertex * cur = start;

    do
    {
      std::multimap<int, std::pair<const Vertex *, const Vertex *> >::iterator it = boundary.find(cur->index());

      if (it == boundary.end())
        return false;

      loop.push_back(cur);
      cur = it->second.second;
      boundary.erase(it);
    } while (cur != start);

    if (loop.size() < 3)
      return false;

    loops.push_back(loop);
  }

  if (loops.empty())
    return false;

  // set up a projection onto the plane of the group that keeps the
  // outside loop counter clockwise
  Vector3Df n = group[0]->normal();

  int u, v;

  if (fabs(n.x()) >= fabs(n.y()) && fabs(n.x()) >= fabs(n.z()))
  { u = 1; v = 2; if (n.x() < 0) { u = 2; v = 1; } }
  else if (fabs(n.y()) >= fabs(n.z()))
  { u = 2; v = 0; if (n.y() < 0) { u = 0; v = 2; } }
  else
  { u = 0; v = 1; if (n.z() < 0) { u = 1; v = 0; } }

  // project all loops to 2d
  std::vector<point2D_s> pts;
  std::vector<const Vertex *> ptVertex;
  std::vector<std::vector<int> > loops2;
  std::vector<double> loopArea;

  double totalArea = 0;

  for (unsigned int l = 0; l < loops.size(); l++)
  {
    std::vector<int> l2;

    for (unsigned int i = 0; i < loops[l].size(); i++)
    {
      point2D_s p;
      p.x = loops[l][i]->position()[u];
      p.y = loops[l][i]->position()[v];
      l2.push_back(pts.size());
      pts.push_back(p);
      ptVertex.push_back(loops[l][i]);
    }

    double area = 0;
    for (unsigned int i = 0; i < l2.size(); i++)
    {
      const point2D_s & a = pts[l2[i]];
      const point2D_s & b = pts[l2[(i+1) % l2.size()]];
      area += a.x*b.y - b.x*a.y;
    }
    area /= 2;

    loops2.push_back(l2);
    loopArea.push_back(area);
    totalArea += area;
  }

  if (totalArea <= 0)
    return false;

  // the loop with the largest area is the outline, all other loops must
  // be holes and run the other way around
  unsigned int outer = 0;
  for (unsigned int l = 1; l < loops2.size(); l++)
    if (loopArea[l] > loopArea[outer])
      outer = l;

  if (loopArea[outer] <= 0)
    return false;

  for (unsigned int l = 0; l < loops2.size(); l++)
    if (l != outer && loopArea[l] >= 0)
      return false;

  std::vector<int> polygon = loops2[outer];

  // connect the holes to the outline with bridges, this makes one big
  // weakly simple polygon that the ear clipper can handle
  std::vector<unsigned int> holeIdx;
  for (unsigned int l = 0; l < loops2.size(); l++)
    if (l != outer)
      holeIdx.push_back(l);

  for (unsigned int h = 0; h < holeIdx.size(); h++)
  {
    const std::vector<int> & hole = loops2[holeIdx[h]];

    // find the closest pair of corners between the polygon so far and
    // the hole where the connecting line crosses no edge
    int bestP = -1;
    int bestH = -1;
    double bestDist = 0;

    for (unsigned int a = 0; a < polygon.size(); a++)
      for (unsigned int b = 0; b < hole.size(); b++)
      {
        const point2D_s & pa = pts[polygon[a]];
        const point2D_s & pb = pts[hole[b]];

        double dist = (pa.x-pb.x)*(pa.x-pb.x) + (pa.y-pb.y)*(pa.y-pb.y);

        if (bestP != -1 && dist >= bestDist)
          continue;

        // the bridge must not cross the polygon, this hole or any of the
        // holes that still wait for their bridge
        bool crosses = false;

        for (unsigned int i = 0; i < polygon.size() && !crosses; i++)
          if (segmentsCross(pa, pb, pts[polygon[i]], pts[polygon[(i+1) % polygon.size()]]))
            crosses = true;

        for (unsigned int h2 = h; h2 < holeIdx.size() && !crosses; h2++)
        {
          const std::vector<int> & hl = loops2[holeIdx[h2]];
          for (unsigned int i = 0; i < hl.size() && !crosses; i++)
            if (segmentsCross(pa, pb, pts[hl[i]], pts[hl[(i+1) % hl.size()]]))
              crosses = true;
        }

        if (!crosses)
        {
          bestP = a;
          bestH = b;
          bestDist = dist;
        }
      }

    if (bestP == -1)
      return false;

    // splice the hole into the polygon, the 2 bridge corners appear twice
    std::vector<int> merged;

    for (int i = 0; i <= bestP; i++)
      merged.push_back(polygon[i]);

    for (unsigned int i = 0; i <= hole.size(); i++)
      merged.push_back(hole[(bestH + i) % hole.size()]);

    for (unsigned int i = bestP; i < polygon.size(); i++)
      merged.push_back(polygon[i]);

    polygon = merged;
  }

  // triangulate
  std::vector<int> tris;

  double eps = 1e-12 * loopArea[outer];
  if (eps < 1e-20) eps = 1e-20;

  double fatEps = 1e-7 * loopArea[outer];

  if (!earClip(polygon, pts, tris, eps, fatEps))
    return false;

  // the area of the triangles must add up to the area of the group
  double triArea = 0;
  for (unsigned int t = 0; t < tris.size(); t += 3)
    triArea += triArea2(pts[tris[t]], pts[tris[t+1]], pts[tris[t+2]]) / 2;

  if (fabs(triArea - totalArea) > 1e-4 * totalArea)
    return false;

  // no emitted triangle may be exactly degenerate, that would get a
  // NaN normal
  for (unsigned int t = 0; t < tris.size(); t += 3)
  {
    Vector3Df a = ptVertex[tris[t  ]]->position();
    Vector3Df b = ptVertex[tris[t+1]]->position();
    Vector3Df c = ptVertex[tris[t+2]]->position();

    Vector3Df cr = (b-a) ^ (c-a);

    if (cr * cr == 0)
      return false;
  }

  // all went well, add the triangles
  for (unsigned int t = 0; t < tris.size(); t += 3)
  {
    std::vector<int> corners;

    for (unsigned int c = 0; c < 3; c++)
    {
      const Vector3Df & p = ptVertex[tris[t+c]]->position();
      corners.push_back(vl.get(p.x(), p.y(), p.z()));
    }

    Face * f2 = dst->addFace(corners);

    f2->_flags = group[0]->_flags;
    f2->_color = group[0]->_color;
    f2->_fb_index = group[0]->_fb_index;
    f2->_fb_face = group[0]->_fb_face;
  }

  return true;
}

Polyhedron * mergeCoplanarFaces(const Polyhedron & src)
{
  // find groups of connected coplanar faces
  std::map<const Face *, int> groupOf;
  std::vector<std::vector<const Face *> > groups;

  for (Polyhedron::const_face_iterator it = src.fBegin(); it != src.fEnd(); it++)
  {
    const Face * f = *it;

    if (f->hole() || groupOf.find(f) != groupOf.end())
      continue;

    Vector3Df n = f->normal();
    double d = n * f->edge()->dst()->position();

    std::vector<const Face *> group;
    std::vector<const Face *> stack;

    groupOf[f] = groups.size();
    stack.push_back(f);

    while (!stack.empty())
    {
      const Face * cur = stack.back();
      stack.pop_back();
      group.push_back(cur);

      Face::const_edge_circulator e = cur->begin();
      Face::const_edge_circulator sentinel = e;

      do
      {
        const HalfEdge * tw = (*e)->twin();

        if (tw && tw->face() && !tw->face()->hole() && groupOf.find(tw->face()) == groupOf.end())
        {
          const Face * cand = tw->face();

          // the candidate is part of the group when it lies in the
          // same plane
          if (n * cand->normal() > 1 - 1e-6)
          {
            bool inPlane = true;

            Face::const_edge_circulator e2 = cand->begin();
            Face::const_edge_circulator sentinel2 = e2;

            do
            {
              if (fabs(n * (*e2)->dst()->position() - d) > 1e-5)
              {
                inPlane = false;
                break;
              }
              e2++;
            } while (e2 != sentinel2);

            if (inPlane)
            {
              groupOf[cand] = groups.size();
              stack.push_back(cand);
            }
          }
        }

        e++;
      } while (e != sentinel);
    }

    groups.push_back(group);
  }

  // build the new polyhedron
  Polyhedron * res = new Polyhedron();
  vertexList_c vl(res);

  for (unsigned int g = 0; g < groups.size(); g++)
  {
    if (groups[g].size() > 1)
    {
      std::set<const Face *> inGroup(groups[g].begin(), groups[g].end());

      // count the faces of the result so we can undo a failed merge
      int facesBefore = res->numFaces();

      if (mergeGroup(res, vl, groups[g], inGroup))
        continue;

      // the merge must not leave half added groups behind
      bt_assert(res->numFaces() == facesBefore);
    }

    for (unsigned int i = 0; i < groups[g].size(); i++)
      copyFace(res, vl, groups[g][i]);
  }

  return res;
}
