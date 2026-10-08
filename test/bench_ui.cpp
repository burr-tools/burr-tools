/* Micro-benchmarks of the redesigned GUI's hot paths in src/uicore: building
 * a shape's mesh, the 3D view's depth sort, vector export.
 *
 * Hidden ([.]): `just test` skips them. `just bench-ui` runs them, and CI
 * reports them without failing on them -- shared runners are too noisy to
 * gate on time. Compare runs on one machine (design/2026-10-08-qtgui-
 * performance-backlog.md, "Measuring").
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include "../src/uicore/camera.h"
#include "../src/uicore/depthsort.h"
#include "../src/uicore/scenemesh.h"
#include "../src/uicore/vectorexport.h"

#include "test_helpers.h"

#include <random>

using namespace btui;

namespace {

  /* an edge^3 brick shape, filled; with `core`, a shell of fixed voxels
   * round a core of variable ones (a puzzle's result shape) */
  std::unique_ptr<voxel_c> cube(const gridType_c & gt, unsigned edge, bool core = false) {
    auto v = bttest::makeVoxel(gt, edge, edge, edge);
    v->setAll(voxel_c::VX_FILLED);
    if (core)
      for (unsigned z = 1; z + 1 < edge; z++)
        for (unsigned y = 1; y + 1 < edge; y++)
          for (unsigned x = 1; x + 1 < edge; x++)
            v->setState(x, y, z, voxel_c::VX_VARIABLE);
    return v;
  }

  MeshOptions blue(void) {
    MeshOptions o;
    o.piece = { 0, 0, 1 };
    return o;
  }
}

TEST_CASE("building a shape's mesh", "[.][bench][scenemesh]") {
  const gridType_c gt(gridType_c::GT_BRICKS);
  const auto solid8 = cube(gt, 8), solid20 = cube(gt, 20), shell20 = cube(gt, 20, true), solid50 = cube(gt, 50);
  BENCHMARK("8^3 solid") { return buildShapeMesh(*solid8, blue()).opaque.size(); };
  BENCHMARK("20^3 solid") { return buildShapeMesh(*solid20, blue()).opaque.size(); };
  BENCHMARK("20^3 shell round a variable core") { return buildShapeMesh(*shell20, blue()).translucent.size(); };
  BENCHMARK("50^3 solid") { return buildShapeMesh(*solid50, blue()).opaque.size(); };
}

TEST_CASE("the 3D view's depth sort", "[.][bench][depthsort]") {
  // what an orbit asks for every frame: the order again for a turned view
  std::mt19937 rng(7);
  std::uniform_real_distribution<float> d(-10.0f, 10.0f);
  std::vector<Vec3> centres(80000);
  for (Vec3 & c : centres)
    c = { d(rng), d(rng), d(rng) };
  Camera cam;
  cam.setViewport(800, 600);
  cam.setScene({ 0, 0, 0 }, 17.0f);
  DepthSorter sorter;
  std::vector<std::uint32_t> idx;
  int step = 0;
  BENCHMARK("80k triangles, a turned view") {
    const float a = float(step++) * 0.01f;
    cam.setOrientation(Camera::lookFrom({ std::sin(a), 0.4f, std::cos(a) }, 0));
    sorter.sort(centres, cam.viewMatrix(), 0, idx);
    return idx.size();
  };
}

TEST_CASE("vector export", "[.][bench][vectorexport]") {
  const gridType_c gt(gridType_c::GT_BRICKS);
  const auto shape = cube(gt, 20, true);
  const ShapeMesh mesh = buildShapeMesh(*shape, blue());
  std::vector<LineSeg> lines;
  addBoxEdges(lines, gridBounds(*shape), Rgba8{ 10, 20, 30, 255 }, 1.0f, 4.0f, 4.0f);
  Camera cam;
  cam.setViewport(800, 600);
  cam.setScene((mesh.boundsMin + mesh.boundsMax) * 0.5f, length(mesh.boundsMax - mesh.boundsMin) * 0.5f);
  VectorInput in;
  in.mesh = &mesh;
  in.lines = &lines;
  in.view = cam.viewMatrix();
  in.projection = cam.projectionMatrix();
  in.width = 800;
  in.height = 600;
  BENCHMARK("project a 20^3 shape") { return projectScene(in).prims.size(); };
  const VectorPage page = projectScene(in);
  BENCHMARK("write it as SVG") { return writeVector(page, VectorFormat::SVG, "bench").size(); };
}
