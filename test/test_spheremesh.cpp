#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "mesh_helpers.h"
#include "stl_reader.h"
#include "test_helpers.h"

#include "halfedge/face.h"
#include "halfedge/polyhedron.h"
#include "halfedge/volume.h"
#include "lib/gridtype.h"
#include "lib/stl.h"
#include "lib/stl_2.h"
#include "lib/voxel.h"

#include <cmath>
#include <filesystem>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

using Catch::Approx;
using namespace bttest;

/* voxel_2_mesh.cpp: 475 lines at 0%, the sphere grid's mesher and the
   largest single file in the mesh package.

   Step six of the design doc, and left until last there for two reasons.
   It is grid-specific, and P1 established what that costs: a sphere-grid
   shape has to be built from coordinates the grid accepts, because ASCII
   art addresses coordinates directly and the sphere grid takes only a
   subset of them. That machinery is reused here.

   Its own geometry needs no separate oracle. Once a legal shape exists the
   assertions are the mesh invariants the earlier files established --
   watertight in the half-edge sense, positive volume, face loops closed --
   applied to its output. */

namespace {

class TempDir {
public:
  TempDir() {
    path_ = std::filesystem::temp_directory_path() /
            ("bttest_sphere_" + std::to_string(++counter()));
    std::filesystem::remove_all(path_);
    std::filesystem::create_directories(path_);
  }
  ~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }
  std::string file(const char * name) const { return (path_ / name).string(); }
private:
  static int & counter() { static int n = 0; return n; }
  std::filesystem::path path_;
};

/**
 * A sphere-grid shape of `cells` spheres, built from coordinates the grid
 * actually accepts.
 *
 * The sphere grid takes only coordinates satisfying its own parity rule --
 * P1's grid-invariant work found 14 of the 27 positions in a 3x3x3 corner
 * are valid, and (1,0,0) is not among them. Filling by ASCII art would put
 * spheres at positions the grid does not have, which is how the mirror case
 * in test_voxel.cpp came to abort rather than merely fail.
 *
 * Returns nullptr when the box does not hold that many, so a caller can say
 * so rather than silently meshing a smaller shape.
 */
std::unique_ptr<voxel_c> sphereShape(const gridType_c & gt, int box, unsigned int cells) {
  std::unique_ptr<voxel_c> v = makeVoxel(gt, box, box, box);

  unsigned int placed = 0;
  for (int z = 0; z < box && placed < cells; z++)
    for (int y = 0; y < box && placed < cells; y++)
      for (int x = 0; x < box && placed < cells; x++)
        if (v->validCoordinate(x, y, z)) {
          v->setState(x, y, z, voxel_c::VX_FILLED);
          placed++;
        }

  if (placed < cells) return nullptr;
  return v;
}

int solidFaces(const Polyhedron & p) {
  int n = 0;
  for (Polyhedron::const_face_iterator it = p.fBegin(); it != p.fEnd(); ++it)
    if (!(*it)->hole()) n++;
  return n;
}

} // namespace

TEST_CASE("sphere mesh: a single sphere meshes to a closed solid with positive volume",
          "[spheremesh]") {
  gridType_c gt(gridType_c::GT_SPHERES);

  std::unique_ptr<voxel_c> v = sphereShape(gt, 4, 1);
  REQUIRE(v != nullptr);
  REQUIRE(v->countState(voxel_c::VX_FILLED) == 1);

  stlExporter_2_c exp;
  std::unique_ptr<Polyhedron> mesh(exp.getMesh(*v));

  REQUIRE(mesh != nullptr);
  REQUIRE(mesh->numVertices() > 0);
  REQUIRE(solidFaces(*mesh) > 0);

  /* every face loop closes and names its own face -- the structural
     property the STL writer walks */
  REQUIRE(faceLoopsClose(*mesh));

  /* a solid, not an inside-out one. The winding is what an STL consumer
     depends on and a mesher can invert with no other symptom. */
  REQUIRE(volume(*mesh) > 0.0);
}

TEST_CASE("sphere mesh: the mesher's volume agrees with an independent calculation",
          "[spheremesh]") {
  gridType_c gt(gridType_c::GT_SPHERES);

  std::unique_ptr<voxel_c> v = sphereShape(gt, 4, 1);
  REQUIRE(v != nullptr);

  stlExporter_2_c exp;
  std::unique_ptr<Polyhedron> mesh(exp.getMesh(*v));
  REQUIRE(mesh != nullptr);

  /* independentVolume() is the separate, much simpler implementation in
     mesh_helpers.h. Agreement between two routes is what makes the sign
     assertion above more than a coincidence, and it is a real check on a
     mesh of a few hundred faces rather than a hand-built fixture. */
  REQUIRE(volume(*mesh) == Approx(independentVolume(*mesh)).epsilon(1e-3));
}

TEST_CASE("sphere mesh: more spheres make a bigger solid", "[spheremesh]") {
  gridType_c gt(gridType_c::GT_SPHERES);

  std::unique_ptr<voxel_c> one = sphereShape(gt, 4, 1);
  std::unique_ptr<voxel_c> four = sphereShape(gt, 4, 4);
  REQUIRE(one != nullptr);
  REQUIRE(four != nullptr);
  REQUIRE(four->countState(voxel_c::VX_FILLED) == 4);

  stlExporter_2_c exp;

  std::unique_ptr<Polyhedron> meshOne(exp.getMesh(*one));
  std::unique_ptr<Polyhedron> meshFour(exp.getMesh(*four));
  REQUIRE(meshOne != nullptr);
  REQUIRE(meshFour != nullptr);

  /* The mesher must depend on the shape it is handed. Without this every
     other case here would hold against one that emitted a fixed solid.

     Not asserted as exactly four times: the spheres touch and are joined
     by connection geometry, so four joined spheres are not four separate
     ones. Only the direction is a property of the operation. */
  REQUIRE(volume(*meshFour) > volume(*meshOne) * 1.5);
  REQUIRE(solidFaces(*meshFour) > solidFaces(*meshOne));
}

TEST_CASE("sphere mesh: raising the recursion count refines the mesh without changing the solid "
          "much", "[spheremesh]") {
  gridType_c gt(gridType_c::GT_SPHERES);

  std::unique_ptr<voxel_c> v = sphereShape(gt, 4, 1);
  REQUIRE(v != nullptr);

  auto meshAt = [&](double recursion) {
    stlExporter_2_c exp;

    bool found = false;
    for (unsigned int i = 0; i < exp.numParameters(); i++)
      if (std::string(exp.getParameterName(i)) == "Recursions") {
        exp.setParameter(i, recursion);
        found = true;
        break;
      }
    REQUIRE(found);

    return std::unique_ptr<Polyhedron>(exp.getMesh(*v));
  };

  std::unique_ptr<Polyhedron> coarse = meshAt(1);
  std::unique_ptr<Polyhedron> fine = meshAt(3);

  REQUIRE(coarse != nullptr);
  REQUIRE(fine != nullptr);

  /* recursion subdivides: more faces, strictly */
  REQUIRE(solidFaces(*fine) > solidFaces(*coarse));

  /* ...approaching the same sphere from inside, so the finer mesh encloses
     at least as much and not wildly more. A subdivision that moved the
     surface rather than refining it would break this band, and a
     "recursion does nothing" would break the face count above. */
  const double coarseVol = volume(*coarse);
  const double fineVol = volume(*fine);

  REQUIRE(coarseVol > 0.0);
  REQUIRE(fineVol >= coarseVol);
  REQUIRE(fineVol < coarseVol * 1.5);
}

TEST_CASE("sphere mesh: the sphere radius scales the solid", "[spheremesh]") {
  gridType_c gt(gridType_c::GT_SPHERES);

  std::unique_ptr<voxel_c> v = sphereShape(gt, 4, 1);
  REQUIRE(v != nullptr);

  auto volumeAtRadius = [&](double radius) {
    stlExporter_2_c exp;

    bool found = false;
    for (unsigned int i = 0; i < exp.numParameters(); i++)
      if (std::string(exp.getParameterName(i)) == "Sphere radius") {
        exp.setParameter(i, radius);
        found = true;
        break;
      }
    REQUIRE(found);

    std::unique_ptr<Polyhedron> mesh(exp.getMesh(*v));
    REQUIRE(mesh != nullptr);
    return volume(*mesh);
  };

  const double small = volumeAtRadius(10.0);
  const double big = volumeAtRadius(20.0);

  REQUIRE(small > 0.0);

  /* doubling the radius multiplies a sphere's volume by eight. A band
     rather than a point, because a single sphere in this grid is not a
     bare sphere -- but narrow enough to exclude "the parameter did
     nothing" and "it scaled linearly". */
  REQUIRE(big > small * 6.0);
  REQUIRE(big < small * 10.0);
}

TEST_CASE("sphere mesh: every parameter of the sphere exporter round-trips", "[spheremesh]") {
  stlExporter_2_c exp;

  REQUIRE(exp.numParameters() == 8);

  for (unsigned int i = 0; i < exp.numParameters(); i++) {
    INFO("parameter " << i << " (" << exp.getParameterName(i) << ")");

    REQUIRE(exp.getParameterName(i) != nullptr);
    REQUIRE(exp.getParameterTooltip(i) != nullptr);

    const double before = exp.getParameter(i);

    exp.setParameter(i, 1.0);
    REQUIRE(exp.getParameter(i) == Approx(1.0));

    exp.setParameter(i, before);
    REQUIRE(exp.getParameter(i) == Approx(before));
  }
}

TEST_CASE("sphere mesh: the sphere exporter writes an STL that parses back", "[spheremesh][stl]") {
  TempDir dir;
  const std::string path = dir.file("spheres.stl");

  gridType_c gt(gridType_c::GT_SPHERES);
  std::unique_ptr<voxel_c> v = sphereShape(gt, 4, 2);
  REQUIRE(v != nullptr);

  stlExporter_2_c exp;
  exp.setBinaryMode(true);
  exp.write(path.c_str(), *v);

  StlMesh m = readBinaryStl(slurp(path));

  REQUIRE(m.triangles.size() > 0);

  /* the file is exactly as long as its triangle count claims -- the
     seek-and-patch header again, on a different exporter */
  REQUIRE(slurp(path).size() == 84 + m.triangles.size() * 50);

  /* Finite coordinates, scanned to a single assertion rather than one per
     component: this mesh runs to thousands of triangles and asserting nine
     times each inflates the suite's count by five figures without covering
     anything further. NaN coordinates are what a degenerate face produces
     and they survive both encodings silently, so the check matters -- it
     is the reporting that needed fixing, not the check. */
  bool allFinite = true;
  size_t firstBad = 0;

  for (size_t t = 0; t < m.triangles.size() && allFinite; t++)
    for (int c = 0; c < 3; c++)
      for (int i = 0; i < 3; i++)
        if (!std::isfinite(m.triangles[t].vertex[c][i])) {
          allFinite = false;
          firstBad = t;
        }

  INFO("first non-finite coordinate in triangle " << firstBad);
  REQUIRE(allFinite);

  /* an outward winding, as for the cube exporter */
  REQUIRE(stlVolume(m) > 0.0);
}

TEST_CASE("sphere mesh: the written STL encloses the volume the mesher reported",
          "[spheremesh][stl]") {
  TempDir dir;
  const std::string path = dir.file("spheres.stl");

  gridType_c gt(gridType_c::GT_SPHERES);
  std::unique_ptr<voxel_c> v = sphereShape(gt, 4, 2);
  REQUIRE(v != nullptr);

  stlExporter_2_c exp;
  exp.setBinaryMode(true);

  std::unique_ptr<Polyhedron> mesh(exp.getMesh(*v));
  REQUIRE(mesh != nullptr);
  const double meshVolume = volume(*mesh);

  exp.write(path.c_str(), *v);
  const double fileVolume = stlVolume(readBinaryStl(slurp(path)));

  /* The end-to-end check: what the mesher built and what reached the file
     are the same solid. This is what would catch the writer dropping faces
     or the coplanar merge changing the geometry -- neither of which the
     per-triangle assertions above would see. */
  REQUIRE(meshVolume > 0.0);
  REQUIRE(fileVolume == Approx(meshVolume).epsilon(1e-2));
}
