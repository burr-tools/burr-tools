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
#include "shapelist.h"

#include "../lib/problem.h"
#include "../lib/puzzle.h"
#include "../lib/voxel.h"

#include <algorithm>
#include <climits>
#include <memory>

namespace btui {

  namespace {
    bool isShape(const puzzle_c & p, int i) {
      return i >= 0 && unsigned(i) < p.getNumberOfShapes();
    }
  }

  std::string shapeId(int position) {
    return "S" + std::to_string(position + 1);
  }

  unsigned int defaultShapeSize(gridType_c::gridType type) {
    switch (type) {
      case gridType_c::GT_RHOMBIC: return 5;
      case gridType_c::GT_SPHERES: return 3;
      case gridType_c::GT_BRICKS:
      case gridType_c::GT_TRIANGULAR_PRISM:
      case gridType_c::GT_TETRA_OCTA:
      case gridType_c::GT_NUM_GRIDS:
        return 6;
    }
    return 6;
  }

  int newShape(puzzle_c & p, int selected) {
    if (isShape(p, selected)) {
      const voxel_c * v = p.getShape(unsigned(selected));
      return int(p.addShape(v->getX(), v->getY(), v->getZ()));
    }
    const unsigned int n = defaultShapeSize(p.getGridType()->getType());
    return int(p.addShape(n, n, n));
  }

  int duplicateShape(puzzle_c & p, int i) {
    if (!isShape(p, i))
      return -1;
    std::unique_ptr<voxel_c> copy(p.getGridType()->getVoxel(p.getShape(unsigned(i))));
    return int(p.addShape(std::move(copy)));
  }

  bool canDeleteShape(const puzzle_c & p, int i) {
    return isShape(p, i) && p.getNumberOfShapes() > 1;
  }

  bool deleteLosesSolutions(const puzzle_c & p, int i) {
    if (!isShape(p, i))
      return false;
    for (unsigned int k = 0; k < p.getNumberOfProblems(); k++) {
      const problem_c * pr = p.getProblem(k);
      if (!pr->usesShape(unsigned(i)))
        continue;
      const solveState_e st = pr->getSolveState();
      if (pr->getNumberOfSavedSolutions() > 0 || st == SS_SOLVING || st == SS_SOLVED)
        return true;
    }
    return false;
  }

  int deleteShape(puzzle_c & p, int i) {
    if (!canDeleteShape(p, i))
      return -1;
    p.removeShape(unsigned(i));
    return std::min(i, int(p.getNumberOfShapes()) - 1);
  }

  bool moveShape(puzzle_c & p, int from, int to) {
    if (!isShape(p, from))
      return false;
    to = std::clamp(to, 0, int(p.getNumberOfShapes()) - 1);
    for (int k = from; k < to; k++)
      p.exchangeShapes(unsigned(k), unsigned(k + 1));
    for (int k = from; k > to; k--)
      p.exchangeShapes(unsigned(k), unsigned(k - 1));
    return from != to;
  }

  bool renameShape(puzzle_c & p, int i, const std::string & label) {
    if (!isShape(p, i) || p.getShape(unsigned(i))->getName() == label)
      return false;
    p.getShape(unsigned(i))->setName(label);
    return true;
  }

  bool changeWeight(puzzle_c & p, int i, int delta) {
    if (!isShape(p, i))
      return false;
    voxel_c * v = p.getShape(unsigned(i));
    const long long w = std::clamp<long long>((long long)v->getWeight() + delta, 0, INT_MAX);
    if (w == v->getWeight())
      return false;
    v->setWeight(int(w));
    return true;
  }
}
