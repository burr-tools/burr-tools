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
#ifndef __SOLVER_SHAPE_H__
#define __SOLVER_SHAPE_H__

/** @file
 * Grid-neutral cell addresses (n, k), placements, and shapes
 * (design/2026-11-19-ng-merge-plan.md, sections 5.2 and 5.3, step B6;
 * design/2026-11-20-core-api.md, section 2).
 *
 * Pure core types: no I/O, no XML, no problem_c or puzzle_c.
 */

#include "math.h"
#include "../symmetries.h"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

class gridType_c;
class voxel_c;

/**
 * Cell address (n, k):
 * - n: lattice-cell / epiped translation coordinates (vec3i_t, e.g. Z^3 for
 *      periodic grids or (theta mod N, z, 0) for cylinders).
 * - k: cell-kind index in [0, grid.kindCount()).
 *
 * Unlike raw embedded (x, y, z) voxel coordinates (which leave most triples
 * unused on complex grids so rotations stay linear), (n, k) decomposes each
 * cell into its repeating epiped n and base kind k inside that epiped
 * (p = base[k] + L * n). Every (n, k) with 0 <= k < kindCount names a real
 * cell, and translating by dn is simply (n + dn, k).
 */
struct cellAddr_t {
  vec3i_t n{};
  uint16_t k = 0;

  constexpr bool operator==(const cellAddr_t &) const = default;
  constexpr auto operator<=>(const cellAddr_t &) const = default;

  constexpr cellAddr_t translated(const vec3i_t & dn) const {
    return { n + dn, k };
  }
};

enum class cellState_e : uint8_t {
  Empty    = 0,
  Filled   = 1,
  Variable = 2,
};

struct cellEntry_t {
  cellAddr_t cell{};
  cellState_e state = cellState_e::Filled;
  uint16_t color = 0;

  constexpr bool operator==(const cellEntry_t &) const = default;
  constexpr auto operator<=>(const cellEntry_t &) const = default;
};

/** Placement (g, t) of a shape on a grid: turn index plus lattice translation. */
struct gridPlacement_t {
  uint16_t turn = 0;
  vec3i_t translation{};

  constexpr bool operator==(const gridPlacement_t &) const = default;
};

/**
 * Grid-neutral shape: a finite set of non-empty cells over (n, k), each with
 * state (Filled or Variable) and colour. No box size, hotspot, or embedded
 * voxel array in the contract.
 *
 * Cells are kept sorted ascending by cellAddr_t with unique addresses.
 */
class shape_c {
public:
  shape_c() = default;
  explicit shape_c(std::vector<cellEntry_t> cells);

  bool empty(void) const { return cells_.empty(); }
  size_t size(void) const { return cells_.size(); }
  const std::vector<cellEntry_t> & cells(void) const { return cells_; }

  size_t countState(cellState_e st) const;
  size_t filledCount(void) const { return countState(cellState_e::Filled); }
  size_t variableCount(void) const { return countState(cellState_e::Variable); }

  /** Lookup cell state/colour; returns Empty/0 if absent. */
  cellEntry_t get(cellAddr_t addr) const;
  /** Set or clear (when state == Empty) a cell. */
  void set(cellAddr_t addr, cellState_e state, uint16_t color = 0);
  void clear(void) { cells_.clear(); }

  bool operator==(const shape_c &) const = default;

  /** Translate every cell by lattice vector dn: (n, k) -> (n + dn, k). */
  shape_c translated(const vec3i_t & dn) const;

  /**
   * Canonical form up to lattice translation: translates so the first
   * (lexicographically smallest) cell sits at n == (0, 0, 0).
   * If shiftOut is non-null, writes the subtracted translation n0 (or {0,0,0}
   * if empty), so `normalised().translated(shift) == *this`.
   */
  shape_c normalised(vec3i_t * shiftOut = nullptr) const;

  /** Test whether *this and other differ only by a lattice translation. */
  bool equalUpToTranslation(const shape_c & other, bool includeColors = true) const;

  /**
   * Apply grid turn g to the shape. Returns std::nullopt if the turn is not
   * valid for this shape (e.g. a sphere turn that moves a ball off the FCC
   * packing).
   */
  std::optional<shape_c> transformed(const gridType_c & grid, unsigned int turn) const;

  /** Apply placement (g, t): turn first, then translate by t. */
  std::optional<shape_c> placed(const gridType_c & grid, const gridPlacement_t & p) const;

  /**
   * All turns g in [0, numTransformationsMirror) that map this shape onto
   * itself up to a lattice translation.
   */
  std::vector<unsigned int> symmetries(const gridType_c & grid, bool includeColors = true) const;

  /**
   * Shape's symmetry group in the grid's symmetries_t encoding (matches
   * voxel_c::selfSymmetries()).
   */
  symmetries_t selfSymmetries(const gridType_c & grid) const;

  /** Extract a shape over (n, k) from a dense voxel_c. */
  static shape_c fromVoxel(const voxel_c & v);

  /** Materialise this shape into dense voxel_c storage for the given grid. */
  std::unique_ptr<voxel_c> toVoxel(const gridType_c & grid) const;

private:
  void canonicaliseOrder(void);

  std::vector<cellEntry_t> cells_;
};

#endif // __SOLVER_SHAPE_H__
