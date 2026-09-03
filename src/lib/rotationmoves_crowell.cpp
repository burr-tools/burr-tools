/* BurrTools
 *
 * Andrew Crowell 90° rotation generator (Sliding-Cube SimpleRot). Independent of rotationMoves_0_c.
 * Feature map (implemented vs not, and how to extend): crowell_solver.h
 */
#include "rotationmoves_crowell.h"

#include "bt_assert.h"
#include "disassemblernode.h"
#include "movementcache.h"
#include "problem.h"
#include "puzzle.h"
#include "gridtype.h"
#include "symmetries.h"
#include "voxel.h"

#include <vector>

static const unsigned char ROT_X_P90 = 1;
static const unsigned char ROT_X_M90 = 3;
static const unsigned char ROT_Y_P90 = 12;
static const unsigned char ROT_Y_M90 = 4;
static const unsigned char ROT_Z_P90 = 16;
static const unsigned char ROT_Z_M90 = 20;

namespace {

static void rotateVectorLocal(int * x, int * y, int * z, unsigned int axis, unsigned int sense) {

  int ox = *x, oy = *y, oz = *z;

  if (axis == 0) {
    if (sense == 0) { *x = ox; *y = -oz; *z = oy; }
    else            { *x = ox; *y = oz;  *z = -oy; }
  } else if (axis == 1) {
    if (sense == 0) { *x = oz;  *y = oy; *z = -ox; }
    else            { *x = -oz; *y = oy; *z = ox; }
  } else {
    if (sense == 0) { *x = -oy; *y = ox; *z = oz; }
    else            { *x = oy;  *y = -ox; *z = oz; }
  }
}

static bool rotateDoubledPoint(int * x, int * y, int * z,
                               const rotationRules_c::pivot_t & pivot,
                               unsigned int axis, unsigned int sense) {

  int dx = (*x) * 2 - pivot.hx;
  int dy = (*y) * 2 - pivot.hy;
  int dz = (*z) * 2 - pivot.hz;

  rotateVectorLocal(&dx, &dy, &dz, axis, sense);

  int nx = pivot.hx + dx;
  int ny = pivot.hy + dy;
  int nz = pivot.hz + dz;
  if ((nx | ny | nz) & 1)
    return false;

  *x = nx / 2;
  *y = ny / 2;
  *z = nz / 2;
  return true;
}

static bool rotateCell(const rotationRules_c::cell_t & cell,
                       const rotationRules_c::pivot_t & pivot,
                       unsigned int axis,
                       unsigned int sense,
                       rotationRules_c::cell_t & out) {

  int x = cell.x, y = cell.y, z = cell.z;
  if (!rotateDoubledPoint(&x, &y, &z, pivot, axis, sense))
    return false;
  out = rotationRules_c::cell_t(x, y, z);
  return true;
}

} // namespace

rotationMoves_crowell_c::rotationMoves_crowell_c(const problem_c & puz, movementCache_c * cache_) :
  problem(puz),
  cache(cache_),
  sym(puz.getPuzzle().getGridType()->getSymmetries()),
  searchnode(0),
  pieces(0),
  nextsubset(1),
  nextpivot(0),
  nextaxis(0),
  nextsense(0),
  active(false),
  largestIdx(0),
  skipRevAxis(0),
  skipRevSense(0),
  skipReverse(false),
  cachedMask(0),
  cellsReady(false)
{
  for (unsigned int i = 0; i < 6; i++)
    dirCounts[i] = 0;
}

unsigned char rotationMoves_crowell_c::rotationTransformId(unsigned int axis, unsigned int sense) {

  static const unsigned char ids[3][2] = {
    { ROT_X_P90, ROT_X_M90 },
    { ROT_Y_P90, ROT_Y_M90 },
    { ROT_Z_P90, ROT_Z_M90 }
  };
  bt_assert(axis < 3 && sense < 2);
  return ids[axis][sense];
}

bool rotationMoves_crowell_c::rotateDoubled(int * x, int * y, int * z,
                                            const rotationRules_c::pivot_t & pivot,
                                            unsigned int axis, unsigned int sense) {
  return rotateDoubledPoint(x, y, z, pivot, axis, sense);
}

unsigned int rotationMoves_crowell_c::nextSubsetMask(unsigned int mask, unsigned int n) const {

  const unsigned int allMask = (n >= 32) ? 0xFFFFFFFFu : ((1u << n) - 1u);
  const unsigned int largestBit = (largestIdx < n) ? (1u << largestIdx) : 0u;

  do {
    mask++;
    if (mask > allMask)
      return 0;
  } while ((mask & largestBit) != 0);

  return mask;
}

void rotationMoves_crowell_c::collectWorldCells(unsigned int pieceIdx,
                                                std::vector<rotationRules_c::cell_t> & out) const {

  out.clear();

  unsigned int pieceId = (*pieces)[pieceIdx];
  unsigned int shapeId = cache->getShapeOfPiece(pieceId);
  unsigned char trans = (unsigned char)searchnode->getTrans(pieceIdx);
  const voxel_c * sh = cache->getTransformedShape(shapeId, trans);

  int px = searchnode->getX(pieceIdx);
  int py = searchnode->getY(pieceIdx);
  int pz = searchnode->getZ(pieceIdx);
  int hx = (int)sh->getHx();
  int hy = (int)sh->getHy();
  int hz = (int)sh->getHz();

  for (unsigned int z = 0; z < sh->getZ(); z++)
    for (unsigned int y = 0; y < sh->getY(); y++)
      for (unsigned int x = 0; x < sh->getX(); x++)
        if (sh->isFilled(x, y, z))
          out.push_back(rotationRules_c::cell_t(px - hx + (int)x, py - hy + (int)y, pz - hz + (int)z));
}

unsigned int rotationMoves_crowell_c::countFilled(unsigned int pieceIdx) const {

  unsigned int n = 0;
  unsigned int pieceId = (*pieces)[pieceIdx];
  unsigned int shapeId = cache->getShapeOfPiece(pieceId);
  unsigned char trans = (unsigned char)searchnode->getTrans(pieceIdx);
  const voxel_c * sh = cache->getTransformedShape(shapeId, trans);

  for (unsigned int z = 0; z < sh->getZ(); z++)
    for (unsigned int y = 0; y < sh->getY(); y++)
      for (unsigned int x = 0; x < sh->getX(); x++)
        if (sh->isFilled(x, y, z))
          n++;
  return n;
}

void rotationMoves_crowell_c::loadSubsetCells(void) {

  cachedStart.clear();
  cachedOccupied.clear();
  cachedMask = nextsubset;
  cellsReady = true;

  if (!pieces || !searchnode)
    return;

  for (unsigned int i = 0; i < pieces->size(); i++) {
    if (searchnode->is_piece_removed(i))
      continue;

    std::vector<rotationRules_c::cell_t> cells;
    collectWorldCells(i, cells);
    if (nextsubset & (1u << i)) {
      for (unsigned int c = 0; c < cells.size(); c++)
        cachedStart.push_back(cells[c]);
    } else {
      for (unsigned int c = 0; c < cells.size(); c++)
        cachedOccupied.push_back(cells[c]);
    }
  }
}

void rotationMoves_crowell_c::rebuildPivotCells(unsigned int axis) {

  pivotCells.clear();

  if (cachedStart.empty())
    return;

  int umin, umax, vmin, vmax, amin;
  if (axis == 0) {
    umin = umax = cachedStart[0].y;
    vmin = vmax = cachedStart[0].z;
    amin = cachedStart[0].x;
  } else if (axis == 1) {
    umin = umax = cachedStart[0].x;
    vmin = vmax = cachedStart[0].z;
    amin = cachedStart[0].y;
  } else {
    umin = umax = cachedStart[0].x;
    vmin = vmax = cachedStart[0].y;
    amin = cachedStart[0].z;
  }

  for (unsigned int c = 1; c < cachedStart.size(); c++) {
    const rotationRules_c::cell_t & p = cachedStart[c];
    int u = (axis == 0) ? p.y : p.x;
    int v = (axis == 2) ? p.y : p.z;
    int a = (axis == 0) ? p.x : ((axis == 1) ? p.y : p.z);
    if (u < umin) umin = u;
    if (u > umax) umax = u;
    if (v < vmin) vmin = v;
    if (v > vmax) vmax = v;
    if (a < amin) amin = a;
  }

  const int da = 2 * amin;

  /* Fortran SimpleRot Rmult=1: voxel centres, SW corners, and the far
   * max-u / max-v edges. Not the Classic dense half-index grid. */
  for (int u = umin; u <= umax; u++) {
    for (int v = vmin; v <= vmax; v++) {
      rotationRules_c::pivot_t centre, corner;
      if (axis == 0) {
        centre.hx = da; centre.hy = 2 * u;     centre.hz = 2 * v;
        corner.hx = da; corner.hy = 2 * u - 1; corner.hz = 2 * v - 1;
      } else if (axis == 1) {
        centre.hx = 2 * u;     centre.hy = da; centre.hz = 2 * v;
        corner.hx = 2 * u - 1; corner.hy = da; corner.hz = 2 * v - 1;
      } else {
        centre.hx = 2 * u;     centre.hy = 2 * v;     centre.hz = da;
        corner.hx = 2 * u - 1; corner.hy = 2 * v - 1; corner.hz = da;
      }
      pivotCells.push_back(centre);
      pivotCells.push_back(corner);
    }
    rotationRules_c::pivot_t top;
    if (axis == 0) { top.hx = da; top.hy = 2 * u - 1; top.hz = 2 * vmax + 1; }
    else if (axis == 1) { top.hx = 2 * u - 1; top.hy = da; top.hz = 2 * vmax + 1; }
    else { top.hx = 2 * u - 1; top.hy = 2 * vmax + 1; top.hz = da; }
    pivotCells.push_back(top);
  }
  for (int v = vmin; v <= vmax; v++) {
    rotationRules_c::pivot_t right;
    if (axis == 0) { right.hx = da; right.hy = 2 * umax + 1; right.hz = 2 * v - 1; }
    else if (axis == 1) { right.hx = 2 * umax + 1; right.hy = da; right.hz = 2 * v - 1; }
    else { right.hx = 2 * umax + 1; right.hy = 2 * v - 1; right.hz = da; }
    pivotCells.push_back(right);
  }
  {
    rotationRules_c::pivot_t far;
    if (axis == 0) { far.hx = da; far.hy = 2 * umax + 1; far.hz = 2 * vmax + 1; }
    else if (axis == 1) { far.hx = 2 * umax + 1; far.hy = da; far.hz = 2 * vmax + 1; }
    else { far.hx = 2 * umax + 1; far.hy = 2 * vmax + 1; far.hz = da; }
    pivotCells.push_back(far);
  }
}

void rotationMoves_crowell_c::startAxis(void) {

  while (active) {
    if (!cellsReady || cachedMask != nextsubset)
      loadSubsetCells();

    if (cachedStart.size() <= 1) {
      nextsubset = nextSubsetMask(nextsubset, (unsigned int)pieces->size());
      cellsReady = false;
      nextaxis = 0;
      nextpivot = 0;
      nextsense = 0;
      if (nextsubset == 0)
        active = false;
      continue;
    }

    if (rules.axisBlocked(cachedOccupied, cachedStart, nextaxis)) {
      nextaxis++;
      nextpivot = 0;
      nextsense = 0;
      if (nextaxis >= 3) {
        nextaxis = 0;
        nextsubset = nextSubsetMask(nextsubset, (unsigned int)pieces->size());
        cellsReady = false;
        if (nextsubset == 0)
          active = false;
      }
      continue;
    }

    rebuildPivotCells(nextaxis);
    if (pivotCells.empty()) {
      nextaxis++;
      nextpivot = 0;
      nextsense = 0;
      if (nextaxis >= 3) {
        nextaxis = 0;
        nextsubset = nextSubsetMask(nextsubset, (unsigned int)pieces->size());
        cellsReady = false;
        if (nextsubset == 0)
          active = false;
      }
      continue;
    }
    return;
  }
}

void rotationMoves_crowell_c::advanceCandidate(void) {

  nextsense++;
  if (nextsense >= 2) {
    nextsense = 0;
    nextpivot++;
    if ((unsigned int)nextpivot >= pivotCells.size()) {
      nextpivot = 0;
      nextaxis++;
      if (nextaxis >= 3) {
        nextaxis = 0;
        nextsubset = nextSubsetMask(nextsubset, (unsigned int)pieces->size());
        cellsReady = false;
        if (nextsubset == 0) {
          active = false;
          return;
        }
      }
      startAxis();
    }
  }
}

bool rotationMoves_crowell_c::skipCurrentDirection(void) const {

  unsigned int dir = nextaxis * 2 + nextsense;
  if (dirCounts[dir] >= DCT_MAX)
    return true;

  if (skipReverse && nextaxis == skipRevAxis && nextsense == skipRevSense) {
    unsigned int rp = searchnode->getRotPiece();
    if (rp < pieces->size() && (nextsubset & (1u << rp)))
      return true;
  }

  return false;
}

disassemblerNode_c * rotationMoves_crowell_c::tryCurrentCandidate(void) {

  if ((unsigned int)nextpivot >= pivotCells.size())
    return 0;

  if (!cellsReady || cachedMask != nextsubset)
    loadSubsetCells();

  if (cachedStart.size() <= 1)
    return 0;

  rotationRules_c::pivot_t pivot = pivotCells[nextpivot];

  std::vector<rotationRules_c::cell_t> combinedEnd;
  combinedEnd.reserve(cachedStart.size());

  for (unsigned int i = 0; i < cachedStart.size(); i++) {
    rotationRules_c::cell_t endCell;
    if (!rotateCell(cachedStart[i], pivot, nextaxis, nextsense, endCell))
      return 0;
    combinedEnd.push_back(endCell);
  }

  if (!rules.allowRotation(cachedOccupied, cachedStart, combinedEnd, pivot, nextaxis, nextsense))
    return 0;

  unsigned int primaryPiece = 0;
  while (primaryPiece < pieces->size() && !(nextsubset & (1u << primaryPiece)))
    primaryPiece++;
  bt_assert(primaryPiece < pieces->size());

  unsigned char rotId = rotationTransformId(nextaxis, nextsense);
  bool changed = false;

  unsigned int dir = ROTATION_DIR_BASE + nextaxis * 2 + nextsense;
  disassemblerNode_c * n = new disassemblerNode_c(pieces->size(), searchnode, (int)dir, 1);
  n->setRotationInfo(primaryPiece, pivot.hx, pivot.hy, pivot.hz);

  for (unsigned int i = 0; i < pieces->size(); i++) {
    if (searchnode->is_piece_removed(i)) {
      n->set(i,
             searchnode->getX(i),
             searchnode->getY(i),
             searchnode->getZ(i),
             searchnode->getTrans(i));
      continue;
    }

    if (nextsubset & (1u << i)) {
      unsigned char oldTrans = (unsigned char)searchnode->getTrans(i);
      unsigned char newTrans = sym->transAdd(oldTrans, rotId);

      int px = searchnode->getX(i);
      int py = searchnode->getY(i);
      int pz = searchnode->getZ(i);
      if (!rotateDoubled(&px, &py, &pz, pivot, nextaxis, nextsense)) {
        if (n->decRefCount())
          delete n;
        return 0;
      }

      if (px != searchnode->getX(i) || py != searchnode->getY(i) ||
          pz != searchnode->getZ(i) || newTrans != oldTrans)
        changed = true;

      n->set(i, px, py, pz, newTrans);
    } else {
      n->set(i,
             searchnode->getX(i),
             searchnode->getY(i),
             searchnode->getZ(i),
             searchnode->getTrans(i));
    }
  }

  if (!changed) {
    if (n->decRefCount())
      delete n;
    return 0;
  }

  return n;
}

void rotationMoves_crowell_c::init_find(disassemblerNode_c * nd, const std::vector<unsigned int> & pcs) {

  searchnode = nd;
  pieces = &pcs;
  nextpivot = 0;
  nextaxis = 0;
  nextsense = 0;
  cellsReady = false;
  cachedMask = 0;
  active = pcs.size() > 1;

  for (unsigned int i = 0; i < 6; i++)
    dirCounts[i] = 0;

  largestIdx = 0;
  unsigned int best = 0;
  bool have = false;
  for (unsigned int i = 0; i < pcs.size(); i++) {
    if (nd->is_piece_removed(i))
      continue;
    unsigned int n = countFilled(i);
    if (!have || n >= best) {
      best = n;
      largestIdx = i;
      have = true;
    }
  }

  skipReverse = false;
  if (nd && isRotationDirection(nd->getDirection())) {
    unsigned int code = nd->getDirection() - ROTATION_DIR_BASE;
    skipRevAxis = code / 2;
    skipRevSense = 1u - (code % 2);
    skipReverse = true;
  }

  nextsubset = 1;
  const unsigned int n = (unsigned int)pcs.size();
  if (active && (nextsubset & (1u << largestIdx)))
    nextsubset = nextSubsetMask(0, n);

  if (!nextsubset)
    active = false;

  if (active)
    startAxis();
}

disassemblerNode_c * rotationMoves_crowell_c::find(void) {

  if (!active)
    return 0;

  while (active) {

    if (skipCurrentDirection()) {
      advanceCandidate();
      continue;
    }

    disassemblerNode_c * node = tryCurrentCandidate();

    if (node)
      dirCounts[nextaxis * 2 + nextsense]++;

    advanceCandidate();

    if (node)
      return node;
  }

  return 0;
}
