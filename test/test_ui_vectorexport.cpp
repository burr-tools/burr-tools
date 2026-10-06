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
