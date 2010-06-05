/***************************************************************************
 *   Copyright (C) 2007 by Pablo Diaz-Gutierrez   *
 *   pablo@ics.uci.edu   *
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

#include "face.h"
#include "vertex.h"
#include "halfedge.h"

#include "../lib/bt_assert.h"

// re-writing to not require twin edges (no use of tangent or src)

Vector3Df Face::normal() const
{
  Vector3Df N;

  const HalfEdge* sentinel = edge();
  const HalfEdge* he = sentinel;
  bt_assert(he);
  const HalfEdge* he2 = he->next();
  bt_assert(he2);
  const HalfEdge* he3 = he2->next();
  bt_assert(he3);

#ifdef OLD_NORMAL
  do
  {
    Vector3Df v(-he->tangent());
    Vector3Df v2(he2->tangent());
    float a = v.angle(v2);
    Vector3Df n = v2 ^ v;
    N += a*n;
    he = he2;
    he2 = he2->next();
  } while (he != sentinel) ;
#else
  Vector3Df v0=he->dst()->position();
  Vector3Df v1=he2->dst()->position();
  Vector3Df v2=he3->dst()->position();
  N = (v1-v0)^(v2-v0);
#endif

 
  N.normalize();

  bt_assert(!std::isnan(N.x()));
  return N;
}


Vector3Df Face::centroid() const
{
  const_edge_circulator e = begin();
  bt_assert(*e);
  const_edge_circulator sentinel = e;
  int n = 0;
  Vector3Df C(0,0,0);
  do
  {
    n++;
    C += (*e)->dst()->position();
    ++e;
  } while (e != sentinel) ;

  return C / n;
}

int Face::size() const
{
  Face::const_edge_circulator e = begin();
  bt_assert(*e);
  Face::const_edge_circulator sentinel = e;
  int n(0);
  do
  {
    n++;
    ++e;
  } while (sentinel != e) ;

  return n;
}

bool Face::contains(const HalfEdge* he) const
{
  Face::const_edge_circulator e = begin();
  bt_assert(*e);
  Face::const_edge_circulator sentinel = e;
  do
  {
    if (*e == he)
      return true;
    ++e;
  } while (sentinel != e) ;

  return false;
}

bool Face::contains(const Vertex* v) const
{
  Face::const_edge_circulator e = begin();
  bt_assert(*e);
  Face::const_edge_circulator sentinel = e;
  do
  {
    if ((*e)->dst() == v)
      return true;
    ++e;
  } while (sentinel != e) ;

  return false;
}

const HalfEdge* Face::fromVertex(int v) const
{
  Face::const_edge_circulator e = begin();
  bt_assert(*e);
  Face::const_edge_circulator sentinel = e;
  do
  {
    if ((*e)->dst()->index() == v)
      return (*e)->next();
    ++e;
  } while (sentinel != e) ;

  return 0;
}

const HalfEdge* Face::toVertex(int v) const
{
  Face::const_edge_circulator e = begin();
  bt_assert(*e);
  Face::const_edge_circulator sentinel = e;
  do
  {
    if ((*e)->dst()->index() == v)
      return *e;
    ++e;
  } while (sentinel != e) ;

  return 0;
}

const HalfEdge* Face::fromVertex(Vertex* v) const
{
  return fromVertex(v->index());
}

const HalfEdge* Face::toVertex(Vertex* v) const
{
  return toVertex(v->index());
}


bool Face::adjacent ( const Face* f2 ) const
{
  Face::const_edge_circulator eit = begin();
  bt_assert(*eit);
  Face::const_edge_circulator sentinel = eit;
  do
  {
    if ( ( *eit )->twin()->face() == f2 )
      return true;
    ++eit;
  }
  while ( eit != sentinel );
  return false;
}

bool Face::degenerate() const
{
  Face::const_edge_circulator eit = begin();
  bt_assert(*eit);
  Face::const_edge_circulator sentinel = eit;
  do
  {
    const HalfEdge* he = *eit;
    if (he->src() == he->dst())
      return true;
    ++eit;
  }
  while ( eit != sentinel );
  return false;
}

