/* Tests for btui::LayoutModel: collapse, focus and the column widths of
 * spec C11 / foundations/density.md.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "../src/uicore/layoutmodel.h"

using namespace btui;
using Catch::Matchers::WithinAbs;

TEST_CASE("column widths at the 1600 dp reference match density.md", "[ui][layout]") {
  LayoutModel m;

  // T-DEN-2 / C11 narrow-window table, Standard
  auto c = m.columns(1600, Density::Standard);
  CHECK_THAT(c.left, WithinAbs(320, 0.01));
  CHECK_THAT(c.centre, WithinAbs(864, 0.01));
  CHECK_THAT(c.right, WithinAbs(340, 0.01));

  m.setRightCollapsed(true);
  c = m.columns(1600, Density::Standard);
  CHECK(c.rightIsRail);
  CHECK_THAT(c.right, WithinAbs(48, 0.01));
  CHECK_THAT(c.centre, WithinAbs(1156, 0.01));

  m.setLeftCollapsed(true);
  c = m.columns(1600, Density::Standard);
  CHECK_THAT(c.centre, WithinAbs(1428, 0.01));   // the spec rounds this to "about 1410"

  // Minimal
  LayoutModel n;
  c = n.columns(1600, Density::Minimal);
  CHECK_THAT(c.left, WithinAbs(264, 0.01));
  CHECK_THAT(c.centre, WithinAbs(970, 0.01));
  CHECK_THAT(c.right, WithinAbs(320, 0.01));
  n.setRightCollapsed(true);
  CHECK_THAT(n.columns(1600, Density::Minimal).centre, WithinAbs(1250, 0.01));
}

TEST_CASE("focus modes override the collapse flags without changing them", "[ui][layout]") {
  LayoutModel m;
  m.setLeftCollapsed(true);

  m.toggleFocus3d();
  CHECK(m.focus() == Focus::Focus3d);
  CHECK(m.leftShownAsRail());
  CHECK(m.rightShownAsRail());
  CHECK(m.leftCollapsed());
  CHECK_FALSE(m.rightCollapsed());

  // Esc leaves focus and the earlier layout returns
  CHECK(m.escape());
  CHECK(m.focus() == Focus::None);
  CHECK(m.leftShownAsRail());
  CHECK_FALSE(m.rightShownAsRail());
  CHECK_FALSE(m.escape());
}

TEST_CASE("Focus 2D gives the voxel editor all remaining width", "[ui][layout]") {
  LayoutModel m;
  m.toggleFocus2d();
  REQUIRE(m.focus() == Focus::Focus2d);
  auto c = m.columns(1600, Density::Standard);
  CHECK(c.leftIsRail);
  CHECK_FALSE(c.rightIsRail);
  CHECK_THAT(c.centre, WithinAbs(360, 0.01));
  CHECK_THAT(c.right, WithinAbs(1600 - 76 - 48 - 360, 0.01));

  // entering Focus 3D replaces it
  m.toggleFocus3d();
  CHECK(m.focus() == Focus::Focus3d);
}

TEST_CASE("Focus 2D exists only in Entities", "[ui][layout]") {
  LayoutModel m;
  m.setWorkspace(Workspace::Puzzle);
  m.toggleFocus2d();
  CHECK(m.focus() == Focus::None);
  m.toggleFocus3d();
  CHECK(m.focus() == Focus::Focus3d);
}

TEST_CASE("collapse state is per workspace and switching resets focus", "[ui][layout]") {
  LayoutModel m;
  m.setRightCollapsed(true);
  m.toggleFocus3d();

  m.setWorkspace(Workspace::Solver);
  CHECK(m.focus() == Focus::None);
  CHECK_FALSE(m.rightCollapsed());
  m.setLeftCollapsed(true);

  m.setWorkspace(Workspace::Entities);
  CHECK(m.rightCollapsed());
  CHECK_FALSE(m.leftCollapsed());
  CHECK(m.leftCollapsed(Workspace::Solver));
}

TEST_CASE("the card keys toggle, and inside a focus mode they show the card", "[ui][layout]") {
  LayoutModel m;
  m.toggleRight();
  CHECK(m.rightCollapsed());
  m.toggleRight();
  CHECK_FALSE(m.rightCollapsed());

  m.setLeftCollapsed(false);
  m.toggleFocus3d();
  m.toggleLeft();
  CHECK(m.focus() == Focus::None);
  CHECK_FALSE(m.leftCollapsed());

  // expanding from a rail leaves focus too
  m.toggleFocus3d();
  m.setRightCollapsed(false);
  CHECK(m.focus() == Focus::None);
}

TEST_CASE("editorVisible follows the layout and the workspace", "[ui][layout]") {
  LayoutModel m;
  CHECK(m.editorVisible());

  m.setRightCollapsed(true);
  CHECK_FALSE(m.editorVisible());

  m.toggleFocus2d();
  CHECK(m.editorVisible());

  m.toggleFocus3d();
  CHECK_FALSE(m.editorVisible());

  m.escape();
  m.setRightCollapsed(false);
  m.setWorkspace(Workspace::Puzzle);
  CHECK_FALSE(m.editorVisible());
}

TEST_CASE("restored collapse flags apply to the right workspace", "[ui][layout]") {
  LayoutModel m;
  m.restoreCollapsed(Workspace::Puzzle, true, false);
  CHECK_FALSE(m.leftCollapsed());
  CHECK(m.leftCollapsed(Workspace::Puzzle));
}
