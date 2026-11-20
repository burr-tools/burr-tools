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
#include "gridtype.h"

#include "assembler_1.h"
#include "movementcache_0.h"
#include "movementcache_1.h"
#include "voxel_0.h"
#include "voxel_1.h"
#include "voxel_2.h"
#include "voxel_3.h"
#include "voxel_4.h"
#include "voxel_lattice.h"
#include "latticegridinfo.h"
#include "symmetries_lattice.h"
#include "movementcache_lattice.h"
#include "symmetries_0.h"
#include "symmetries_1.h"
#include "symmetries_2.h"
#include "stl_0.h"
#include "stl_2.h"
#include "solver/solverspec.h"

#include "../tools/xml.h"

#include <array>
#include <cmath>
#include <stdlib.h>

gridType_c::gridType_c(xmlParser_c & pars)
{
  pars.require(xmlParser_c::START_TAG, "gridType");

  std::string typeStr = pars.getAttributeValue("type");

  if (!typeStr.length())
    pars.exception("grid type needs a valid 'type' attribute");

  // set to the correct size
  type = (gridType)atoi(typeStr.c_str());

  switch (type) {

    case GT_BRICKS:
    case GT_TRIANGULAR_PRISM:
    case GT_SPHERES:
    case GT_RHOMBIC:
    case GT_TETRA_OCTA:
    case GT_HEXAGONAL_PRISM:
    case GT_CAIRO_PRISM:
    case GT_RHOMBIC_TETRAHEDRA:
    case GT_OCTAHEDRAL_TETRAHEDRA:
    case GT_DESCRIBED_BRICKS:
    case GT_DESCRIBED_TRIANGULAR_PRISM:
      break;

    default:
      pars.exception("puzzle with unknown grid type");
      break;
  }

  pars.skipSubTree();
}

gridType_c::gridType_c(const gridType_c & orig) : type(orig.type), sym(nullptr)
{
}

void gridType_c::save(xmlWriter_c & xml) const
{
  xml.newTag("gridType");

  xml.newAttrib("type", (unsigned long)type);

  xml.endTag("gridType");
}

gridType_c::gridType_c(void) {
  type = GT_BRICKS;
}

gridType_c::gridType_c(gridType gt) {

  type = gt;

  switch (type) {
    case GT_BRICKS:
      break;

    case GT_TRIANGULAR_PRISM:
      break;

    case GT_SPHERES:
      break;

    case GT_RHOMBIC:
      break;

    case GT_TETRA_OCTA:
      break;

    default:
      bt_assert(isDescribed(type));
      break;
  }
}

gridType_c::~gridType_c(void) = default;

std::unique_ptr<movementCache_c> gridType_c::getMovementCache(std::shared_ptr<const solverSpec_t> puz) const
{
  switch (type) {
    case GT_BRICKS:           return std::make_unique<movementCache_0_c>(puz);
    case GT_TRIANGULAR_PRISM: return std::make_unique<movementCache_1_c>(puz);
    default:
      if (isDescribed(type) && latticeGridInfo_c::get(type).canDisassemble())
        return std::make_unique<movementCache_lattice_c>(puz, latticeGridInfo_c::get(type));
      return nullptr;
  }
}

voxel_c * gridType_c::getVoxel(unsigned int x, unsigned int y, unsigned int z, voxel_type init) const
{
  switch (type) {
    case GT_BRICKS:           return new voxel_0_c(x, y, z, this, init);
    case GT_TRIANGULAR_PRISM: return new voxel_1_c(x, y, z, this, init);
    case GT_SPHERES:          return new voxel_2_c(x, y, z, this, init);
    case GT_RHOMBIC:          return new voxel_3_c(x, y, z, this, init);
    case GT_TETRA_OCTA:       return new voxel_4_c(x, y, z, this, init);
    default:
      if (isDescribed(type)) return new voxel_lattice_c(x, y, z, this, init);
      return 0;
  }
}

voxel_c * gridType_c::getVoxel(xmlParser_c & pars) const
{
  switch (type) {
    case GT_BRICKS:           return new voxel_0_c(pars, this);
    case GT_TRIANGULAR_PRISM: return new voxel_1_c(pars, this);
    case GT_SPHERES:          return new voxel_2_c(pars, this);
    case GT_RHOMBIC:          return new voxel_3_c(pars, this);
    case GT_TETRA_OCTA:       return new voxel_4_c(pars, this);
    default:
      if (isDescribed(type)) return new voxel_lattice_c(pars, this);
      return 0;
  }
}

voxel_c * gridType_c::getVoxel(const voxel_c & orig) const
{
  switch (type) {
    case GT_BRICKS:           return new voxel_0_c(orig);
    case GT_TRIANGULAR_PRISM: return new voxel_1_c(orig);
    case GT_SPHERES:          return new voxel_2_c(orig);
    case GT_RHOMBIC:          return new voxel_3_c(orig);
    case GT_TETRA_OCTA:       return new voxel_4_c(orig);
    default:
      if (isDescribed(type)) return new voxel_lattice_c(orig);
      return 0;
  }
}

voxel_c * gridType_c::getVoxel(const voxel_c * orig) const
{
  switch (type) {
    case GT_BRICKS:           return new voxel_0_c(orig);
    case GT_TRIANGULAR_PRISM: return new voxel_1_c(orig);
    case GT_SPHERES:          return new voxel_2_c(orig);
    case GT_RHOMBIC:          return new voxel_3_c(orig);
    case GT_TETRA_OCTA:       return new voxel_4_c(orig);
    default:
      if (isDescribed(type)) return new voxel_lattice_c(orig);
      return 0;
  }
}

const symmetries_c * gridType_c::getSymmetries(void) const
{
  std::call_once(symOnce, [this] {
    switch(type) {
      case GT_BRICKS:
      case GT_RHOMBIC:
      case GT_TETRA_OCTA:
        sym = std::make_unique<symmetries_0_c>();
        break;
      case GT_TRIANGULAR_PRISM:
        sym = std::make_unique<symmetries_1_c>();
        break;
      case GT_SPHERES:
        sym = std::make_unique<symmetries_2_c>();
        break;
      default:
        if (isDescribed(type))
          sym = std::make_unique<symmetries_lattice_c>(latticeGridInfo_c::get(type));
        break;
    }
  });

  return sym.get();
}

unsigned int gridType_c::getCapabilities(void) const
{
  switch (type)
  {
    case GT_BRICKS:           return CAP_ASSEMBLE
                                   | CAP_DISASSEMBLE
                                   | CAP_STLEXPORT
                                   ;

    case GT_TRIANGULAR_PRISM: return CAP_ASSEMBLE
                                   | CAP_DISASSEMBLE
                                   | CAP_STLEXPORT
                                   ;

    case GT_SPHERES:          return CAP_ASSEMBLE
                                   | CAP_STLEXPORT
                                   ;

    case GT_RHOMBIC:          return CAP_ASSEMBLE
                                   | CAP_STLEXPORT
                                   ;

    case GT_TETRA_OCTA:       return CAP_ASSEMBLE
                                   | CAP_STLEXPORT
                                   ;
    /* described grids: assembly; disassembly where the grid has movement
     * directions that are lattice vectors; STL export through the
     * Minkowski mesher with the cells' polyhedra (minkmesh.h) */
    default:
      if (!isDescribed(type)) return 0;
      return CAP_ASSEMBLE | CAP_STLEXPORT | (latticeGridInfo_c::get(type).canDisassemble() ? CAP_DISASSEMBLE : 0);
  }
}

std::unique_ptr<assembler_c> gridType_c::findAssembler(std::shared_ptr<const solverSpec_t> p, const solverSettings_t & settings)
{
  if (assembler_1_c::canHandle(*p)) {
    fprintf(stderr, "using assembler 1\n");
    return std::make_unique<assembler_1_c>(std::move(p), settings);
  }

  return nullptr;
}

stlExporter_c * gridType_c::getStlExporter(void) const
{
  switch (type) {
    case GT_BRICKS:           return new stlExporter_0_c(true);
    case GT_TRIANGULAR_PRISM: return new stlExporter_0_c();
    case GT_SPHERES:          return new stlExporter_2_c();
    case GT_RHOMBIC:          return new stlExporter_0_c();
    case GT_TETRA_OCTA:       return new stlExporter_0_c();
    default: return isDescribed(type) ? new stlExporter_0_c() : 0;
  }
}

namespace {

inline int floorDivInt(int a, int b) {
  bt_assert(b > 0);
  int q = a / b;
  int r = a % b;
  if (r < 0) q--;
  return q;
}

inline int roundNear(double x) {
  return static_cast<int>(x + (x < 0.0 ? -0.5 : 0.5));
}

// Native cube rotation/mirror matrices (shared by GT_BRICKS, GT_RHOMBIC, GT_TETRA_OCTA)
static const int kCubeRotMatrices[][9] = {
#include "tabs_0/rotmatrix.inc"
};
constexpr unsigned int kNumCubeTurns = sizeof(kCubeRotMatrices) / sizeof(kCubeRotMatrices[0]);

// Native triangular prism rotation/mirror matrices (GT_TRIANGULAR_PRISM)
static const double kPrismRotMatrices[][9] = {
#include "tabs_1/rotmatrix.inc"
};
constexpr unsigned int kNumPrismTurns = sizeof(kPrismRotMatrices) / sizeof(kPrismRotMatrices[0]);

// Native sphere rotation/mirror matrices (GT_SPHERES)
static const double kSphereRotMatrices[][9] = {
#include "tabs_2/rotmatrix.inc"
};
constexpr unsigned int kNumSphereTurns = sizeof(kSphereRotMatrices) / sizeof(kSphereRotMatrices[0]);

// Native rhombic (GT_RHOMBIC, period 5): 24 base positions in [0..4]^3
struct rhombicBases_s {
  std::array<vec3i_t, 24> pos{};
  std::array<int8_t, 125> idx{};

  rhombicBases_s() {
    idx.fill(-1);
    int k = 0;
    for (int z = 0; z < 5; z++)
      for (int y = 0; y < 5; y++)
        for (int x = 0; x < 5; x++) {
          int ax = x > 2 ? 4 - x : x;
          int ay = y > 2 ? 4 - y : y;
          int az = z > 2 ? 4 - z : z;
          if (ax != ay && ay != az && ax != az) {
            pos[k] = { x, y, z };
            idx[x + 5 * (y + 5 * z)] = static_cast<int8_t>(k);
            k++;
          }
        }
    bt_assert(k == 24);
  }
};

const rhombicBases_s & rhombicBases() {
  static const rhombicBases_s table;
  return table;
}

// Native tetra-octa (GT_TETRA_OCTA): 10 base positions across even (x in 0..2) and odd (x in 3..5) cubes
constexpr std::array<vec3i_t, 10> kTetraOctaBases = {{
  {1, 1, 1}, {2, 0, 0}, {0, 2, 0}, {0, 0, 2}, {2, 2, 2},
  {4, 1, 1}, {3, 0, 0}, {5, 2, 0}, {3, 2, 2}, {5, 0, 2},
}};

int tetraOctaBaseIndex(int rx, int ry, int rz) {
  for (int k = 0; k < 10; k++)
    if (kTetraOctaBases[k].x == rx && kTetraOctaBases[k].y == ry && kTetraOctaBases[k].z == rz)
      return k;
  return -1;
}

// Exact native triangular prism voxel point transform (matches voxel_1_c::transformPoint)
vec3i_t transformPrismVoxel(vec3i_t v, unsigned int turn) {
  const double xs = 0.5 + 0.5 * v.x;
  double ys = v.y * std::sqrt(0.75);
  const double zs = v.z;
  ys += ((v.x + v.y) & 1) ? std::sqrt(1.0 / 3.0) : std::sqrt(1.0 / 12.0);

  const double * rm = kPrismRotMatrices[turn];
  double xpn = rm[0] * xs + rm[1] * ys + rm[2] * zs;
  double ypn = rm[3] * xs + rm[4] * ys + rm[5] * zs;
  double zpn = rm[6] * xs + rm[7] * ys + rm[8] * zs;

  xpn = (xpn - 0.5) * 2.0;
  ypn /= std::sqrt(0.75);

  return { roundNear(xpn), static_cast<int>(std::floor(ypn)), roundNear(zpn) };
}

// Exact voxel-space point transform for periodic grids (not GT_SPHERES).
// For GT_RHOMBIC and GT_TETRA_OCTA, negating an axis reflects about the
// cell-period centre (x -> -x - 1), matching voxel_3_c / voxel_4_c's
// padded voxel_0_c::transform().
vec3i_t transformPeriodicVoxel(const gridType_c & gt, vec3i_t v, unsigned int turn) {
  switch (gt.getType()) {
    case gridType_c::GT_BRICKS: {
      const int * rm = kCubeRotMatrices[turn];
      return {
        rm[0] * v.x + rm[1] * v.y + rm[2] * v.z,
        rm[3] * v.x + rm[4] * v.y + rm[5] * v.z,
        rm[6] * v.x + rm[7] * v.y + rm[8] * v.z
      };
    }
    case gridType_c::GT_RHOMBIC:
    case gridType_c::GT_TETRA_OCTA: {
      const int * rm = kCubeRotMatrices[turn];
      const int sx = (rm[0] < 0 || rm[1] < 0 || rm[2] < 0) ? -1 : 0;
      const int sy = (rm[3] < 0 || rm[4] < 0 || rm[5] < 0) ? -1 : 0;
      const int sz = (rm[6] < 0 || rm[7] < 0 || rm[8] < 0) ? -1 : 0;
      return {
        rm[0] * v.x + rm[1] * v.y + rm[2] * v.z + sx,
        rm[3] * v.x + rm[4] * v.y + rm[5] * v.z + sy,
        rm[6] * v.x + rm[7] * v.y + rm[8] * v.z + sz
      };
    }
    case gridType_c::GT_TRIANGULAR_PRISM:
      return transformPrismVoxel(v, turn);
    default: {
      bt_assert(gt.isDescribed());
      int x = v.x, y = v.y, z = v.z;
      const bool ok = latticeGridInfo_c::get(gt.getType()).apply(turn, x, y, z);
      bt_assert(ok);
      (void)ok;
      return { x, y, z };
    }
  }
}

// Precomputed O(1) affine cell transform table for every periodic grid:
// transformCell((n, k), t) = (M[t] * n + shift[t][k], newKind[t][k]).
struct periodicCellTable_s {
  struct turnMap_s {
    int M[9] = {1,0,0, 0,1,0, 0,0,1};
    std::vector<vec3i_t> shift;
    std::vector<uint16_t> newKind;
  };
  std::vector<turnMap_s> turns;

  explicit periodicCellTable_s(gridType_c::gridType t) {
    gridType_c gt(t);
    const unsigned int K = gt.kindCount();
    const unsigned int nt = gt.getSymmetries()->getNumTransformationsMirror();
    turns.resize(nt);

    for (unsigned int tr = 0; tr < nt; tr++) {
      auto & tm = turns[tr];
      tm.shift.resize(K);
      tm.newKind.resize(K);

      for (unsigned int k = 0; k < K; k++) {
        const vec3i_t v0 = gt.cellToVoxel({ {0, 0, 0}, static_cast<uint16_t>(k) });
        const vec3i_t w0 = transformPeriodicVoxel(gt, v0, tr);
        const auto mapped = gt.voxelToCell(w0);
        bt_assert(mapped.has_value());
        tm.shift[k] = mapped->n;
        tm.newKind[k] = mapped->k;
      }

      // Columns of M[tr]: image of lattice basis vectors e0, e1, e2 from base k = 0
      const vec3i_t base0 = gt.cellToVoxel({ {0, 0, 0}, 0 });
      const vec3i_t wBase = transformPeriodicVoxel(gt, base0, tr);
      const auto cBase = gt.voxelToCell(wBase);
      bt_assert(cBase.has_value());

      const vec3i_t basis[3] = { {1, 0, 0}, {0, 1, 0}, {0, 0, 1} };
      for (int j = 0; j < 3; j++) {
        const vec3i_t vj = gt.cellToVoxel({ basis[j], 0 });
        const vec3i_t wj = transformPeriodicVoxel(gt, vj, tr);
        const auto cj = gt.voxelToCell(wj);
        bt_assert(cj.has_value() && cj->k == cBase->k);
        const vec3i_t col = cj->n - cBase->n;
        tm.M[0 * 3 + j] = col.x;
        tm.M[1 * 3 + j] = col.y;
        tm.M[2 * 3 + j] = col.z;
      }
    }
  }
};

const periodicCellTable_s & periodicCellTable(gridType_c::gridType t) {
  static std::once_flag once[gridType_c::GT_NUM_GRIDS];
  static std::unique_ptr<periodicCellTable_s> tables[gridType_c::GT_NUM_GRIDS];
  bt_assert(t != gridType_c::GT_SPHERES && t < gridType_c::GT_NUM_GRIDS);
  std::call_once(once[t], [t] { tables[t] = std::make_unique<periodicCellTable_s>(t); });
  return *tables[t];
}

} // namespace

unsigned int gridType_c::kindCount(void) const {
  switch (type) {
    case GT_BRICKS:           return 1;
    case GT_TRIANGULAR_PRISM: return 2;
    case GT_SPHERES:          return 1;
    case GT_RHOMBIC:          return 24;
    case GT_TETRA_OCTA:       return 10;
    default:
      if (isDescribed(type))
        return latticeGridInfo_c::get(type).baseCount();
      bt_te("gridType_c::kindCount: unknown grid type");
  }
}

vec3i_t gridType_c::cellToVoxel(cellAddr_t c) const {
  if (c.k >= kindCount())
    bt_te("gridType_c::cellToVoxel: cell kind out of range");

  switch (type) {
    case GT_BRICKS:
      return c.n;

    case GT_TRIANGULAR_PRISM:
      return { 2 * c.n.x + c.n.y + static_cast<int>(c.k), c.n.y, c.n.z };

    case GT_SPHERES:
      return { 2 * c.n.x + c.n.y + c.n.z, c.n.y, c.n.z };

    case GT_RHOMBIC: {
      const vec3i_t & b = rhombicBases().pos[c.k];
      return { 5 * c.n.x + b.x, 5 * c.n.y + b.y, 5 * c.n.z + b.z };
    }

    case GT_TETRA_OCTA: {
      const vec3i_t & b = kTetraOctaBases[c.k];
      return {
        6 * c.n.x + 3 * c.n.y + 3 * c.n.z + b.x,
        3 * c.n.y + b.y,
        3 * c.n.z + b.z
      };
    }

    default: {
      bt_assert(isDescribed(type));
      const latticeGridInfo_c & inf = latticeGridInfo_c::get(type);
      const auto b = inf.basePosition(c.k);
      int lx0, ly0, lz0, lx1, ly1, lz1, lx2, ly2, lz2;
      inf.latticeColumn(0, lx0, ly0, lz0);
      inf.latticeColumn(1, lx1, ly1, lz1);
      inf.latticeColumn(2, lx2, ly2, lz2);
      return {
        b[0] + lx0 * c.n.x + lx1 * c.n.y + lx2 * c.n.z,
        b[1] + ly0 * c.n.x + ly1 * c.n.y + ly2 * c.n.z,
        b[2] + lz0 * c.n.x + lz1 * c.n.y + lz2 * c.n.z
      };
    }
  }
}

std::optional<cellAddr_t> gridType_c::voxelToCell(vec3i_t v) const {
  switch (type) {
    case GT_BRICKS:
      return cellAddr_t{ v, 0 };

    case GT_TRIANGULAR_PRISM: {
      const uint16_t k = static_cast<uint16_t>((v.x + v.y) & 1);
      // v.x - v.y - k is always even because (v.x - v.y) has same parity as (v.x + v.y).
      const int n0 = (v.x - v.y - static_cast<int>(k)) / 2;
      return cellAddr_t{ { n0, v.y, v.z }, k };
    }

    case GT_SPHERES: {
      if (((v.x + v.y + v.z) & 1) != 0)
        return std::nullopt;
      const int n0 = (v.x - v.y - v.z) / 2;
      return cellAddr_t{ { n0, v.y, v.z }, 0 };
    }

    case GT_RHOMBIC: {
      const int nx = floorDivInt(v.x, 5);
      const int ny = floorDivInt(v.y, 5);
      const int nz = floorDivInt(v.z, 5);
      const int rx = v.x - 5 * nx;
      const int ry = v.y - 5 * ny;
      const int rz = v.z - 5 * nz;
      const int8_t k = rhombicBases().idx[rx + 5 * (ry + 5 * rz)];
      if (k < 0)
        return std::nullopt;
      return cellAddr_t{ { nx, ny, nz }, static_cast<uint16_t>(k) };
    }

    case GT_TETRA_OCTA: {
      const int xc = floorDivInt(v.x, 3);
      const int yc = floorDivInt(v.y, 3);
      const int zc = floorDivInt(v.z, 3);
      const int xs = v.x - 3 * xc;
      const int ys = v.y - 3 * yc;
      const int zs = v.z - 3 * zc;
      const int odd = (xc + yc + zc) & 1;
      const int k = tetraOctaBaseIndex(xs + 3 * odd, ys, zs);
      if (k < 0)
        return std::nullopt;
      const int n0 = (xc - yc - zc - odd) / 2;
      return cellAddr_t{ { n0, yc, zc }, static_cast<uint16_t>(k) };
    }

    default: {
      if (!isDescribed(type))
        return std::nullopt;
      const auto [b, e] = latticeGridInfo_c::get(type).decomposeVoxel(v.x, v.y, v.z);
      if (b < 0)
        return std::nullopt;
      return cellAddr_t{ { e[0], e[1], e[2] }, static_cast<uint16_t>(b) };
    }
  }
}

vec3i_t gridType_c::latticeToVoxelDelta(vec3i_t dn) const {
  switch (type) {
    case GT_BRICKS:
      return dn;
    case GT_TRIANGULAR_PRISM:
      return { 2 * dn.x + dn.y, dn.y, dn.z };
    case GT_SPHERES:
      return { 2 * dn.x + dn.y + dn.z, dn.y, dn.z };
    case GT_RHOMBIC:
      return { 5 * dn.x, 5 * dn.y, 5 * dn.z };
    case GT_TETRA_OCTA:
      return { 6 * dn.x + 3 * dn.y + 3 * dn.z, 3 * dn.y, 3 * dn.z };
    default: {
      bt_assert(isDescribed(type));
      const latticeGridInfo_c & inf = latticeGridInfo_c::get(type);
      int lx0, ly0, lz0, lx1, ly1, lz1, lx2, ly2, lz2;
      inf.latticeColumn(0, lx0, ly0, lz0);
      inf.latticeColumn(1, lx1, ly1, lz1);
      inf.latticeColumn(2, lx2, ly2, lz2);
      return {
        lx0 * dn.x + lx1 * dn.y + lx2 * dn.z,
        ly0 * dn.x + ly1 * dn.y + ly2 * dn.z,
        lz0 * dn.x + lz1 * dn.y + lz2 * dn.z
      };
    }
  }
}

std::optional<vec3i_t> gridType_c::voxelDeltaToLattice(vec3i_t dv) const {
  switch (type) {
    case GT_BRICKS:
      return dv;
    case GT_TRIANGULAR_PRISM:
      if ((dv.x + dv.y) & 1) return std::nullopt;
      return vec3i_t{ (dv.x - dv.y) / 2, dv.y, dv.z };
    case GT_SPHERES:
      if ((dv.x + dv.y + dv.z) & 1) return std::nullopt;
      return vec3i_t{ (dv.x - dv.y - dv.z) / 2, dv.y, dv.z };
    case GT_RHOMBIC:
      if (dv.x % 5 != 0 || dv.y % 5 != 0 || dv.z % 5 != 0) return std::nullopt;
      return vec3i_t{ dv.x / 5, dv.y / 5, dv.z / 5 };
    case GT_TETRA_OCTA: {
      if (dv.x % 3 != 0 || dv.y % 3 != 0 || dv.z % 3 != 0) return std::nullopt;
      const int cx = dv.x / 3, cy = dv.y / 3, cz = dv.z / 3;
      if ((cx + cy + cz) & 1) return std::nullopt;
      return vec3i_t{ (cx - cy - cz) / 2, cy, cz };
    }
    default: {
      if (!isDescribed(type)) return std::nullopt;
      int n0 = 0, n1 = 0, n2 = 0;
      if (!latticeGridInfo_c::get(type).decomposeLatticeVector(dv.x, dv.y, dv.z, n0, n1, n2))
        return std::nullopt;
      return vec3i_t{ n0, n1, n2 };
    }
  }
}

std::optional<cellAddr_t> gridType_c::transformCell(cellAddr_t c, unsigned int turn) const {
  const unsigned int nt = getSymmetries()->getNumTransformationsMirror();
  if (turn >= nt)
    bt_te("gridType_c::transformCell: turn out of range");
  if (c.k >= kindCount())
    bt_te("gridType_c::transformCell: cell kind out of range");

  if (type == GT_SPHERES) {
    const vec3i_t v = cellToVoxel(c);
    const double * rm = kSphereRotMatrices[turn];
    const double xpn = rm[0] * v.x + rm[1] * v.y + rm[2] * v.z;
    const double ypn = rm[3] * v.x + rm[4] * v.y + rm[5] * v.z;
    const double zpn = rm[6] * v.x + rm[7] * v.y + rm[8] * v.z;
    const int xn = roundNear(xpn);
    const int yn = roundNear(ypn);
    const int zn = roundNear(zpn);
    if (std::fabs(xpn - xn) > 0.01 || std::fabs(ypn - yn) > 0.01 || std::fabs(zpn - zn) > 0.01)
      return std::nullopt;
    return voxelToCell({ xn, yn, zn });
  }

  const auto & tm = periodicCellTable(type).turns[turn];
  const vec3i_t n = {
    tm.M[0] * c.n.x + tm.M[1] * c.n.y + tm.M[2] * c.n.z + tm.shift[c.k].x,
    tm.M[3] * c.n.x + tm.M[4] * c.n.y + tm.M[5] * c.n.z + tm.shift[c.k].y,
    tm.M[6] * c.n.x + tm.M[7] * c.n.y + tm.M[8] * c.n.z + tm.shift[c.k].z
  };
  return cellAddr_t{ n, tm.newKind[c.k] };
}

bool gridType_c::transformCells(std::vector<cellEntry_t> & cells, unsigned int turn) const {
  const unsigned int nt = getSymmetries()->getNumTransformationsMirror();
  if (turn >= nt)
    bt_te("gridType_c::transformCells: turn out of range");
  if (cells.empty())
    return true;

  if (type == GT_SPHERES) {
    // Rotate relative to the first cell, matching voxel_2_c::transform:
    // sphere turns are valid per shape depending on inter-ball difference vectors.
    const double * rm = kSphereRotMatrices[turn];
    const vec3i_t v0 = cellToVoxel(cells[0].cell);
    for (auto & e : cells) {
      if (e.cell.k != 0)
        bt_te("gridType_c::transformCells: invalid sphere cell kind");
      const vec3i_t dv = cellToVoxel(e.cell) - v0;
      const double xpn = rm[0] * dv.x + rm[1] * dv.y + rm[2] * dv.z;
      const double ypn = rm[3] * dv.x + rm[4] * dv.y + rm[5] * dv.z;
      const double zpn = rm[6] * dv.x + rm[7] * dv.y + rm[8] * dv.z;
      const int xn = roundNear(xpn);
      const int yn = roundNear(ypn);
      const int zn = roundNear(zpn);
      if (std::fabs(xpn - xn) > 0.01 || std::fabs(ypn - yn) > 0.01 || std::fabs(zpn - zn) > 0.01)
        return false;
      auto addr = voxelToCell({ xn, yn, zn });
      if (!addr)
        return false;
      e.cell = *addr;
    }
    return true;
  }

  const unsigned int K = kindCount();
  const auto & tm = periodicCellTable(type).turns[turn];
  for (auto & e : cells) {
    if (e.cell.k >= K)
      bt_te("gridType_c::transformCells: cell kind out of range");
    const uint16_t k = e.cell.k;
    const vec3i_t & n = e.cell.n;
    e.cell = {
      {
        tm.M[0] * n.x + tm.M[1] * n.y + tm.M[2] * n.z + tm.shift[k].x,
        tm.M[3] * n.x + tm.M[4] * n.y + tm.M[5] * n.z + tm.shift[k].y,
        tm.M[6] * n.x + tm.M[7] * n.y + tm.M[8] * n.z + tm.shift[k].z
      },
      tm.newKind[k]
    };
  }
  return true;
}

std::optional<vec3i_t> gridType_c::transformVoxelOffset(vec3i_t p, vec3i_t ref, unsigned int turn) const {
  const unsigned int nt = getSymmetries()->getNumTransformationsMirror();
  if (turn >= nt)
    bt_te("gridType_c::transformVoxelOffset: turn out of range");

  switch (type) {
    case GT_BRICKS:
    case GT_RHOMBIC:
    case GT_TETRA_OCTA: {
      const vec3i_t d = p - ref;
      const int * rm = kCubeRotMatrices[turn];
      return vec3i_t{
        rm[0] * d.x + rm[1] * d.y + rm[2] * d.z,
        rm[3] * d.x + rm[4] * d.y + rm[5] * d.z,
        rm[6] * d.x + rm[7] * d.y + rm[8] * d.z
      };
    }
    case GT_TRIANGULAR_PRISM:
      return transformPrismVoxel(p, turn) - transformPrismVoxel(ref, turn);
    case GT_SPHERES: {
      const vec3i_t d = p - ref;
      const double * rm = kSphereRotMatrices[turn];
      const double xpn = rm[0] * d.x + rm[1] * d.y + rm[2] * d.z;
      const double ypn = rm[3] * d.x + rm[4] * d.y + rm[5] * d.z;
      const double zpn = rm[6] * d.x + rm[7] * d.y + rm[8] * d.z;
      return vec3i_t{ roundNear(xpn), roundNear(ypn), roundNear(zpn) };
    }
    default: {
      bt_assert(isDescribed(type));
      const latticeGridInfo_c & inf = latticeGridInfo_c::get(type);
      int px = p.x, py = p.y, pz = p.z;
      int rx = ref.x, ry = ref.y, rz = ref.z;
      if (!inf.apply(turn, px, py, pz) || !inf.apply(turn, rx, ry, rz))
        return std::nullopt;
      return vec3i_t{ px - rx, py - ry, pz - rz };
    }
  }
}

mat3d_t gridType_c::physicalTransformMatrix(unsigned int turn) const {
  const unsigned int nt = getSymmetries()->getNumTransformationsMirror();
  if (turn >= nt)
    bt_te("gridType_c::physicalTransformMatrix: turn out of range");

  double m[9];
  switch (type) {
    case GT_BRICKS:
    case GT_RHOMBIC:
    case GT_TETRA_OCTA:
      for (int i = 0; i < 9; i++) m[i] = static_cast<double>(kCubeRotMatrices[turn][i]);
      return mat3d_t::fromArray(m);
    case GT_TRIANGULAR_PRISM:
      return mat3d_t::fromArray(kPrismRotMatrices[turn]);
    case GT_SPHERES:
      return mat3d_t::fromArray(kSphereRotMatrices[turn]);
    default:
      bt_assert(isDescribed(type));
      latticeGridInfo_c::get(type).physicalMatrix(turn, m);
      return mat3d_t::fromArray(m);
  }
}

