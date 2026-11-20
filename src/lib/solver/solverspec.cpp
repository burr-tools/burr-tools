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
#include "solverspec.h"

/* Each accessor mirrors problem_c's of the same name (problem.cpp),
 * including its assertions. */

const voxel_c * solverSpec_t::getPartShape(unsigned int part) const {
  bt_assert(part < parts.size());
  bt_assert(parts[part].shapeId < shapes.size());
  return shapes[parts[part].shapeId].get();
}

unsigned int solverSpec_t::getPartMinimum(unsigned int part) const {
  bt_assert(part < parts.size());
  return parts[part].min;
}

unsigned int solverSpec_t::getPartMaximum(unsigned int part) const {
  bt_assert(part < parts.size());
  return parts[part].max;
}

unsigned int solverSpec_t::getShapeIdOfPart(unsigned int part) const {
  bt_assert(part < parts.size());
  return parts[part].shapeId;
}

unsigned int solverSpec_t::getNumberOfPieces(void) const {
  unsigned int n = 0;
  for (const solverPart_t & p : parts)
    n += p.max;
  return n;
}

unsigned int solverSpec_t::getPartIdToPieceId(unsigned int pieceId) const {
  unsigned int part = 0;
  while (part < parts.size() && pieceId >= parts[part].max) {
    pieceId -= parts[part].max;
    part++;
  }
  bt_assert(part < parts.size());
  return part;
}

unsigned int solverSpec_t::getResultId(void) const {
  bt_assert(resultValid());
  return result;
}

const voxel_c * solverSpec_t::getResultShape(void) const {
  bt_assert(resultValid());
  return shapes[result].get();
}

bool solverSpec_t::placementAllowed(unsigned int pc, unsigned int res) const {
  bt_assert(pc <= colours);
  bt_assert(res <= colours);

  if (colours == 0)
    return true;

  return (pc == 0) || (res == 0) || (colourConstraints.find((pc-1) << 16 | (res-1)) != colourConstraints.end());
}

unsigned short solverSpec_t::getNumberOfPartGroups(unsigned int part) const {
  bt_assert(part < parts.size());
  return parts[part].groups.size();
}

unsigned short solverSpec_t::getPartGroupId(unsigned int part, unsigned int groupID) const {
  bt_assert(part < parts.size());
  bt_assert(groupID < parts[part].groups.size());
  return parts[part].groups[groupID].first;
}

unsigned short solverSpec_t::getPartGroupCount(unsigned int part, unsigned int groupID) const {
  bt_assert(part < parts.size());
  bt_assert(groupID < parts[part].groups.size());
  return parts[part].groups[groupID].second;
}
