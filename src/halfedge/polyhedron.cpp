/***************************************************************************
 *   Copyright (C) 2007 by Pablo Diaz-Gutierrez                            *
 *   pablo@ics.uci.edu                                                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "polyhedron.h"

#include "../lib/bt_assert.h"

#include <algorithm>
#include <stack>
#include <iterator>
#include <float.h>


using namespace std;

Polyhedron::~Polyhedron()
{
  for ( face_iterator fit=fBegin() ; fit!=fEnd() ; ++fit )
    delete *fit;
  _faces.clear();

  for ( edge_iterator eit=eBegin() ; eit!=eEnd() ; ++eit )
    delete *eit;
  _halfEdges.clear();

  for ( vertex_iterator vit=vBegin() ; vit!=vEnd() ; ++vit )
    delete *vit;
  _vertices.clear();
}

void eraseFaces ( Polyhedron* poly, const set<Face*>& faces )
{
  for ( set<Face*>::const_iterator sit=faces.begin() ; sit!=faces.end() ; ++sit )
  {
    Face* f = *sit;
    HalfEdge* he = f->edge();
    const HalfEdge* const sentinel = he;
    do
    {
      HalfEdge* he2 = he->next();
      if ( he->twin() )
        he->twin()->twin ( 0 );
      if ( he->dst()->edge() == he2 )
        he->dst()->edge ( 0 ); // WARNING: Think about this...
      poly->erase ( he );
      he = he2;
    }
    while ( he != sentinel ) ;
    poly->erase ( f );
  }
  for (Polyhedron::edge_iterator eit= poly->eBegin() ; eit!=poly->eEnd() ; ++eit )
  {
    if ((*eit)->dst()->edge()==0 && (*eit)->twin())
      (*eit)->dst()->edge((*eit)->twin());
  }
}

void insertAdjacentFaces ( Vertex* V, set<Face*>& faces )
{
  Vertex::edge_circulator eit = V->begin();
  const Vertex::edge_circulator sentinel = eit;
  do
  {
    HalfEdge* he = *eit;
    bt_assert ( he );
    Face* f = he->face();
    bt_assert ( f );
    if ( faces.end() == faces.find ( f ) )
      faces.insert ( f );
    ++eit;
  }
  while ( eit != sentinel );
}


// Adds all the faces that are adjacent to any vertices in 'verts'
void insertAdjacentFaces ( Polyhedron* poly, set<Face*>& faces, const set<const Vertex*>& verts )
{
  for ( Polyhedron::edge_iterator it=poly->eBegin() ; it!=poly->eEnd() ; ++it )
  {
    HalfEdge* he = *it;
    const Vertex* V = he->dst();
    if ( verts.end() != verts.find ( V ) )
    {
      Face* f = he->face();
      bt_assert ( f );
      if ( faces.end() == faces.find ( f ) )
        faces.insert ( f );
    }
  }
}

Vertex* Polyhedron::addVertex ( const Vector3Df& v )
{
  Vertex* V = new Vertex ( v, _vertices.size() );
  _vertices.push_back ( V );
  return V;
}

class heInfo
{
  public:

    HalfEdge * h;
    float angle;

    heInfo(HalfEdge * _h, float a) : h(_h), angle(a) {}
};

bool operator<(const heInfo & i1, const heInfo & i2) { return i1.angle < i2.angle; }

static void intersectionXY(Vector3Df start, Vector3Df dir, float & x, float & y)
{

  float t = - start.z()/dir.z();

  x = start.x() + t*dir.x();
  y = start.y() + t*dir.y();
}

static void intersectionXZ(Vector3Df start, Vector3Df dir, float & x, float & y)
{
  float t = - start.y()/dir.y();

  y = start.x() + t*dir.x();
  x = start.z() + t*dir.z();
}

static void intersectionYZ(Vector3Df start, Vector3Df dir, float & x, float & y)
{
  float t = - start.x()/dir.x();

  x = start.y() + t*dir.y();
  y = start.z() + t*dir.z();
}

void Polyhedron::finalize(void)
{
  bt_assert ( check ( false ) );

  // so that does this function do? it joins halfedges. After creating the polyhedon by doing a lot of
  // addFace calls you will have a lot of open halfedges where the twin is missing, this is resolved in
  // here
  //
  // the original function from the library did not do it properly for our need, it only acceped 2 halfedges
  // that connect 2 vertices, but we may have more, so here is our own version that can handle
  // an even number

  // first we collect all edges, that there are, we sort them by index
  // at the end there may be 1, 2 or more edges connecting 2 vertices
  multimap<pair<int,int>, HalfEdge*> connections;
  for ( edge_iterator it=eBegin() ; it!=eEnd() ; ++it )
  {
    HalfEdge* he = *it;
    Vertex* Va = he->prev()->dst();
    Vertex* Vb = he->dst();
    int a = Va->index();
    int b = Vb->index();
    if (a > b)
    {
      int c = a;
      a = b;
      b = c;
    }
    pair<int,int> idx ( a, b );
    connections.insert(pair<pair<int, int>, HalfEdge*>(idx, he));
  }

  set<pair<int, int> > handeled;
  for ( edge_iterator it=eBegin() ; it!=eEnd() ; ++it )
  {
    HalfEdge* he = *it;
    Vertex* Va = he->prev()->dst();
    Vertex* Vb = he->dst();
    int a = Va->index();
    int b = Vb->index();
    if (a > b)
    {
      int c = a;
      a = b;
      b = c;
    }
    pair<int,int> idx(a, b);

    // this edge has already been done
    if (handeled.find(idx) != handeled.end())
      continue;

    // add the edge, to make sure we don't process it again
    handeled.insert(idx);

    map<pair<int,int>, HalfEdge*>::iterator cit = connections.find(idx);
    // now we have the very first halfedge conection our 2 vertices, first let's count how many there are

    int n = 0;
    {
      map<pair<int,int>, HalfEdge*>::iterator cit2 = cit;
      while (cit2->first == idx)
      {
        n++;
        cit2++;
      }
    }

    // when there is only one halfedge here, we continue because that one will be closed later on
    if (n == 1) continue;

    // this case is the most common and simple to handle: simply connect the 2 halfedges
    if (n == 2)
    {
      HalfEdge* he = cit->second;
      cit++;
      HalfEdge* he2 = cit->second;

      bt_assert(he != he2);
      he->twin ( he2 );
      he2->twin ( he );
    }
    else if (n & 1)
    {
      // oops odd number of edges meet, we can't handle that, there is something wrong ... probably
      bt_assert(0);
    }
    else
    {
      // ok, now the case of an even number of halfedges...
      // the idea is as following: sort the edges by angle of their face (when looked along the edge)
      // always connect 2 faces that are side by side (angle wise) and surround the same filled area
      // so when going counter clockwise around the edge, we look for an halfedge that goes from top
      // to bottom. The next one has to be bottom to top and those are connected. Then go on

      vector<heInfo> info;

      // calculate the angles this is done in the following way (this is a bit of vector maths so be prepared):
      //
      // we span up 3 vectors: one is the vector of the edge  (A)
      // one is the perpendicular vector pointing to another vertex of the face of that edge (B)
      // and the last one is the vector perpendicular to those both (C)
      // the origin is the ending point of the edge O
      //
      // now we do the following for each edge e:
      // - take an other point P from the edge e
      // - solve xB+yC+zA+O = P
      // - angle = atan2(x, y)
      //
      // this will result in the angle 0 for the first edge as for that onethe result will be x=1, y=0
      // it is also obvious that the filled part for the face of that first edge will be with a positive angle
      // from that angle 0 on. so we have to look for the next face with the next angle to connect with
      //
      // BUUUT: we are not interested in the real angles, but rather in relative angles, so we don't need to
      // project the 3rd point onto the plane with the normal A, we can just as well use the xy, xz or yz plane,
      // we just have to make sure that A is not parallel to that plane

      Vector3Df A = cit->second->prev()->dst()->position() - cit->second->dst()->position();

      void (*projection)(Vector3Df, Vector3Df, float &, float &);
      bool inv = false;

      // choose plane to project onto as the rest of the code is identical except for the
      // projection, we use a function pointer here... not really object oriented, but
      // simple enough in this case
      if (fabs(A.z()) > 0.0001)
      {
        projection = intersectionXY;
        if (A.z() < 0) inv = true;
      }
      else if (fabs(A.y()) > 0.0001)
      {
        projection = intersectionXZ;
        if (A.y() < 0) inv = true;
      }
      else
      {
        projection = intersectionYZ;
        if (A.x() < 0) inv = true;
      }

      // calculate the origin
      float Ox, Oy;
      projection(cit->second->dst()->position(), A, Ox, Oy);
      float baseA = -1;

      while (cit->first == idx)
      {
        float Px, Py;
        projection(cit->second->next()->dst()->position(), A, Px, Py);
        Px -= Ox;
        Py -= Oy;

        float angle = atan2(Py, Px);
        if (inv) angle = -angle;

        if (baseA == -1)
          baseA = angle;

        angle -= baseA;
        while (angle < 0) angle += 2*M_PI;

        heInfo hi(cit->second, angle);
        info.push_back(hi);

        cit++;
      }

      // sort the info entries by angle
      sort(info.begin(), info.end());

      // now take one halfedge after the other

      unsigned int i = 0;

      while (i < info.size())
      {
        // the 2 edges must run in different directions
        bt_assert(info[i].h->prev()->dst() == info[i+1].h->dst());
        bt_assert(info[i+1].h->prev()->dst() == info[i].h->dst());
        info[i].h->twin(info[i+1].h);
        info[i+1].h->twin(info[i].h);
#if 0
        // if you enable this you can see, which faces are joined by the code above
        // this might be useful, if bugs turn up
        info[i].h->face()->_color = 1+i/2;
        info[i+1].h->face()->_color = 1+i/2;
#endif

        i += 2;
      }
    }
  }
  bt_assert(connections.size() <= (unsigned)numHalfEdges());

  bt_assert ( check ( false ) );
  closeSurface();
  bt_assert ( check ( true ) );
}


void Polyhedron::linkVerticesToEdges()
{
  for ( vertex_iterator vit=vBegin() ; vit!=vEnd() ; ++vit )
    ( *vit )->edge ( 0 );
  for ( edge_iterator it=eBegin() ; it!=eEnd() ; ++it )
  {
    HalfEdge* he = *it;
    bt_assert ( he->next() );
    he->dst()->edge ( he->next() );
  }
}

/**
 * Adds the necessary hole-tagged faces to complete the manifold.
 * @param connections Precomputed set of halfedges without a twin.
 */
void closeSurface ( Polyhedron* poly, map<pair<int,int>, HalfEdge*>& connections )
{
  cerr << "closeSurface with " << connections.size() << " unmatched half-edges\n";
  // Add half-edges around the detected boundaries, each connected set being assigned to a face/hole
  while ( !connections.empty() )
  {
    map<pair<int,int>, HalfEdge*>::iterator pit = connections.begin();
    pair<int,int> pts = pit->first;
    HalfEdge* e = pit->second;

    const int start = pts.second; // pts.second is the dst() of a surface half-edge
    int v = start;
    stack<int> s;
    s.push ( pts.second );
    s.push ( pts.first );
    set<int> visited;
    visited.insert ( pts.first );
    visited.insert ( pts.second );
    bt_assert ( visited.size() == s.size() );
    do
    {
      // Set 'e' to the next HalfEdge in the hole, crossing through pinched vertices.
      e = e->prev();
      set<const HalfEdge*> seen;
      while ( e->twin() && seen.end() == seen.find ( e ) )
      {
        seen.insert ( e );
        e = e->twin()->prev();
      }
      bt_assert ( e->face() );
      if ( seen.end() != seen.find ( e ) )
        break;

      // If 'v' has been visited, pop back the stack until we find it again.
      //      v = e->dst()->index();
      v = e->prev()->dst()->index();
      if ( visited.end() != visited.find ( v ) )
      {
        // Construct a face with the required vertices
        vector<int> corners;
        corners.push_back ( v );
        bt_assert ( s.top() != v );
        do
        {
          const int prev = s.top();
          corners.push_back ( prev );
          s.pop();
          bt_assert ( visited.end() != visited.find ( prev ) );
          visited.erase ( prev );
          bt_assert ( visited.size() == s.size() );
        }
        while ( s.top() != v ) ;
        bt_assert ( s.top() == v );
        s.pop();

        reverse ( corners.begin(), corners.end() );

        Face* f = poly->addFace ( corners );
        f->hole ( true );

        // Connect the edges of the new face to their twins
        HalfEdge* he = f->edge();
        const HalfEdge* const sentinel = he;
        do
        {
          HalfEdge* aux = he->prev();
          const int a = aux->dst()->index();
          const int b = he->dst()->index();
          bt_assert ( a != b );

          pair<int,int> ridx ( b, a );
          bt_assert ( connections.end() != connections.find ( ridx ) );
          HalfEdge* t = connections[ridx];
          connections.erase ( ridx );
          bt_assert ( t );
          bt_assert ( t != he );
          bt_assert ( t->dst() != he->dst() );
          t->twin ( he );
          he->twin ( t );

          bt_assert ( he->src()->index() == a );

          he = aux;
        }
        while ( he != sentinel ) ;
      }

      visited.insert ( v );
      s.push ( v );
      bt_assert ( visited.size() == s.size() );
    }
    while ( v != start ) ;
  }
}


void findUnmatchedHalfEdges ( Polyhedron* poly, map<pair<int,int>, HalfEdge*>& connections )
{
  for ( Polyhedron::edge_iterator it=poly->eBegin() ; it!=poly->eEnd() ; ++it )
  {
    HalfEdge* he = *it;
    if ( !he->twin() )
    {
      Vertex* Va = he->prev()->dst();
      Vertex* Vb = he->dst();
      int a = Va->index();
      int b = Vb->index();
      bt_assert ( a != b );
      pair<int,int> idx ( a, b );

      // Repeated half-edge after processing: The half-edge data structure only supports manifolds.
      bt_assert(connections.end() == connections.find(idx));

      bt_assert(connections.end() == connections.find ( pair<int,int> ( b, a ) ) );
      connections[idx] = he;
    }
    else
    {
      bt_assert ( he->twin()->twin() == he );
    }
  }
}


/// Adds the necessary hole-tagged faces to complete the manifold.
void Polyhedron::closeSurface ()
{
  // Find edges without a twin
  map<pair<int,int>, HalfEdge*> connections;
  findUnmatchedHalfEdges ( this, connections );
  if (!connections.empty())
    ::closeSurface ( this, connections );
}


HalfEdge* Polyhedron::edge ( int from, int to )
{
  Vertex* V = vertex(from);
  if (V)
    return V->edgeTo ( to );
  return 0;
}

const HalfEdge* Polyhedron::edge ( int from, int to ) const
{
  const Vertex* V = vertex(from);
  if (V)
    return V->edgeTo ( to );
  return 0;
}

bool faceIsHole ( const Face* f ) { return f->hole(); }

int Polyhedron::numHoles() const
{
  return count_if ( fBegin(), fEnd(), faceIsHole );
}


bool Polyhedron::check ( bool holesFilled ) const
{
  for ( unsigned i=0 ; i<_vertices.size() ; i++ )
  {
    const Vertex* V = _vertices[i];
    bt_assert ( V->index() == ( int ) i );
    bt_assert ( V->check ( holesFilled ) );
  }

  for ( Polyhedron::const_edge_iterator it=eBegin() ; it!=eEnd() ; ++it )
  {
    const HalfEdge* he = *it;
    bt_assert ( _halfEdges[he->index() ] == *it );
    bt_assert ( he->check ( holesFilled ) );
    if ( holesFilled )
    {
      // No adjacent holes, please.
      bt_assert ( !he->face()->hole() || !he->twin()->face()->hole() );
    }
  }

  for ( Polyhedron::const_face_iterator it=fBegin() ; it!=fEnd() ; ++it )
  {
    set<const HalfEdge*> visited;
    const Face* f = *it;
    bt_assert ( f == _faces[f->index() ] );
    Face::const_edge_circulator eit = f->begin();
    Face::const_edge_circulator sentinel = eit;
    do
    {
      bt_assert ( ( *eit )->face() == f );
      bt_assert ( visited.end() == visited.find ( *eit ) );
      visited.insert ( *eit );
      ++eit;
    }
    while ( eit != sentinel );
  }

  return true;
}

/// Tells if the Polyhedron contains a given Vertex
bool Polyhedron::contains ( const Vertex* V ) const
{
  if ( V->index() >= numVertices() )
    return false;
  return vertex ( V->index() ) == V;
}

/// Tells if the Polyhedron contains a given Face
bool Polyhedron::contains ( const Face* F ) const
{
  if ( F->index() >= numFaces() )
    return false;
  return face ( F->index() ) == F;
}

/// Tells if the Polyhedron contains a given HalfEdge
bool Polyhedron::contains ( const HalfEdge* he ) const
{
  if ( he->index() >= numHalfEdges() )
    return false;
  return halfedge ( he->index() ) == he;
}


Face* Polyhedron::_face ( const std::vector<int>& corners ) const
{
  bt_assert ( corners.size() >= 2 );
  const HalfEdge* he = edge ( corners[0], corners[1] );
  if ( !he )
    return 0;
  return (Face*) he->face();
}

Face* Polyhedron::addFace(int a, int b, int c)
{
  vector<int> corners(3);
  corners[0] = a;
  corners[1] = b;
  corners[2] = c;
  return addFace(corners);
}

Face* Polyhedron::addFace(const std::vector<int>& corners)
{
  Face* f = new Face(0);
  f->index(_faces.size());
  _faces.push_back(f);

  // Create one HalfEdge per consecutive pair of corners
  vector<HalfEdge*> edges;
  const unsigned nc = corners.size();
  HalfEdge* he = 0;
  for (unsigned i=0 ; i<nc ; ++i)
  {
    int c1 = corners[i];
    int c2 = corners[(i+1)%nc];
    he = new HalfEdge(_vertices[c2], f, _halfEdges.size());
    bt_assert(!he->twin());
    bt_assert(he->face());
    _halfEdges.push_back(he);
    _vertices[c1]->edge(he);
    edges.push_back(he);
  }
  f->edge(he);

  // Connect each HalfEdge to its next in the face
  bt_assert(nc == edges.size());
  const unsigned& ne = nc;
  for (unsigned e=0 ; e<ne ; e++)
    edges[e]->next( edges[(e+1)%ne] );

  return f;
}

/**
 * Erases a face, assuming it's been disconnected from the Polyhedron.
 * @param f Face to erase.
 */
void Polyhedron::erase(Face* f)
{
  const int i = f->index();
  if (_faces[i] != _faces.back())
  {
    swap(_faces[i], _faces.back());
    _faces[i]->index(i);
  }

  _faces.pop_back();
  delete f;
}

/**
 * Erases a halfedge, assuming it's been disconnected from the Polyhedron.
 * @param he HalfEdge to erase.
 */
void Polyhedron::erase(HalfEdge* he)
{
  const int i = he->index();
  if (_halfEdges[i] != _halfEdges.back())
  {
    swap(_halfEdges[i], _halfEdges.back());
    _halfEdges[i]->index(i);
  }

  _halfEdges.pop_back();
  delete he;
}

/**
 * Erases a vertex, assuming it's been disconnected from the Polyhedron.
 * @param v Vertex to erase.
 */
void Polyhedron::erase(Vertex* v)
{
  const int i = v->index();
  if (_vertices[i] != _vertices.back())
  {
    swap(_vertices[i], _vertices.back());
    _vertices[i]->index(i);
  }

  _vertices.pop_back();
  delete v;
}

void Polyhedron::loadVertices(const std::vector<float>& verts)
{
  bt_assert(0 == verts.size()%3);

  int nv = verts.size()/3;
  _vertices.resize(nv);
  for(int i=0 ; i<nv ; ++i)
  {
    Vector3Df v(verts[3*i], verts[3*i+1], verts[3*i+2]);
    _vertices[i] = new Vertex(v, i);
    bt_assert(!_vertices[i]->edge());
  }
}

