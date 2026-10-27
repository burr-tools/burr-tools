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
#include "voxeledit.h"

#include "../lib/puzzle.h"
#include "../lib/voxel.h"

#include <algorithm>
#include <cmath>

namespace btui {

  namespace {
    std::array<int, 3> dimsOf(const voxel_c & v) {
      return { int(v.getX()), int(v.getY()), int(v.getZ()) };
    }

    bool inside(const voxel_c & v, const Cell3 & c) {
      return c.x >= 0 && c.y >= 0 && c.z >= 0 &&
             c.x < int(v.getX()) && c.y < int(v.getY()) && c.z < int(v.getZ()) &&
             v.validCoordinate(c.x, c.y, c.z);
    }

    /* the bounding box once, after a batch of changes, not per voxel */
    struct BatchedBounds {
      voxel_c & v;
      explicit BatchedBounds(voxel_c & s) : v(s) { v.skipRecalcBoundingBox(true); }
      ~BatchedBounds() { v.skipRecalcBoundingBox(false); }
      BatchedBounds(const BatchedBounds &) = delete;
      BatchedBounds & operator=(const BatchedBounds &) = delete;
    };
  }

  PlaneAxes planeAxes(Plane p) {
    switch (p) {
      case Plane::XY: return { 0, 1, 2 };
      case Plane::XZ: return { 0, 2, 1 };
      case Plane::YZ: return { 2, 1, 0 };
    }
    return { 0, 1, 2 };
  }

  Cell3 voxelAt(Plane p, int col, int row, int layer) {
    const PlaneAxes a = planeAxes(p);
    int c[3] = { 0, 0, 0 };
    c[a.col] = col;
    c[a.row] = row;
    c[a.layer] = layer;
    return { c[0], c[1], c[2] };
  }

  bool planeAvailable(gridType_c::gridType type, Plane p) {
    return p == Plane::XY || type == gridType_c::GT_BRICKS;
  }

  char axisLetter(int axis) {
    return axis == 0 ? 'X' : axis == 1 ? 'Y' : 'Z';
  }

  bool layerHasVoxels(const voxel_c & shape, Plane p, int layer) {
    const PlaneAxes a = planeAxes(p);
    const std::array<int, 3> d = dimsOf(shape);
    if (layer < 0 || layer >= d[size_t(a.layer)])
      return false;
    for (int r = 0; r < d[size_t(a.row)]; r++)
      for (int c = 0; c < d[size_t(a.col)]; c++) {
        const Cell3 v = voxelAt(p, c, r, layer);
        if (shape.getState(unsigned(v.x), unsigned(v.y), unsigned(v.z)) != voxel_c::VX_EMPTY)
          return true;
      }
    return false;
  }

  std::optional<Tool> quickTool(Platform p, ModifierKeys m) {
    // macOS: Command (Qt's control) is Alt's partner for Variable, Control (Qt's meta) erases
    const bool erase = p == Platform::Mac ? m.meta : m.control;
    const bool variable = p == Platform::Mac ? (m.alt || m.control) : m.alt;
    if (erase)
      return Tool::Erase;
    if (variable)
      return Tool::Variable;
    if (m.shift)
      return Tool::Fixed;
    return std::nullopt;
  }

  Tool toolInEffect(Tool active, Platform p, ModifierKeys m, bool rightButton) {
    if (rightButton)
      return Tool::Erase;
    return quickTool(p, m).value_or(active);
  }

  std::vector<Cell3> expand(Cell3 t, std::array<int, 3> dims, const Modifiers & m) {
    std::vector<Cell3> s;
    for (int a = 0; a < 3; a++)
      if (dims[size_t(a)] <= 0)
        return s;
    s.push_back(t);
    // Span first: the whole line along each spanned axis
    for (int a = 0; a < 3; a++) {
      if (!m.span[size_t(a)])
        continue;
      std::vector<Cell3> next;
      for (const Cell3 & c : s)
        for (int v = 0; v < dims[size_t(a)]; v++) {
          Cell3 n = c;
          (a == 0 ? n.x : a == 1 ? n.y : n.z) = v;
          next.push_back(n);
        }
      s = std::move(next);
    }
    // then Mirror about each mirrored axis's centre plane
    for (int a = 0; a < 3; a++) {
      if (!m.mirror[size_t(a)])
        continue;
      const size_t n = s.size();
      for (size_t i = 0; i < n; i++) {
        Cell3 c = s[i];
        int & v = (a == 0 ? c.x : a == 1 ? c.y : c.z);
        v = dims[size_t(a)] - 1 - v;
        s.push_back(c);
      }
    }
    std::erase_if(s, [&](const Cell3 & c) {
      return c.x < 0 || c.y < 0 || c.z < 0 || c.x >= dims[0] || c.y >= dims[1] || c.z >= dims[2];
    });
    std::sort(s.begin(), s.end());
    s.erase(std::unique(s.begin(), s.end()), s.end());
    return s;
  }

  bool applyTool(voxel_c & shape, const std::vector<Cell3> & cells, Tool t, unsigned int colour) {
    if (colour >= 64)
      colour = 0;
    bool changed = false;
    BatchedBounds batch(shape);
    for (const Cell3 & c : cells) {
      if (!inside(shape, c))
        continue;
      const unsigned x = unsigned(c.x), y = unsigned(c.y), z = unsigned(c.z);
      const int state = shape.getState(x, y, z);
      switch (t) {
        case Tool::Fixed:
        case Tool::Variable: {
          const int want = t == Tool::Fixed ? voxel_c::VX_FILLED : voxel_c::VX_VARIABLE;
          if (state != want) {
            shape.setState(x, y, z, want);
            changed = true;
          }
          if (shape.getColor(x, y, z) != colour) {
            shape.setColor(x, y, z, colour);
            changed = true;
          }
          break;
        }
        case Tool::Erase:
          if (state != voxel_c::VX_EMPTY) {
            shape.setState(x, y, z, voxel_c::VX_EMPTY);
            shape.setColor(x, y, z, 0);
            changed = true;
          }
          break;
        case Tool::Paint:
          if (state != voxel_c::VX_EMPTY && shape.getColor(x, y, z) != colour) {
            shape.setColor(x, y, z, colour);
            changed = true;
          }
          break;
      }
    }
    return changed;
  }

  std::vector<GhostCell> previewTool(const voxel_c & shape, const std::vector<Cell3> & cells, Tool t, unsigned int colour) {
    if (colour >= 64)
      colour = 0;
    std::vector<GhostCell> out;
    for (const Cell3 & c : cells) {
      if (!inside(shape, c))
        continue;
      const unsigned x = unsigned(c.x), y = unsigned(c.y), z = unsigned(c.z);
      const int state = shape.getState(x, y, z);
      const bool sameColour = shape.getColor(x, y, z) == colour;
      switch (t) {
        case Tool::Fixed:
        case Tool::Variable: {
          const int want = t == Tool::Fixed ? voxel_c::VX_FILLED : voxel_c::VX_VARIABLE;
          if (state == voxel_c::VX_EMPTY)
            out.push_back({ c, GhostKind::Place });
          else if (state != want || !sameColour)
            out.push_back({ c, GhostKind::Replace });
          break;
        }
        case Tool::Erase:
          if (state != voxel_c::VX_EMPTY)
            out.push_back({ c, GhostKind::Erase });
          break;
        case Tool::Paint:
          if (state != voxel_c::VX_EMPTY && !sameColour)
            out.push_back({ c, GhostKind::Paint });
          break;
      }
    }
    return out;
  }

  int cellSize(double areaW, double areaH, double unitsW, double unitsH) {
    if (unitsW <= 0 || unitsH <= 0)
      return 16;
    const double s = std::floor(std::min((areaW - 70) / unitsW, (areaH - 60) / unitsH));
    return int(std::clamp(s, 16.0, 120.0));
  }

  int sizeStep(gridType_c::gridType type) {
    switch (type) {
      case gridType_c::GT_RHOMBIC: return 5;
      case gridType_c::GT_TETRA_OCTA: return 3;
      case gridType_c::GT_BRICKS:
      case gridType_c::GT_TRIANGULAR_PRISM:
      case gridType_c::GT_SPHERES:
      case gridType_c::GT_NUM_GRIDS:
        return 1;
    }
    return 1;
  }

  int gridSteps(const voxel_c & shape, int axis) {
    const int step = sizeStep(shape.getGridType()->getType());
    const std::array<int, 3> d = dimsOf(shape);
    return (d[size_t(std::clamp(axis, 0, 2))] + step - 1) / step;
  }

  ResizeResult resizeShape(voxel_c & shape, int axis, int steps) {
    if (axis < 0 || axis > 2)
      return ResizeResult::Unchanged;
    const int step = sizeStep(shape.getGridType()->getType());
    std::array<int, 3> d = dimsOf(shape);
    const int want = std::clamp(steps, 1, maxGridSteps) * step;
    if (want == d[size_t(axis)])
      return ResizeResult::Unchanged;
    // a shrink must not cut a voxel off (OQ-5: refuse, do not clip)
    const bool empty = shape.countState(voxel_c::VX_EMPTY) == shape.getXYZ();
    if (want < d[size_t(axis)] && !empty) {
      const int top = axis == 0 ? int(shape.boundX2()) : axis == 1 ? int(shape.boundY2()) : int(shape.boundZ2());
      if (top >= want)
        return ResizeResult::Blocked;
    }
    d[size_t(axis)] = want;
    shape.resize(unsigned(d[0]), unsigned(d[1]), unsigned(d[2]), 0);
    return ResizeResult::Changed;
  }

  ResizeAllResult resizeAllShapes(puzzle_c & p, int axis, int deltaSteps) {
    ResizeAllResult r;
    if (deltaSteps == 0 || axis < 0 || axis > 2)
      return r;
    for (unsigned s = 0; s < p.getNumberOfShapes(); s++) {
      voxel_c & v = *p.getShape(s);
      switch (resizeShape(v, axis, gridSteps(v, axis) + deltaSteps)) {
        case ResizeResult::Changed: r.changed++; break;
        case ResizeResult::Blocked: r.blocked++; break;
        case ResizeResult::Unchanged: break;
      }
    }
    return r;
  }
}
