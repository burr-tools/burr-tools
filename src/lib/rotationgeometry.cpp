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
#include <atomic>
#include <bit>
#include <cmath>
#include <exception>
#include <iterator>
#include <limits>
#include <map>
#include <mutex>
#include <numbers>
#include <optional>
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
  if (!(model.play > 0 && model.chamfer >= 0 && half > 0 && model.chamfer < half))
    bt_te("planarScene_c: needs play > 0, chamfer >= 0 and a cube larger than its chamfer");

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

  std::vector<std::pair<double, double>> todo = { { 0.0, 1.0 } };
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
    if ((s1 - s0) * lipschitz < MIN_DISPLACEMENT || sm == s0 || sm == s1) {
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

  if (waypoints.empty())
    bt_te("certifyPath: empty path");
  for (const pose_t & p : waypoints)
    if (!std::isfinite(p.theta) || !std::isfinite(p.du) || !std::isfinite(p.dv))
      bt_te("certifyPath: non-finite pose");

  verdict_e v = PATH_CLEAR;

  if (waypoints.size() == 1)
    return (clearance(waypoints[0]) < 0) ? PATH_BLOCKED : PATH_CLEAR;

  for (size_t i = 0; i + 1 < waypoints.size(); i++)
    if (!certifySegment(waypoints[i], waypoints[i + 1], &v))
      return v;

  return PATH_CLEAR;
}

verdict_e planarScene_c::certifyQuarterTurn(unsigned int sense) const {
  return certifyPath(makePath({ PATH_PURE, sense, 0, 0, 0 }));
}

path_t makePath(const pathSpec_t & spec) {

  if (spec.sense > 1 || (spec.kind != PATH_PURE && spec.kind != PATH_WIGGLE && spec.kind != PATH_SLIDE))
    bt_te("makePath: sense must be 0 or 1, kind one of pathKind_e");

  const double end = (spec.sense == 0) ? std::numbers::pi / 2 : -std::numbers::pi / 2;

  if (spec.kind == PATH_PURE)
    return { { 0, 0, 0 }, { end, 0, 0 } };

  if ((spec.dirU == 0 && spec.dirV == 0) || std::abs(spec.dirU) > 1 || std::abs(spec.dirV) > 1 ||
      !std::isfinite(spec.amount) || !(spec.amount > 0))
    bt_te("makePath: shifts need a compass direction and a positive finite amount");

  const double len = std::hypot((double)spec.dirU, (double)spec.dirV);
  const double su = spec.amount * spec.dirU / len;
  const double sv = spec.amount * spec.dirV / len;

  if (spec.kind == PATH_SLIDE)
    return { { 0, 0, 0 }, { 0, su, sv }, { end, su, sv }, { end, 0, 0 } };

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

pose_t poseAt(const path_t & path, double t) {

  if (path.empty())
    bt_te("poseAt: empty path");
  if (!std::isfinite(t))
    bt_te("poseAt: non-finite parameter");
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

pathSpec_t transformSpec(const pathSpec_t & base, unsigned char xform) {
  pathSpec_t s = base;
  if (xform & 4)
    s.sense = 1 - base.sense;
  auto [du, dv] = applyOffsetTransform(xform, base.dirU, base.dirV);
  s.dirU = du;
  s.dirV = dv;
  return s;
}

}

pathTable_c::pathTable_c(const model_t & model, int r, const pathSpec_t & s) :
  radius(r),
  spec(s)
{
  if (r < 1)
    bt_te("pathTable_c: radius must be at least 1");

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
  if (reach > std::numeric_limits<int16_t>::max())
    bt_te("pathTable_c: radius too large");

  const int evenW = 2 * r + 1;
  const int oddW = 2 * r;
  const size_t numSlots = (size_t)evenW * evenW + (size_t)oddW * oddW;
  offsets.resize(numSlots + 1);

  auto fillSlot = [&](size_t idx, int du, int dv, int parity) {
    offsets[idx] = (uint32_t)entries.size();
    /* place the pivot at the origin's centre or corner in the Z frame,
     * where (u, v) = (x, y); the cells follow from the doubled offsets */
    const cell_t moving = { (du + parity) / 2, (dv + parity) / 2, 0 };

    for (int su = -reach + parity; su <= reach; su += 2)
      for (int sv = -reach + parity; sv <= reach; sv += 2) {
        if (su == du && sv == dv)
          continue;
        if (quarterArcDistance(0.5 * su, 0.5 * sv, 0.5 * du, 0.5 * dv, angle > 0) > hitRange)
          continue;
        const cell_t stat = { (su + parity) / 2, (sv + parity) / 2, 0 };
        planarScene_c scene({ moving }, { stat }, 2, parity, parity, 0, model);
        if (scene.certifyPath(path) != PATH_CLEAR)
          entries.push_back({ (int16_t)su, (int16_t)sv });
      }
  };

  size_t idx = 0;
  for (int eu = 0; eu < evenW; eu++)
    for (int ev = 0; ev < evenW; ev++)
      fillSlot(idx++, 2 * eu - 2 * r, 2 * ev - 2 * r, 0);
  for (int ou = 0; ou < oddW; ou++)
    for (int ov = 0; ov < oddW; ov++)
      fillSlot(idx++, 2 * ou - 2 * r + 1, 2 * ov - 2 * r + 1, 1);
  offsets[numSlots] = (uint32_t)entries.size();
  entries.shrink_to_fit();
}

unsigned int pathTable_c::slot(int du, int dv) const {
  const int evenW = 2 * radius + 1;
  if ((du & 1) == 0) {
    int eu = (du + 2 * radius) >> 1;
    int ev = (dv + 2 * radius) >> 1;
    return (unsigned int)(eu * evenW + ev);
  }
  const int oddW = 2 * radius;
  int ou = (du + 2 * radius - 1) >> 1;
  int ov = (dv + 2 * radius - 1) >> 1;
  return (unsigned int)(evenW * evenW + ou * oddW + ov);
}

bool pathTable_c::covers(int du, int dv) const {
  return du >= -2 * radius && du <= 2 * radius && dv >= -2 * radius && dv <= 2 * radius;
}

blockerRange_c pathTable_c::blockersTransformed(int du, int dv, unsigned char xform) const {
  const unsigned int s = slot(du, dv);
  const auto * data = entries.data();
  return blockerRange_c(data + offsets[s], data + offsets[s + 1], xform);
}

blockerRange_c pathTable_c::blockers(int du, int dv) const {
  if (!covers(du, dv) || ((du ^ dv) & 1))
    bt_te("pathTable_c::blockers: offset outside the table or on an edge pivot");
  return blockersTransformed(du, dv, 0);
}

bool pathTable_c::blocked(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                          unsigned int axis, int pivotHx, int pivotHy, int pivotHz) const {
  return pathTableView_c(*this).blocked(moving, stat, axis, pivotHx, pivotHy, pivotHz);
}

blockerRange_c pathTableView_c::blockers(int du, int dv) const {
  if (!base->covers(du, dv) || ((du ^ dv) & 1))
    bt_te("pathTable_c::blockers: offset outside the table or on an edge pivot");
  auto [bu, bv] = applyOffsetTransform(inverseOffsetTransform(xform), du, dv);
  return base->blockersTransformed(bu, bv, xform);
}

bool pathTableView_c::blocked(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                              unsigned int axis, int pivotHx, int pivotHy, int pivotHz) const {
  if (axis > 2)
    bt_te("pathTable_c::blocked: axis must be 0 to 2");
  for (const cell_t & c : moving) {
    int u, v;
    doubledOffset(c, axis, pivotHx, pivotHy, pivotHz, &u, &v);
    if (!base->covers(u, v) || ((u ^ v) & 1))
      bt_te("pathTable_c::blocked: offset outside the table or on an edge pivot");
  }
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

rotationTables_c::rotationTables_c(const model_t & model, int r, const std::vector<double> & amounts) :
  radius(r)
{
  for (double a : amounts)
    if (!std::isfinite(a) || !(a > 0))
      bt_te("rotationTables_c: shift amounts must be positive and finite");

  /* the tables computed with the checker: the pure turn and, per amount
   * and kind, directions (1,0) and (1,1); they are independent, so they
   * are built in parallel */
  std::vector<pathSpec_t> specs = { { PATH_PURE, 0, 0, 0, 0 } };
  std::vector<double> ascending = amounts;
  std::sort(ascending.begin(), ascending.end());
  ascending.erase(std::unique(ascending.begin(), ascending.end()), ascending.end());
  for (double amount : ascending)
    for (pathKind_e kind : { PATH_WIGGLE, PATH_SLIDE }) {
      specs.push_back({ kind, 0, 1, 0, amount });
      specs.push_back({ kind, 0, 1, 1, amount });
    }

  std::vector<std::optional<pathTable_c>> built(specs.size());
  std::exception_ptr firstError;
  {
    std::atomic<size_t> next{0};
    std::atomic<bool> failed{false};
    std::mutex errorMutex;
    auto work = [&]() {
      try {
        for (size_t i; !failed.load(std::memory_order_relaxed) &&
                       (i = next.fetch_add(1, std::memory_order_relaxed)) < specs.size(); )
          built[i].emplace(model, r, specs[i]);
      } catch (...) {
        failed.store(true, std::memory_order_relaxed);
        std::lock_guard<std::mutex> lock(errorMutex);
        if (!firstError)
          firstError = std::current_exception();
      }
    };
    const unsigned int threads = std::max(1u, std::min<unsigned int>(std::thread::hardware_concurrency(), specs.size()));
    std::vector<std::jthread> pool;
    for (unsigned int t = 1; t < threads; t++)
      pool.emplace_back(work);
    work();
  }
  if (firstError)
    std::rethrow_exception(firstError);

  baseTables.reserve(built.size());
  for (auto & b : built)
    baseTables.push_back(std::move(*b));

  paths[0].push_back({ 0, 0, baseTables[0].getSpec() });

  size_t base = 1;
  for (size_t a = 0; a < ascending.size(); a++)
    for (int kind = 0; kind < 2; kind++, base += 2) {
      /* the 8 compass directions counter-clockwise from (1,0) alternate
       * between turned views of (1,0) and of (1,1) */
      const uint16_t axisIdx = (uint16_t)base;
      const uint16_t diagIdx = (uint16_t)(base + 1);
      for (unsigned char k = 0; k < 4; k++) {
        paths[0].push_back({ axisIdx, k, transformSpec(baseTables[axisIdx].getSpec(), k) });
        paths[0].push_back({ diagIdx, k, transformSpec(baseTables[diagIdx].getSpec(), k) });
      }
    }

  paths[1].reserve(paths[0].size());
  for (const pathEntry_t & e : paths[0]) {
    unsigned char xf = e.xform | 4;
    paths[1].push_back({ e.baseIdx, xf, transformSpec(baseTables[e.baseIdx].getSpec(), xf) });
  }

  /* sense-0 blockers: those blocking every path go into blockedByAll,
   * partial blockers go into pairOffset/pairBits (most-blocking first);
   * sense 1 mirrors across the u axis */
  const int evenW = 2 * r + 1;
  const int oddW = 2 * r;
  const size_t numSlots = (size_t)evenW * evenW + (size_t)oddW * oddW;
  const size_t numPaths = paths[0].size();
  words = (unsigned int)((numPaths + 63) / 64);
  blockedByAllOffsets.resize(numSlots + 1);
  pairStart.assign(numSlots + 1, 0);

  auto fillSlotData = [&](size_t s, int du, int dv) {
    blockedByAllOffsets[s] = (uint32_t)blockedByAllEntries.size();
    std::map<std::pair<int16_t, int16_t>, std::vector<uint64_t>> byOff;
    for (size_t i = 0; i < numPaths; i++)
      for (const auto & [su, sv] : table(0, (unsigned int)i).blockers(du, dv)) {
        std::vector<uint64_t> & b = byOff[{ (int16_t)su, (int16_t)sv }];
        if (b.empty())
          b.assign(words, 0);
        b[i / 64] |= uint64_t(1) << (i % 64);
      }
    std::vector<std::pair<size_t, std::pair<int16_t, int16_t>>> partial;
    for (const auto & [off, b] : byOff) {
      size_t count = 0;
      for (uint64_t w : b)
        count += (size_t)std::popcount(w);
      if (count == numPaths) {
        blockedByAllEntries.push_back(off);
        inverseReach = std::max({ inverseReach, std::abs((int)off.first - du), std::abs((int)off.second - dv) });
      } else
        partial.push_back({ count, off });
    }
    std::stable_sort(partial.begin(), partial.end(), [](const auto & a, const auto & b) { return a.first > b.first; });
    for (const auto & p : partial) {
      pairOffset.push_back(p.second);
      const std::vector<uint64_t> & b = byOff[p.second];
      pairBits.insert(pairBits.end(), b.begin(), b.end());
    }
    pairStart[s + 1] = (uint32_t)pairOffset.size();
  };

  size_t s = 0;
  for (int eu = 0; eu < evenW; eu++)
    for (int ev = 0; ev < evenW; ev++)
      fillSlotData(s++, 2 * eu - 2 * r, 2 * ev - 2 * r);
  for (int ou = 0; ou < oddW; ou++)
    for (int ov = 0; ov < oddW; ov++)
      fillSlotData(s++, 2 * ou - 2 * r + 1, 2 * ov - 2 * r + 1);
  blockedByAllOffsets[numSlots] = (uint32_t)blockedByAllEntries.size();
  blockedByAllEntries.shrink_to_fit();
  pairOffset.shrink_to_fit();
  pairBits.shrink_to_fit();

  /* the inverse for sense 0: static cube relative to moving cube -> moving
   * cube offsets; (su-du, sv-dv) are always even, so index only even offsets */
  const int invHalf = inverseReach / 2;
  const int iw = 2 * invHalf + 1;
  const size_t invSlots = (size_t)iw * iw;
  std::vector<uint32_t> counts(invSlots, 0);
  auto invSlotOf = [&](int ddu, int ddv) -> size_t {
    return (size_t)((ddu >> 1) + invHalf) * iw + ((ddv >> 1) + invHalf);
  };
  auto visitCommon = [&](auto && fn) {
    size_t idx = 0;
    for (int eu = 0; eu < evenW; eu++)
      for (int ev = 0; ev < evenW; ev++, idx++) {
        int du = 2 * eu - 2 * r, dv = 2 * ev - 2 * r;
        for (uint32_t k = blockedByAllOffsets[idx]; k < blockedByAllOffsets[idx + 1]; k++)
          fn(du, dv, blockedByAllEntries[k].first, blockedByAllEntries[k].second);
      }
    for (int ou = 0; ou < oddW; ou++)
      for (int ov = 0; ov < oddW; ov++, idx++) {
        int du = 2 * ou - 2 * r + 1, dv = 2 * ov - 2 * r + 1;
        for (uint32_t k = blockedByAllOffsets[idx]; k < blockedByAllOffsets[idx + 1]; k++)
          fn(du, dv, blockedByAllEntries[k].first, blockedByAllEntries[k].second);
      }
  };
  visitCommon([&](int du, int dv, int su, int sv) {
    counts[invSlotOf(su - du, sv - dv)]++;
  });
  inverseOffsets.resize(invSlots + 1);
  uint32_t total = 0;
  for (size_t i = 0; i < invSlots; i++) {
    inverseOffsets[i] = total;
    total += counts[i];
  }
  inverseOffsets[invSlots] = total;
  inverseEntries.resize(total);
  std::vector<uint32_t> cursor = inverseOffsets;
  visitCommon([&](int du, int dv, int su, int sv) {
    const size_t slot = invSlotOf(su - du, sv - dv);
    inverseEntries[cursor[slot]++] = { (int16_t)du, (int16_t)dv };
  });
}

blockerRange_c rotationTables_c::pivotsBlockedAt(unsigned int sense, int du, int dv) const {
  if (sense > 1)
    bt_te("rotationTables_c::pivotsBlockedAt: sense must be 0 or 1");
  if ((du & 1) || (dv & 1) || std::abs(du) > inverseReach || std::abs(dv) > inverseReach)
    return {};
  const unsigned char xf = sense ? 4 : 0;
  const int bdv = sense ? -dv : dv;
  const int invHalf = inverseReach / 2;
  const int iw = 2 * invHalf + 1;
  const size_t slot = (size_t)((du >> 1) + invHalf) * iw + ((bdv >> 1) + invHalf);
  const auto * data = inverseEntries.data();
  return blockerRange_c(data + inverseOffsets[slot], data + inverseOffsets[slot + 1], xf);
}

pathTableView_c rotationTables_c::table(unsigned int sense, unsigned int idx) const {
  if (sense > 1 || idx >= paths[sense].size())
    bt_te("rotationTables_c::table: no such path");
  const pathEntry_t & e = paths[sense][idx];
  return pathTableView_c(&baseTables[e.baseIdx], &e.spec, e.xform);
}

blockerRange_c rotationTables_c::blockedOnAllPaths(unsigned int sense, int du, int dv) const {
  const pathTable_c & first = baseTables[0];
  if (sense > 1 || !first.covers(du, dv) || ((du ^ dv) & 1))
    bt_te("rotationTables_c::blockedOnAllPaths: offset outside the tables or on an edge pivot");
  const unsigned char xf = sense ? 4 : 0;
  const int bdv = sense ? -dv : dv;
  const unsigned int s = first.slot(du, bdv);
  const auto * data = blockedByAllEntries.data();
  return blockerRange_c(data + blockedByAllOffsets[s], data + blockedByAllOffsets[s + 1], xf);
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

}
