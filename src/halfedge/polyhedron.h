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

/** @file
 * Declaration of classes Polyhedron and Polyhedron_GL, enumeration filetype_e and other functions.
 */

#ifndef __POLYHEDRON_H__
#define __POLYHEDRON_H__

#include "halfedge.h"
#include "face.h"
#include "vertex.h"

#include "../lib/bt_assert.h"

#include <vector>
#include <map>
#include <set>

/** Class that holds the representation of a manifold mesh in a half-edge data structure.
 *  It contains a set of Vertex, HalfEdge and Face objects that together represent the mesh.
 * I/O functionality, get/set methods and other simple manipulation routines are directly implemented in the class.
 * More advanced algorithms are implemented through external functions.
 * Note that HE_exception objects may be thrown if something goes wrong inside this class.
 */
class Polyhedron
{
  public:

    /// Default constructor. Creates an empty Polyhedron.
    Polyhedron() {}

    /**
     * Destructor. Deletes all the used memory.
     */
    ~Polyhedron();

    /**
     * Adds a new vertex to the Polyhedron. Its adjacency information is empty.
     * @param v Position of the vertex to be added.
     * @return A pointer to the newly created Vertex.
     */
    Vertex* addVertex ( const Vector3Df& v );

    /**
     * Adds a new face to the Polyhedron. Its adjacency information (esp. some _twin links) may be empty.
     * @param corners
     * @return A pointer to the newly created Face.
     */
    Face* addFace(const std::vector<int>& corners);

    /**
     * Adds a new triangular face to the Polyhedron. Its adjacency information (esp. some _twin links) may be empty.
     * @param a First corner of the triangle.
     * @param b Second corner of the triangle.
     * @param c Third corner of the triangle.
     * @return The newly added Face.
     */
    Face* addFace ( int a, int b, int c );

    void punchHole ( Face* f);
    void erase ( Face* f);
    void erase ( HalfEdge* he );
    void erase ( Vertex* v );

    /**
     *    Tells the number of vertices in the Polyhedron.
     * @return Number of vertices in the Polyhedron.
     */
    int numVertices() const { return ( int ) _vertices.size(); }

    /**
     *    Tells the number of faces in the Polyhedron.
     * @return Number of faces in the Polyhedron.
     */
    int numFaces() const { return ( int ) _faces.size(); }

    /**
     *    Tells the number of half-edges in the Polyhedron.
     * @return Number of half-edges in the Polyhedron.
     */
    int numHalfEdges() const { return ( int ) _halfEdges.size(); }

    /**
     *    Tells the number of holes in the Polyhedron.
     *  Some faces are tagged as being holes, thus not representing any surface, but a gap in it. This method counts the number of such faces.
     * @return Number of holes in the Polyhedron.
     */
    int numHoles() const;

    /**
     * Tells if two vertices are adjacent.
     * @param v1 First Vertex
     * @param v2 Second Vertex
     * @return True if they're adjacent; false otherwise.
     */
    bool adjacent ( const Vertex* v1, const Vertex* v2 ) const { return v1->adjacent ( v2 ); }

    /**
     * Tells if two faces are adjacent.
     * @param f1 First Face
     * @param f2 Second Face
     * @return True if they're adjacent; false otherwise.
     */
    bool adjacent ( const Face* f1, const Face* f2 ) const { return f1->adjacent ( f2 ); }

    /**
     * Finds the HalfEdge that connects two vertices.
     * These vertices are indicated by their indices, and should be within range.
     * @param from Index of the vertex from which the searched HalfEdge departs.
     * @param to Index of the vertex where the searched HalfEdge ends.
     * @return The desired HalfEdge, or 0 if it does not exist.
     */
    HalfEdge* edge ( int from, int to );
    HalfEdge* edge ( const Vertex* vFrom, const Vertex* vTo )
    {
      return edge ( vFrom->index(), vTo->index() );
    }
    const HalfEdge* edge ( int from, int to ) const;
    const HalfEdge* edge ( const Vertex* vFrom, const Vertex* vTo ) const
    {
      return edge ( vFrom->index(), vTo->index() );
    }

    /// Returns the indicated Vertex.
    Vertex* vertex ( int v )
    {
      return ( v>=0&&v< ( int ) _vertices.size() ) ? _vertices[v] : 0;
    }

    /// Returns the indicated Vertex.
    const Vertex* vertex ( int v ) const
    {
      return ( v>=0&&v< ( int ) _vertices.size() ) ? _vertices[v] : 0;
    }

    /// Returns the indicated Face.
    Face* face ( int f )
    {
      return ( f>=0&&f< ( int ) _faces.size() ) ? _faces[f] : 0;
    }

    /// Returns the indicated Face.
    const Face* face ( int f ) const
    {
      return ( f>=0&&f< ( int ) _faces.size() ) ? _faces[f] : 0;
    }

    /// Returns the indicated Face.
    const Face* face (const std::vector<int>& corners) const { return _face(corners); }

    /// Returns the indicated Face.
    Face* face (const std::vector<int>& corners) { return _face(corners); }

    /// Returns the indicated HalfEdge.
    HalfEdge* halfedge ( int h )
    {
      return ( h>=0&&h< ( int ) _halfEdges.size() ) ? _halfEdges[h] : 0;
    }

    /// Returns the indicated HalfEdge.
    const HalfEdge* halfedge ( int h ) const { return ( h>=0&&h< ( int ) _halfEdges.size() ) ? _halfEdges[h] : 0; }

    const std::vector<Vertex*>& vertices() const { return _vertices; } ///<Gets the array of vertices being used.

    /**
     * Makes the _twin connections for all halfedges, and closes the holes.
     */
    void finalize(void);

    /// Connects vertices to edges, as a fix when things have goon bananas.
    void linkVerticesToEdges();

    // Iterators/circulators
#include "polyhedron_iterators.h"

    /**
     * Verifies that the data structure is internally consistent.
     * @param holesFilled Have the holes been filled yet?
     * @return True if everything was fine; False otherwise.
     */
    bool check ( bool holesFilled=true ) const;

    /**
     * A degree 2 Vertex is bypassed by its two adjacent edges, which merge into a single one.
     * @pre Vertex V MUST have degree 2. This is assert()-ed in the debugging version.
     * @param V Vertex to be bypassed.
     * @param eraseVertex If true, Vertex V will be deleted afterwards.
     * @return One of the HalfEdges that bypass Vertex V.
     */
    const HalfEdge* bypass(Vertex* V, bool eraseVertex=true);

    bool contains(const Vertex* V) const;
    bool contains(const Face* F) const;
    bool contains(const HalfEdge* he) const;

    /// Bulk-adding vertices
    void loadVertices ( const std::vector<float>& verts );

  protected:
    void closeSurface ();
    Face* _face(const std::vector<int>& corners) const;

    std::vector<Face*> _faces;
    std::set<Face*> _holes;
    std::vector<HalfEdge*> _halfEdges;
    std::vector<Vertex*> _vertices;
};

void eraseFaces ( Polyhedron* poly, const std::set<Face*>& faces );
/**
 * Writes out a complete description of the Polyhedron
 * @param  out Stream where the description is to be written.
 * @return Reference to the written stream.
 */
//  std::ostream& operator<< ( std::ostream& out, const Polyhedron& p );

/**
 * A small helper class to quickly find existing vertices in a polyhedron
 */
class vertexList_c
{
  public:
    std::map<Vector3Df, int> vertMap;
    std::vector<float> verts;
    int cnt;
    Polyhedron * poly;

    vertexList_c(Polyhedron * p) : cnt(0), poly(p) {
      // TODO, we should check the polyhedron for existing vertices, right now we assume the polyhedron is
      // empty at the start
      bt_assert(p->numVertices() == 0);
    }

    /**
     * Returns the index to use for the given coordinates.
     * This function either adds a new vertex to the polygedron and returns
     * that index, or it returns the index of a vertex with the same coordinates
     */
    int get(float x, float y, float z)
    {
      Vector3Df v(x, y, z);
      std::map<Vector3Df, int>::iterator i = vertMap.find(v);
      int idx;
      if (i != vertMap.end())
      {
        idx = i->second;
      }
      else
      {
        verts.push_back(v.x());
        verts.push_back(v.y());
        verts.push_back(v.z());
        vertMap.insert(std::pair<Vector3Df, int>(v, cnt));
        poly->addVertex(v);
        idx = cnt;
        cnt++;
      }

      return idx;
    }

    Face * addFace(const std::vector<int>& corners) { return poly->addFace(corners); }
};

#endif
