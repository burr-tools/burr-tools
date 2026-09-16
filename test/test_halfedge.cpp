#include <catch2/catch_test_macros.hpp>

#include "mesh_helpers.h"

#include "halfedge/face.h"
#include "halfedge/halfedge.h"
#include "halfedge/polyhedron.h"
#include "halfedge/vertex.h"
#include "halfedge/volume.h"

#include <memory>
#include <set>

using namespace bttest;

/* The half-edge data structure: polyhedron.cpp, vertex.cpp, face.cpp and
   halfedge.cpp, 668 lines between them and barely touched.

   Step three of the mesh/STL design doc. Its central point is that this
   structure's invariants are checkable without golden files -- every
   half-edge has a twin, twins are mutual, face loops close, the Euler
   characteristic holds -- so nothing here records a mesh and compares. */

/* ------------------------------------------------------------------ */
/* construction                                                        */
/* ------------------------------------------------------------------ */

TEST_CASE("polyhedron: a tetrahedron has the counts its topology requires", "[halfedge]") {
  std::unique_ptr<Polyhedron> p = tetrahedron();

  REQUIRE(p->numVertices() == 4);
  REQUIRE(p->numFaces() == 4);

  /* six edges, each stored as two opposed half-edges */
  REQUIRE(p->numHalfEdges() == 12);

  /* a closed surface of genus 0 -- and the check that ties the three counts
     together rather than letting each be wrong independently */
  REQUIRE(eulerCharacteristic(*p) == 2);

  REQUIRE(p->numHoles() == 0);
}

TEST_CASE("polyhedron: a cube built from twelve triangles has the counts its topology requires",
          "[halfedge]") {
  std::unique_ptr<Polyhedron> p = cube();

  REQUIRE(p->numVertices() == 8);
  REQUIRE(p->numFaces() == 12);

  /* 12 triangles x 3 = 36 half-edges, so 18 edges */
  REQUIRE(p->numHalfEdges() == 36);
  REQUIRE(eulerCharacteristic(*p) == 2);
  REQUIRE(p->numHoles() == 0);
}

TEST_CASE("polyhedron: a single triangle is an open surface with a boundary", "[halfedge]") {
  std::unique_ptr<Polyhedron> p = openTriangle();

  REQUIRE(p->numVertices() == 3);

  /* The face the caller asked for, plus the hole face finalize() creates to
     cap the boundary -- which is what makes the structure closed enough for
     every half-edge to have a twin. Stated explicitly because "numFaces()
     == 1" is the natural expectation and is wrong. */
  REQUIRE(p->numFaces() == 2);
  REQUIRE(p->numHoles() == 1);

  REQUIRE(p->numHalfEdges() == 6);

  /* every vertex is on the boundary of an open surface */
  for (int i = 0; i < p->numVertices(); i++) {
    INFO("vertex " << i);
    REQUIRE(p->vertex(i)->isBoundary());
  }
}

/* ------------------------------------------------------------------ */
/* the structural invariants                                           */
/* ------------------------------------------------------------------ */

TEST_CASE("halfedge: every half-edge has a mutual twin whose endpoints agree", "[halfedge]") {
  /* The defining property of the representation. Checked on all three
     fixtures, including the open one, because finalize() reaches the twin
     links by a different route there -- through the hole face it
     synthesises rather than through a neighbouring real face. */
  std::unique_ptr<Polyhedron> shapes[] = { tetrahedron(), cube(), openTriangle() };
  const char * names[] = { "tetrahedron", "cube", "open triangle" };

  for (int i = 0; i < 3; i++) {
    INFO(names[i]);
    REQUIRE(twinsAreMutual(*shapes[i]));
    REQUIRE(endpointsAgree(*shapes[i]));
  }
}

TEST_CASE("halfedge: a half-edge and its twin run in opposite directions", "[halfedge]") {
  std::unique_ptr<Polyhedron> p = cube();

  /* twin()->dst() == src() is asserted above; this is the other half of the
     statement, and the one that would catch a twin link joining two edges
     that merely share a vertex rather than the whole edge */
  for (Polyhedron::const_edge_iterator it = p->eBegin(); it != p->eEnd(); ++it) {
    const HalfEdge * he = *it;

    INFO("half-edge " << he->index());
    REQUIRE(he->dst() == he->twin()->src());
    REQUIRE(he->src() == he->twin()->dst());

    /* and no edge collapses to a point */
    REQUIRE_FALSE(he->degenerate());
  }
}

TEST_CASE("halfedge: next and prev are inverse around a face", "[halfedge]") {
  std::unique_ptr<Polyhedron> p = cube();

  for (Polyhedron::const_edge_iterator it = p->eBegin(); it != p->eEnd(); ++it) {
    const HalfEdge * he = *it;

    INFO("half-edge " << he->index());
    REQUIRE(he->next()->prev() == he);
    REQUIRE(he->prev()->next() == he);
  }
}

TEST_CASE("polyhedron: face loops close and every edge in a loop names its own face",
          "[halfedge]") {
  std::unique_ptr<Polyhedron> shapes[] = { tetrahedron(), cube() };
  const char * names[] = { "tetrahedron", "cube" };

  for (int i = 0; i < 2; i++) {
    INFO(names[i]);
    REQUIRE(faceLoopsClose(*shapes[i]));

    /* all faces are triangles in both fixtures, so size() is pinned too --
       faceLoopsClose would be satisfied by a loop of any consistent length */
    for (Polyhedron::const_face_iterator it = shapes[i]->fBegin();
         it != shapes[i]->fEnd(); ++it)
      REQUIRE((*it)->size() == 3);
  }
}

TEST_CASE("polyhedron: indices agree with the positions the elements are stored at", "[halfedge]") {
  std::unique_ptr<Polyhedron> p = cube();

  for (int i = 0; i < p->numVertices(); i++) {
    INFO("vertex " << i);
    REQUIRE(p->vertex(i)->index() == i);
  }
  for (int i = 0; i < p->numFaces(); i++) {
    INFO("face " << i);
    REQUIRE(p->face(i)->index() == i);
  }
  for (int i = 0; i < p->numHalfEdges(); i++) {
    INFO("half-edge " << i);
    REQUIRE(p->halfedge(i)->index() == i);
  }
}

TEST_CASE("polyhedron: out-of-range accessors return null rather than reading past the end",
          "[halfedge]") {
  std::unique_ptr<Polyhedron> p = tetrahedron();

  REQUIRE(p->vertex(-1) == nullptr);
  REQUIRE(p->vertex(p->numVertices()) == nullptr);
  REQUIRE(p->face(-1) == nullptr);
  REQUIRE(p->face(p->numFaces()) == nullptr);
  REQUIRE(p->halfedge(-1) == nullptr);
  REQUIRE(p->halfedge(p->numHalfEdges()) == nullptr);

  /* the const overloads are separate function bodies */
  const Polyhedron & cp = *p;
  REQUIRE(cp.vertex(-1) == nullptr);
  REQUIRE(cp.face(-1) == nullptr);
  REQUIRE(cp.halfedge(-1) == nullptr);
}

#ifndef NDEBUG
TEST_CASE("polyhedron: the library's own check passes on every fixture", "[halfedge]") {
  /* Polyhedron::check() walks every vertex, half-edge and face and asserts
     the structure's invariants. It is built from bt_assert, so it throws on
     failure and compiles to nothing under NDEBUG -- hence the guard, and
     hence the separate cases above that hold in a release build too. A case
     whose only assertion was check() would silently test nothing there.

     Kept anyway because check() covers more than the invariants spelled out
     above, including the "no two adjacent faces are both holes" rule that
     nothing else here reaches. */
  REQUIRE_NOTHROW(tetrahedron()->check(true));
  REQUIRE_NOTHROW(cube()->check(true));

  /* the open triangle has a hole, so it is checked with holes not required
     to be filled */
  REQUIRE_NOTHROW(openTriangle()->check(false));
}
#endif

/* ------------------------------------------------------------------ */
/* navigation                                                          */
/* ------------------------------------------------------------------ */

TEST_CASE("polyhedron: edge finds the half-edge between two vertices and its twin the other way",
          "[halfedge]") {
  std::unique_ptr<Polyhedron> p = tetrahedron();

  HalfEdge * forward = p->edge(0, 1);
  REQUIRE(forward != nullptr);
  REQUIRE(forward->src()->index() == 0);
  REQUIRE(forward->dst()->index() == 1);

  HalfEdge * backward = p->edge(1, 0);
  REQUIRE(backward != nullptr);

  /* the two directions of one edge are twins, which is what ties the
     lookup to the structure rather than to a search that happens to
     succeed both ways */
  REQUIRE(backward == forward->twin());

  /* a pair with no edge between them reports so. Every vertex pair in a
     tetrahedron IS joined, so this uses an index that does not exist. */
  REQUIRE(p->edge(0, 99) == nullptr);
}

TEST_CASE("vertex: the circulator visits each departing edge once and comes back", "[halfedge]") {
  std::unique_ptr<Polyhedron> p = cube();

  for (int i = 0; i < p->numVertices(); i++) {
    const Vertex * v = p->vertex(i);

    std::set<const HalfEdge *> seen;
    Vertex::const_edge_circulator it = v->begin();
    Vertex::const_edge_circulator sentinel = it;

    int guard = 0;
    do {
      INFO("vertex " << i);

      /* every departing edge starts where it should */
      REQUIRE((*it)->src() == v);

      /* and is visited exactly once */
      REQUIRE(seen.insert(*it).second);

      ++it;
      REQUIRE(++guard < 100);   // the circulator must terminate
    } while (it != sentinel);

    /* valence() must agree with what the circulator actually walked --
       two separate routes to the same number */
    INFO("vertex " << i);
    REQUIRE((int)seen.size() == v->valence());
    REQUIRE(v->degree() == v->valence());
  }
}

TEST_CASE("vertex: adjacency agrees with there being an edge between two vertices", "[halfedge]") {
  std::unique_ptr<Polyhedron> p = tetrahedron();

  /* every pair in a tetrahedron is joined */
  for (int a = 0; a < 4; a++)
    for (int b = 0; b < 4; b++) {
      if (a == b) continue;

      INFO("vertices " << a << " and " << b);
      REQUIRE(p->adjacent(p->vertex(a), p->vertex(b)));
      REQUIRE(p->vertex(a)->edgeTo(b) != nullptr);
    }

  /* a cube has non-adjacent pairs -- the diagonal -- so the predicate is
     shown to be capable of saying no */
  std::unique_ptr<Polyhedron> c = cube();
  REQUIRE_FALSE(c->adjacent(c->vertex(0), c->vertex(6)));
  REQUIRE(c->vertex(0)->edgeTo(6) == nullptr);
}

TEST_CASE("face: contains and adjacent agree with the face's own edge loop", "[halfedge]") {
  std::unique_ptr<Polyhedron> p = tetrahedron();

  const Face * f = p->face(0);

  /* every edge in the loop is contained, and so is each of its endpoints */
  const HalfEdge * start = f->edge();
  const HalfEdge * he = start;
  do {
    REQUIRE(f->contains(he));
    REQUIRE(f->contains(he->dst()));
    he = he->next();
  } while (he != start);

  /* in a tetrahedron every face shares an edge with every other */
  for (int i = 1; i < 4; i++) {
    INFO("face " << i);
    REQUIRE(p->adjacent(f, p->face(i)));
  }

  /* a face is not degenerate -- no repeated vertices */
  REQUIRE_FALSE(f->degenerate());
}

TEST_CASE("face: fromVertex and toVertex find the edge at the named end", "[halfedge]") {
  std::unique_ptr<Polyhedron> p = tetrahedron();

  const Face * f = p->face(0);
  const HalfEdge * some = f->edge();

  const int from = some->src()->index();
  const int to = some->dst()->index();

  REQUIRE(f->fromVertex(from) == some);
  REQUIRE(f->toVertex(to) == some);

  /* and they disagree about which edge, since the two ends name different
     edges of the loop -- without this a single-edge face would satisfy both
     trivially */
  REQUIRE(f->fromVertex(to) != f->toVertex(to));
}

/* ------------------------------------------------------------------ */
/* geometry                                                            */
/* ------------------------------------------------------------------ */

TEST_CASE("face: the centroid is the average of the face's corners", "[halfedge]") {
  std::unique_ptr<Polyhedron> p = cube();

  for (Polyhedron::const_face_iterator it = p->fBegin(); it != p->fEnd(); ++it) {
    const Face * f = *it;

    Vector3Df sum;
    int n = 0;
    const HalfEdge * start = f->edge();
    const HalfEdge * he = start;
    do {
      sum += he->dst()->position();
      n++;
      he = he->next();
    } while (he != start);

    Vector3Df expected = sum / (float)n;
    Vector3Df got = f->centroid();

    INFO("face " << f->index());
    REQUIRE(got.x() == meshApprox(expected.x()));
    REQUIRE(got.y() == meshApprox(expected.y()));
    REQUIRE(got.z() == meshApprox(expected.z()));
  }
}

TEST_CASE("face: the normal is a unit vector perpendicular to the face's own edges", "[halfedge]") {
  std::unique_ptr<Polyhedron> p = cube();

  for (Polyhedron::const_face_iterator it = p->fBegin(); it != p->fEnd(); ++it) {
    const Face * f = *it;
    Vector3Df n = f->normal();

    INFO("face " << f->index());
    REQUIRE(n.module() == meshApprox(1.0));

    /* perpendicular to every edge vector in the loop -- the property that
       makes it a normal, and one a normalised but arbitrary vector would
       fail */
    const HalfEdge * start = f->edge();
    const HalfEdge * he = start;
    do {
      Vector3Df along = he->dst()->position() - he->src()->position();
      REQUIRE(n * along == meshApprox(0.0));
      he = he->next();
    } while (he != start);
  }
}

TEST_CASE("face: the cube's face normals point outwards", "[halfedge]") {
  std::unique_ptr<Polyhedron> p = cube(2.0f);

  /* the cube runs from (0,0,0) to (2,2,2), so its centre is (1,1,1). An
     outward normal points away from the centre, which fixes the winding --
     and the winding is what the volume sign depends on. */
  Vector3Df centre(1, 1, 1);

  for (Polyhedron::const_face_iterator it = p->fBegin(); it != p->fEnd(); ++it) {
    const Face * f = *it;

    Vector3Df outward = f->centroid() - centre;

    INFO("face " << f->index());
    REQUIRE(f->normal() * outward > 0.0f);
  }
}

TEST_CASE("halfedge: the midpoint is halfway between the edge's endpoints", "[halfedge]") {
  std::unique_ptr<Polyhedron> p = cube();

  for (Polyhedron::const_edge_iterator it = p->eBegin(); it != p->eEnd(); ++it) {
    const HalfEdge * he = *it;

    Vector3Df expected = (he->src()->position() + he->dst()->position()) / 2.0f;
    Vector3Df got = he->midpoint();

    INFO("half-edge " << he->index());
    REQUIRE(got.x() == meshApprox(expected.x()));
    REQUIRE(got.y() == meshApprox(expected.y()));
    REQUIRE(got.z() == meshApprox(expected.z()));

    /* and a half-edge and its twin share a midpoint, since they span the
       same edge */
    Vector3Df twinMid = he->twin()->midpoint();
    REQUIRE(twinMid.x() == meshApprox(got.x()));
  }
}

/* ------------------------------------------------------------------ */
/* volume                                                              */
/* ------------------------------------------------------------------ */

TEST_CASE("volume: a cube's volume is the cube of its side", "[halfedge][volume]") {
  /* volume.cpp is 28 lines at 0%, and it is the measuring instrument
     several later cases rely on, so it is checked first and against values
     that are genuinely known rather than against another implementation. */
  REQUIRE(volume(*cube(1.0f)) == meshApprox(1.0));
  REQUIRE(volume(*cube(2.0f)) == meshApprox(8.0));
  REQUIRE(volume(*cube(0.5f)) == meshApprox(0.125));
}

TEST_CASE("volume: scaling a mesh by a factor multiplies its volume by the cube of it",
          "[halfedge][volume]") {
  const double one = volume(*cube(1.0f));
  const double three = volume(*cube(3.0f));

  /* asserted as the ratio rather than as two constants, so it holds
     whatever the absolute figures are */
  REQUIRE(three == meshApprox(one * 27.0));
}

TEST_CASE("volume: a tetrahedron's volume matches the determinant formula", "[halfedge][volume]") {
  /* The fixture's vertices are (1,1,1), (1,-1,-1), (-1,1,-1), (-1,-1,1) --
     four alternating corners of the cube spanning [-1,1]^3. That
     tetrahedron has volume 8/3: the cube's volume is 8, and cutting the
     four corner tetrahedra off leaves this one, each corner piece being
     1/6 of the cube. 8 - 4*(8/6) = 8/3. */
  REQUIRE(volume(*tetrahedron()) == meshApprox(8.0 / 3.0));
}

TEST_CASE("volume: the library's volume agrees with an independent calculation",
          "[halfedge][volume]") {
  /* independentVolume() in mesh_helpers.h is a deliberately separate and
     much simpler implementation -- a triangle fan from the origin -- so
     this comparison is not circular. Two different routes to the same
     number is what makes the hand-computed constants above more than a
     coincidence. */
  std::unique_ptr<Polyhedron> shapes[] = { tetrahedron(), cube(1.0f), cube(3.0f) };
  const char * names[] = { "tetrahedron", "unit cube", "cube of side 3" };

  for (int i = 0; i < 3; i++) {
    INFO(names[i]);
    REQUIRE(volume(*shapes[i]) == meshApprox(independentVolume(*shapes[i])));
  }
}
