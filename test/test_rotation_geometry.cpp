#include <catch2/catch_test_macros.hpp>

#include "lib/bt_assert.h"
#include "lib/gridtype.h"
#include "lib/rotationgeometry.h"
#include "lib/symmetries.h"
#include "lib/voxel.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <numbers>
#include <random>
#include <set>
#include <string>
#include <tuple>
#include <vector>

using namespace rotationGeometry;

namespace {

const double QUARTER = std::numbers::pi / 2;

/* The cells of a shape relative to its hotspot, which is how the
 * disassembler places a piece: world cell = position - hotspot + local. */
std::set<std::tuple<int, int, int>> cellsAroundHotspot(const voxel_c & v) {
  std::set<std::tuple<int, int, int>> out;
  for (unsigned int z = 0; z < v.getZ(); z++)
    for (unsigned int y = 0; y < v.getY(); y++)
      for (unsigned int x = 0; x < v.getX(); x++)
        if (v.isFilled(x, y, z))
          out.insert({ (int)x - v.getHx(), (int)y - v.getHy(), (int)z - v.getHz() });
  return out;
}

/* Independent overlap oracle for the tests: two squares of half side h
 * overlap iff a vertex of one lies strictly inside the other or two edges
 * cross properly. Deliberately a different algorithm than the separating
 * axis / signed distance code under test. */
struct quad_t {
  double p[4][2];
};

quad_t makeQuad(double cu, double cv, double theta, double h) {
  static const int sg[4][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
  quad_t q;
  double c = std::cos(theta), s = std::sin(theta);
  for (int i = 0; i < 4; i++) {
    q.p[i][0] = cu + h * (sg[i][0] * c - sg[i][1] * s);
    q.p[i][1] = cv + h * (sg[i][0] * s + sg[i][1] * c);
  }
  return q;
}

double cross(const double * o, const double * a, const double * b) {
  return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0]);
}

bool strictlyInside(const quad_t & q, const double * pt) {
  for (int i = 0; i < 4; i++)
    if (cross(q.p[i], q.p[(i + 1) & 3], pt) <= 1e-9)
      return false;
  return true;
}

bool quadsOverlap(const quad_t & a, const quad_t & b) {
  for (int i = 0; i < 4; i++)
    if (strictlyInside(a, b.p[i]) || strictlyInside(b, a.p[i]))
      return true;
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++) {
      const double * a0 = a.p[i], * a1 = a.p[(i + 1) & 3];
      const double * b0 = b.p[j], * b1 = b.p[(j + 1) & 3];
      double d1 = cross(a0, a1, b0), d2 = cross(a0, a1, b1);
      double d3 = cross(b0, b1, a0), d4 = cross(b0, b1, a1);
      if (((d1 > 1e-9 && d2 < -1e-9) || (d1 < -1e-9 && d2 > 1e-9)) &&
          ((d3 > 1e-9 && d4 < -1e-9) || (d3 < -1e-9 && d4 > 1e-9)))
        return true;
    }
  return false;
}

/* Dense-sampling oracle along a piecewise linear path. Cells
 * and pivot use the same in-plane frames as rotationGeometry. */
bool oracleCollides(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                    unsigned int axis, int phx, int phy, int phz, double play,
                    const std::vector<pose_t> & path, int stepsPerSegment) {
  auto uv = [axis](double x, double y, double z, double * u, double * v) {
    if (axis == 0)      { *u = y; *v = z; }
    else if (axis == 1) { *u = z; *v = x; }
    else                { *u = x; *v = y; }
  };
  auto layer = [axis](const cell_t & c) { return axis == 0 ? c.x : (axis == 1 ? c.y : c.z); };
  double pu, pv;
  uv(0.5 * phx, 0.5 * phy, 0.5 * phz, &pu, &pv);
  const double h = 0.5 * (1 - play);

  for (size_t seg = 0; seg + 1 < path.size(); seg++)
    for (int k = 0; k <= stepsPerSegment; k++) {
      double t = (double)k / stepsPerSegment;
      const pose_t & a = path[seg];
      const pose_t & b = path[seg + 1];
      double th = a.theta + t * (b.theta - a.theta);
      double du = a.du + t * (b.du - a.du);
      double dv = a.dv + t * (b.dv - a.dv);
      for (const cell_t & m : moving) {
        double mu, mv;
        uv(m.x, m.y, m.z, &mu, &mv);
        mu -= pu; mv -= pv;
        double ru = mu * std::cos(th) - mv * std::sin(th) + pu + du;
        double rv = mu * std::sin(th) + mv * std::cos(th) + pv + dv;
        quad_t mq = makeQuad(ru, rv, th, h);
        for (const cell_t & s : stat) {
          if (layer(s) != layer(m))
            continue;
          double su, sv;
          uv(s.x, s.y, s.z, &su, &sv);
          if (quadsOverlap(mq, makeQuad(su, sv, 0, h)))
            return true;
        }
      }
    }
  return false;
}

/* Random face-connected group of 2..5 cubes in a 4x4x2 box plus up to 6
 * random static cubes, as used by the randomised tests below. */
void randomScene(std::mt19937 & rng, std::vector<cell_t> & moving, std::vector<cell_t> & stat) {
  static const int d[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
  std::uniform_int_distribution<int> g(0, 3), gz(0, 1);
  std::set<std::tuple<int, int, int>> cells = { { g(rng), g(rng), gz(rng) } };
  size_t size = 2 + rng() % 4;
  while (cells.size() < size) {
    auto it = cells.begin();
    std::advance(it, rng() % cells.size());
    auto [x, y, z] = *it;
    int k = rng() % 6;
    int nx = x + d[k][0], ny = y + d[k][1], nz = z + d[k][2];
    if (nx >= 0 && ny >= 0 && nz >= 0 && nx < 4 && ny < 4 && nz < 2)
      cells.insert({ nx, ny, nz });
  }
  moving.clear();
  stat.clear();
  for (auto [x, y, z] : cells)
    moving.push_back({ x, y, z });
  int n = rng() % 7;
  for (int i = 0; i < n; i++) {
    cell_t c = { g(rng), g(rng), gz(rng) };
    if (!cells.count({ c.x, c.y, c.z }))
      stat.push_back(c);
  }
}

/* End cells of a quarter turn about a doubled pivot; false if a cube would
 * leave the lattice or land on a static cube. */
bool quarterTurnEnd(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                    unsigned int axis, unsigned int sense, int phx, int phy, int phz,
                    std::vector<cell_t> & end) {
  end.clear();
  for (const cell_t & c : moving) {
    int dx = 2 * c.x - phx, dy = 2 * c.y - phy, dz = 2 * c.z - phz;
    quarterTurn(&dx, &dy, &dz, axis, sense);
    int x = phx + dx, y = phy + dy, z = phz + dz;
    if ((x | y | z) & 1)
      return false;
    end.push_back({ x / 2, y / 2, z / 2 });
  }
  for (const cell_t & e : end)
    for (const cell_t & s : stat)
      if (e.x == s.x && e.y == s.y && e.z == s.z)
        return false;
  return true;
}

}

TEST_CASE("quarterTurnTrans rotates shapes like quarterTurn rotates vectors", "[rotations]") {

  /* asymmetric pentacube, so every orientation is distinct */
  gridType_c gt;
  std::unique_ptr<voxel_c> base(gt.getVoxel(3, 2, 2, voxel_c::VX_EMPTY));
  base->set(0, 0, 0, voxel_c::VX_FILLED);
  base->set(1, 0, 0, voxel_c::VX_FILLED);
  base->set(2, 0, 0, voxel_c::VX_FILLED);
  base->set(0, 1, 0, voxel_c::VX_FILLED);
  base->set(2, 1, 1, voxel_c::VX_FILLED);
  base->setHotspot(1, 0, 0);

  const symmetries_c * sym = gt.getSymmetries();

  for (unsigned int t = 0; t < sym->getNumTransformations(); t++)
    for (unsigned int axis = 0; axis < 3; axis++)
      for (unsigned int sense = 0; sense < 2; sense++) {
        INFO("orientation " << t << " axis " << axis << " sense " << sense);

        std::unique_ptr<voxel_c> start(gt.getVoxel(*base));
        REQUIRE(start->transform(t));

        unsigned char t2 = sym->transAdd((unsigned char)t, quarterTurnTrans(axis, sense));
        std::unique_ptr<voxel_c> end(gt.getVoxel(*base));
        REQUIRE(end->transform(t2));

        std::set<std::tuple<int, int, int>> expected;
        for (auto [x, y, z] : cellsAroundHotspot(*start)) {
          quarterTurn(&x, &y, &z, axis, sense);
          expected.insert({ x, y, z });
        }
        CHECK(cellsAroundHotspot(*end) == expected);
      }
}

TEST_CASE("a bar sweeps only the quarter it turns through", "[rotations]") {

  /* bar (0,0)-(1,0) turns about the centre of (0,0) around Z: +90° sweeps
   * the +x/+y quadrant, -90° the +x/-y quadrant */
  const std::vector<cell_t> bar = { { 0, 0, 0 }, { 1, 0, 0 } };
  const model_t model = { 0.081 };

  planarScene_c blockedAbove(bar, { { 1, 1, 0 } }, 2, 0, 0, 0, model);
  CHECK(blockedAbove.certifyQuarterTurn(0) == PATH_BLOCKED);
  CHECK(blockedAbove.certifyQuarterTurn(1) == PATH_CLEAR);

  planarScene_c diagonalBehind(bar, { { -1, -1, 0 } }, 2, 0, 0, 0, model);
  CHECK(diagonalBehind.certifyQuarterTurn(0) == PATH_CLEAR);
  CHECK(diagonalBehind.certifyQuarterTurn(1) == PATH_CLEAR);
}

TEST_CASE("a shimmy path clears what the pure rotation cannot", "[rotations]") {

  /* bar (0,0)-(1,0) turning +90° about the centre of (0,0), static cube at
   * (-1,0): the spinning pivot cube's corner digs into it. Sliding the bar
   * away from it by a quarter cube first, turning, and sliding back clears. */
  const std::vector<cell_t> bar = { { 0, 0, 0 }, { 1, 0, 0 } };
  planarScene_c scene(bar, { { -1, 0, 0 } }, 2, 0, 0, 0, { 0.081 });

  CHECK(scene.certifyQuarterTurn(0) == PATH_BLOCKED);
  CHECK(scene.certifyPath({ { 0, 0, 0 }, { 0, 0.25, 0 }, { QUARTER, 0.25, 0 }, { QUARTER, 0, 0 } }) == PATH_CLEAR);
}

TEST_CASE("the path checker agrees with a dense independent oracle", "[rotations]") {

  std::mt19937 rng(20261019);
  std::uniform_real_distribution<double> shift(-0.3, 0.3);
  const double play = 0.081;

  int clear = 0, blocked = 0, compared = 0;
  for (int it = 0; it < 1500; it++) {
    std::vector<cell_t> moving, stat;
    randomScene(rng, moving, stat);
    unsigned int axis = rng() % 3, sense = rng() % 2;
    int phx = (int)(rng() % 9) - 1, phy = (int)(rng() % 9) - 1, phz = (int)(rng() % 9) - 1;
    std::vector<cell_t> end;
    if (!quarterTurnEnd(moving, stat, axis, sense, phx, phy, phz, end))
      continue;

    double angle = (sense == 0) ? QUARTER : -QUARTER;
    std::vector<pose_t> path = { { 0, 0, 0 } };
    if (rng() % 2) {
      /* rotate with a detour through a random mid-turn translation */
      path.push_back({ angle / 2, shift(rng), shift(rng) });
    }
    path.push_back({ angle, 0, 0 });

    planarScene_c scene(moving, stat, axis, phx, phy, phz, { play });
    verdict_e v = scene.certifyPath(path);
    bool collides = oracleCollides(moving, stat, axis, phx, phy, phz, play, path, 3000);

    INFO("iteration " << it);
    /* sound: never clear when the oracle sees an overlap */
    if (collides)
      CHECK(v != PATH_CLEAR);
    /* blocked verdicts come from an actual overlapping sample */
    if (v == PATH_BLOCKED)
      CHECK(collides);

    compared++;
    clear += (v == PATH_CLEAR);
    blocked += (v == PATH_BLOCKED);
  }

  /* the random scenes must exercise both outcomes */
  CHECK(compared > 500);
  CHECK(clear > 100);
  CHECK(blocked > 100);
}

TEST_CASE("inversePose undoes a pose", "[rotations]") {

  /* a pose maps x to R(theta)(x - P) + P + d, P the pivot (origin here) */
  auto apply = [](const pose_t & p, double u, double v) {
    const double c = std::cos(p.theta), s = std::sin(p.theta);
    return std::pair{ c * u - s * v + p.du, s * u + c * v + p.dv };
  };
  std::mt19937 rng(7);
  std::uniform_real_distribution<double> r(-2, 2);
  for (int it = 0; it < 100; it++) {
    const pose_t p = { r(rng), r(rng), r(rng) };
    const double u = r(rng), v = r(rng);
    auto [mu, mv] = apply(p, u, v);
    auto [bu, bv] = apply(inversePose(p), mu, mv);
    CHECK(std::fabs(bu - u) < 1e-12);
    CHECK(std::fabs(bv - v) < 1e-12);
  }
}

TEST_CASE("a model without play is rejected", "[rotations]") {
  CHECK_THROWS_AS(planarScene_c({ { 0, 0, 0 } }, { { 1, 0, 0 } }, 2, 0, 0, 0, { 0 }), assert_exception);
  CHECK_THROWS_AS(pathTable_c({ 0 }, 2, { PATH_PURE, 0, 0, 0, 0 }), assert_exception);
}

TEST_CASE("rotation paths start at rest and end on the quarter-turned lattice pose", "[rotations]") {
  for (pathKind_e kind : { PATH_PURE, PATH_WIGGLE, PATH_SLIDE })
    for (unsigned int sense = 0; sense < 2; sense++) {
      path_t path = makePath({ kind, sense, -1, 1, 0.3 });
      CHECK(path.front().theta == 0);
      CHECK(path.front().du == 0);
      CHECK(path.front().dv == 0);
      CHECK(path.back().theta == ((sense == 0) ? QUARTER : -QUARTER));
      CHECK(path.back().du == 0);
      CHECK(path.back().dv == 0);
    }
  CHECK_THROWS_AS(makePath({ PATH_SLIDE, 0, 0, 0, 0.3 }), assert_exception);
  CHECK_THROWS_AS(makePath({ PATH_WIGGLE, 0, 1, 0, 0 }), assert_exception);
}

TEST_CASE("the rotation tables decide every path like the path checker", "[rotations]") {

  /* Pairs collide independently along a fixed path, so per-cube lookups must
   * reproduce the whole-scene verdict of every path exactly, including the
   * tables that are turned and mirrored copies rather than computed. */
  struct setup_t { model_t model; int scenes; };
  for (setup_t setup : { setup_t{ { 0.081 }, 300 }, setup_t{ { 0.02 }, 150 } }) {
    rotationTables_c tables(setup.model, 4, { 0.15, 0.3 });
    REQUIRE(tables.size() == 1 + 2 * 2 * 8);

    std::mt19937 rng(42);
    int pureBlocked = 0, rescued = 0;

    for (int it = 0; it < setup.scenes; it++) {
      std::vector<cell_t> moving, stat;
      randomScene(rng, moving, stat);
      unsigned int axis = rng() % 3, sense = rng() % 2;
      /* centre pivots have even, corner pivots odd doubled coordinates */
      int parity = rng() % 2;
      int phx = 2 * (int)(rng() % 5) - parity;
      int phy = 2 * (int)(rng() % 5) - parity;
      int phz = 2 * (int)(rng() % 5) - parity;

      planarScene_c scene(moving, stat, axis, phx, phy, phz, setup.model);
      int firstClear = -1;
      for (unsigned int i = 0; i < tables.size(); i++) {
        const pathTable_c & t = tables.table(sense, i);
        REQUIRE(t.getSpec().sense == sense);
        bool expected = scene.certifyPath(makePath(t.getSpec())) != PATH_CLEAR;
        INFO("iteration " << it << " path " << i << " play " << setup.model.play);
        CHECK(t.blocked(moving, stat, axis, phx, phy, phz) == expected);
        if (!expected && firstClear < 0)
          firstClear = (int)i;
      }
      CHECK(tables.firstClear(moving, stat, axis, phx, phy, phz, sense) == firstClear);

      pureBlocked += (firstClear != 0);
      rescued += (firstClear > 0);
    }

    /* the scenes must include moves only a shifted path makes possible */
    CHECK(pureBlocked > 30);
    CHECK(rescued > 5);
  }
}

TEST_CASE("pivotsBlockedAt is exactly the inverse of blockedOnAllPaths", "[rotations]") {

  const int radius = 3;
  rotationTables_c tables({ 0.081 }, radius, { 0.2 });

  for (unsigned int sense = 0; sense < 2; sense++) {
    /* every forward entry appears in the inverse ... */
    std::set<std::tuple<int, int, int, int>> forward;
    for (int du = -2 * radius; du <= 2 * radius; du++)
      for (int dv = -2 * radius; dv <= 2 * radius; dv++) {
        if ((du ^ dv) & 1)
          continue;
        for (auto [su, sv] : tables.blockedOnAllPaths(sense, du, dv)) {
          forward.insert({ du, dv, su, sv });
          const auto & inv = tables.pivotsBlockedAt(sense, su - du, sv - dv);
          INFO("sense " << sense << " moving " << du << "," << dv << " static " << su << "," << sv);
          CHECK(std::find(inv.begin(), inv.end(), std::pair<int, int>(du, dv)) != inv.end());
        }
      }
    /* ... and the inverse holds nothing else */
    for (int ddu = -8 * radius; ddu <= 8 * radius; ddu++)
      for (int ddv = -8 * radius; ddv <= 8 * radius; ddv++)
        for (auto [du, dv] : tables.pivotsBlockedAt(sense, ddu, ddv))
          CHECK(forward.count({ du, dv, du + ddu, dv + ddv }) == 1);
  }
}

TEST_CASE("turned and mirrored path tables equal directly computed ones", "[rotations]") {

  const model_t model = { 0.081 };
  rotationTables_c tables(model, 3, { 0.3 });

  for (unsigned int sense = 0; sense < 2; sense++)
    for (unsigned int i = 1; i < tables.size(); i += 3) {
      const pathTable_c & derived = tables.table(sense, i);
      pathTable_c direct(model, 3, derived.getSpec());
      INFO("sense " << sense << " path " << i);
      for (int du = -6; du <= 6; du++)
        for (int dv = -6; dv <= 6; dv++) {
          if ((du ^ dv) & 1)
            continue;
          std::set<std::pair<int, int>> a(derived.blockers(du, dv).begin(), derived.blockers(du, dv).end());
          std::set<std::pair<int, int>> b(direct.blockers(du, dv).begin(), direct.blockers(du, dv).end());
          CHECK(a == b);
        }
    }
}

TEST_CASE("path tables find blockers of far-out cubes", "[rotations]") {

  /* A cube at offset (r, r) swings out to r·√2 along an axis, so the
   * static cubes it can hit lie beyond the table's square of moving
   * offsets. Compare the outermost entries with an unpruned brute force. */
  const model_t model = { 0.081 };
  const int radius = 8;
  for (pathSpec_t spec : { pathSpec_t{ PATH_PURE, 0, 0, 0, 0 }, pathSpec_t{ PATH_SLIDE, 1, -1, 1, 0.3 } }) {
    pathTable_c table(model, radius, spec);
    const path_t path = makePath(spec);
    for (auto [du, dv] : { std::pair{ 16, 16 }, std::pair{ -15, 15 }, std::pair{ 16, -2 }, std::pair{ -16, -16 } }) {
      int parity = du & 1;
      const cell_t moving = { (du + parity) / 2, (dv + parity) / 2, 0 };
      std::set<std::pair<int, int>> expected;
      for (int sx = -16; sx <= 16; sx++)
        for (int sy = -16; sy <= 16; sy++) {
          int su = 2 * sx - parity, sv = 2 * sy - parity;
          if (su == du && sv == dv)
            continue;
          planarScene_c scene({ moving }, { { sx, sy, 0 } }, 2, parity, parity, 0, model);
          if (scene.certifyPath(path) != PATH_CLEAR)
            expected.insert({ su, sv });
        }
      INFO("offset " << du << "," << dv << " kind " << spec.kind);
      std::set<std::pair<int, int>> got(table.blockers(du, dv).begin(), table.blockers(du, dv).end());
      CHECK(got == expected);
    }
  }
}

TEST_CASE("the rotation tables rescue a turn by shifting away from the blocker", "[rotations]") {

  /* bar (0,0)-(1,0) turning +90° about the centre of (0,0) next to a static
   * cube at (-1,0): the pure turn digs in, a path shifting along +u clears */
  const std::vector<cell_t> bar = { { 0, 0, 0 }, { 1, 0, 0 } };
  const std::vector<cell_t> stat = { { -1, 0, 0 } };
  const model_t model = { 0.081 };
  rotationTables_c tables(model, 3, { 0.15, 0.3 });

  int idx = tables.firstClear(bar, stat, 2, 0, 0, 0, 0);
  REQUIRE(idx > 0);
  const pathSpec_t & spec = tables.table(0, idx).getSpec();
  CHECK(spec.dirU == 1);
  planarScene_c scene(bar, stat, 2, 0, 0, 0, model);
  CHECK(scene.certifyPath(makePath(spec)) == PATH_CLEAR);
}

TEST_CASE("invalid axes, senses and kinds throw, in release builds too", "[rotations]") {
  int x = 1, y = 0, z = 0;
  CHECK_THROWS_AS(quarterTurn(&x, &y, &z, 3, 0), assert_exception);
  CHECK_THROWS_AS(quarterTurn(&x, &y, &z, 0, 2), assert_exception);
  CHECK_THROWS_AS(quarterTurnTrans(3, 0), assert_exception);
  CHECK_THROWS_AS(quarterTurnTrans(0, 2), assert_exception);

  const model_t model = { 0.081 };
  CHECK_THROWS_AS(planarScene_c({ { 0, 0, 0 } }, { { 1, 0, 0 } }, 3, 0, 0, 0, model), assert_exception);
  CHECK_THROWS_AS(makePath({ PATH_PURE, 2, 0, 0, 0 }), assert_exception);
  CHECK_THROWS_AS(makePath({ (pathKind_e)3, 0, 1, 0, 0.1 }), assert_exception);
  CHECK_THROWS_AS(pathTable_c(model, 1, { PATH_SLIDE, 2, 1, 0, 0.1 }), assert_exception);

  rotationTables_c tables(model, 2, {});
  CHECK_THROWS_AS(tables.firstClear({ { 0, 0, 0 } }, { { 1, 0, 0 } }, 3, 0, 0, 0, 0), assert_exception);
  CHECK_THROWS_AS(tables.table(0, 0).blocked({ { 0, 0, 0 } }, { { 1, 0, 0 } }, 3, 0, 0, 0), assert_exception);
}

TEST_CASE("the tables need the whole moving group within their radius", "[rotations]") {

  /* the cube at x = 3 is on a layer without static cubes, so it cannot
   * collide, but the tables of radius 2 do not cover it: they throw rather
   * than guess */
  const std::vector<cell_t> moving = { { 0, 0, 0 }, { 1, 0, 0 }, { 3, 0, 1 } };
  const std::vector<cell_t> stat = { { 0, 2, 0 } };
  const model_t model = { 0.081 };
  CHECK(planarScene_c(moving, stat, 2, 0, 0, 0, model).certifyQuarterTurn(0) == PATH_CLEAR);
  CHECK_THROWS_AS(rotationTables_c(model, 2, {}).firstClear(moving, stat, 2, 0, 0, 0, 0), assert_exception);
  CHECK(rotationTables_c(model, 3, {}).firstClear(moving, stat, 2, 0, 0, 0, 0) == 0);
}

TEST_CASE("the rotation tables order their amounts ascending, once each", "[rotations]") {
  rotationTables_c tables({ 0.081 }, 1, { 0.4, 0.1, 0.4 });
  REQUIRE(tables.size() == 1 + 2 * 16);
  for (unsigned int i = 1; i < tables.size(); i++)
    CHECK(tables.table(0, i).getSpec().amount == ((i <= 16) ? 0.1 : 0.4));
}

TEST_CASE("a path in contact along a stretch is undecided at once", "[rotations]") {

  /* shift into contact with the row above, then slide along it: the
   * clearance is 0 all the way, which no subdivision can prove clear */
  planarScene_c scene({ { 0, 0, 0 } }, { { -1, 1, 0 }, { 0, 1, 0 }, { 1, 1, 0 } }, 2, 0, 0, 0, { 0.081 });
  const auto start = std::chrono::steady_clock::now();
  CHECK(scene.certifyPath({ { 0, 0, 0 }, { 0, 0, 0.081 }, { 0, 1, 0.081 } }) == PATH_UNDECIDED);
  CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(1));
}

/* Size and build cost of the rotation tables, and how many turns each kind
 * of path makes possible that the pure turn cannot: the numbers quoted in
 * design/2026-10-19-rotation-moves.md. Run with
 *   ./build/test_burrtools "[rotations-table-stats]" */
TEST_CASE("rotation table statistics", "[.][rotations-table-stats]") {

  const std::vector<double> amounts = { 0.1, 0.2, 0.3, 0.4 };

  for (int radius : { 6, 8 })
    for (model_t model : { model_t{ 0.081 }, model_t{ 0.02 }, model_t{ 0.01 } }) {
      auto start = std::chrono::steady_clock::now();
      rotationTables_c tables(model, radius, amounts);
      double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();

      size_t pure = 0, all = 0;
      for (unsigned int i = 0; i < tables.size(); i++)
        for (int du = -2 * radius; du <= 2 * radius; du++)
          for (int dv = -2 * radius; dv <= 2 * radius; dv++)
            if (!((du ^ dv) & 1)) {
              size_t n = tables.table(0, i).blockers(du, dv).size() + tables.table(1, i).blockers(du, dv).size();
              all += n;
              if (i == 0)
                pure += n;
            }

      WARN("radius " << radius << " play " << model.play << ": "
           << tables.size() << " paths per sense built in " << ms << " ms, " << pure
           << " blocking entries for the pure turns, " << all << " for all paths");

      if (radius != 6)
        continue;

      /* which first path rescues a turn the pure turn cannot make */
      std::mt19937 rng(1);
      int candidates = 0, pureClear = 0, rescued = 0;
      std::vector<int> byKindAmount(2 * amounts.size(), 0);
      for (int it = 0; it < 20000; it++) {
        std::vector<cell_t> moving, stat;
        randomScene(rng, moving, stat);
        unsigned int axis = rng() % 3, sense = rng() % 2;
        int parity = rng() % 2;
        int phx = 2 * (int)(rng() % 5) - parity;
        int phy = 2 * (int)(rng() % 5) - parity;
        int phz = 2 * (int)(rng() % 5) - parity;
        std::vector<cell_t> end;
        if (!quarterTurnEnd(moving, stat, axis, sense, phx, phy, phz, end))
          continue;
        candidates++;
        int idx = tables.firstClear(moving, stat, axis, phx, phy, phz, sense);
        if (idx == 0)
          pureClear++;
        if (idx > 0) {
          rescued++;
          byKindAmount[(idx - 1) / 8]++;
        }
      }
      std::string detail;
      for (size_t a = 0; a < amounts.size(); a++)
        detail += " amount " + std::to_string(amounts[a]).substr(0, 4) + ": wiggle " +
                  std::to_string(byKindAmount[2 * a]) + ", slide " + std::to_string(byKindAmount[2 * a + 1]) + ";";
      WARN("  " << candidates << " candidate turns with a free end pose, " << pureClear << " pure, "
           << rescued << " only by a shifted path, first clear path by kind:" << detail);
    }
}
