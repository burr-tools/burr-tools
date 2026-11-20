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
#include "assembler.h"

#include "solver/solverspec.h"
#include "voxel.h"
#include "symmetries.h"
#include "gridtype.h"
#include "assembly.h"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <thread>

placementFinder_c::placementFinder_c(const solverSpec_t & problem,
                                     const gridType_c & grid,
                                     const shape_c & resultShape,
                                     const std::vector<unsigned int> & resCellCols)
  : kindCount_(grid.kindCount()),
    resShape_(&resultShape),
    resCols_(resCellCols),
    byKind_(kindCount_)
{
  bt_assert(resCellCols.size() == resultShape.size());

  colorTableWidth_ = problem.colorNumber() + 1;
  if (colorTableWidth_ > 1) {
    colorAllowed_.resize(colorTableWidth_ * colorTableWidth_);
    for (unsigned int p = 0; p < colorTableWidth_; p++)
      for (unsigned int r = 0; r < colorTableWidth_; r++)
        colorAllowed_[p * colorTableWidth_ + r] = problem.placementAllowed(p, r) ? 1 : 0;
  }

  if (resultShape.empty())
    return;

  const auto & cells = resultShape.cells();
  nMin_ = cells.front().cell.n;
  nMax_ = cells.front().cell.n;
  for (const auto & e : cells) {
    nMin_.x = std::min(nMin_.x, e.cell.n.x);
    nMin_.y = std::min(nMin_.y, e.cell.n.y);
    nMin_.z = std::min(nMin_.z, e.cell.n.z);
    nMax_.x = std::max(nMax_.x, e.cell.n.x);
    nMax_.y = std::max(nMax_.y, e.cell.n.y);
    nMax_.z = std::max(nMax_.z, e.cell.n.z);
  }

  nx_ = static_cast<long>(nMax_.x) - nMin_.x + 1;
  ny_ = static_cast<long>(nMax_.y) - nMin_.y + 1;
  nz_ = static_cast<long>(nMax_.z) - nMin_.z + 1;

  // Use a tight dense (n - nMin, k) lookup when bounded; fall back to binary
  // search on resShape_->cells() for pathologically spread shapes.
  constexpr uint64_t kMaxDenseSlots = 4000000ULL;
  const uint64_t totalSlots =
      static_cast<uint64_t>(nx_) * static_cast<uint64_t>(ny_) *
      static_cast<uint64_t>(nz_) * static_cast<uint64_t>(kindCount_);
  useDense_ = (nx_ > 0 && ny_ > 0 && nz_ > 0 && totalSlots <= kMaxDenseSlots);

  if (useDense_)
    grid_.resize(static_cast<size_t>(totalSlots));

  for (size_t i = 0; i < cells.size(); i++) {
    const auto & e = cells[i];
    bt_assert(e.cell.k < kindCount_);
    long gIdx = 0;
    if (useDense_) {
      gIdx = static_cast<long>(e.cell.k) +
             static_cast<long>(kindCount_) * (
               (e.cell.n.x - nMin_.x) + nx_ * (
                 (e.cell.n.y - nMin_.y) + ny_ * static_cast<long>(e.cell.n.z - nMin_.z)));
      grid_[static_cast<size_t>(gIdx)] = { resCellCols[i], e.color, e.state };
    }
    byKind_[e.cell.k].push_back({ e.cell.n, gIdx, resCellCols[i], e.color });
  }
}

void placementFinder_c::find(const shape_c & rotShape,
                             const std::function<void(vec3i_t dn, const std::vector<unsigned int> & cols)> & emit) const
{
  if (rotShape.empty() || !resShape_ || resShape_->empty())
    return;

  struct pieceCell_s {
    cellAddr_t cell;
    long denseOff = 0;
    uint16_t color = 0;
  };

  std::vector<pieceCell_s> filled;
  std::vector<pieceCell_s> colorChecks;
  filled.reserve(rotShape.size());

  const long K = static_cast<long>(kindCount_);
  vec3i_t pMin{ INT_MAX, INT_MAX, INT_MAX };
  vec3i_t pMax{ INT_MIN, INT_MIN, INT_MIN };

  for (const auto & e : rotShape.cells()) {
    if (e.state == cellState_e::Filled) {
      pMin.x = std::min(pMin.x, e.cell.n.x);
      pMin.y = std::min(pMin.y, e.cell.n.y);
      pMin.z = std::min(pMin.z, e.cell.n.z);
      pMax.x = std::max(pMax.x, e.cell.n.x);
      pMax.y = std::max(pMax.y, e.cell.n.y);
      pMax.z = std::max(pMax.z, e.cell.n.z);
      const long off = useDense_
          ? static_cast<long>(e.cell.k) + K * (e.cell.n.x + nx_ * (e.cell.n.y + ny_ * static_cast<long>(e.cell.n.z)))
          : 0;
      filled.push_back({ e.cell, off, e.color });
    } else if (colorTableWidth_ > 1 && e.color != 0) {
      const long off = useDense_
          ? static_cast<long>(e.cell.k) + K * (e.cell.n.x + nx_ * (e.cell.n.y + ny_ * static_cast<long>(e.cell.n.z)))
          : 0;
      colorChecks.push_back({ e.cell, off, e.color });
    }
  }

  if (filled.empty())
    return;

  const vec3i_t dnMin = nMin_ - pMin;
  const vec3i_t dnMax = nMax_ - pMax;
  if (dnMax.x < dnMin.x || dnMax.y < dnMin.y || dnMax.z < dnMin.z)
    return;

  const pieceCell_s & anchor = filled.front();
  const auto & candidates = byKind_[anchor.cell.k];
  if (candidates.empty())
    return;

  std::vector<unsigned int> cols(filled.size());
  const bool checkColors = (colorTableWidth_ > 1);

  if (useDense_) {
    for (const auto & rc : candidates) {
      const vec3i_t dn = rc.n - anchor.cell.n;
      if (dn.x < dnMin.x || dn.x > dnMax.x ||
          dn.y < dnMin.y || dn.y > dnMax.y ||
          dn.z < dnMin.z || dn.z > dnMax.z)
        continue;

      if (checkColors && !colorAllowed_[anchor.color * colorTableWidth_ + rc.color])
        continue;

      const long base = rc.gridIdx - anchor.denseOff;
      cols[0] = rc.col;

      bool ok = true;
      for (size_t i = 1; i < filled.size(); i++) {
        const cellSlot_s & slot = grid_[static_cast<size_t>(base + filled[i].denseOff)];
        if (slot.state == cellState_e::Empty) {
          ok = false;
          break;
        }
        if (checkColors && !colorAllowed_[filled[i].color * colorTableWidth_ + slot.color]) {
          ok = false;
          break;
        }
        cols[i] = slot.col;
      }
      if (!ok)
        continue;

      for (const auto & cc : colorChecks) {
        const vec3i_t tn = cc.cell.n + dn;
        uint16_t resColor = 0;
        if (tn.x >= nMin_.x && tn.x <= nMax_.x &&
            tn.y >= nMin_.y && tn.y <= nMax_.y &&
            tn.z >= nMin_.z && tn.z <= nMax_.z) {
          resColor = grid_[static_cast<size_t>(base + cc.denseOff)].color;
        }
        if (!colorAllowed_[cc.color * colorTableWidth_ + resColor]) {
          ok = false;
          break;
        }
      }
      if (!ok)
        continue;

      emit(dn, cols);
    }
  } else {
    const auto & rCells = resShape_->cells();
    auto lookup = [&](const cellAddr_t & addr) -> ptrdiff_t {
      auto it = std::lower_bound(rCells.begin(), rCells.end(), addr,
                                 [](const cellEntry_t & e, const cellAddr_t & a) {
                                   return e.cell < a;
                                 });
      if (it != rCells.end() && it->cell == addr)
        return it - rCells.begin();
      return -1;
    };

    for (const auto & rc : candidates) {
      const vec3i_t dn = rc.n - anchor.cell.n;
      if (dn.x < dnMin.x || dn.x > dnMax.x ||
          dn.y < dnMin.y || dn.y > dnMax.y ||
          dn.z < dnMin.z || dn.z > dnMax.z)
        continue;

      if (checkColors && !colorAllowed_[anchor.color * colorTableWidth_ + rc.color])
        continue;

      cols[0] = rc.col;
      bool ok = true;
      for (size_t i = 1; i < filled.size(); i++) {
        const ptrdiff_t idx = lookup(filled[i].cell.translated(dn));
        if (idx < 0) {
          ok = false;
          break;
        }
        if (checkColors && !colorAllowed_[filled[i].color * colorTableWidth_ + rCells[idx].color]) {
          ok = false;
          break;
        }
        cols[i] = resCols_[idx];
      }
      if (!ok)
        continue;

      for (const auto & cc : colorChecks) {
        const ptrdiff_t idx = lookup(cc.cell.translated(dn));
        const uint16_t resColor = (idx >= 0) ? rCells[idx].color : 0;
        if (!colorAllowed_[cc.color * colorTableWidth_ + resColor]) {
          ok = false;
          break;
        }
      }
      if (!ok)
        continue;

      emit(dn, cols);
    }
  }
}

assembler_c::errState assembler_c::createMatrix(bool /*keepRotations*/, bool /*complete*/)
{
  return ERR_NONE;
}

assembler_c::errState assembler_c::setPosition(const char * /*string*/, const char * /*version*/)
{
  return ERR_CAN_NOT_RESTORE_VERSION;
}

void assembler_c::stop(void) {
  std::lock_guard<std::mutex> lock(runStopMutex);
  runStop.request_stop();
}

std::pair<std::string, std::string> assembler_c::saveState(void) const
{
  return { "", "" };
}

void assembler_c::assemble(std::function<bool(std::unique_ptr<assembly_c>)> callback_fn)
{
  class fn_adapter_cb : public assembler_cb {
    std::function<bool(std::unique_ptr<assembly_c>)> fn;
  public:
    explicit fn_adapter_cb(std::function<bool(std::unique_ptr<assembly_c>)> f) : fn(std::move(f)) {}
    bool assembly(std::unique_ptr<assembly_c> a) override {
      return fn(std::move(a));
    }
  };
  fn_adapter_cb adapter(std::move(callback_fn));
  assemble(&adapter);
}

void assembler_c::prewarmSharedShapeCaches(const solverSpec_t & problem) {

  const symmetries_c * sym = problem.getGridType()->getSymmetries();
  const unsigned int numTrans = sym ? sym->getNumTransformationsMirror() : 0;

  auto warm = [numTrans](const voxel_c * v) {
    if (!v) return;

    /* selfSymmetries() first: normalizeTransformation() goes through it for
     * every placement, so it is the most frequently raced of the three
     */
    v->selfSymmetries();

    for (unsigned int t = 0; t < numTrans; t++) {
      int x, y, z;
      int x1, x2, y1, y2, z1, z2;
      v->getHotspot(t, &x, &y, &z);
      v->getBoundingBox(t, &x1, &x2, &y1, &y2, &z1, &z2);
    }
  };

  warm(getResultShape(problem));

  for (unsigned int i = 0; i < problem.getNumberOfParts(); i++)
    warm(problem.getPartShape(i));
}

unsigned int assembler_c::getEffectiveThreads(void) const {

  if (settings.threads > 0)
    return settings.threads;   /* already clamped */

  unsigned int hw = std::thread::hardware_concurrency();
  return (hw > 0) ? std::min(hw, MAX_THREADS) : 1;
}
