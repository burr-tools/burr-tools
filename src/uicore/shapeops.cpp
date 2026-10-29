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
#include "shapeops.h"

#include "voxeledit.h"

#include "../lib/puzzle.h"
#include "../lib/voxel.h"

#include <algorithm>
#include <cmath>
#include <deque>

namespace btui {

  namespace {

    using G = gridType_c::gridType;

    /* What a transform task does per voxel type: a library transform
     * (voxel_c::transform's number) or a translation -- legacy's applyTask
     * tables, one per tool tab */
    struct Move {
      int task;
      int transform;            ///< -1: a translation
      int dx, dy, dz;
    };

    // bricks; rhombic and tetra-octa with their nudge step (5, 6): ToolTab_0, _3, _4
    std::vector<Move> cubicMoves(int s) {
      return {
        { 0, -1, s, 0, 0 }, { 1, -1, -s, 0, 0 }, { 2, -1, 0, s, 0 }, { 3, -1, 0, -s, 0 },
        { 4, -1, 0, 0, s }, { 5, -1, 0, 0, -s },
        { 6, 1, 0, 0, 0 }, { 7, 3, 0, 0, 0 }, { 8, 4, 0, 0, 0 }, { 9, 12, 0, 0, 0 },
        { 10, 16, 0, 0, 0 }, { 11, 20, 0, 0, 0 },
        { 12, 24, 0, 0, 0 }, { 13, 34, 0, 0, 0 }, { 14, 32, 0, 0, 0 },
      };
    }

    const std::vector<Move> & moves(G t) {
      static const std::vector<Move> bricks = cubicMoves(1), rhombic = cubicMoves(5), tetra = cubicMoves(6);
      // ToolTab_1
      static const std::vector<Move> prism = {
        { 0, -1, 1, 1, 0 }, { 1, -1, -1, 1, 0 }, { 2, -1, 2, 0, 0 }, { 3, -1, -2, 0, 0 },
        { 4, -1, 0, 0, 1 }, { 5, -1, 0, 0, -1 }, { 27, -1, 1, -1, 0 }, { 28, -1, -1, -1, 0 },
        { 6, 9, 0, 0, 0 }, { 9, 6, 0, 0, 0 }, { 10, 5, 0, 0, 0 }, { 11, 1, 0, 0, 0 },
        { 12, 12, 0, 0, 0 }, { 13, 15, 0, 0, 0 }, { 14, 18, 0, 0, 0 },
      };
      // ToolTab_2
      static const std::vector<Move> spheres = {
        { 0, -1, 1, 0, 1 }, { 1, -1, 0, 1, 1 }, { 2, -1, -1, 0, 1 }, { 3, -1, 0, -1, 1 },
        { 4, -1, 1, 1, 0 }, { 5, -1, -1, 1, 0 }, { 27, -1, -1, -1, 0 }, { 28, -1, 1, -1, 0 },
        { 29, -1, 1, 0, -1 }, { 30, -1, 0, 1, -1 }, { 31, -1, -1, 0, -1 }, { 32, -1, 0, -1, -1 },
        { 6, 14, 0, 0, 0 }, { 7, 9, 0, 0, 0 }, { 8, 16, 0, 0, 0 }, { 9, 5, 0, 0, 0 },
        { 10, 6, 0, 0, 0 }, { 11, 2, 0, 0, 0 },
        { 12, 120, 0, 0, 0 }, { 13, 124, 0, 0, 0 }, { 14, 141, 0, 0, 0 },
      };
      switch (t) {
        case gridType_c::GT_TRIANGULAR_PRISM: return prism;
        case gridType_c::GT_SPHERES: return spheres;
        case gridType_c::GT_RHOMBIC: return rhombic;
        case gridType_c::GT_TETRA_OCTA: return tetra;
        case gridType_c::GT_BRICKS:
        case gridType_c::GT_NUM_GRIDS: return bricks;
      }
      return bricks;
    }

    const Move * moveOf(G t, int task) {
      for (const Move & m : moves(t))
        if (m.task == task)
          return &m;
      return nullptr;
    }

    G typeOf(const voxel_c & v) { return v.getGridType()->getType(); }

    unsigned int filled(const voxel_c & v) {
      return v.countState(voxel_c::VX_FILLED) + v.countState(voxel_c::VX_VARIABLE);
    }

    /* counted: the bounding box of an empty 1x1x1 grid is not reversed */
    bool empty(const voxel_c & v) { return filled(v) == 0; }

    std::unique_ptr<voxel_c> copyOf(const voxel_c & v) {
      return std::unique_ptr<voxel_c>(v.getGridType()->getVoxel(v));
    }

    // --- legacy's To origin and Center, per voxel type ---------------------

    void origin(voxel_c & s) {
      int dx = -int(s.boundX1()), dy = -int(s.boundY1()), dz = -int(s.boundZ1());
      switch (typeOf(s)) {
        case gridType_c::GT_TRIANGULAR_PRISM:
          // an even move in X + Y, or the triangles turn over
          if ((dx + dy) & 1) {
            if (dx < 0) dx++; else dy++;
          }
          break;
        case gridType_c::GT_SPHERES:
          if ((dx + dy + dz) & 1) {
            if (dx < 0) dx++; else if (dy < 0) dy++; else dz++;
          }
          break;
        case gridType_c::GT_RHOMBIC:
          dx = -(int(s.boundX1()) / 5) * 5; dy = -(int(s.boundY1()) / 5) * 5; dz = -(int(s.boundZ1()) / 5) * 5;
          break;
        case gridType_c::GT_TETRA_OCTA:
          dx = -(int(s.boundX1()) / 6) * 6; dy = -(int(s.boundY1()) / 6) * 6; dz = -(int(s.boundZ1()) / 6) * 6;
          break;
        case gridType_c::GT_BRICKS:
        case gridType_c::GT_NUM_GRIDS:
          break;
      }
      s.translate(dx, dy, dz, 0);
    }

    void center(voxel_c & s) {
      const G t = typeOf(s);
      if (t == gridType_c::GT_RHOMBIC || t == gridType_c::GT_TETRA_OCTA) {
        // the nearest whole cell (5 or 6 units) that keeps the piece inside
        const int u = t == gridType_c::GT_RHOMBIC ? 5 : 6, half = t == gridType_c::GT_RHOMBIC ? 2 : 3;
        auto step = [&](int size, int lo, int hi) {
          int f = (size - (hi - lo + 1)) / 2 - lo;
          if (f % u <= half)
            f -= f % u;
          else if (hi + (u - f % u) < size)
            f += u - f % u;
          return f;
        };
        s.translate(step(int(s.getX()), int(s.boundX1()), int(s.boundX2())),
                    step(int(s.getY()), int(s.boundY1()), int(s.boundY2())),
                    step(int(s.getZ()), int(s.boundZ1()), int(s.boundZ2())), 0);
        return;
      }
      int fx = int(s.getX()) - (int(s.boundX2()) - int(s.boundX1()) + 1);
      int fy = int(s.getY()) - (int(s.boundY2()) - int(s.boundY1()) + 1);
      int fz = int(s.getZ()) - (int(s.boundZ2()) - int(s.boundZ1()) + 1);
      // an odd margin: the grid grows a cell so the piece sits exactly in the middle
      if ((fx & 1) || (fy & 1) || (fz & 1)) {
        s.resize(s.getX() + unsigned(fx & 1), s.getY() + unsigned(fy & 1), s.getZ() + unsigned(fz & 1), 0);
        fx += fx & 1;
        fy += fy & 1;
        fz += fz & 1;
      }
      int dx = fx / 2 - int(s.boundX1()), dy = fy / 2 - int(s.boundY1()), dz = fz / 2 - int(s.boundZ1());
      // and the lattice's parity kept (prisms: X + Y even, spheres: X + Y + Z)
      const bool odd = t == gridType_c::GT_TRIANGULAR_PRISM ? ((dx + dy) & 1) != 0
                     : t == gridType_c::GT_SPHERES          ? ((dx + dy + dz) & 1) != 0 : false;
      if (odd) {
        s.resize(s.getX() + 2, s.getY(), s.getZ(), 0);
        fx += 2;
        dx = fx / 2 - int(s.boundX1());
      }
      s.translate(dx, dy, dz, 0);
    }

    // --- the surface sets -----------------------------------------------------

    bool inGrid(const voxel_c & v, int x, int y, int z) {
      return x >= 0 && y >= 0 && z >= 0 && x < int(v.getX()) && y < int(v.getY()) && z < int(v.getZ());
    }

    int surfaceAction(voxel_c & v, voxel_c::VoxelAction action, bool inside) {
      const std::vector<bool> outer = outerVoxels(v);
      int n = 0;
      for (unsigned int i = 0; i < v.getXYZ(); i++) {
        if (v.getState(i) == voxel_c::VX_EMPTY || outer[i] == inside)
          continue;
        const int before = v.getState(i), colour = int(v.getColor(i));
        switch (action) {
          case voxel_c::ACT_FIXED: v.setState(i, voxel_c::VX_FILLED); break;
          case voxel_c::ACT_VARIABLE: v.setState(i, voxel_c::VX_VARIABLE); break;
          case voxel_c::ACT_DECOLOR: v.setColor(i, 0); break;
        }
        n += v.getState(i) != before || int(v.getColor(i)) != colour;
      }
      return n;
    }

    bool isTransform(G t, int task) { return moveOf(t, task) != nullptr; }

    bool isFit(int task) {
      return task == task::Prune || task == task::Origin || task == task::Center;
    }
  }

  // --- the Transform buttons ----------------------------------------------------

  const OpsDescriptor & opsDescriptor(gridType_c::gridType t) {
    using K = OpKind;
    // the matrix of C04: Flip | Rotate −/+ | Nudge −/+ per axis
    static const std::vector<TransformButton> matrix = {
      { 12, K::Flip, 0, 0, "flip-x", 0, 0, "Flip", "Mirror the piece across the X plane", "flip.x" },
      { 13, K::Flip, 1, 0, "flip-y", 0, 0, "Flip", "Mirror the piece across the Y plane", "flip.y" },
      { 14, K::Flip, 2, 0, "flip-z", 0, 0, "Flip", "Mirror the piece across the Z plane", "flip.z" },
      // legacy's left icon (clockwise) is −, its right icon +
      { 6, K::Rotate, 0, -1, "rotate-x-neg", 0, 0, "−90°", "Rotate about X by −90°", "rotate.x.neg" },
      { 7, K::Rotate, 0, 1, "rotate-x-pos", 0, 0, "+90°", "Rotate about X by +90°", "rotate.x.pos" },
      { 9, K::Rotate, 1, -1, "rotate-y-neg", 0, 0, "−90°", "Rotate about Y by −90°", "rotate.y.neg" },
      { 8, K::Rotate, 1, 1, "rotate-y-pos", 0, 0, "+90°", "Rotate about Y by +90°", "rotate.y.pos" },
      { 10, K::Rotate, 2, -1, "rotate-z-neg", 0, 0, "−90°", "Rotate about Z by −90°", "rotate.z.neg" },
      { 11, K::Rotate, 2, 1, "rotate-z-pos", 0, 0, "+90°", "Rotate about Z by +90°", "rotate.z.pos" },
      { 1, K::Nudge, 0, -1, "nudge-x-neg", 0, 0, "−1", "Nudge along X by −1", "nudge.x.neg" },
      { 0, K::Nudge, 0, 1, "nudge-x-pos", 0, 0, "+1", "Nudge along X by +1", "nudge.x.pos" },
      { 3, K::Nudge, 1, -1, "nudge-y-neg", 0, 0, "−1", "Nudge along Y by −1", "nudge.y.neg" },
      { 2, K::Nudge, 1, 1, "nudge-y-pos", 0, 0, "+1", "Nudge along Y by +1", "nudge.y.pos" },
      { 5, K::Nudge, 2, -1, "nudge-z-neg", 0, 0, "−1", "Nudge along Z by −1", "nudge.z.neg" },
      { 4, K::Nudge, 2, 1, "nudge-z-pos", 0, 0, "+1", "Nudge along Z by +1", "nudge.z.pos" },
    };
    static const OpsDescriptor bricks{ false, true, true, matrix };
    static const OpsDescriptor rhombic{ false, true, false, matrix };
    static const OpsDescriptor tetra{ false, true, false, matrix };
    // ToolTab_1's TransformButtons type 1: 180° about X and Y, ±60° about Z;
    // the nudge pad along the triangles' edges, then a layer down and up
    static const OpsDescriptor prism{ true, true, false, {
      matrix[0], matrix[1], matrix[2],
      { 6, K::Rotate, 0, 0, "rotate-x-pos", 0, 0, "180°", "Rotate about X by 180°", "rotate.x" },
      { 9, K::Rotate, 1, 0, "rotate-y-pos", 0, 0, "180°", "Rotate about Y by 180°", "rotate.y" },
      { 10, K::Rotate, 2, -1, "rotate-z-neg", 0, 0, "−60°", "Rotate about Z by −60°", "rotate.z.neg" },
      { 11, K::Rotate, 2, 1, "rotate-z-pos", 0, 0, "+60°", "Rotate about Z by +60°", "rotate.z.pos" },
      { 1, K::Nudge, 0, 0, "", 120, 0, "", "Nudge up and left in the layer", "nudge.upLeft" },
      { 0, K::Nudge, 0, 0, "", 60, 0, "", "Nudge up and right in the layer", "nudge.upRight" },
      { 3, K::Nudge, 0, 0, "", 180, 0, "", "Nudge left in the layer", "nudge.left" },
      { 2, K::Nudge, 0, 0, "", 0, 0, "", "Nudge right in the layer", "nudge.right" },
      { 28, K::Nudge, 0, 0, "", 240, 0, "", "Nudge down and left in the layer", "nudge.downLeft" },
      { 27, K::Nudge, 0, 0, "", 300, 0, "", "Nudge down and right in the layer", "nudge.downRight" },
      { 5, K::Nudge, 2, -1, "nudge-z-neg", 0, -1, "−1", "Nudge a layer down (Z −1)", "nudge.z.neg" },
      { 4, K::Nudge, 2, 1, "nudge-z-pos", 0, 1, "+1", "Nudge a layer up (Z +1)", "nudge.z.pos" },
    } };
    // ToolTab_2's type 2: 12 nudges, four up a layer, four in it, four down
    static const OpsDescriptor spheres{ true, false, false, {
      matrix[0], matrix[1], matrix[2], matrix[3], matrix[4], matrix[5], matrix[6], matrix[7], matrix[8],
      { 0, K::Nudge, 0, 0, "", 0, 1, "", "Nudge up one layer, towards +X", "nudge.up.px" },
      { 1, K::Nudge, 0, 0, "", 90, 1, "", "Nudge up one layer, towards +Y", "nudge.up.py" },
      { 2, K::Nudge, 0, 0, "", 180, 1, "", "Nudge up one layer, towards −X", "nudge.up.nx" },
      { 3, K::Nudge, 0, 0, "", 270, 1, "", "Nudge up one layer, towards −Y", "nudge.up.ny" },
      { 4, K::Nudge, 0, 0, "", 45, 0, "", "Nudge in the layer, towards +X +Y", "nudge.in.pxpy" },
      { 5, K::Nudge, 0, 0, "", 135, 0, "", "Nudge in the layer, towards −X +Y", "nudge.in.nxpy" },
      { 27, K::Nudge, 0, 0, "", 225, 0, "", "Nudge in the layer, towards −X −Y", "nudge.in.nxny" },
      { 28, K::Nudge, 0, 0, "", 315, 0, "", "Nudge in the layer, towards +X −Y", "nudge.in.pxny" },
      { 29, K::Nudge, 0, 0, "", 0, -1, "", "Nudge down one layer, towards +X", "nudge.down.px" },
      { 30, K::Nudge, 0, 0, "", 90, -1, "", "Nudge down one layer, towards +Y", "nudge.down.py" },
      { 31, K::Nudge, 0, 0, "", 180, -1, "", "Nudge down one layer, towards −X", "nudge.down.nx" },
      { 32, K::Nudge, 0, 0, "", 270, -1, "", "Nudge down one layer, towards −Y", "nudge.down.ny" },
    } };
    switch (t) {
      case gridType_c::GT_TRIANGULAR_PRISM: return prism;
      case gridType_c::GT_SPHERES: return spheres;
      case gridType_c::GT_RHOMBIC: return rhombic;
      case gridType_c::GT_TETRA_OCTA: return tetra;
      case gridType_c::GT_BRICKS:
      case gridType_c::GT_NUM_GRIDS: return bricks;
    }
    return bricks;
  }

  // --- running ---------------------------------------------------------------------

  bool applyTask(voxel_c & v, int task) {
    bool ok = true;
    const G t = typeOf(v);
    if (const Move * m = moveOf(t, task)) {
      if (m->transform >= 0)
        ok = v.transform(unsigned(m->transform));
      else
        v.translate(m->dx, m->dy, m->dz, 0);
    } else {
      switch (task) {
        case task::Prune: v.minimizePiece(); break;
        case task::FixedInner: surfaceAction(v, voxel_c::ACT_FIXED, true); break;
        case task::FixedOuter: surfaceAction(v, voxel_c::ACT_FIXED, false); break;
        case task::VariableInner: surfaceAction(v, voxel_c::ACT_VARIABLE, true); break;
        case task::VariableOuter: surfaceAction(v, voxel_c::ACT_VARIABLE, false); break;
        case task::ClearInner: surfaceAction(v, voxel_c::ACT_DECOLOR, true); break;
        case task::ClearOuter: surfaceAction(v, voxel_c::ACT_DECOLOR, false); break;
        case task::Scale2: v.scale(2); break;
        case task::Scale3: v.scale(3); break;
        case task::Origin: if (!empty(v)) origin(v); break;
        case task::Center: if (!empty(v)) center(v); break;
        case task::FillHoles: v.fillHoles(0); break;
        default: ok = false; break;
      }
    }
    v.initHotspot();
    return ok;
  }

  int minimizeFactor(const puzzle_c & p, const std::vector<unsigned int> & shapes) {
    // legacy ToolTab_0::cb_transform: each prime that divides every shape, as often as it does
    std::vector<std::unique_ptr<voxel_c>> copies;
    for (unsigned int s : shapes)
      if (s < p.getNumberOfShapes())
        copies.push_back(copyOf(*p.getShape(s)));
    if (copies.empty())
      return 1;
    int factor = 1;
    for (unsigned char prime : { 2, 3, 5, 7, 11, 13, 17, 19 }) {
      for (;;) {
        bool all = true;
        for (const auto & c : copies)
          if (!c->scaleDown(prime, false)) {
            all = false;
            break;
          }
        if (!all)
          break;
        for (const auto & c : copies)
          c->scaleDown(prime, true);
        factor *= prime;
      }
    }
    return factor;
  }

  OpCheck checkTask(const puzzle_c & p, const std::vector<unsigned int> & shapes, int task) {
    std::vector<const voxel_c *> vs;
    for (unsigned int s : shapes)
      if (s < p.getNumberOfShapes())
        vs.push_back(p.getShape(s));
    if (vs.empty())
      return { false, "No shape selected" };
    const G t = p.getGridType()->getType();
    const OpsDescriptor & d = opsDescriptor(t);

    if (isTransform(t, task)) {
      // a transform that would lose voxels -- out of the grid -- is refused (C04, OQ-7)
      for (const voxel_c * v : vs) {
        if (empty(*v))
          continue;
        auto c = copyOf(*v);
        if (!applyTask(*c, task) || filled(*c) != filled(*v))
          return { false, "Blocked: the piece would leave the grid" };
      }
      return {};
    }
    if (isFit(task)) {
      for (const voxel_c * v : vs)
        if (!empty(*v))
          return {};
      return { false, "The shape is empty" };
    }
    if (task == task::Scale2 || task == task::Scale3) {
      if (!d.scale)
        return { false, "Not available for this voxel type" };
      const int n = task == task::Scale2 ? 2 : 3;
      const int step = sizeStep(t);
      for (const voxel_c * v : vs)
        for (unsigned int dim : { v->getX(), v->getY(), v->getZ() })
          if ((int(dim) + step - 1) / step * n > maxGridSteps)
            return { false, "Blocked: grid would exceed " + std::to_string(maxGridSteps) };
      return {};
    }
    if (task == task::Minimize) {
      if (!d.minimize)
        return { false, "Not available for this voxel type" };
      // an empty grid divides by anything: nothing to reduce
      if (std::all_of(vs.begin(), vs.end(), [](const voxel_c * v) { return empty(*v); }))
        return { false, "The shape is empty" };
      if (minimizeFactor(p, shapes) <= 1)
        return { false, "Already at minimum resolution" };
      return {};
    }
    return {};
  }

  OpResult runTask(puzzle_c & p, const std::vector<unsigned int> & shapes, int task) {
    OpResult r;
    std::vector<voxel_c *> vs;
    for (unsigned int s : shapes)
      if (s < p.getNumberOfShapes())
        vs.push_back(p.getShape(s));

    if (task == task::Minimize) {
      const int factor = minimizeFactor(p, shapes);
      if (factor <= 1)
        return r;
      for (unsigned char prime : { 2, 3, 5, 7, 11, 13, 17, 19 })
        for (int f = factor; f % prime == 0; f /= prime)
          for (voxel_c * v : vs)
            v->scaleDown(prime, true);
      for (voxel_c * v : vs)
        v->initHotspot();
      r.changed = true;
      r.message = "Resolution reduced ÷" + std::to_string(factor);
      return r;
    }

    const bool inner = task == task::FixedInner || task == task::VariableInner || task == task::ClearInner;
    const bool surface = task >= task::FixedInner && task <= task::ClearOuter;
    int counted = 0;
    const bool transform = isTransform(p.getGridType()->getType(), task);
    for (voxel_c * v : vs) {
      // nothing to fit or move in an empty shape (C04: no-op, no flash)
      if ((isFit(task) || transform) && empty(*v))
        continue;
      auto before = copyOf(*v);
      const unsigned int filledBefore = filled(*v);
      if (surface) {
        static constexpr voxel_c::VoxelAction actions[] = { voxel_c::ACT_FIXED, voxel_c::ACT_VARIABLE, voxel_c::ACT_DECOLOR };
        counted += surfaceAction(*v, actions[(task - task::FixedInner) / 2], inner);
        v->initHotspot();
      } else {
        applyTask(*v, task);
        if (task == task::FillHoles)
          counted += int(filled(*v)) - int(filledBefore);
      }
      r.changed = r.changed || !before->identicalInBB(v, true) || v->getX() != before->getX() ||
                  v->getY() != before->getY() || v->getZ() != before->getZ() ||
                  v->boundX1() != before->boundX1() || v->boundY1() != before->boundY1() ||
                  v->boundZ1() != before->boundZ1();
    }
    if (task == task::FillHoles)
      r.message = counted > 0 ? "Filled " + std::to_string(counted) + (counted == 1 ? " interior voxel" : " interior voxels")
                              : "No interior holes found";
    else if (surface)
      r.message = counted > 0 ? std::to_string(counted) + (inner ? " inner" : " outer") + (counted == 1 ? " voxel updated" : " voxels updated")
                              : std::string(inner ? "No inner voxel to change" : "No outer voxel to change");
    return r;
  }

  std::vector<bool> outerVoxels(const voxel_c & v) {
    const unsigned int n = v.getXYZ();
    std::vector<bool> exterior(n, false), outer(n, false);
    std::deque<unsigned int> todo;
    unsigned int x, y, z;
    int nx, ny, nz;
    // the empty cells beside the outside, then every empty cell they reach
    for (unsigned int i = 0; i < n; i++) {
      if (v.getState(i) != voxel_c::VX_EMPTY || !v.indexToXYZ(i, &x, &y, &z) || !v.validCoordinate(int(x), int(y), int(z)))
        continue;
      for (unsigned int k = 0; v.getNeighbor(k, 0, int(x), int(y), int(z), &nx, &ny, &nz); k++)
        if (!inGrid(v, nx, ny, nz)) {
          exterior[i] = true;
          todo.push_back(i);
          break;
        }
    }
    while (!todo.empty()) {
      const unsigned int i = todo.front();
      todo.pop_front();
      v.indexToXYZ(i, &x, &y, &z);
      for (unsigned int k = 0; v.getNeighbor(k, 0, int(x), int(y), int(z), &nx, &ny, &nz); k++) {
        if (!inGrid(v, nx, ny, nz))
          continue;
        const unsigned int j = unsigned(v.getIndex(unsigned(nx), unsigned(ny), unsigned(nz)));
        if (!exterior[j] && v.getState(j) == voxel_c::VX_EMPTY) {
          exterior[j] = true;
          todo.push_back(j);
        }
      }
    }
    for (unsigned int i = 0; i < n; i++) {
      if (v.getState(i) == voxel_c::VX_EMPTY || !v.indexToXYZ(i, &x, &y, &z))
        continue;
      for (unsigned int k = 0; v.getNeighbor(k, 0, int(x), int(y), int(z), &nx, &ny, &nz); k++)
        if (!inGrid(v, nx, ny, nz) || exterior[unsigned(v.getIndex(unsigned(nx), unsigned(ny), unsigned(nz)))]) {
          outer[i] = true;
          break;
        }
    }
    return outer;
  }

  // --- the hover preview ---------------------------------------------------------------

  namespace {

    // the null space of (M + lambda I) as the longest cross product of two rows (legacy)
    bool nullSpace(const double m[9], double lambda, Vec3 & axis) {
      float a[9];
      for (int i = 0; i < 9; i++)
        a[i] = float(m[i]) + ((i % 3 == i / 3) ? float(lambda) : 0.0f);
      const Vec3 cand[3] = {
        { a[1] * a[5] - a[2] * a[4], a[2] * a[3] - a[0] * a[5], a[0] * a[4] - a[1] * a[3] },
        { a[1] * a[8] - a[2] * a[7], a[2] * a[6] - a[0] * a[8], a[0] * a[7] - a[1] * a[6] },
        { a[4] * a[8] - a[5] * a[7], a[5] * a[6] - a[3] * a[8], a[3] * a[7] - a[4] * a[6] },
      };
      int best = -1;
      float bestLen = 1e-4f;
      for (int i = 0; i < 3; i++)
        if (length(cand[i]) > bestLen) {
          bestLen = length(cand[i]);
          best = i;
        }
      if (best < 0)
        return false;
      axis = cand[best] * (1.0f / bestLen);
      return true;
    }
  }

  TaskPreview previewTask(const voxel_c & v, int task) {
    TaskPreview out;
    const Move * m = moveOf(typeOf(v), task);
    if (!m)
      return out;
    auto c = copyOf(v);
    if (!applyTask(*c, task) || filled(*c) != filled(v)) {
      out.blocked = true;
    } else {
      out.result = std::move(c);
    }

    if (m->transform < 0) {
      // the nudge's direction in drawing space
      float ox = 0, oy = 0, oz = 0, x = float(m->dx), y = float(m->dy), zz = float(m->dz);
      v.recalcSpaceCoordinates(&ox, &oy, &oz);
      v.recalcSpaceCoordinates(&x, &y, &zz);
      const Vec3 d{ x - ox, y - oy, zz - oz };
      if (length(d) > 1e-6f) {
        out.hint = TaskPreview::Hint::Nudge;
        out.axis = normalize(d);
      }
      return out;
    }

    // legacy computeTransformGeometry: the transform's linear part says what it is
    double mat[9];
    v.getTransformMatrix(unsigned(m->transform), mat);
    const double det = mat[0] * (mat[4] * mat[8] - mat[5] * mat[7]) - mat[1] * (mat[3] * mat[8] - mat[5] * mat[6]) +
                       mat[2] * (mat[3] * mat[7] - mat[4] * mat[6]);
    if (det > 0) {
      const float cosA = std::clamp(float(mat[0] + mat[4] + mat[8] - 1.0) * 0.5f, -1.0f, 1.0f);
      const float angle = std::acos(cosA);
      if (angle < 0.02f)
        return out;
      // the antisymmetric part gives the axis with its sign; at 180° it vanishes
      const Vec3 w{ float(mat[7] - mat[5]), float(mat[2] - mat[6]), float(mat[3] - mat[1]) };
      if (length(w) > 1e-4f)
        out.axis = normalize(w);
      else if (!nullSpace(mat, -1.0, out.axis))
        return out;
      out.hint = TaskPreview::Hint::Rotate;
      out.angleDeg = angle * 180.0f / 3.14159265f;
    } else if (det < 0) {
      // a mirror: the plane's normal is the −1 eigenvector
      if (nullSpace(mat, 1.0, out.axis))
        out.hint = TaskPreview::Hint::Mirror;
    }
    return out;
  }
}
