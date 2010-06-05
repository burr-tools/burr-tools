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

#ifndef __VERTEX_H__
#define __VERTEX_H__

#include "vector3.h"


class HalfEdge;
class Face;
class Polyhedron;

/// Class that represents the vertices of the mesh. It basic topologic and geometric information.
class Vertex
{
  public:
    /// Circulator for the edges departing from this Vertex.
    class edge_circulator;
    class const_edge_circulator;
    class edge_circulator
    {
      public:
        edge_circulator(const Vertex* v, HalfEdge* h) : _base(v), _here(h) {}

        const Vertex* base() const { return _base; };
        HalfEdge* operator*() { return _here; }
        void operator++();
        void operator++(int) { ++(*this); }
        void operator--();
        void operator--(int) { --(*this); }
        bool operator==(const edge_circulator& e) const { return _base==e._base && _here==e._here; }
        bool operator!=(const edge_circulator& e) const { return !(e == *this); }
      protected:
        const Vertex* _base;
        HalfEdge* _here;
        friend class const_edge_circulator;
    };

    /// Const circulator for the edges departing from this Vertex.
    class const_edge_circulator
    {
      public:
        const_edge_circulator(const Vertex* v, const HalfEdge* h) : _base(v), _here(h) {}
        const_edge_circulator(const edge_circulator& ec) : _base(ec.base()), _here(ec._here) {}

        const Vertex* base() const { return _base; };
        const HalfEdge* operator*() const { return _here; }
        void operator++();
        void operator++(int) { ++(*this); }
        void operator--();
        void operator--(int) { --(*this); }
        bool operator==(const const_edge_circulator& e) const { return _base==e._base && _here==e._here; }
        bool operator!=(const const_edge_circulator& e) const { return !(e == *this); }
      protected:
        const Vertex* _base;
        const HalfEdge* _here;
        friend class edge_circulator;
    };

    /// Circulator referring to an arbitrary departing HalfEdge.
    edge_circulator begin() { return edge_circulator(this, _edge); }
    /// Const circulator referring to an arbitrary departing HalfEdge.
    const_edge_circulator begin() const { return const_edge_circulator(this, _edge); }

    explicit Vertex(int i=-1) : _edge(0), _index(i) {}    ///< Constructor
    explicit Vertex(const Vector3Df& pos, int i=-1) : _pos(pos), _edge(0), _index(i) {}    ///< Constructor
    explicit Vertex(const Vector3Df& pos, HalfEdge* e, int i=-1) : _pos(pos), _edge(e), _index(i) {}  ///< Constructor

    // Set methods
    void index(int i) { _index = i; } ///< Sets the index of the Vertex in the mesh.
    void position(const Vector3Df& pos) { _pos = pos; } ///< Sets the position of the Vertex.
    void edge(HalfEdge* e) { _edge = e; }  ///< Associates an arbitrary HalfEdge to the Vertex.

    // Get methods
    int index() const { return _index; }  ///< Returns the index of the Vertex object.
    const Vector3Df& position() const { return _pos; } ///< Returns the position of the Vertex.
    HalfEdge* edge() { return _edge; }              ///< Returns an arbitrary HalfEdge associated to the Vertex.
    const HalfEdge* edge() const { return _edge; }  ///< Returns an arbitrary const HalfEdge associated to the Vertex.
    bool adjacent(const Vertex* V2) const;

    // Simple computations
    Vector3Df normal() const;    ///< Computes the normal of the Vertex.
    int valence() const;    ///< Counts the number of HalfEdge objects departing from the Vertex.
    int degree() const { return valence(); }    ///< Counts the number of HalfEdge objects departing from the Vertex.
    bool isBoundary() const;

    bool operator==(const Vertex& v) const { return _index == v._index; } /// Are these vertices the same one?
    /// Returns the HalfEdge that departs from this Vertex and ends in Vertex 'v'.
    HalfEdge* edgeTo(const Vertex* v) { return edgeTo(v->index()); }
    /// Returns the HalfEdge that departs from this Vertex and ends in Vertex 'v'.
    const HalfEdge* edgeTo(const Vertex* v) const { return edgeTo(v->index()); }

    /// Returns the HalfEdge that departs from this Vertex and ends in Vertex with index 'v'.
    HalfEdge* edgeTo(int v);
    /// Returns the HalfEdge that departs from this Vertex and ends in Vertex with index 'v'.
    const HalfEdge* edgeTo(int v) const;

    /// Returns the HalfEdge that departs from this Vertex and is adjacent to Face 'f'.
    const HalfEdge* edgeAdjacentTo(const Face* f) const;

    bool check(bool holesFilled=true) const; ///< Sanity check.

    /// Returns the Vector3Df between the positions of two vertices.
    Vector3Df operator-(const Vertex& V) const { return V._pos-_pos; }

  protected:
    Vector3Df _pos;
    HalfEdge* _edge;
    int _index;
};

#endif
