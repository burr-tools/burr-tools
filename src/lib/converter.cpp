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
#include "converter.h"

#include "puzzle.h"
#include "problem.h"
#include "voxel.h"
#include "latticegridinfo.h"

#include <algorithm>
#include <array>
#include <climits>
#include <vector>

bool canConvert(gridType_c::gridType src, gridType_c::gridType dst)
{
  return (
          (src == gridType_c::GT_BRICKS && dst == gridType_c::GT_RHOMBIC)
          ||
          (src == gridType_c::GT_BRICKS && dst == gridType_c::GT_TETRA_OCTA)
         );
}

/* do the conversion, if it can't be done (you should check first)
 * nothing will happen.
 * the shapes inside the puzzle will be converted and then the
 * gridtype within the puzzle will be changed
 */
puzzle_c * doConvert(puzzle_c * p, gridType_c::gridType type) {

  if (!canConvert(p->getGridType()->getType(), type)) return 0;

  // for now only the conversion from brick to rhombic is available

  // create a gridType for the target grid
  gridType_c * gt = new gridType_c(type);

  puzzle_c * pNew = new puzzle_c(gt);

  // now convert all shapes
  for (unsigned int i = 0; i < p->getNumberOfShapes(); i++)
  {
    const voxel_c * v = p->getShape(i);

    voxel_c * vn = 0;
    // this is now conversion specific....

    if (p->getGridType()->getType() == gridType_c::GT_BRICKS && type == gridType_c::GT_RHOMBIC)
    {
      vn = gt->getVoxel(v->getX()*5, v->getY()*5, v->getZ()*5, voxel_c::VX_EMPTY);
      vn->skipRecalcBoundingBox(true);

      for (unsigned int x = 0; x < v->getX(); x++)
        for (unsigned int y = 0; y < v->getY(); y++)
          for (unsigned int z = 0; z < v->getZ(); z++) {
            voxel_type st = v->get(x, y, z);

            if (st != 0)
              for (int ax = 0; ax < 5; ax++)
                for (int ay = 0; ay < 5; ay++)
                  for (int az = 0; az < 5; az++)
                    if (vn->validCoordinate(5*x+ax, 5*y+ay, 5*z+az))
                      vn->set(5*x+ax, 5*y+ay, 5*z+az, st);
          }
    }
    else if (p->getGridType()->getType() == gridType_c::GT_BRICKS && type == gridType_c::GT_TETRA_OCTA)
    {
      vn = gt->getVoxel(v->getX()*3, v->getY()*3, v->getZ()*3, voxel_c::VX_EMPTY);
      vn->skipRecalcBoundingBox(true);

      for (unsigned int x = 0; x < v->getX(); x++)
        for (unsigned int y = 0; y < v->getY(); y++)
          for (unsigned int z = 0; z < v->getZ(); z++) {
            voxel_type st = v->get(x, y, z);

            if (st != 0)
              for (int ax = 0; ax < 3; ax++)
                for (int ay = 0; ay < 3; ay++)
                  for (int az = 0; az < 3; az++)
                    if (vn->validCoordinate(3*x+ax, 3*y+ay, 3*z+az))
                      vn->set(3*x+ax, 3*y+ay, 3*z+az, st);
          }
    }
    else
      bt_assert(0);

    vn->skipRecalcBoundingBox(false);
    vn->setName(v->getName());
    pNew->addShape(vn);
  }

  // copy over colours
  for (unsigned int i = 0; i < p->colorNumber(); i++)
  {
    unsigned char r, g, b;
    p->getColor(i, &r, &g, &b);
    pNew->addColor(r, g, b);
  }

  // copy comment
  pNew->setComment(p->getComment());
  pNew->setCommentPopup(p->getCommentPopup());

  // create the problems as copies from the old puzzle
  for (unsigned int i = 0; i < p->getNumberOfProblems(); i++)
  {
    const problem_c * prob = p->getProblem(i);
    problem_c * probNew = pNew->getProblem(pNew->addProblem(prob));

    probNew->setName(prob->getName());
  }

  return pNew;
}


gridType_c::gridType describedCounterpart(gridType_c::gridType native) {
  switch (native) {
    case gridType_c::GT_BRICKS:           return gridType_c::GT_DESCRIBED_BRICKS;
    case gridType_c::GT_TRIANGULAR_PRISM: return gridType_c::GT_DESCRIBED_TRIANGULAR_PRISM;
    case gridType_c::GT_RHOMBIC:          return gridType_c::GT_RHOMBIC_TETRAHEDRA;
    case gridType_c::GT_TETRA_OCTA:       return gridType_c::GT_OCTAHEDRAL_TETRAHEDRA;
    case gridType_c::GT_SPHERES:
    case gridType_c::GT_HEXAGONAL_PRISM:
    case gridType_c::GT_CAIRO_PRISM:
    case gridType_c::GT_RHOMBIC_TETRAHEDRA:
    case gridType_c::GT_OCTAHEDRAL_TETRAHEDRA:
    case gridType_c::GT_DESCRIBED_BRICKS:
    case gridType_c::GT_DESCRIBED_TRIANGULAR_PRISM:
    case gridType_c::GT_NUM_GRIDS:
      break;
  }
  return gridType_c::GT_NUM_GRIDS;
}

namespace {

/* the NG voxels one of our cells becomes, as NG's importer maps them
 * (import.cpp at fdae7951; checked in step G0 of the grid plan) */
void describedCells(gridType_c::gridType src, int x, int y, int z, std::vector<std::array<int, 3>> & out) {
  out.clear();
  switch (src) {
    case gridType_c::GT_BRICKS:
    case gridType_c::GT_TETRA_OCTA:
      out.push_back({x, y, z});
      break;
    case gridType_c::GT_TRIANGULAR_PRISM: {
      int xn = x + 1;
      int yn = (3 * y + 1) / 2 - (x + y % 2) / 2;
      int zn = -xn - yn;
      out.push_back({xn + z, yn + z, zn + z});
      break;
    }
    case gridType_c::GT_RHOMBIC: {
      int xr = x % 5, yr = y % 5, zr = z % 5;
      if (xr > 2) xr++;
      if (yr > 2) yr++;
      if (zr > 2) zr++;
      const int bx = 6 * (x / 5) + xr, by = 6 * (y / 5) + yr, bz = 6 * (z / 5) + zr;
      if (xr == 2) { out.push_back({bx, by, bz}); out.push_back({bx + 1, by, bz}); }
      if (yr == 2) { out.push_back({bx, by, bz}); out.push_back({bx, by + 1, bz}); }
      if (zr == 2) { out.push_back({bx, by, bz}); out.push_back({bx, by, bz + 1}); }
      break;
    }
    default:
      bt_assert(0);
  }
}

} // namespace

puzzle_c * convertToDescribed(const puzzle_c * p) {

  const gridType_c::gridType src = p->getGridType()->getType();
  const gridType_c::gridType dst = describedCounterpart(src);
  if (dst == gridType_c::GT_NUM_GRIDS) return nullptr;

  const latticeGridInfo_c & inf = latticeGridInfo_c::get(dst);
  gridType_c * gt = new gridType_c(dst);
  puzzle_c * pNew = new puzzle_c(gt);

  std::vector<std::array<int, 3>> cells;

  for (unsigned int i = 0; i < p->getNumberOfShapes(); i++) {

    const voxel_c * v = p->getShape(i);

    // the cells and their values, in NG's coordinates
    struct cell_s { int x, y, z; voxel_type val; };
    std::vector<cell_s> mapped;
    int mx = INT_MAX, my = INT_MAX, mz = INT_MAX;

    for (unsigned int z = 0; z < v->getZ(); z++)
      for (unsigned int y = 0; y < v->getY(); y++)
        for (unsigned int x = 0; x < v->getX(); x++) {
          const voxel_type st = v->get(x, y, z);
          if (!st) continue;
          describedCells(src, x, y, z, cells);
          for (const auto & c : cells) {
            bt_assert(inf.isVoxel(c[0], c[1], c[2]));
            mapped.push_back({c[0], c[1], c[2], st});
            mx = std::min(mx, c[0]);
            my = std::min(my, c[1]);
            mz = std::min(mz, c[2]);
          }
        }

    voxel_c * vn;
    if (mapped.empty()) {
      vn = gt->getVoxel(1, 1, 1, voxel_c::VX_EMPTY);
    } else {
      // move by a lattice vector to non negative coordinates
      int vx, vy, vz;
      inf.alignDown(mx, my, mz, vx, vy, vz);
      int sx = 0, sy = 0, sz = 0;
      for (const auto & c : mapped) {
        sx = std::max(sx, c.x - vx + 1);
        sy = std::max(sy, c.y - vy + 1);
        sz = std::max(sz, c.z - vz + 1);
      }
      vn = gt->getVoxel(sx, sy, sz, voxel_c::VX_EMPTY);
      vn->skipRecalcBoundingBox(true);
      for (const auto & c : mapped)
        vn->set(c.x - vx, c.y - vy, c.z - vz, c.val);
      vn->skipRecalcBoundingBox(false);
    }

    vn->setName(v->getName());
    vn->initHotspot();
    pNew->addShape(vn);
  }

  for (unsigned int i = 0; i < p->colorNumber(); i++) {
    unsigned char r, g, b;
    p->getColor(i, &r, &g, &b);
    pNew->addColor(r, g, b);
  }

  pNew->setComment(p->getComment());
  pNew->setCommentPopup(p->getCommentPopup());

  for (unsigned int i = 0; i < p->getNumberOfProblems(); i++) {
    const problem_c * prob = p->getProblem(i);
    problem_c * probNew = pNew->getProblem(pNew->addProblem(prob));
    probNew->setName(prob->getName());
  }

  return pNew;
}
