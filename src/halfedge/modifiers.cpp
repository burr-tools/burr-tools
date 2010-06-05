#include "modifiers.h"
#include "polyhedron.h"
#include <set>
#include <map>
#include "../lib/voxel.h"

using namespace std;
#define Epsilon 1.0e-5

void bevelPolyhedron(Polyhedron & poly, float val)
{
}

void offsetPolyhedron(Polyhedron & poly, float val)
{
}

#ifdef DEBUG
void dumpVerts(Polyhedron &poly)
{
  printf("vertices=%d\n",poly.numVertices());
  for (Polyhedron::vertex_iterator vit = poly.vBegin(); vit != poly.vEnd(); vit++)
    {
      printf("v%d=(%4.2f,%4.2f,%4.2f)\n",(*vit)->index(),
	     (*vit)->position().x(),(*vit)->position().y(),
	     (*vit)->position().z());
    }
}
void dumpFace(Face *f)
{
  printf("face(%d:%x): ", f->index(),f->_flags);
  HalfEdge* he = f->edge();
  const HalfEdge* const sentinel = he;
  do {
    printf("e%d,",he->index());
    if (he->twin())
      printf("f%d,",he->twin()->face()->index());
    else
      printf("*");
    //printf("(%4.2f,%4.2f,%4.2f) ",he->dst()->position().x(),
    //       he->dst()->position().y(),he->dst()->position().z());
    printf("v%d ",he->dst()->index());
    he=he->next();
  } while (sentinel != he);
  printf("\n");
}
void dumpFaces(Polyhedron &poly, bool chkDisconnect)
{
  for (Polyhedron::face_iterator fit = poly.fBegin(); fit != poly.fEnd(); fit++)
    if (chkDisconnect)
    {
      HalfEdge* he = (*fit)->edge();
      const HalfEdge* const sentinel = he;
      do
      {
	if (he->twin()==0)
	  break;
	he = he->next();
      }
      while (he != sentinel);

      if (he->twin()==0)
      {
	dumpFace(*fit);
      }

    }
    else
      dumpFace(*fit);
}

#endif

int findBestTri(vector<Vertex*> vs, int &offset)
{
  int vsize = vs.size();
  if (vsize<4)
  {
    offset=0;
    return vsize;
  }
  int best_tri=vsize;
  int good_tri=vsize;
  int good_quad=vsize;
  float best_angle=999999;
  for (offset=0; offset<vsize; offset++)
  {
    Vector3Df v0=vs[offset]->position();
    Vector3Df n0=vs[offset]->edge()->face()->normal();
    Vector3Df v1=vs[(offset+1)%vsize]->position();
    Vector3Df n1=vs[(offset+1)%vsize]->edge()->face()->normal();
    Vector3Df v2=vs[(offset+2)%vsize]->position();
    Vector3Df n=(v1-v0)^(v2-v0);
    if (n.squaredModule()<Epsilon)
    {
      continue;
    }
    float angle0, angle1;
    n.normalize();
    angle0=fabs(n.angle(n0));
    angle1=fabs(n.angle(n1));
    Vector3Df v3=vs[(offset+3)%vsize]->position();
    float dist = n * (v3-v0);
    if (angle0<Epsilon && angle1<Epsilon) // really good triangl
    {
      if (fabs(dist)<Epsilon) // found a good quad
      {
	return 4;
      }
      else
      {
	return 3;
      }
    }
    else
    {
      if (angle0<best_angle)
      {
	best_angle=angle0;
	best_tri = offset;
      }
      if (angle1<best_angle)
      {
	best_angle=angle1;
	best_tri = offset;
      }
      if (fabs(dist)<Epsilon) // found a good quad
      {
	good_quad = offset;
      }
    }

  }
  if (good_quad<vsize)
  {
    offset = good_quad;
    return 4;
  }
  else if (best_tri<vsize)
  {
    offset=best_tri;
  }
  else
  {
    offset=0;
  }
  return 3;
}

// this routine tries to find the minimal set of tris and quads to cap the edge list

void findOptimizedFaces(Polyhedron &poly, const vector<Vertex*>& corners)
{
  vector<Vertex*> working_set;
  vector<int> pts;
  for (vector<Vertex*>::const_reverse_iterator rit = corners.rbegin(); rit < corners.rend(); ++rit)
    working_set.push_back(*rit);
#ifdef DEBUG
  printf("closing hole of %d verts\n",working_set.size());
#endif
  while (working_set.size()>2)
  {
    int offset;
    int ret=findBestTri(working_set,offset);
    int old_size=working_set.size();

#ifdef DEBUG
    for (int i=0; i<old_size; i++)
      printf("v%d ",working_set[i]->index());
    printf("- ret = %d, offset=%d\n",ret,offset);
#endif
    for (int j=0; j<ret; j++)
      pts.push_back(working_set[(offset+j)%old_size]->index());
    Face *f=poly.addFace(pts);
#ifdef DEBUG
    dumpFace(f);
#endif
    pts.clear();

    if (ret==4)
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
      working_set.erase(working_set.begin()+(offset+1)%old_size);
  }

}

void fillPolyhedronHoles(Polyhedron & poly, bool fillOutsides)
{
  set<Face*> faces_to_remove;
#ifdef DEBUG
  dumpVerts(poly);
  printf("faces(before)=%d\n",poly.numFaces());
#endif
  for (Polyhedron::face_iterator fit = poly.fBegin(); fit != poly.fEnd(); ++fit)
  {
    if (((*fit)->_flags & (FF_OFFSET_FACE|
			   FF_BEVEL_FACE))==0) // find untouched face
    {
      Face::edge_circulator ei = (*fit)->begin();
      Face::edge_circulator sentinel=ei;
      Face *f = *fit;
      set<Face*> faces;
      do
      {
	HalfEdge *edge = (*ei);
	uint32_t newflag=0;
	if ((*ei)->twin())
	do
	{
	  if ((f->_flags & (FF_OFFSET_FACE|FF_BEVEL_FACE))
	      && !(f->_flags&FF_PROCESSED_FACE))
	  {
	    f->_flags |= FF_PROCESSED_FACE;
	    newflag |= f->_flags;
	    if (fillOutsides)
	    faces.insert(f);
	  }
	  f=edge->twin()->face();
	  edge=edge->twin()->next()->next();
	} while (f->_flags & (FF_OFFSET_FACE|FF_BEVEL_FACE));

	if (faces.size()>1) // opportunity to simplify
	{
          for (set<Face*>::const_iterator sit=faces.begin() ; sit!=faces.end() ; ++sit )
	    faces_to_remove.insert(*sit);

	  // construct face to replace facets we're removing
	  vector<int> face4(4);
          face4[0]=(*ei)->dst()->index(); face4[1]=(*ei)->src()->index();
	  edge=edge->prev()->prev();
	  face4[2]=edge->dst()->index(); face4[3]=edge->src()->index();

	  f=poly.addFace(face4);	  // add new one
	  f->_flags=newflag;

#ifdef DEBUG
	  printf("adding ");
	  dumpFace(f);
#endif
	}
	faces.clear();

        ei++;
      }
      while (ei != sentinel);
    }
  }

  if (faces_to_remove.size())
  {
#ifdef DEBUG
    for (set<Face*>::const_iterator sit=faces_to_remove.begin() ; sit!=faces_to_remove.end() ; ++sit )
    {
      printf("removing ");
      dumpFace(*sit);
    }
#endif
    eraseFaces(&poly, faces_to_remove);	  // erase old faces
    faces_to_remove.clear();
  }

  // remove any unprocessed faces that have detached edges
  while (1)
  {
    for (Polyhedron::face_iterator fit = poly.fBegin(); fit != poly.fEnd(); ++fit)
    {
      if (((*fit)->_flags & FF_PROCESSED_FACE)==0)
      {
	if ((*fit)->_flags&FF_BEVEL_FACE) // beveled corners - remove detached...
	{
	  Face::edge_circulator ei = (*fit)->begin();
	  Face::edge_circulator sentinel=ei;
	  do
	  {
	    if ((*ei)->twin()==0)
	      break;
	    ei++;
	  }
	  while (ei != sentinel);

	  if ((*ei)->twin()==0)
	  {
	    faces_to_remove.insert(*fit);
	  }
	}
	else if ((*fit)->_flags&FF_OFFSET_FACE) // untouched offsets are holes
	{
	  faces_to_remove.insert(*fit);
	}
      }
    }

    if (faces_to_remove.size())
    {
#ifdef DEBUG
      for (set<Face*>::const_iterator sit=faces_to_remove.begin() ; sit!=faces_to_remove.end() ; ++sit )
      {
	printf("removing ");
	dumpFace(*sit);
      }
#endif
      eraseFaces(&poly, faces_to_remove);	  // erase old faces
      faces_to_remove.clear();
    }
    else
      break;
  }

  // construct a list of unpaired edges - these are holes that we need to cap
  // keep track of their src vertex, as
  map<pair<Vertex*,Vertex*>,HalfEdge*> conn;
  map<pair<Vertex*,Vertex*>,HalfEdge*>::iterator cit;
  set<Vertex*> handled;
  vector<Vertex*> pts_list;
  int pairs_fixed=0;
  for (Polyhedron::edge_iterator eit = poly.eBegin(); eit != poly.eEnd(); eit++)
  {
    if ((*eit)->twin()==0)
    {
      pair<Vertex*,Vertex*> idx((*eit)->dst(),(*eit)->prev()->dst());
      cit=conn.find(idx);
      if (cit==conn.end())
      {
	pair<Vertex*,Vertex*> idx2 ((*eit)->prev()->dst(),(*eit)->dst());
	conn.insert(pair<pair<Vertex*,Vertex*>,HalfEdge*>(idx2,(*eit)));
      }
      else
      {
	cit->second->twin(*eit); (*eit)->twin(cit->second);
	pairs_fixed++;
	conn.erase(cit);
      }
    }
  }

#ifdef DEBUG
  dumpFaces(poly,1);
#endif

  map<Vertex*,HalfEdge*> seams;
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
    while (handled.find(v)==handled.end())
    {
      handled.insert(v);
      pts_list.push_back(v);
      v->edge(e);
      v = e->dst();
      e = seams.find(v)->second;
    }
    if (pts_list.size())
    {
#ifdef DEBUG
      for (vector<Vertex*>::reverse_iterator rit = pts_list.rbegin(); rit < pts_list.rend(); ++rit)
	printf("v%d ",(*rit)->index());
      printf("\n");
#endif
      findOptimizedFaces(poly,pts_list);
      pts_list.clear();
    }
  }

  poly.finalize();

}
void scalePolyhedron(Polyhedron & poly, float val)
{
   for (Polyhedron::vertex_iterator it = poly.vBegin(); it != poly.vEnd(); it++)
   {
      (*it)->position((*it)->position()*val);
   }
}

