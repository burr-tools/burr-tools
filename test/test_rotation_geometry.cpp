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

/* Dense-sampling oracle along a piecewise linear path, rounding = 0. Cells
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

TEST_CASE("a cube spinning next to a face neighbour needs rounded edges", "[rotations]") {

  /* Cube side a = 1 - play spins about its own centre, the neighbour's face
   * is at 1 - a/2. The rounded core reaches (a/2 - r)·√2 + r at 45°, so the
   * spin is possible iff r >= (a/√2 - (1 - a/2)) / (√2 - 1). */
  const std::vector<cell_t> moving = { { 0, 0, 0 } };
  const std::vector<cell_t> stat = { { 1, 0, 0 } };
  const double play = 0.081;
  const double a = 1 - play;
  const double rmin = (a / std::sqrt(2.0) - (1 - a / 2)) / (std::sqrt(2.0) - 1);

  for (unsigned int axis : { 1u, 2u })
    for (unsigned int sense = 0; sense < 2; sense++) {
      planarScene_c sharp(moving, stat, axis, 0, 0, 0, { play, 0 });
      CHECK(sharp.certifyQuarterTurn(sense) == PATH_BLOCKED);
      planarScene_c almost(moving, stat, axis, 0, 0, 0, { play, rmin - 0.01 });
      CHECK(almost.certifyQuarterTurn(sense) == PATH_BLOCKED);
      planarScene_c round(moving, stat, axis, 0, 0, 0, { play, rmin + 0.01 });
      CHECK(round.certifyQuarterTurn(sense) == PATH_CLEAR);
    }

  /* about the X axis the neighbour is on another layer and never in the way */
  planarScene_c alongAxis(moving, stat, 0, 0, 0, 0, { play, 0 });
  CHECK(alongAxis.certifyQuarterTurn(0) == PATH_CLEAR);
}

TEST_CASE("a bar sweeps only the quarter it turns through", "[rotations]") {

  /* bar (0,0)-(1,0) turns about the centre of (0,0) around Z: +90° sweeps
   * the +x/+y quadrant, -90° the +x/-y quadrant */
  const std::vector<cell_t> bar = { { 0, 0, 0 }, { 1, 0, 0 } };
  const model_t model = { 0.081, 0 };

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
  planarScene_c scene(bar, { { -1, 0, 0 } }, 2, 0, 0, 0, { 0.081, 0 });

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

    planarScene_c scene(moving, stat, axis, phx, phy, phz, { play, 0 });
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

TEST_CASE("a model without play is rejected", "[rotations]") {
  CHECK_THROWS_AS(planarScene_c({ { 0, 0, 0 } }, { { 1, 0, 0 } }, 2, 0, 0, 0, { 0, 0 }), assert_exception);
  CHECK_THROWS_AS(quarterTurnTable_c({ 0, 0 }, 2), assert_exception);
}

TEST_CASE("the quarter-turn table decides like the path checker", "[rotations]") {

  /* pairs collide independently along a fixed motion, so per-cube lookups
   * must reproduce the whole-scene verdict exactly */
  for (model_t model : { model_t{ 0.081, 0 }, model_t{ 0.02, 0.05 } }) {
    quarterTurnTable_c table(model, 4);
    std::mt19937 rng(42);
    int clear = 0, blocked = 0;

    for (int it = 0; it < 3000; it++) {
      std::vector<cell_t> moving, stat;
      randomScene(rng, moving, stat);
      unsigned int axis = rng() % 3, sense = rng() % 2;
      /* centre pivots have even, corner pivots odd doubled coordinates */
      int parity = rng() % 2;
      int phx = 2 * (int)(rng() % 5) - parity;
      int phy = 2 * (int)(rng() % 5) - parity;
      int phz = 2 * (int)(rng() % 5) - parity;

      planarScene_c scene(moving, stat, axis, phx, phy, phz, model);
      bool expected = scene.certifyQuarterTurn(sense) != PATH_CLEAR;

      INFO("iteration " << it << " play " << model.play << " rounding " << model.rounding);
      CHECK(table.blocked(moving, stat, axis, phx, phy, phz, sense) == expected);
      clear += !expected;
      blocked += expected;
    }

    CHECK(clear > 300);
    CHECK(blocked > 300);
  }
}

/* Size and build cost of the quarter-turn table for a few models, the
 * numbers quoted in design/2026-10-19-rotation-moves.md. Run with
 *   ./build/test_burrtools "[rotations-table-stats]" */
TEST_CASE("quarter-turn table statistics", "[.][rotations-table-stats]") {

  for (int radius : { 6, 10 })
    for (model_t model : { model_t{ 0.081, 0 }, model_t{ 0.02, 0.05 }, model_t{ 0.01, 0 } }) {
      auto start = std::chrono::steady_clock::now();
      quarterTurnTable_c table(model, radius);
      double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();

      size_t entries = 0, largest = 0;
      for (int du = -2 * radius; du <= 2 * radius; du++)
        for (int dv = -2 * radius; dv <= 2 * radius; dv++)
          if (!((du ^ dv) & 1)) {
            size_t n = table.blockers(du, dv, 0).size() + table.blockers(du, dv, 1).size();
            entries += n;
            largest = std::max(largest, table.blockers(du, dv, 0).size());
          }

      WARN("radius " << radius << " play " << model.play << " rounding " << model.rounding
           << ": built in " << ms << " ms, " << entries << " blocking entries, at most "
           << largest << " per moving cube and sense");
    }
}

TEST_CASE("quarter-turn table entries of both senses are mirror images", "[rotations]") {

  /* mirroring v maps a +90° turn onto a -90° turn, and squares onto squares */
  quarterTurnTable_c table({ 0.081, 0 }, 3);
  for (int du = -6; du <= 6; du++)
    for (int dv = -6; dv <= 6; dv++) {
      if ((du ^ dv) & 1)
        continue;
      const auto & minusEntry = table.blockers(du, dv, 1);
      std::set<std::pair<int, int>> minus(minusEntry.begin(), minusEntry.end());
      std::set<std::pair<int, int>> mirrored;
      for (auto [su, sv] : table.blockers(du, -dv, 0))
        mirrored.insert({ su, -sv });
      INFO("offset " << du << "," << dv);
      CHECK(minus == mirrored);
    }
}
