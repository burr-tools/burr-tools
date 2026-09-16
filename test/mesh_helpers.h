#ifndef BTTEST_MESH_HELPERS_H
#define BTTEST_MESH_HELPERS_H

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "halfedge/face.h"
#include "halfedge/halfedge.h"
#include "halfedge/polyhedron.h"
#include "halfedge/vector3.h"
#include "halfedge/vertex.h"

#include <cmath>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

/*
 * Shared fixtures and invariant checks for the half-edge mesh cases.
 *
 * The mesh/STL design doc makes these the package's most reusable asset:
 * every file after the data-structure cases depends on having a small,
 * hand-checkable Polyhedron to work with, and on a vocabulary for saying
 * what is true of one without recording its contents.
 *
 * The fixtures are deliberately tiny -- a tetrahedron is four vertices, four
 * faces and twelve half-edges, all of which can be worked out on paper.
 */

namespace bttest {

/* One tolerance for the mesh cases, matching the one test_vector3.cpp
   settles on and for the same reason: these are floats routed through
   double-precision arithmetic, and the coordinates range over a couple of
   orders of magnitude. Relative, with a small absolute margin so that
   comparisons against zero do not need a special case. */
inline Catch::Approx meshApprox(double v) {
  return Catch::Approx(v).epsilon(1e-5).margin(1e-6);
}

/**
 * A regular tetrahedron with unit-ish coordinates, wound so every face's
 * normal points outwards.
 *
 * V=4, E=6 (12 half-edges), F=4. Euler characteristic 4 - 6 + 4 = 2, as a
 * closed surface of genus 0 must have.
 */
inline std::unique_ptr<Polyhedron> tetrahedron() {
  auto p = std::make_unique<Polyhedron>();

  p->addVertex(Vector3Df( 1,  1,  1));   // 0
  p->addVertex(Vector3Df( 1, -1, -1));   // 1
  p->addVertex(Vector3Df(-1,  1, -1));   // 2
  p->addVertex(Vector3Df(-1, -1,  1));   // 3

  /* Wound counter-clockwise seen from outside. Getting this backwards does
     not fail construction -- it silently inverts every normal and negates
     the volume -- so the volume cases assert the sign explicitly rather
     than taking the winding on trust. */
  p->addFace(0, 1, 2);
  p->addFace(0, 3, 1);
  p->addFace(0, 2, 3);
  p->addFace(1, 3, 2);

  p->finalize();
  return p;
}

/**
 * The unit cube from (0,0,0) to (s,s,s), as 12 triangles.
 *
 * V=8, E=18 (36 half-edges), F=12. Euler 8 - 18 + 12 = 2.
 *
 * Its volume is exactly s^3, which makes it the natural fixture for
 * checking the volume function and anything that scales.
 */
inline std::unique_ptr<Polyhedron> cube(float s = 1.0f) {
  auto p = std::make_unique<Polyhedron>();

  p->addVertex(Vector3Df(0, 0, 0));   // 0
  p->addVertex(Vector3Df(s, 0, 0));   // 1
  p->addVertex(Vector3Df(s, s, 0));   // 2
  p->addVertex(Vector3Df(0, s, 0));   // 3
  p->addVertex(Vector3Df(0, 0, s));   // 4
  p->addVertex(Vector3Df(s, 0, s));   // 5
  p->addVertex(Vector3Df(s, s, s));   // 6
  p->addVertex(Vector3Df(0, s, s));   // 7

  // bottom (z=0), normal -z
  p->addFace(0, 2, 1); p->addFace(0, 3, 2);
  // top (z=s), normal +z
  p->addFace(4, 5, 6); p->addFace(4, 6, 7);
  // front (y=0), normal -y
  p->addFace(0, 1, 5); p->addFace(0, 5, 4);
  // right (x=s), normal +x
  p->addFace(1, 2, 6); p->addFace(1, 6, 5);
  // back (y=s), normal +y
  p->addFace(2, 3, 7); p->addFace(2, 7, 6);
  // left (x=0), normal -x
  p->addFace(3, 0, 4); p->addFace(3, 4, 7);

  p->finalize();
  return p;
}

/**
 * A single triangle: an open surface with a boundary, not a closed solid.
 *
 * Useful for the cases that must distinguish a closed mesh from one with a
 * hole -- notably fillPolyhedronHoles, whose case would otherwise pass
 * against a fixture that never had a hole to fill.
 */
inline std::unique_ptr<Polyhedron> openTriangle() {
  auto p = std::make_unique<Polyhedron>();

  p->addVertex(Vector3Df(0, 0, 0));
  p->addVertex(Vector3Df(1, 0, 0));
  p->addVertex(Vector3Df(0, 1, 0));

  p->addFace(0, 1, 2);

  p->finalize();
  return p;
}

/** every half-edge has a twin and twins are mutual */
inline bool twinsAreMutual(const Polyhedron & p) {
  for (Polyhedron::const_edge_iterator it = p.eBegin(); it != p.eEnd(); ++it) {
    const HalfEdge * he = *it;
    if (he->twin() == nullptr) return false;
    if (he->twin()->twin() != he) return false;
  }
  return true;
}

/** he->src() is he->twin()->dst(), on every edge */
inline bool endpointsAgree(const Polyhedron & p) {
  for (Polyhedron::const_edge_iterator it = p.eBegin(); it != p.eEnd(); ++it) {
    const HalfEdge * he = *it;
    if (he->src() != he->twin()->dst()) return false;
  }
  return true;
}

/** walking next() around each face returns to the start in size() steps */
inline bool faceLoopsClose(const Polyhedron & p) {
  for (Polyhedron::const_face_iterator it = p.fBegin(); it != p.fEnd(); ++it) {
    const Face * f = *it;
    const int n = f->size();
    if (n < 3) return false;

    const HalfEdge * start = f->edge();
    const HalfEdge * he = start;

    for (int i = 0; i < n; i++) {
      if (he->face() != f) return false;
      he = he->next();
    }

    if (he != start) return false;
  }
  return true;
}

/** V - E + F, with E counted as half the half-edges */
inline int eulerCharacteristic(const Polyhedron & p) {
  return p.numVertices() - p.numHalfEdges() / 2 + p.numFaces();
}

/**
 * Signed volume by the divergence theorem, summed over triangles fanned
 * from the origin.
 *
 * Deliberately a SEPARATE implementation from src/halfedge/volume.cpp, and
 * a much simpler one: it is the independent oracle the volume cases check
 * that function against, so sharing code with it would make the comparison
 * circular. Only valid for triangulated meshes, which every fixture here
 * is.
 */
inline double independentVolume(const Polyhedron & p) {
  double total = 0;

  for (Polyhedron::const_face_iterator it = p.fBegin(); it != p.fEnd(); ++it) {
    const Face * f = *it;
    if (f->hole()) continue;

    std::vector<Vector3Df> corner;
    const HalfEdge * start = f->edge();
    const HalfEdge * he = start;
    do {
      corner.push_back(he->dst()->position());
      he = he->next();
    } while (he != start);

    /* fan the polygon from its first corner */
    for (size_t i = 1; i + 1 < corner.size(); i++) {
      const Vector3Df & a = corner[0];
      const Vector3Df & b = corner[i];
      const Vector3Df & c = corner[i + 1];

      total += a.x() * (b.y() * c.z() - b.z() * c.y())
             - a.y() * (b.x() * c.z() - b.z() * c.x())
             + a.z() * (b.x() * c.y() - b.y() * c.x());
    }
  }

  return total / 6.0;
}

} // namespace bttest

#endif
