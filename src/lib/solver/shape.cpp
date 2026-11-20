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
#include "shape.h"

#include "../bt_assert.h"
#include "../gridtype.h"
#include "../latticegridinfo.h"
#include "../voxel.h"

#include <algorithm>
#include <climits>

namespace {

int floorDiv(int a, int b) {
  bt_assert(b > 0);
  int q = a / b;
  int r = a % b;
  if (r < 0) q--;
  return q;
}

} // namespace

shape_c::shape_c(std::vector<cellEntry_t> cells) : cells_(std::move(cells)) {
  canonicaliseOrder();
}

void shape_c::canonicaliseOrder(void) {
  // Drop any empty cells, sort by cellAddr_t, and deduplicate (last wins).
  cells_.erase(
      std::remove_if(cells_.begin(), cells_.end(),
                     [](const cellEntry_t & e) { return e.state == cellState_e::Empty; }),
      cells_.end());
  std::stable_sort(cells_.begin(), cells_.end(),
                   [](const cellEntry_t & a, const cellEntry_t & b) {
                     return a.cell < b.cell;
                   });
  if (cells_.size() > 1) {
    size_t write = 0;
    for (size_t read = 0; read < cells_.size(); read++) {
      if (write > 0 && cells_[write - 1].cell == cells_[read].cell) {
        cells_[write - 1] = cells_[read];
      } else {
        cells_[write++] = cells_[read];
      }
    }
    cells_.resize(write);
  }
}

size_t shape_c::countState(cellState_e st) const {
  size_t c = 0;
  for (const auto & e : cells_)
    if (e.state == st)
      c++;
  return c;
}

cellEntry_t shape_c::get(cellAddr_t addr) const {
  auto it = std::lower_bound(cells_.begin(), cells_.end(), addr,
                             [](const cellEntry_t & e, const cellAddr_t & a) {
                               return e.cell < a;
                             });
  if (it != cells_.end() && it->cell == addr)
    return *it;
  return { addr, cellState_e::Empty, 0 };
}

void shape_c::set(cellAddr_t addr, cellState_e state, uint16_t color) {
  auto it = std::lower_bound(cells_.begin(), cells_.end(), addr,
                             [](const cellEntry_t & e, const cellAddr_t & a) {
                               return e.cell < a;
                             });
  if (state == cellState_e::Empty) {
    if (it != cells_.end() && it->cell == addr)
      cells_.erase(it);
    return;
  }
  if (it != cells_.end() && it->cell == addr) {
    it->state = state;
    it->color = color;
  } else {
    cells_.insert(it, { addr, state, color });
  }
}

shape_c shape_c::translated(const vec3i_t & dn) const {
  shape_c out;
  out.cells_.reserve(cells_.size());
  for (const auto & e : cells_)
    out.cells_.push_back({ e.cell.translated(dn), e.state, e.color });
  // Adding a constant dn to every n preserves lexicographical order on (n, k).
  return out;
}

shape_c shape_c::normalised(vec3i_t * shiftOut) const {
  if (cells_.empty()) {
    if (shiftOut) *shiftOut = {};
    return {};
  }
  const vec3i_t origin = cells_.front().cell.n;
  if (shiftOut) *shiftOut = origin;
  return translated(-origin);
}

bool shape_c::equalUpToTranslation(const shape_c & other, bool includeColors) const {
  if (cells_.size() != other.cells_.size())
    return false;
  if (cells_.empty())
    return true;
  const vec3i_t dn = other.cells_.front().cell.n - cells_.front().cell.n;
  for (size_t i = 0; i < cells_.size(); i++) {
    const auto & a = cells_[i];
    const auto & b = other.cells_[i];
    if (a.cell.n + dn != b.cell.n || a.cell.k != b.cell.k || a.state != b.state)
      return false;
    if (includeColors && a.color != b.color)
      return false;
  }
  return true;
}

std::optional<shape_c> shape_c::transformed(const gridType_c & grid, unsigned int turn) const {
  if (cells_.empty())
    return shape_c{};
  std::vector<cellEntry_t> out = cells_;
  if (!grid.transformCells(out, turn))
    return std::nullopt;
  return shape_c(std::move(out));
}

std::optional<shape_c> shape_c::placed(const gridType_c & grid, const gridPlacement_t & p) const {
  auto t = transformed(grid, p.turn);
  if (!t)
    return std::nullopt;
  return t->translated(p.translation);
}

std::vector<unsigned int> shape_c::symmetries(const gridType_c & grid, bool includeColors) const {
  std::vector<unsigned int> res;
  const symmetries_c * sym = grid.getSymmetries();
  if (!sym)
    return res;
  const unsigned int nt = sym->getNumTransformationsMirror();
  for (unsigned int t = 0; t < nt; t++) {
    auto mapped = transformed(grid, t);
    if (mapped && equalUpToTranslation(*mapped, includeColors))
      res.push_back(t);
  }
  return res;
}

symmetries_t shape_c::selfSymmetries(const gridType_c & grid) const {
  const symmetries_c * sym = grid.getSymmetries();
  if (!sym)
    return symmetryInvalid();
  auto v = toVoxel(grid);
  return sym->calculateSymmetry(v.get());
}

shape_c shape_c::fromVoxel(const voxel_c & v) {
  const gridType_c * gt = v.getGridType();
  bt_assert(gt != nullptr);

  std::vector<cellEntry_t> entries;
  if (v.getXYZ() == 0)
    return {};

  for (unsigned int z = v.boundZ1(); z <= v.boundZ2(); z++)
    for (unsigned int y = v.boundY1(); y <= v.boundY2(); y++)
      for (unsigned int x = v.boundX1(); x <= v.boundX2(); x++) {
        const int st = v.getState(x, y, z);
        if (st == voxel_c::VX_EMPTY)
          continue;
        auto addr = gt->voxelToCell({ static_cast<int>(x), static_cast<int>(y), static_cast<int>(z) });
        if (!addr)
          bt_te("shape_c::fromVoxel: filled voxel at invalid grid coordinate");
        cellState_e cs = (st == voxel_c::VX_VARIABLE) ? cellState_e::Variable : cellState_e::Filled;
        uint16_t col = static_cast<uint16_t>(v.getColor(x, y, z));
        entries.push_back({ *addr, cs, col });
      }

  return shape_c(std::move(entries));
}

std::unique_ptr<voxel_c> shape_c::toVoxel(const gridType_c & grid) const {
  if (cells_.empty()) {
    std::unique_ptr<voxel_c> empty(grid.getVoxel(1, 1, 1, voxel_c::VX_EMPTY));
    empty->initHotspot();
    return empty;
  }

  std::vector<std::pair<vec3i_t, voxel_type>> raw;
  raw.reserve(cells_.size());
  int mx = INT_MAX, my = INT_MAX, mz = INT_MAX;
  for (const auto & e : cells_) {
    const vec3i_t p = grid.cellToVoxel(e.cell);
    voxel_type val = (e.state == cellState_e::Variable) ? voxel_c::VX_VARIABLE : voxel_c::VX_FILLED;
    val |= static_cast<voxel_type>(e.color << 2);
    raw.push_back({ p, val });
    mx = std::min(mx, p.x);
    my = std::min(my, p.y);
    mz = std::min(mz, p.z);
  }

  // Choose a grid-valid lattice translation vector v <= (mx, my, mz) so all
  // shifted voxel coordinates are non-negative and stay on the same grid class.
  int vx = 0, vy = 0, vz = 0;
  switch (grid.getType()) {
    case gridType_c::GT_BRICKS:
      vx = mx; vy = my; vz = mz;
      break;
    case gridType_c::GT_TRIANGULAR_PRISM:
      vx = mx; vy = my; vz = mz;
      if ((vx + vy) & 1) vx--;
      break;
    case gridType_c::GT_SPHERES:
      vx = mx; vy = my; vz = mz;
      if ((vx + vy + vz) & 1) vx--;
      break;
    case gridType_c::GT_RHOMBIC:
      vx = 5 * floorDiv(mx, 5);
      vy = 5 * floorDiv(my, 5);
      vz = 5 * floorDiv(mz, 5);
      break;
    case gridType_c::GT_TETRA_OCTA: {
      int cx = floorDiv(mx, 3);
      int cy = floorDiv(my, 3);
      int cz = floorDiv(mz, 3);
      if ((cx + cy + cz) & 1) cx--;
      vx = 3 * cx;
      vy = 3 * cy;
      vz = 3 * cz;
      break;
    }
    default:
      bt_assert(grid.isDescribed());
      latticeGridInfo_c::get(grid.getType()).alignDown(mx, my, mz, vx, vy, vz);
      break;
  }

  int sx = 1, sy = 1, sz = 1;
  for (const auto & [p, _] : raw) {
    sx = std::max(sx, p.x - vx + 1);
    sy = std::max(sy, p.y - vy + 1);
    sz = std::max(sz, p.z - vz + 1);
  }

  std::unique_ptr<voxel_c> out(grid.getVoxel(
      static_cast<unsigned int>(sx),
      static_cast<unsigned int>(sy),
      static_cast<unsigned int>(sz),
      voxel_c::VX_EMPTY));
  out->skipRecalcBoundingBox(true);
  for (const auto & [p, val] : raw)
    out->set(static_cast<unsigned int>(p.x - vx),
             static_cast<unsigned int>(p.y - vy),
             static_cast<unsigned int>(p.z - vz),
             val);
  out->skipRecalcBoundingBox(false);
  out->initHotspot();
  return out;
}
