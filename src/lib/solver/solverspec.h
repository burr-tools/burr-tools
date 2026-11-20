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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifndef __SOLVER_SOLVERSPEC_H__
#define __SOLVER_SOLVERSPEC_H__

/** @file
 * The problem a solver works on, as an immutable value
 * (design/2026-11-19-ng-merge-plan.md, 4.3).
 *
 * A host builds one when a solve starts (makeSolverSpec in problem.h builds
 * it from a problem_c) and hands it to the engines as
 * std::shared_ptr<const solverSpec_t>. The engines read the problem only
 * through it, so they do not depend on problem_c or puzzle_c, and an edit of
 * the document during a solve cannot reach a running engine.
 */

#include "../gridtype.h"
#include "../voxel.h"
#include "../rotationgeometry.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

/** one part of the problem: a shape with a count range and its groups */
struct solverPart_t {
  unsigned int shapeId;   ///< index into solverSpec_t::shapes
  unsigned int min, max;  ///< count range, min <= max
  /** group membership for the disassembler: (group id, count) pairs, as problem_c keeps them */
  std::vector<std::pair<unsigned short, unsigned short>> groups;
};

/**
 * The problem as a value.
 *
 * Shape ids are the puzzle's shape ids: `shapes` has one entry per shape of
 * the puzzle, a copy for every shape a part or the result uses and null for
 * the others, so getShapeIdOfPart and `result` mean what they mean in
 * problem_c.
 *
 * The accessors answer exactly as problem_c's of the same name do (the free
 * function getResultShape too), so code moved from problem_c to the spec
 * changes types, not logic.
 */
struct solverSpec_t {

  /** the grid, shared with the puzzle the spec was built from: shapes and
   *  the assemblies an engine makes point to it (voxel_c::getGridType,
   *  assembly_c's symmetries), so it lives as long as either does */
  std::shared_ptr<const gridType_c> grid;
  /** indexed by shape id; copies, null where the problem uses no shape */
  std::vector<std::shared_ptr<const voxel_c>> shapes;
  std::vector<solverPart_t> parts;             ///< problem_c's parts, same order
  unsigned int result = 0xFFFFFFFF;            ///< shape id of the result, invalid when >= shapes.size()
  unsigned int colours = 0;                    ///< puzzle_c::colorNumber()
  /** problem_c's colour constraints, same encoding: piece colour - 1 in the
   *  high 16 bits, result colour - 1 in the low 16 */
  std::set<uint32_t> colourConstraints;
  std::optional<unsigned int> maxHoles;        ///< problem_c::maxHolesDefined/getMaxHoles
  /** puzzle_c::getRotationModel: the model the pieces were made for, if the puzzle stores one */
  std::optional<rotationGeometry::model_t> rotationModel;

  /** \name the accessors of problem_c and puzzle_c the engines use */
  //@{
  const gridType_c * getGridType(void) const { return grid.get(); }
  unsigned int colorNumber(void) const { return colours; }
  const std::optional<rotationGeometry::model_t> & getRotationModel(void) const { return rotationModel; }

  unsigned int getNumberOfParts(void) const { return parts.size(); }
  const voxel_c * getPartShape(unsigned int part) const;
  unsigned int getPartMinimum(unsigned int part) const;
  unsigned int getPartMaximum(unsigned int part) const;
  unsigned int getShapeIdOfPart(unsigned int part) const;
  unsigned int getNumberOfPieces(void) const;
  unsigned int getPartIdToPieceId(unsigned int pieceId) const;

  bool resultValid(void) const { return result < shapes.size(); }
  unsigned int getResultId(void) const;
  const voxel_c * getResultShape(void) const;

  bool placementAllowed(unsigned int pc, unsigned int res) const;

  unsigned short getNumberOfPartGroups(unsigned int part) const;
  unsigned short getPartGroupId(unsigned int part, unsigned int groupID) const;
  unsigned short getPartGroupCount(unsigned int part, unsigned int groupID) const;

  bool maxHolesDefined(void) const { return maxHoles.has_value(); }
  unsigned int getMaxHoles(void) const { bt_assert(maxHoles); return *maxHoles; }
  //@}
};

/** the result shape, as getResultShape(const problem_c &) */
inline const voxel_c * getResultShape(const solverSpec_t & spec) { return spec.getResultShape(); }

#endif
