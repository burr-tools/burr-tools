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

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include "rotationgeometry.h"

#include "bt_assert.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cmath>
#include <iterator>
#include <limits>
#include <map>
#include <numbers>
#include <atomic>
#include <optional>
#include <queue>
#include <unordered_map>
#include <thread>

namespace rotationGeometry {

void quarterTurn(int * x, int * y, int * z, unsigned int axis, unsigned int sense) {

  if (axis > 2 || sense > 1)
    bt_te("quarterTurn: axis must be 0 to 2, sense 0 or 1");

  int ox = *x, oy = *y, oz = *z;

  if (axis == 0) {
    if (sense == 0) { *y = -oz; *z = oy; }
    else            { *y = oz;  *z = -oy; }
  } else if (axis == 1) {
    if (sense == 0) { *x = oz;  *z = -ox; }
    else            { *x = -oz; *z = ox; }
  } else {
    if (sense == 0) { *x = -oy; *y = ox; }
    else            { *x = oy;  *y = -ox; }
  }
}

unsigned char quarterTurnTrans(unsigned int axis, unsigned int sense) {

  /* brick grid orientation ids, see tabs_0/rotmatrix.inc */
  static const unsigned char ids[3][2] = {
    { 1, 3 },
    { 12, 4 },
    { 16, 20 }
  };
  if (axis > 2 || sense > 1)
    bt_te("quarterTurnTrans: axis must be 0 to 2, sense 0 or 1");
  return ids[axis][sense];
}

void turnAbout(int * x, int * y, int * z, int hx, int hy, int hz, unsigned int axis, unsigned int sense) {
  int dx = 2 * *x - hx, dy = 2 * *y - hy, dz = 2 * *z - hz;
  quarterTurn(&dx, &dy, &dz, axis, sense);
  bt_assert(!((hx + dx) & 1) && !((hy + dy) & 1) && !((hz + dz) & 1));
  *x = (hx + dx) / 2;
  *y = (hy + dy) / 2;
  *z = (hz + dz) / 2;
}

namespace {

/* In-plane coordinates of a vector for a rotation about `axis`, ordered so
 * that a positive angle is right-handed about the axis. */
void inPlane(double x, double y, double z, unsigned int axis, double * u, double * v) {
  if (axis == 0)      { *u = y; *v = z; }
  else if (axis == 1) { *u = z; *v = x; }
  else                { *u = x; *v = y; }
}

double axial(const cell_t & c, unsigned int axis) {
  return (axis == 0) ? c.x : ((axis == 1) ? c.y : c.z);
}

/* std::min and std::max of doubles, by value. The std:: ones take and
 * return references, which makes their operands addressable: under
 * AddressSanitizer each call of a function using them then sets up an
 * instrumented stack frame, and the two functions below run hundreds of
 * millions of times in a table build. Same results: `b < a` picks as
 * std::min does, `a < b` as std::max. */
inline double minOf(double a, double b) { return (b < a) ? b : a; }
inline double maxOf(double a, double b) { return (a < b) ? b : a; }

/* squared distance of point p from segment a-b */
double segmentPointDistance2(double px, double py,
                             double ax, double ay, double bx, double by) {
  double dx = bx - ax, dy = by - ay;
  double t = ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy);
  t = (t < 0.0) ? 0.0 : minOf(t, 1.0);   // std::clamp(t, 0.0, 1.0)
  double ex = ax + t * dx - px, ey = ay + t * dy - py;
  return ex * ex + ey * ey;
}

/* Signed distance between the axis-aligned square at (su, sv) and the square
 * at (mu, mv) turned by theta, both with half side h and their corners cut
 * at 45° by k (an octagon, a square when k is 0): their distance when apart,
 * minus the penetration depth when overlapping. For convex polygons the
 * separating axis test over all edge normals yields the exact penetration
 * depth. */
double squareSignedDistance(double su, double sv, double mu, double mv,
                            double c, double s, double h, double k) {

  /* corners counter-clockwise, two per corner of the square when cut */
  static const int sg[4][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
  double loc[8][2];
  int n = 0;
  for (int i = 0; i < 4; i++) {
    const double x = h * sg[i][0], y = h * sg[i][1];
    if (k > 0) {
      /* entering the corner along the previous edge, leaving along the next */
      const bool horizontalIn = (sg[i][0] != sg[i][1]);
      loc[n][0] = horizontalIn ? x - sg[i][0] * k : x;
      loc[n][1] = horizontalIn ? y : y - sg[i][1] * k;
      n++;
      loc[n][0] = horizontalIn ? x : x - sg[i][0] * k;
      loc[n][1] = horizontalIn ? y - sg[i][1] * k : y;
      n++;
    } else {
      loc[n][0] = x;
      loc[n][1] = y;
      n++;
    }
  }

  double sp[8][2], mp[8][2];
  for (int i = 0; i < n; i++) {
    sp[i][0] = su + loc[i][0];
    sp[i][1] = sv + loc[i][1];
    mp[i][0] = mu + loc[i][0] * c - loc[i][1] * s;
    mp[i][1] = mv + loc[i][0] * s + loc[i][1] * c;
  }

  /* edge normals of both polygons; opposite edges share one */
  const double r = std::numbers::sqrt2 / 2;
  const double normals[8][2] = { { 1, 0 }, { 0, 1 }, { c, s }, { -s, c },
                                 { r, r }, { r, -r }, { r * (c - s), r * (s + c) }, { r * (c + s), r * (s - c) } };
  const int nn = (k > 0) ? 8 : 4;
  double depth = std::numeric_limits<double>::max();
  bool apart = false;
  for (int j = 0; j < nn && !apart; j++) {
    double smin = std::numeric_limits<double>::max(), smax = -smin;
    double mmin = smin, mmax = -smin;
    for (int i = 0; i < n; i++) {
      double ds = sp[i][0] * normals[j][0] + sp[i][1] * normals[j][1];
      double dm = mp[i][0] * normals[j][0] + mp[i][1] * normals[j][1];
      smin = minOf(smin, ds); smax = maxOf(smax, ds);
      mmin = minOf(mmin, dm); mmax = maxOf(mmax, dm);
    }
    double overlap = minOf(smax, mmax) - maxOf(smin, mmin);
    if (overlap <= 0)
      apart = true;
    else
      depth = minOf(depth, overlap);
  }

  if (!apart)
    return -depth;

  double dist2 = std::numeric_limits<double>::max();
  for (int i = 0; i < n; i++)
    for (int j = 0; j < n; j++) {
      int j2 = (j + 1) % n;
      dist2 = minOf(dist2, segmentPointDistance2(sp[i][0], sp[i][1], mp[j][0], mp[j][1], mp[j2][0], mp[j2][1]));
      dist2 = minOf(dist2, segmentPointDistance2(mp[i][0], mp[i][1], sp[j][0], sp[j][1], sp[j2][0], sp[j2][1]));
    }
  return std::sqrt(dist2);
}

/* Below this point displacement a segment that is neither proven clear nor
 * found blocked is reported undecided, which ends the check: undecided
 * counts as blocked, and finishing the segment would subdivide every
 * stretch of near contact down to this resolution. */
const double MIN_DISPLACEMENT = 1e-7;

}

planarScene_c::planarScene_c(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                             unsigned int axis, int pivotHx, int pivotHy, int pivotHz,
                             const model_t & model) :
  half(0.5 * (1.0 - model.play)),
  chamfer(model.chamfer),
  rho(0),
  /* the octagon's inner radius is the smaller distance to an axis face or
   * a bevel face, its outer radius that of a bevel end point */
  inRadius(std::min(half, (2 * half - model.chamfer) / std::numbers::sqrt2)),
  outRadius(std::hypot(half, half - model.chamfer))
{
  if (axis > 2)
    bt_te("planarScene_c: axis must be 0 to 2");
  /* Without play the cubes touch at the start pose, and a cube turning about
   * a corner it shares with a diagonal neighbour touches it all the way: no
   * finite check can prove that clear, every such turn would come out
   * undecided. Real pieces always have play. */
  if (!validModel(model))
    bt_te("planarScene_c: needs play > 0, chamfer >= 0 and a cube larger than its chamfer");

  double pu, pv;
  inPlane(0.5 * pivotHx, 0.5 * pivotHy, 0.5 * pivotHz, axis, &pu, &pv);

  /* the layers of the moving cubes, ascending along the axis, built in
   * place: a table build makes millions of one-cube scenes, and a map
   * beside the layers was one more allocation each */
  auto layerOf = [this](int a) {
    return std::lower_bound(layers.begin(), layers.end(), a,
                            [](const layer_t & l, int key) { return l.axial < key; });
  };
  for (const cell_t & c : moving) {
    square_t sq;
    inPlane(c.x, c.y, c.z, axis, &sq.u, &sq.v);
    sq.u -= pu;
    sq.v -= pv;
    const int a = (int)axial(c, axis);
    auto l = layerOf(a);
    if (l == layers.end() || l->axial != a)
      l = layers.insert(l, layer_t{ a, {}, {} });
    l->moving.push_back(sq);
    rho = std::max(rho, std::sqrt(sq.u * sq.u + sq.v * sq.v) + half * std::sqrt(2.0));
  }

  /* static cubes on layers without moving cubes can never be hit */
  for (const cell_t & c : stat) {
    const int a = (int)axial(c, axis);
    const auto l = layerOf(a);
    if (l == layers.end() || l->axial != a)
      continue;
    square_t sq;
    inPlane(c.x, c.y, c.z, axis, &sq.u, &sq.v);
    sq.u -= pu;
    sq.v -= pv;
    l->stat.push_back(sq);
  }

  std::erase_if(layers, [](const layer_t & l) { return l.stat.empty(); });
}

std::vector<planarScene_c::pair_t> planarScene_c::pairs(void) const {
  std::vector<pair_t> out;
  for (const layer_t & l : layers)
    for (const square_t & m : l.moving)
      for (const square_t & s : l.stat)
        out.push_back({ m.u, m.v, s.u, s.v });
  return out;
}

double planarScene_c::clearance(const pose_t & p) const {

  const double c = std::cos(p.theta);
  const double s = std::sin(p.theta);
  const double reach = 2.0 * std::sqrt(2.0) * half;

  double best = std::numeric_limits<double>::max();

  for (const layer_t & l : layers)
    for (const square_t & m : l.moving) {
      double mu = m.u * c - m.v * s + p.du;
      double mv = m.u * s + m.v * c + p.dv;
      for (const square_t & st : l.stat) {
        double du = mu - st.u, dv = mv - st.v;
        /* the squares' circumcircles bound the signed distance from below:
         * skip when sqrt(d2) - reach >= best, without the root */
        const double bound = best + reach;
        if (bound >= 0 && du * du + dv * dv >= bound * bound)
          continue;
        best = std::min(best, squareSignedDistance(st.u, st.v, mu, mv, c, s, half, chamfer));
      }
    }

  return best;
}

int planarScene_c::classify(const pose_t & p, double margin, double * clOut) const {

  const double c = std::cos(p.theta);
  const double s = std::sin(p.theta);
  /* Each cube contains its in-circle and lies in its circumcircle, so
   * centres closer than 2·inRadius overlap, and centres at least
   * 2·outRadius + margin apart are at least margin apart. */
  const double inner = 2 * inRadius;
  const double outer = 2 * outRadius + margin;

  /* pairs skipped below are at least margin apart, so when some pair is
   * closer, the smallest computed clearance is the clearance */
  double best = margin;
  for (const layer_t & l : layers)
    for (const square_t & m : l.moving) {
      double mu = m.u * c - m.v * s + p.du;
      double mv = m.u * s + m.v * c + p.dv;
      for (const square_t & st : l.stat) {
        double du = mu - st.u, dv = mv - st.v;
        double d2 = du * du + dv * dv;
        if (d2 < inner * inner)
          return -1;
        if (d2 >= outer * outer)
          continue;
        double cl = squareSignedDistance(st.u, st.v, mu, mv, c, s, half, chamfer);
        if (cl < 0)
          return -1;
        best = std::min(best, cl);
      }
    }

  *clOut = best;
  return (best < margin) ? 0 : 1;
}

bool planarScene_c::certifySegment(const pose_t & a, const pose_t & b, verdict_e * v) const {

  /* No point of the moving group moves further than `lipschitz` over the
   * whole segment, and the clearance is 1-Lipschitz in point displacement.
   * So clearance(mid) >= lipschitz * halfwidth proves an interval clear. */
  const double lipschitz = std::hypot(b.du - a.du, b.dv - a.dv) + rho * std::fabs(b.theta - a.theta);

  /* the intervals still to decide; one buffer per thread, as a table build
   * certifies millions of segments (and an allocation costs much more
   * under AddressSanitizer than the few classify calls most of them need) */
  thread_local std::vector<std::pair<double, double>> todo;
  todo.assign(1, { 0.0, 1.0 });
  while (!todo.empty()) {
    auto [s0, s1] = todo.back();
    todo.pop_back();

    double sm = 0.5 * (s0 + s1);
    pose_t p = { a.theta + sm * (b.theta - a.theta),
                 a.du + sm * (b.du - a.du),
                 a.dv + sm * (b.dv - a.dv) };
    double cl;
    int c = classify(p, 0.5 * (s1 - s0) * lipschitz, &cl);

    if (c < 0) {
      *v = PATH_BLOCKED;
      return false;
    }
    if (c > 0)
      continue;
    if ((s1 - s0) * lipschitz < MIN_DISPLACEMENT) {
      *v = PATH_UNDECIDED;
      return false;
    }
    /* the clearance still proves sm ± cl / lipschitz clear: split only
     * what lies outside */
    const double w = cl / lipschitz;
    todo.push_back({ s0, sm - w });
    todo.push_back({ sm + w, s1 });
  }

  return true;
}

verdict_e planarScene_c::certifyPath(const std::vector<pose_t> & waypoints) const {

  bt_assert(!waypoints.empty());

  verdict_e v = PATH_CLEAR;

  if (waypoints.size() == 1)
    return (clearance(waypoints[0]) < 0) ? PATH_BLOCKED : PATH_CLEAR;

  for (size_t i = 0; i + 1 < waypoints.size(); i++)
    if (!certifySegment(waypoints[i], waypoints[i + 1], &v))
      return v;

  return PATH_CLEAR;
}

verdict_e planarScene_c::certifyQuarterTurn(unsigned int sense) const {
  return certifyPath(makePath({ PATH_PURE, sense }));
}

path_t makePath(const pathSpec_t & spec) {

  if (spec.sense > 1 || (unsigned int)spec.kind > PATH_RAMP)
    bt_te("makePath: sense must be 0 or 1, kind one of pathKind_e");

  const double end = (spec.sense == 0) ? std::numbers::pi / 2 : -std::numbers::pi / 2;

  if (spec.kind == PATH_PURE)
    return { { 0, 0, 0 }, { end, 0, 0 } };

  if (spec.kind == PATH_PLANNED) {
    if (spec.waypoints.size() < 2 || spec.waypoints.back().theta != end)
      bt_te("makePath: a planned path needs waypoints ending at the quarter turn");
    return spec.waypoints;
  }

  if (spec.dir < 0 || spec.dir >= DIRECTIONS || !(spec.amount > 0))
    bt_te("makePath: shifts need a direction and a positive amount");

  double su, sv;
  directionVector(spec.dir, &su, &sv);
  su *= spec.amount;
  sv *= spec.amount;

  if (spec.kind == PATH_SLIDE)
    return { { 0, 0, 0 }, { 0, su, sv }, { end, su, sv }, { end, 0, 0 } };

  if (spec.kind == PATH_RAMP) {
    if (spec.holdFrom <= 0 || spec.holdTo < spec.holdFrom || spec.holdTo >= 90)
      bt_te("makePath: a ramp needs 0 < holdFrom <= holdTo < 90");
    /* reversed (reversePath), out by a and held to b is out by 90 - b and
     * held to 90 - a, in the other sense: families hold both */
    path_t path = { { 0, 0, 0 }, { end * spec.holdFrom / 90, su, sv } };
    if (spec.holdTo > spec.holdFrom)
      path.push_back({ end * spec.holdTo / 90, su, sv });
    path.push_back({ end, 0, 0 });
    return path;
  }

  /* The shift follows the bulge of a square turning about its centre: its
   * extent along an axis grows by cos θ + sin θ - 1 (in half sides), 0 at
   * both ends and largest at 45°. With amount equal to a cube's own bulge
   * the cube's leading face stays on the plane it started on, i.e. its
   * corner glides along the neighbour it rubs against. The profile is
   * symmetric in t and 1 - t, so a reversed wiggle is a wiggle of the
   * other sense. */
  path_t path;
  for (int k = 0; k <= WIGGLE_SEGMENTS; k++) {
    double t = (double)k / WIGGLE_SEGMENTS;
    double th = 0.5 * std::numbers::pi * t;
    double w = (std::cos(th) + std::sin(th) - 1) / (std::numbers::sqrt2 - 1);
    path.push_back({ end * t, w * su, w * sv });
  }
  /* exactly back on the lattice, cos(π/2) is not quite 0 */
  path.back() = { end, 0, 0 };
  return path;
}

void directionVector(int dir, double * u, double * v) {
  bt_assert(dir >= 0 && dir < DIRECTIONS);
  /* the compass directions as before 16 directions, (1, 1) / √2 and not
   * cos 45°, which may differ in the last bit */
  static const int compass[8][2] = { { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 }, { 0, -1 }, { 1, -1 } };
  if (dir % 2 == 0) {
    const double len = std::hypot((double)compass[dir / 2][0], (double)compass[dir / 2][1]);
    *u = compass[dir / 2][0] / len;
    *v = compass[dir / 2][1] / len;
  } else {
    const double a = 2 * std::numbers::pi * dir / DIRECTIONS;
    *u = std::cos(a);
    *v = std::sin(a);
  }
}

namespace {

std::vector<double> ascending(std::vector<double> amounts) {
  std::sort(amounts.begin(), amounts.end());
  amounts.erase(std::unique(amounts.begin(), amounts.end()), amounts.end());
  return amounts;
}

}

std::vector<pathSpec_t> compassFamily(const std::vector<double> & amounts) {
  std::vector<pathSpec_t> family = { { PATH_PURE, 0, 0, 0 } };
  for (double amount : ascending(amounts))
    for (pathKind_e kind : { PATH_WIGGLE, PATH_SLIDE })
      for (int dir = 0; dir < DIRECTIONS; dir += 2)
        family.push_back({ kind, 0, dir, amount });
  return family;
}

std::vector<pathSpec_t> wideExtension(const std::vector<double> & compass, const std::vector<double> & wide) {
  std::vector<pathSpec_t> family;
  for (double amount : ascending(wide)) {
    const bool inCompass = std::find(compass.begin(), compass.end(), amount) != compass.end();
    for (pathKind_e kind : { PATH_WIGGLE, PATH_SLIDE })
      for (int dir = 0; dir < DIRECTIONS; dir++)
        if (!inCompass || dir % 2)
          family.push_back({ kind, 0, dir, amount });
    for (auto [from, to] : { std::pair{ 45, 45 }, std::pair{ 15, 45 }, std::pair{ 45, 75 } })
      for (int dir = 0; dir < DIRECTIONS; dir++)
        family.push_back({ PATH_RAMP, 0, dir, amount, {}, from, to });
  }
  return family;
}

pose_t poseAt(const path_t & path, double t) {

  bt_assert(!path.empty());
  if (path.size() == 1 || t <= 0)
    return path.front();
  if (t >= 1)
    return path.back();

  const double segments = (double)(path.size() - 1);
  const size_t seg = std::min((size_t)(t * segments), path.size() - 2);
  const double local = t * segments - (double)seg;
  const pose_t & a = path[seg];
  const pose_t & b = path[seg + 1];
  return { a.theta + local * (b.theta - a.theta),
           a.du + local * (b.du - a.du),
           a.dv + local * (b.dv - a.dv) };
}

namespace {

/* No point of the moving group moves further than this over the segment
 * (see certifySegment). */
double segmentLipschitz(const pose_t & a, const pose_t & b, double rho) {
  return std::hypot(b.du - a.du, b.dv - a.dv) + rho * std::fabs(b.theta - a.theta);
}

pose_t lerp(const pose_t & a, const pose_t & b, double s) {
  return { a.theta + s * (b.theta - a.theta), a.du + s * (b.du - a.du), a.dv + s * (b.dv - a.dv) };
}

}

double planarScene_c::segmentMinimum(const pose_t & a, const pose_t & b, double eps, double stopBelow) const {

  const double lipschitz = segmentLipschitz(a, b, rho);

  /* Piyavskii-style branch and bound: an interval of half width w around a
   * sample with clearance c has no clearance below c - lipschitz·w. Refine
   * the interval with the lowest such bound until it is within eps of the
   * lowest sample, which makes it a lower bound within eps of the minimum. */
  struct interval_t {
    double lower, s0, s1;
    bool operator<(const interval_t & o) const { return lower > o.lower; }
  };
  std::priority_queue<interval_t> todo;

  double best = std::min(clearance(a), clearance(b));

  auto push = [&](double s0, double s1) {
    const double c = clearance(lerp(a, b, 0.5 * (s0 + s1)));
    best = std::min(best, c);
    todo.push({ c - 0.5 * (s1 - s0) * lipschitz, s0, s1 });
  };
  push(0, 1);

  while (true) {
    if (best < stopBelow)
      return best;
    const interval_t i = todo.top();
    if (i.lower >= best - eps || (i.s1 - i.s0) * lipschitz < MIN_DISPLACEMENT)
      return std::min(i.lower, best);
    todo.pop();
    const double sm = 0.5 * (i.s0 + i.s1);
    push(i.s0, sm);
    push(sm, i.s1);
  }
}

double planarScene_c::minClearance(const std::vector<pose_t> & waypoints, double eps) const {

  bt_assert(!waypoints.empty());
  if (waypoints.size() == 1)
    return clearance(waypoints[0]);

  double result = std::numeric_limits<double>::max();
  for (size_t i = 0; i + 1 < waypoints.size(); i++)
    result = std::min(result, segmentMinimum(waypoints[i], waypoints[i + 1], eps, -std::numeric_limits<double>::max()));
  return result;
}

bool planarScene_c::clearanceAtLeast(const std::vector<pose_t> & waypoints, double threshold) const {

  bt_assert(!waypoints.empty());
  if (clearance(waypoints[0]) < threshold)
    return false;

  for (size_t i = 0; i + 1 < waypoints.size(); i++) {
    const pose_t & a = waypoints[i];
    const pose_t & b = waypoints[i + 1];
    const double lipschitz = segmentLipschitz(a, b, rho);
    thread_local std::vector<std::pair<double, double>> todo;   // as in certifySegment
    todo.assign(1, { 0.0, 1.0 });
    while (!todo.empty()) {
      auto [s0, s1] = todo.back();
      todo.pop_back();
      const double sm = 0.5 * (s0 + s1);
      const double c = clearance(lerp(a, b, sm));
      if (c < threshold || (s1 - s0) * lipschitz < MIN_DISPLACEMENT)
        return false;
      /* the sample proves sm ± (c - threshold) / lipschitz */
      const double w = (lipschitz > 0) ? (c - threshold) / lipschitz : 1;
      if (sm - w > s0)
        todo.push_back({ s0, sm - w });
      if (sm + w < s1)
        todo.push_back({ sm + w, s1 });
    }
  }
  return true;
}

bool validModel(const model_t & model) {
  return model.play > 0 && model.chamfer >= 0 && model.chamfer < 0.5 * (1.0 - model.play);
}

path_t reversePath(const path_t & path) {
  bt_assert(!path.empty());
  const double end = path.back().theta;
  path_t r;
  for (auto it = path.rbegin(); it != path.rend(); ++it)
    r.push_back({ it->theta - end, it->du, it->dv });
  return r;
}

/* The polyline with fewer waypoints: from each kept waypoint jump to the
 * farthest one the straight segment reaches with at least `keep` clearance. */
path_t shortcutPath(const planarScene_c & scene, const path_t & full, double keep) {
  path_t path = { full[0] };
  size_t i = 0;
  while (i + 1 < full.size()) {
    size_t j = full.size() - 1;
    size_t step = j - i;
    while (j > i + 1 && !scene.clearanceAtLeast({ full[i], full[j] }, keep)) {
      step = std::max<size_t>(1, step / 2);
      j = (j - i > step) ? j - step : i + 1;
    }
    path.push_back(full[j]);
    i = j;
  }
  return path;
}

namespace {

/* Doubled in-plane offset of a cell from a doubled pivot, same frames as inPlane. */
void doubledOffset(const cell_t & c, unsigned int axis, int phx, int phy, int phz, int * u, int * v) {
  int dx = 2 * c.x - phx, dy = 2 * c.y - phy, dz = 2 * c.z - phz;
  if (axis == 0)      { *u = dy; *v = dz; }
  else if (axis == 1) { *u = dz; *v = dx; }
  else                { *u = dx; *v = dy; }
}

/* Distance of point (pu, pv) from the arc that (mu, mv) sweeps in a quarter
 * turn about the origin, counter-clockwise if positive. Free of
 * trigonometry, as it runs for every candidate of every table entry: the
 * arc ends at m turned by 90 degrees, and as the swept sector spans less than
 * 180 degrees, p lies in it iff it is on the inner side of both rays. */
double quarterArcDistance(double pu, double pv, double mu, double mv, bool positive) {
  const double eu = positive ? -mv : mv, ev = positive ? mu : -mu;
  const double r = std::sqrt(mu * mu + mv * mv);
  const double rp2 = pu * pu + pv * pv;
  if (r == 0)
    return std::sqrt(rp2);

  const double turn = positive ? 1 : -1;
  if (rp2 > 0 && turn * (mu * pv - mv * pu) >= 0 && turn * (pu * ev - pv * eu) >= 0)
    return std::fabs(std::sqrt(rp2) - r);

  const double toStart = (pu - mu) * (pu - mu) + (pv - mv) * (pv - mv);
  const double toEnd = (pu - eu) * (pu - eu) + (pv - ev) * (pv - ev);
  return std::sqrt(std::min(toStart, toEnd));
}

/* Is the cell at doubled in-plane position (u2, v2), in the given layer,
 * occupied? The doubled coordinates of a cell are even. */
bool occupiedAt(const occupancy_c & stat, unsigned int axis, int layer, int u2, int v2) {
  const int u = u2 / 2, v = v2 / 2;
  if (axis == 0)      return stat(layer, u, v);
  else if (axis == 1) return stat(v, layer, u);
  else                return stat(u, v, layer);
}

}

pathTable_c::pathTable_c(int r, const pathSpec_t & s) :
  radius(r),
  width(4 * r + 1),
  spec(s),
  table(width * width)
{
  if (r < 1)
    bt_te("pathTable_c: radius must be at least 1");
}

pathTable_c::pathTable_c(const model_t & model, int r, const pathSpec_t & s) :
  pathTable_c(r, s)
{
  const path_t path = makePath(spec);
  const double angle = path.back().theta;
  const double shift = (spec.kind == PATH_PURE) ? 0 : spec.amount;

  /* Two cubes can only touch when their centres are at most the sum of their
   * circumradii apart, and the moving centre stays within `shift` of its
   * pure arc (the shifts of a path are convex combinations of its waypoint
   * shifts, all at most `shift` long). */
  const double hitRange = std::sqrt(2.0) * (1 - model.play) + shift + 1e-6;
  /* the arcs stay within radius·√2 of the pivot along each axis */
  const int reach = 2 * ((int)std::ceil(r * std::sqrt(2.0) + hitRange) + 1);

  for (int du = -2 * r; du <= 2 * r; du++)
    for (int dv = -2 * r; dv <= 2 * r; dv++) {
      int parity = du & 1;
      if ((dv & 1) != parity)
        continue;

      /* place the pivot at the origin's centre or corner in the Z frame,
       * where (u, v) = (x, y); the cells follow from the doubled offsets */
      const std::vector<cell_t> moving = { { (du + parity) / 2, (dv + parity) / 2, 0 } };
      std::vector<cell_t> stat(1);   // one blocker at a time, the vectors reused
      std::vector<std::pair<int, int>> & entry = table[index(du, dv)];

      for (int su = -reach + parity; su <= reach; su += 2)
        for (int sv = -reach + parity; sv <= reach; sv += 2) {
          if (su == du && sv == dv)
            continue;
          if (quarterArcDistance(0.5 * su, 0.5 * sv, 0.5 * du, 0.5 * dv, angle > 0) > hitRange)
            continue;
          stat[0] = { (su + parity) / 2, (sv + parity) / 2, 0 };
          planarScene_c scene(moving, stat, 2, parity, parity, 0, model);
          if (scene.certifyPath(path) != PATH_CLEAR)
            entry.push_back({ su, sv });
        }
    }
}

pathTable_c::pathTable_c(const model_t & model, const pathTable_c & tighter) :
  pathTable_c(tighter.radius, tighter.spec)
{
  const path_t path = makePath(spec);
  std::vector<cell_t> moving(1), stat(1);
  for (int du = -2 * radius; du <= 2 * radius; du++)
    for (int dv = -2 * radius; dv <= 2 * radius; dv++) {
      const int parity = du & 1;
      if ((dv & 1) != parity)
        continue;
      moving[0] = { (du + parity) / 2, (dv + parity) / 2, 0 };
      std::vector<std::pair<int, int>> & entry = table[index(du, dv)];
      for (const auto & [su, sv] : tighter.table[index(du, dv)]) {
        stat[0] = { (su + parity) / 2, (sv + parity) / 2, 0 };
        planarScene_c scene(moving, stat, 2, parity, parity, 0, model);
        if (scene.certifyPath(path) != PATH_CLEAR)
          entry.push_back({ su, sv });
      }
    }
}

unsigned int pathTable_c::index(int du, int dv) const {
  return (unsigned int)((du + 2 * radius) * width + dv + 2 * radius);
}

bool pathTable_c::covers(int du, int dv) const {
  return du >= -2 * radius && du <= 2 * radius && dv >= -2 * radius && dv <= 2 * radius;
}

const std::vector<std::pair<int, int>> & pathTable_c::blockers(int du, int dv) const {
  if (!covers(du, dv) || ((du ^ dv) & 1))
    bt_te("pathTable_c::blockers: offset outside the table or on an edge pivot");
  return table[index(du, dv)];
}

bool pathTable_c::blocked(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                          unsigned int axis, int pivotHx, int pivotHy, int pivotHz) const {
  if (axis > 2)
    bt_te("pathTable_c::blocked: axis must be 0 to 2");
  const occupancy_c occupied(stat);
  int pu, pv;
  doubledOffset({ 0, 0, 0 }, axis, -pivotHx, -pivotHy, -pivotHz, &pu, &pv);
  for (const cell_t & c : moving) {
    int u, v;
    doubledOffset(c, axis, pivotHx, pivotHy, pivotHz, &u, &v);
    for (const auto & [su, sv] : blockers(u, v))
      if (occupiedAt(occupied, axis, (int)axial(c, axis), pu + su, pv + sv))
        return true;
  }
  return false;
}

pathTable_c pathTable_c::turned(void) const {

  pathSpec_t s = spec;
  s.dir = (spec.dir + DIRECTIONS / 4) % DIRECTIONS;

  pathTable_c t(radius, s);
  for (int du = -2 * radius; du <= 2 * radius; du++)
    for (int dv = -2 * radius; dv <= 2 * radius; dv++)
      for (const auto & [su, sv] : table[index(du, dv)])
        t.table[t.index(-dv, du)].push_back({ -sv, su });
  return t;
}

pathTable_c pathTable_c::mirrored(void) const {

  pathSpec_t s = spec;
  s.sense = 1 - spec.sense;
  s.dir = (DIRECTIONS - spec.dir) % DIRECTIONS;

  pathTable_c t(radius, s);
  for (int du = -2 * radius; du <= 2 * radius; du++)
    for (int dv = -2 * radius; dv <= 2 * radius; dv++)
      for (const auto & [su, sv] : table[index(du, dv)])
        t.table[t.index(du, -dv)].push_back({ su, -sv });
  return t;
}

namespace {

/* a path's direction reduced to the first quarter (0 to 3 sixteenths):
 * the table that is computed, the others are turned copies */
pathSpec_t baseOf(pathSpec_t s) {
  if (s.kind != PATH_PURE)
    s.dir %= DIRECTIONS / 4;
  return s;
}

bool sameSpec(const pathSpec_t & a, const pathSpec_t & b) {
  return a.kind == b.kind && a.dir == b.dir && a.amount == b.amount && a.holdFrom == b.holdFrom && a.holdTo == b.holdTo;
}

/* f(0) to f(n - 1), independent, on up to one thread per core */
template <class F> void inParallel(size_t n, F f) {
  std::atomic<size_t> next{0};
  auto work = [&]() {
    for (size_t i; (i = next++) < n; )
      f(i);
  };
  const unsigned int threads = std::max(1u, std::min<unsigned int>(std::thread::hardware_concurrency(), n));
  std::vector<std::jthread> pool;
  for (unsigned int t = 1; t < threads; t++)
    pool.emplace_back(work);
  work();
}

}

rotationTables_c::rotationTables_c(const model_t & model, int r, const std::vector<pathSpec_t> & family) :
  radius(r)
{
  /* the tables computed with the checker: each path's direction reduced
   * to the first quarter (0 to 3), once per distinct path; they are
   * independent, so they are built in parallel. The others are turned
   * copies: turning the whole scene about the pivot commutes with the
   * motion. */
  std::vector<pathSpec_t> specs;
  std::vector<size_t> baseIndex;
  for (const pathSpec_t & s : family) {
    if (s.sense != 0 || s.kind == PATH_PLANNED)
      bt_te("rotationTables_c: a family holds fixed +90° paths");
    const pathSpec_t b = baseOf(s);
    size_t i = 0;
    while (i < specs.size() && !sameSpec(specs[i], b))
      i++;
    if (i == specs.size())
      specs.push_back(b);
    baseIndex.push_back(i);
  }

  std::vector<std::optional<pathTable_c>> built(specs.size());
  inParallel(specs.size(), [&](size_t i) { built[i].emplace(model, r, specs[i]); });
  assemble(family, baseIndex, built);
}

rotationTables_c::rotationTables_c(const model_t & model, const rotationTables_c & tighter) :
  radius(tighter.radius)
{
  /* tighter's +90° tables turned back to their direction in the first
   * quarter: the distinct tables it was built from */
  const std::vector<pathTable_c> & family = tighter.tables[0];
  std::vector<size_t> baseIndex;
  std::vector<const pathTable_c *> first;   // a family table per distinct one
  std::vector<pathSpec_t> specs;
  for (const pathTable_c & t : family) {
    const pathSpec_t b = baseOf(t.getSpec());
    size_t i = 0;
    while (i < specs.size() && !sameSpec(specs[i], b))
      i++;
    if (i == specs.size()) {
      specs.push_back(b);
      first.push_back(&t);
    }
    baseIndex.push_back(i);
  }

  std::vector<std::optional<pathTable_c>> built(specs.size());
  inParallel(specs.size(), [&](size_t i) {
    pathTable_c base = *first[i];
    if (base.getSpec().kind != PATH_PURE)
      for (int k = base.getSpec().dir / (DIRECTIONS / 4); k % 4 != 0; k++)
        base = base.turned();
    built[i].emplace(model, base);
  });

  std::vector<pathSpec_t> specsOfFamily;
  for (const pathTable_c & t : family)
    specsOfFamily.push_back(t.getSpec());
  assemble(specsOfFamily, baseIndex, built);
}

void rotationTables_c::assemble(const std::vector<pathSpec_t> & family, const std::vector<size_t> & baseIndex,
                                std::vector<std::optional<pathTable_c>> & built) {

  for (size_t i = 0; i < family.size(); i++) {
    pathTable_c t = *built[baseIndex[i]];
    if (family[i].kind != PATH_PURE)
      for (int k = 0; k < family[i].dir / (DIRECTIONS / 4); k++)
        t = t.turned();
    tables[0].push_back(std::move(t));
  }

  for (const pathTable_c & t : tables[0])
    tables[1].push_back(t.mirrored());

  /* blockers common to every path, the quick reject in firstClear */
  for (unsigned int sense = 0; sense < 2; sense++) {
    const size_t entries = tables[sense][0].table.size();
    blockedByAll[sense].resize(entries);
    for (size_t e = 0; e < entries; e++) {
      std::vector<std::pair<int, int>> common = tables[sense][0].table[e];
      std::sort(common.begin(), common.end());
      for (size_t i = 1; i < tables[sense].size() && !common.empty(); i++) {
        std::vector<std::pair<int, int>> other = tables[sense][i].table[e];
        std::sort(other.begin(), other.end());
        std::vector<std::pair<int, int>> both;
        std::set_intersection(common.begin(), common.end(), other.begin(), other.end(), std::back_inserter(both));
        common.swap(both);
      }
      blockedByAll[sense][e] = std::move(common);
    }
  }

  /* the other blockers with the set of paths each blocks, for firstClear */
  const size_t n = tables[0].size();
  words = (unsigned int)((n + 63) / 64);
  for (unsigned int sense = 0; sense < 2; sense++) {
    const size_t entries = tables[sense][0].table.size();
    pairStart[sense].assign(entries + 1, 0);
    for (size_t e = 0; e < entries; e++) {
      std::map<std::pair<int, int>, std::vector<uint64_t>> paths;
      for (size_t i = 0; i < n; i++)
        for (const auto & off : tables[sense][i].table[e]) {
          std::vector<uint64_t> & b = paths[off];
          if (b.empty())
            b.assign(words, 0);
          b[i / 64] |= uint64_t(1) << (i % 64);
        }
      std::vector<std::pair<size_t, std::pair<int, int>>> partial;
      for (const auto & [off, b] : paths) {
        size_t count = 0;
        for (uint64_t w : b)
          count += (size_t)std::popcount(w);
        if (count < n)
          partial.push_back({ count, off });
      }
      std::stable_sort(partial.begin(), partial.end(), [](const auto & a, const auto & b) { return a.first > b.first; });
      for (const auto & p : partial) {
        pairOffset[sense].push_back(p.second);
        const std::vector<uint64_t> & b = paths[p.second];
        pairBits[sense].insert(pairBits[sense].end(), b.begin(), b.end());
      }
      pairStart[sense][e + 1] = (uint32_t)pairOffset[sense].size();
    }
  }

  /* the inverse: static cube relative to moving cube -> moving cube offsets */
  const int r = radius;
  const int w = 4 * r + 1;
  for (unsigned int sense = 0; sense < 2; sense++)
    for (int e = 0; e < w * w; e++)
      for (const auto & [su, sv] : blockedByAll[sense][e])
        inverseReach = std::max({ inverseReach, std::abs(su - (e / w - 2 * r)), std::abs(sv - (e % w - 2 * r)) });
  const int iw = 2 * inverseReach + 1;
  for (unsigned int sense = 0; sense < 2; sense++) {
    inverse[sense].resize((size_t)iw * iw);
    for (int e = 0; e < w * w; e++) {
      const int du = e / w - 2 * r, dv = e % w - 2 * r;
      for (const auto & [su, sv] : blockedByAll[sense][e])
        inverse[sense][(size_t)(su - du + inverseReach) * iw + (sv - dv + inverseReach)].push_back({ du, dv });
    }
  }
}

const std::vector<std::pair<int, int>> rotationTables_c::none;

const std::vector<std::pair<int, int>> & rotationTables_c::pivotsBlockedAt(unsigned int sense, int du, int dv) const {
  if (sense > 1)
    bt_te("rotationTables_c::pivotsBlockedAt: sense must be 0 or 1");
  if (std::abs(du) > inverseReach || std::abs(dv) > inverseReach)
    return none;
  return inverse[sense][(size_t)(du + inverseReach) * (2 * inverseReach + 1) + (dv + inverseReach)];
}

const pathTable_c & rotationTables_c::table(unsigned int sense, unsigned int idx) const {
  if (sense > 1 || idx >= tables[sense].size())
    bt_te("rotationTables_c::table: no such path");
  return tables[sense][idx];
}

const std::vector<std::pair<int, int>> & rotationTables_c::blockedOnAllPaths(unsigned int sense, int du, int dv) const {
  const pathTable_c & first = tables[sense > 1 ? 0 : sense][0];
  if (sense > 1 || !first.covers(du, dv) || ((du ^ dv) & 1))
    bt_te("rotationTables_c::blockedOnAllPaths: offset outside the tables or on an edge pivot");
  return blockedByAll[sense][first.index(du, dv)];
}

occupancy_c::occupancy_c(const std::vector<cell_t> & cells) {
  if (cells.empty())
    return;
  int x1 = cells[0].x, y1 = cells[0].y, z1 = cells[0].z;
  x0 = x1; y0 = y1; z0 = z1;
  for (const cell_t & c : cells) {
    x0 = std::min(x0, c.x); x1 = std::max(x1, c.x);
    y0 = std::min(y0, c.y); y1 = std::max(y1, c.y);
    z0 = std::min(z0, c.z); z1 = std::max(z1, c.z);
  }
  sx = x1 - x0 + 1;
  sy = y1 - y0 + 1;
  sz = z1 - z0 + 1;
  bits.assign((size_t)sx * sy * sz, false);
  for (const cell_t & c : cells)
    bits[((size_t)(c.z - z0) * sy + (c.y - y0)) * sx + (c.x - x0)] = true;
}

int rotationTables_c::firstClear(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                                 unsigned int axis, int pivotHx, int pivotHy, int pivotHz,
                                 unsigned int sense) const {
  return firstClear(moving, occupancy_c(stat), axis, pivotHx, pivotHy, pivotHz, sense);
}

int rotationTables_c::firstClear(const std::vector<cell_t> & moving, const occupancy_c & stat,
                                 unsigned int axis, int pivotHx, int pivotHy, int pivotHz,
                                 unsigned int sense) const {
  if (sense > 1 || axis > 2)
    bt_te("rotationTables_c::firstClear: sense must be 0 or 1, axis 0 to 2");

  const pathTable_c & first = tables[sense][0];

  /* in-plane doubled pivot and each moving cube's table entry */
  int pu, pv;
  doubledOffset({ 0, 0, 0 }, axis, -pivotHx, -pivotHy, -pivotHz, &pu, &pv);

  struct movingCube_t { int layer; unsigned int entry; };
  std::vector<movingCube_t> cubes;
  cubes.reserve(moving.size());
  for (const cell_t & c : moving) {
    int u, v;
    doubledOffset(c, axis, pivotHx, pivotHy, pivotHz, &u, &v);
    if (!first.covers(u, v) || ((u ^ v) & 1))
      bt_te("rotationTables_c::firstClear: cube outside the tables or edge pivot");
    cubes.push_back({ (int)axial(c, axis), first.index(u, v) });
  }

  /* static cube at doubled offset (su, sv) from the pivot, in the layer */
  auto occupied = [&](int layer, int su, int sv) { return occupiedAt(stat, axis, layer, pu + su, pv + sv); };

  for (const movingCube_t & m : cubes)
    for (const auto & [su, sv] : blockedByAll[sense][m.entry])
      if (occupied(m.layer, su, sv))
        return -1;

  /* OR the paths every occupied blocker blocks; the first path left over
   * is the first clear one */
  const size_t n = tables[sense].size();
  const uint64_t lastWord = (n % 64) ? (uint64_t(1) << (n % 64)) - 1 : ~uint64_t(0);
  uint64_t local[16];
  std::vector<uint64_t> heap;
  uint64_t * blocked = local;
  if (words > 16) {
    heap.assign(words, 0);
    blocked = heap.data();
  } else
    std::fill(local, local + words, 0);
  auto allBlocked = [&]() {
    for (unsigned int w = 0; w + 1 < words; w++)
      if (blocked[w] != ~uint64_t(0))
        return false;
    return (blocked[words - 1] & lastWord) == lastWord;
  };

  for (const movingCube_t & m : cubes)
    for (uint32_t j = pairStart[sense][m.entry]; j < pairStart[sense][m.entry + 1]; j++) {
      const auto & [su, sv] = pairOffset[sense][j];
      if (!occupied(m.layer, su, sv))
        continue;
      const uint64_t * b = &pairBits[sense][(size_t)j * words];
      for (unsigned int w = 0; w < words; w++)
        blocked[w] |= b[w];
      if (allBlocked())
        return -1;
    }

  for (size_t i = 0; i < n; i++)
    if (!((blocked[i / 64] >> (i % 64)) & 1))
      return (int)i;
  return -1;
}

}
