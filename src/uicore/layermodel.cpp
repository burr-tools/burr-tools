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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include "layermodel.h"

#include "../lib/latticegridinfo.h"
#include "../lib/symmetries.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace btui {

  namespace {

    constexpr double kEps = 1e-6;
    // heights closer than this are one layer (the corners are floats)
    constexpr double kSameHeight = 1e-4;

    struct Vec3 { double x, y, z; };

    /* a cell: its corners' edges and its extent in z */
    struct cell_s {
      std::array<int, 3> voxel;
      std::vector<std::pair<Vec3, Vec3>> edges;
      double zlo, zhi;
    };

    double cross(Point2 o, Point2 a, Point2 b) {
      return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
    }

    /* the convex hull, counter-clockwise, without collinear points */
    std::vector<Point2> hull(std::vector<Point2> p) {
      std::sort(p.begin(), p.end(), [](Point2 a, Point2 b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
      if (p.size() < 3) return {};
      std::vector<Point2> h(2 * p.size());
      size_t k = 0;
      for (size_t i = 0; i < p.size(); i++) {
        while (k >= 2 && cross(h[k - 2], h[k - 1], p[i]) <= kEps) k--;
        h[k++] = p[i];
      }
      for (size_t i = p.size() - 1, t = k + 1; i-- > 0;) {
        while (k >= t && cross(h[k - 2], h[k - 1], p[i]) <= kEps) k--;
        h[k++] = p[i];
      }
      h.resize(k > 0 ? k - 1 : 0);
      if (h.size() < 3) return {};
      return h;
    }

    /* the cross-section of a convex cell with the plane z = h */
    std::vector<Point2> section(const cell_s & c, double h) {
      std::vector<Point2> pts;
      for (const auto & [a, b] : c.edges) {
        const double da = a.z - h, db = b.z - h;
        if (std::fabs(da) <= kEps) pts.push_back({a.x, a.y});
        if (std::fabs(db) <= kEps) pts.push_back({b.x, b.y});
        if ((da < -kEps && db > kEps) || (da > kEps && db < -kEps)) {
          const double t = da / (da - db);
          pts.push_back({a.x + t * (b.x - a.x), a.y + t * (b.y - a.y)});
        }
      }
      return hull(std::move(pts));
    }

  }

  namespace {

    // the mean of a cell's face corners: moved and mirrored with the cell
    Vec3 cellCentre(const latticeGridInfo_c & inf, int x, int y, int z) {
      std::vector<float> c;
      Vec3 m{0, 0, 0};
      size_t n = 0;
      for (unsigned int f = 0; f < inf.faces(x, y, z); f++) {
        c.clear();
        inf.faceCorners(x, y, z, f, 0, 0, c);
        for (size_t i = 0; i + 2 < c.size(); i += 3, n++) {
          m.x += c[i]; m.y += c[i+1]; m.z += c[i+2];
        }
      }
      if (n) { m.x /= n; m.y /= n; m.z /= n; }
      return m;
    }

    /* the grid's mirror planes perpendicular to axis are where its flip
     * (latticeGridInfo_c::flip) followed by a lattice translation is a true
     * reflection: v -> R v + s + T with no shift across the axis. s comes
     * from where the flip takes one cell, T from the lattice measured in
     * physical space (the cells' centres, as faceCorners() places them);
     * the plane is at (s + T)[axis] / 2. Of those, the one nearest to want;
     * NaN when there is none. */
    double nearestMirrorPlane(const latticeGridInfo_c & inf, int axis, double want) {
      const unsigned char t = inf.flip(axis);
      if (t == TND) return std::nan("");
      const std::array<int, 3> p0 = inf.basePosition(0);
      int q[3] = {p0[0], p0[1], p0[2]};
      if (!inf.apply(t, q[0], q[1], q[2])) return std::nan("");
      const Vec3 a = cellCentre(inf, p0[0], p0[1], p0[2]);
      const Vec3 b = cellCentre(inf, q[0], q[1], q[2]);
      double ra[3] = {a.x, a.y, a.z};
      ra[axis] = -ra[axis];
      const double s[3] = {b.x - ra[0], b.y - ra[1], b.z - ra[2]};
      double P[3][3];  // P[j]: lattice column j in physical space
      for (int j = 0; j < 3; j++) {
        int lx, ly, lz;
        inf.latticeColumn(j, lx, ly, lz);
        const Vec3 c = cellCentre(inf, p0[0] + lx, p0[1] + ly, p0[2] + lz);
        P[j][0] = c.x - a.x; P[j][1] = c.y - a.y; P[j][2] = c.z - a.z;
      }
      double best = std::nan("");
      const int R = 8;
      for (int n0 = -R; n0 <= R; n0++)
        for (int n1 = -R; n1 <= R; n1++)
          for (int n2 = -R; n2 <= R; n2++) {
            double v[3];
            for (int k = 0; k < 3; k++) v[k] = s[k] + n0 * P[0][k] + n1 * P[1][k] + n2 * P[2][k];
            bool pure = true;
            for (int k = 0; k < 3; k++)
              if (k != axis && std::fabs(v[k]) > 1e-4) pure = false;
            if (!pure) continue;
            const double c = v[axis] / 2;
            if (std::isnan(best) || std::fabs(c - want) < std::fabs(best - want) - 1e-9 ||
                (std::fabs(std::fabs(c - want) - std::fabs(best - want)) <= 1e-9 && c < best))
              best = c;
          }
      return best;
    }

  }

  double polygonArea(const std::vector<Point2> & poly) {
    double a = 0;
    for (size_t i = 0; i < poly.size(); i++) {
      const Point2 & p = poly[i];
      const Point2 & q = poly[(i + 1) % poly.size()];
      a += p.x * q.y - q.x * p.y;
    }
    return a / 2;
  }

  bool inConvexPolygon(const std::vector<Point2> & poly, Point2 p) {
    if (poly.size() < 3) return false;
    for (size_t i = 0; i < poly.size(); i++)
      if (cross(poly[i], poly[(i + 1) % poly.size()], p) < -kEps)
        return false;
    return true;
  }

  LayerModel::LayerModel(const gridType_c & gt, unsigned int sx_, unsigned int sy_, unsigned int sz_)
    : sx(sx_), sy(sy_), sz(sz_) {

    if (!gt.isDescribed() || sx == 0 || sy == 0 || sz == 0)
      return;
    const latticeGridInfo_c & inf = latticeGridInfo_c::get(gt.getType());

    std::vector<cell_s> cells;
    std::vector<float> corners;
    for (unsigned int z = 0; z < sz; z++)
      for (unsigned int y = 0; y < sy; y++)
        for (unsigned int x = 0; x < sx; x++) {
          if (!inf.isVoxel(x, y, z)) continue;
          cell_s c{{(int)x, (int)y, (int)z}, {}, std::numeric_limits<double>::max(), std::numeric_limits<double>::lowest()};
          for (unsigned int f = 0; f < inf.faces(x, y, z); f++) {
            corners.clear();
            inf.faceCorners(x, y, z, f, 0, 0, corners);
            const size_t n = corners.size() / 3;
            for (size_t i = 0; i < n; i++) {
              const size_t j = (i + 1) % n;
              const Vec3 a{corners[3 * i], corners[3 * i + 1], corners[3 * i + 2]};
              const Vec3 b{corners[3 * j], corners[3 * j + 1], corners[3 * j + 2]};
              c.edges.push_back({a, b});
              c.zlo = std::min(c.zlo, a.z);
              c.zhi = std::max(c.zhi, a.z);
            }
          }
          if (!c.edges.empty())
            cells.push_back(std::move(c));
        }
    if (cells.empty())
      return;

    // the planes: the cells' centre heights, one layer per distinct height
    std::vector<double> heights;
    for (const cell_s & c : cells)
      heights.push_back((c.zlo + c.zhi) / 2);
    std::sort(heights.begin(), heights.end());
    for (double h : heights)
      if (layers.empty() || h - layers.back().height > kSameHeight)
        layers.push_back({h, {}});

    home.assign(static_cast<size_t>(sx) * sy * sz, -1);
    boxLo = {std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};
    boxHi = {std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest()};

    for (const cell_s & c : cells) {
      const double centre = (c.zlo + c.zhi) / 2;
      // the layers whose planes cut the cell
      auto first = std::lower_bound(layers.begin(), layers.end(), c.zlo + kEps,
                                    [](const layer_s & l, double h) { return l.height < h; });
      for (auto it = first; it != layers.end() && it->height < c.zhi - kEps; ++it) {
        std::vector<Point2> outline = section(c, it->height);
        if (outline.empty()) continue;
        for (const Point2 & p : outline) {
          boxLo = {std::min(boxLo.x, p.x), std::min(boxLo.y, p.y)};
          boxHi = {std::max(boxHi.x, p.x), std::max(boxHi.y, p.y)};
        }
        it->cells.push_back({c.voxel, std::move(outline)});
        if (std::fabs(it->height - centre) <= kSameHeight)
          home[c.voxel[0] + static_cast<size_t>(sx) * (c.voxel[1] + static_cast<size_t>(sy) * c.voxel[2])] =
              static_cast<int>(it - layers.begin());
      }
    }

    // the mirror planes nearest the middle of the drawing (see mirrorPlane())
    planeX = nearestMirrorPlane(inf, 0, (boxLo.x + boxHi.x) / 2);
    planeY = nearestMirrorPlane(inf, 1, (boxLo.y + boxHi.y) / 2);
  }

  int LayerModel::hit(unsigned int l, Point2 p) const {
    if (l >= layers.size()) return -1;
    const std::vector<LayerCell> & cs = layers[l].cells;
    for (size_t i = 0; i < cs.size(); i++)
      if (inConvexPolygon(cs[i].outline, p))
        return static_cast<int>(i);
    return -1;
  }

  Point2 LayerModel::centre(unsigned int l, unsigned int i) const {
    if (l >= layers.size() || i >= layers[l].cells.size()) return {};
    const std::vector<Point2> & poly = layers[l].cells[i].outline;
    // the area-weighted centroid of the polygon
    double a = 0, cx = 0, cy = 0;
    for (size_t k = 0; k < poly.size(); k++) {
      const Point2 & p = poly[k];
      const Point2 & q = poly[(k + 1) % poly.size()];
      const double c = p.x * q.y - q.x * p.y;
      a += c;
      cx += (p.x + q.x) * c;
      cy += (p.y + q.y) * c;
    }
    if (std::fabs(a) <= kEps) return poly.empty() ? Point2{} : poly[0];
    return {cx / (3 * a), cy / (3 * a)};
  }

  namespace {

    /* whether a convex polygon and the axis-parallel rectangle [lo, hi]
     * overlap in more than a border (separating axes: the rectangle's two and
     * the polygon's edge normals) */
    bool overlapsRect(const std::vector<Point2> & poly, Point2 lo, Point2 hi) {
      if (poly.size() < 3) return false;
      double pxl = poly[0].x, pxh = poly[0].x, pyl = poly[0].y, pyh = poly[0].y;
      for (const Point2 & p : poly) {
        pxl = std::min(pxl, p.x); pxh = std::max(pxh, p.x);
        pyl = std::min(pyl, p.y); pyh = std::max(pyh, p.y);
      }
      if (pxh <= lo.x + kEps || hi.x <= pxl + kEps) return false;
      if (pyh <= lo.y + kEps || hi.y <= pyl + kEps) return false;
      const Point2 rect[4] = {lo, {hi.x, lo.y}, hi, {lo.x, hi.y}};
      for (size_t i = 0; i < poly.size(); i++) {
        const Point2 & p = poly[i];
        const Point2 & q = poly[(i + 1) % poly.size()];
        // the polygon lies left of its edge p->q (counter-clockwise); the
        // rectangle is separated when all of it is right of the edge or on it
        const double len = std::hypot(q.x - p.x, q.y - p.y);
        if (len <= kEps) continue;
        double best = std::numeric_limits<double>::lowest();
        for (const Point2 & r : rect)
          best = std::max(best, cross(p, q, r) / len);
        if (best <= kEps) return false;
      }
      return true;
    }

  }

  bool LayerModel::hasMirror(unsigned int tool) const {
    if (tool != TOOL_MIRROR_X && tool != TOOL_MIRROR_Y) return false;
    for (unsigned int l = 0; l < layers.size(); l++)
      for (unsigned int i = 0; i < layers[l].cells.size(); i++) {
        const Point2 c = centre(l, i);
        unsigned int missing = 0;
        if (region(l, c, c, tool, &missing).size() > 1 || missing == 0) return true;
      }
    return false;
  }

  std::vector<CellRef> LayerModel::region(unsigned int l, Point2 a, Point2 b, unsigned int tools,
                                          unsigned int * missing) const {
    std::vector<CellRef> out;
    if (missing) *missing = 0;
    if (l >= layers.size()) return out;

    const Point2 lo{std::min(a.x, b.x), std::min(a.y, b.y)};
    const Point2 hi{std::max(a.x, b.x), std::max(a.y, b.y)};

    // the selected cells, as the layer and the outline of where they go, so
    // the mirrors can move them
    struct seed_s { unsigned int layer; std::vector<Point2> outline; };
    std::vector<seed_s> seeds;
    for (unsigned int i = 0; i < layers[l].cells.size(); i++)
      if (overlapsRect(layers[l].cells[i].outline, lo, hi))
        seeds.push_back({l, layers[l].cells[i].outline});

    if ((tools & TOOL_MIRROR_X) && !std::isnan(planeX))
      for (size_t i = 0, n = seeds.size(); i < n; i++) {
        seed_s m = seeds[i];
        for (Point2 & p : m.outline) p.x = 2 * planeX - p.x;
        seeds.push_back(std::move(m));
      }
    if ((tools & TOOL_MIRROR_Y) && !std::isnan(planeY))
      for (size_t i = 0, n = seeds.size(); i < n; i++) {
        seed_s m = seeds[i];
        for (Point2 & p : m.outline) p.y = 2 * planeY - p.y;
        seeds.push_back(std::move(m));
      }
    if (tools & TOOL_MIRROR_Z)
      for (size_t i = 0, n = seeds.size(); i < n; i++)
        seeds.push_back({static_cast<unsigned int>(layers.size()) - 1 - seeds[i].layer, seeds[i].outline});

    // the cell of layer m whose outline is the given one (the same corners in
    // any order), -1 for none: a mirror image counts only when it is a cell
    auto cellAt = [this](unsigned int m, const std::vector<Point2> & outline) -> int {
      Point2 c;
      for (const Point2 & p : outline) { c.x += p.x; c.y += p.y; }
      c.x /= outline.size();
      c.y /= outline.size();
      const int i = hit(m, c);
      if (i < 0) return -1;
      const std::vector<Point2> & o = layers[m].cells[i].outline;
      if (o.size() != outline.size()) return -1;
      for (const Point2 & p : outline) {
        bool found = false;
        for (const Point2 & q : o)
          found |= std::fabs(p.x - q.x) <= kSameHeight && std::fabs(p.y - q.y) <= kSameHeight;
        if (!found) return -1;
      }
      return i;
    };

    const bool stack = tools & (TOOL_STACK_X | TOOL_STACK_Y | TOOL_STACK_Z);
    for (const seed_s & s : seeds) {
      const int self = cellAt(s.layer, s.outline);
      if (self < 0) {  // a mirror image that is no cell
        if (missing) (*missing)++;
        continue;
      }
      if (!stack) {
        out.push_back({s.layer, static_cast<unsigned int>(self)});
        continue;
      }
      const Point2 c = centre(s.layer, static_cast<unsigned int>(self));
      const std::vector<LayerCell> & cs = layers[s.layer].cells;
      for (unsigned int i = 0; i < cs.size(); i++) {
        double xl = cs[i].outline[0].x, xh = xl, yl = cs[i].outline[0].y, yh = yl;
        for (const Point2 & p : cs[i].outline) {
          xl = std::min(xl, p.x); xh = std::max(xh, p.x);
          yl = std::min(yl, p.y); yh = std::max(yh, p.y);
        }
        // a convex outline is crossed by a line iff the line passes strictly
        // between its extremes
        if (((tools & TOOL_STACK_X) && yl < c.y - kEps && c.y + kEps < yh) ||
            ((tools & TOOL_STACK_Y) && xl < c.x - kEps && c.x + kEps < xh))
          out.push_back({s.layer, i});
      }
      if (tools & TOOL_STACK_Z)
        for (unsigned int m = 0; m < layers.size(); m++) {
          const int i = hit(m, c);
          if (i >= 0) out.push_back({m, static_cast<unsigned int>(i)});
        }
    }

    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
  }

  int LayerModel::layerOf(int x, int y, int z) const {
    if (x < 0 || y < 0 || z < 0 || (unsigned)x >= sx || (unsigned)y >= sy || (unsigned)z >= sz || home.empty())
      return -1;
    return home[x + static_cast<size_t>(sx) * (y + static_cast<size_t>(sy) * z)];
  }

}
