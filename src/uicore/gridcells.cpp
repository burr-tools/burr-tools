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
#include "gridcells.h"

#include "../lib/voxel.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace btui {

  namespace {

    constexpr double kTriHeight = 0.8660254037844386;   // sqrt(3)/2
    constexpr double kSphereR = 0.7071067811865476;      // touching the diagonal neighbours

    // floor division, as legacy's intdiv_inf
    int floorDiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

    void ccw(std::vector<Pt> & poly) {
      double area = 0;
      for (size_t i = 0; i < poly.size(); i++) {
        const Pt & a = poly[i];
        const Pt & b = poly[(i + 1) % poly.size()];
        area += a.x * b.y - b.x * a.y;
      }
      if (area < 0)
        std::reverse(poly.begin(), poly.end());
    }

    /* a prism layer's triangle (legacy gridEditor_1_c::drawNormalTile) */
    std::vector<Pt> prismTriangle(int x, int y) {
      const double x1 = x / 2.0;
      if ((x + y) & 1)      // base at the top
        return { { x1, (y + 1) * kTriHeight }, { x1 + 1, (y + 1) * kTriHeight }, { x1 + 0.5, y * kTriHeight } };
      return { { x1, y * kTriHeight }, { x1 + 1, y * kTriHeight }, { x1 + 0.5, (y + 1) * kTriHeight } };
    }

    /* a rhombic layer's triangle (legacy gridEditor_3: getTriangle) */
    std::vector<Pt> rhombicTriangle(int x, int y, int z) {
      const int xc = floorDiv(x, 5), yc = floorDiv(y, 5);
      x -= 5 * xc;
      y -= 5 * yc;
      z -= 5 * floorDiv(z, 5);
      const double X = xc, Y = yc, h = 0.5;
      if (z == 2) {
        if (x == 0 && y == 1) return { { X + h, Y + h }, { X, Y + h }, { X, Y } };
        if (x == 0 && y == 3) return { { X + h, Y + h }, { X, Y + h }, { X, Y + 1 } };
        if (x == 1 && y == 0) return { { X + h, Y + h }, { X + h, Y }, { X, Y } };
        if (x == 1 && y == 4) return { { X + h, Y + h }, { X + h, Y + 1 }, { X, Y + 1 } };
        if (x == 3 && y == 0) return { { X + h, Y + h }, { X + h, Y }, { X + 1, Y } };
        if (x == 3 && y == 4) return { { X + h, Y + h }, { X + h, Y + 1 }, { X + 1, Y + 1 } };
        if (x == 4 && y == 1) return { { X + h, Y + h }, { X + 1, Y + h }, { X + 1, Y } };
        if (x == 4 && y == 3) return { { X + h, Y + h }, { X + 1, Y + h }, { X + 1, Y + 1 } };
        return {};
      }
      if (x < 2) return { { X, Y }, { X + h, Y + h }, { X, Y + 1 } };
      if (x > 2) return { { X + 1, Y }, { X + h, Y + h }, { X + 1, Y + 1 } };
      if (y < 2) return { { X, Y }, { X + h, Y + h }, { X + 1, Y } };
      return { { X, Y + 1 }, { X + h, Y + h }, { X + 1, Y + 1 } };
    }

    /* a tetra-octa layer's triangle or square (legacy gridEditor_4: getPolygon) */
    std::vector<Pt> tetraOctaPolygon(int x, int y, int z) {
      const int xc = floorDiv(x, 3), yc = floorDiv(y, 3);
      x -= 3 * xc;
      y -= 3 * yc;
      z -= 3 * floorDiv(z, 3);
      const double X = xc, Y = yc;
      if (z == 1)
        return { { X, Y }, { X + 1, Y }, { X + 1, Y + 1 }, { X, Y + 1 } };
      if (x == 0 && y == 0) return { { X, Y }, { X + 1, Y }, { X, Y + 1 } };
      if (x == 2 && y == 0) return { { X + 1, Y }, { X + 1, Y + 1 }, { X, Y } };
      if (x == 0 && y == 2) return { { X, Y + 1 }, { X, Y }, { X + 1, Y + 1 } };
      if (x == 2 && y == 2) return { { X + 1, Y + 1 }, { X, Y + 1 }, { X + 1, Y } };
      return {};
    }

    /* the unit squares a cell's bounds reach, clamped to the layout's: both
     * ends inclusive, so a cell is found from either side of an edge it is
     * on, and a little padded, as insideConvex's tolerance is */
    struct Span { int x0, y0, x1, y1; };
    Span spanOf(const GridLayout & l, const GridCell & c) {
      constexpr double pad = 1e-6;
      double x0 = c.centre.x - c.radius, x1 = c.centre.x + c.radius;
      double y0 = c.centre.y - c.radius, y1 = c.centre.y + c.radius;
      if (!c.round) {
        x0 = y0 = std::numeric_limits<double>::max();
        x1 = y1 = std::numeric_limits<double>::lowest();
        for (const Pt & q : c.outline) {
          x0 = std::min(x0, q.x); x1 = std::max(x1, q.x);
          y0 = std::min(y0, q.y); y1 = std::max(y1, q.y);
        }
      }
      auto bx = [&](double v) { return std::clamp(int(std::floor(v)), 0, l.bucketCols - 1); };
      auto by = [&](double v) { return std::clamp(int(std::floor(v)), 0, l.bucketRows - 1); };
      return { bx(x0 - pad), by(y0 - pad), bx(x1 + pad), by(y1 + pad) };
    }

    /* GridLayout's buckets, counted then filled (in rising cell order) */
    void index(GridLayout & l) {
      l.bucketCols = std::max(1, int(std::ceil(l.width)));
      l.bucketRows = std::max(1, int(std::ceil(l.height)));
      const size_t n = size_t(l.bucketCols) * size_t(l.bucketRows);
      l.bucketStart.assign(n + 1, 0);
      std::vector<Span> spans;
      spans.reserve(l.cells.size());
      for (const GridCell & c : l.cells) {
        const Span s = spanOf(l, c);
        spans.push_back(s);
        for (int y = s.y0; y <= s.y1; y++)
          for (int x = s.x0; x <= s.x1; x++)
            l.bucketStart[size_t(y * l.bucketCols + x) + 1]++;
      }
      for (size_t i = 0; i < n; i++)
        l.bucketStart[i + 1] += l.bucketStart[i];
      l.bucketCells.resize(size_t(l.bucketStart[n]));
      std::vector<int> fill(l.bucketStart.begin(), l.bucketStart.end() - 1);
      for (size_t i = 0; i < spans.size(); i++) {
        const Span & s = spans[i];
        for (int y = s.y0; y <= s.y1; y++)
          for (int x = s.x0; x <= s.x1; x++)
            l.bucketCells[size_t(fill[size_t(y * l.bucketCols + x)]++)] = int(i);
      }
    }

    bool insideConvex(const std::vector<Pt> & poly, Pt p) {
      if (poly.size() < 3)
        return false;
      for (size_t i = 0; i < poly.size(); i++) {
        const Pt & a = poly[i];
        const Pt & b = poly[(i + 1) % poly.size()];
        if ((b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x) < -1e-9)
          return false;
      }
      return true;
    }
  }

  GridLayout gridLayout(const voxel_c & shape, Plane p, int layer) {
    GridLayout out;
    const gridType_c::gridType type = shape.getGridType()->getType();
    if (!planeAvailable(type, p))
      return out;
    const PlaneAxes a = planeAxes(p);
    const int d[3] = { int(shape.getX()), int(shape.getY()), int(shape.getZ()) };
    const int cols = d[a.col], rows = d[a.row];
    if (cols <= 0 || rows <= 0 || layer < 0 || layer >= d[a.layer])
      return out;
    out.cols = cols;
    out.rows = rows;

    switch (type) {
      case gridType_c::GT_BRICKS:
        out.squares = true;
        out.width = cols;
        out.height = rows;
        break;
      case gridType_c::GT_TRIANGULAR_PRISM:
        out.width = (cols + 1) / 2.0;
        out.height = rows * kTriHeight;
        break;
      case gridType_c::GT_SPHERES:
        out.width = cols - 1 + 2 * kSphereR;
        out.height = rows - 1 + 2 * kSphereR;
        break;
      case gridType_c::GT_RHOMBIC:
        out.width = (cols + 4) / 5;
        out.height = (rows + 4) / 5;
        break;
      case gridType_c::GT_TETRA_OCTA:
        out.width = (cols + 2) / 3;
        out.height = (rows + 2) / 3;
        break;
      case gridType_c::GT_NUM_GRIDS:
        return GridLayout{};
    }

    for (int r = 0; r < rows; r++)
      for (int c = 0; c < cols; c++) {
        const Cell3 v = voxelAt(p, c, r, layer);
        if (!shape.validCoordinate(v.x, v.y, v.z))
          continue;
        GridCell cell;
        cell.voxel = v;
        cell.col = c;
        cell.row = r;
        switch (type) {
          case gridType_c::GT_BRICKS:
            cell.outline = { { double(c), double(r) }, { c + 1.0, double(r) }, { c + 1.0, r + 1.0 }, { double(c), r + 1.0 } };
            break;
          case gridType_c::GT_TRIANGULAR_PRISM:
            cell.outline = prismTriangle(c, r);
            break;
          case gridType_c::GT_SPHERES: {
            cell.round = true;
            cell.radius = kSphereR;
            cell.centre = { c + kSphereR, r + kSphereR };
            for (int i = 0; i < 24; i++) {
              const double t = 2 * std::numbers::pi * i / 24;
              cell.outline.push_back({ cell.centre.x + kSphereR * std::cos(t), cell.centre.y + kSphereR * std::sin(t) });
            }
            break;
          }
          // drawn by the column, row and layer: the grids are alike along
          // every axis, so the XY drawing is every plane's (planeAvailable)
          case gridType_c::GT_RHOMBIC:
            cell.outline = rhombicTriangle(c, r, layer);
            break;
          case gridType_c::GT_TETRA_OCTA:
            cell.outline = tetraOctaPolygon(c, r, layer);
            break;
          case gridType_c::GT_NUM_GRIDS:
            break;
        }
        if (cell.outline.size() < 3)
          continue;
        ccw(cell.outline);
        out.cells.push_back(std::move(cell));
      }
    index(out);
    return out;
  }

  Pt cellCentre(const GridCell & c) {
    if (c.round)
      return c.centre;
    Pt m{ 0, 0 };
    for (const Pt & q : c.outline) {
      m.x += q.x;
      m.y += q.y;
    }
    const double n = double(std::max<size_t>(1, c.outline.size()));
    return { m.x / n, m.y / n };
  }

  int stepCell(const GridLayout & layout, int from, double dx, double dy) {
    if (from < 0 || size_t(from) >= layout.cells.size())
      return -1;
    const double len = std::hypot(dx, dy);
    if (len <= 0)
      return -1;
    dx /= len;
    dy /= len;
    const Pt o = cellCentre(layout.cells[size_t(from)]);
    int best = -1;
    double bestScore = 0;
    for (size_t i = 0; i < layout.cells.size(); i++) {
      if (int(i) == from)
        continue;
      const Pt c = cellCentre(layout.cells[i]);
      const double vx = c.x - o.x, vy = c.y - o.y;
      const double d = std::hypot(vx, vy);
      if (d < 1e-9)
        continue;
      const double cosine = (vx * dx + vy * dy) / d;
      if (cosine < 0.5)                    // outside 60 degrees
        continue;
      // near and straight: a step along the row beats a diagonal one
      const double score = d / (cosine * cosine);
      if (best < 0 || score < bestScore - 1e-9) {
        best = int(i);
        bestScore = score;
      }
    }
    return best;
  }

  int nearestCell(const GridLayout & layout, Pt p) {
    int best = -1;
    double bestD = 0;
    for (size_t i = 0; i < layout.cells.size(); i++) {
      const Pt c = cellCentre(layout.cells[i]);
      const double d = std::hypot(c.x - p.x, c.y - p.y);
      if (best < 0 || d < bestD) {
        best = int(i);
        bestD = d;
      }
    }
    return best;
  }

  int cellAt(const GridLayout & layout, Pt p) {
    // the unit square's cells only (GridLayout's buckets); the first that
    // holds the point, as a scan of every cell in order would find
    if (layout.cells.empty() || layout.bucketStart.empty() || !std::isfinite(p.x) || !std::isfinite(p.y))
      return -1;
    constexpr double pad = 1e-6;
    if (p.x < -pad || p.y < -pad || p.x > layout.bucketCols + pad || p.y > layout.bucketRows + pad)
      return -1;
    const int bx = std::clamp(int(std::floor(p.x)), 0, layout.bucketCols - 1);
    const int by = std::clamp(int(std::floor(p.y)), 0, layout.bucketRows - 1);
    const size_t b = size_t(by * layout.bucketCols + bx);
    for (int k = layout.bucketStart[b]; k < layout.bucketStart[b + 1]; k++) {
      const int i = layout.bucketCells[size_t(k)];
      const GridCell & c = layout.cells[size_t(i)];
      if (c.round) {
        const double dx = p.x - c.centre.x, dy = p.y - c.centre.y;
        if (dx * dx + dy * dy <= c.radius * c.radius)
          return i;
      } else if (insideConvex(c.outline, p)) {
        return i;
      }
    }
    return -1;
  }
}
