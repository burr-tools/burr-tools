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
#include "latticegridinfo.h"

#include "bt_assert.h"
#include "symmetries.h"
#include "latticegrid/griddescriptions.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <mutex>
#include <bit>
#include <numeric>

using BurrTools::Maths::Vector;
using BurrTools::PuzzleLibrary::Grid;
using BurrTools::PuzzleLibrary::RotationalSymmetryGroup;
using BurrTools::PuzzleLibrary::TRotation;

struct latticeGridInfo_c::ng_s {
  Grid grid;
  std::shared_ptr<const Grid::ExpensiveInformation> inf;
};

namespace {

size_t ngIdOf(gridType_c::gridType type) {
  using namespace BurrTools::PuzzleLibrary;
  switch (type) {
    case gridType_c::GT_HEXAGONAL_PRISM:            return NG_GRID_HEXAGONAL_PRISMS;
    case gridType_c::GT_CAIRO_PRISM:                return NG_GRID_CAIRO_PRISMS;
    case gridType_c::GT_RHOMBIC_TETRAHEDRA:         return NG_GRID_RHOMBIC_TETRAHEDRA;
    case gridType_c::GT_OCTAHEDRAL_TETRAHEDRA:      return NG_GRID_OCTAHEDRAL_TETRAHEDRA;
    case gridType_c::GT_DESCRIBED_BRICKS:           return NG_GRID_CUBES;
    case gridType_c::GT_DESCRIBED_TRIANGULAR_PRISM: return NG_GRID_TRIANGULAR_PRISMS;
    case gridType_c::GT_BRICKS:
    case gridType_c::GT_TRIANGULAR_PRISM:
    case gridType_c::GT_SPHERES:
    case gridType_c::GT_RHOMBIC:
    case gridType_c::GT_TETRA_OCTA:
    case gridType_c::GT_NUM_GRIDS:
      break;
  }
  bt_te("latticeGridInfo_c: not a described grid");
}

// the voxel's polyhedron vertices, in physical space
std::vector<Vector<float>> physicalVertices(const Grid & g, const Grid::Information & inf, const Vector<int> & p) {
  auto [b, e] = g.getBaseForVoxelPosition(p);
  std::vector<Vector<float>> out;
  if (b < 0) return out;
  const Vector<float> o = inf.physicalTranslations * Vector<float>((float)e.x(), (float)e.y(), (float)e.z());
  for (const auto & v : inf.physicalShapes[b].vertices)
    out.push_back(v + o);
  return out;
}

} // namespace

const latticeGridInfo_c & latticeGridInfo_c::get(gridType_c::gridType type) {

  static std::once_flag once[gridType_c::GT_NUM_GRIDS];
  static std::unique_ptr<latticeGridInfo_c> infos[gridType_c::GT_NUM_GRIDS];

  if ((int)type < 0 || type >= gridType_c::GT_NUM_GRIDS)
    bt_te("latticeGridInfo_c: grid type out of range");

  std::call_once(once[type], [type] { infos[type].reset(new latticeGridInfo_c(type)); });
  return *infos[type];
}

// here, where ng_s is complete
latticeGridInfo_c::~latticeGridInfo_c() = default;

latticeGridInfo_c::latticeGridInfo_c(gridType_c::gridType type) : ngGrid(ngIdOf(type)) {

  BurrTools::PuzzleLibrary::registerNgGrids();

  auto n = std::make_unique<ng_s>(ng_s{Grid(Grid::StorageId{{ngGrid}}), nullptr});
  n->inf = n->grid.getExpensiveInformation();
  ng = std::move(n);
  const Grid & g = ng->grid;
  const Grid::ExpensiveInformation & inf = *ng->inf;

  // the lattice and the voxel to physical map, as columns
  for (int j = 0; j < 3; j++) {
    Vector<int> e(j == 0, j == 1, j == 2);
    Vector<int> c = inf.translationalSymmetry * e;
    L[0*3+j] = c.x(); L[1*3+j] = c.y(); L[2*3+j] = c.z();
    Vector<float> f = inf.voxelToPhysical * Vector<float>(j == 0, j == 1, j == 2);
    v2p[0*3+j] = f.x(); v2p[1*3+j] = f.y(); v2p[2*3+j] = f.z();
  }

  // the grid's rotations, then the same mirrored
  std::vector<int> rot;
  for (int r = 0; r < TRotation::ROT_MIRROR; r++)
    if (!inf.validTransformations[r].empty())
      rot.push_back(r);
  bt_assert(!rot.empty() && rot[0] == TRotation::ROT_IDENTITY);
  numRot = (unsigned int)rot.size();
  for (int r : rot) {
    if (inf.validTransformations[r + TRotation::ROT_MIRROR].empty())
      bt_te("latticeGridInfo_c: a rotation of the grid without its mirrored counterpart");
  }

  // physical to voxel, the inverse of v2p, for the physical rotations
  double inv[9];
  {
    double m[9];
    for (int k = 0; k < 9; k++) m[k] = v2p[k];
    double det = m[0]*(m[4]*m[8]-m[5]*m[7]) - m[1]*(m[3]*m[8]-m[5]*m[6]) + m[2]*(m[3]*m[7]-m[4]*m[6]);
    bt_assert(std::fabs(det) > 1e-9);
    inv[0] =  (m[4]*m[8]-m[5]*m[7])/det; inv[1] = -(m[1]*m[8]-m[2]*m[7])/det; inv[2] =  (m[1]*m[5]-m[2]*m[4])/det;
    inv[3] = -(m[3]*m[8]-m[5]*m[6])/det; inv[4] =  (m[0]*m[8]-m[2]*m[6])/det; inv[5] = -(m[0]*m[5]-m[2]*m[3])/det;
    inv[6] =  (m[3]*m[7]-m[4]*m[6])/det; inv[7] = -(m[0]*m[7]-m[1]*m[6])/det; inv[8] =  (m[0]*m[4]-m[1]*m[3])/det;
  }

  trans.resize(2*numRot);
  for (unsigned int t = 0; t < 2*numRot; t++) {
    trans_s & ts = trans[t];
    ts.ng = rot[t % numRot] + (t >= numRot ? (int)TRotation::ROT_MIRROR : 0);
    const TRotation r(ts.ng);
    const auto & R = r.matrix();
    const auto & sh = inf.validTransformations[ts.ng][0];

    const int dm = R.denominator();
    const int ds = sh.denominator();
    ts.den = std::lcm(dm, ds);

    for (int j = 0; j < 3; j++) {
      auto c = R * Vector<int>(j == 0, j == 1, j == 2);
      const int f = ts.den / c.denominator();
      ts.m[0*3+j] = c.x() * f; ts.m[1*3+j] = c.y() * f; ts.m[2*3+j] = c.z() * f;
    }
    ts.t[0] = sh.x() * (ts.den / ds);
    ts.t[1] = sh.y() * (ts.den / ds);
    ts.t[2] = sh.z() * (ts.den / ds);

    // physical: v2p * (m / den) * inv
    double tmp[9];
    for (int i = 0; i < 3; i++)
      for (int j = 0; j < 3; j++) {
        double s = 0;
        for (int k = 0; k < 3; k++) s += v2p[i*3+k] * (double)ts.m[k*3+j] / ts.den;
        tmp[i*3+j] = s;
      }
    for (int i = 0; i < 3; i++)
      for (int j = 0; j < 3; j++) {
        double s = 0;
        for (int k = 0; k < 3; k++) s += tmp[i*3+k] * inv[k*3+j];
        ts.phys[i*3+j] = std::fabs(s) < 1e-9 ? 0 : s;
      }
  }

  // products: first t1, then t2 is NG's t2 * t1
  const unsigned int nt = 2*numRot;
  mult.assign(nt * nt, TND);
  for (unsigned int t1 = 0; t1 < nt; t1++)
    for (unsigned int t2 = 0; t2 < nt; t2++) {
      const TRotation p = TRotation(trans[t2].ng) * TRotation(trans[t1].ng);
      if (!p) continue;
      for (unsigned int t = 0; t < nt; t++)
        if (trans[t].ng == p.value()) {
          mult[t1 * nt + t2] = (unsigned char)t;
          break;
        }
    }

  // NG's symmetry groups in our numbering; 0 for groups with rotations the grid does not have
  bt_assert(nt <= 64); // a group is a 64 bit mask of transformations
  for (auto grp : RotationalSymmetryGroup::allGroups()) {
    const auto s = grp.symmetries();
    uint64_t bits = 0;
    int inGroup = 0;
    for (int r = 0; r < (int)TRotation::NUM_ROTATION_MATRICES; r++)
      if (s.contains(TRotation(r))) inGroup++;
    for (unsigned int t = 0; t < nt; t++)
      if (s.contains(TRotation(trans[t].ng)))
        bits |= uint64_t(1) << t;
    groups.push_back(std::popcount(bits) == inGroup ? bits : 0);
  }

  // movement: directions, the cells each step must find empty
  for (const auto & v : inf.movementVectors)
    dirs.push_back({v.x(), v.y(), v.z()});
  checks.resize(inf.basePositions.size());
  for (size_t b = 0; b < inf.basePositions.size(); b++) {
    checks[b].resize(dirs.size());
    for (size_t d = 0; d < dirs.size() && b < inf.movementChecks.size(); d++)
      for (const auto & c : inf.movementChecks[b][d]) {
        checks[b][d].push_back({c.x(), c.y(), c.z()});
        reach = std::max({reach, std::abs((int)c.x()), std::abs((int)c.y()), std::abs((int)c.z())});
      }
  }
  disassemble = !dirs.empty();
  for (const auto & d : dirs)
    if (!g.isValidTranslation(Vector<int>(d[0], d[1], d[2])))
      disassemble = false;

  // how far each kind of voxel reaches past its point in drawing space
  for (size_t b = 0; b < inf.basePositions.size(); b++) {
    const Vector<int> p = inf.basePositions[b];
    float c[3] = {(float)p.x(), (float)p.y(), (float)p.z()};
    toPhysical(c[0], c[1], c[2]);
    for (const auto & v : physicalVertices(g, inf, p)) {
      const float d[3] = {v.x() - c[0], v.y() - c[1], v.z() - c[2]};
      for (int k = 0; k < 3; k++) {
        reachLo[k] = std::min(reachLo[k], d[k]);
        reachHi[k] = std::max(reachHi[k], d[k]);
      }
    }
  }

  // edge and corner neighbours: voxels whose polyhedra share exactly two
  // vertices, or exactly one (face neighbours share a whole face)
  edgeNb.resize(inf.basePositions.size());
  cornerNb.resize(inf.basePositions.size());
  const int R = 7;
  for (size_t b = 0; b < inf.basePositions.size(); b++) {
    const Vector<int> p = inf.basePositions[b];
    const auto pv = physicalVertices(g, inf, p);
    for (int dx = -R; dx <= R; dx++)
      for (int dy = -R; dy <= R; dy++)
        for (int dz = -R; dz <= R; dz++) {
          if (!dx && !dy && !dz) continue;
          const Vector<int> q = p + Vector<int>(dx, dy, dz);
          const auto qv = physicalVertices(g, inf, q);
          if (qv.empty()) continue;
          int shared = 0;
          for (const auto & a : pv)
            for (const auto & c : qv)
              if (std::fabs(a.x()-c.x()) < 1e-3f && std::fabs(a.y()-c.y()) < 1e-3f && std::fabs(a.z()-c.z()) < 1e-3f)
                shared++;
          if (shared == 2) edgeNb[b].push_back({dx, dy, dz});
          else if (shared == 1) cornerNb[b].push_back({dx, dy, dz});
        }
  }
}

int latticeGridInfo_c::base(int x, int y, int z) const {
  return ng->grid.getBaseForVoxelPosition(Vector<int>(x, y, z)).first;
}

unsigned int latticeGridInfo_c::baseCount(void) const {
  return static_cast<unsigned int>(ng->inf->basePositions.size());
}

std::array<int, 3> latticeGridInfo_c::basePosition(unsigned int k) const {
  bt_assert(k < ng->inf->basePositions.size());
  const auto & p = ng->inf->basePositions[k];
  return { p.x(), p.y(), p.z() };
}

std::pair<int, std::array<int, 3>> latticeGridInfo_c::decomposeVoxel(int x, int y, int z) const {
  auto [b, e] = ng->grid.getBaseForVoxelPosition(Vector<int>(x, y, z));
  return { b, { e.x(), e.y(), e.z() } };
}

bool latticeGridInfo_c::decomposeLatticeVector(int dx, int dy, int dz, int & n0, int & n1, int & n2) const {
  auto [e, rem] = ng->grid.getEpipedAndRemainderForVoxelPosition(Vector<int>(dx, dy, dz));
  if (rem.x() != 0 || rem.y() != 0 || rem.z() != 0)
    return false;
  n0 = e.x();
  n1 = e.y();
  n2 = e.z();
  return true;
}

bool latticeGridInfo_c::isVoxel(int x, int y, int z) const {
  return base(x, y, z) >= 0;
}

bool latticeGridInfo_c::isLatticeVector(int x, int y, int z) const {
  return ng->grid.isValidTranslation(Vector<int>(x, y, z));
}

bool latticeGridInfo_c::apply(unsigned int t, int & x, int & y, int & z) const {
  bt_assert(t < trans.size());
  const trans_s & ts = trans[t];
  const long nx = (long)ts.m[0]*x + (long)ts.m[1]*y + (long)ts.m[2]*z + ts.t[0];
  const long ny = (long)ts.m[3]*x + (long)ts.m[4]*y + (long)ts.m[5]*z + ts.t[1];
  const long nz = (long)ts.m[6]*x + (long)ts.m[7]*y + (long)ts.m[8]*z + ts.t[2];
  if (nx % ts.den || ny % ts.den || nz % ts.den) return false;
  x = (int)(nx / ts.den);
  y = (int)(ny / ts.den);
  z = (int)(nz / ts.den);
  return true;
}

unsigned char latticeGridInfo_c::transformationWithMatrix(const int m[9]) const {
  for (unsigned int t = 0; t < trans.size(); t++) {
    bool same = true;
    for (int k = 0; k < 9 && same; k++)
      same = trans[t].m[k] == (long)m[k] * trans[t].den;
    if (same) return (unsigned char)t;
  }
  return TND;
}

void latticeGridInfo_c::alignDown(int mx, int my, int mz, int & vx, int & vy, int & vz) const {

  // start from the lattice cell of m, then look at the cells around it
  auto [e, rem] = ng->grid.getEpipedAndRemainderForVoxelPosition(Vector<int>(mx, my, mz));
  (void)rem;

  long best = std::numeric_limits<long>::max();
  const int R = 4;
  for (int a = -R; a <= R; a++)
    for (int b = -R; b <= R; b++)
      for (int c = -R; c <= R; c++) {
        const int n0 = e.x() + a, n1 = e.y() + b, n2 = e.z() + c;
        const int ux = L[0]*n0 + L[1]*n1 + L[2]*n2;
        const int uy = L[3]*n0 + L[4]*n1 + L[5]*n2;
        const int uz = L[6]*n0 + L[7]*n1 + L[8]*n2;
        if (ux > mx || uy > my || uz > mz) continue;
        const long cost = ((long)mx - ux) + ((long)my - uy) + ((long)mz - uz);
        if (cost < best) {
          best = cost;
          vx = ux; vy = uy; vz = uz;
        }
      }
  if (best == std::numeric_limits<long>::max())
    bt_te("latticeGridInfo_c::alignDown: no lattice vector below the point");
}

void latticeGridInfo_c::physicalMatrix(unsigned int t, double m[9]) const {
  bt_assert(t < trans.size());
  for (int k = 0; k < 9; k++) m[k] = trans[t].phys[k];
}

unsigned char latticeGridInfo_c::flip(int axis) const {
  for (unsigned int t = 0; t < trans.size(); t++) {
    bool same = true;
    for (int r = 0; r < 3 && same; r++)
      for (int c = 0; c < 3 && same; c++) {
        const double want = (r != c) ? 0.0 : (r == axis ? -1.0 : 1.0);
        same = std::fabs(trans[t].phys[3*r+c] - want) < 1e-6;
      }
    if (same) return (unsigned char)t;
  }
  return TND;
}

void latticeGridInfo_c::toPhysical(float & x, float & y, float & z) const {
  const float a = x, b = y, c = z;
  x = v2p[0]*a + v2p[1]*b + v2p[2]*c;
  y = v2p[3]*a + v2p[4]*b + v2p[5]*c;
  z = v2p[6]*a + v2p[7]*b + v2p[8]*c;
}

unsigned int latticeGridInfo_c::faces(int x, int y, int z) const {
  const int b = base(x, y, z);
  return b < 0 ? 0 : (unsigned int)ng->inf->faceNeighbours[b].size();
}

bool latticeGridInfo_c::faceNeighbour(int x, int y, int z, unsigned int n, int & xn, int & yn, int & zn) const {
  const int b = base(x, y, z);
  if (b < 0 || n >= ng->inf->faceNeighbours[b].size()) return false;
  const auto & d = ng->inf->faceNeighbours[b][n].diff;
  xn = x + d.x();
  yn = y + d.y();
  zn = z + d.z();
  return true;
}

void latticeGridInfo_c::faceCorners(int x, int y, int z, unsigned int n, double /*bevel*/, double offset, std::vector<float> & out) const {

  const Grid::ExpensiveInformation & inf = *ng->inf;
  auto [b, e] = ng->grid.getBaseForVoxelPosition(Vector<int>(x, y, z));
  if (b < 0 || n >= inf.physicalShapes[b].faces.size()) return;

  const Vector<float> o = inf.physicalTranslations * Vector<float>((float)e.x(), (float)e.y(), (float)e.z());
  const size_t first = inf.meshFaceStartIndex[b][n];
  const size_t k = inf.physicalShapes[b].faces[n].size();

  /* Start at the corner with the smallest position, so that the face and
   * its copy on the neighbouring voxel (the same polygon, wound the other
   * way) start at the same corner: the mesh builder's connection prisms
   * pair corner i with the neighbour's corner -i. The bevel is not drawn
   * yet (step G6 of the grid plan); the offset is.
   */
  size_t start = 0;
  for (size_t i = 1; i < k; i++) {
    const auto & a = inf.meshVertices[first + i].position;
    const auto & s = inf.meshVertices[first + start].position;
    if (a.x() < s.x() - 1e-4f ||
        (std::fabs(a.x() - s.x()) <= 1e-4f && (a.y() < s.y() - 1e-4f ||
         (std::fabs(a.y() - s.y()) <= 1e-4f && a.z() < s.z()))))
      start = i;
  }

  for (size_t i = 0; i < k; i++) {
    const auto & v = inf.meshVertices[first + (start + i) % k];
    out.push_back(v.position.x() + o.x() + (float)offset * v.offset.x());
    out.push_back(v.position.y() + o.y() + (float)offset * v.offset.y());
    out.push_back(v.position.z() + o.z() + (float)offset * v.offset.z());
  }
}
