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
#include "assembly.h"
#include "solution.h"
#include "symmetries.h"
#include "latticegridinfo.h"
#include "disassembly.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <sstream>

#include "../tools/xml.h"
#include <memory>
#include <vector>

namespace {

/* the native grid one of NG's cut cube grids corresponds to, GT_NUM_GRIDS for none */
gridType_c::gridType nativeCutCube(gridType_c::gridType ng) {
  switch (ng) {
    case gridType_c::GT_RHOMBIC_TETRAHEDRA:    return gridType_c::GT_RHOMBIC;
    case gridType_c::GT_OCTAHEDRAL_TETRAHEDRA: return gridType_c::GT_TETRA_OCTA;
    default:                                   return gridType_c::GT_NUM_GRIDS;
  }
}

bool canConvertNative(gridType_c::gridType src, gridType_c::gridType dst)
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
puzzle_c * doConvertNative(const puzzle_c * p, gridType_c::gridType type) {

  if (!canConvertNative(p->getGridType()->getType(), type)) return 0;

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

} // namespace

bool canConvert(gridType_c::gridType src, gridType_c::gridType dst, cutCubeGrids_e cut)
{
  if (cut == cutCubeGrids_e::native)
    return canConvertNative(src, dst);
  const gridType_c::gridType nat = nativeCutCube(dst);
  return nat != gridType_c::GT_NUM_GRIDS && canConvertNative(src, nat);
}

puzzle_c * doConvert(puzzle_c * p, gridType_c::gridType type, cutCubeGrids_e cut) {

  if (!canConvert(p->getGridType()->getType(), type, cut)) return 0;
  if (cut == cutCubeGrids_e::native)
    return doConvertNative(p, type);

  /* NG's grid: through the native grid, the conversion the FLTK GUI makes */
  std::unique_ptr<puzzle_c> nat(doConvertNative(p, nativeCutCube(type)));
  bt_assert(nat);
  puzzle_c * res = convertToDescribed(nat.get());
  bt_assert(res && res->getGridType()->getType() == type);
  return res;
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
      /* floored, so that cells below 0 (a saved assembly or disassembly
       * may place pieces there) map like the others */
      auto fdiv2 = [](int a) { return a >= 0 ? a / 2 : -((-a + 1) / 2); };
      const int ymod = ((y % 2) + 2) % 2;
      int xn = x + 1;
      int yn = fdiv2(3 * y + 1) - fdiv2(x + ymod);
      int zn = -xn - yn;
      out.push_back({xn + z, yn + z, zn + z});
      break;
    }
    case gridType_c::GT_RHOMBIC: {
      /* floored, so that cells below 0 (an old file's assembly may place
       * pieces there) map with the same period */
      auto fdiv = [](int a) { return a >= 0 ? a / 5 : -((-a + 4) / 5); };
      int xr = x - 5 * fdiv(x), yr = y - 5 * fdiv(y), zr = z - 5 * fdiv(z);
      if (xr > 2) xr++;
      if (yr > 2) yr++;
      if (zr > 2) zr++;
      const int bx = 6 * fdiv(x) + xr, by = 6 * fdiv(y) + yr, bz = 6 * fdiv(z) + zr;
      if (xr == 2) { out.push_back({bx, by, bz}); out.push_back({bx + 1, by, bz}); }
      if (yr == 2) { out.push_back({bx, by, bz}); out.push_back({bx, by + 1, bz}); }
      if (zr == 2) { out.push_back({bx, by, bz}); out.push_back({bx, by, bz + 1}); }
      break;
    }
    default:
      bt_assert(0);
  }
}

/* our cube rotation matrices (tabs_0), which bricks, rhombic and tetra-octa
 * share: voxel_3_c and voxel_4_c transform with voxel_0_c's table */
const int cubeMatrices[][9] = {
#include "tabs_0/rotmatrix.inc"
};
constexpr unsigned int numCubeMatrices = sizeof(cubeMatrices) / sizeof(cubeMatrices[0]);
static_assert(numCubeMatrices == 48);

/* the described grid's transformation numbers of a native grid's, empty when
 * the native grid has no table (triangular prisms: their saved assemblies are
 * not converted; only burrTxt -D converts them, for cross-checks) */
std::vector<unsigned char> transformationTable(gridType_c::gridType native) {
  std::vector<unsigned char> res;
  if (native != gridType_c::GT_BRICKS && native != gridType_c::GT_RHOMBIC && native != gridType_c::GT_TETRA_OCTA)
    return res;
  const latticeGridInfo_c & inf = latticeGridInfo_c::get(describedCounterpart(native));
  for (unsigned int t = 0; t < numCubeMatrices; t++)
    res.push_back(inf.transformationWithMatrix(cubeMatrices[t]));
  return res;
}

typedef std::array<int, 3> cell_t;

/* Our triangular prism turns (tabs_1/rotmatrix.inc), and the way voxel_1_c
 * turns a cell with them: a native prism file's assemblies are given in
 * these, and the importer needs no other part of the native prism grid. */
const double s34 = std::sqrt(0.75);
const double prismMatrices[][9] = {
  {  1.0,  0.0, 0.0,  0.0,  1.0, 0.0, 0.0, 0.0,  1.0 },
  {  0.5, -s34, 0.0,  s34,  0.5, 0.0, 0.0, 0.0,  1.0 },
  { -0.5, -s34, 0.0,  s34, -0.5, 0.0, 0.0, 0.0,  1.0 },
  { -1.0, -0.0, 0.0,  0.0, -1.0, 0.0, 0.0, 0.0,  1.0 },
  { -0.5,  s34, 0.0, -s34, -0.5, 0.0, 0.0, 0.0,  1.0 },
  {  0.5,  s34, 0.0, -s34,  0.5, 0.0, 0.0, 0.0,  1.0 },
  { -1.0,  0.0, 0.0,  0.0,  1.0, 0.0, 0.0, 0.0, -1.0 },
  { -0.5, -s34, 0.0, -s34,  0.5, 0.0, 0.0, 0.0, -1.0 },
  {  0.5, -s34, 0.0, -s34, -0.5, 0.0, 0.0, 0.0, -1.0 },
  {  1.0, -0.0, 0.0,  0.0, -1.0, 0.0, 0.0, 0.0, -1.0 },
  {  0.5,  s34, 0.0,  s34, -0.5, 0.0, 0.0, 0.0, -1.0 },
  { -0.5,  s34, 0.0,  s34,  0.5, 0.0, 0.0, 0.0, -1.0 },
  { -1.0,  0.0, 0.0,  0.0,  1.0, 0.0, 0.0, 0.0,  1.0 },
  { -0.5, -s34, 0.0, -s34,  0.5, 0.0, 0.0, 0.0,  1.0 },
  {  0.5, -s34, 0.0, -s34, -0.5, 0.0, 0.0, 0.0,  1.0 },
  {  1.0, -0.0, 0.0,  0.0, -1.0, 0.0, 0.0, 0.0,  1.0 },
  {  0.5,  s34, 0.0,  s34, -0.5, 0.0, 0.0, 0.0,  1.0 },
  { -0.5,  s34, 0.0,  s34,  0.5, 0.0, 0.0, 0.0,  1.0 },
  {  1.0,  0.0, 0.0,  0.0,  1.0, 0.0, 0.0, 0.0, -1.0 },
  {  0.5, -s34, 0.0,  s34,  0.5, 0.0, 0.0, 0.0, -1.0 },
  { -0.5, -s34, 0.0,  s34, -0.5, 0.0, 0.0, 0.0, -1.0 },
  { -1.0, -0.0, 0.0,  0.0, -1.0, 0.0, 0.0, 0.0, -1.0 },
  { -0.5,  s34, 0.0, -s34, -0.5, 0.0, 0.0, 0.0, -1.0 },
  {  0.5,  s34, 0.0, -s34,  0.5, 0.0, 0.0, 0.0, -1.0 },
};
constexpr unsigned int numPrismMatrices = sizeof(prismMatrices) / sizeof(prismMatrices[0]);

cell_t prismTurn(unsigned int t, int x, int y, int z) {
  const double * m = prismMatrices[t];
  const double xp = 0.5 + 0.5 * x;
  const double yp = y * s34 + (((x + y) & 1) ? std::sqrt(1.0 / 3) : std::sqrt(1.0 / 12));
  const double zp = z;
  double xpn = m[0] * xp + m[1] * yp + m[2] * zp;
  double ypn = m[3] * xp + m[4] * yp + m[5] * zp;
  const double zpn = m[6] * xp + m[7] * yp + m[8] * zp;
  xpn = (xpn - 0.5) * 2;
  ypn /= s34;
  return { (int)(xpn + (xpn < 0 ? -0.5 : 0.5)), (int)std::floor(ypn), (int)(zpn + (zpn < 0 ? -0.5 : 0.5)) };
}

/* the filled cells of a native triangular prism shape placed as
 * (trans, x, y, z) in an assembly: voxel_1_c's turn, the hotspot on the
 * position */
std::vector<cell_t> prismPlacedCells(const voxel_c & shape, unsigned char trans, int x, int y, int z) {
  if (trans >= numPrismMatrices)
    bt_te("convertToDescribed: a saved assembly has a transformation its grid does not have");
  const cell_t h = prismTurn(trans, shape.getHx(), shape.getHy(), shape.getHz());
  std::vector<cell_t> res;
  for (unsigned int cz = 0; cz < shape.getZ(); cz++)
    for (unsigned int cy = 0; cy < shape.getY(); cy++)
      for (unsigned int cx = 0; cx < shape.getX(); cx++)
        if (shape.getState(cx, cy, cz) != voxel_c::VX_EMPTY) {
          const cell_t c = prismTurn(trans, cx, cy, cz);
          res.push_back({c[0] - h[0] + x, c[1] - h[1] + y, c[2] - h[2] + z});
        }
  return res;
}

/* the filled cells of a shape placed as (trans, x, y, z) in an assembly */
std::vector<cell_t> placedCells(const voxel_c & shape, unsigned char trans, int x, int y, int z) {
  if (shape.getGridType()->getType() == gridType_c::GT_TRIANGULAR_PRISM)
    return prismPlacedCells(shape, trans, x, y, z);
  std::unique_ptr<voxel_c> pc(shape.getGridType()->getVoxel(&shape));
  if (!pc->transform(trans))
    bt_te("convertToDescribed: a saved assembly has a transformation its grid does not have");
  const int dx = x - pc->getHx(), dy = y - pc->getHy(), dz = z - pc->getHz();
  std::vector<cell_t> res;
  for (unsigned int cz = 0; cz < pc->getZ(); cz++)
    for (unsigned int cy = 0; cy < pc->getY(); cy++)
      for (unsigned int cx = 0; cx < pc->getX(); cx++)
        if (pc->getState(cx, cy, cz) != voxel_c::VX_EMPTY)
          res.push_back({(int)cx + dx, (int)cy + dy, (int)cz + dz});
  return res;
}

/* The placement on the described grid of a piece placed natively as
 * (trans, x, y, z): the native piece's cells go through the importer's map
 * (describedCells) and the shift that moved the problem's result onto the
 * described grid's space; the transformation through the table; the position
 * is where the transformed described piece covers exactly those cells. */
placement_c convertPlacement(gridType_c::gridType src, const voxel_c & nativeShape, const voxel_c & shape,
                             unsigned char trans, int x, int y, int z, const std::vector<unsigned char> & table,
                             const cell_t & shift) {

  std::vector<cell_t> target, cells;
  for (const cell_t & c : placedCells(nativeShape, trans, x, y, z)) {
    describedCells(src, c[0], c[1], c[2], cells);
    for (const cell_t & d : cells)
      target.push_back({d[0] - shift[0], d[1] - shift[1], d[2] - shift[2]});
  }
  if (target.empty())
    bt_te("convertToDescribed: a saved assembly's piece does not map onto the described grid");
  std::sort(target.begin(), target.end());

  /* The transformation through the table where the grids share their
   * matrices; else (prisms) the first of the described grid's that covers
   * the same cells. */
  std::vector<unsigned char> candidates;
  if (!table.empty()) {
    if (trans >= table.size() || table[trans] == TND)
      bt_te("convertToDescribed: a saved assembly has a transformation the described grid does not have");
    candidates.push_back(table[trans]);
  } else {
    const unsigned int n = shape.getGridType()->getSymmetries()->getNumTransformationsMirror();
    for (unsigned int t = 0; t < n; t++)
      candidates.push_back(static_cast<unsigned char>(t));
  }

  for (const unsigned char t : candidates) {
    std::unique_ptr<voxel_c> pc(shape.getGridType()->getVoxel(&shape));
    if (!pc->transform(t)) {
      if (!table.empty())
        bt_te("convertToDescribed: the transformation table names a transformation the grid does not have");
      continue;
    }
    std::vector<cell_t> got = placedCells(shape, t, pc->getHx(), pc->getHy(), pc->getHz());
    if (target.size() != got.size())
      continue;

    // the shift that moves the transformed piece onto the target cells
    cell_t lo1 = target[0], lo2 = got[0];
    for (const cell_t & c : target) for (int k = 0; k < 3; k++) lo1[k] = std::min(lo1[k], c[k]);
    for (const cell_t & c : got)    for (int k = 0; k < 3; k++) lo2[k] = std::min(lo2[k], c[k]);
    const cell_t d = {lo1[0] - lo2[0], lo1[1] - lo2[1], lo1[2] - lo2[2]};
    for (cell_t & c : got) for (int k = 0; k < 3; k++) c[k] += d[k];
    std::sort(got.begin(), got.end());
    if (target == got)
      return placement_c(t, pc->getHx() + d[0], pc->getHy() + d[1], pc->getHz() + d[2]);
  }
  bt_te("convertToDescribed: a saved assembly's piece does not map onto the described grid");
}

/* What a separation needs of each placed piece: its native cells and
 * position as assembled, and its described position. */
struct placedPiece_s {
  std::vector<cell_t> cells;
  cell_t nativePos;
  cell_t describedPos;
};

/* A separation's states on the described grid. A state holds each piece's
 * position (its hotspot, as the assembly does); the move from the assembled
 * position is a translation of the native grid, and becomes the translation
 * that moves the piece's described cells where its moved native cells map,
 * the same for every cell of the piece (or the conversion fails). */
std::unique_ptr<separation_c> convertSeparation(gridType_c::gridType src, const separation_c & sep,
                                                const std::vector<placedPiece_s> & cellsOf) {
  std::unique_ptr<separation_c> removed, left;
  if (sep.getRemoved()) removed = convertSeparation(src, *sep.getRemoved(), cellsOf);
  if (sep.getLeft())    left = convertSeparation(src, *sep.getLeft(), cellsOf);
  auto res = std::make_unique<separation_c>(std::move(removed), std::move(left), sep.getPieces());

  std::vector<cell_t> from, to;
  // last to first: addstate() puts a state in front of those added before
  for (unsigned int s = sep.getMoves() + 1; s-- > 0;) {
    const state_c * st = sep.getState(s);
    if (st->hasTrans() || st->getRotation())
      bt_te("convertToDescribed: a disassembly with turns does not convert");
    auto ns = std::make_unique<state_c>(sep.getPieceNumber());
    for (unsigned int i = 0; i < sep.getPieceNumber(); i++) {
      const unsigned int piece = sep.getPieceName(i);
      if (piece >= cellsOf.size() || cellsOf[piece].cells.empty())
        bt_te("convertToDescribed: a disassembly names a piece the assembly does not place");
      const placedPiece_s & pp = cellsOf[piece];
      const int dx = st->getX(i) - pp.nativePos[0], dy = st->getY(i) - pp.nativePos[1],
                dz = st->getZ(i) - pp.nativePos[2];
      bool first = true;
      cell_t delta{0, 0, 0};
      for (const cell_t & c : pp.cells) {
        describedCells(src, c[0], c[1], c[2], from);
        describedCells(src, c[0] + dx, c[1] + dy, c[2] + dz, to);
        if (from.size() != to.size())
          bt_te("convertToDescribed: a disassembly move is no translation of the described grid");
        for (size_t k = 0; k < from.size(); k++) {
          const cell_t dd = {to[k][0] - from[k][0], to[k][1] - from[k][1], to[k][2] - from[k][2]};
          if (first)
            delta = dd;
          else if (dd != delta)
            bt_te("convertToDescribed: a disassembly move is no translation of the described grid");
          first = false;
        }
      }
      ns->set(i, pp.describedPos[0] + delta[0], pp.describedPos[1] + delta[1], pp.describedPos[2] + delta[2]);
      if (st->pieceRemoved(i) != ns->pieceRemoved(i))
        bt_te("convertToDescribed: a removed piece does not stay removed");
    }
    res->addstate(std::move(ns));
  }
  return res;
}

} // namespace

unsigned char describedTransformation(gridType_c::gridType native, unsigned int t) {
  const std::vector<unsigned char> table = transformationTable(native);
  return t < table.size() ? table[t] : TND;
}

puzzle_c * convertToDescribed(const puzzle_c * p) {

  const gridType_c::gridType src = p->getGridType()->getType();
  const gridType_c::gridType dst = describedCounterpart(src);
  if (dst == gridType_c::GT_NUM_GRIDS) return nullptr;

  const latticeGridInfo_c & inf = latticeGridInfo_c::get(dst);
  gridType_c * gt = new gridType_c(dst);
  std::unique_ptr<puzzle_c> pNew(new puzzle_c(gt));

  std::vector<cell_t> cells;
  // per shape, the vector its mapped cells were moved by
  std::vector<cell_t> shift;

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
      shift.push_back({0, 0, 0});
    } else {
      // move by a lattice vector to non negative coordinates
      int vx, vy, vz;
      inf.alignDown(mx, my, mz, vx, vy, vz);
      shift.push_back({vx, vy, vz});
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
  if (p->getRotationModel())
    pNew->setRotationModel(*p->getRotationModel());

  const std::vector<unsigned char> table = transformationTable(src);

  for (unsigned int i = 0; i < p->getNumberOfProblems(); i++) {
    const problem_c * prob = p->getProblem(i);
    problem_c * probNew = pNew->getProblem(pNew->addProblem(prob));
    probNew->setName(prob->getName());

    /* The saved assemblies and their disassemblies. The native rhombic and
     * tetra-octa grids have no disassembler, so their files hold none;
     * prism and brick disassemblies move pieces by translations, which
     * convert. A disassembly with turns (rotation moves, cubes only) is
     * dropped and found again by solving. Without a result nothing is kept.
     */
    if (!prob->resultValid()) continue;

    const cell_t & resShift = shift[prob->getResultId()];
    std::vector<std::unique_ptr<solution_c>> sols;
    auto lock = prob->lockSolutions();
    for (unsigned int s = 0; s < prob->getNumberOfSavedSolutions(); s++) {
      const solution_c * sol = prob->getSavedSolution(s);
      const assembly_c * a = sol->getAssembly();
      if (!a) continue;
      auto an = std::make_unique<assembly_c>(gt);
      // per placed piece (the numbering disassemblies use), its native cells
      std::vector<placedPiece_s> cellsOf;
      for (unsigned int pc = 0; pc < a->placementCount(); pc++) {
        if (!a->isPlaced(pc)) {
          an->addNonPlacement();
          continue;
        }
        const unsigned int part = prob->getPartIdToPieceId(pc);
        const placement_c pl = convertPlacement(src, *prob->getPartShape(part), *probNew->getPartShape(part),
                                                a->getTransformation(pc), a->getX(pc), a->getY(pc), a->getZ(pc),
                                                table, resShift);
        an->addPlacement(pl.getTransformation(), pl.getX(), pl.getY(), pl.getZ());
        cellsOf.push_back({placedCells(*prob->getPartShape(part), a->getTransformation(pc),
                                       a->getX(pc), a->getY(pc), a->getZ(pc)),
                           {a->getX(pc), a->getY(pc), a->getZ(pc)},
                           {pl.getX(), pl.getY(), pl.getZ()}});
      }
      std::unique_ptr<separation_c> tree;
      if (const separation_c * sep = sol->getDisassembly()) {
        bool turns = false;
        std::vector<const separation_c *> todo{sep};
        while (!todo.empty() && !turns) {
          const separation_c * t = todo.back();
          todo.pop_back();
          for (unsigned int k = 0; k <= t->getMoves(); k++)
            if (t->getState(k)->hasTrans() || t->getState(k)->getRotation())
              turns = true;
          if (t->getRemoved()) todo.push_back(t->getRemoved());
          if (t->getLeft()) todo.push_back(t->getLeft());
        }
        if (!turns)
          tree = convertSeparation(src, *sep, cellsOf);
      }
      if (tree) {
        sols.push_back(std::make_unique<solution_c>(std::move(an), sol->getAssemblyNumber(), std::move(tree),
                                                    sol->getSolutionNumber()));
      } else if (!sol->getDisassembly() && sol->getDisassemblyInfo()) {
        /* only the summary of a disassembly (moves per level): it does not
         * depend on the grid, so it is kept as it is */
        const auto * info = dynamic_cast<const separationInfo_c *>(sol->getDisassemblyInfo());
        bt_assert(info);
        std::ostringstream os;
        {
          xmlWriter_c xml(os);
          info->save(xml);
        }
        std::istringstream is(os.str());
        xmlParser_c pars(is);
        pars.nextTag();
        sols.push_back(std::make_unique<solution_c>(std::move(an), sol->getAssemblyNumber(),
                                                    std::make_unique<separationInfo_c>(pars),
                                                    sol->getSolutionNumber()));
      } else
        sols.push_back(std::make_unique<solution_c>(std::move(an), sol->getAssemblyNumber()));
    }
    probNew->adoptResults(*prob, std::move(sols));
  }

  return pNew.release();
}
