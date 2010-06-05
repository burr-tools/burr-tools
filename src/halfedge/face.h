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

#ifndef __FACE_H__
#define __FACE_H__

#include "halfedge.h"
#include <stdint.h>

class Vertex;
class Polyhedron;

/**
 * Class that represents the faces of the mesh. It contains a point to an arbitrary HalfEdge in its boundary,
 * which can be used to traverse the whole face.
 */
class Face
{

  public:
    class const_edge_circulator;
    class edge_circulator;

    /// Circulator that traverses the HalfEdge(s) that compound the boundary of the Race.
    class edge_circulator
    {
      public:

        edge_circulator(const Face* f, HalfEdge* h) : _base(f), _here(h) {}

        HalfEdge* operator*() { return _here; }
        void operator++() { _here = _here->next(); }
        void operator++(int) { ++(*this); }
        void operator--() { _here = _here->prev(); }
        void operator--(int) { --(*this); }
        bool operator==(const edge_circulator& e) const { return _base==e._base && _here==e._here; }
        bool operator!=(const edge_circulator& e) const { return !(e == *this); }

      protected:

        const Face* _base;
        HalfEdge* _here;

        friend class const_edge_circulator;
    };

    /**
     *    Reference to an arbitrary HalfEdge of the Race, used for traversal.
     * @return An edge_circulator that refers to an arbitrary HalfEdge of the Race.
     */
    edge_circulator begin() { return edge_circulator(this, _edge); }

    /// Constant circulator that traverses the HalfEdge(s) that compound the boundary of the Race.
    class const_edge_circulator
    {
      public:

        const_edge_circulator(const Face* f, const HalfEdge* h) : _base(f), _here(h) {}
        const_edge_circulator(const edge_circulator& ec) : _base(ec._base), _here(ec._here) {}

        const HalfEdge* operator*() { return _here; }
        void operator++() { _here = _here->next(); }
        void operator++(int) { ++(*this); }
        void operator--() { _here = _here->prev(); }
        void operator--(int) { --(*this); }
        bool operator==(const const_edge_circulator& e) const { return _base==e._base && _here==e._here; }
        bool operator!=(const const_edge_circulator& e) const { return !(e == *this); }

      protected:

        const Face* _base;
        const HalfEdge* _here;
    };

    /**
     *    Reference to an arbitrary const HalfEdge of the Race, used for traversal.
     * @return A const_edge_circulator that refers to an arbitrary const HalfEdge of the Race.
     */
    const_edge_circulator begin() const { return const_edge_circulator(this, _edge); }

    /**
     *    Constructor.
     * @param e A HalfEdge that is to be associated to the Race.
     * @param hole Indicates whether the Race is actually a hole. By default, false.
     */
    explicit Face(HalfEdge* e, bool hole=false, int idx=-1) : _edge(e), _hole(hole), _index(idx), _flags(0), _fb_index(0), _fb_face(0) {}

    // Set methods
    void hole(bool h) { _hole = h; }       ///< Sets the 'hole' flag of the Race to true of false.
    void edge(HalfEdge* e) { _edge = e; }  ///< Sets the referenc HalfEdge of the Race.

    // Get methods
    bool hole() const { return _hole; }            ///< Tells if the Race is actually a hole.
    HalfEdge* edge() { return _edge; }             ///< Returns an arbitrary HalfEdge on the Race's boundary.
    const HalfEdge* edge() const { return _edge; } ///< Returns an arbitrary const HalfEdge on the Race's boundary.
    const HalfEdge* fromVertex(int v) const;       ///< Returns a const HalfEdge on the Race that starts at vertex with index 'v'
    const HalfEdge* fromVertex(Vertex* v) const;   ///< Returns a const HalfEdge on the Race that starts at Vertex 'v'
    const HalfEdge* toVertex(int v) const;         ///< Returns a const HalfEdge on the Race that ends at vertex with index 'v'
    const HalfEdge* toVertex(Vertex* v) const;     ///< Returns a const HalfEdge on the Race that ends at Vertex 'v'

    // Simple computations
    int size() const;                        ///< Number of HalfEdge's in the Race.
    int index() const { return _index; }     ///< Index of the face in the Polyhedron.
    void index(int i) { _index=i; }          ///< Modifies the index of the Face (dangerous!).
    bool contains(const HalfEdge* he) const; ///< Tells if this Face contains HalfEdge he.
    bool contains(const Vertex* v) const;    ///< Tells if this Face contains Vertex v.
    bool adjacent(const Face* f2) const;     ///< Tells if this Face is adjacent to f2.
    bool degenerate() const;                 ///< Tells is the Face contains degenerate features like repeated vertices.
    Vector3Df normal() const;      ///< Computes the normal of the Face.
    Vector3Df centroid() const;    ///< Computes the centroid of the Face.


  protected:
    HalfEdge* _edge;
    bool _hole;
    int _index;

    // this part contains some additional data that I attatch to each face for BurrTools
    // is is not needed for a face and only useful for out purposes
  public:
    uint32_t _flags;  /// some additional flags used to draw faces differently
    // feedback information on pieces, which voxel and which face of the voxel
    uint32_t _fb_index;
    uint32_t _fb_face;
    uint8_t  _color;  /// this is the color constraint index
};

#endif
