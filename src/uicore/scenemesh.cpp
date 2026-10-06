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
#include <memory>

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
      const float k = (r - SHRINK) / r;
      return ic + (p2 - ic) * k + n * MY;
    }

    struct Builder {
      const voxel_c & shape;
      const MeshOptions & opt;
      ShapeMesh & out;

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
        const float dashed = variable ? 1.0f : 0.0f;
        const size_t k = corners.size();
        for (size_t i = 1; i + 1 < k; i++) {
          const float o0 = outline ? 0.0f : kNoOutline;
          const float o1 = outline && i + 1 == k - 1 ? 0.0f : kNoOutline;
          const float o2 = outline && i == 1 ? 0.0f : kNoOutline;
          push(dst, corners[0], n, col, cell, 1 + o0, o1, o2, dashed);
          push(dst, corners[i], n, col, cell, o0, 1 + o1, o2, dashed);
          push(dst, corners[i + 1], n, col, cell, o0, o1, 1 + o2, dashed);
        }
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

  ShapeMesh buildShapeMesh(const voxel_c & shape, const MeshOptions & opt) {
    ShapeMesh out;
    float cx, cy, cz;
    shape.calculateSize(&cx, &cy, &cz);
    out.boundsMin = { 0, 0, 0 };
    out.boundsMax = { cx, cy, cz };

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

  ShapeMesh buildPolyhedronMesh(const Polyhedron & poly, bool insides) {
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
    const Rgba8 col = rgba(grey, grey, grey, insides ? 0.1f : 1.0f);
    const Vec3 noCell{ -1e4f, -1e4f, -1e4f };
    std::vector<MeshVertex> & dst = insides ? out.translucent : out.opaque;

    auto push = [&dst, col, noCell](Vec3 p, Vec3 n) {
      MeshVertex m;
      m.pos[0] = p.x; m.pos[1] = p.y; m.pos[2] = p.z;
      m.normal[0] = n.x; m.normal[1] = n.y; m.normal[2] = n.z;
      m.color = col;
      m.cell[0] = noCell.x; m.cell[1] = noCell.y; m.cell[2] = noCell.z;
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
      corners.clear();
      Face::const_edge_circulator e = f->begin();
      Face::const_edge_circulator sentinel = e;
      do {
        corners.push_back(toVec((*e)->dst()->position()));
        ++e;
      } while (e != sentinel);
      for (size_t i = 1; i + 1 < corners.size(); i++) {
        push(corners[0], n);
        push(corners[i], n);
        push(corners[i + 1], n);
      }
    }
    return out;
  }

  std::vector<PickTriangle> buildPickMesh(const voxel_c & shape) {
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
    const float size[3] = { float(shape.getX()), float(shape.getY()), float(shape.getZ()) };
    float lo[3] = { 0, 0, 0 }, hi[3] = { size[0], size[1], size[2] };
    lo[axis] = float(layer);
    hi[axis] = float(layer + 1);

    // map the grid box's corners to drawing space and take their extent;
    // for cubes the mapping is the identity
    Box b{ { 1e30f, 1e30f, 1e30f }, { -1e30f, -1e30f, -1e30f } };
    for (int i = 0; i < 8; i++) {
      float x = (i & 1) ? hi[0] : lo[0];
      float y = (i & 2) ? hi[1] : lo[1];
      float z = (i & 4) ? hi[2] : lo[2];
      shape.recalcSpaceCoordinates(&x, &y, &z);
      b.min = { std::min(b.min.x, x), std::min(b.min.y, y), std::min(b.min.z, z) };
      b.max = { std::max(b.max.x, x), std::max(b.max.y, y), std::max(b.max.z, z) };
    }

    // the in-plane extent is the whole grid as drawn
    const Box g = gridBounds(shape);
    for (int i = 0; i < 3; i++) {
      if (i == axis)
        continue;
      float * mn = i == 0 ? &b.min.x : (i == 1 ? &b.min.y : &b.min.z);
      float * mx = i == 0 ? &b.max.x : (i == 1 ? &b.max.y : &b.max.z);
      *mn = g.min[i];
      *mx = g.max[i];
    }
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

  void addAxes(std::vector<LineSeg> & out, const voxel_c & shape, Rgba8 x, Rgba8 y, Rgba8 z, float widthDp) {
    const Box g = gridBounds(shape);
    const float extra = 0.7f;
    out.push_back({ { 0, 0, 0 }, { g.max.x + extra, 0, 0 }, x, widthDp });
    out.push_back({ { 0, 0, 0 }, { 0, g.max.y + extra, 0 }, y, widthDp });
    out.push_back({ { 0, 0, 0 }, { 0, 0, g.max.z + extra }, z, widthDp });
  }
}
