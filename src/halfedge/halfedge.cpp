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

#include "halfedge.h"

#include "face.h"
#include "vertex.h"

#include "../lib/bt_assert.h"

#include <set>

HalfEdge* HalfEdge::prev()
{
  HalfEdge* he = _next;
  bt_assert(he);
  while (he && he->next() != this)
  {
    bt_assert(he->next());
    he = he->next();
    bt_assert(he);
  }

  return he;
}

const HalfEdge* HalfEdge::prev() const
{
  HalfEdge* he = _next;
  bt_assert(he);
  while (he && he->next() != this)
  {
    bt_assert(he->next());
    he = he->next();
    bt_assert(he);
  }

  return he;
}

Vector3Df HalfEdge::normal() const
{
  return (src()->normal()+dst()->normal())/2;
}


Vector3Df HalfEdge::tangent() const
{
  return dst()->position() - src()->position();
}

Vector3Df HalfEdge::midpoint() const
{
  return (src()->position() + dst()->position()) / 2;
}

bool HalfEdge::isBoundary() const
{
  return face()->hole() || twin()->face()->hole();
}

bool HalfEdge::check(bool holesFilled) const
{
  bt_assert(prev()->next() == this);
  if (holesFilled)
  {
    bt_assert ( twin() );
    bt_assert ( twin() != this );
    bt_assert ( twin()->twin() == this );
    bt_assert(prev()->dst() == twin()->dst());
    bt_assert(prev()->dst() == src());
    bt_assert(prev()->dst() == twin()->next()->src());
    bt_assert(src() == twin()->dst());
    bt_assert(src() == twin()->next()->src());
    bt_assert(src() == twin()->next()->src());
    bt_assert(src() != dst());
    //	 bt_assert(next()->dst() != twin()->next()->dst());
  }
  bt_assert ( next() != 0 );
  bt_assert ( face() != 0 );
  bt_assert ( dst() != 0 );
  bt_assert ( ! ( * dst() == * prev()->dst() ) );

  // Make sure the half edge is correctly linked in a face, and that the vertices are unique
  {
    std::set<const HalfEdge*> visited;
    std::set<int> iVisited;
    const HalfEdge* e = this;
    const HalfEdge* sentinel = e;
    do
    {
      bt_assert ( visited.end() == visited.find ( e ) );
      visited.insert ( e );
      bt_assert ( iVisited.end() == iVisited.find ( e->dst()->index() ) );
      iVisited.insert(e->dst()->index());
      e = e->next();
    }
    while ( e != sentinel );
  }

  bt_assert ( prev()->next() == this );

  return true;
}
