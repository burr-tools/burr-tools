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

#ifndef __HALFEDGE_H__
#define __HALFEDGE_H__

#include "vector3.h"

class Vertex;
class Face;
class Polyhedron;

/**
 * Class that represents the half-edges of the mesh.
 * Each edge [a,b] of the mesh is represented by two HalfEdge objects, one from 'a' to 'b' and one the other way around.
 * Each HalfEdge is also associated to the polygon (or hole) that is adjacent to that edge on a given side. By convention,
 * the associated face for a HalfEdge is that which the HalfEdge traverses in countercloskwise order. For example, face [a,b,c]
 * is associated to HalfEdge objects [a,b], [b,c] and [c,a]. The symmetric HalfEdge objects are associated to other faces.
 *
 * The four differentiating pieces of data a HalfEdge contains are its twin() (or symmetric) HalfEdge, the next() HalfEdge in
 * the associated Face, the face() itself to which the HalfEdge is associated, and the Vertex dst() where the HalfEdge ends.
 */
class HalfEdge
{
  public:
    /// Constructor.
    explicit HalfEdge ( Vertex* v, Face* f, HalfEdge* t, HalfEdge* n, int idx=-1 )
      : _dst ( v ), _face ( f ), _twin ( t ), _next ( n ), _index ( idx ) {}
    /// Constructor.
    explicit HalfEdge ( Vertex* v, Face* f, int idx=-1 )
      : _dst ( v ), _face ( f ), _twin ( 0 ), _next ( 0 ), _index ( idx ) {}

    // Set methods
    void next ( HalfEdge* n ) { _next = n; } ///< Sets the next HalfEdge in the Face
    void twin ( HalfEdge* p ) { _twin = p; } ///< Sets the symmetric HalfEdge to this one.
    void face ( Face* f ) { _face = f; }  ///< Sets the associated Face object.
    void dst ( Vertex* v ) { _dst = v; }  ///< Sets the destination Vertex object.

    // Get methods
    HalfEdge* next() { return _next; }             ///< Returns the next HalfEdge in the Face.
    const HalfEdge* next() const { return _next; } ///< Returns the next HalfEdge in the Face. (Const method)
    HalfEdge* prev();                              ///< Returns the previous HalfEdge in the Face.
    const HalfEdge* prev() const;                  ///< Returns the previous HalfEdge in the Face. (Const method)
    HalfEdge* twin() { return _twin; }             ///< Returns the twin (symmetric) HalfEdge.
    const HalfEdge* twin() const { return _twin; } ///< Returns the twin (symmetric) HalfEdge. (Const method)
    Face* face() { return _face; }                 ///< Returns the associated Face object.
    const Face* face() const { return _face; }     ///< Returns the associated Face object. (Const method)
    Vertex* dst() { return _dst; }                 ///< Returns the destination Vertex of the HalfEdge.
    const Vertex* dst() const { return _dst; }     ///< Returns the destination Vertex of the HalfEdge. (Const method)
    int index() const { return _index; }           ///< Returns the index of the HalfEdge in the Polyhedron.
    void index(int i) { _index=i; }                ///< Modifies the index of the HalfEdge (dangerous!).

    /// Returns the source Vertex of the HalfEdge. (DANGEROUS if _twin is not set!)
    Vertex* src() { return _twin->dst(); }
    /// Returns the source Vertex of the HalfEdge. (Const method) (DANGEROUS if _twin is not set!)
    const Vertex* src() const { return _twin->dst(); }

    // Simple computations
    Vector3Df normal() const;   ///< Computes the normal of the HalfEdge (average of the two adjacent faces).
    Vector3Df tangent() const;  ///< Returns the tangent direction of the HalfEdge.
    Vector3Df midpoint() const;  ///< Returns the middle point of the HalfEdge.
    float squaredLength() const { return tangent().squaredModule(); } ///< Squared length of the HalfEdge.
    float length() const { return tangent().module(); }  ///< Length of the segment.
    bool isBoundary() const;  ///< Tells if the half-edge is in a boundary of the mesh.

    bool degenerate() const { return src() == _dst; } ///< Is this a degenerate edge?
    bool check ( bool holesFilled=true ) const;   ///< Checks the local connectivity of the HalfEdge

  protected:

    Vertex* _dst;
    Face* _face;
    HalfEdge* _twin;
    HalfEdge* _next;
    int _index;
};


#endif
