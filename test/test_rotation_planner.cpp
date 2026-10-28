#include <catch2/catch_test_macros.hpp>

#include "lib/disassembly.h"
#include "lib/rotationgeometry.h"
#include "lib/rotationtables.h"
#include "tools/xml.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
#include <sstream>
#include <vector>

/* The clearance-optimal turn planner (rotationGeometry::planTurn), the play
 * a turn needs (requiredPlay) and planned paths in saved disassemblies. See
 * design/2026-10-19-rotation-moves.md. */

using namespace rotationGeometry;

namespace {

const model_t DEFAULT_MODEL = { 0.081, 0 };

/* Coarser than the default grid, to keep the cases quick in the dev build;
 * the scenes below are chosen to be decided at this resolution too. */
planSettings_t quickSettings(void) {
  planSettings_t s;
  s.shift = 0.02;
  s.steps = 90;
  return s;
}

/* Step 21 of Andrew Crowell's PackTIC_3 solution file: piece d turns -90°
 * about X out of piece a. No path of the default family gets through at
 * play 0.081, but a motion with an asymmetric shift does; the planner found
 * it needs play 0.057 (and none below 0.041). */
struct crowellTurn_t {
  std::vector<cell_t> moving, stat;
  unsigned int axis, sense;
  int hx, hy, hz;
};

crowellTurn_t packTic3(void) {
  return {
    { {1,1,1}, {1,2,1}, {2,2,0}, {2,2,1}, {2,2,2}, {3,2,0} },
    { {0,0,0}, {0,0,3}, {0,1,0}, {0,1,3}, {0,2,0}, {0,2,1}, {0,2,3}, {0,3,0}, {0,3,3}, {1,0,0}, {1,0,3},
      {1,2,0}, {1,3,0}, {1,3,3}, {2,0,0}, {2,0,1}, {2,0,3}, {2,3,0}, {2,3,3}, {3,0,0}, {3,0,3}, {3,1,2},
      {3,1,3}, {3,2,2}, {3,2,3}, {3,3,0}, {3,3,1}, {3,3,2}, {3,3,3} },
    0, 1, 0, 4, 2
  };
}

/* Step 11 of MysTIC: pieces c and d turn together; at play 0.081 only a
 * motion with a few thousandths of a cell to spare gets through, which the
 * planner's fast mode must still find. */
crowellTurn_t mysTic(void) {
  return {
    { {1,0,2}, {1,0,3}, {1,1,2}, {1,1,3}, {1,2,2}, {1,3,1}, {1,3,2}, {2,0,0}, {2,0,2}, {2,0,3}, {2,1,2},
      {2,1,3}, {2,3,2}, {3,0,0}, {3,0,2}, {3,0,3}, {3,1,3}, {4,0,0}, {4,0,1}, {4,0,2}, {4,0,3}, {4,1,0},
      {4,1,3}, {4,2,0}, {4,2,3}, {4,3,0} },
    { {0,2,1}, {0,3,0}, {0,3,1}, {1,2,1}, {2,2,1}, {3,2,1}, {3,2,2}, {1,1,0}, {1,1,1}, {1,2,0}, {1,3,3},
      {1,4,0}, {1,4,3}, {2,1,1}, {2,2,0}, {2,3,0}, {2,3,3}, {2,4,0}, {2,4,1}, {2,4,3}, {3,1,1}, {3,3,3},
      {3,4,0}, {3,4,1}, {3,4,2}, {3,4,3}, {4,3,2}, {4,4,1}, {4,4,2}, {4,4,3} },
    2, 1, 2, 6, 0
  };
}

planarScene_c sceneOf(const crowellTurn_t & t, const model_t & model) {
  return planarScene_c(t.moving, t.stat, t.axis, t.hx, t.hy, t.hz, model);
}

/* the moving cubes turned a quarter about the pivot: the reverse turn's start */
std::vector<cell_t> turned(const crowellTurn_t & t) {
  std::vector<cell_t> r = t.moving;
  for (cell_t & c : r)
    turnAbout(&c.x, &c.y, &c.z, t.hx, t.hy, t.hz, t.axis, t.sense);
  return r;
}

}

TEST_CASE("the planner finds a turn the path family misses", "[rotations]") {

  const crowellTurn_t t = packTic3();
  const planarScene_c scene = sceneOf(t, DEFAULT_MODEL);

  auto tables = sharedRotationTables(rotationSettings_t{}, 5);
  CHECK(tables->firstClear(t.moving, t.stat, t.axis, t.hx, t.hy, t.hz, t.sense) < 0);

  SECTION("widest path") {
    const turnPlan_t plan = planTurn(scene, t.sense, quickSettings());
    REQUIRE(plan.found);
    CHECK(plan.clearance > 0);
    CHECK(plan.path.front().theta == 0);
    CHECK(plan.path.back().theta == -0.5 * std::numbers::pi);
    CHECK(scene.certifyPath(plan.path) == PATH_CLEAR);
    /* the certified value is a lower bound of the clearance at every sample */
    for (double s = 0; s <= 1; s += 0.01)
      CHECK(scene.clearance(poseAt(plan.path, s)) >= plan.clearance - 1e-9);
  }

  SECTION("fast mode, as the search uses it") {
    planSettings_t fast = quickSettings();
    fast.widest = false;
    fast.floor = 0;
    const turnPlan_t plan = planTurn(scene, t.sense, fast);
    REQUIRE(plan.found);
    CHECK(plan.clearance >= 0);
    CHECK(scene.certifyPath(plan.path) == PATH_CLEAR);
  }

  SECTION("the reverse turn, from the end, with the path run backwards") {
    const turnPlan_t plan = planTurn(scene, t.sense, quickSettings());
    REQUIRE(plan.found);
    const planarScene_c back(turned(t), t.stat, t.axis, t.hx, t.hy, t.hz, DEFAULT_MODEL);
    const path_t r = reversePath(plan.path);
    CHECK(r.front().theta == 0);
    CHECK(r.back().theta == 0.5 * std::numbers::pi);
    CHECK(back.certifyPath(r) == PATH_CLEAR);
    CHECK(std::fabs(back.minClearance(r) - plan.clearance) < 1e-3);
  }
}

TEST_CASE("the planner's fast mode finds a turn with almost no room", "[rotations]") {
  const crowellTurn_t t = mysTic();
  const planarScene_c scene = sceneOf(t, DEFAULT_MODEL);
  planSettings_t fast;   // the default grid: this one needs it
  fast.widest = false;
  fast.floor = 0;
  const turnPlan_t plan = planTurn(scene, t.sense, fast);
  REQUIRE(plan.found);
  CHECK(plan.clearance >= 0);
  CHECK(plan.clearance < 0.01);
  CHECK(scene.certifyPath(plan.path) == PATH_CLEAR);
}

TEST_CASE("an impossible turn is ruled out with a proof", "[rotations]") {
  /* a cube turning about a corner through a wall of static cubes */
  const std::vector<cell_t> moving = { { 0, 0, 0 } };
  const std::vector<cell_t> stat = { { 1, 0, 0 }, { 1, -1, 0 }, { 0, -1, 0 }, { -1, -1, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 }, { -1, 1, 0 } };
  /* pivot at the corner (0.5, 0.5): the cube would land on (0, 1) or (1, 0) */
  const planarScene_c scene(moving, stat, 2, 1, 1, 0, DEFAULT_MODEL);
  planSettings_t s = quickSettings();
  s.floor = 0;
  s.upper = true;
  const turnPlan_t plan = planTurn(scene, 0, s);
  CHECK_FALSE(plan.found);
  CHECK(plan.upper < 0);
}

TEST_CASE("required play stays below the play a bevel allows", "[rotations]") {
  /* the cube of the test above, walled in: no play lets it turn. With this
   * bevel the cubes vanish at play 0.2, below the default maxPlay */
  const std::vector<cell_t> moving = { { 0, 0, 0 } };
  const std::vector<cell_t> stat = { { 1, 0, 0 }, { 1, -1, 0 }, { 0, -1, 0 }, { -1, -1, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 }, { -1, 1, 0 } };
  playBound_t pb;
  CHECK_NOTHROW(pb = requiredPlay(moving, stat, 2, 1, 1, 0, 0, { 0.081, 0.4 }, quickSettings()));
  CHECK_FALSE(pb.found);
}

TEST_CASE("required play: proven possible above, impossible below", "[rotations]") {
  const crowellTurn_t t = packTic3();
  const playBound_t pb = requiredPlay(t.moving, t.stat, t.axis, t.hx, t.hy, t.hz, t.sense, DEFAULT_MODEL, quickSettings(), 0.002);
  REQUIRE(pb.found);
  /* the default grid gives 0.057; the coarse one a little more */
  CHECK(pb.needs > 0.045);
  CHECK(pb.needs < 0.075);
  CHECK(pb.atLeast > 0.02);
  CHECK(pb.atLeast <= pb.needs);
  CHECK(sceneOf(t, { pb.needs, 0 }).certifyPath(pb.path) == PATH_CLEAR);
}

TEST_CASE("more play adds at least as much clearance along a path", "[rotations]") {
  /* the conversion of clearance into play the planner relies on: shrinking
   * every cube by d/2 per face adds at least d to every clearance */
  std::mt19937 rng(4711);
  std::uniform_int_distribution<int> cellDist(-2, 2);
  std::uniform_real_distribution<double> shift(-0.3, 0.3);
  for (int scene = 0; scene < 30; scene++) {
    std::vector<cell_t> moving = { { 0, 0, 0 }, { 1, 0, 0 } }, stat;
    for (int i = 0; i < 6; i++) {
      cell_t c = { cellDist(rng), cellDist(rng), 0 };
      if (std::none_of(moving.begin(), moving.end(), [&](const cell_t & m) { return m.x == c.x && m.y == c.y; }))
        stat.push_back(c);
    }
    path_t path = { { 0, 0, 0 } };
    for (int k = 1; k < 4; k++)
      path.push_back({ k * std::numbers::pi / 8, shift(rng), shift(rng) });
    path.push_back({ std::numbers::pi / 2, 0, 0 });
    const double c1 = planarScene_c(moving, stat, 2, 1, 1, 0, { 0.05, 0 }).minClearance(path);
    const double c2 = planarScene_c(moving, stat, 2, 1, 1, 0, { 0.15, 0 }).minClearance(path);
    CHECK(c2 >= c1 + 0.1 - 2e-4);
  }
}

TEST_CASE("a planned rotation path survives saving and loading exactly", "[rotations]") {
  const crowellTurn_t t = packTic3();
  const turnPlan_t plan = planTurn(sceneOf(t, DEFAULT_MODEL), t.sense, quickSettings());
  REQUIRE(plan.found);

  state_c st(2);
  st.set(0, 0, 0, 0);
  st.set(1, 1, 2, 3);
  st.setTrans(0, 0);
  st.setTrans(1, 5);
  /* field by field: GCC 13 at -O3 takes the nested aggregate's vector for
   * uninitialized (-Wmaybe-uninitialized, an error with --werror) */
  stateRotation_t r;
  r.axis = t.axis;
  r.pivotU = 1;
  r.pivotV = 3;
  r.path.kind = PATH_PLANNED;
  r.path.sense = t.sense;
  r.path.waypoints = plan.path;
  r.pieces = { 1 };
  st.setRotation(r);

  std::ostringstream out;
  {
    xmlWriter_c xml(out);
    st.save(xml, 2);
  }
  std::istringstream in(out.str());
  xmlParser_c pars(in);
  pars.nextTag();
  state_c loaded(pars, 2);

  REQUIRE(loaded.getRotation() != nullptr);
  const pathSpec_t & p = loaded.getRotation()->path;
  CHECK(p.kind == PATH_PLANNED);
  CHECK(p.sense == t.sense);
  REQUIRE(p.waypoints.size() == plan.path.size());
  for (size_t i = 0; i < p.waypoints.size(); i++) {
    CHECK(p.waypoints[i].theta == plan.path[i].theta);
    CHECK(p.waypoints[i].du == plan.path[i].du);
    CHECK(p.waypoints[i].dv == plan.path[i].dv);
  }
  CHECK(makePath(p).size() == plan.path.size());
}

TEST_CASE("fixed rotation paths of every kind survive saving and loading", "[rotations]") {

  auto roundTrip = [](const pathSpec_t & spec) {
    state_c st(2);
    st.set(0, 0, 0, 0);
    st.set(1, 1, 2, 3);
    st.setRotation({ 2, 1, 3, spec, { 1 }, false });
    std::ostringstream out;
    {
      xmlWriter_c xml(out);
      st.save(xml, 2);
    }
    std::istringstream in(out.str());
    xmlParser_c pars(in);
    pars.nextTag();
    state_c loaded(pars, 2);
    REQUIRE(loaded.getRotation() != nullptr);
    return loaded.getRotation()->path;
  };

  for (const pathSpec_t & spec : { pathSpec_t{ PATH_PURE, 1, 0, 0 }, pathSpec_t{ PATH_WIGGLE, 0, 3, 0.25 },
                                   pathSpec_t{ PATH_SLIDE, 1, 14, 0.5 }, pathSpec_t{ PATH_RAMP, 0, 9, 0.55, {}, 45, 75 } }) {
    const pathSpec_t p = roundTrip(spec);
    INFO("kind " << spec.kind);
    CHECK(p.kind == spec.kind);
    CHECK(p.sense == spec.sense);
    if (spec.kind != PATH_PURE) {
      CHECK(p.dir == spec.dir);
      CHECK(p.amount == spec.amount);
      CHECK(p.holdFrom == spec.holdFrom);
      CHECK(p.holdTo == spec.holdTo);
    }
  }

  /* a ramp that does not ramp is rejected on loading, the same one with
   * its hold in order is not */
  auto load = [](int from, int to) {
    std::ostringstream xml;
    xml << "<state><dx>0 1</dx><dy>0 2</dy><dz>0 3</dz>"
        << "<rotation axis=\"z\" pu=\"1\" pv=\"3\" path=\"ramp\" angle=\"90\" pieces=\"1\" "
        << "dir=\"1\" amount=\"0.2\" from=\"" << from << "\" to=\"" << to << "\"/></state>";
    std::istringstream in(xml.str());
    xmlParser_c pars(in);
    pars.nextTag();
    state_c st(pars, 2);
  };
  CHECK_NOTHROW(load(30, 60));
  CHECK_THROWS(load(60, 30));
  CHECK_THROWS(load(0, 30));
}

TEST_CASE("a state with a truncated path or an impossible orientation is rejected on loading", "[rotations]") {
  auto load = [](const std::string & inner) {
    std::istringstream in("<state><dx>0 1</dx><dy>0 2</dy><dz>0 3</dz>" + inner + "</state>");
    xmlParser_c pars(in);
    pars.nextTag();
    state_c st(pars, 2);
  };
  const std::string planned = "<rotation axis=\"z\" pu=\"1\" pv=\"3\" path=\"planned\" angle=\"90\" pieces=\"1\" waypoints=\"";
  CHECK_NOTHROW(load(planned + "0 0 0 1.5707963267948966 0 0\"/>"));
  CHECK_THROWS(load(planned + "0 0 0 1.5707963267948966 0 0 0.3\"/>"));
  CHECK_THROWS(load(planned + "0 0 0 x 0 0\"/>"));

  CHECK_NOTHROW(load("<dt>0 5</dt>"));
  CHECK_THROWS(load("<dt>0 300</dt>"));
}
