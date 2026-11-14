/* Tests for btui's vector export: projecting a frame onto a page and the
 * six legacy formats.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "../src/uicore/camera.h"
#include "../src/uicore/scenemesh.h"
#include "../src/uicore/vectorexport.h"

#include "test_helpers.h"

#include <set>
#include <string>

using namespace btui;

namespace {

  struct Scene {
    gridType_c gt{ gridType_c::GT_BRICKS };
    std::unique_ptr<voxel_c> v;
    ShapeMesh mesh;
    Camera cam;
    std::vector<LineSeg> lines;

    explicit Scene(bool variable = false) {
      v = bttest::makeVoxel(gt, 2, 2, 2);
      v->setAll(voxel_c::VX_FILLED);
      if (variable)
        v->setState(1, 1, 1, voxel_c::VX_VARIABLE);
      MeshOptions o;
      o.piece = { 0, 0, 1 };
      mesh = buildShapeMesh(*v, o);
      cam.setViewport(320, 240);
      cam.setScene((mesh.boundsMin + mesh.boundsMax) * 0.5f, length(mesh.boundsMax - mesh.boundsMin) * 0.5f);
      cam.home();
      cam.tick(10000);
      addBoxEdges(lines, gridBounds(*v), Rgba8{ 10, 20, 30, 255 }, 1.0f, 4.0f, 4.0f);
    }

    VectorInput input(void) const {
      VectorInput in;
      in.mesh = &mesh;
      in.lines = &lines;
      in.view = cam.viewMatrix();
      in.projection = cam.projectionMatrix();
      in.width = 320;
      in.height = 240;
      return in;
    }
  };

  size_t count(const std::string & s, const std::string & what) {
    size_t n = 0;
    for (size_t p = s.find(what); p != std::string::npos; p = s.find(what, p + 1))
      n++;
    return n;
  }
}

TEST_CASE("the view projects onto the page, back faces dropped, back to front", "[ui][vectorexport]") {
  Scene s;
  const VectorPage page = projectScene(s.input());
  CHECK(page.width == 320);
  CHECK(page.height == 240);

  size_t polys = 0, lines = 0;
  for (const auto & p : page.prims) {
    if (p.kind == VectorPrimitive::Kind::Polygon) polys++; else lines++;
    for (size_t i = 0; i < p.xy.size(); i += 2) {
      CHECK(p.xy[i] >= 0);
      CHECK(p.xy[i] <= 320);
      CHECK(p.xy[i + 1] >= 0);
      CHECK(p.xy[i + 1] <= 240);
    }
  }
  const size_t triangles = s.mesh.opaque.size() / 3;
  // a cube seen from a corner shows three of its six sides
  CHECK(polys > triangles / 4);
  CHECK(polys < triangles * 3 / 4);
  CHECK(lines == 12);

  for (size_t i = 1; i < page.prims.size(); i++)
    CHECK(page.prims[i - 1].depth >= page.prims[i].depth);
}

TEST_CASE("faces are shaded as the renderer shades them", "[ui][vectorexport]") {
  Scene s;
  VectorInput in = s.input();
  in.lines = nullptr;

  in.lighting = false;
  std::set<int> flatBlues;
  for (const auto & p : projectScene(in).prims) {
    CHECK(p.r == p.g);     // blue: the light shade lifts red and green alike
    flatBlues.insert(int(std::lround(p.b * 1000)));
  }
  // unlit: the two checker shades at 92 %, nothing else
  CHECK(flatBlues.size() <= 2);

  in.lighting = true;
  std::set<int> litBlues;
  for (const auto & p : projectScene(in).prims)
    litBlues.insert(int(std::lround(p.b * 1000)));
  CHECK(litBlues.size() > flatBlues.size());
}

TEST_CASE("variable voxels and dimmed layers keep their transparency", "[ui][vectorexport]") {
  Scene s(true);
  VectorInput in = s.input();
  in.lines = nullptr;
  bool translucent = false;
  for (const auto & p : projectScene(in).prims)
    translucent |= p.a < 1.0f;
  CHECK(translucent);

  Scene solid;
  in = solid.input();
  in.lines = nullptr;
  in.dimAxis = 2;
  in.dimLayer = 0;
  std::set<int> alphas;
  for (const auto & p : projectScene(in).prims)
    alphas.insert(int(std::lround(p.a * 100)));
  CHECK(alphas.count(100) == 1);
  CHECK(alphas.count(28) == 1);
}

TEST_CASE("the Classic style keeps the faces seen from behind", "[ui][vectorexport]") {
  // as the 3D view draws it: the bevelled mesh's open seams between voxels
  // show the far side, which is seen from behind (SceneFrame::cullBackFaces)
  gridType_c gt{ gridType_c::GT_BRICKS };
  auto v = bttest::makeVoxel(gt, 2, 2, 2);
  v->setAll(voxel_c::VX_FILLED);
  MeshOptions o;
  o.style = VoxelStyle::Legacy;
  o.piece = { 0, 0, 1 };
  const ShapeMesh mesh = buildShapeMesh(*v, o);
  Camera cam;
  cam.setViewport(320, 240);
  cam.setScene((mesh.boundsMin + mesh.boundsMax) * 0.5f, length(mesh.boundsMax - mesh.boundsMin) * 0.5f);
  cam.home();
  cam.tick(10000);
  VectorInput in;
  in.mesh = &mesh;
  in.view = cam.viewMatrix();
  in.projection = cam.projectionMatrix();
  in.width = 320;
  in.height = 240;
  const size_t front = projectScene(in).prims.size();
  in.cullBackFaces = false;
  const VectorPage both = projectScene(in);
  CHECK(both.prims.size() > front);
  // still back to front
  for (size_t i = 1; i < both.prims.size(); i++)
    CHECK(both.prims[i - 1].depth >= both.prims[i].depth);
}

TEST_CASE("an empty view gives an empty page", "[ui][vectorexport]") {
  VectorInput in;
  in.width = 100;
  in.height = 50;
  const VectorPage page = projectScene(in);
  CHECK(page.prims.empty());
  CHECK(writeVector(page, VectorFormat::SVG, "x").find("<polygon") == std::string::npos);
}

TEST_CASE("every legacy format is written", "[ui][vectorexport]") {
  Scene s(true);
  const VectorPage page = projectScene(s.input());
  const size_t polys = size_t(std::count_if(page.prims.begin(), page.prims.end(),
    [](const VectorPrimitive & p) { return p.kind == VectorPrimitive::Kind::Polygon; }));

  SECTION("SVG") {
    const std::string out = writeVector(page, VectorFormat::SVG, "x");
    CHECK(out.rfind("<?xml", 0) == 0);
    CHECK(out.find("width=\"320pt\" height=\"240pt\"") != std::string::npos);
    CHECK(count(out, "<polygon") == polys);
    CHECK(count(out, "<line") == 12);
    CHECK(out.find("fill-opacity") != std::string::npos);
    CHECK(out.find("stroke-dasharray=\"4 4\"") != std::string::npos);
    CHECK(out.find("</svg>") != std::string::npos);
  }
  SECTION("PostScript and EPS") {
    const std::string ps = writeVector(page, VectorFormat::PS, "x");
    const std::string eps = writeVector(page, VectorFormat::EPS, "x");
    CHECK(ps.rfind("%!PS-Adobe-3.0\n", 0) == 0);
    CHECK(eps.rfind("%!PS-Adobe-3.0 EPSF-3.0\n", 0) == 0);
    CHECK(eps.find("%%BoundingBox: 0 0 320 240") != std::string::npos);
    CHECK(ps.find("showpage") != std::string::npos);
    CHECK(eps.find("showpage") == std::string::npos);
    CHECK(count(ps, " fill") >= polys);
  }
  SECTION("PDF with a valid cross-reference table") {
    const std::string out = writeVector(page, VectorFormat::PDF, "x");
    CHECK(out.rfind("%PDF-1.4", 0) == 0);
    const size_t sx = out.rfind("startxref\n");
    REQUIRE(sx != std::string::npos);
    const size_t xref = std::stoul(out.substr(sx + 10));
    REQUIRE(out.compare(xref, 4, "xref") == 0);
    // every object starts where the table says
    const size_t n = std::stoul(out.substr(xref + 7));
    size_t line = out.find('\n', out.find('\n', xref + 5) + 1) + 1;   // after the free entry
    for (size_t i = 1; i < n; i++) {
      const size_t off = std::stoul(out.substr(line, 10));
      CHECK(out.compare(off, std::to_string(i).size() + 6, std::to_string(i) + " 0 obj") == 0);
      line += 20;
    }
    CHECK(out.find("/ca 0.6") != std::string::npos);     // the variable voxel's transparency
  }
  SECTION("PGF and TeX") {
    const std::string pgf = writeVector(page, VectorFormat::PGF, "x");
    CHECK(pgf.find("\\begin{pgfpicture}") != std::string::npos);
    CHECK(pgf.find("\\end{pgfpicture}") != std::string::npos);
    CHECK(count(pgf, "\\pgfusepath{fill}") == polys);
    const std::string tex = writeVector(page, VectorFormat::TeX, "my-burr");
    CHECK(tex.find("\\includegraphics{my-burr}") != std::string::npos);
    CHECK(tex.find("\\begin{picture}(320,240)(0,0)") != std::string::npos);
  }
  CHECK(vectorExtension(VectorFormat::EPS) == "eps");
}

TEST_CASE("objects anchored in the view land at their places on the page", "[ui][vectorexport]") {
  Scene s(false);
  VectorInput in = s.input();
  in.lines = nullptr;
  const VectorPage plain = projectScene(in);

  // two copies, side by side, as the Puzzle scene places them
  ShapeMesh two;
  for (const float x : { -10.0f, 10.0f })
    for (MeshVertex v : s.mesh.opaque) {
      v.cell[0] = x;
      v.cell[1] = v.cell[2] = 0;
      two.opaque.push_back(v);
    }
  in.mesh = &two;
  in.anchored = true;
  const VectorPage page = projectScene(in);

  REQUIRE(page.prims.size() == 2 * plain.prims.size());
  float low = page.width, high = 0, plainLow = page.width, plainHigh = 0;
  for (const VectorPrimitive & p : page.prims)
    for (size_t i = 0; i < p.xy.size(); i += 2) {
      low = std::min(low, p.xy[i]);
      high = std::max(high, p.xy[i]);
    }
  for (const VectorPrimitive & p : plain.prims)
    for (size_t i = 0; i < p.xy.size(); i += 2) {
      plainLow = std::min(plainLow, p.xy[i]);
      plainHigh = std::max(plainHigh, p.xy[i]);
    }
  CHECK(high - low > 1.5f * (plainHigh - plainLow));

  // and the same mesh not anchored stacks them
  in.anchored = false;
  float stackedLow = page.width, stackedHigh = 0;
  for (const VectorPrimitive & p : projectScene(in).prims)
    for (size_t i = 0; i < p.xy.size(); i += 2) {
      stackedLow = std::min(stackedLow, p.xy[i]);
      stackedHigh = std::max(stackedHigh, p.xy[i]);
    }
  CHECK_THAT(stackedHigh - stackedLow, Catch::Matchers::WithinAbs(plainHigh - plainLow, 0.01));
}

// --- a shape's picture for the lists ----------------------------------------------

TEST_CASE("a shape's picture fills its square, centred, in the shape's colour", "[ui][vectorexport]") {
  gridType_c gt{ gridType_c::GT_BRICKS };
  auto v = bttest::makeVoxel(gt, 4, 1, 1);           // a bar: wider than high, seen from Home
  v->setAll(voxel_c::VX_FILLED);
  v->setState(3, 0, 0, voxel_c::VX_VARIABLE);         // variable voxels drawn opaque
  const VectorPage page = shapePicture(*v, Rgb{ 0, 0, 1 }, 28, 2);
  REQUIRE_FALSE(page.prims.empty());
  CHECK(page.width == 28);
  float x0 = 99, y0 = 99, x1 = -99, y1 = -99;
  for (const VectorPrimitive & p : page.prims) {
    CHECK(p.kind == VectorPrimitive::Kind::Polygon);
    CHECK(p.a == 1.0f);
    CHECK(p.b > p.r);                                  // blue, shaded
    for (size_t i = 0; i + 1 < p.xy.size(); i += 2) {
      x0 = std::min(x0, p.xy[i]);
      x1 = std::max(x1, p.xy[i]);
      y0 = std::min(y0, p.xy[i + 1]);
      y1 = std::max(y1, p.xy[i + 1]);
    }
  }
  using Catch::Matchers::WithinAbs;
  // the longer side reaches the margins, the other is centred
  CHECK_THAT(std::max(x1 - x0, y1 - y0), WithinAbs(24, 0.01));
  CHECK_THAT(x0 + x1, WithinAbs(28, 0.01));
  CHECK_THAT(y0 + y1, WithinAbs(28, 0.01));
  for (size_t i = 1; i < page.prims.size(); i++)
    CHECK(page.prims[i - 1].depth >= page.prims[i].depth);   // back to front
}

TEST_CASE("an empty shape's picture is empty", "[ui][vectorexport]") {
  gridType_c gt{ gridType_c::GT_BRICKS };
  auto v = bttest::makeVoxel(gt, 2, 2, 2);
  CHECK(shapePicture(*v, Rgb{ 1, 0, 0 }, 28).prims.empty());
}
