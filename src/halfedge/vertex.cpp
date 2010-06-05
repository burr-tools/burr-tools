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

#include "vertex.h"

#include "halfedge.h"
#include "face.h"

#include "../lib/bt_assert.h"

using namespace std;


Vector3Df Vertex::normal() const
{
  if (!_edge)
    return Vector3Df(0, 0, 0);

  const_edge_circulator e = begin();
  const_edge_circulator sentinel = e;

  Vector3Df N;
  do
  {
    bt_assert(this != (*e)->dst());
    Vector3Df v((*e)->tangent());
    ++e;

    // Do not use the normal of faces which are actually holes
    if ((*e)->face()->hole())
      continue;

    Vector3Df v2((*e)->tangent());
    float a = v.angle(v2);
    if (a > 0.0001)
    {
      Vector3Df n = v2 ^ v;
      N += a*n;
    }
  } while (e != sentinel) ;

  N.normalize();
  return N;
}

int Vertex::valence() const
{
  if (!_edge)
    return 0;
  const_edge_circulator e = begin();
  const_edge_circulator sentinel = e;

  int n(0);
  do
  {
    n++;
    ++e;
  } while (e != sentinel) ;

  return n;
}


const HalfEdge* Vertex::edgeAdjacentTo(const Face* f) const
{
  if (!_edge)
    return 0;
  const_edge_circulator e = begin();
  const_edge_circulator sentinel = e;

  do
  {
    if (f == (*e)->face())
      return *e;
    ++e;
  } while (e != sentinel) ;

  return 0;
}


HalfEdge* Vertex::edgeTo(int v)
{
  if (!_edge)
    return 0;
  edge_circulator e = begin();
  edge_circulator sentinel = e;

  do
  {
    if (v == (*e)->dst()->index())
      return *e;
    ++e;
  } while (e != sentinel) ;

  return 0;
}

const HalfEdge* Vertex::edgeTo(int v) const
{
  if (!_edge)
    return 0;
  const_edge_circulator e = begin();
  const_edge_circulator sentinel = e;

  do
  {
    if (v == (*e)->dst()->index())
      return *e;
    ++e;
  } while (e != sentinel) ;

  return 0;
}


void Vertex::edge_circulator::operator++()
{
  bt_assert(_here->check());
  bt_assert(_here->twin()->dst() == _base);
  _here = _here->twin()->next();
}

void Vertex::const_edge_circulator::operator++()
{
  bt_assert(_here->check());
  bt_assert(_here->twin()->dst() == _base);
  _here = _here->twin()->next();
}

void Vertex::edge_circulator::operator--()
{
  bt_assert(_here->check());
  bt_assert(_here->prev()->dst() == _base);
  _here = _here->prev()->twin();
}

void Vertex::const_edge_circulator::operator--()
{
  bt_assert(_here->check());
  bt_assert(_here->prev()->dst() == _base);
  _here = _here->prev()->twin();
}


bool Vertex::isBoundary() const
{
  if (!_edge)
    return true;
  const_edge_circulator e = begin();
  const_edge_circulator sentinel = e;

  do
  {
    if ((*e)->isBoundary())
      return true;
    ++e;
  } while (e != sentinel) ;

  return false;
}

bool Vertex::adjacent ( const Vertex* v2 ) const
{
  if (!_edge)
    return false;
  Vertex::const_edge_circulator eit = begin();
  Vertex::const_edge_circulator sentinel = eit;
  do
  {
    if ( ( *eit )->dst() == v2 )
      return true;
    ++eit;
  }
  while ( eit != sentinel );
  return false;
}


bool Vertex::check(bool holesFilled) const
{
  bt_assert(_index >= 0);
  if (_edge && holesFilled)
  {
    Vertex::const_edge_circulator eit = begin();
    Vertex::const_edge_circulator sentinel = eit;
    do
    {
      const HalfEdge* const heSentinel = *eit;
      const HalfEdge* he = heSentinel;
      bt_assert(heSentinel->twin()->dst() == this);
      do
      {
        bt_assert(he->dst());
        bt_assert(he->face());
        bt_assert(he->next());
        bt_assert(he->twin());
        bt_assert(he->twin()->twin());
        bt_assert(he == he->twin()->twin());
        he = he->next();
      } while (he != heSentinel) ;

      ++eit;
    }
    while ( eit != sentinel );
  }

  return true;
}

