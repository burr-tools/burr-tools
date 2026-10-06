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
#include "puzzletools.h"

#include "../lib/assembly.h"
#include "../lib/converter.h"
#include "../lib/millable.h"
#include "../lib/problem.h"
#include "../lib/puzzle.h"
#include "../lib/solution.h"
#include "../lib/symmetries.h"
#include "../lib/voxel.h"
#include "../lib/voxeltable.h"

#include <algorithm>
#include <memory>

namespace btui {

  std::vector<gridType_c::gridType> convertTargets(gridType_c::gridType from) {
    std::vector<gridType_c::gridType> out;
    for (int i = 0; i < gridType_c::GT_NUM_GRIDS; i++)
      if (canConvert(from, gridType_c::gridType(i)))
        out.push_back(gridType_c::gridType(i));
    return out;
  }

  namespace {

    /* legacy mainwindow.cpp voxelTableVector_c: identical-shape lookups over
     * the shapes collected so far */
    class VectorTable : public voxelTable_c {
      public:
        explicit VectorTable(const std::vector<std::unique_ptr<voxel_c>> & s) : shapes(s) {}
      protected:
        const voxel_c * findSpace(unsigned int index) const override { return shapes[index].get(); }
      private:
        const std::vector<std::unique_ptr<voxel_c>> & shapes;
    };
  }

  unsigned int importAssemblies(puzzle_c & p, const ImportAssembliesOptions & opt) {
    if (opt.sourceProblem >= p.getNumberOfProblems())
      return 0;
    problem_c * src = p.getProblem(opt.sourceProblem);
    const bool bricks = p.getGridType()->getType() == gridType_c::GT_BRICKS;

    std::vector<std::unique_ptr<voxel_c>> found;
    VectorTable table(found);

    for (unsigned int s = 0; s < src->getNumberOfSavedSolutions(); s++) {
      auto shape = src->getSavedSolution(s)->getAssembly()->createSpace(*src);

      if (opt.dropDisconnected && !shape->connected(0, true, voxel_c::VX_EMPTY))
        continue;

      const symmetries_t sym = shape->selfSymmetries();
      if (opt.dropMirror && shape->getGridType()->getSymmetries()->symmetryContainsMirror(sym))
        continue;
      if (opt.dropSymmetric && !unSymmetric(sym))
        continue;
      if (bricks && opt.dropNonMillable && !isMillable(shape.get()))
        continue;
      if (bricks && opt.dropNonNotchable && !isNotchable(shape.get()))
        continue;

      const unsigned int voxels = shape->countState(voxel_c::VX_FILLED);
      if (voxels < opt.shapeMin || voxels > opt.shapeMax)
        continue;

      if (opt.dropIdentical && table.getSpace(shape.get()))
        continue;

      found.push_back(std::move(shape));
      if (opt.dropIdentical)
        table.addSpace(unsigned(found.size() - 1));
    }

    problem_c * dst = nullptr;
    if (opt.destination == ImportAssembliesOptions::Destination::NewProblem)
      dst = p.getProblem(p.addProblem());
    else if (opt.destination == ImportAssembliesOptions::Destination::ExistingProblem && opt.destinationProblem < p.getNumberOfProblems())
      dst = p.getProblem(opt.destinationProblem);

    const unsigned int added = unsigned(found.size());
    for (auto & shape : found) {
      const unsigned int i = p.addShape(std::move(shape));
      if (dst) {
        dst->setShapeMaximum(i, opt.rangeMax);
        dst->setShapeMinimum(i, opt.rangeMin);
      }
    }
    return added;
  }

  struct ShapeStatusCalculator::Impl {
    explicit Impl(const puzzle_c & p) : table(&p) {}
    voxelTablePuzzle_c table;
  };

  ShapeStatusCalculator::ShapeStatusCalculator(const puzzle_c & p) : puz(p), d(std::make_unique<Impl>(p)) {}
  ShapeStatusCalculator::~ShapeStatusCalculator(void) = default;

  bool ShapeStatusCalculator::done(void) const { return pos >= puz.getNumberOfShapes(); }

  /* legacy statusWindow_c's constructor, one row at a time */
  ShapeStatus ShapeStatusCalculator::next(void) {
    ShapeStatus r;
    if (done())
      return r;
    const unsigned int s = pos++;
    const voxel_c * v = puz.getShape(s);
    const symmetries_c * sym = puz.getGridType()->getSymmetries();

    r.fixed = v->countState(voxel_c::VX_FILLED);
    r.variable = v->countState(voxel_c::VX_VARIABLE);

    unsigned int idx;
    unsigned char trans;
    if (d->table.getSpace(v, &idx, &trans, voxelTable_c::PAR_MIRROR))
      r.identicalMirror = int(idx);
    if (d->table.getSpace(v, &idx, &trans, 0))
      r.identicalShape = int(idx);
    if (d->table.getSpace(v, &idx, &trans, voxelTable_c::PAR_COLOUR) && trans < sym->getNumTransformations())
      r.identicalComplete = int(idx);

    r.connectedFace = v->connected(0, true, 0);
    r.connectedEdge = v->connected(1, true, 0);
    r.connectedCorner = v->connected(2, true, 0);
    r.holes2d = !v->connected(0, false, 0, false);
    r.holes3d = !v->connected(0, false, 0);

    if (puz.getGridType()->getType() == gridType_c::GT_BRICKS) {
      r.notchable = isNotchable(v);
      r.millable = isMillable(v);
    }

    r.symmetryKnown = sym->symmetryKnown(v);
    if (r.symmetryKnown)
      r.symmetry = (long long)sym->calculateSymmetry(v);

    d->table.addSpace(s, voxelTable_c::PAR_MIRROR);
    d->table.addSpace(s, voxelTable_c::PAR_MIRROR | voxelTable_c::PAR_COLOUR);
    return r;
  }

  void removeShapes(puzzle_c & p, std::vector<unsigned int> indices) {
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    // from the top, so earlier indices stay valid
    for (auto it = indices.rbegin(); it != indices.rend(); ++it) {
      const unsigned int s = *it;
      if (s >= p.getNumberOfShapes())
        continue;
      for (unsigned int i = 0; i < p.getNumberOfProblems(); i++)
        if (p.getProblem(i)->usesShape(s))
          p.getProblem(i)->removeAllSolutions();
      p.removeShape(s);
    }
  }
}
