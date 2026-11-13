/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */
#include "rotationgeometry.h"

#include "bt_assert.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <cmath>
#include <numbers>
#include <queue>

/* decideTurn: a quarter turn decided in the space of poses. See
 * design/2026-10-19-rotation-moves.md, "The planner".
 *
 * A moving cube at m (relative to the pivot) in pose (θ, d) overlaps the
 * static cube at s exactly when the shift d lies in the convex polygon
 *
 *   s - R(θ) m + K(θ),   K(θ) = O (+) R(θ) O,
 *
 * O being the cube's outline (a square, or an octagon with bevels) and (+)
 * the Minkowski sum. The edge normals of K(θ) are those of O and those of O
 * turned by θ, and along a normal n its support is h_O(n) + h_O(R(-θ) n):
 * d is inside when |n · (d - centre)| < h for every one of them, and at
 * least q outside when one of them exceeds h by q.
 *
 * A box of poses, angles within δ of θ and shifts within (a_u, a_v) of d,
 * is judged at its centre: turning by up to δ moves no point of the moving
 * cube by more than ε = δ (|m| + cube circumradius), which is how far the
 * obstacle of that pair can move or change; the shift part of the box
 * reaches a_u |n_u| + a_v |n_v| along n. So the box is clear of the pair
 * when some normal separates by more than the reach plus ε, and inside the
 * obstacle when every normal is inside by more than that.
 */

namespace rotationGeometry {

namespace {

/* Boxes alive in all planner runs at once: concurrent runs (the workers of
 * a Slow solve) share a budget, so their memory stays bounded however many
 * there are. A run that needs more than is left waits for the others to
 * finish rather than giving up, so that what a run finds never depends on
 * what else runs: only maxBoxes and stop end it early. Runs take the budget
 * in chunks of a tenth of it, so the lock is taken a few times per run,
 * not per box; a run that never needs a chunk never takes it. */
constexpr size_t CHUNKS = 10;
struct boxBudget_t {
  std::mutex m;
  std::condition_variable freed;
  size_t budget = 2000000;
  size_t live = 0;             ///< held by the runs
  unsigned int runs = 0;       ///< runs holding a share
  unsigned int waiting = 0;    ///< of them, waiting for room
};
boxBudget_t & boxBudget(void) {
  static boxBudget_t b;
  return b;
}

/* One run's share of the budget. */
class budgetShare_c {
  size_t held = 0;
  bool counted = false;       ///< among the runs, from the first hold on
public:
  budgetShare_c(void) = default;
  ~budgetShare_c(void) {
    if (!counted)
      return;
    boxBudget_t & b = boxBudget();
    {
      std::lock_guard<std::mutex> lock(b.m);
      b.live -= held;
      b.runs--;
    }
    b.freed.notify_all();
  }
  budgetShare_c(const budgetShare_c &) = delete;
  budgetShare_c & operator=(const budgetShare_c &) = delete;

  /* Room for n boxes. Waits while other runs hold the budget, except when
   * every other run waits as well: then this one goes on past the budget
   * (by at most one run's maxBoxes and a chunk), as otherwise none would
   * ever go on.
   * False only when told to stop while waiting. */
  bool hold(size_t n, const std::atomic<bool> * stop) {
    if (n <= held)
      return true;
    boxBudget_t & b = boxBudget();
    std::unique_lock<std::mutex> lock(b.m);
    if (!counted) {
      b.runs++;
      counted = true;
    }
    const size_t chunk = std::max<size_t>(b.budget / CHUNKS, 1);
    const size_t want = (n - held + chunk - 1) / chunk * chunk;
    while (b.live + want > b.budget && b.waiting + 1 < b.runs) {
      if (stop && stop->load(std::memory_order_relaxed))
        return false;
      b.waiting++;
      /* timed: a stop is a flag, not a notification */
      b.freed.wait_for(lock, std::chrono::milliseconds(20));
      b.waiting--;
    }
    b.live += want;
    held += want;
    return true;
  }
};

class poseBoxes_c {

  enum state_e : uint8_t {
    FREE,     ///< every pose in the box keeps the margin
    MIXED,    ///< not known
    BLOCKED,  ///< every pose in the box collides (or has less room than `wall`)
    INNER     ///< split: no longer a leaf
  };

  struct pair_t {
    double mu, mv, su, sv;
    double lever;   ///< how far a point of the moving cube is from the pivot at most
  };

  /* removal: whether the poses of a box are goal poses, those from which
   * the group leaves (decideRemoval) */
  enum goal_e : uint8_t {
    GOAL_SOME,   ///< not known
    GOAL_ALL,    ///< every pose in the box is one, with the margin
    GOAL_NONE    ///< none is
  };

  struct box_t {
    double t0, t1, u0, u1, v0, v1;   ///< angle (as an amount turned, times sigma) and shift
    state_e state = MIXED;
    goal_e goal = GOAL_NONE;
    bool smallest = false;           ///< MIXED and at the resolution: not split
    double deficit = 0;              ///< MIXED: how far it is from FREE, see cost()
    double goalDeficit = 0;          ///< GOAL_SOME: how far it is from GOAL_ALL
    int child = -1, children = 0;
    std::vector<uint32_t> pairs;     ///< MIXED: the pairs not known to be clear of the box
    std::vector<uint32_t> goalPairs; ///< GOAL_SOME: the goal obstacles not known to be clear of it
    std::vector<int> adj;            ///< leaves sharing a face (and stale ones, split since)
  };

  const decideSettings_t & settings;
  /* Room below which a pose counts as blocked. 0 decides the turn: blocked
   * is colliding. The margin itself asks for a motion that keeps the
   * margin, and rules that out where there is none (widestTurn). */
  const double wall;
  const double sigma;        ///< +1: the turn is +90°, -1: -90°
  const double quarter = 0.5 * std::numbers::pi;
  /* removal (nullptr: the quarter turn): the angles are those between
   * -tMax and tMax, the goal is a set of poses, each judged against
   * goalObstacles: for a slide along the axis the static cubes of the layers
   * ahead, for one in the plane the same layer's obstacles swept backwards
   * along the slide, see classifyGoal */
  const removalGoal_t * removal = nullptr;
  double tMin = 0, tMax = quarter;
  std::vector<pair_t> goalObstacles;
  double lever = 0;          ///< largest lever of a pair: an angle of a is an arc of a·lever
  /* The size boxes are cut down to at present. It starts coarse and is
   * halved, down to the resolution, whenever every chain needs a box of
   * that size: a wide passage is then found among few boxes, where cutting
   * to the resolution at once follows every crevice to its end first. */
  double level;
  std::vector<pair_t> allPairs;
  /* before the boxes: given back once they are freed */
  budgetShare_c share;       ///< room for the boxes, see boxBudget_t
  std::vector<box_t> boxes;

  /* the cube's outline and its support along its own edge normals */
  int corners = 0, normals = 0;
  double outline[8][2];
  double base[4][2];
  double hBase[4];

  double support(double nx, double ny) const {
    double b = outline[0][0] * nx + outline[0][1] * ny;
    for (int i = 1; i < corners; i++)
      b = std::max(b, outline[i][0] * nx + outline[i][1] * ny);
    return b;
  }

  /* FREE, MIXED or BLOCKED for a box, looking at the pairs `from` (those
   * its parent could not rule out); leaves the undecided ones in b.pairs */
  void classify(box_t & b, const std::vector<uint32_t> & from) const {
    const double tc = 0.5 * (b.t0 + b.t1), dl = 0.5 * (b.t1 - b.t0);
    const double uc = 0.5 * (b.u0 + b.u1), vc = 0.5 * (b.v0 + b.v1);
    const double au = 0.5 * (b.u1 - b.u0), av = 0.5 * (b.v1 - b.v0);
    const double c = std::cos(sigma * tc), s = std::sin(sigma * tc);

    /* the edge normals of K at the centre angle, its support along them
     * and the box's reach */
    double nx[8], ny[8], h[8], reach[8];
    int cnt = 0;
    for (int i = 0; i < normals; i++) {
      nx[cnt] = base[i][0];
      ny[cnt] = base[i][1];
      h[cnt] = hBase[i] + support(nx[cnt] * c + ny[cnt] * s, -nx[cnt] * s + ny[cnt] * c);
      cnt++;
      nx[cnt] = base[i][0] * c - base[i][1] * s;
      ny[cnt] = base[i][0] * s + base[i][1] * c;
      h[cnt] = support(nx[cnt], ny[cnt]) + hBase[i];
      cnt++;
    }
    for (int i = 0; i < cnt; i++)
      reach[i] = au * std::fabs(nx[i]) + av * std::fabs(ny[i]);

    b.pairs.clear();
    b.deficit = 0;
    for (uint32_t pi : from) {
      const pair_t & p = allPairs[pi];
      const double x = uc - (p.su - (c * p.mu - s * p.mv)), y = vc - (p.sv - (s * p.mu + c * p.mv));
      const double eps = dl * p.lever;
      double out = -1e9, in = -1e9;
      for (int i = 0; i < cnt; i++) {
        const double q = std::fabs(nx[i] * x + ny[i] * y) - h[i];
        out = std::max(out, q - reach[i]);
        in = std::max(in, q + reach[i]);
      }
      if (in < wall - eps - 1e-12) {
        b.state = BLOCKED;
        b.pairs = {};
        return;
      }
      if (!(out > eps + settings.margin + 1e-12)) {
        b.pairs.push_back(pi);
        b.deficit = std::max(b.deficit, eps + settings.margin - out);
      }
    }
    if (b.pairs.empty()) {
      b.state = FREE;
      b.pairs = {};
    }
    else {
      b.state = MIXED;
      b.smallest = atLevel(b);
    }
  }

  /* GOAL_ALL, GOAL_SOME or GOAL_NONE for a box classified already, looking
   * at the goal obstacles `from` (those its parent could not rule out).
   *
   * A goal obstacle is c(θ) + K(θ) (+) S, S the slide backwards: nothing
   * for one along the axis (the obstacle is that of a cube ahead), the ray
   * {-t w : t >= 0} for one along w in the plane. The support of the sum is
   * that of K along a normal n with n · w >= 0 and infinite along the
   * others, and its edge normals are among those of K and the two normal to
   * w: signed tests along those decide it like the obstacles in classify. */
  void classifyGoal(box_t & b, const std::vector<uint32_t> & from) const {
    b.goalPairs.clear();
    b.goalDeficit = 0;
    if (b.state == BLOCKED) {
      b.goal = GOAL_NONE;
      return;
    }
    const double tc = 0.5 * (b.t0 + b.t1), dl = 0.5 * (b.t1 - b.t0);
    const double uc = 0.5 * (b.u0 + b.u1), vc = 0.5 * (b.v0 + b.v1);
    const double au = 0.5 * (b.u1 - b.u0), av = 0.5 * (b.v1 - b.v0);
    const double c = std::cos(sigma * tc), s = std::sin(sigma * tc);

    double nx[18], ny[18], h[18];
    int cnt = 0;
    const bool ray = !removal->axial;
    auto add = [&](double x, double y, double hh) {
      for (int sg = 1; sg >= -1; sg -= 2)
        if (!ray || sg * (x * removal->wu + y * removal->wv) > -1e-12) {
          nx[cnt] = sg * x;
          ny[cnt] = sg * y;
          h[cnt] = hh;
          cnt++;
        }
    };
    for (int i = 0; i < normals; i++) {
      add(base[i][0], base[i][1], hBase[i] + support(base[i][0] * c + base[i][1] * s, -base[i][0] * s + base[i][1] * c));
      const double x = base[i][0] * c - base[i][1] * s, y = base[i][0] * s + base[i][1] * c;
      add(x, y, support(x, y) + hBase[i]);
    }
    if (ray) {
      const double x = -removal->wv, y = removal->wu;
      const double hh = support(x, y) + support(x * c + y * s, -x * s + y * c);
      nx[cnt] = x; ny[cnt] = y; h[cnt] = hh; cnt++;
      nx[cnt] = -x; ny[cnt] = -y; h[cnt] = hh; cnt++;
    }

    for (uint32_t pi : from) {
      const pair_t & p = goalObstacles[pi];
      const double x = uc - (p.su - (c * p.mu - s * p.mv)), y = vc - (p.sv - (s * p.mu + c * p.mv));
      const double eps = dl * p.lever;
      double out = -1e9, in = -1e9;
      for (int i = 0; i < cnt; i++) {
        const double q = nx[i] * x + ny[i] * y - h[i];
        const double reach = au * std::fabs(nx[i]) + av * std::fabs(ny[i]);
        out = std::max(out, q - reach);
        in = std::max(in, q + reach);
      }
      if (in < -eps - 1e-12) {
        b.goal = GOAL_NONE;
        b.goalPairs = {};
        return;
      }
      if (!(out > eps + settings.margin + 1e-12)) {
        b.goalPairs.push_back(pi);
        b.goalDeficit = std::max(b.goalDeficit, eps + settings.margin - out);
      }
    }
    b.goal = (b.goalPairs.empty() && b.state == FREE) ? GOAL_ALL : GOAL_SOME;
  }

  /* true if the box is not cut further at the present level */
  bool atLevel(const box_t & b) const {
    return !(0.5 * std::max(b.u1 - b.u0, b.v1 - b.v0) > level) && !(0.5 * (b.t1 - b.t0) * lever > level);
  }

  /* true if the boxes share a face, not just an edge or a corner. Every
   * bound is a sum of halves of the root's: equal ones compare equal. */
  static bool touch(const box_t & a, const box_t & b) {
    const bool ot = a.t0 < b.t1 && b.t0 < a.t1, ou = a.u0 < b.u1 && b.u0 < a.u1, ov = a.v0 < b.v1 && b.v0 < a.v1;
    return ((a.t1 == b.t0 || b.t1 == a.t0) && ou && ov) ||
           ((a.u1 == b.u0 || b.u1 == a.u0) && ot && ov) ||
           ((a.v1 == b.v0 || b.v1 == a.v0) && ot && ou);
  }

  /* cuts a MIXED leaf: the angle in two when its arc is the larger extent,
   * else the shift in four */
  void split(int id) {
    const double dl = 0.5 * (boxes[id].t1 - boxes[id].t0), a = 0.5 * (boxes[id].u1 - boxes[id].u0);
    const bool angle = (dl * lever > a && dl * lever > level) || !(a > level);
    const int cnt = angle ? 2 : 4, first = (int)boxes.size();
    boxes.resize(boxes.size() + cnt);
    box_t & p = boxes[id];
    for (int k = 0; k < cnt; k++) {
      box_t & ch = boxes[first + k];
      ch.t0 = p.t0; ch.t1 = p.t1; ch.u0 = p.u0; ch.u1 = p.u1; ch.v0 = p.v0; ch.v1 = p.v1;
      if (angle) {
        const double m = 0.5 * (p.t0 + p.t1);
        (k == 0 ? ch.t1 : ch.t0) = m;
      }
      else {
        const double mu = 0.5 * (p.u0 + p.u1), mv = 0.5 * (p.v0 + p.v1);
        ((k & 1) ? ch.u0 : ch.u1) = mu;
        ((k & 2) ? ch.v0 : ch.v1) = mv;
      }
      classify(ch, p.pairs);
      /* a box with no goal pose has none in its parts: its list is empty
       * because nothing is left to look at, not because all is clear */
      if (removal && p.goal != GOAL_NONE)
        classifyGoal(ch, p.goalPairs);
    }
    for (int k = 0; k < cnt; k++) {
      box_t & ch = boxes[first + k];
      if (ch.state == BLOCKED)
        continue;
      for (int j = 0; j < cnt; j++)
        if (j != k && boxes[first + j].state != BLOCKED && touch(ch, boxes[first + j]))
          ch.adj.push_back(first + j);
      for (int x : p.adj) {
        box_t & other = boxes[x];
        if (other.state == INNER || !touch(ch, other))
          continue;
        ch.adj.push_back(x);
        other.adj.push_back(first + k);
      }
    }
    p.state = INNER;
    p.child = first;
    p.children = cnt;
    p.adj = {};
    p.pairs = {};
    p.goalPairs = {};
  }

  /* the leaves whose closed box holds the pose (t, 0, 0) */
  void leavesAt(int id, double t, std::vector<int> & out) const {
    const box_t & b = boxes[id];
    if (t < b.t0 || t > b.t1 || 0 < b.u0 || 0 > b.u1 || 0 < b.v0 || 0 > b.v1)
      return;
    if (b.state != INNER) {
      out.push_back(id);
      return;
    }
    for (int k = 0; k < b.children; k++)
      leavesAt(b.child + k, t, out);
  }

  /* What a chain pays for passing a box. A free one: next to nothing, by
   * its size, so that short chains win. An undecided one: its deficit, how
   * much more room around its centre would make it free. That is little
   * where the centre is free (and less with every cut), and at least the
   * centre's depth where it is inside an obstacle, however small the box:
   * the search follows the free poses and leaves the walls alone. An
   * undecided box that will not be split costs more than any chain
   * without one. */
  static constexpr double STUCK = 1e6;
  double cost(const box_t & b) const {
    if (b.state == FREE)
      return 1e-6 * std::max(b.u1 - b.u0, (b.t1 - b.t0) * lever);
    return b.smallest ? STUCK : b.deficit + 1e-9;
  }

  /* What a chain pays for ending in a box (removal): nothing where every
   * pose is a goal, the goal deficit where that is not known, and STUCK if
   * such a box is not cut further. */
  double goalCost(const box_t & b) const {
    if (b.goal == GOAL_ALL)
      return 0;
    return atLevel(b) ? STUCK : b.goalDeficit + 1e-9;
  }

public:

  poseBoxes_c(const poseBoxes_c &) = delete;
  poseBoxes_c & operator=(const poseBoxes_c &) = delete;

  /* removal_: nullptr for the quarter turn of sense; else the goal, with
   * goalScene the scene whose pairs are the goal obstacles (ignored for a
   * slide in the plane, whose obstacles are the scene's own), sense unused */
  poseBoxes_c(const planarScene_c & scene, unsigned int sense, const decideSettings_t & settings_, double wall_ = 0,
              const removalGoal_t * removal_ = nullptr, const planarScene_c * goalScene = nullptr) :
    settings(settings_), wall(wall_), sigma((sense == 0 || removal_) ? 1.0 : -1.0), removal(removal_),
    level(std::max(settings_.resolution, 1.0 / 32)) {

    const double half = scene.halfSide(), bevel = scene.bevel();
    const double r = std::numbers::sqrt2 / 2;
    if (bevel > 0) {
      const double o[8][2] = { { half, half - bevel }, { half - bevel, half }, { -half + bevel, half }, { -half, half - bevel },
                               { -half, -half + bevel }, { -half + bevel, -half }, { half - bevel, -half }, { half, -half + bevel } };
      corners = 8;
      normals = 4;
      std::copy(&o[0][0], &o[0][0] + 16, &outline[0][0]);
    }
    else {
      const double o[4][2] = { { half, half }, { -half, half }, { -half, -half }, { half, -half } };
      corners = 4;
      normals = 2;
      std::copy(&o[0][0], &o[0][0] + 8, &outline[0][0]);
    }
    const double n[4][2] = { { 1, 0 }, { 0, 1 }, { r, r }, { r, -r } };
    for (int i = 0; i < 4; i++) {
      base[i][0] = n[i][0];
      base[i][1] = n[i][1];
      hBase[i] = support(n[i][0], n[i][1]);
    }

    /* a static cube further from the pivot than the moving one can get
     * within the shift box never meets it */
    const double circum = std::hypot(half, half - bevel);
    for (const planarScene_c::pair_t & p : scene.pairs()) {
      const double m = std::hypot(p.mu, p.mv);
      if (std::hypot(p.su, p.sv) > m + 2 * circum + settings.maxShift * std::numbers::sqrt2)
        continue;
      allPairs.push_back({ p.mu, p.mv, p.su, p.sv, m + circum });
      lever = std::max(lever, m + circum);
    }
    if (removal) {
      tMin = -removal->maxAngle;
      tMax = removal->maxAngle;
      /* a cube ahead is met at the pose, within the shift box, like one of
       * the same layer; a slide in the plane meets every cube of the layer
       * in its way, however far */
      const bool ray = !removal->axial;
      for (const planarScene_c::pair_t & p : (ray ? scene : *goalScene).pairs()) {
        const double m = std::hypot(p.mu, p.mv);
        if (!ray && std::hypot(p.su, p.sv) > m + 2 * circum + settings.maxShift * std::numbers::sqrt2)
          continue;
        goalObstacles.push_back({ p.mu, p.mv, p.su, p.sv, m + circum });
        lever = std::max(lever, m + circum);
      }
    }
    if (lever == 0)
      lever = 1;
  }

  turnDecision_t run(void) {
    turnDecision_t res;
    const double D = settings.maxShift;

    boxes.reserve(1024);
    boxes.emplace_back();
    {
      box_t & root = boxes[0];
      root.t0 = tMin; root.t1 = tMax; root.u0 = -D; root.u1 = D; root.v0 = -D; root.v1 = D;
      std::vector<uint32_t> all(allPairs.size());
      for (size_t i = 0; i < all.size(); i++)
        all[i] = (uint32_t)i;
      classify(root, all);
      if (removal) {
        all.resize(goalObstacles.size());
        for (size_t i = 0; i < all.size(); i++)
          all[i] = (uint32_t)i;
        classifyGoal(boxes[0], all);
      }
    }

    std::vector<double> dist;
    std::vector<int> prev;
    std::vector<unsigned int> stamp;
    std::vector<int> starts, goals, chain;
    typedef std::pair<double, int> entry_t;

    for (unsigned int round = 1; ; round++) {
      res.rounds = round;
      if (settings.stop && settings.stop->load(std::memory_order_relaxed)) {
        res.gaveUp = true;
        break;
      }

      /* the cheapest chain of leaves from the start to the end pose */
      dist.resize(boxes.size());
      prev.resize(boxes.size());
      stamp.resize(boxes.size(), 0);
      starts.clear();
      goals.clear();
      leavesAt(0, 0, starts);
      if (!removal)
        leavesAt(0, quarter, goals);
      std::priority_queue<entry_t, std::vector<entry_t>, std::greater<entry_t>> queue;
      auto reach = [&](int id, double d, int from) {
        if (stamp[id] == round && dist[id] <= d)
          return;
        stamp[id] = round;
        dist[id] = d;
        prev[id] = from;
        queue.push({ d, id });
      };
      for (int id : starts)
        if (boxes[id].state != BLOCKED)
          reach(id, cost(boxes[id]), -1);
      /* removal: a box that may hold a goal pose is joined to the goal at
       * its goalCost, an entry -2 - id in the queue */
      int hit = -1;
      double hitDist = 0;
      while (!queue.empty()) {
        const auto [d, id] = queue.top();
        queue.pop();
        if (id < -1) {
          hit = -2 - id;
          hitDist = d;
          break;
        }
        if (d > dist[id])
          continue;
        if (removal) {
          if (boxes[id].goal != GOAL_NONE)
            queue.push({ d + goalCost(boxes[id]), -2 - id });
        }
        else if (std::find(goals.begin(), goals.end(), id) != goals.end()) {
          hit = id;
          hitDist = d;
          break;
        }
        /* drop the neighbours split since on the way */
        std::vector<int> & adj = boxes[id].adj;
        size_t keep = 0;
        for (size_t k = 0; k < adj.size(); k++) {
          const int x = adj[k];
          if (boxes[x].state == INNER)
            continue;
          adj[keep++] = x;
          reach(x, d + cost(boxes[x]), id);
        }
        adj.resize(keep);
      }

      /* the blocked boxes separate the start from the end. Faces are
       * enough for that: where a motion passes from box to box through an
       * edge or a corner, no box around that pose is blocked, and those
       * are joined by faces. */
      if (hit < 0) {
        res.answer = TURN_IMPOSSIBLE;
        break;
      }
      /* the cheapest chain needs an undecided box of the smallest size, so
       * every chain does: smaller boxes, or at the resolution no further
       * cut can end in a motion */
      if (hitDist >= STUCK) {
        if (!(level > settings.resolution))
          break;
        level = std::max(settings.resolution, 0.5 * level);
        for (box_t & b : boxes)
          if (b.state == MIXED)
            b.smallest = atLevel(b);
        continue;
      }

      chain.clear();
      for (int id = hit; id >= 0; id = prev[id])
        chain.push_back(id);
      std::reverse(chain.begin(), chain.end());

      const bool goalKnown = !removal || boxes[hit].goal == GOAL_ALL;
      if (goalKnown && std::all_of(chain.begin(), chain.end(), [&](int id) { return boxes[id].state == FREE; })) {
        /* through the middle of the face each two boxes share: within a
         * box every straight segment stays in it */
        res.answer = TURN_POSSIBLE;
        res.path.push_back({ 0, 0, 0 });
        for (size_t k = 0; k + 1 < chain.size(); k++) {
          const box_t & a = boxes[chain[k]], & b = boxes[chain[k + 1]];
          res.path.push_back({ sigma * 0.5 * (std::max(a.t0, b.t0) + std::min(a.t1, b.t1)),
                               0.5 * (std::max(a.u0, b.u0) + std::min(a.u1, b.u1)),
                               0.5 * (std::max(a.v0, b.v0) + std::min(a.v1, b.v1)) });
        }
        if (removal) {
          /* every pose of the last box is a goal: its centre */
          const box_t & b = boxes[hit];
          res.path.push_back({ 0.5 * (b.t0 + b.t1), 0.5 * (b.u0 + b.u1), 0.5 * (b.v0 + b.v1) });
        }
        else
          res.path.push_back({ sigma * quarter, 0, 0 });
        break;
      }

      if (boxes.size() >= settings.maxBoxes || !share.hold(boxes.size(), settings.stop)) {
        res.gaveUp = true;
        break;
      }
      for (int id : chain)
        if (boxes[id].state == MIXED && !boxes[id].smallest)
          split(id);
      /* a free box that may hold a goal pose, cut to find out */
      if (removal && boxes[hit].state == FREE && boxes[hit].goal == GOAL_SOME && !atLevel(boxes[hit]))
        split(hit);
    }
    res.boxes = boxes.size();
    return res;
  }
};

}

size_t setPlannerBoxBudget(size_t boxes) {
  boxBudget_t & b = boxBudget();
  size_t before;
  {
    std::lock_guard<std::mutex> lock(b.m);
    before = b.budget;
    b.budget = boxes;
  }
  b.freed.notify_all();
  return before;
}

turnDecision_t decideTurn(const planarScene_c & scene, unsigned int sense, const decideSettings_t & settings) {
  if (sense > 1 || !(settings.maxShift > 0) || !(settings.resolution > 0) || !(settings.margin >= 0))
    bt_te("decideTurn: needs sense 0 or 1, maxShift > 0, resolution > 0, margin >= 0");
  return poseBoxes_c(scene, sense, settings).run();
}

turnVerdict_t answerTurn(const planarScene_c & scene, unsigned int sense, const decideSettings_t & settings,
                         const std::vector<path_t> & known) {
  turnVerdict_t v;
  for (size_t i = 0; i < known.size(); i++)
    if (!known[i].empty() && scene.certifyPath(known[i]) == PATH_CLEAR) {
      v.answer = TURN_POSSIBLE;
      v.path = known[i];
      v.known = (int)i;
      return v;
    }

  turnDecision_t d = decideTurn(scene, sense, settings);
  v.answer = d.answer;
  if (d.answer == TURN_POSSIBLE) {
    /* the path keeps the margin by construction; shortcut, half of it */
    v.path = shortcutPath(scene, d.path, 0.5 * settings.margin);
    if (scene.certifyPath(v.path) != PATH_CLEAR) {
      v.answer = TURN_UNDECIDED;
      v.path.clear();
    }
  }
  return v;
}

turnVerdict_t answerTurn(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                         unsigned int axis, int pivotHx, int pivotHy, int pivotHz, unsigned int sense,
                         const model_t & model, const decideSettings_t & settings,
                         const std::vector<knownPath_t> & known) {
  const planarScene_c scene(moving, stat, axis, pivotHx, pivotHy, pivotHz, model);
  std::optional<planarScene_c> swapped;
  for (size_t i = 0; i < known.size(); i++) {
    if (known[i].path.empty())
      continue;
    if (known[i].inverse && !swapped)
      swapped.emplace(stat, moving, axis, pivotHx, pivotHy, pivotHz, model);
    if ((known[i].inverse ? *swapped : scene).certifyPath(known[i].path) == PATH_CLEAR) {
      turnVerdict_t v;
      v.answer = TURN_POSSIBLE;
      if (!known[i].inverse)
        v.path = known[i].path;
      v.known = (int)i;
      return v;
    }
  }
  return answerTurn(scene, sense, settings);
}

namespace {

void checkRemoval(const removalGoal_t & goal, const decideSettings_t & settings) {
  if ((goal.axial && goal.sign != 1 && goal.sign != -1) ||
      (!goal.axial && !(std::fabs(std::hypot(goal.wu, goal.wv) - 1) < 1e-9)) ||
      !(goal.maxAngle > 0 && goal.maxAngle <= std::numbers::pi) ||
      !(settings.maxShift > 0) || !(settings.resolution > 0) || !(settings.margin >= 0))
    bt_te("decideRemoval: needs sign +1 or -1, a unit slide direction, 0 < maxAngle <= pi, maxShift > 0, resolution > 0, margin >= 0");
}

/* the scene whose pairs are those of a moving cube and a static cube of a
 * layer the cube passes when the group slides along the axis by sign,
 * beyond its own: each such static cube copied into the moving layer */
planarScene_c aheadScene(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                         unsigned int axis, int pivotHx, int pivotHy, int pivotHz, const model_t & model, int sign) {
  std::vector<int> layers;
  for (const cell_t & c : moving)
    layers.push_back(axialCoordinate(c.x, c.y, c.z, axis));
  std::sort(layers.begin(), layers.end());
  layers.erase(std::unique(layers.begin(), layers.end()), layers.end());
  std::vector<cell_t> copies;
  for (const cell_t & c : stat) {
    const int a = axialCoordinate(c.x, c.y, c.z, axis);
    for (int l : layers)
      if (sign * (a - l) > 0) {
        cell_t k = c;
        (axis == 0 ? k.x : axis == 1 ? k.y : k.z) = l;
        copies.push_back(k);
      }
  }
  return planarScene_c(moving, copies, axis, pivotHx, pivotHy, pivotHz, model);
}

}

turnDecision_t decideRemoval(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                             unsigned int axis, int pivotHx, int pivotHy, int pivotHz, const model_t & model,
                             const removalGoal_t & goal, const decideSettings_t & settings) {
  checkRemoval(goal, settings);
  const planarScene_c scene(moving, stat, axis, pivotHx, pivotHy, pivotHz, model);
  if (!goal.axial)
    return poseBoxes_c(scene, 0, settings, 0, &goal, nullptr).run();
  const planarScene_c ahead = aheadScene(moving, stat, axis, pivotHx, pivotHy, pivotHz, model, goal.sign);
  return poseBoxes_c(scene, 0, settings, 0, &goal, &ahead).run();
}

verdict_e certifyRemoval(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                         unsigned int axis, int pivotHx, int pivotHy, int pivotHz, const model_t & model,
                         const removalGoal_t & goal, const path_t & path) {
  checkRemoval(goal, decideSettings_t());
  if (path.empty() || path[0].theta != 0 || path[0].du != 0 || path[0].dv != 0)
    bt_te("certifyRemoval: the path starts at the identity pose");
  const planarScene_c scene(moving, stat, axis, pivotHx, pivotHy, pivotHz, model);
  const verdict_e v = scene.certifyPath(path);
  if (v != PATH_CLEAR)
    return v;
  const pose_t & e = path.back();

  /* along the axis: the slide keeps the pose in the plane, so each moving
   * cube is clear of every static cube it passes when their footprints
   * are apart at that pose */
  if (goal.axial)
    return aheadScene(moving, stat, axis, pivotHx, pivotHy, pivotHz, model, goal.sign).clearance(e) > 0 ? PATH_CLEAR : PATH_BLOCKED;

  /* in the plane: the slide up to where every moving cube is more than two
   * cells beyond every static one along w (a cube's footprint is narrower
   * than that), from there on they only part further */
  int pu, pv;
  inPlaneDoubled(pivotHx, pivotHy, pivotHz, axis, &pu, &pv);
  const double c = std::cos(e.theta), s = std::sin(e.theta);
  double lo = 1e300, hi = -1e300;
  for (const cell_t & m : moving) {
    int u, v;
    inPlaneDoubled(m.x, m.y, m.z, axis, &u, &v);
    const double mu = 0.5 * u - 0.25 * pu, mv = 0.5 * v - 0.25 * pv;
    lo = std::min(lo, (c * mu - s * mv + e.du) * goal.wu + (s * mu + c * mv + e.dv) * goal.wv);
  }
  for (const cell_t & st : stat) {
    int u, v;
    inPlaneDoubled(st.x, st.y, st.z, axis, &u, &v);
    hi = std::max(hi, (0.5 * u - 0.25 * pu) * goal.wu + (0.5 * v - 0.25 * pv) * goal.wv);
  }
  const double t = std::max(0.0, hi - lo) + 2;
  return scene.certifyPath({ e, { e.theta, e.du + t * goal.wu, e.dv + t * goal.wv } });
}

turnPlan_t widestTurn(const planarScene_c & scene, unsigned int sense, const decideSettings_t & settings, double tolerance) {

  turnPlan_t plan;
  turnDecision_t best = decideTurn(scene, sense, settings);
  if (best.answer != TURN_POSSIBLE) {
    plan.undecided = best.answer == TURN_UNDECIDED;
    return plan;
  }

  /* every motion starts and ends where it must: it keeps no more room
   * than those two poses have */
  const double quarter = (sense == 0 ? 0.5 : -0.5) * std::numbers::pi;
  double lo = settings.margin;
  double hi = std::min(scene.clearance({ 0, 0, 0 }), scene.clearance({ quarter, 0, 0 }));
  /* poses with less room than the margin count as blocked: a margin no
   * motion keeps is ruled out like a turn that is impossible. Boxes of a
   * fraction of the tolerance tell the margins apart. */
  decideSettings_t wider = settings;
  wider.resolution = std::max(settings.resolution, 0.25 * tolerance);
  while (hi - lo > tolerance) {
    wider.margin = 0.5 * (lo + hi);
    turnDecision_t d = poseBoxes_c(scene, sense, wider, wider.margin).run();
    if (d.answer == TURN_POSSIBLE) {
      lo = wider.margin;
      best = std::move(d);
    }
    else
      hi = wider.margin;
  }

  plan.found = true;
  plan.path = shortcutPath(scene, best.path, 0.9 * lo);
  plan.clearance = scene.minClearance(plan.path);
  return plan;
}

namespace {

/* The smallest value of one model parameter (play or bevel, the other
 * fixed) at which answerTurn says possible, and the largest at which it
 * says impossible: both only add motions as they grow. modelAt gives the
 * model for a value; start is the model's own value, tried first; lo is
 * where the search stops below (a value there is not tried unless
 * tryLowest), hi the largest valid value. */
template <class ModelAt>
playBound_t bisectModel(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                        unsigned int axis, int pivotHx, int pivotHy, int pivotHz, unsigned int sense,
                        const decideSettings_t & settings, double tolerance, double start, double lowest, double highest,
                        double minStep, bool tryLowest, const std::vector<knownPath_t> & known, ModelAt modelAt) {

  playBound_t result;

  /* is the turn possible at x: answerTurn says so. The largest value
   * proven impossible is kept on the way. */
  double impossible = 0;
  auto tryAt = [&](double x) {
    turnVerdict_t v = answerTurn(moving, stat, axis, pivotHx, pivotHy, pivotHz, sense, modelAt(x), settings, known);
    if (v.answer == TURN_IMPOSSIBLE)
      impossible = std::max(impossible, x);
    if (v.answer != TURN_POSSIBLE)
      return false;
    result.path = std::move(v.path);
    result.known = v.known;
    return true;
  };

  /* bracket: possible at hi, not (proven) possible at lo */
  double lo = lowest, hi = start;
  if (!tryAt(start)) {
    bool found = false;
    while (!found && hi < highest) {
      lo = hi;
      hi = std::min(highest, std::max(1.5 * hi, hi + minStep));
      found = tryAt(hi);
    }
    if (!found) {
      result.lower = impossible;
      return result;
    }
  }
  else if (tryLowest && start > lowest && tryAt(lowest))
    hi = lo;

  while (hi - lo > tolerance) {
    const double mid = 0.5 * (lo + hi);
    if (tryAt(mid))
      hi = mid;
    else
      lo = mid;
  }
  result.found = true;
  result.upper = hi;

  /* the lower end: the largest value proven impossible. Impossible stays
   * impossible below, so this is a bisection too, between the two proofs. */
  double below = impossible, above = hi;
  while (above - below > tolerance) {
    const double mid = 0.5 * (below + above);
    const planarScene_c scene(moving, stat, axis, pivotHx, pivotHy, pivotHz, modelAt(mid));
    if (decideTurn(scene, sense, settings).answer == TURN_IMPOSSIBLE)
      below = mid;
    else
      above = mid;
  }
  result.lower = below;
  return result;
}

}

playBound_t requiredPlay(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                         unsigned int axis, int pivotHx, int pivotHy, int pivotHz, unsigned int sense,
                         const model_t & model, const decideSettings_t & settings,
                         double tolerance, double maxPlay, const std::vector<knownPath_t> & known) {
  /* below 0.001 passages are hairlines; more play must leave the cube
   * larger than its chamfer */
  return bisectModel(moving, stat, axis, pivotHx, pivotHy, pivotHz, sense, settings, tolerance, model.play, 1e-3,
                     std::min(maxPlay, 1.0 - 2.0 * model.chamfer - 1e-6), 0, false, known,
                     [&](double play) { return model_t{ play, model.chamfer }; });
}

playBound_t requiredBevel(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                          unsigned int axis, int pivotHx, int pivotHy, int pivotHz, unsigned int sense,
                          const model_t & model, const decideSettings_t & settings,
                          double tolerance, const std::vector<knownPath_t> & known) {
  /* the bevel must leave the cube larger than it (validModel) */
  return bisectModel(moving, stat, axis, pivotHx, pivotHy, pivotHz, sense, settings, tolerance, model.chamfer, 0,
                     0.5 * (1.0 - model.play) - 1e-6, 0.01, true, known,
                     [&](double bevel) { return model_t{ model.play, bevel }; });
}

}
