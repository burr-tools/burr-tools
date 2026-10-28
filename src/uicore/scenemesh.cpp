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
#include "scenemesh.h"

#include "../lib/gridtype.h"
#include "../lib/voxel.h"
#include "../halfedge/polyhedron.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <set>

namespace btui {

  namespace {

    std::uint8_t channel(float f) {
      return std::uint8_t(std::clamp(f, 0.0f, 1.0f) * 255.0f + 0.5f);
    }

    Rgba8 rgba(float r, float g, float b, float a = 1.0f) {
      return { channel(r), channel(g), channel(b), channel(a) };
    }

    Vec3 toVec(const Vector3Df & v) { return { v.x(), v.y(), v.z() }; }

    /* legacy voxelframe.cpp: the variable-voxel marker is the face shrunk by
     * SHRINK toward its incentre and lifted by MY along its normal */
    constexpr float SHRINK = 0.13f;
    constexpr float MY = 0.005f;

    Vec3 shrinkCorner(Vec3 p1, Vec3 p2, Vec3 p3, Vec3 n) {
      // incentre and inradius of the triangle (p1, p2, p3); returns p2 moved
      // toward the incentre so the shrunk triangle's sides are SHRINK inside
      const float c = length(p2 - p1), b = length(p3 - p1), a = length(p2 - p3);
      const float per = a + b + c;
      if (per <= 1e-9f) return p2;
      const Vec3 ic = p1 * (a / per) + p2 * (b / per) + p3 * (c / per);
      const float s = per / 2;
      const float r = std::sqrt(std::max(0.0f, (s - a) * (s - b) * (s - c) / s));
      if (r <= 1e-9f) return p2;
      // a face whose inradius is under SHRINK (a small facet, a sphere mesh)
      // shrinks to its incentre, not through it into an inverted marker
      const float k = std::max(0.0f, (r - SHRINK) / r);
      return ic + (p2 - ic) * k + n * MY;
    }

    struct Builder {
      const voxel_c & shape;
      const MeshOptions & opt;
      ShapeMesh & out;
      /* the variable voxels' edges drawn so far, by their ends rounded to
       * 1/1024: an edge two faces or two voxels share is one line */
      std::set<std::array<std::int64_t, 6>> edgesSeen{};

      Vec3 cellOf(std::uint32_t voxel) const {
        unsigned int x, y, z;
        if (shape.indexToXYZ(voxel, &x, &y, &z))
          return { float(x), float(y), float(z) };
        return { -1e4f, -1e4f, -1e4f };
      }

      void push(std::vector<MeshVertex> & v, Vec3 p, Vec3 n, Rgba8 c, Vec3 cell) {
        MeshVertex m;
        m.pos[0] = p.x; m.pos[1] = p.y; m.pos[2] = p.z;
        m.normal[0] = n.x; m.normal[1] = n.y; m.normal[2] = n.z;
        m.color = c;
        m.cell[0] = cell.x; m.cell[1] = cell.y; m.cell[2] = cell.z;
        v.push_back(m);
      }

      void push(std::vector<MeshVertex> & v, Vec3 p, Vec3 n, Rgba8 c, Vec3 cell, float e0, float e1, float e2, float dashed) {
        push(v, p, n, c, cell);
        float * e = v.back().edge;
        e[0] = e0; e[1] = e1; e[2] = e2; e[3] = dashed;
      }

      std::vector<Vec3> cornersOf(const Face * f) const {
        std::vector<Vec3> corners;
        Face::const_edge_circulator e = f->begin();
        Face::const_edge_circulator sentinel = e;
        do {
          corners.push_back(toVec((*e)->dst()->position()));
          ++e;
        } while (e != sentinel);
        return corners;
      }

      /* the flat style's colour: the voxel's colour or the shape's, as is --
       * no light and dark alternation */
      Rgba8 flatColor(unsigned int color, bool variable) const {
        Rgba8 c = rgba(opt.piece.r, opt.piece.g, opt.piece.b);
        if (opt.colors == ColorMode::Voxel && color > 0 && color <= opt.palette.size()) {
          const Rgb & p = opt.palette[color - 1];
          c = rgba(p.r, p.g, p.b);
        }
        if (variable)
          c.a = channel(opt.variableAlpha);
        return c;
      }

      /* A drawing-mesh face (spheres) in the flat style: one colour, no
       * outline over its many small facets. */
      void addFlatFace(const Face * f) {
        if (f->hole() || (f->_flags & FF_INSIDE_FACE))
          return;
        const bool variable = shape.getState(f->_fb_index) == voxel_c::VX_VARIABLE;
        addFlatPolygon(cornersOf(f), toVec(f->normal()), flatColor(f->_color, variable), cellOf(f->_fb_index),
                       variable, false);
      }

      /* Every voxel's faces in the flat style. A face is drawn where the
       * neighbour is empty; a fixed voxel also faces a variable neighbour,
       * so it stays a closed solid seen through it; and a variable voxel
       * faces a variable neighbour, so the inner variable voxels show
       * through the outer ones, every boundary outlined (the library's flat
       * mesh treats both kinds as filled and draws neither). Two variable
       * neighbours give two faces in one place, facing away from each
       * other: from any side one of them is culled. */
      void addFlatVoxels(void) {
        struct Side { std::vector<Vec3> corners; int nx = 0, ny = 0, nz = 0; };
        std::vector<Side> sides;
        std::vector<float> fc;
        for (unsigned int z = 0; z < shape.getZ(); z++)
          for (unsigned int y = 0; y < shape.getY(); y++)
            for (unsigned int x = 0; x < shape.getX(); x++) {
              if (shape.isEmpty(x, y, z))
                continue;
              sides.clear();
              Vec3 centre{ 0, 0, 0 };
              int nx, ny, nz;
              for (unsigned int n = 0; shape.getNeighbor(n, 0, int(x), int(y), int(z), &nx, &ny, &nz); n++) {
                fc.clear();
                shape.getConnectionFace(int(x), int(y), int(z), int(n), 0, 0, fc);
                if (fc.size() < 9)
                  continue;
                Side s{ {}, nx, ny, nz };
                Vec3 mid{ 0, 0, 0 };
                for (size_t i = 0; i + 2 < fc.size(); i += 3) {
                  s.corners.push_back({ fc[i], fc[i + 1], fc[i + 2] });
                  mid = mid + s.corners.back();
                }
                centre = centre + mid * (1.0f / float(s.corners.size()));
                sides.push_back(std::move(s));
              }
              if (sides.empty())
                continue;
              centre = centre * (1.0f / float(sides.size()));

              const bool variable = shape.isVariable(x, y, z);
              const Rgba8 col = flatColor(shape.getColor(x, y, z), variable);
              const Vec3 cell{ float(x), float(y), float(z) };
              for (const Side & s : sides) {
                const int other = shape.getState2(s.nx, s.ny, s.nz);
                if (!(other == voxel_c::VX_EMPTY || other == voxel_c::VX_VARIABLE))
                  continue;
                // outward: away from the voxel's centre
                Vec3 nrm = cross(s.corners[1] - s.corners[0], s.corners[2] - s.corners[0]);
                const float len = length(nrm);
                if (len <= 1e-9f)
                  continue;
                nrm = nrm * (1.0f / len);
                if (dot(nrm, s.corners[0] - centre) < 0)
                  nrm = nrm * -1.0f;
                addFlatPolygon(s.corners, nrm, col, cell, variable, true);
              }
            }
      }

      /* A flat-style polygon, fanned into triangles. With `outline` the
       * polygon's own sides are outlined (dashed on variable voxels) but not
       * the fan's diagonals: in each triangle (0, i, i+1) the side opposite
       * corner 0 is always a side of the polygon, the one opposite corner i
       * only in the last triangle, the one opposite corner i+1 only in the
       * first. */
      void addFlatPolygon(const std::vector<Vec3> & corners, Vec3 n, Rgba8 col, Vec3 cell, bool variable, bool outline) {
        std::vector<MeshVertex> & dst = variable && col.a < 255 ? out.translucent : out.opaque;
        const size_t k = corners.size();
        // a variable voxel's sides are dashed lines (ShapeMesh::edges), not
        // the face shader's outline
        if (variable && outline) {
          for (size_t i = 0; i < k; i++)
            addEdge(corners[i], corners[(i + 1) % k]);
          outline = false;
        }
        const float dashed = 0.0f;
        for (size_t i = 1; i + 1 < k; i++) {
          const float o0 = outline ? 0.0f : kNoOutline;
          const float o1 = outline && i + 1 == k - 1 ? 0.0f : kNoOutline;
          const float o2 = outline && i == 1 ? 0.0f : kNoOutline;
          push(dst, corners[0], n, col, cell, 1 + o0, o1, o2, dashed);
          push(dst, corners[i], n, col, cell, o0, 1 + o1, o2, dashed);
          push(dst, corners[i + 1], n, col, cell, o0, o1, 1 + o2, dashed);
        }
      }

      void addEdge(Vec3 a, Vec3 b) {
        auto q = [](float v) { return std::int64_t(std::llround(double(v) * 1024.0)); };
        std::array<std::int64_t, 6> key{ q(a.x), q(a.y), q(a.z), q(b.x), q(b.y), q(b.z) };
        // the same edge either way round
        if (std::lexicographical_compare(key.begin() + 3, key.end(), key.begin(), key.begin() + 3)) {
          std::swap_ranges(key.begin(), key.begin() + 3, key.begin() + 3);
          std::swap(a, b);
        }
        if (!edgesSeen.insert(key).second)
          return;
        // the face outline's look (C06, the mock's render3d): black at 32 %,
        // 1.2 dp, dashes of 3 dp
        out.edges.push_back({ a, b, Rgba8{ 0, 0, 0, 82 }, 1.2f, 3.0f, 3.0f });
      }

      /* the colour legacy drawShape() gives a face in the voxel style */
      Rgba8 faceColor(const Face * f) const {
        if (opt.colors == ColorMode::Voxel && f->_color > 0 && f->_color <= opt.palette.size() &&
            !(f->_flags & FF_VARIABLE_FACE)) {
          const Rgb & c = opt.palette[f->_color - 1];
          return rgba(c.r, c.g, c.b);
        }
        if (f->_flags & FF_VARIABLE_FACE)
          return rgba(0, 0, 0);
        if (f->_flags & FF_COLOR_LIGHT)
          return rgba(lightShade(opt.piece.r), lightShade(opt.piece.g), lightShade(opt.piece.b));
        return rgba(darkShade(opt.piece.r), darkShade(opt.piece.g), darkShade(opt.piece.b));
      }

      void addFace(const Face * f) {
        if (f->hole() || (f->_flags & FF_INSIDE_FACE))
          return;

        // legacy draws variable voxels opaque, told apart by the black marker
        std::vector<MeshVertex> & dst = out.opaque;
        const Vec3 n = toVec(f->normal());
        const Vec3 cell = cellOf(f->_fb_index);
        const Rgba8 col = faceColor(f);

        // a fan over the face, as legacy draws it
        const std::vector<Vec3> corners = cornersOf(f);
        if (corners.size() < 3)
          return;

        for (size_t i = 1; i + 1 < corners.size(); i++) {
          push(dst, corners[0], n, col, cell);
          push(dst, corners[i], n, col, cell);
          push(dst, corners[i + 1], n, col, cell);
        }

        // the black marker on variable voxels (FF_VARIABLE_MARK); legacy
        // handles triangles and quadrilaterals only, and so does this
        if ((f->_flags & FF_VARIABLE_MARK) && (corners.size() == 3 || corners.size() == 4)) {
          const Rgba8 black = rgba(0, 0, 0);
          const size_t k = corners.size();
          std::vector<Vec3> s(k);
          for (size_t i = 0; i < k; i++)
            s[i] = shrinkCorner(corners[(i + k - 1) % k], corners[i], corners[(i + 1) % k], n);
          for (size_t i = 1; i + 1 < k; i++) {
            push(dst, s[0], n, black, cell);
            push(dst, s[i], n, black, cell);
            push(dst, s[i + 1], n, black, cell);
          }
        }
      }
    };
  }

  namespace {

    /* `shape` without its variable voxels */
    std::unique_ptr<voxel_c> fixedVoxels(const voxel_c & shape) {
      std::unique_ptr<voxel_c> v(shape.getGridType()->getVoxel(shape));
      for (unsigned int i = 0; i < v->getXYZ(); i++)
        if (v->getState(i) == voxel_c::VX_VARIABLE)
          v->setState(i, voxel_c::VX_EMPTY);
      return v;
    }

    /* The STL style's faces: the library's STL-look mesh of `fixed` (no
     * variable voxels) -- the export's meshers at the view's defaults, in
     * the shape's own units, each face tagged with its voxel
     * (voxel_c::getSTLMesh; it falls back to the flat mesh itself when a
     * mesher fails). False when even that is not to be had, and the caller
     * draws the voxels flat instead. */
    /* Bricks' STL faces are merged across voxels -- a column's side is one
     * face -- so a face would dim as a whole under Dim other layers. Cut at
     * every cell boundary it crosses (bricks: whole units of the grid), each
     * piece takes the cell it lies in: its centre, nudged inward from the
     * face, rounded down. */
    void splitAtCells(std::vector<MeshVertex> & tris, const voxel_c & shape) {
      const int dims[3] = { int(shape.getX()), int(shape.getY()), int(shape.getZ()) };
      using Poly = std::vector<MeshVertex>;
      auto lerp = [](const MeshVertex & a, const MeshVertex & b, float t) {
        MeshVertex m = a;
        for (int k = 0; k < 3; k++)
          m.pos[k] = a.pos[k] + (b.pos[k] - a.pos[k]) * t;
        return m;
      };
      // the part of a convex polygon on one side of pos[axis] = at
      auto clip = [&](const Poly & poly, int axis, float at, bool below) {
        Poly out;
        for (size_t i = 0; i < poly.size(); i++) {
          const MeshVertex & a = poly[i], & b = poly[(i + 1) % poly.size()];
          const float da = a.pos[axis] - at, db = b.pos[axis] - at;
          const bool ina = below ? da <= 0 : da >= 0, inb = below ? db <= 0 : db >= 0;
          if (ina)
            out.push_back(a);
          if (ina != inb && std::fabs(da - db) > 1e-9f)
            out.push_back(lerp(a, b, da / (da - db)));
        }
        return out;
      };
      std::vector<Poly> polys;
      for (size_t i = 0; i + 2 < tris.size(); i += 3)
        polys.push_back({ tris[i], tris[i + 1], tris[i + 2] });
      for (int axis = 0; axis < 3; axis++) {
        std::vector<Poly> next;
        for (Poly & poly : polys) {
          float lo = 1e30f, hi = -1e30f;
          for (const MeshVertex & v : poly) {
            lo = std::min(lo, v.pos[axis]);
            hi = std::max(hi, v.pos[axis]);
          }
          Poly rest = std::move(poly);
          for (int k = int(std::floor(lo + 1e-4f)) + 1; float(k) < hi - 1e-4f; k++) {
            Poly part = clip(rest, axis, float(k), true);
            if (part.size() >= 3)
              next.push_back(std::move(part));
            rest = clip(rest, axis, float(k), false);
          }
          if (rest.size() >= 3)
            next.push_back(std::move(rest));
        }
        polys = std::move(next);
      }
      tris.clear();
      for (const Poly & poly : polys) {
        Vec3 c{ 0, 0, 0 };
        for (const MeshVertex & v : poly)
          c += Vec3(v.pos[0], v.pos[1], v.pos[2]);
        c = c * (1.0f / float(poly.size()));
        const Vec3 n{ poly[0].normal[0], poly[0].normal[1], poly[0].normal[2] };
        float cell[3];
        for (int k = 0; k < 3; k++)
          cell[k] = float(std::clamp(int(std::floor(c[k] - n[k] * 1e-3f)), 0, dims[k] - 1));
        for (size_t i = 1; i + 1 < poly.size(); i++)
          for (const MeshVertex * v : { &poly[0], &poly[i], &poly[i + 1] }) {
            MeshVertex m = *v;
            m.cell[0] = cell[0]; m.cell[1] = cell[1]; m.cell[2] = cell[2];
            tris.push_back(m);
          }
      }
    }

    /* The prism, rhombic and tetra-octa grids' STL faces are merged across
     * cells too, and their cells are no whole units of the drawing: each
     * face is cut by the walls of the filled cells it crosses -- a cell is
     * the convex polyhedron of its connection faces, as the flat mesh draws
     * it -- and each piece takes its cell. The mesher's surface lies inside
     * the filled cells (its gap); a face they do not cover, to rounding,
     * stays whole with the voxel the mesher gave it. */
    void splitAtCellWalls(std::vector<MeshVertex> & tris, const voxel_c & shape) {
      using Poly = std::vector<MeshVertex>;
      struct Plane { Vec3 n; float d; };                 // inside: dot(n, p) <= d
      struct Cell { float at[3]; Vec3 lo, hi; std::vector<Plane> walls; };
      std::vector<Cell> cells;
      std::vector<float> corners;
      for (unsigned int z = 0; z < shape.getZ(); z++)
        for (unsigned int y = 0; y < shape.getY(); y++)
          for (unsigned int x = 0; x < shape.getX(); x++) {
            if (shape.getState(x, y, z) != voxel_c::VX_FILLED)
              continue;
            std::vector<std::vector<Vec3>> faces;
            int nx, ny, nz;
            for (unsigned int n = 0; shape.getNeighbor(n, 0, int(x), int(y), int(z), &nx, &ny, &nz); n++) {
              corners.clear();
              shape.getConnectionFace(int(x), int(y), int(z), int(n), 0, 0, corners);
              std::vector<Vec3> f;
              for (size_t k = 0; k + 2 < corners.size(); k += 3)
                f.push_back({ corners[k], corners[k + 1], corners[k + 2] });
              if (f.size() >= 3)
                faces.push_back(std::move(f));
            }
            if (faces.size() < 4)                         // no closed cell: leave the faces whole
              return;
            Cell c{ { float(x), float(y), float(z) }, { 1e30f, 1e30f, 1e30f }, { -1e30f, -1e30f, -1e30f }, {} };
            Vec3 mid{ 0, 0, 0 };
            int count = 0;
            for (const auto & f : faces)
              for (const Vec3 & v : f) {
                mid += v;
                count++;
                c.lo = { std::min(c.lo.x, v.x), std::min(c.lo.y, v.y), std::min(c.lo.z, v.z) };
                c.hi = { std::max(c.hi.x, v.x), std::max(c.hi.y, v.y), std::max(c.hi.z, v.z) };
              }
            mid = mid * (1.0f / float(count));
            for (const auto & f : faces) {
              Vec3 n = normalize(cross(f[1] - f[0], f[2] - f[0]));
              if (length(n) < 0.5f)
                continue;
              if (dot(n, mid - f[0]) > 0)                 // outward
                n = -n;
              c.walls.push_back({ n, dot(n, f[0]) });
            }
            cells.push_back(std::move(c));
          }
      if (cells.empty())
        return;

      auto lerp = [](const MeshVertex & a, const MeshVertex & b, float t) {
        MeshVertex m = a;
        for (int k = 0; k < 3; k++)
          m.pos[k] = a.pos[k] + (b.pos[k] - a.pos[k]) * t;
        return m;
      };
      auto posOf = [](const MeshVertex & v) { return Vec3(v.pos[0], v.pos[1], v.pos[2]); };
      // the part of a convex polygon inside a wall
      auto clip = [&](const Poly & poly, const Plane & w) {
        Poly out;
        for (size_t i = 0; i < poly.size(); i++) {
          const MeshVertex & a = poly[i], & b = poly[(i + 1) % poly.size()];
          const float da = dot(w.n, posOf(a)) - w.d, db = dot(w.n, posOf(b)) - w.d;
          if (da <= 1e-6f)
            out.push_back(a);
          if ((da <= 1e-6f) != (db <= 1e-6f) && std::fabs(da - db) > 1e-12f)
            out.push_back(lerp(a, b, da / (da - db)));
        }
        return out;
      };
      auto area = [&](const Poly & poly) {
        Vec3 sum{ 0, 0, 0 };
        for (size_t i = 1; i + 1 < poly.size(); i++)
          sum += cross(posOf(poly[i]) - posOf(poly[0]), posOf(poly[i + 1]) - posOf(poly[0]));
        return 0.5f * length(sum);
      };

      std::vector<MeshVertex> out;
      out.reserve(tris.size() * 4);
      std::vector<std::pair<Poly, const Cell *>> pieces;
      for (size_t i = 0; i + 2 < tris.size(); i += 3) {
        const Poly tri{ tris[i], tris[i + 1], tris[i + 2] };
        const Vec3 a = posOf(tri[0]), b = posOf(tri[1]), c = posOf(tri[2]);
        const Vec3 lo{ std::min({ a.x, b.x, c.x }), std::min({ a.y, b.y, c.y }), std::min({ a.z, b.z, c.z }) };
        const Vec3 hi{ std::max({ a.x, b.x, c.x }), std::max({ a.y, b.y, c.y }), std::max({ a.z, b.z, c.z }) };
        const Vec3 n = normalize(cross(b - a, c - a));
        const float whole = area(tri);
        pieces.clear();
        float covered = 0;
        for (const Cell & cell : cells) {
          if (cell.hi.x < lo.x || cell.lo.x > hi.x || cell.hi.y < lo.y || cell.lo.y > hi.y ||
              cell.hi.z < lo.z || cell.lo.z > hi.z)
            continue;
          // the face's plane through the cell's box
          const Vec3 centre = (cell.lo + cell.hi) * 0.5f, half = (cell.hi - cell.lo) * 0.5f;
          if (std::fabs(dot(n, centre - a)) > std::fabs(n.x) * half.x + std::fabs(n.y) * half.y + std::fabs(n.z) * half.z)
            continue;
          Poly piece = tri;
          for (const Plane & w : cell.walls) {
            piece = clip(piece, w);
            if (piece.size() < 3)
              break;
          }
          const float part = piece.size() >= 3 ? area(piece) : 0.0f;
          if (part <= whole * 1e-6f)
            continue;
          covered += part;
          pieces.emplace_back(std::move(piece), &cell);
        }
        if (whole <= 0 || covered < whole * 0.98f) {
          out.insert(out.end(), tri.begin(), tri.end());
          continue;
        }
        for (const auto & [poly, cell] : pieces)
          for (size_t k = 1; k + 1 < poly.size(); k++)
            for (const MeshVertex * v : { &poly[0], &poly[k], &poly[k + 1] }) {
              MeshVertex m = *v;
              m.cell[0] = cell->at[0]; m.cell[1] = cell->at[1]; m.cell[2] = cell->at[2];
              out.push_back(m);
            }
      }
      tris = std::move(out);
    }

    bool addStlFaces(const voxel_c & fixed, const MeshOptions & opt, ShapeMesh & out) {
      bool filled = false;
      for (unsigned int i = 0; i < fixed.getXYZ() && !filled; i++)
        filled = fixed.getState(i) == voxel_c::VX_FILLED;
      if (!filled)
        return true;                                  // nothing to draw

      const std::unique_ptr<Polyhedron> poly(fixed.getSTLMesh());
      if (!poly || poly->vBegin() == poly->vEnd())
        return false;
      out.opaque = std::move(buildPolyhedronMesh(*poly, false, &opt.piece, &fixed).opaque);
      switch (fixed.getGridType()->getType()) {
        case gridType_c::GT_BRICKS:
          splitAtCells(out.opaque, fixed);
          break;
        case gridType_c::GT_TRIANGULAR_PRISM:
        case gridType_c::GT_RHOMBIC:
        case gridType_c::GT_TETRA_OCTA:
          splitAtCellWalls(out.opaque, fixed);
          break;
        case gridType_c::GT_SPHERES:                    // a ball's faces are its own already
        case gridType_c::GT_NUM_GRIDS:
          break;
      }
      return true;
    }
  }

  ShapeMesh buildShapeMesh(const voxel_c & shape, const MeshOptions & opt) {
    ShapeMesh out;
    float cx, cy, cz;
    shape.calculateSize(&cx, &cy, &cz);
    out.boundsMin = { 0, 0, 0 };
    out.boundsMax = { cx, cy, cz };

    if (opt.style == VoxelStyle::Stl) {
      const std::unique_ptr<voxel_c> fixed = fixedVoxels(shape);
      if (!addStlFaces(*fixed, opt, out)) {
        Builder f{ *fixed, opt, out };
        f.addFlatVoxels();
      }
      return out;
    }
    Builder b{ shape, opt, out };
    if (opt.style == VoxelStyle::Legacy) {
      std::unique_ptr<Polyhedron> poly(shape.getDrawingMesh());
      if (poly)
        for (Polyhedron::const_face_iterator it = poly->fBegin(); it != poly->fEnd(); ++it)
          b.addFace(*it);
      return out;
    }

    /* Flat: the voxel faces themselves. Spheres keep their round drawing
     * mesh -- the mock draws them as smooth balls -- without outlining its
     * many small facets. */
    if (shape.getGridType()->getType() == gridType_c::GT_SPHERES) {
      std::unique_ptr<Polyhedron> poly(shape.getDrawingMesh());
      if (poly)
        for (Polyhedron::const_face_iterator it = poly->fBegin(); it != poly->fEnd(); ++it)
          b.addFlatFace(*it);
      return out;
    }
    b.addFlatVoxels();
    return out;
  }

  ShapeMesh buildPolyhedronMesh(const Polyhedron & poly, bool insides, const Rgb * colour, const voxel_c * cells) {
    ShapeMesh out;
    bool first = true;
    for (Polyhedron::const_vertex_iterator it = poly.vBegin(); it != poly.vEnd(); ++it) {
      const Vec3 p = toVec((*it)->position());
      if (first) {
        out.boundsMin = out.boundsMax = p;
        first = false;
      }
      out.boundsMin = { std::min(out.boundsMin.x, p.x), std::min(out.boundsMin.y, p.y), std::min(out.boundsMin.z, p.z) };
      out.boundsMax = { std::max(out.boundsMax.x, p.x), std::max(out.boundsMax.y, p.y), std::max(out.boundsMax.z, p.z) };
    }

    const float grey = darkShade(0.5f);
    const Rgba8 col = colour ? rgba(colour->r, colour->g, colour->b, insides ? 0.1f : 1.0f)
                             : rgba(grey, grey, grey, insides ? 0.1f : 1.0f);
    const Vec3 noCell{ -1e4f, -1e4f, -1e4f };
    std::vector<MeshVertex> & dst = insides ? out.translucent : out.opaque;

    auto push = [&dst, col](Vec3 p, Vec3 n, Vec3 cell) {
      MeshVertex m;
      m.pos[0] = p.x; m.pos[1] = p.y; m.pos[2] = p.z;
      m.normal[0] = n.x; m.normal[1] = n.y; m.normal[2] = n.z;
      m.color = col;
      m.cell[0] = cell.x; m.cell[1] = cell.y; m.cell[2] = cell.z;
      dst.push_back(m);
    };

    std::vector<Vec3> corners;
    for (Polyhedron::const_face_iterator it = poly.fBegin(); it != poly.fEnd(); ++it) {
      const Face * f = *it;
      // no exporter sets FF_INSIDE_FACE today; legacy hides such faces
      // unless the insides show, and so does this
      if (f->hole() || ((f->_flags & FF_INSIDE_FACE) && !insides))
        continue;
      Vec3 n = toVec(f->normal());
      if (f->_flags & FF_INSIDE_FACE)
        n = n * -1.0f;
      Vec3 cell = noCell;
      unsigned int x, y, z;
      if (cells && cells->indexToXYZ(f->_fb_index, &x, &y, &z))
        cell = Vec3(float(x), float(y), float(z));
      corners.clear();
      Face::const_edge_circulator e = f->begin();
      Face::const_edge_circulator sentinel = e;
      do {
        corners.push_back(toVec((*e)->dst()->position()));
        ++e;
      } while (e != sentinel);
      for (size_t i = 1; i + 1 < corners.size(); i++) {
        push(corners[0], n, cell);
        push(corners[i], n, cell);
        push(corners[i + 1], n, cell);
      }
    }
    return out;
  }

  std::vector<PickTriangle> buildPickMesh(const voxel_c & shape) {
    if (shape.getGridType()->getType() == gridType_c::GT_SPHERES)
      return buildPickMeshFromFlatMesh(shape);
    std::vector<PickTriangle> out;
    std::vector<float> fc;
    std::vector<Vec3> c;
    for (unsigned int z = 0; z < shape.getZ(); z++)
      for (unsigned int y = 0; y < shape.getY(); y++)
        for (unsigned int x = 0; x < shape.getX(); x++) {
          if (shape.isEmpty(x, y, z))
            continue;
          const std::uint32_t index = shape.getIndex(x, y, z);
          int nx, ny, nz;
          for (unsigned int n = 0; shape.getNeighbor(n, 0, int(x), int(y), int(z), &nx, &ny, &nz); n++) {
            // a face shows where the neighbour is empty or outside the grid
            if (shape.getState2(nx, ny, nz) != voxel_c::VX_EMPTY)
              continue;
            fc.clear();
            shape.getConnectionFace(int(x), int(y), int(z), int(n), 0, 0, fc);
            c.clear();
            for (size_t i = 0; i + 2 < fc.size(); i += 3)
              c.push_back({ fc[i], fc[i + 1], fc[i + 2] });
            for (size_t i = 1; i + 1 < c.size(); i++)
              out.push_back({ c[0], c[i], c[i + 1], index, std::int32_t(n) });
          }
        }
    return out;
  }

  std::vector<PickTriangle> buildPickMeshFromFlatMesh(const voxel_c & shape) {
    std::vector<PickTriangle> out;
    std::unique_ptr<Polyhedron> poly(shape.getFlatMesh());
    if (!poly)
      return out;

    for (Polyhedron::const_face_iterator it = poly->fBegin(); it != poly->fEnd(); ++it) {
      const Face * f = *it;
      if (f->hole() || (f->_flags & FF_INSIDE_FACE))
        continue;
      std::vector<Vec3> c;
      Face::const_edge_circulator e = f->begin();
      Face::const_edge_circulator sentinel = e;
      do {
        c.push_back(toVec((*e)->dst()->position()));
        ++e;
      } while (e != sentinel);
      for (size_t i = 1; i + 1 < c.size(); i++)
        out.push_back({ c[0], c[i], c[i + 1], f->_fb_index, f->_fb_face });
    }
    return out;
  }

  PickHit pick(const std::vector<PickTriangle> & mesh, Vec3 o, Vec3 d) {
    PickHit best;
    // Moeller-Trumbore, both sides: the flat mesh's faces point outward and
    // a ray from outside meets the front of the nearest one
    for (const PickTriangle & tr : mesh) {
      const Vec3 e1 = tr.b - tr.a, e2 = tr.c - tr.a;
      const Vec3 p = cross(d, e2);
      const float det = dot(e1, p);
      if (std::fabs(det) < 1e-12f)
        continue;
      const float inv = 1.0f / det;
      const Vec3 s = o - tr.a;
      const float u = dot(s, p) * inv;
      if (u < 0 || u > 1)
        continue;
      const Vec3 q = cross(s, e1);
      const float v = dot(d, q) * inv;
      if (v < 0 || u + v > 1)
        continue;
      const float t = dot(e2, q) * inv;
      if (t <= 0)
        continue;
      if (!best.hit || t < best.t) {
        best.hit = true;
        best.t = t;
        best.point = o + d * t;
        best.voxel = tr.voxel;
        best.face = tr.face;
      }
    }
    return best;
  }

  int layerAxis(Plane p) {
    switch (p) {
      case Plane::XY: return 2;
      case Plane::XZ: return 1;
      default:        return 0;
    }
  }

  int layerCount(const voxel_c & shape, Plane p) {
    switch (layerAxis(p)) {
      case 0:  return int(shape.getX());
      case 1:  return int(shape.getY());
      default: return int(shape.getZ());
    }
  }

  Box gridBounds(const voxel_c & shape) {
    float cx, cy, cz;
    shape.calculateSize(&cx, &cy, &cz);
    return { { 0, 0, 0 }, { cx, cy, cz } };
  }

  Box layerSlab(const voxel_c & shape, Plane p, int layer) {
    const int axis = layerAxis(p);

    /* Along the layer axis: the layer's cells as they are drawn, which is
     * more than one layer step where cells reach into the next layer
     * (spheres, rhombic and tetra-octa cells). A cell is the faces round it
     * as the flat style draws them (getConnectionFace); a sphere is a ball
     * of radius 0.5 round its centre (the drawing mesh's). Every voxel type
     * repeats within 10 cells, so a window of the plane that size will do. */
    const int dims[3] = { int(shape.getX()), int(shape.getY()), int(shape.getZ()) };
    const bool spheres = shape.getGridType()->getType() == gridType_c::GT_SPHERES;
    float lo = 1e30f, hi = -1e30f;
    auto take = [&](float v) { lo = std::min(lo, v); hi = std::max(hi, v); };
    int c[3];
    std::vector<float> fc;
    const int a1 = (axis + 1) % 3, a2 = (axis + 2) % 3;
    c[axis] = layer;
    for (c[a1] = 0; c[a1] < std::min(dims[a1], 10); c[a1]++)
      for (c[a2] = 0; c[a2] < std::min(dims[a2], 10); c[a2]++) {
        if (!shape.validCoordinate(c[0], c[1], c[2]))
          continue;
        if (spheres) {
          float m[3] = { float(c[0]), float(c[1]), float(c[2]) };
          shape.recalcSpaceCoordinates(&m[0], &m[1], &m[2]);
          // the centre is half a unit in from the mapped coordinate
          take(m[axis]);
          take(m[axis] + 1.0f);
          continue;
        }
        int nx, ny, nz;
        for (unsigned int n = 0; shape.getNeighbor(n, 0, c[0], c[1], c[2], &nx, &ny, &nz); n++) {
          fc.clear();
          shape.getConnectionFace(c[0], c[1], c[2], int(n), 0, 0, fc);
          for (size_t i = 0; i + 2 < fc.size(); i += 3)
            take(fc[i + size_t(axis)]);
        }
      }
    if (lo > hi) {
      // no cell of the type in the layer: its step, mapped to drawing space
      float s[3] = { 0, 0, 0 }, e[3] = { 0, 0, 0 };
      s[axis] = float(layer);
      e[axis] = float(layer + 1);
      shape.recalcSpaceCoordinates(&s[0], &s[1], &s[2]);
      shape.recalcSpaceCoordinates(&e[0], &e[1], &e[2]);
      lo = s[axis];
      hi = e[axis];
    }

    // the in-plane extent is the whole grid as drawn
    Box b = gridBounds(shape);
    (axis == 0 ? b.min.x : axis == 1 ? b.min.y : b.min.z) = lo;
    (axis == 0 ? b.max.x : axis == 1 ? b.max.y : b.max.z) = hi;
    return b;
  }

  void addBoxEdges(std::vector<LineSeg> & out, const Box & b, Rgba8 color, float widthDp, float dashDp, float gapDp) {
    auto corner = [&](int i) {
      return Vec3{ (i & 1) ? b.max.x : b.min.x, (i & 2) ? b.max.y : b.min.y, (i & 4) ? b.max.z : b.min.z };
    };
    static const int edges[12][2] = {
      { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 },   // along X
      { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 },   // along Y
      { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },   // along Z
    };
    for (const auto & e : edges)
      out.push_back({ corner(e[0]), corner(e[1]), color, widthDp, dashDp, gapDp });
  }

  void addBoxFaces(std::vector<MeshVertex> & out, const Box & b, Rgba8 color) {
    auto corner = [&](int i) {
      return Vec3{ (i & 1) ? b.max.x : b.min.x, (i & 2) ? b.max.y : b.min.y, (i & 4) ? b.max.z : b.min.z };
    };
    // each face as two triangles, counter-clockwise seen from outside
    static const int quads[6][4] = {
      { 1, 3, 7, 5 }, { 0, 4, 6, 2 },   // +X, -X
      { 2, 6, 7, 3 }, { 0, 1, 5, 4 },   // +Y, -Y
      { 4, 5, 7, 6 }, { 0, 2, 3, 1 },   // +Z, -Z
    };
    static const Vec3 normals[6] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
    for (int f = 0; f < 6; f++) {
      const int idx[6] = { quads[f][0], quads[f][1], quads[f][2], quads[f][0], quads[f][2], quads[f][3] };
      for (int i : idx) {
        MeshVertex v;
        Vec3 p = corner(i);
        v.pos[0] = p.x; v.pos[1] = p.y; v.pos[2] = p.z;
        v.normal[0] = normals[f].x; v.normal[1] = normals[f].y; v.normal[2] = normals[f].z;
        v.color = color;
        // never inside a dimmed layer test: the slab is always drawn as is
        v.cell[0] = v.cell[1] = v.cell[2] = -1e4f;
        out.push_back(v);
      }
    }
  }

  void addGhost(std::vector<MeshVertex> & faces, std::vector<LineSeg> & lines, const voxel_c & shape,
                const std::vector<std::array<int, 3>> & cells, const GhostStyle & style, Vec3 offset) {
    if (cells.empty())
      return;
    /* the cells alone, as variable voxels: the flat builder draws every
     * face of one (a variable neighbour hides none) and its edges */
    std::unique_ptr<voxel_c> g(shape.getGridType()->getVoxel(shape.getX(), shape.getY(), shape.getZ(), voxel_c::VX_EMPTY));
    for (const auto & c : cells)
      if (c[0] >= 0 && c[1] >= 0 && c[2] >= 0 && unsigned(c[0]) < g->getX() && unsigned(c[1]) < g->getY() &&
          unsigned(c[2]) < g->getZ() && g->validCoordinate(c[0], c[1], c[2]))
        g->setState(unsigned(c[0]), unsigned(c[1]), unsigned(c[2]), voxel_c::VX_VARIABLE);
    MeshOptions opt;
    opt.style = VoxelStyle::Flat;
    opt.variableAlpha = 1;
    const ShapeMesh m = buildShapeMesh(*g, opt);
    for (const auto * list : { &m.opaque, &m.translucent })
      for (MeshVertex v : *list) {
        v.color = style.fill;
        v.edge[0] = v.edge[1] = v.edge[2] = kNoOutline;
        v.cell[0] = v.cell[1] = v.cell[2] = -1e4f;
        v.pos[0] += offset.x;
        v.pos[1] += offset.y;
        v.pos[2] += offset.z;
        faces.push_back(v);
      }
    for (LineSeg e : m.edges) {
      e.a += offset;
      e.b += offset;
      e.color = style.outline;
      e.widthDp = style.widthDp;
      e.dashDp = style.dashDp;
      e.gapDp = style.gapDp;
      lines.push_back(e);
    }
  }

  void addFaceOutline(std::vector<LineSeg> & lines, const std::vector<PickTriangle> & pick, std::uint32_t voxel,
                      std::int32_t face, const GhostStyle & style) {
    auto q = [](float v) { return std::int64_t(std::llround(double(v) * 1024.0)); };
    // each side by its ends rounded to 1/1024, either way round, and how often it occurs
    std::map<std::array<std::int64_t, 6>, std::pair<std::pair<Vec3, Vec3>, int>> sides;
    for (const PickTriangle & t : pick) {
      if (t.voxel != voxel || t.face != face)
        continue;
      const Vec3 p[3] = { t.a, t.b, t.c };
      for (int i = 0; i < 3; i++) {
        Vec3 a = p[i], b = p[(i + 1) % 3];
        std::array<std::int64_t, 6> k{ q(a.x), q(a.y), q(a.z), q(b.x), q(b.y), q(b.z) };
        if (std::lexicographical_compare(k.begin() + 3, k.end(), k.begin(), k.begin() + 3)) {
          std::swap_ranges(k.begin(), k.begin() + 3, k.begin() + 3);
          std::swap(a, b);
        }
        auto & e = sides[k];
        e.first = { a, b };
        e.second++;
      }
    }
    for (const auto & [k, e] : sides)
      if (e.second == 1)
        lines.push_back({ e.first.first, e.first.second, style.outline, style.widthDp, style.dashDp, style.gapDp });
  }

  void addAxes(std::vector<LineSeg> & out, const voxel_c & shape, Rgba8 x, Rgba8 y, Rgba8 z, float widthDp) {
    const Box g = gridBounds(shape);
    const float extra = 0.7f;
    out.push_back({ { 0, 0, 0 }, { g.max.x + extra, 0, 0 }, x, widthDp });
    out.push_back({ { 0, 0, 0 }, { 0, g.max.y + extra, 0 }, y, widthDp });
    out.push_back({ { 0, 0, 0 }, { 0, 0, g.max.z + extra }, z, widthDp });
  }

  namespace {
    Vec3 centreOf(const Box & b) { return (b.min + b.max) * 0.5f; }

    /* a unit vector at right angles to a unit axis (legacy's choice of it) */
    Vec3 sideOf(Vec3 axis) {
      const Vec3 ref = std::fabs(axis.z) > 0.9f ? Vec3(1, 0, 0) : Vec3(0, 0, 1);
      const Vec3 s = normalize(cross(ref, axis));
      return length(s) > 0.5f ? s : Vec3(1, 0, 0);
    }

    /* an arrowhead as two strokes back from the tip */
    void arrowHead(std::vector<LineSeg> & out, Vec3 tip, Vec3 dir, Vec3 side, float size, Rgba8 color, float widthDp) {
      const Vec3 back = tip - dir * size;
      out.push_back({ tip, back + side * (size * 0.5f), color, widthDp });
      out.push_back({ tip, back - side * (size * 0.5f), color, widthDp });
    }
  }

  void addResultGhost(std::vector<MeshVertex> & faces, std::vector<LineSeg> & lines, const voxel_c & shape,
                      const voxel_c * result, const GhostStyle & style) {
    if (!result)
      return;
    std::vector<std::array<int, 3>> cells;
    for (unsigned int z = 0; z < result->getZ(); z++)
      for (unsigned int y = 0; y < result->getY(); y++)
        for (unsigned int x = 0; x < result->getX(); x++)
          if (result->getState(x, y, z) != voxel_c::VX_EMPTY)
            cells.push_back({ int(x), int(y), int(z) });
    addGhost(faces, lines, *result, cells, style, centreOf(gridBounds(shape)) - centreOf(gridBounds(*result)));
  }

  float transformHintRadius(const voxel_c & shape) {
    const Box g = gridBounds(shape);
    return std::min(0.4f * length(g.max - g.min) * 0.5f + 0.5f, 2.0f);
  }

  void addTransformHint(std::vector<LineSeg> & out, Vec3 centre, float radius, TransformHint kind, Vec3 axis,
                        float angleDeg, Rgba8 color, float widthDp) {
    axis = normalize(axis);
    if (length(axis) < 0.5f)
      return;
    const Vec3 u = sideOf(axis);
    if (kind == TransformHint::Rotate) {
      const Vec3 v = cross(axis, u);
      const float sweep = radians(std::clamp(angleDeg, 60.0f, 300.0f));
      constexpr int segs = 32;
      auto at = [&](float a) { return centre + (u * std::cos(a) + v * std::sin(a)) * radius; };
      for (int i = 0; i < segs; i++)
        out.push_back({ at(sweep * float(i) / segs), at(sweep * float(i + 1) / segs), color, widthDp });
      // the head at the arc's leading end, along the sweep
      const Vec3 radial = u * std::cos(sweep) + v * std::sin(sweep);
      const Vec3 tangent = v * std::cos(sweep) - u * std::sin(sweep);
      arrowHead(out, at(sweep), tangent, radial, radius * 0.22f, color, widthDp);
      return;
    }
    const Vec3 a = centre - axis * radius, b = centre + axis * radius;
    out.push_back({ a, b, color, widthDp });
    arrowHead(out, b, axis, u, radius * 0.3f, color, widthDp);
    if (kind == TransformHint::Mirror)
      arrowHead(out, a, -axis, u, radius * 0.3f, color, widthDp);
  }
}
