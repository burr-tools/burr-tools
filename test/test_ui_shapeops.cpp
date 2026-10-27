/* Tests for btui's shape operations: the inspector's Transform (C04), Fit &
 * scale (C03) and Repair & surface (C05), per voxel type (C14), against
 * the spec's acceptance criteria and legacy's commands.
 */
#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"

#include "../src/uicore/shapeops.h"
#include "../src/uicore/voxeledit.h"

#include "../src/lib/puzzle.h"
#include "../src/lib/voxel.h"

#include <map>
#include <memory>
#include <set>
#include <string>

using namespace btui;

namespace {

  /* a puzzle of the type with one shape of the size */
  std::unique_ptr<puzzle_c> puzzleWith(gridType_c::gridType t, unsigned x, unsigned y, unsigned z) {
    auto p = std::make_unique<puzzle_c>(new gridType_c(t));
    p->addShape(x, y, z);
    return p;
  }

  int count(const voxel_c & v) {
    return int(v.countState(voxel_c::VX_FILLED) + v.countState(voxel_c::VX_VARIABLE));
  }

  const TransformButton & button(gridType_c::gridType t, std::string_view id) {
    for (const TransformButton & b : opsDescriptor(t).buttons)
      if (b.id == id)
        return b;
    FAIL("no button " << id);
    return opsDescriptor(t).buttons.front();
  }

  // an asymmetric piece: an L with a tail, so no rotation maps it onto itself
  void fillL(voxel_c & v) {
    v.setState(0, 0, 0, voxel_c::VX_FILLED);
    v.setState(1, 0, 0, voxel_c::VX_FILLED);
    v.setState(2, 0, 0, voxel_c::VX_VARIABLE);
    v.setState(0, 1, 0, voxel_c::VX_FILLED);
    v.setState(0, 1, 1, voxel_c::VX_FILLED);
    v.setColor(1, 0, 0, 2);
  }
}

TEST_CASE("each voxel type offers its legacy Transform buttons", "[ui][shapeops]") {
  // AC-C04-01, AC-C14-03: Brick 15; Spheres 3 flip + 6 rotate + 12 nudge;
  // Triangular Prism 3 flip + 4 rotate + 8 nudge; every task the type's own
  auto tally = [](gridType_c::gridType t) {
    std::map<OpKind, int> n;
    std::set<std::string_view> ids;
    std::set<int> tasks;
    for (const TransformButton & b : opsDescriptor(t).buttons) {
      n[b.kind]++;
      CHECK(ids.insert(b.id).second);
      CHECK(tasks.insert(b.task).second);
      CHECK_FALSE(b.tooltip.empty());
    }
    return n;
  };
  for (gridType_c::gridType t : { gridType_c::GT_BRICKS, gridType_c::GT_RHOMBIC, gridType_c::GT_TETRA_OCTA }) {
    auto n = tally(t);
    CHECK(n[OpKind::Flip] == 3);
    CHECK(n[OpKind::Rotate] == 6);
    CHECK(n[OpKind::Nudge] == 6);
    CHECK_FALSE(opsDescriptor(t).pads);
  }
  auto s = tally(gridType_c::GT_SPHERES);
  CHECK(s[OpKind::Flip] == 3);
  CHECK(s[OpKind::Rotate] == 6);
  CHECK(s[OpKind::Nudge] == 12);
  auto pr = tally(gridType_c::GT_TRIANGULAR_PRISM);
  CHECK(pr[OpKind::Flip] == 3);
  CHECK(pr[OpKind::Rotate] == 4);
  CHECK(pr[OpKind::Nudge] == 8);
  CHECK(opsDescriptor(gridType_c::GT_SPHERES).pads);
  CHECK(opsDescriptor(gridType_c::GT_TRIANGULAR_PRISM).pads);
  // AC-C14-04 and the library: spheres do not scale; only bricks minimize
  CHECK_FALSE(opsDescriptor(gridType_c::GT_SPHERES).scale);
  CHECK(opsDescriptor(gridType_c::GT_TRIANGULAR_PRISM).scale);
  CHECK(opsDescriptor(gridType_c::GT_BRICKS).minimize);
  CHECK_FALSE(opsDescriptor(gridType_c::GT_RHOMBIC).minimize);
}

TEST_CASE("flips and rotations undo themselves; rotations swap the grid's sides", "[ui][shapeops]") {
  // AC-C04-02, AC-C04-04
  gridType_c gt(gridType_c::GT_BRICKS);
  auto v = bttest::makeVoxel(gt, 4, 3, 5);
  fillL(*v);
  auto same = [&](const voxel_c & a) { return a.identicalInBB(v.get(), true) && count(a) == count(*v); };
  for (std::string_view flip : { "flip.x", "flip.y", "flip.z" }) {
    auto c = bttest::makeVoxel(gt, 4, 3, 5);
    fillL(*c);
    applyTask(*c, button(gridType_c::GT_BRICKS, flip).task);
    CHECK_FALSE(same(*c));
    applyTask(*c, button(gridType_c::GT_BRICKS, flip).task);
    CHECK(same(*c));
  }
  for (char a : { 'x', 'y', 'z' }) {
    const std::string pos = std::string("rotate.") + a + ".pos", neg = std::string("rotate.") + a + ".neg";
    auto c = bttest::makeVoxel(gt, 4, 3, 5);
    fillL(*c);
    applyTask(*c, button(gridType_c::GT_BRICKS, pos).task);
    applyTask(*c, button(gridType_c::GT_BRICKS, neg).task);
    CHECK(same(*c));
    for (int i = 0; i < 4; i++)
      applyTask(*c, button(gridType_c::GT_BRICKS, pos).task);
    CHECK(same(*c));
  }
  // about Y, X and Z swap; about X, Y and Z; about Z, X and Y
  auto dims = [&](std::string_view id) {
    auto c = bttest::makeVoxel(gt, 4, 3, 5);
    fillL(*c);
    applyTask(*c, button(gridType_c::GT_BRICKS, id).task);
    return std::array<unsigned, 3>{ c->getX(), c->getY(), c->getZ() };
  };
  CHECK(dims("rotate.y.pos") == std::array<unsigned, 3>{ 5, 3, 4 });
  CHECK(dims("rotate.x.pos") == std::array<unsigned, 3>{ 4, 5, 3 });
  CHECK(dims("rotate.z.pos") == std::array<unsigned, 3>{ 3, 4, 5 });
}

TEST_CASE("a nudge to the edge goes, one more is blocked and changes nothing", "[ui][shapeops]") {
  // AC-C04-03 (OQ-7: the spec blocks; legacy dropped the voxels)
  auto p = puzzleWith(gridType_c::GT_BRICKS, 4, 3, 3);
  p->getShape(0)->setState(1, 1, 1, voxel_c::VX_FILLED);
  const int right = button(gridType_c::GT_BRICKS, "nudge.x.pos").task;
  for (int i = 0; i < 2; i++) {
    REQUIRE(checkTask(*p, { 0 }, right).allowed);
    CHECK(runTask(*p, { 0 }, right).changed);
  }
  CHECK(p->getShape(0)->getState(3, 1, 1) == voxel_c::VX_FILLED);
  const OpCheck c = checkTask(*p, { 0 }, right);
  CHECK_FALSE(c.allowed);
  CHECK(c.reason == "Blocked: the piece would leave the grid");
  // an empty shape: allowed, nothing happens (C04)
  auto e = puzzleWith(gridType_c::GT_BRICKS, 2, 2, 2);
  CHECK(checkTask(*e, { 0 }, right).allowed);
  CHECK_FALSE(runTask(*e, { 0 }, right).changed);
}

TEST_CASE("prune, centre and to origin place the piece as legacy does", "[ui][shapeops]") {
  // AC-C03-02
  auto p = puzzleWith(gridType_c::GT_BRICKS, 7, 6, 5);
  voxel_c & v = *p->getShape(0);
  v.setState(4, 3, 2, voxel_c::VX_FILLED);
  v.setState(5, 3, 2, voxel_c::VX_FILLED);
  // to origin: the box's corner at 0, 0, 0; the grid as it was
  runTask(*p, { 0 }, task::Origin);
  CHECK(p->getShape(0)->boundX1() == 0);
  CHECK(p->getShape(0)->boundY1() == 0);
  CHECK(p->getShape(0)->boundZ1() == 0);
  CHECK(p->getShape(0)->getX() == 7);
  // centre: an even margin either side (the grid grows a cell where it is odd: legacy)
  runTask(*p, { 0 }, task::Center);
  const voxel_c & c = *p->getShape(0);
  CHECK(c.boundX1() == c.getX() - 1 - c.boundX2());
  CHECK(c.boundY1() == c.getY() - 1 - c.boundY2());
  CHECK(c.boundZ1() == c.getZ() - 1 - c.boundZ2());
  // prune: the grid is the piece's box
  runTask(*p, { 0 }, task::Prune);
  CHECK(p->getShape(0)->getX() == 2);
  CHECK(p->getShape(0)->getY() == 1);
  CHECK(p->getShape(0)->getZ() == 1);
  CHECK(count(*p->getShape(0)) == 2);
  // an empty shape: nothing to fit -- a 1x1x1 one too, whose bounding box does not say so
  for (unsigned n : { 3u, 1u }) {
    auto e = puzzleWith(gridType_c::GT_BRICKS, n, n, n);
    for (int t : { task::Prune, task::Center, task::Origin }) {
      const OpCheck ce = checkTask(*e, { 0 }, t);
      CHECK_FALSE(ce.allowed);
      CHECK(ce.reason == "The shape is empty");
    }
  }
}

TEST_CASE("x2 and x3 then Minimize give the shape back; Minimize says when it cannot", "[ui][shapeops]") {
  // AC-C03-03, -04, -05
  for (int scale : { task::Scale2, task::Scale3 }) {
    const int n = scale == task::Scale2 ? 2 : 3;
    auto p = puzzleWith(gridType_c::GT_BRICKS, 3, 2, 2);
    fillL(*p->getShape(0));
    const int before = count(*p->getShape(0));
    auto orig = std::unique_ptr<voxel_c>(p->getGridType()->getVoxel(*p->getShape(0)));
    REQUIRE(checkTask(*p, { 0 }, scale).allowed);
    runTask(*p, { 0 }, scale);
    CHECK(count(*p->getShape(0)) == before * n * n * n);
    CHECK(p->getShape(0)->getX() == 3u * unsigned(n));
    CHECK(minimizeFactor(*p, { 0 }) == n);
    REQUIRE(checkTask(*p, { 0 }, task::Minimize).allowed);
    const OpResult r = runTask(*p, { 0 }, task::Minimize);
    CHECK(r.message == "Resolution reduced ÷" + std::to_string(n));
    CHECK(p->getShape(0)->getX() == 3);
    CHECK(p->getShape(0)->identicalInBB(orig.get(), true));
  }
  // nothing to divide: disabled, and why
  auto p = puzzleWith(gridType_c::GT_BRICKS, 3, 2, 2);
  fillL(*p->getShape(0));
  const OpCheck m = checkTask(*p, { 0 }, task::Minimize);
  CHECK_FALSE(m.allowed);
  CHECK(m.reason == "Already at minimum resolution");
  // a factor two shapes share: x2 and x4 minimize together by 2 (Apply to all)
  p->addShape(1, 1, 1);
  p->getShape(1)->setState(0, 0, 0, voxel_c::VX_FILLED);
  runTask(*p, { 0 }, task::Scale2);
  runTask(*p, { 0 }, task::Scale2);
  runTask(*p, { 1 }, task::Scale2);
  CHECK(minimizeFactor(*p, { 0 }) == 4);
  CHECK(minimizeFactor(*p, { 0, 1 }) == 2);
  // the grid's limit (C02's 1000 steps)
  auto big = puzzleWith(gridType_c::GT_BRICKS, 400, 1, 1);
  const OpCheck b = checkTask(*big, { 0 }, task::Scale3);
  CHECK_FALSE(b.allowed);
  CHECK(b.reason == "Blocked: grid would exceed " + std::to_string(maxGridSteps));
  CHECK(checkTask(*big, { 0 }, task::Scale2).allowed);
  // spheres do not scale; rhombic shapes do not minimize
  auto sp = puzzleWith(gridType_c::GT_SPHERES, 2, 2, 2);
  CHECK(checkTask(*sp, { 0 }, task::Scale2).reason == "Not available for this voxel type");
  auto rh = puzzleWith(gridType_c::GT_RHOMBIC, 5, 5, 5);
  CHECK(checkTask(*rh, { 0 }, task::Minimize).reason == "Not available for this voxel type");
}

TEST_CASE("Fill interior holes fills the cavity and says how much", "[ui][shapeops]") {
  // AC-C05-01: a hollow 5x5x5 shell
  auto p = puzzleWith(gridType_c::GT_BRICKS, 5, 5, 5);
  voxel_c & v = *p->getShape(0);
  for (unsigned x = 0; x < 5; x++)
    for (unsigned y = 0; y < 5; y++)
      for (unsigned z = 0; z < 5; z++)
        if (x == 0 || y == 0 || z == 0 || x == 4 || y == 4 || z == 4)
          v.setState(x, y, z, voxel_c::VX_FILLED);
  OpResult r = runTask(*p, { 0 }, task::FillHoles);
  CHECK(count(*p->getShape(0)) == 125);
  CHECK(r.message == "Filled 27 interior voxels");
  r = runTask(*p, { 0 }, task::FillHoles);
  CHECK_FALSE(r.changed);
  CHECK(r.message == "No interior holes found");
}

TEST_CASE("the surface sets: outer touches the outside, cavity walls are inner", "[ui][shapeops]") {
  // AC-C05-02: a solid 3x3x3 cube, 26 outer and 1 inner
  auto p = puzzleWith(gridType_c::GT_BRICKS, 3, 3, 3);
  p->getShape(0)->setAll(voxel_c::VX_FILLED);
  auto outer = outerVoxels(*p->getShape(0));
  CHECK(std::count(outer.begin(), outer.end(), true) == 26);
  OpResult r = runTask(*p, { 0 }, task::VariableInner);
  CHECK(r.message == "1 inner voxel updated");
  CHECK(p->getShape(0)->getState(1, 1, 1) == voxel_c::VX_VARIABLE);
  CHECK(p->getShape(0)->countState(voxel_c::VX_VARIABLE) == 1);
  // AC-C05-03: Clear colour > Outer leaves the inner colour
  for (unsigned i = 0; i < p->getShape(0)->getXYZ(); i++)
    p->getShape(0)->setColor(i, 3);
  r = runTask(*p, { 0 }, task::ClearOuter);
  CHECK(r.message == "26 outer voxels updated");
  CHECK(p->getShape(0)->getColor(1, 1, 1) == 3);
  CHECK(p->getShape(0)->getColor(0, 0, 0) == 0);
  // AC-C05-04: around an enclosed cavity the walls are inner
  auto q = puzzleWith(gridType_c::GT_BRICKS, 5, 5, 5);
  q->getShape(0)->setAll(voxel_c::VX_FILLED);
  q->getShape(0)->setState(2, 2, 2, voxel_c::VX_EMPTY);
  outer = outerVoxels(*q->getShape(0));
  CHECK_FALSE(outer[unsigned(q->getShape(0)->getIndex(2, 2, 1))]);
  CHECK(std::count(outer.begin(), outer.end(), true) == 98);
}

TEST_CASE("every voxel type's buttons run, and preview what they will do", "[ui][shapeops]") {
  // legacy previewTransform: rotations an axis, flips a plane, nudges a direction
  for (gridType_c::gridType t : bttest::ALL_GRIDS) {
    INFO(bttest::gridName(t));
    gridType_c gt(t);
    const int big = t == gridType_c::GT_RHOMBIC ? 20 : t == gridType_c::GT_TETRA_OCTA ? 24 : 8;
    auto v = bttest::makeVoxel(gt, unsigned(big), unsigned(big), unsigned(big));
    // the cell of the type nearest the middle
    int best = -1, bx = 0, by = 0, bz = 0;
    for (int x = 0; x < big; x++)
      for (int y = 0; y < big; y++)
        for (int z = 0; z < big; z++)
          if (v->validCoordinate(x, y, z)) {
            const int d = std::abs(2 * x - big) + std::abs(2 * y - big) + std::abs(2 * z - big);
            if (best < 0 || d < best) {
              best = d;
              bx = x; by = y; bz = z;
            }
          }
    REQUIRE(best >= 0);
    v->setState(unsigned(bx), unsigned(by), unsigned(bz), voxel_c::VX_FILLED);
    for (const TransformButton & b : opsDescriptor(t).buttons) {
      INFO(b.id);
      const TaskPreview pv = previewTask(*v, b.task);
      CHECK_FALSE(pv.blocked);
      REQUIRE(pv.result);
      CHECK(count(*pv.result) == 1);
      switch (b.kind) {
        case OpKind::Flip: CHECK(pv.hint == TaskPreview::Hint::Mirror); break;
        case OpKind::Rotate: CHECK(pv.hint == TaskPreview::Hint::Rotate); break;
        case OpKind::Nudge: CHECK(pv.hint == TaskPreview::Hint::Nudge); break;
      }
      CHECK(std::abs(length(pv.axis) - 1.0f) < 1e-3f);
    }
  }
}
