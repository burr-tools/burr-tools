/* Tests for btui's voxel editor rules: the tools and quick tools, Mirror and
 * Span, the planes, what a tool does to a voxel, the cell size and the grid
 * size (C02, C07, C08).
 */
#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"

#include "../src/uicore/voxeledit.h"

#include "../src/lib/puzzle.h"
#include "../src/lib/voxel.h"

#include <array>
#include <cmath>
#include <memory>
#include <set>

using namespace btui;

namespace {
  Modifiers mirrorX(void) { Modifiers m; m.mirror[0] = true; return m; }
}

TEST_CASE("Mirror X places the cell's mirror image too; the centre column only once", "[ui][voxeledit]") {
  // T-C07-2 (AC-C07-03)
  CHECK(expand({ 0, 1, 1 }, { 4, 4, 4 }, mirrorX()) == std::vector<Cell3>{ { 0, 1, 1 }, { 3, 1, 1 } });
  CHECK(expand({ 1, 0, 0 }, { 3, 1, 1 }, mirrorX()) == std::vector<Cell3>{ { 1, 0, 0 } });
}

TEST_CASE("Span fills the whole line along an axis; two axes a plane", "[ui][voxeledit]") {
  // T-C07-3 (AC-C07-04)
  Modifiers z;
  z.span[2] = true;
  CHECK(expand({ 2, 1, 0 }, { 4, 4, 4 }, z).size() == 4);
  Modifiers xz = z;
  xz.span[0] = true;
  const std::vector<Cell3> plane = expand({ 2, 1, 0 }, { 4, 4, 4 }, xz);
  CHECK(plane.size() == 16);
  for (const Cell3 & c : plane)
    CHECK(c.y == 1);
  Modifiers all;
  all.span = { true, true, true };
  CHECK(expand({ 0, 0, 0 }, { 2, 3, 4 }, all).size() == 24);
}

TEST_CASE("Span then Mirror: Span Y and Mirror X on a 4-cube reach 8 cells", "[ui][voxeledit]") {
  // T-C07-3 (AC-C07-05)
  Modifiers m = mirrorX();
  m.span[1] = true;
  const std::vector<Cell3> s = expand({ 0, 1, 1 }, { 4, 4, 4 }, m);
  CHECK(s.size() == 8);
  for (const Cell3 & c : s) {
    CHECK((c.x == 0 || c.x == 3));
    CHECK(c.z == 1);
  }
}

TEST_CASE("expand drops a target outside the grid and an empty grid reaches nothing", "[ui][voxeledit]") {
  CHECK(expand({ 4, 0, 0 }, { 4, 4, 4 }, Modifiers{}).empty());
  CHECK(expand({ -1, 0, 0 }, { 4, 4, 4 }, Modifiers{}).empty());
  CHECK(expand({ 0, 0, 0 }, { 0, 4, 4 }, mirrorX()).empty());
}

TEST_CASE("the quick tools: Ctrl erases, Alt is Variable, Shift is Fixed; Ctrl > Alt > Shift", "[ui][voxeledit]") {
  for (Platform p : { Platform::Windows, Platform::Linux }) {
    CHECK_FALSE(quickTool(p, {}).has_value());
    CHECK(quickTool(p, { .shift = true }) == Tool::Fixed);
    CHECK(quickTool(p, { .alt = true }) == Tool::Variable);
    CHECK(quickTool(p, { .control = true }) == Tool::Erase);
    CHECK(quickTool(p, { .shift = true, .alt = true }) == Tool::Variable);
    CHECK(quickTool(p, { .shift = true, .alt = true, .control = true }) == Tool::Erase);
    CHECK_FALSE(quickTool(p, { .meta = true }).has_value());
  }
}

TEST_CASE("on macOS Option or Command is Variable and Control (Qt's meta) erases", "[ui][voxeledit]") {
  const Platform mac = Platform::Mac;
  CHECK(quickTool(mac, { .alt = true }) == Tool::Variable);
  CHECK(quickTool(mac, { .control = true }) == Tool::Variable);   // Command
  CHECK(quickTool(mac, { .meta = true }) == Tool::Erase);         // Control
  CHECK(quickTool(mac, { .shift = true, .control = true, .meta = true }) == Tool::Erase);
}

TEST_CASE("the right button erases whatever the tool and the modifiers", "[ui][voxeledit]") {
  // AC-C08-RC
  for (Tool t : { Tool::Fixed, Tool::Variable, Tool::Erase, Tool::Paint }) {
    CHECK(toolInEffect(t, Platform::Windows, { .shift = true }, true) == Tool::Erase);
    CHECK(toolInEffect(t, Platform::Windows, { .alt = true }, true) == Tool::Erase);
    CHECK(toolInEffect(t, Platform::Windows, {}, false) == t);
  }
  CHECK(toolInEffect(Tool::Paint, Platform::Windows, { .shift = true }, false) == Tool::Fixed);
}

TEST_CASE("Fixed and Variable set state and colour, Erase empties, Paint recolours only voxels", "[ui][voxeledit]") {
  // T-C07-1 (AC-C07-02)
  gridType_c gt(gridType_c::GT_BRICKS);
  auto v = bttest::makeVoxel(gt, 3, 1, 1);
  CHECK(applyTool(*v, { { 0, 0, 0 } }, Tool::Fixed, 2));
  CHECK(v->getState(0, 0, 0) == voxel_c::VX_FILLED);
  CHECK(v->getColor(0, 0, 0) == 2);
  CHECK_FALSE(applyTool(*v, { { 0, 0, 0 } }, Tool::Fixed, 2));     // nothing to change

  CHECK(applyTool(*v, { { 0, 0, 0 } }, Tool::Paint, 5));
  CHECK(v->getState(0, 0, 0) == voxel_c::VX_FILLED);
  CHECK(v->getColor(0, 0, 0) == 5);
  CHECK_FALSE(applyTool(*v, { { 1, 0, 0 } }, Tool::Paint, 5));     // empty: no change
  CHECK(v->getState(1, 0, 0) == voxel_c::VX_EMPTY);

  CHECK(applyTool(*v, { { 1, 0, 0 } }, Tool::Variable, 0));
  CHECK(v->getState(1, 0, 0) == voxel_c::VX_VARIABLE);
  CHECK(applyTool(*v, { { 1, 0, 0 } }, Tool::Fixed, 0));          // Fixed over Variable
  CHECK(v->getState(1, 0, 0) == voxel_c::VX_FILLED);

  CHECK(applyTool(*v, { { 0, 0, 0 }, { 2, 0, 0 } }, Tool::Erase, 0));
  CHECK(v->getState(0, 0, 0) == voxel_c::VX_EMPTY);
  CHECK(v->getColor(0, 0, 0) == 0);
  CHECK_FALSE(applyTool(*v, { { 2, 0, 0 } }, Tool::Erase, 0));
}

TEST_CASE("applyTool skips cells outside the shape and keeps the bounding box right", "[ui][voxeledit]") {
  gridType_c gt(gridType_c::GT_BRICKS);
  auto v = bttest::makeVoxel(gt, 3, 3, 3);
  CHECK_FALSE(applyTool(*v, { { 3, 0, 0 }, { -1, 0, 0 }, { 0, 0, 9 } }, Tool::Fixed, 0));
  CHECK(applyTool(*v, { { 1, 2, 0 }, { 2, 1, 1 } }, Tool::Fixed, 99));   // a colour beyond the library's: Default
  CHECK(v->getColor(1, 2, 0) == 0);
  CHECK(v->boundX1() == 1);
  CHECK(v->boundX2() == 2);
  CHECK(v->boundY2() == 2);
  CHECK(v->boundZ2() == 1);
}

TEST_CASE("the preview names what each tool would do to each cell, and nothing it would not", "[ui][voxeledit]") {
  // C06's ghost table (AC-C06-10, AC-C08-10)
  gridType_c gt(gridType_c::GT_BRICKS);
  auto v = bttest::makeVoxel(gt, 4, 1, 1);
  v->setState(1, 0, 0, voxel_c::VX_FILLED);                          // colour 0
  v->setState(2, 0, 0, voxel_c::VX_VARIABLE);
  v->setColor(3, 0, 0, 0);
  const std::vector<Cell3> row{ { 0, 0, 0 }, { 1, 0, 0 }, { 2, 0, 0 }, { 3, 0, 0 }, { 9, 0, 0 } };
  using G = GhostCell;
  // Fixed: the empty cells placed, the variable voxel replaced, the same fixed one left
  CHECK(previewTool(*v, row, Tool::Fixed, 0) ==
        std::vector<G>{ { { 0, 0, 0 }, GhostKind::Place }, { { 2, 0, 0 }, GhostKind::Replace }, { { 3, 0, 0 }, GhostKind::Place } });
  // another colour replaces the fixed voxel too
  CHECK(previewTool(*v, row, Tool::Fixed, 2).size() == 4);
  CHECK(previewTool(*v, row, Tool::Variable, 0) ==
        std::vector<G>{ { { 0, 0, 0 }, GhostKind::Place }, { { 1, 0, 0 }, GhostKind::Replace }, { { 3, 0, 0 }, GhostKind::Place } });
  CHECK(previewTool(*v, row, Tool::Erase, 0) ==
        std::vector<G>{ { { 1, 0, 0 }, GhostKind::Erase }, { { 2, 0, 0 }, GhostKind::Erase } });
  // Paint: voxels that would take the colour; the empty cells and a voxel already that colour, none
  CHECK(previewTool(*v, row, Tool::Paint, 0).empty());
  CHECK(previewTool(*v, row, Tool::Paint, 3) ==
        std::vector<G>{ { { 1, 0, 0 }, GhostKind::Paint }, { { 2, 0, 0 }, GhostKind::Paint } });
  // what the preview names is what the tool changes
  for (Tool t : { Tool::Fixed, Tool::Variable, Tool::Erase, Tool::Paint }) {
    auto w = bttest::makeVoxel(gt, 4, 1, 1);
    w->setState(1, 0, 0, voxel_c::VX_FILLED);
    w->setState(2, 0, 0, voxel_c::VX_VARIABLE);
    const auto before = previewTool(*w, row, t, 3);
    CHECK(applyTool(*w, row, t, 3) == !before.empty());
    CHECK(previewTool(*w, row, t, 3).empty());          // done: nothing left to do
  }
}

namespace {
  // a ray straight down -Z through (x, y), from above the grid
  PickHit down(const voxel_c & v, float x, float y) {
    return pick(buildPickMesh(v), Vec3{ x, y, 10 }, Vec3{ 0, 0, -1 });
  }
}

TEST_CASE("a 3D press acts beyond the face for Fixed and Variable, on the voxel for Erase and Paint", "[ui][voxeledit]") {
  // C06 3D editing (AC-C06-06, T-C06-2)
  gridType_c gt(gridType_c::GT_BRICKS);
  auto v = bttest::makeVoxel(gt, 3, 3, 3);
  v->setState(1, 1, 1, voxel_c::VX_FILLED);
  const PickHit h = down(*v, 1.5f, 1.5f);
  REQUIRE(h.hit);
  const PickHit none{};
  Target3d t = target3d(*v, Tool::Fixed, h, none, false);
  CHECK(t.source == TargetSource::Face);
  CHECK(t.cell == Cell3{ 1, 1, 2 });                     // on top of it
  t = target3d(*v, Tool::Variable, h, none, true);
  CHECK(t.cell == Cell3{ 1, 1, 2 });
  for (Tool tool : { Tool::Erase, Tool::Paint }) {
    t = target3d(*v, tool, h, none, false);
    CHECK(t.source == TargetSource::Face);
    CHECK(t.cell == Cell3{ 1, 1, 1 });
  }
  // the top layer: beyond it is outside the grid -- rejected, the voxel hit kept for the outline
  v->setState(1, 1, 2, voxel_c::VX_FILLED);
  t = target3d(*v, Tool::Fixed, down(*v, 1.5f, 1.5f), none, false);
  CHECK(t.source == TargetSource::Rejected);
  CHECK(t.cell == Cell3{ 1, 1, 2 });
  // nothing under the pointer, the slab hidden: nothing, as the shape has voxels
  CHECK(target3d(*v, Tool::Fixed, down(*v, 0.5f, 0.5f), none, false).source == TargetSource::None);
}

TEST_CASE("the active layer's slab acts as the 2D grid where the pointer is on no voxel", "[ui][voxeledit]") {
  // product decisions 2026-10-27 and -28
  gridType_c gt(gridType_c::GT_BRICKS);
  auto v = bttest::makeVoxel(gt, 4, 3, 4);
  v->setState(2, 2, 0, voxel_c::VX_FILLED);                // below the active layer 2
  auto layer = layerCells(*v, Plane::XY, 2);
  CHECK(layer->countState(voxel_c::VX_FILLED) == 12);
  auto slabAt = [&](float x, float y) { return down(*layer, x, y); };

  // over an empty column: the slab's cell
  Target3d t = target3d(*v, Tool::Fixed, down(*v, 0.5f, 0.5f), slabAt(0.5f, 0.5f), true);
  CHECK(t.source == TargetSource::Slab);
  CHECK(t.cell == Cell3{ 0, 0, 2 });
  // on a voxel below the slab: its face decides, not the slab in front of it
  t = target3d(*v, Tool::Fixed, down(*v, 2.5f, 2.5f), slabAt(2.5f, 2.5f), true);
  CHECK(t.source == TargetSource::Face);
  CHECK(t.cell == Cell3{ 2, 2, 1 });
  // the slab hidden: the same
  t = target3d(*v, Tool::Fixed, down(*v, 2.5f, 2.5f), slabAt(2.5f, 2.5f), false);
  CHECK(t.source == TargetSource::Face);
  CHECK(t.cell == Cell3{ 2, 2, 1 });
  // a voxel in the slab: its face
  v->setState(0, 0, 2, voxel_c::VX_FILLED);
  t = target3d(*v, Tool::Fixed, down(*v, 0.5f, 0.5f), slabAt(0.5f, 0.5f), true);
  CHECK(t.source == TargetSource::Face);
  CHECK(t.cell == Cell3{ 0, 0, 3 });
  // a voxel above the slab, in front of it: its face
  v->setState(1, 0, 3, voxel_c::VX_FILLED);
  t = target3d(*v, Tool::Erase, down(*v, 1.5f, 0.5f), slabAt(1.5f, 0.5f), true);
  CHECK(t.source == TargetSource::Face);
  CHECK(t.cell == Cell3{ 1, 0, 3 });
}

TEST_CASE("an empty shape with the slab hidden places at the origin", "[ui][voxeledit]") {
  for (gridType_c::gridType type : bttest::ALL_GRIDS) {
    INFO(bttest::gridName(type));
    gridType_c gt(type);
    auto v = bttest::makeVoxel(gt, 4, 4, 4);
    const PickHit none{};
    const Target3d t = target3d(*v, Tool::Fixed, none, none, false);
    CHECK(t.source == TargetSource::Origin);
    CHECK(v->validCoordinate(t.cell.x, t.cell.y, t.cell.z));
    // and no cell of the type is nearer the origin
    const int sum = t.cell.x + t.cell.y + t.cell.z;
    for (int x = 0; x < 4; x++)
      for (int y = 0; y < 4; y++)
        for (int z = 0; z < 4; z++)
          if (x + y + z < sum)
            CHECK_FALSE(v->validCoordinate(x, y, z));
    // the slab shown: it is where the first voxel goes; off it, nothing
    CHECK(target3d(*v, Tool::Fixed, none, none, true).source == TargetSource::None);
  }
}

TEST_CASE("applyTool skips coordinates that are not a cell of the voxel type", "[ui][voxeledit]") {
  gridType_c gt(gridType_c::GT_SPHERES);
  auto v = bttest::makeVoxel(gt, 2, 2, 1);
  CHECK_FALSE(applyTool(*v, { { 1, 0, 0 } }, Tool::Fixed, 0));   // x+y+z odd
  CHECK(applyTool(*v, { { 1, 1, 0 } }, Tool::Fixed, 0));
}

TEST_CASE("planes map columns, rows and layers to X Y Z; a 4x3x5 shape has 5, 3 and 4 layers", "[ui][voxeledit]") {
  // T-C08-1 (AC-C08-01)
  CHECK(voxelAt(Plane::XY, 1, 2, 3) == Cell3{ 1, 2, 3 });
  CHECK(voxelAt(Plane::XZ, 1, 2, 3) == Cell3{ 1, 3, 2 });
  CHECK(voxelAt(Plane::YZ, 1, 2, 3) == Cell3{ 3, 2, 1 });
  gridType_c gt(gridType_c::GT_BRICKS);
  auto v = bttest::makeVoxel(gt, 4, 3, 5);
  CHECK(layerCount(*v, Plane::XY) == 5);
  CHECK(layerCount(*v, Plane::XZ) == 3);
  CHECK(layerCount(*v, Plane::YZ) == 4);
  for (Plane p : { Plane::XY, Plane::XZ, Plane::YZ })
    CHECK(planeAxes(p).layer == layerAxis(p));
}

TEST_CASE("every voxel type but the prism has the XZ and YZ planes", "[ui][voxeledit]") {
  for (gridType_c::gridType t : bttest::ALL_GRIDS) {
    INFO(bttest::gridName(t));
    CHECK(planeAvailable(t, Plane::XY));
    CHECK(planeAvailable(t, Plane::XZ) == (t != gridType_c::GT_TRIANGULAR_PRISM));
    CHECK(planeAvailable(t, Plane::YZ) == (t != gridType_c::GT_TRIANGULAR_PRISM));
  }
}

TEST_CASE("the grids with every plane look the same along every axis", "[ui][voxeledit]") {
  // why an XZ or YZ layer may be drawn as an XY one (planeAvailable): the
  // cells, and every corner of their faces, map onto each other when the
  // axes are swapped -- all six orders of X, Y, Z
  const int perms[6][3] = { { 0, 1, 2 }, { 0, 2, 1 }, { 1, 0, 2 }, { 1, 2, 0 }, { 2, 0, 1 }, { 2, 1, 0 } };
  for (gridType_c::gridType t : bttest::ALL_GRIDS) {
    if (!planeAvailable(t, Plane::YZ))
      continue;
    INFO(bttest::gridName(t));
    gridType_c gt(t);
    const int n = 10;                          // two of the rhombic grid's 5-unit blocks
    auto v = bttest::makeVoxel(gt, n, n, n);
    // a cell's corners in drawing space, rounded to 1/1000
    auto corners = [&](int x, int y, int z) {
      std::set<std::array<long, 3>> out;
      std::vector<float> fc;
      int nx, ny, nz;
      for (unsigned k = 0; v->getNeighbor(k, 0, x, y, z, &nx, &ny, &nz); k++) {
        fc.clear();
        v->getConnectionFace(x, y, z, int(k), 0, 0, fc);
        for (size_t i = 0; i + 2 < fc.size(); i += 3)
          out.insert({ std::lround(fc[i] * 1000), std::lround(fc[i + 1] * 1000), std::lround(fc[i + 2] * 1000) });
      }
      return out;
    };
    int checked = 0;
    for (const auto & p : perms)
      for (int x = 0; x < n; x++)
        for (int y = 0; y < n; y++)
          for (int z = 0; z < n; z++) {
            const int c[3] = { x, y, z };
            const int q[3] = { c[p[0]], c[p[1]], c[p[2]] };
            REQUIRE(v->validCoordinate(x, y, z) == v->validCoordinate(q[0], q[1], q[2]));
            if (t == gridType_c::GT_SPHERES || !v->validCoordinate(x, y, z) || x + y + z > 12)
              continue;
            std::set<std::array<long, 3>> mapped;
            for (const auto & k : corners(x, y, z))
              mapped.insert({ k[size_t(p[0])], k[size_t(p[1])], k[size_t(p[2])] });
            CHECK(mapped == corners(q[0], q[1], q[2]));
            checked++;
          }
    if (t != gridType_c::GT_SPHERES)
      CHECK(checked > 100);
  }
}

TEST_CASE("a layer's dot: the layer holds a voxel, in its plane", "[ui][voxeledit]") {
  // T-C08-3 (AC-C08-02)
  gridType_c gt(gridType_c::GT_BRICKS);
  auto v = bttest::makeVoxel(gt, 4, 3, 5);
  v->setState(1, 2, 3, voxel_c::VX_VARIABLE);
  CHECK(layerHasVoxels(*v, Plane::XY, 3));
  CHECK_FALSE(layerHasVoxels(*v, Plane::XY, 2));
  CHECK(layerHasVoxels(*v, Plane::XZ, 2));
  CHECK_FALSE(layerHasVoxels(*v, Plane::XZ, 0));
  CHECK(layerHasVoxels(*v, Plane::YZ, 1));
  CHECK_FALSE(layerHasVoxels(*v, Plane::YZ, 7));
  CHECK_FALSE(layerHasVoxels(*v, Plane::YZ, -1));
}

TEST_CASE("the cell size fits the area, clamped to 16 ... 120", "[ui][voxeledit]") {
  // T-C08-2 (AC-C08-03)
  CHECK(cellSize(360, 500, 4, 3) == 72);
  CHECK(cellSize(5000, 5000, 2, 2) == 120);
  CHECK(cellSize(100, 100, 40, 40) == 16);
  CHECK(cellSize(0, 0, 0, 0) == 16);
}

TEST_CASE("sizes count steps: 5 voxels rhombic, 3 tetra-octa, else 1", "[ui][voxeledit]") {
  for (gridType_c::gridType t : bttest::ALL_GRIDS) {
    INFO(bttest::gridName(t));
    gridType_c gt(t);
    const int step = sizeStep(t);
    CHECK(step == (t == gridType_c::GT_RHOMBIC ? 5 : t == gridType_c::GT_TETRA_OCTA ? 3 : 1));
    auto v = bttest::makeVoxel(gt, unsigned(2 * step), unsigned(step), unsigned(step));
    CHECK(gridSteps(*v, 0) == 2);
    CHECK(resizeShape(*v, 0, 4) == ResizeResult::Changed);
    CHECK(int(v->getX()) == 4 * step);
    CHECK(resizeShape(*v, 0, 4) == ResizeResult::Unchanged);
    CHECK(resizeShape(*v, 1, 0) == ResizeResult::Unchanged);   // clamped to one step
    CHECK(resizeShape(*v, 2, maxGridSteps + 5) == ResizeResult::Changed);
    CHECK(gridSteps(*v, 2) == maxGridSteps);
  }
}

TEST_CASE("a shrink that would cut voxels off is blocked and changes nothing", "[ui][voxeledit]") {
  // T-C02-1 (AC-C02-04)
  gridType_c gt(gridType_c::GT_BRICKS);
  auto v = bttest::makeVoxel(gt, 4, 3, 4);
  v->setState(2, 0, 0, voxel_c::VX_FILLED);
  v->setColor(2, 0, 0, 3);
  CHECK(resizeShape(*v, 0, 2) == ResizeResult::Blocked);
  CHECK(v->getX() == 4);
  CHECK(resizeShape(*v, 0, 3) == ResizeResult::Changed);
  CHECK(v->getX() == 3);
  CHECK(v->getState(2, 0, 0) == voxel_c::VX_FILLED);
  CHECK(v->getColor(2, 0, 0) == 3);
  CHECK(resizeShape(*v, 1, 1) == ResizeResult::Changed);    // nothing beyond y=0
  CHECK(resizeShape(*v, 0, 5) == ResizeResult::Changed);    // growing keeps the voxels
  CHECK(v->getState(2, 0, 0) == voxel_c::VX_FILLED);
}

TEST_CASE("apply to all changes every shape by the same steps, leaving blocked ones", "[ui][voxeledit]") {
  // AC-C02-03
  puzzle_c p(std::make_unique<gridType_c>(gridType_c::GT_BRICKS));
  p.addShape(2, 2, 2);
  p.addShape(5, 1, 1);
  p.addShape(3, 3, 3);
  p.getShape(1)->setState(4, 0, 0, voxel_c::VX_FILLED);

  ResizeAllResult r = resizeAllShapes(p, 0, 1);
  CHECK(r.changed == 3);
  CHECK(r.blocked == 0);
  CHECK(p.getShape(0)->getX() == 3);
  CHECK(p.getShape(1)->getX() == 6);

  r = resizeAllShapes(p, 0, -2);
  CHECK(r.changed == 2);
  CHECK(r.blocked == 1);
  CHECK(p.getShape(0)->getX() == 1);
  CHECK(p.getShape(1)->getX() == 6);
  CHECK(p.getShape(2)->getX() == 2);

  r = resizeAllShapes(p, 0, -5);             // clamped at one step
  CHECK(p.getShape(0)->getX() == 1);
  CHECK(p.getShape(2)->getX() == 1);
  CHECK(resizeAllShapes(p, 0, 0).changed == 0);
}
