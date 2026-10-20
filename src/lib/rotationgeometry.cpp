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
#include <cmath>
#include <iterator>
#include <limits>
#include <map>
#include <numbers>
#include <atomic>
#include <optional>
#include <thread>

namespace rotationGeometry {

void quarterTurn(int * x, int * y, int * z, unsigned int axis, unsigned int sense) {

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
  bt_assert(axis < 3 && sense < 2);
  return ids[axis][sense];
}

void inPlaneDoubled(int x, int y, int z, unsigned int axis, int * u, int * v) {
  if (axis == 0)      { *u = 2 * y; *v = 2 * z; }
  else if (axis == 1) { *u = 2 * z; *v = 2 * x; }
  else                { *u = 2 * x; *v = 2 * y; }
}

int axialCoordinate(int x, int y, int z, unsigned int axis) {
  return (axis == 0) ? x : ((axis == 1) ? y : z);
}

void pivotOf(unsigned int axis, int u, int v, int * hx, int * hy, int * hz) {
  if (axis == 0)      { *hx = 0; *hy = u; *hz = v; }
  else if (axis == 1) { *hx = v; *hy = 0; *hz = u; }
  else                { *hx = u; *hy = v; *hz = 0; }
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

/* squared distance of point p from segment a-b */
double segmentPointDistance2(double px, double py,
                             double ax, double ay, double bx, double by) {
  double dx = bx - ax, dy = by - ay;
  double t = ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy);
  t = std::clamp(t, 0.0, 1.0);
  double ex = ax + t * dx - px, ey = ay + t * dy - py;
  return ex * ex + ey * ey;
}

/* Signed distance between the axis-aligned square at (su, sv) and the square
 * at (mu, mv) turned by theta, both with half side h: their distance when
 * apart, minus the penetration depth when overlapping. For convex polygons
 * the separating axis test over all edge normals yields the exact
 * penetration depth. */
double squareSignedDistance(double su, double sv, double mu, double mv,
                            double c, double s, double h) {

  double sp[4][2], mp[4][2];
  static const int sg[4][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
  for (int i = 0; i < 4; i++) {
    sp[i][0] = su + h * sg[i][0];
    sp[i][1] = sv + h * sg[i][1];
    mp[i][0] = mu + h * (sg[i][0] * c - sg[i][1] * s);
    mp[i][1] = mv + h * (sg[i][0] * s + sg[i][1] * c);
  }

  const double normals[4][2] = { { 1, 0 }, { 0, 1 }, { c, s }, { -s, c } };
  double depth = std::numeric_limits<double>::max();
  bool apart = false;
  for (int k = 0; k < 4 && !apart; k++) {
    double smin = std::numeric_limits<double>::max(), smax = -smin;
    double mmin = smin, mmax = -smin;
    for (int i = 0; i < 4; i++) {
      double ds = sp[i][0] * normals[k][0] + sp[i][1] * normals[k][1];
      double dm = mp[i][0] * normals[k][0] + mp[i][1] * normals[k][1];
      smin = std::min(smin, ds); smax = std::max(smax, ds);
      mmin = std::min(mmin, dm); mmax = std::max(mmax, dm);
    }
    double overlap = std::min(smax, mmax) - std::max(smin, mmin);
    if (overlap <= 0)
      apart = true;
    else
      depth = std::min(depth, overlap);
  }

  if (!apart)
    return -depth;

  double dist2 = std::numeric_limits<double>::max();
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++) {
      int j2 = (j + 1) & 3;
      dist2 = std::min(dist2, segmentPointDistance2(sp[i][0], sp[i][1], mp[j][0], mp[j][1], mp[j2][0], mp[j2][1]));
      dist2 = std::min(dist2, segmentPointDistance2(mp[i][0], mp[i][1], sp[j][0], sp[j][1], sp[j2][0], sp[j2][1]));
    }
  return std::sqrt(dist2);
}

/* Below this point displacement a segment that is neither proven clear nor
 * found blocked is reported undecided. */
const double MIN_DISPLACEMENT = 1e-7;

}

planarScene_c::planarScene_c(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                             unsigned int axis, int pivotHx, int pivotHy, int pivotHz,
                             const model_t & model) :
  half(0.5 * (1.0 - model.play) - model.rounding),
  rounding(model.rounding),
  rho(0)
{
  bt_assert(axis < 3);
  /* Without play the cubes touch at the start pose, and a cube turning about
   * a corner it shares with a diagonal neighbour touches it all the way: no
   * finite check can prove that clear, it only subdivides towards
   * MIN_DISPLACEMENT forever. Real pieces always have play. */
  if (!(model.play > 0 && model.rounding >= 0 && half > 0))
    bt_te("planarScene_c: needs play > 0, rounding >= 0 and a cube larger than its rounding");

  double pu, pv;
  inPlane(0.5 * pivotHx, 0.5 * pivotHy, 0.5 * pivotHz, axis, &pu, &pv);

  std::map<int, layer_t> byLayer;
  for (const cell_t & c : moving) {
    square_t sq;
    inPlane(c.x, c.y, c.z, axis, &sq.u, &sq.v);
    sq.u -= pu;
    sq.v -= pv;
    byLayer[axial(c, axis)].moving.push_back(sq);
    rho = std::max(rho, std::sqrt(sq.u * sq.u + sq.v * sq.v) + half * std::sqrt(2.0));
  }

  /* static cubes on layers without moving cubes can never be hit */
  for (const cell_t & c : stat) {
    auto it = byLayer.find(axial(c, axis));
    if (it == byLayer.end())
      continue;
    square_t sq;
    inPlane(c.x, c.y, c.z, axis, &sq.u, &sq.v);
    sq.u -= pu;
    sq.v -= pv;
    it->second.stat.push_back(sq);
  }

  for (auto & l : byLayer)
    if (!l.second.stat.empty())
      layers.push_back(std::move(l.second));
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
         * skip when sqrt(d2) - reach - 2 rounding >= best, without the root */
        const double bound = best + reach + 2 * rounding;
        if (bound >= 0 && du * du + dv * dv >= bound * bound)
          continue;
        best = std::min(best, squareSignedDistance(st.u, st.v, mu, mv, c, s, half) - 2 * rounding);
      }
    }

  return best;
}

bool planarScene_c::certifySegment(const pose_t & a, const pose_t & b, verdict_e * v) const {

  /* No point of the moving group moves further than `lipschitz` over the
   * whole segment, and the clearance is 1-Lipschitz in point displacement.
   * So clearance(mid) >= lipschitz * halfwidth proves an interval clear. */
  const double lipschitz = std::hypot(b.du - a.du, b.dv - a.dv) + rho * std::fabs(b.theta - a.theta);

  std::vector<std::pair<double, double>> todo = { { 0.0, 1.0 } };
  while (!todo.empty()) {
    auto [s0, s1] = todo.back();
    todo.pop_back();

    double sm = 0.5 * (s0 + s1);
    pose_t p = { a.theta + sm * (b.theta - a.theta),
                 a.du + sm * (b.du - a.du),
                 a.dv + sm * (b.dv - a.dv) };
    double cl = clearance(p);

    if (cl < 0) {
      *v = PATH_BLOCKED;
      return false;
    }
    if (cl >= 0.5 * (s1 - s0) * lipschitz)
      continue;
    if ((s1 - s0) * lipschitz < MIN_DISPLACEMENT) {
      *v = PATH_UNDECIDED;
      continue;
    }
    todo.push_back({ s0, sm });
    todo.push_back({ sm, s1 });
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
      return PATH_BLOCKED;

  return v;
}

verdict_e planarScene_c::certifyQuarterTurn(unsigned int sense) const {
  const double angle = (sense == 0) ? std::numbers::pi / 2 : -std::numbers::pi / 2;
  return certifyPath({ { 0, 0, 0 }, { angle, 0, 0 } });
}

path_t makePath(const pathSpec_t & spec) {

  const double end = (spec.sense == 0) ? std::numbers::pi / 2 : -std::numbers::pi / 2;

  if (spec.kind == PATH_PURE)
    return { { 0, 0, 0 }, { end, 0, 0 } };

  if ((spec.dirU == 0 && spec.dirV == 0) || std::abs(spec.dirU) > 1 || std::abs(spec.dirV) > 1 || !(spec.amount > 0))
    bt_te("makePath: shifts need a compass direction and a positive amount");

  const double len = std::hypot((double)spec.dirU, (double)spec.dirV);
  const double su = spec.amount * spec.dirU / len;
  const double sv = spec.amount * spec.dirV / len;

  if (spec.kind == PATH_SLIDE)
    return { { 0, 0, 0 }, { 0, su, sv }, { end, su, sv }, { end, 0, 0 } };

  path_t path;
  for (int k = 0; k <= WIGGLE_SEGMENTS; k++) {
    double t = (double)k / WIGGLE_SEGMENTS;
    double w = std::sin(std::numbers::pi * t);
    path.push_back({ end * t, w * su, w * sv });
  }
  /* exactly back on the lattice, sin(π) is not quite 0 */
  path.back() = { end, 0, 0 };
  return path;
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
      const cell_t moving = { (du + parity) / 2, (dv + parity) / 2, 0 };
      std::vector<std::pair<int, int>> & entry = table[index(du, dv)];

      for (int su = -reach + parity; su <= reach; su += 2)
        for (int sv = -reach + parity; sv <= reach; sv += 2) {
          if (su == du && sv == dv)
            continue;
          if (quarterArcDistance(0.5 * su, 0.5 * sv, 0.5 * du, 0.5 * dv, angle > 0) > hitRange)
            continue;
          const cell_t stat = { (su + parity) / 2, (sv + parity) / 2, 0 };
          planarScene_c scene({ moving }, { stat }, 2, parity, parity, 0, model);
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
  bt_assert(axis < 3);
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
  s.dirU = -spec.dirV;
  s.dirV = spec.dirU;

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
  s.dirV = -spec.dirV;

  pathTable_c t(radius, s);
  for (int du = -2 * radius; du <= 2 * radius; du++)
    for (int dv = -2 * radius; dv <= 2 * radius; dv++)
      for (const auto & [su, sv] : table[index(du, dv)])
        t.table[t.index(du, -dv)].push_back({ su, -sv });
  return t;
}

rotationTables_c::rotationTables_c(const model_t & model, int r, const std::vector<double> & amounts) :
  radius(r)
{
  /* the tables computed with the checker: the pure turn and, per amount
   * and kind, directions (1,0) and (1,1); they are independent, so they
   * are built in parallel */
  std::vector<pathSpec_t> specs = { { PATH_PURE, 0, 0, 0, 0 } };
  for (double amount : amounts)
    for (pathKind_e kind : { PATH_WIGGLE, PATH_SLIDE }) {
      specs.push_back({ kind, 0, 1, 0, amount });
      specs.push_back({ kind, 0, 1, 1, amount });
    }

  std::vector<std::optional<pathTable_c>> built(specs.size());
  {
    std::atomic<size_t> next{0};
    auto work = [&]() {
      for (size_t i; (i = next++) < specs.size(); )
        built[i].emplace(model, r, specs[i]);
    };
    const unsigned int threads = std::max(1u, std::min<unsigned int>(std::thread::hardware_concurrency(), specs.size()));
    std::vector<std::jthread> pool;
    for (unsigned int t = 1; t < threads; t++)
      pool.emplace_back(work);
    work();
  }

  tables[0].push_back(std::move(*built[0]));

  size_t base = 1;
  for (size_t a = 0; a < amounts.size(); a++)
    for (int kind = 0; kind < 2; kind++, base += 2) {
      /* the 8 compass directions counter-clockwise from (1,0) alternate
       * between turned copies of (1,0) and of (1,1) */
      pathTable_c axisDir = std::move(*built[base]);
      pathTable_c diagonal = std::move(*built[base + 1]);
      for (int k = 0; k < 4; k++) {
        tables[0].push_back(axisDir);
        tables[0].push_back(diagonal);
        axisDir = axisDir.turned();
        diagonal = diagonal.turned();
      }
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

  /* the inverse: static cube relative to moving cube -> moving cube offsets */
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

  for (size_t i = 0; i < tables[sense].size(); i++) {
    bool blocked = false;
    for (size_t k = 0; k < cubes.size() && !blocked; k++)
      for (const auto & [su, sv] : tables[sense][i].table[cubes[k].entry])
        if (occupied(cubes[k].layer, su, sv)) {
          blocked = true;
          break;
        }
    if (!blocked)
      return (int)i;
  }
  return -1;
}

}
