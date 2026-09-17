#include <catch2/catch_test_macros.hpp>

#include "mesh_helpers.h"

#include "halfedge/face.h"
#include "halfedge/halfedge.h"
#include "halfedge/modifiers.h"
#include "halfedge/polyhedron.h"
#include "halfedge/vertex.h"
#include "halfedge/volume.h"

#include <memory>
#include <vector>

using namespace bttest;

/* modifiers.cpp: 606 lines at 1%, the largest single uncovered file in the
   mesh package.

   Step four of the design doc, and its rule governs everything here: each
   operation is checked against what it PRESERVES or ESTABLISHES, never
   against recorded output. Volume, face counts and structural validity are
   all things that can be asserted without knowing in advance what the mesh
   will look like. */

namespace {

/**
 * A unit cube whose top face is a fan of four triangles around an added
 * centre vertex, instead of the usual two.
 *
 * This is the fixture mergeCoplanarFaces needs. A plain cube is no good:
 * each of its faces is already two coplanar triangles, and merging a pair
 * into a quad and retriangulating gives two triangles back, so the face
 * count does not move and a "fewer faces" assertion proves nothing.
 *
 * The extra vertex is INTERIOR to the top face -- it adds no vertex to the
 * face's outline. That matters because the header promises to keep every
 * vertex on a merged group's boundary, so a subdivision that added boundary
 * vertices would have to keep them and the count would not drop either.
 * Here four coplanar triangles can collapse to two.
 */
std::unique_ptr<Polyhedron> cubeWithFannedTop() {
  auto p = std::make_unique<Polyhedron>();

  p->addVertex(Vector3Df(0, 0, 0));        // 0
  p->addVertex(Vector3Df(1, 0, 0));        // 1
  p->addVertex(Vector3Df(1, 1, 0));        // 2
  p->addVertex(Vector3Df(0, 1, 0));        // 3
  p->addVertex(Vector3Df(0, 0, 1));        // 4
  p->addVertex(Vector3Df(1, 0, 1));        // 5
  p->addVertex(Vector3Df(1, 1, 1));        // 6
  p->addVertex(Vector3Df(0, 1, 1));        // 7
  p->addVertex(Vector3Df(0.5f, 0.5f, 1));  // 8 -- the centre of the top face

  // bottom
  p->addFace(0, 2, 1); p->addFace(0, 3, 2);
  // top, fanned from vertex 8
  p->addFace(4, 5, 8); p->addFace(5, 6, 8);
  p->addFace(6, 7, 8); p->addFace(7, 4, 8);
  // sides
  p->addFace(0, 1, 5); p->addFace(0, 5, 4);
  p->addFace(1, 2, 6); p->addFace(1, 6, 5);
  p->addFace(2, 3, 7); p->addFace(2, 7, 6);
  p->addFace(3, 0, 4); p->addFace(3, 4, 7);

  p->finalize();
  return p;
}

/** the number of faces that are not holes */
int solidFaces(const Polyhedron & p) {
  int n = 0;
  for (Polyhedron::const_face_iterator it = p.fBegin(); it != p.fEnd(); ++it)
    if (!(*it)->hole()) n++;
  return n;
}

} // namespace

/* ------------------------------------------------------------------ */
/* scaling                                                             */
/* ------------------------------------------------------------------ */

TEST_CASE("scalePolyhedron: uniform scaling multiplies the volume by the cube of the factor",
          "[modifiers]") {
  std::unique_ptr<Polyhedron> p = cube(1.0f);

  const int vertices = p->numVertices();
  const int faces = p->numFaces();
  const double before = volume(*p);

  scalePolyhedron(*p, 3.0f);

  /* the ratio, not two constants, so it holds whatever the absolute
     figures are */
  REQUIRE(volume(*p) == meshApprox(before * 27.0));

  /* scaling moves vertices; it must not add or remove any, nor any face */
  REQUIRE(p->numVertices() == vertices);
  REQUIRE(p->numFaces() == faces);

  /* and the structure still hangs together afterwards */
  REQUIRE(twinsAreMutual(*p));
  REQUIRE(faceLoopsClose(*p));
}

TEST_CASE("scalePolyhedron: scaling up then down returns the original coordinates", "[modifiers]") {
  std::unique_ptr<Polyhedron> p = cube(1.0f);

  std::vector<Vector3Df> before;
  for (int i = 0; i < p->numVertices(); i++)
    before.push_back(p->vertex(i)->position());

  scalePolyhedron(*p, 8.0f);
  scalePolyhedron(*p, 1.0f / 8.0f);

  /* 8 is a power of two, so the round trip is exact in binary floating
     point and the tolerance is not doing the work here */
  for (int i = 0; i < p->numVertices(); i++) {
    INFO("vertex " << i);
    REQUIRE(p->vertex(i)->position().x() == meshApprox(before[i].x()));
    REQUIRE(p->vertex(i)->position().y() == meshApprox(before[i].y()));
    REQUIRE(p->vertex(i)->position().z() == meshApprox(before[i].z()));
  }
}

TEST_CASE("scalePolyhedron: the three-argument overload scales each axis independently",
          "[modifiers]") {
  std::unique_ptr<Polyhedron> p = cube(1.0f);

  scalePolyhedron(*p, 2.0f, 3.0f, 5.0f);

  /* the unit cube becomes a 2 x 3 x 5 box */
  REQUIRE(volume(*p) == meshApprox(30.0));

  /* Asserting the extent on each axis separately, because the volume alone
     is satisfied by any three factors with the same product -- including
     the uniform scaling by the cube root of 30 that a broken overload
     might do. */
  float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f, minZ = 1e9f, maxZ = -1e9f;
  for (int i = 0; i < p->numVertices(); i++) {
    const Vector3Df & v = p->vertex(i)->position();
    minX = std::fmin(minX, v.x()); maxX = std::fmax(maxX, v.x());
    minY = std::fmin(minY, v.y()); maxY = std::fmax(maxY, v.y());
    minZ = std::fmin(minZ, v.z()); maxZ = std::fmax(maxZ, v.z());
  }

  REQUIRE(maxX - minX == meshApprox(2.0));
  REQUIRE(maxY - minY == meshApprox(3.0));
  REQUIRE(maxZ - minZ == meshApprox(5.0));
}

TEST_CASE("scalePolyhedron: scaling by one leaves every coordinate alone", "[modifiers]") {
  std::unique_ptr<Polyhedron> p = tetrahedron();

  std::vector<Vector3Df> before;
  for (int i = 0; i < p->numVertices(); i++)
    before.push_back(p->vertex(i)->position());

  scalePolyhedron(*p, 1.0f);
  scalePolyhedron(*p, 1.0f, 1.0f, 1.0f);

  for (int i = 0; i < p->numVertices(); i++) {
    INFO("vertex " << i);
    REQUIRE(p->vertex(i)->position() == before[i]);
  }
}

/* ------------------------------------------------------------------ */
/* merging coplanar faces                                              */
/* ------------------------------------------------------------------ */

TEST_CASE("mergeCoplanarFaces: the fanned top really is four coplanar triangles", "[modifiers]") {
  /* The premise the merge cases rest on, asserted rather than assumed. If
     the fixture's top were not four coplanar faces there would be nothing
     to merge and every "the count went down" assertion below would be
     testing the wrong thing -- or passing for the wrong reason. */
  std::unique_ptr<Polyhedron> p = cubeWithFannedTop();

  REQUIRE(p->numVertices() == 9);
  REQUIRE(solidFaces(*p) == 14);   // 2 bottom + 4 top + 8 sides

  int topFaces = 0;
  for (Polyhedron::const_face_iterator it = p->fBegin(); it != p->fEnd(); ++it) {
    const Face * f = *it;
    if (f->hole()) continue;

    /* a face of the top lies entirely at z == 1 */
    bool allTop = true;
    const HalfEdge * start = f->edge();
    const HalfEdge * he = start;
    do {
      if (he->dst()->position().z() != meshApprox(1.0)) allTop = false;
      he = he->next();
    } while (he != start);

    if (allTop) topFaces++;
  }

  REQUIRE(topFaces == 4);
}

TEST_CASE("mergeCoplanarFaces: merging preserves the volume", "[modifiers]") {
  std::unique_ptr<Polyhedron> src = cubeWithFannedTop();

  const double before = volume(*src);
  REQUIRE(before == meshApprox(1.0));   // it is still a unit cube

  std::unique_ptr<Polyhedron> merged(mergeCoplanarFaces(*src));
  REQUIRE(merged != nullptr);

  /* The property that matters most: retriangulating a coplanar group must
     not move the surface. A merge that dropped a triangle or wound one
     backwards changes the volume, and nothing else here would notice. */
  REQUIRE(volume(*merged) == meshApprox(before));
}

TEST_CASE("mergeCoplanarFaces: merging reduces the face count on a group that can collapse",
          "[modifiers]") {
  std::unique_ptr<Polyhedron> src = cubeWithFannedTop();

  const int before = solidFaces(*src);

  std::unique_ptr<Polyhedron> merged(mergeCoplanarFaces(*src));
  REQUIRE(merged != nullptr);

  const int after = solidFaces(*merged);

  /* The header's contract is "fewer, larger triangles". The four fanned
     top triangles form one square, which needs only two. Asserting a
     strict decrease rather than an exact number: the retriangulation is
     free to choose its own diagonal and to handle the other faces as it
     sees fit, and pinning the exact count would be recording output. */
  REQUIRE(after < before);
}

TEST_CASE("mergeCoplanarFaces: the merged mesh is still a valid closed surface", "[modifiers]") {
  std::unique_ptr<Polyhedron> src = cubeWithFannedTop();
  std::unique_ptr<Polyhedron> merged(mergeCoplanarFaces(*src));

  REQUIRE(merged != nullptr);

  /* Every face loop closes and names its own face -- the structural
     property the STL writer depends on, since it walks each face's edge
     circulator and nothing else. */
  REQUIRE(faceLoopsClose(*merged));

  /* The result is NOT twin-linked, and that is the contract rather than a
     defect. mergeCoplanarFaces builds a fresh polyhedron by adding faces
     and never calls finalize(), which is what establishes twin links; its
     header documents the requirement only on the SOURCE ("the source
     polyhedron must have its twin links set"). Its one caller, the STL
     writer in stl.cpp, iterates faces and their edge loops and never
     touches a twin.

     Pinned because it is a trap: any future caller wanting adjacency,
     numHoles(), or check() off a merged mesh has to finalize() it first,
     and nothing in the signature says so. */
  REQUIRE_FALSE(twinsAreMutual(*merged));

  /* ...and finalize() does establish them, so the omission is a missing
     step rather than a mesh that cannot be repaired */
  merged->finalize();

  REQUIRE(twinsAreMutual(*merged));
  REQUIRE(endpointsAgree(*merged));
  REQUIRE(faceLoopsClose(*merged));

  /* still a topological sphere once the links are in place */
  REQUIRE(eulerCharacteristic(*merged) == 2);
}

TEST_CASE("mergeCoplanarFaces: the source polyhedron is left untouched", "[modifiers]") {
  std::unique_ptr<Polyhedron> src = cubeWithFannedTop();

  const int faces = solidFaces(*src);
  const int vertices = src->numVertices();
  const double vol = volume(*src);

  std::unique_ptr<Polyhedron> merged(mergeCoplanarFaces(*src));
  REQUIRE(merged != nullptr);

  /* the signature takes a const reference and returns a new polyhedron, so
     the caller's mesh must survive intact -- stl.cpp relies on this, it
     reset()s its own pointer from the result */
  REQUIRE(solidFaces(*src) == faces);
  REQUIRE(src->numVertices() == vertices);
  REQUIRE(volume(*src) == meshApprox(vol));

  /* and the result is genuinely a different object, not the same one back */
  REQUIRE(merged.get() != src.get());
}

TEST_CASE("mergeCoplanarFaces: a mesh with nothing to merge survives unchanged in volume and "
          "validity", "[modifiers]") {
  /* A tetrahedron has no two coplanar faces at all, so every group is a
     single face and the retriangulation has nothing to do. The result must
     still be a well-formed mesh of the same volume -- the identity case,
     which a merge that mangled singleton groups would fail. */
  std::unique_ptr<Polyhedron> src = tetrahedron();

  const double before = volume(*src);

  std::unique_ptr<Polyhedron> merged(mergeCoplanarFaces(*src));
  REQUIRE(merged != nullptr);

  REQUIRE(volume(*merged) == meshApprox(before));
  REQUIRE(solidFaces(*merged) == solidFaces(*src));
  REQUIRE(faceLoopsClose(*merged));

  /* as above, twins are the caller's job after a merge */
  merged->finalize();
  REQUIRE(twinsAreMutual(*merged));
  REQUIRE(eulerCharacteristic(*merged) == 2);
}

TEST_CASE("mergeCoplanarFaces: merging is idempotent", "[modifiers]") {
  /* Once a group has been collapsed there is nothing further to collapse,
     so a second pass must change neither the volume nor the face count. A
     merge that kept finding work to do would either be failing to merge
     properly the first time or introducing faces on each pass. */
  std::unique_ptr<Polyhedron> src = cubeWithFannedTop();

  std::unique_ptr<Polyhedron> once(mergeCoplanarFaces(*src));
  REQUIRE(once != nullptr);

  const int faces = solidFaces(*once);
  const double vol = volume(*once);

  /* finalize() before merging again, or the second pass is not a merge.

     mergeCoplanarFaces groups coplanar neighbours by walking half-edge
     twin() links, and it builds its result with addFace/copyFace without
     joining the twins afterwards. So `once` comes back with no twin links
     at all: fed straight back in, every face looks like an isolated group
     of one, the size > 1 branch is never taken, and the second call simply
     copies each face across. Equal face counts and volumes then say
     nothing about idempotence -- they are what a copy produces.

     finalize() is what joins the half-edges, so this is what actually
     asks the merge to run twice. */
  once->finalize();

  std::unique_ptr<Polyhedron> twice(mergeCoplanarFaces(*once));
  REQUIRE(twice != nullptr);

  REQUIRE(solidFaces(*twice) == faces);
  REQUIRE(volume(*twice) == meshApprox(vol));
}

TEST_CASE("mergeCoplanarFaces: every face of the result is a triangle", "[modifiers]") {
  /* "retriangulated with fewer, larger triangles" -- the output is fed
     straight to the STL writer, which fans each face into triangles and
     would silently emit garbage for a face that was not one. */
  std::unique_ptr<Polyhedron> merged(mergeCoplanarFaces(*cubeWithFannedTop()));
  REQUIRE(merged != nullptr);

  for (Polyhedron::const_face_iterator it = merged->fBegin(); it != merged->fEnd(); ++it) {
    const Face * f = *it;
    if (f->hole()) continue;

    INFO("face " << f->index());
    REQUIRE(f->size() == 3);
  }
}
