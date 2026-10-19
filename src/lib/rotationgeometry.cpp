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
#include <limits>
#include <map>
#include <numbers>
#include <set>
#include <tuple>

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

double segmentPointDistance(double px, double py,
                            double ax, double ay, double bx, double by) {
  double dx = bx - ax, dy = by - ay;
  double t = ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy);
  t = std::clamp(t, 0.0, 1.0);
  double ex = ax + t * dx - px, ey = ay + t * dy - py;
  return std::sqrt(ex * ex + ey * ey);
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

  double dist = std::numeric_limits<double>::max();
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++) {
      int j2 = (j + 1) & 3;
      dist = std::min(dist, segmentPointDistance(sp[i][0], sp[i][1], mp[j][0], mp[j][1], mp[j2][0], mp[j2][1]));
      dist = std::min(dist, segmentPointDistance(mp[i][0], mp[i][1], sp[j][0], sp[j][1], sp[j2][0], sp[j2][1]));
    }
  return dist;
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
        /* the squares' circumcircles bound the signed distance from below */
        if (std::sqrt(du * du + dv * dv) - reach - 2 * rounding >= best)
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

namespace {

/* Doubled in-plane offset of a cell from a doubled pivot, same frames as inPlane. */
void doubledOffset(const cell_t & c, unsigned int axis, int phx, int phy, int phz, int * u, int * v) {
  int dx = 2 * c.x - phx, dy = 2 * c.y - phy, dz = 2 * c.z - phz;
  if (axis == 0)      { *u = dy; *v = dz; }
  else if (axis == 1) { *u = dz; *v = dx; }
  else                { *u = dx; *v = dy; }
}

}

quarterTurnTable_c::quarterTurnTable_c(const model_t & model, int r) :
  radius(r),
  width(4 * r + 1)
{
  if (r < 1)
    bt_te("quarterTurnTable_c: radius must be at least 1");

  /* a cube's corners stay within 1.5 cells of its centre path, so the static
   * cubes that can be hit lie within two more cells */
  const int reach = 2 * (r + 2);

  for (unsigned int sense = 0; sense < 2; sense++) {
    table[sense].resize(width * width);

    for (int du = -2 * r; du <= 2 * r; du++)
      for (int dv = -2 * r; dv <= 2 * r; dv++) {
        int parity = du & 1;
        if ((dv & 1) != parity)
          continue;

        /* place the pivot at the origin's centre or corner in the Z frame,
         * where (u, v) = (x, y); the cells follow from the doubled offsets */
        const cell_t moving = { (du + parity) / 2, (dv + parity) / 2, 0 };
        std::vector<std::pair<int, int>> & entry = table[sense][index(du, dv)];

        for (int su = -reach + parity; su <= reach; su += 2)
          for (int sv = -reach + parity; sv <= reach; sv += 2) {
            if (su == du && sv == dv)
              continue;
            const cell_t stat = { (su + parity) / 2, (sv + parity) / 2, 0 };
            planarScene_c scene({ moving }, { stat }, 2, parity, parity, 0, model);
            if (scene.certifyQuarterTurn(sense) != PATH_CLEAR)
              entry.push_back({ su, sv });
          }
      }
  }
}

unsigned int quarterTurnTable_c::index(int du, int dv) const {
  return (unsigned int)((du + 2 * radius) * width + dv + 2 * radius);
}

bool quarterTurnTable_c::covers(int du, int dv) const {
  return du >= -2 * radius && du <= 2 * radius && dv >= -2 * radius && dv <= 2 * radius;
}

const std::vector<std::pair<int, int>> & quarterTurnTable_c::blockers(int du, int dv, unsigned int sense) const {
  if (sense > 1 || !covers(du, dv) || ((du ^ dv) & 1))
    bt_te("quarterTurnTable_c::blockers: offset outside the table or on an edge pivot");
  return table[sense][index(du, dv)];
}

bool quarterTurnTable_c::blocked(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                                 unsigned int axis, int pivotHx, int pivotHy, int pivotHz,
                                 unsigned int sense) const {

  bt_assert(axis < 3);

  std::set<std::tuple<int, int, int>> occupied;
  for (const cell_t & c : stat) {
    int u, v;
    doubledOffset(c, axis, pivotHx, pivotHy, pivotHz, &u, &v);
    occupied.insert({ (int)axial(c, axis), u, v });
  }

  for (const cell_t & c : moving) {
    int u, v;
    doubledOffset(c, axis, pivotHx, pivotHy, pivotHz, &u, &v);
    int layer = (int)axial(c, axis);
    for (const auto & [su, sv] : blockers(u, v, sense))
      if (occupied.count({ layer, su, sv }))
        return true;
  }

  return false;
}

}
