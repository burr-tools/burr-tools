#include <catch2/catch_test_macros.hpp>

#include "lib/disassembly.h"
#include "lib/rotationgeometry.h"
#include "lib/rotationtables.h"
#include "tools/xml.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <numbers>
#include <random>
#include <sstream>
#include <vector>

/* The turn planner (rotationGeometry::decideTurn), the motion with the most
 * room (widestTurn), the play a turn needs (requiredPlay) and planned paths
 * in saved disassemblies. See design/2026-10-19-rotation-moves.md. */

using namespace rotationGeometry;

namespace {

/* PR #50's sharp model, which the Crowell scenes below were measured at;
 * the default bevel would let the family clear some of them */
const model_t SHARP_MODEL = { 0.081, 0 };

rotationSettings_t sharpSettings(void) {
  rotationSettings_t s;
  s.model = SHARP_MODEL;
  return s;
}

/* Step 21 of Andrew Crowell's PackTIC_3 solution file: piece d turns -90°
 * about X out of piece a. No path of the default family gets through at
 * play 0.081 with sharp edges, but a motion with an asymmetric shift does:
 * the turn needs play 0.054, and is impossible below that. */
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
 * planner must still find. */
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

namespace {

/* decideTurn's path as the solver uses it: shortened, keeping half the margin */
path_t usablePath(const planarScene_c & scene, const turnDecision_t & d) {
  return shortcutPath(scene, d.path, 0.5 * decideSettings_t().margin);
}

}

TEST_CASE("a turn the path family misses is decided: possible with a motion, impossible with less play", "[rotations]") {
  const crowellTurn_t t = packTic3();
  const decideSettings_t s;

  for (const double play : { 0.081, 0.06 }) {
    const planarScene_c scene = sceneOf(t, { play, 0 });
    const turnDecision_t d = decideTurn(scene, t.sense, s);
    REQUIRE(d.answer == TURN_POSSIBLE);
    CHECK_FALSE(d.gaveUp);

    /* from the start to the quarter turn, within the angles and shifts searched */
    REQUIRE(d.path.size() >= 2);
    CHECK(d.path.front().theta == 0);
    CHECK(d.path.front().du == 0);
    CHECK(d.path.front().dv == 0);
    CHECK(d.path.back().theta == -std::numbers::pi / 2);
    CHECK(d.path.back().du == 0);
    CHECK(d.path.back().dv == 0);
    for (const pose_t & w : d.path) {
      CHECK(w.theta <= 0);
      CHECK(w.theta >= -std::numbers::pi / 2);
      CHECK(std::fabs(w.du) <= s.maxShift);
      CHECK(std::fabs(w.dv) <= s.maxShift);
    }
    CHECK(scene.certifyPath(d.path) == PATH_CLEAR);

    /* what the solver stores: a handful of waypoints, still certified */
    const path_t path = usablePath(scene, d);
    CHECK(path.size() <= 12);
    CHECK(scene.certifyPath(path) == PATH_CLEAR);
  }

  /* no motion at all, however it shifts: a proof for every play up to 0.05 */
  for (const double play : { 0.05, 0.03 }) {
    const turnDecision_t d = decideTurn(sceneOf(t, { play, 0 }), t.sense, s);
    CHECK(d.answer == TURN_IMPOSSIBLE);
    CHECK(d.path.empty());
    CHECK_FALSE(d.gaveUp);
  }
}

TEST_CASE("a turn and its reverse are decided alike", "[rotations]") {
  /* the poses of the reverse turn are the same poses seen from the other
   * end, so the answer may not depend on where the search starts */
  const crowellTurn_t t = packTic3();
  for (const double play : { 0.081, 0.045 }) {
    const turnDecision_t there = decideTurn(sceneOf(t, { play, 0 }), t.sense, decideSettings_t());
    const planarScene_c back(turned(t), t.stat, t.axis, t.hx, t.hy, t.hz, { play, 0 });
    const turnDecision_t again = decideTurn(back, t.sense ^ 1, decideSettings_t());
    CHECK(there.answer == again.answer);
    CHECK(there.answer == (play > 0.06 ? TURN_POSSIBLE : TURN_IMPOSSIBLE));
    if (again.answer == TURN_POSSIBLE) {
      CHECK(back.certifyPath(usablePath(back, again)) == PATH_CLEAR);
      /* and one motion serves both, as the solver's cache assumes */
      CHECK(back.certifyPath(reversePath(usablePath(sceneOf(t, { play, 0 }), there))) == PATH_CLEAR);
    }
  }
}

TEST_CASE("a turn with almost no room is decided", "[rotations]") {
  const crowellTurn_t t = mysTic();
  const planarScene_c scene = sceneOf(t, SHARP_MODEL);
  const turnDecision_t d = decideTurn(scene, t.sense, decideSettings_t());
  REQUIRE(d.answer == TURN_POSSIBLE);
  const path_t path = usablePath(scene, d);
  CHECK(scene.certifyPath(path) == PATH_CLEAR);
  CHECK(scene.minClearance(path) < 0.01);

  CHECK(decideTurn(sceneOf(t, { 0.06, 0 }), t.sense, decideSettings_t()).answer == TURN_IMPOSSIBLE);
}

TEST_CASE("a wide passage is found before the crevices are followed to their ends", "[rotations]") {
  /* GalacTIC at its printed play, four pieces turning off the fifth: there
   * is a motion with a few thousandths to spare. Cutting boxes down to the
   * resolution wherever they are nearly free finds a hairline motion after
   * 150 000 boxes; coarse boxes first find the wide one in a fraction. */
  const crowellTurn_t t = {
    { {1,0,1}, {1,1,1}, {2,1,1}, {2,2,1}, {2,2,2}, {3,2,2}, {1,3,0}, {0,3,1}, {1,3,1}, {0,4,1}, {1,4,1},
      {2,4,1}, {1,2,2}, {1,3,2}, {0,4,2}, {0,1,-1}, {3,1,-1}, {0,2,-1}, {3,2,-1}, {0,3,-1}, {3,3,-1}, {0,4,-1},
      {1,4,-1}, {2,4,-1}, {3,4,-1}, {0,1,0}, {3,1,0}, {3,4,0}, {0,1,1}, {3,1,1}, {3,4,1}, {0,1,2}, {2,1,2},
      {3,1,2}, {0,2,2}, {0,3,2}, {3,3,2}, {1,4,2}, {2,4,2}, {3,4,2} },
    { {1,1,-1}, {1,2,-1}, {0,2,0}, {1,2,0} },
    2, 0, -1, 1, 0
  };
  const planarScene_c scene = sceneOf(t, { 0.0108, 0.0653 });
  const turnDecision_t d = decideTurn(scene, t.sense, decideSettings_t());
  REQUIRE(d.answer == TURN_POSSIBLE);
  CHECK(d.boxes < 60000);       // 19 969
  const path_t path = usablePath(scene, d);
  CHECK(scene.certifyPath(path) == PATH_CLEAR);
  CHECK(scene.minClearance(path) > 0.0005);   // 0.0023
}

TEST_CASE("a turn through a wall is impossible at every play and with every bevel", "[rotations]") {
  const std::vector<cell_t> moving = { { 0, 0, 0 } };
  const std::vector<cell_t> stat = { { 1, 0, 0 }, { 1, -1, 0 }, { 0, -1, 0 }, { -1, -1, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 }, { -1, 1, 0 } };
  for (const model_t & model : { SHARP_MODEL, model_t{ 0.02, 0 }, model_t{ 0.0108, 0.0544 }, model_t{ 0.2, 0.1 } })
    for (unsigned int sense = 0; sense < 2; sense++) {
      const turnDecision_t d = decideTurn(planarScene_c(moving, stat, 2, 1, 1, 0, model), sense, decideSettings_t());
      CHECK(d.answer == TURN_IMPOSSIBLE);
      /* the cube lands on a static one: a few boxes are enough to see it */
      CHECK(d.boxes < 1000);
    }
}

TEST_CASE("a turn with nothing in the way is the plain quarter turn", "[rotations]") {
  const std::vector<cell_t> moving = { { 0, 0, 0 }, { 1, 0, 0 } };
  /* nothing at all, and a cube too far away to matter */
  for (const std::vector<cell_t> & stat : { std::vector<cell_t>{}, std::vector<cell_t>{ { 9, 9, 0 } } })
    for (unsigned int sense = 0; sense < 2; sense++) {
      const turnDecision_t d = decideTurn(planarScene_c(moving, stat, 2, 1, 1, 0, SHARP_MODEL), sense, decideSettings_t());
      REQUIRE(d.answer == TURN_POSSIBLE);
      REQUIRE(d.path.size() == 2);
      CHECK(d.path[1].theta == (sense == 0 ? 1 : -1) * std::numbers::pi / 2);
      CHECK(d.boxes == 1);
    }
}

TEST_CASE("an undecided turn says why", "[rotations]") {
  const crowellTurn_t t = packTic3();

  SECTION("told to stop") {
    std::atomic<bool> stop{ true };
    decideSettings_t s;
    s.stop = &stop;
    const turnDecision_t d = decideTurn(sceneOf(t, SHARP_MODEL), t.sense, s);
    CHECK(d.answer == TURN_UNDECIDED);
    CHECK(d.gaveUp);
    CHECK(d.path.empty());
  }
  SECTION("out of boxes") {
    decideSettings_t s;
    s.maxBoxes = 50;
    const turnDecision_t d = decideTurn(sceneOf(t, SHARP_MODEL), t.sense, s);
    CHECK(d.answer == TURN_UNDECIDED);
    CHECK(d.gaveUp);
  }
  SECTION("too close to call at the resolution") {
    /* play 0.055 is just enough (see above); boxes of 1/1024 cannot tell */
    decideSettings_t s;
    s.resolution = 1.0 / 1024;
    const turnDecision_t coarse = decideTurn(sceneOf(t, { 0.055, 0 }), t.sense, s);
    CHECK(coarse.answer == TURN_UNDECIDED);
    CHECK_FALSE(coarse.gaveUp);
    CHECK(decideTurn(sceneOf(t, { 0.055, 0 }), t.sense, decideSettings_t()).answer == TURN_POSSIBLE);
  }
}

TEST_CASE("decideTurn rejects settings it cannot search with", "[rotations]") {
  const crowellTurn_t t = packTic3();
  const planarScene_c scene = sceneOf(t, SHARP_MODEL);
  decideSettings_t s;
  CHECK_THROWS(decideTurn(scene, 2, s));
  s.maxShift = 0;
  CHECK_THROWS(decideTurn(scene, 0, s));
  s = decideSettings_t();
  s.resolution = 0;
  CHECK_THROWS(decideTurn(scene, 0, s));
  s = decideSettings_t();
  s.margin = -1e-3;
  CHECK_THROWS(decideTurn(scene, 0, s));
}

TEST_CASE("decideTurn never contradicts a certified path", "[rotations]") {
  /* Random small scenes: where a path of the fixed family is certified
   * clear, the turn may not be called impossible; where it is called
   * possible, its own path is certified; and with more play a possible
   * turn stays possible. */
  std::mt19937 rng(20261111);   // taken as it comes: the same scenes with every standard library
  std::vector<pathSpec_t> family = compassFamily({ 0.05, 0.1, 0.2, 0.3, 0.45 });
  const std::vector<pathSpec_t> wide = wideExtension({ 0.05, 0.1, 0.2, 0.3, 0.45 }, { 0.1, 0.3, 0.55 });
  family.insert(family.end(), wide.begin(), wide.end());

  int possible = 0, impossible = 0, familyClear = 0, beyondFamily = 0;
  for (int n = 0; n < 60; n++) {
    const std::vector<cell_t> moving = (n % 2) ? std::vector<cell_t>{ { 0, 0, 0 }, { 1, 0, 0 } }
                                               : std::vector<cell_t>{ { 0, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 } };
    std::vector<cell_t> stat;
    for (int i = 5 + (int)(rng() % 6); i > 0; i--) {
      const int x = (int)(rng() % 6) - 2, y = (int)(rng() % 6) - 2;
      const cell_t c = { x, y, 0 };
      if (std::none_of(moving.begin(), moving.end(), [&](const cell_t & m) { return m.x == c.x && m.y == c.y; }))
        stat.push_back(c);
    }
    const unsigned int sense = n % 4 < 2 ? 0 : 1;
    const model_t models[3] = { { 0.03, 0 }, { 0.081, 0 }, { 0.0108, 0.0544 } };
    turnAnswer_e last = TURN_IMPOSSIBLE;
    for (int m = 0; m < 3; m++) {
      const planarScene_c scene(moving, stat, 2, 1, 1, 0, models[m]);
      const turnDecision_t d = decideTurn(scene, sense, decideSettings_t());
      CHECK_FALSE(d.gaveUp);

      bool clear = false;
      for (pathSpec_t spec : family) {
        spec.sense = sense;
        if (scene.certifyPath(makePath(spec)) == PATH_CLEAR) {
          clear = true;
          break;
        }
      }
      if (clear) {
        familyClear++;
        CHECK(d.answer != TURN_IMPOSSIBLE);
      }
      if (d.answer == TURN_POSSIBLE) {
        possible++;
        if (!clear)
          beyondFamily++;
        CHECK(scene.certifyPath(d.path) == PATH_CLEAR);
        CHECK(scene.certifyPath(usablePath(scene, d)) == PATH_CLEAR);
      }
      if (d.answer == TURN_IMPOSSIBLE)
        impossible++;

      /* sharp cubes with more play fit inside those with less */
      if (m == 1 && last == TURN_POSSIBLE)
        CHECK(d.answer == TURN_POSSIBLE);
      last = d.answer;
    }
  }
  /* the scenes exercise both answers, and turns the family has no path
   * for; all 180 are decided (the counts as measured, with room for a
   * borderline scene falling the other way on another platform) */
  CHECK(possible >= 140);        // 147
  CHECK(impossible >= 30);       // 33
  CHECK(familyClear >= 120);     // 126
  CHECK(beyondFamily >= 15);     // 21
  CHECK(possible + impossible >= 178);
}

TEST_CASE("the widest motion of a turn the path family misses", "[rotations]") {

  const crowellTurn_t t = packTic3();
  const planarScene_c scene = sceneOf(t, SHARP_MODEL);

  auto tables = sharedRotationTables(sharpSettings(), 5);
  CHECK(tables->firstClear(t.moving, t.stat, t.axis, t.hx, t.hy, t.hz, t.sense) < 0);

  const turnPlan_t plan = widestTurn(scene, t.sense, decideSettings_t(), 0.002);
  REQUIRE(plan.found);
  CHECK_FALSE(plan.undecided);
  CHECK(plan.path.front().theta == 0);
  CHECK(plan.path.back().theta == -0.5 * std::numbers::pi);
  CHECK(scene.certifyPath(plan.path) == PATH_CLEAR);

  SECTION("its clearance is a lower bound at every sample") {
    for (double s = 0; s <= 1; s += 0.01)
      CHECK(scene.clearance(poseAt(plan.path, s)) >= plan.clearance - 1e-9);
  }

  SECTION("it has more room than the first motion found") {
    const turnDecision_t d = decideTurn(scene, t.sense, decideSettings_t());
    REQUIRE(d.answer == TURN_POSSIBLE);
    CHECK(plan.clearance > scene.minClearance(usablePath(scene, d)));
    /* play 0.081 where 0.054 is needed: about the difference to spare */
    CHECK(plan.clearance > 0.025);
    CHECK(plan.clearance < 0.04);
  }

  SECTION("the reverse turn, from the end, with the path run backwards") {
    const planarScene_c back(turned(t), t.stat, t.axis, t.hx, t.hy, t.hz, SHARP_MODEL);
    const path_t r = reversePath(plan.path);
    CHECK(r.front().theta == 0);
    CHECK(r.back().theta == 0.5 * std::numbers::pi);
    CHECK(back.certifyPath(r) == PATH_CLEAR);
    CHECK(std::fabs(back.minClearance(r) - plan.clearance) < 1e-3);
  }
}

TEST_CASE("the widest motion: none through a wall, undecided when stopped", "[rotations]") {
  SECTION("a turn through a wall") {
    const std::vector<cell_t> moving = { { 0, 0, 0 } };
    const std::vector<cell_t> stat = { { 1, 0, 0 }, { 1, -1, 0 }, { 0, -1, 0 }, { -1, -1, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 }, { -1, 1, 0 } };
    const turnPlan_t plan = widestTurn(planarScene_c(moving, stat, 2, 1, 1, 0, SHARP_MODEL), 0, decideSettings_t());
    CHECK_FALSE(plan.found);
    CHECK_FALSE(plan.undecided);
    CHECK(plan.path.empty());
  }
  SECTION("told to stop") {
    const crowellTurn_t t = mysTic();
    std::atomic<bool> stop{ true };
    decideSettings_t s;
    s.stop = &stop;
    const turnPlan_t plan = widestTurn(sceneOf(t, SHARP_MODEL), t.sense, s);
    CHECK_FALSE(plan.found);
    CHECK(plan.undecided);
  }
}

TEST_CASE("required play stays below the play a bevel allows", "[rotations]") {
  /* the cube of the test above, walled in: no play lets it turn. With this
   * bevel the cubes vanish at play 0.2, below the default maxPlay */
  const std::vector<cell_t> moving = { { 0, 0, 0 } };
  const std::vector<cell_t> stat = { { 1, 0, 0 }, { 1, -1, 0 }, { 0, -1, 0 }, { -1, -1, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 }, { -1, 1, 0 } };
  playBound_t pb;
  CHECK_NOTHROW(pb = requiredPlay(moving, stat, 2, 1, 1, 0, 0, { 0.081, 0.4 }, decideSettings_t()));
  CHECK_FALSE(pb.found);
}

TEST_CASE("required play: proven possible above, impossible below", "[rotations]") {
  const crowellTurn_t t = packTic3();
  const playBound_t pb = requiredPlay(t.moving, t.stat, t.axis, t.hx, t.hy, t.hz, t.sense, SHARP_MODEL, decideSettings_t(), 0.002);
  REQUIRE(pb.found);
  /* about 0.054 is needed; found to the tolerance, and ruled out just below */
  CHECK(pb.upper > 0.0535);
  CHECK(pb.upper < 0.056);
  CHECK(pb.lower < pb.upper);
  CHECK(pb.lower > pb.upper - 0.003);
  CHECK(sceneOf(t, { pb.upper, 0 }).certifyPath(pb.path) == PATH_CLEAR);
  /* at the bound the turn is possible, at the lower one it is not */
  CHECK(decideTurn(sceneOf(t, { pb.upper, 0 }), t.sense, decideSettings_t()).answer == TURN_POSSIBLE);
  CHECK(decideTurn(sceneOf(t, { pb.lower, 0 }), t.sense, decideSettings_t()).answer == TURN_IMPOSSIBLE);
}

TEST_CASE("required play of a turn that works at the model's play is at most that play", "[rotations]") {
  const crowellTurn_t t = packTic3();
  const turnPlan_t known = widestTurn(sceneOf(t, SHARP_MODEL), t.sense, decideSettings_t());
  REQUIRE(known.found);
  const playBound_t pb = requiredPlay(t.moving, t.stat, t.axis, t.hx, t.hy, t.hz, t.sense, SHARP_MODEL, decideSettings_t(), 0.002, 0.4,
                                      { { known.path, false } });
  REQUIRE(pb.found);
  CHECK(pb.upper <= SHARP_MODEL.play);
  CHECK(pb.upper < 0.056);
}

TEST_CASE("required bevel: proven possible above, impossible below, at a fixed play", "[rotations]") {
  /* packTic3's turn needs about 0.054 play without a bevel; with less
   * play a bevel can make up for it */
  const crowellTurn_t t = packTic3();
  const model_t model = { 0.04, 0 };
  REQUIRE(decideTurn(sceneOf(t, model), t.sense, decideSettings_t()).answer == TURN_IMPOSSIBLE);
  const playBound_t bb = requiredBevel(t.moving, t.stat, t.axis, t.hx, t.hy, t.hz, t.sense, model, decideSettings_t(), 0.002);
  REQUIRE(bb.found);
  CHECK(bb.upper > 0);
  CHECK(bb.lower < bb.upper);
  CHECK(bb.lower > bb.upper - 0.003);
  CHECK(sceneOf(t, { model.play, bb.upper }).certifyPath(bb.path) == PATH_CLEAR);
  CHECK(decideTurn(sceneOf(t, { model.play, bb.lower }), t.sense, decideSettings_t()).answer == TURN_IMPOSSIBLE);

  /* at a play that turns without a bevel it needs none */
  const playBound_t none = requiredBevel(t.moving, t.stat, t.axis, t.hx, t.hy, t.hz, t.sense, { 0.06, 0.03 },
                                         decideSettings_t(), 0.002);
  REQUIRE(none.found);
  CHECK(none.upper == 0);

  /* walled in: no bevel the play allows makes it */
  const std::vector<cell_t> moving = { { 0, 0, 0 } };
  const std::vector<cell_t> stat = { { 1, 0, 0 }, { 1, -1, 0 }, { 0, -1, 0 }, { -1, -1, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 }, { -1, 1, 0 } };
  CHECK_FALSE(requiredBevel(moving, stat, 2, 1, 1, 0, 0, { 0.01, 0 }, decideSettings_t(), 0.01).found);
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
  const turnPlan_t plan = widestTurn(sceneOf(t, SHARP_MODEL), t.sense, decideSettings_t(), 0.01);
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

TEST_CASE("fixed rotation paths of every kind are saved as their polyline", "[rotations]") {

  auto save = [](const pathSpec_t & spec) {
    state_c st(2);
    st.set(0, 0, 0, 0);
    st.set(1, 1, 2, 3);
    st.setRotation({ 2, 1, 3, spec, { 1 }, false });
    std::ostringstream out;
    {
      xmlWriter_c xml(out);
      st.save(xml, 2);
    }
    return out.str();
  };
  auto load = [](const std::string & text) {
    std::istringstream in(text);
    xmlParser_c pars(in);
    pars.nextTag();
    state_c loaded(pars, 2);
    REQUIRE(loaded.getRotation() != nullptr);
    return loaded.getRotation()->path;
  };

  for (const pathSpec_t & spec : { pathSpec_t{ PATH_PURE, 1, 0, 0 }, pathSpec_t{ PATH_WIGGLE, 0, 3, 0.25 },
                                   pathSpec_t{ PATH_SLIDE, 1, 14, 0.5 }, pathSpec_t{ PATH_RAMP, 0, 9, 0.55, {}, 45, 75 } }) {
    INFO("kind " << spec.kind);
    const std::string text = save(spec);
    /* the file does not name the family: it can change without the format */
    CHECK(text.find("path=") == std::string::npos);
    CHECK(text.find("amount=") == std::string::npos);
    const pathSpec_t p = load(text);
    CHECK(p.kind == PATH_PLANNED);
    CHECK(p.sense == spec.sense);
    /* the motion that is animated and certified is bit for bit the same */
    const path_t want = makePath(spec), got = makePath(p);
    REQUIRE(got.size() == want.size());
    for (size_t i = 0; i < got.size(); i++) {
      CHECK(got[i].theta == want[i].theta);
      CHECK(got[i].du == want[i].du);
      CHECK(got[i].dv == want[i].dv);
    }
    /* and saved again it is the same text */
    CHECK(save(p) == text);
  }
}

TEST_CASE("a state with a truncated path or an impossible orientation is rejected on loading", "[rotations]") {
  auto load = [](const std::string & inner) {
    std::istringstream in("<state><dx>0 1</dx><dy>0 2</dy><dz>0 3</dz>" + inner + "</state>");
    xmlParser_c pars(in);
    pars.nextTag();
    state_c st(pars, 2);
  };
  const std::string planned = "<rotation axis=\"z\" pu=\"1\" pv=\"3\" angle=\"90\" pieces=\"1\" waypoints=\"";
  CHECK_NOTHROW(load(planned + "0 0 0 1.5707963267948966 0 0\"/>"));
  CHECK_THROWS(load(planned + "0 0 0 1.5707963267948966 0 0 0.3\"/>"));
  CHECK_THROWS(load(planned + "0 0 0 x 0 0\"/>"));

  CHECK_NOTHROW(load("<dt>0 5</dt>"));
  CHECK_THROWS(load("<dt>0 300</dt>"));

  /* no path, a path that does not start at the identity or end at the
   * quarter turn, a shift or turn no search makes, a garbled piece list */
  CHECK_THROWS(load("<rotation axis=\"z\" pu=\"1\" pv=\"3\" angle=\"90\" pieces=\"1\"/>"));
  CHECK_THROWS(load(planned + "0 0 0.1 1.5707963267948966 0 0\"/>"));
  CHECK_THROWS(load(planned + "0 0 0 1.5707963267948966 0.1 0\"/>"));
  CHECK_THROWS(load(planned + "0 0 0 -1.5707963267948966 0 0\"/>"));
  CHECK_NOTHROW(load(planned + "0 0 0 0 0.2 0 1.5707963267948966 0.2 0 1.5707963267948966 0 0\"/>"));
  CHECK_THROWS(load(planned + "0 0 0 0 1e300 0 1.5707963267948966 0 0\"/>"));
  CHECK_THROWS(load(planned + "0 0 0 0 0 -3 1.5707963267948966 0 0\"/>"));
  CHECK_THROWS(load(planned + "0 0 0 7 0 0 1.5707963267948966 0 0\"/>"));
  CHECK_THROWS(load(planned + "0 0 0 0 nan 0 1.5707963267948966 0 0\"/>"));
  const std::string pieces = "<rotation axis=\"z\" pu=\"1\" pv=\"3\" angle=\"90\" waypoints=\"0 0 0 1.5707963267948966 0 0\" pieces=\"";
  CHECK_NOTHROW(load(pieces + "1\"/>"));
  CHECK_THROWS(load(pieces + "1 x\"/>"));

  /* the axis is a letter, the angle 90 or -90 degrees */
  const std::string pure = "<rotation pu=\"1\" pv=\"3\" pieces=\"1\" ";
  const std::string plus = " waypoints=\"0 0 0 1.5707963267948966 0 0\"/>";
  CHECK_NOTHROW(load(pure + "axis=\"x\" angle=\"-90\" waypoints=\"0 0 0 -1.5707963267948966 0 0\"/>"));
  CHECK_NOTHROW(load(pure + "axis=\"z\" angle=\"90\"" + plus));
  CHECK_THROWS(load(pure + "axis=\"2\" angle=\"90\"" + plus));
  CHECK_THROWS(load(pure + "axis=\"z\" angle=\"45\"" + plus));
}

namespace {

/* A bar of two cubes along x in layer z = 0, in a box: a closed ring of
 * static cubes around it in its own layer (room to turn), a floor below
 * and a lid above with a slot along y over the pivot cube. The bar leaves
 * upwards after a quarter turn about z, and no other way. */
struct boxedBar_t {
  std::vector<cell_t> moving = { { 0, 0, 0 }, { 1, 0, 0 } };
  std::vector<cell_t> stat;
  boxedBar_t(bool slot, bool crossSlot, bool ringGap) {
    for (int x = -3; x <= 4; x++)
      for (int y = -3; y <= 3; y++) {
        stat.push_back({ x, y, -1 });
        if (!(slot && x == 0 && (y == 0 || y == 1)))
          stat.push_back({ x, y, 1 });
        /* a second lid whose slot is along x: the bar fits through one or the other, not both */
        if (crossSlot && !(y == 0 && (x == 0 || x == 1)))
          stat.push_back({ x, y, 2 });
        if ((x == -3 || x == 4 || y == -3 || y == 3) && !(ringGap && x == 0 && y == 3))
          stat.push_back({ x, y, 0 });
      }
  }
};

removalGoal_t axialGoal(int sign) {
  removalGoal_t g;
  g.sign = sign;
  return g;
}

removalGoal_t planeGoal(double wu, double wv) {
  removalGoal_t g;
  g.axial = false;
  g.wu = wu;
  g.wv = wv;
  return g;
}

}

TEST_CASE("a group that leaves along the axis only after a turn", "[rotations]") {
  const model_t model = { 0.05, 0.02 };
  const decideSettings_t s;
  const boxedBar_t b(true, false, false);

  /* up: a quarter turn either way puts the bar under the slot */
  const removalGoal_t up = axialGoal(1);
  const turnDecision_t d = decideRemoval(b.moving, b.stat, 2, 0, 0, 0, model, up, s);
  REQUIRE(d.answer == TURN_POSSIBLE);
  CHECK(std::fabs(std::fabs(d.path.back().theta) - std::numbers::pi / 2) < 0.1);
  CHECK(certifyRemoval(b.moving, b.stat, 2, 0, 0, 0, model, up, d.path) == PATH_CLEAR);
  /* where the bar starts it cannot leave upwards */
  CHECK(certifyRemoval(b.moving, b.stat, 2, 0, 0, 0, model, up, { { 0, 0, 0 } }) == PATH_BLOCKED);

  /* down through the floor, sideways through the ring: no */
  CHECK(decideRemoval(b.moving, b.stat, 2, 0, 0, 0, model, axialGoal(-1), s).answer == TURN_IMPOSSIBLE);
  for (const auto & [u, v] : { std::pair{ 1.0, 0.0 }, { 0.0, 1.0 }, { -1.0, 0.0 }, { 0.0, -1.0 } })
    CHECK(decideRemoval(b.moving, b.stat, 2, 0, 0, 0, model, planeGoal(u, v), s).answer == TURN_IMPOSSIBLE);

  /* turning by at most 60 degrees does not reach the slot */
  removalGoal_t small = up;
  small.maxAngle = std::numbers::pi / 3;
  CHECK(decideRemoval(b.moving, b.stat, 2, 0, 0, 0, model, small, s).answer == TURN_IMPOSSIBLE);
}

TEST_CASE("a pose to leave from is clear of every layer on the way out", "[rotations]") {
  const model_t model = { 0.05, 0.02 };
  /* no slot, or a slot in the first lid and one across it in the second */
  for (const bool slot : { false, true }) {
    const boxedBar_t b(slot, slot, false);
    CHECK(decideRemoval(b.moving, b.stat, 2, 0, 0, 0, model, axialGoal(1), decideSettings_t()).answer == TURN_IMPOSSIBLE);
  }
}

TEST_CASE("a group that leaves in the plane only after a turn", "[rotations]") {
  const model_t model = { 0.05, 0.02 };
  const boxedBar_t b(false, false, true);
  const removalGoal_t out = planeGoal(0, 1);
  const turnDecision_t d = decideRemoval(b.moving, b.stat, 2, 0, 0, 0, model, out, decideSettings_t());
  REQUIRE(d.answer == TURN_POSSIBLE);
  CHECK(certifyRemoval(b.moving, b.stat, 2, 0, 0, 0, model, out, d.path) == PATH_CLEAR);
  CHECK(certifyRemoval(b.moving, b.stat, 2, 0, 0, 0, model, out, { { 0, 0, 0 } }) == PATH_BLOCKED);
  CHECK(decideRemoval(b.moving, b.stat, 2, 0, 0, 0, model, planeGoal(0, -1), decideSettings_t()).answer == TURN_IMPOSSIBLE);
  /* the gap is in the ring only: up and down stay closed */
  CHECK(decideRemoval(b.moving, b.stat, 2, 0, 0, 0, model, axialGoal(1), decideSettings_t()).answer == TURN_IMPOSSIBLE);
}

TEST_CASE("a group that is free leaves without moving", "[rotations]") {
  const std::vector<cell_t> moving = { { 0, 0, 0 } }, stat = { { 1, 0, 0 }, { 0, 0, 1 } };
  const turnDecision_t d = decideRemoval(moving, stat, 2, 0, 0, 0, SHARP_MODEL, axialGoal(-1), decideSettings_t());
  REQUIRE(d.answer == TURN_POSSIBLE);
  CHECK(certifyRemoval(moving, stat, 2, 0, 0, 0, SHARP_MODEL, axialGoal(-1), d.path) == PATH_CLEAR);
  CHECK(d.boxes < 100);
}

TEST_CASE("decideRemoval rejects goals it cannot search for", "[rotations]") {
  const boxedBar_t b(true, false, false);
  removalGoal_t g;
  g.sign = 0;
  CHECK_THROWS(decideRemoval(b.moving, b.stat, 2, 0, 0, 0, SHARP_MODEL, g, decideSettings_t()));
  CHECK_THROWS(decideRemoval(b.moving, b.stat, 2, 0, 0, 0, SHARP_MODEL, planeGoal(1, 1), decideSettings_t()));
  g = removalGoal_t();
  g.maxAngle = 0;
  CHECK_THROWS(decideRemoval(b.moving, b.stat, 2, 0, 0, 0, SHARP_MODEL, g, decideSettings_t()));
  CHECK_THROWS(certifyRemoval(b.moving, b.stat, 2, 0, 0, 0, SHARP_MODEL, axialGoal(1), {}));
}

TEST_CASE("decideRemoval never contradicts a certified removal", "[rotations]") {
  /* Random small scenes in a 4x4x3 block: a found removal is certified,
   * and one that needs no motion at all is not called impossible. */
  std::mt19937 rng(20261112);
  int possible = 0, impossible = 0;
  for (int scene = 0; scene < 40; scene++) {
    std::vector<cell_t> moving, stat;
    for (int x = 0; x < 4; x++)
      for (int y = 0; y < 4; y++)
        for (int z = 0; z < 3; z++) {
          const unsigned r = rng() % 8;
          if (r < 2)
            moving.push_back({ x, y, z });
          else if (r < 3)
            stat.push_back({ x, y, z });
        }
    if (moving.empty())
      continue;
    const model_t model = { 0.02 + 0.01 * (rng() % 8), 0.01 * (rng() % 6) };
    const unsigned int axis = rng() % 3;
    for (const removalGoal_t & g : { axialGoal(1), axialGoal(-1), planeGoal(1, 0), planeGoal(0, -1) }) {
      decideSettings_t s;
      s.maxBoxes = 100000;
      const turnDecision_t d = decideRemoval(moving, stat, axis, 3, 3, 3, model, g, s);
      if (d.answer == TURN_POSSIBLE) {
        possible++;
        CHECK(certifyRemoval(moving, stat, axis, 3, 3, 3, model, g, d.path) == PATH_CLEAR);
      }
      if (d.answer == TURN_IMPOSSIBLE) {
        impossible++;
        CHECK(certifyRemoval(moving, stat, axis, 3, 3, 3, model, g, { { 0, 0, 0 } }) != PATH_CLEAR);
      }
    }
  }
  /* both answers occur */
  INFO(possible << " possible, " << impossible << " impossible");
  CHECK(possible > 10);
  CHECK(impossible > 10);
}
