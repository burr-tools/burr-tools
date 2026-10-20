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
#include "movementanalysator.h"
#include "simd_config.h"

#include "bt_assert.h"
#include "movementcache.h"
#include "problem.h"
#include "puzzle.h"
#include "disassemblernode.h"
#include "voxel.h"
#include "disassemblerhashes.h"
#include "gridtype.h"
#include "symmetries.h"

#include <algorithm>
#include <climits>
#include <memory>

#include <string.h>
#include <bit>

static uint64_t hashPieces(const std::vector<unsigned int> & p) {
  uint64_t h = p.size();
  for (unsigned int x : p)
    h = h * 31 + x;
  return h;
}

void movementAnalysator_c::prepare(void) {

  const unsigned int n = pieces->size();
  const uint64_t pcsHash = hashPieces(*pieces);

  /* Incremental fast path: if the previous prepare() ran for our parent
   * node with the same piece subset, only pairs touching moved pieces
   * can differ. prevSearch is refcounted, hence alive to prevent ABA.
   * pcsHash and prevN guard against pointer reuse of pieces across different
   * subproblem stack frames. */
  bool incremental = prevSearch && searchnode && searchnode->getComefrom() == prevSearch
    && pieces == prevPieces && next_pn == prevN && pcsHash == prevPiecesHash && n > 0;

  std::vector<unsigned int> moved;
  if (incremental) {
    for (unsigned int i = 0; i < n; i++)
      if ((searchnode->getX(i) != prevSearch->getX(i)) ||
          (searchnode->getY(i) != prevSearch->getY(i)) ||
          (searchnode->getZ(i) != prevSearch->getZ(i)) ||
          (searchnode->getTrans(i) != prevSearch->getTrans(i)))
        moved.push_back(i);

    /* prepareIncremental updates prevFill in-place for touched pairs,
     * copies it once to matrix, and applies dirty-worklist closure. */
    prepareIncremental(moved);
  } else {
    prepareFill();
    /* snapshot the FILL matrix: the incremental base must be pre-closure
     * values (closure only decreases and can never repair upward) */
    prevFill = matrix;
    closureFull();
  }

  /* rotate the refcounted owner for the next call */
  if (prevSearch != searchnode) {
    if (prevSearch && prevSearch->decRefCount())
      delete prevSearch;
    prevSearch = searchnode;
    if (prevSearch)
      prevSearch->incRefCount();
  }
  prevPieces = pieces;
  prevPiecesHash = pcsHash;
  prevN = next_pn;
}

void movementAnalysator_c::prepareFill(void) {

  unsigned int * idx = matrix.data();

  int idxCol = cache->numDirections();
  int idxRow = cache->numDirections() * (piecenumber- pieces->size());

  for (unsigned int j = 0; j < pieces->size(); j++) {
    for (unsigned int i = 0; i < pieces->size(); i++) {
      if (i != j)
        cache->getMoValue(searchnode->getX(j) - searchnode->getX(i),
                          searchnode->getY(j) - searchnode->getY(i),
                          searchnode->getZ(j) - searchnode->getZ(i),
                          searchnode->getTrans(i), searchnode->getTrans(j),
                          (*pieces)[i], (*pieces)[j], idx);

      // the diagonals are always zero and will stay that for ever they are initialised
      // to that value in the init function so only the other values need
      idx += idxCol;
    }
    idx += idxRow;
  }

}

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#elif defined(__aarch64__) || defined(__ARM_NEON)
#include <arm_neon.h>
#endif

static bool simdDisabled() {
  return !SimdConfig::isDisassemblerSimdEnabled();
}

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
__attribute__((target("avx512f")))
static void rfw_avx512(unsigned int * block, unsigned int n) {
  for (unsigned int k = 0; k < n; k++) {
    const unsigned int * row_k = block + (size_t)k * n;
    for (unsigned int y = 0; y < n; y++) {
      if (y == k) continue;
      unsigned int * row_y = block + (size_t)y * n;
      unsigned int yk = row_y[k];
      if (yk >= 30000) continue;

      __m512i vyk = _mm512_set1_epi32(yk);
      unsigned int x = 0;
      for (; x + 16 <= n; x += 16) {
        __m512i rk = _mm512_loadu_si512(reinterpret_cast<const void*>(row_k + x));
        __m512i ry = _mm512_loadu_si512(reinterpret_cast<const void*>(row_y + x));
        __m512i sum = _mm512_add_epi32(vyk, rk);
        __m512i min_val = _mm512_min_epu32(ry, sum);
        _mm512_storeu_si512(reinterpret_cast<void*>(row_y + x), min_val);
      }
      for (; x + 8 <= n; x += 8) {
        __m256i rk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(row_k + x));
        __m256i ry = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(row_y + x));
        __m256i sum = _mm256_add_epi32(_mm256_set1_epi32(yk), rk);
        __m256i min_val = _mm256_min_epu32(ry, sum);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(row_y + x), min_val);
      }
      for (; x < n; x++) {
        unsigned int sum = yk + row_k[x];
        if (sum < row_y[x]) row_y[x] = sum;
      }
    }
  }
}

__attribute__((target("avx2")))
static void rfw_avx2(unsigned int * block, unsigned int n) {
  for (unsigned int k = 0; k < n; k++) {
    const unsigned int * row_k = block + (size_t)k * n;
    for (unsigned int y = 0; y < n; y++) {
      if (y == k) continue;
      unsigned int * row_y = block + (size_t)y * n;
      unsigned int yk = row_y[k];
      if (yk >= 30000) continue;

      __m256i vyk = _mm256_set1_epi32(yk);
      unsigned int x = 0;
      for (; x + 8 <= n; x += 8) {
        __m256i rk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(row_k + x));
        __m256i ry = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(row_y + x));
        __m256i sum = _mm256_add_epi32(vyk, rk);
        __m256i min_val = _mm256_min_epu32(ry, sum);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(row_y + x), min_val);
      }
      for (; x < n; x++) {
        unsigned int sum = yk + row_k[x];
        if (sum < row_y[x]) row_y[x] = sum;
      }
    }
  }
}
#endif

#if defined(__aarch64__) || defined(__ARM_NEON)
static void rfw_neon(unsigned int * block, unsigned int n) {
  for (unsigned int k = 0; k < n; k++) {
    const unsigned int * row_k = block + (size_t)k * n;
    for (unsigned int y = 0; y < n; y++) {
      if (y == k) continue;
      unsigned int * row_y = block + (size_t)y * n;
      unsigned int yk = row_y[k];
      if (yk >= 30000) continue;

      uint32x4_t vyk = vdupq_n_u32(yk);
      unsigned int x = 0;
      for (; x + 4 <= n; x += 4) {
        uint32x4_t rk = vld1q_u32(row_k + x);
        uint32x4_t ry = vld1q_u32(row_y + x);
        uint32x4_t sum = vaddq_u32(vyk, rk);
        uint32x4_t min_val = vminq_u32(ry, sum);
        vst1q_u32(row_y + x, min_val);
      }
      for (; x < n; x++) {
        unsigned int sum = yk + row_k[x];
        if (sum < row_y[x]) row_y[x] = sum;
      }
    }
  }
}
#endif

static void rfw_scalar(unsigned int * block, unsigned int n) {
  for (unsigned int k = 0; k < n; k++) {
    const unsigned int * row_k = block + (size_t)k * n;
    for (unsigned int y = 0; y < n; y++) {
      if (y == k) continue;
      unsigned int * row_y = block + (size_t)y * n;
      unsigned int yk = row_y[k];
      if (yk >= 30000) continue;
      for (unsigned int x = 0; x < n; x++) {
        unsigned int sum = yk + row_k[x];
        if (sum < row_y[x]) {
          row_y[x] = sum;
        }
      }
    }
  }
}

void movementAnalysator_c::selectKernels(void) {

  use_avx512_kernel = false;
  use_avx2_kernel = false;
  use_neon_kernel = false;

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  use_avx512_kernel = __builtin_cpu_supports("avx512f") && !simdDisabled() && SimdConfig::isAvx512Allowed();
  use_avx2_kernel = __builtin_cpu_supports("avx2") && !simdDisabled();
#elif defined(__aarch64__) || defined(__ARM_NEON)
  use_neon_kernel = !simdDisabled();
#endif
}

void movementAnalysator_c::closureFull(void) {

  const unsigned int n = pieces->size();
  const unsigned int dirs = cache->numDirections();
  const unsigned int rowStep = dirs * piecenumber;

  if (planar_block.size() < (size_t)n * n) {
    planar_block.resize((size_t)n * n);
  }

  /* Roy-Floyd-Warshall all-pairs shortest paths on movement constraints.
   * Planar memory layout enables contiguous vector loads/stores/mins.
   * Kernel choice was resolved by selectKernels() at run entry. */
  for (unsigned int d = 0; d < dirs; d++) {
    // 1. Pack direction d into contiguous planar block
    for (unsigned int y = 0; y < n; y++) {
      const unsigned int * src = matrix.data() + (size_t)y * rowStep + d;
      unsigned int * dst = planar_block.data() + (size_t)y * n;
      for (unsigned int x = 0; x < n; x++) {
        dst[x] = src[x * dirs];
      }
    }

    // 2. Transitive closure on contiguous planar block
#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
    if (use_avx512_kernel && n >= 16) {
      rfw_avx512(planar_block.data(), n);
    } else if (use_avx2_kernel) {
      rfw_avx2(planar_block.data(), n);
    } else {
      rfw_scalar(planar_block.data(), n);
    }
#elif defined(__aarch64__) || defined(__ARM_NEON)
    if (use_neon_kernel) {
      rfw_neon(planar_block.data(), n);
    } else {
      rfw_scalar(planar_block.data(), n);
    }
#else
    rfw_scalar(planar_block.data(), n);
#endif

    // 3. Unpack planar block back into matrix
    for (unsigned int y = 0; y < n; y++) {
      const unsigned int * src = planar_block.data() + (size_t)y * n;
      unsigned int * dst = matrix.data() + (size_t)y * rowStep + d;
      for (unsigned int x = 0; x < n; x++) {
        dst[x * dirs] = src[x];
      }
    }
  }
}

void movementAnalysator_c::prepareIncremental(const std::vector<unsigned int> & moved) {

  const unsigned int n = pieces->size();
  const unsigned int dirs = cache->numDirections();

  /* Pairs touching a moved piece need fresh cache values; the rest is
   * already correct from the inherited parent base. Panels of untouched
   * pairs keep base values since relative offsets, piece identities, and
   * orientations are unchanged. */
  std::vector<char> isMoved(n, 0);
  for (unsigned int m : moved)
    isMoved[m] = 1;



  for (unsigned int j = 0; j < pieces->size(); j++) {
    for (unsigned int i = 0; i < pieces->size(); i++) {
      if ((i != j) && (isMoved[i] || isMoved[j]))
        cache->getMoValue(searchnode->getX(j) - searchnode->getX(i),
                          searchnode->getY(j) - searchnode->getY(i),
                          searchnode->getZ(j) - searchnode->getZ(i),
                          searchnode->getTrans(i), searchnode->getTrans(j),
                          (*pieces)[i], (*pieces)[j],
                          &prevFill[((size_t)i + (size_t)j * piecenumber) * dirs]);
    }
  }
  /* Single copy from the updated fill snapshot to working matrix for closure */
  matrix = prevFill;

  closureFull();
}

/*
 * suppose you want to move piece x y units into one direction, if you hit another piece
 * on your way and this piece can be moved then it may be nice to also move this piece
 *
 * so this function adjusts the movement of other pieces so that one piece can be moved
 * the requested number of units.
 *
 * in the worst case when no movement in the selected direction is possible all values are
 * set to the same value meaning the whole puzzle is moved
 *
 * to distinguish "good" and "bad" moves the function returns true, if less maxPieces
 * have to be moved, this value should not be larger than halve of the pieces in the puzzle
 */
bool movementAnalysator_c::checkmovement(unsigned int maxPieces, unsigned int nextstep) {

  stats.checkCalls++;

  unsigned int nd = nextdir >> 1;
  unsigned int dirs = cache->numDirections();
  bt_assert(nd < dirs);

  /* Bitboard flood fill over the closure matrix. The 64-bit masks cover at
   * most 64 pieces; larger problems fall through to the classic loop below.
   */
  if (next_pn <= 64) {
    uint64_t moved_mask = 1ULL << nextpiece;
    uint64_t check_mask = moved_mask;
    unsigned int moved_count = 1;
    uint64_t all_pieces_mask = (next_pn == 64) ? ~0ULL : ((1ULL << next_pn) - 1);
    unsigned int rowIdx2 = piecenumber * dirs;

    if (nextdir & 1) {
      while (check_mask) {
        int i = std::countr_zero(check_mask);
        check_mask &= check_mask - 1;

        const unsigned int * row = matrix.data() + nd + (size_t)piecenumber * i * dirs;
        uint64_t candidates = all_pieces_mask & ~moved_mask;
        while (candidates) {
          int j = std::countr_zero(candidates);
          candidates &= candidates - 1;

          if (nextstep > row[j * dirs]) {
            moved_mask |= (1ULL << j);
            check_mask |= (1ULL << j);
            moved_count++;
            if (moved_count > maxPieces)
              return false;
          }
        }
      }
    } else {
      while (check_mask) {
        int i = std::countr_zero(check_mask);
        check_mask &= check_mask - 1;

        const unsigned int * col = matrix.data() + nd + i * dirs;
        uint64_t candidates = all_pieces_mask & ~moved_mask;
        while (candidates) {
          int j = std::countr_zero(candidates);
          candidates &= candidates - 1;

          if (nextstep > col[(size_t)j * rowIdx2]) {
            moved_mask |= (1ULL << j);
            check_mask |= (1ULL << j);
            moved_count++;
            if (moved_count > maxPieces)
              return false;
          }
        }
      }
    }

    for (int i = 0; i < next_pn; i++) {
      movement[i] = (moved_mask & (1ULL << i)) ? nextstep : 0;
    }

    stats.checkSuccess++;
    return true;
  }

  /* Fallback for problems with more than 64 pieces, which the 64-bit
   * bitboard path above cannot represent. We count the number of pieces
   * that need to be moved, if this number gets bigger than halve of the
   * pieces of the current problem we stop and return that this movement
   * is rubbish
   */
  unsigned int moved_pieces = 1;

  /* Initialise the movement matrix. We want to move 'nextpiece' 'nextstep' units
   * into the current direction, so we initialise the matrix with all
   * zero except for our piece
   */
  for (int i = 0; i < next_pn; i++) {
    movement[i] = 0;
    check[i] = false;
  }
  movement[nextpiece] = nextstep;
  check[nextpiece] = true;

  bool finished;
  unsigned int rowIdx = (piecenumber-next_pn)*dirs;
  unsigned int rowIdx2 = piecenumber*dirs;
  unsigned int rowIdx3 = (piecenumber*next_pn-1)*dirs;

  // the idea here is the following, if we want to move
  // a piece the matrix tells us if we can do that with respect to
  // another piece, if we can't that other piece must be moved as well
  // and with that new moved piece we need to check that piece, too
  //
  // the comments are only in the first part the second is the same
  // just for the other directions
  if (nextdir & 1) {

    do {

      finished = true;
      unsigned int * idx = matrix.data() + nd;

      // go over all pieces
      for (int i = 0; i < next_pn; i++)
      {
        // if the piece needs to be checked
        if (check[i])
        {
          // check it against all other pieces
          for (int j = 0; j < next_pn; j++)
          {
            // if it is another piece that is still stationary (if it is already
            // moving it moves by the same amount as the other piece, so there
            // will be no problems here
            if ((i != j) && (movement[j] == 0)) {
              // if the requested movement is more than the matrix allows
              // we must also move the new piece

              if (movement[i] > *idx) {  // idx points to matrix[(j+piecenumber*i)*dirs+nd]
                // count the number of moved pieces, if there are more
                // than halve, we bail out because it doesn't make sense
                // to move more than that amount
                moved_pieces++;
                if (moved_pieces > maxPieces)
                  return false;

                // to we move that new piece by the same amount
                // as the first piece and we also need to check
                // that new piece
                movement[j] = nextstep;
                check[j] = true;
                finished = false;
              }
            }
            idx += dirs;
          }
          // the current piece is now checked, so we don't need to do that again
          check[i] = false;
          idx += rowIdx;
        }
        else
        {
          idx += rowIdx2;
        }
      }
    } while (!finished);

  } else {

    do {

      finished = true;
      unsigned int * idx = matrix.data() + nd;

      for (int i = 0; i < next_pn; i++)
      {
        if (check[i])
        {
          for (int j = 0; j < next_pn; j++) {
            if ((i != j) && (movement[j] == 0)) {
              if (movement[i] > *idx) {  // idx should point to matrix[(i + piecenumber * j)*dirs+nr]
                moved_pieces++;
                if (moved_pieces > maxPieces)
                  return false;

                movement[j] = nextstep;
                check[j] = true;
                finished = false;
              }
            }
            idx += rowIdx2;
          }
          check[i] = false;
          idx -= rowIdx3;
        }
        else
        {
          idx += dirs;
        }
      }

    } while (!finished);
  }

  stats.checkSuccess++;
  return true;
}

movementAnalysator_c::movementAnalysator_c(const problem_c & problem,
                                           std::shared_ptr<const rotationGeometry::rotationTables_c> rotationTables) :
  cache(problem.getPuzzle().getGridType()->getMovementCache(problem)),
  matrix(cache ? cache->numDirections() * problem.getNumberOfPieces() * problem.getNumberOfPieces() : 0, 0),
  movement(problem.getNumberOfPieces()),
  weights(problem.getNumberOfPieces()),
  check(problem.getNumberOfPieces(), 0),
  piecenumber(problem.getNumberOfPieces()),
  planar_block(problem.getNumberOfPieces() * problem.getNumberOfPieces(), 0),
  prevFill(matrix.size(), 0),
  nodes(std::make_unique<countingNodeHash>()),
  nextstate(-1),
  maxstep((unsigned int) -1),
  rotations(std::move(rotationTables)) {

  /* we assert that there must be a cache, otherwise no disassembly
   * analysis is possible anyway and this should not
   * have been called
   */
  bt_assert(cache);

  /* create the weights array */
  unsigned int pc = 0;
  for (unsigned int i = 0; i < problem.getNumberOfParts(); i++) {
    for (unsigned int j = 0; j < problem.getPartMaximum(i); j++)
      weights[pc++] = problem.getPartShape(i)->getWeight();
  }

  if (rotations) {
    rotationProblem = &problem;
    for (unsigned int i = 0; i < problem.getNumberOfParts(); i++) {
      partVoxels.push_back(problem.getPartShape(i));
      for (unsigned int j = 0; j < problem.getPartMaximum(i); j++)
        piecePart.push_back(i);
    }
    hotspotCells.resize(partVoxels.size());
    planeCellCache.resize(partVoxels.size());
  }
}

movementAnalysator_c::~movementAnalysator_c() {
  clearRotations();
  if (prevSearch && prevSearch->decRefCount())
    delete prevSearch;
}

static int max(int a, int b) { if (a > b) return a; else return b; }

/* creates a new node with the information from the movement array
 * where the pieces move by amount in direction nextdir
 * staring point is the searchnode
 */
disassemblerNode_c * movementAnalysator_c::newNode(unsigned int amount) {

  // calculate the weight of the all the stationary and all the
  // moving pieces
  int moveWeight = 0;
  int stilWeight = 0;
  int nd = nextdir;

  for (unsigned int i = 0; i < pieces->size(); i++) {
    if (movement[i]) {
      bt_assert(amount == movement[i]);

      moveWeight = max(moveWeight, weights[(*pieces)[i]]);

    } else {
      stilWeight = max(stilWeight, weights[(*pieces)[i]]);
    }
  }

  /* we need to invert the movement direction, when the
   * weight of the currently moved pieces is bigger than
   * those of stationary pieces
   */
  if (stilWeight < moveWeight) {

    // stationary pieces become moved, moved piece become stationary
    for (unsigned int i = 0; i < pieces->size(); i++)
      if (movement[i])
        movement[i] = 0;
      else
        movement[i] = amount;

    // and the direction changes to the opposite direction
    nd ^= 1;
  }

  disassemblerNode_c * n = new disassemblerNode_c(pieces->size(), searchnode, nd, amount);

  /* create a new state with the pieces moved */
  for (unsigned int i = 0; i < pieces->size(); i++) {

    if (movement[i]) {

      if (movement[i] >= 10000) {

        int mx, my, mz;

        cache->getDirection(nd >> 1, &mx, &my, &mz);

        if (nd & 1) {
          mx = -mx;
          my = -my;
          mz = -mz;
        }

        n->setRemove(i, mx, my, mz);

      } else {

        int mx, my, mz;

        cache->getDirection(nd >> 1, &mx, &my, &mz);

        mx *= movement[i];
        my *= movement[i];
        mz *= movement[i];

        if (nd & 1) {
          mx = -mx;
          my = -my;
          mz = -mz;
        }

        n->set(i, mx, my, mz);
      }

    } else {

      n->set(i, 0, 0, 0);

    }
  }

  return n;
}

/* creates a new node that contains the merged movements of the given 2 nodes
 * merged movement means that a piece is moved the maximum amount specified in
 * both nodes. But only one direction is allowed, so if one piece moves this
 * way and another piece that way 0 is returned
 * the function also returns zero, if the new node would be identical to n1 or n0
 * also the amount must be identical in both nodes, so if piece a moves 1 unit
 * in node n0 and another piece move 2 units in node n1 0 is returned
 */
disassemblerNode_c * movementAnalysator_c::newNodeMerge(const disassemblerNode_c *n0, const disassemblerNode_c *n1) {

  // assert that direction are along the same axis
  bt_assert((nextdir | 1) == (n0->getDirection() | 1));
  bt_assert((nextdir | 1) == (n1->getDirection() | 1));

  bool invert0 = (nextdir != n0->getDirection());
  bool invert1 = (nextdir != n1->getDirection());

  // both nodes need to have the same movement amount, if not return 0
  int amount = n0->getAmount();
  if (amount != n1->getAmount()) return 0;

  /* we need to make sure the new node is different from n0 and n1
   */
  bool different0 = false;
  bool different1 = false;
  int moved = 0;
  bool move0, move1;

  for (int i = 0; i < next_pn; i++) {

    // calculate the movement of the merged node by first finding out if the
    // piece has been moved within one node
    move0 = ((n0->getX(i) != searchnode->getX(i)) ||
             (n0->getY(i) != searchnode->getY(i)) ||
             (n0->getZ(i) != searchnode->getZ(i))) ^ invert0;
    move1 = ((n1->getX(i) != searchnode->getX(i)) ||
             (n1->getY(i) != searchnode->getY(i)) ||
             (n1->getZ(i) != searchnode->getZ(i))) ^ invert1;

    // and if it has been moved in one of them, it needs
    // to be moved in the new node
    if (move0 || move1) {
      movement[i] = amount;
      moved++;
    } else
      movement[i] = 0;

    // the new node differs from the old one if there was a movement cause by the other node which
    // was not available in the first one
    different0 |= (move1 && !move0);
    different1 |= (move0 && !move1);
  }

  // if no or all pieces are moved, exit, this created degenerated nodes
  if (moved == 0 || moved == next_pn) return 0;

  // if the new node is equal to n0 or n1, exit
  if (!different0 || !different1) return 0;

  return newNode(amount);
}


void movementAnalysator_c::init_find(disassemblerNode_c * nd, const std::vector<unsigned int> & pcs) {

  /* Initialise the state machine for the find routine
   */
  nextdir = 0;
  nextpiece = 0;
  nextstep = 1;
  nextstate = 0;
  next_pn = pcs.size();

  searchnode = nd;
  pieces = &pcs;

  clearRotations();

  /* when a new search has been started we need to first calculate
   * the movement matrices, this is a table that contains one 2 dimensional
   * matrix for each of the 6 directions where movement is possible
   *
   * the matrices contains possible movement of one piece if other pieces
   * are not moved. So a one in column 2 row 4 means that piece nr. 2 can
   * be moved one unit it we fix piece nr. 4
   *
   * the algorithm used here is describes in Bill Cutlers booklet
   * "Computer Analysis of All 6 Piece Burrs"
   */
  prepare();
}

/* at first we check if movement is possible at all in the current direction, if so
 * the next thing to do is to check if something can be removed, and finally we look for longer
 * movements in the actual direction
 */
disassemblerNode_c * movementAnalysator_c::find(void) {

  disassemblerNode_c * n = 0;

  // repeat until we either find a movement or have checked everything
  while (!n) {

    switch (nextstate) {
      case 0:
        // check, if a single piece can be removed
        if (checkmovement(1, 30000))
          n = newNode(30000);

        nextpiece++;
        if (nextpiece >= next_pn) {
          nextpiece = 0;
          nextdir++;
          if (nextdir >= 2*cache->numDirections()) {
            nextstate++;
            nextdir = 0;
          }
        }
        break;
      case 1:
        // check, if a group of pieces can be removed
        if (checkmovement(next_pn/2, 30000))
          n = newNode(30000);

        nextpiece++;
        if (nextpiece >= next_pn) {
          nextpiece = 0;
          nextdir++;
          if (nextdir >= 2*cache->numDirections()) {
            nextstate++;
            nextdir = 0;
            nodes->clear();
          }
        }
        break;
      case 2:
        // check, if pieces can be moved
        if ((nextstep <= maxstep) && checkmovement(next_pn/2, nextstep)) {
          n = newNode(nextstep);
          bt_assert(n);

          // we need to merge the gained node with all already found
          // nodes with the same step and if that leads to valid new nodes
          // we also need to return those

          // but first we check, if we have this node already found (maybe via a merger)
          // and if so we delete it
          if (nodes->insert(n)) {
            delete n;
            n = 0;

          } else {

            nextstate = 99;
            state99node = n;
            nodes->initScan();
            state99nextState = 2;
          }

          // if we can move something, we try larger steps
          nextstep++;

        } else {

          // if not, lets try the next piece
          nextstep = 1;
          nextpiece++;
          if (nextpiece >= next_pn) {
            nextpiece = 0;
            nextdir++;
            nodes->clear();
            if (nextdir >= 2*cache->numDirections()) {
              nextstate++;
            }
          }
        }
        break;

      case 99:

        // this is a special state that takes the last found node and creates mergers with all
        // the already found nodes.
        // a merger is a new node that contains the movement of one node AND the movement of
        // the 2nd node at the same time. Of course both nodes need to point into the same
        // direction and in both nodes the pieces need to be moved by
        // the same amount
        //
        // This is needed because when moving groups of pieces and both pieces are independent of
        // one another the code above alone wont find movements where both pieces are moved at
        // the same time but rather one after the other

        {
          const disassemblerNode_c * nd2 = nodes->nextScan();

          if (nd2) {
            n = newNodeMerge(state99node, nd2);

            // if the node is valid check if we already know that node, if so
            // delete it
            if (n && nodes->insert(n)) {
              delete n;
              n = 0;
            }

          } else
            nextstate = state99nextState;
        }

        break;

      case 3:
        // rotation moves, only when rotation tables were given; without
        // them this is the end of the search as it always was
        if (!rotations)
          return 0;

        if (!rotationsListed) {
          listRotations();
          rotationsListed = true;
        }

        if (nextRotation < rotationNodes.size()) {
          n = rotationNodes[nextRotation];
          rotationNodes[nextRotation++] = nullptr;
        } else
          nextstate++;

        break;

      default:
        // endstate, do nothing
        return 0;
    }
  }

  stats.nodesReturned++;
  return n;
}

/*
  This is a cut-down version of find that is only intended to find the first possible move, and it must
  involve the indicated piece and be in the indicated direction
  and we'll do the preparation call from within this routine too, so we only have to call one routine from outside
*/
disassemblerNode_c * movementAnalysator_c::findMatching(disassemblerNode_c * nd, const std::vector<unsigned int> & pcs, unsigned int piece, int dx, int dy, int dz) {

  /* note that a language with lightweight threads and pipes could have done the find method as a generator instead of a state machine,
  and then we wouldn't be passing arguments around in these instance variables */


  // the direction is already determined, but we need to calculate it from the coordinates I guess
  {
    // calculate the direction from (dx,dy,dz)
    // we need to use getDirection in reverse
    int numDirs = cache->numDirections();
    int dx0, dy0, dz0;
    int dirIdx;
    for (dirIdx = 0; dirIdx < numDirs; dirIdx++) {
      cache->getDirection(dirIdx, &dx0, &dy0, &dz0);
      if ((dx0 == dx) && (dy0 == dy) && (dz0 == dz)) {
        nextdir = dirIdx << 1;
        break;
      }
      if ((dx0 == -dx) && (dy0 == -dy) && (dz0 == -dz)) {
        nextdir = (dirIdx << 1) | 1;
        break;
      }
    }

    // if there's no matching direction, we can't find any moves
    // note that presently, this may actually happen with odd grid types
    if (!(dirIdx < numDirs)) return 0;  // failed to find a matching direction
  }

  // use the piece id mapping array to determine the correct piece id to use in the disassembler
  // but note that it's the inverse of the mapping we need, so we apply it backwards with a loop
  // this routine is presently only called once in response to a user action, so this is fine
  {
    unsigned int i = 0;
    while (i < pcs.size())
    {
      if (pcs[i] == piece) break;
      i++;
    }
    bt_assert(i < pcs.size());  // failed to find a matching piece id
    nextpiece = i;
  }

  nextstep = 1;           // always 1 for this operation
  nextstate = 2;          // doesn't matter since we're not a state machine
  next_pn = pcs.size();

  searchnode = nd;
  pieces = &pcs;

  // calculate the movement matrices
  prepare();

  if (checkmovement(next_pn/2, nextstep)) {
    // we found a move
    disassemblerNode_c * n = newNode(nextstep);
    bt_assert(n);
    return n;
  }

  // indicate no valid move
  return 0;
}

void movementAnalysator_c::completeFind(disassemblerNode_c * searchnode, const std::vector<unsigned int> & pieces, std::vector<disassemblerNode_c*> * result) {

  /* Same per-run kernel resolution as disassemble(): this is the other
   * run entry point (used e.g. by the GUI movement browser). */
  selectKernels();

  init_find(searchnode, pieces);

  for (unsigned int i = 0; i < result->size(); i++)
    delete (*result)[i];
  result->clear();

  disassemblerNode_c * nd;

  maxstep = 1;

  std::vector<disassemblerNode_c*> toremove;

  while ((nd = find()) != 0) {
    for (unsigned int i = 0; i < result->size(); i++) {
      if (*(*result)[i] == *nd) {
        toremove.push_back(nd);
        nd = 0;
        break;
      }
    }

    if (nd)
      result->push_back(nd);
  }

  maxstep = (unsigned int)-1;

  for (unsigned int i = 0; i < toremove.size(); i++)
    delete toremove[i];
}


/* ---------------------------------------------------------------------------
 * Rotation moves (design/2026-10-19-rotation-moves.md)
 * ------------------------------------------------------------------------ */

const std::vector<rotationGeometry::cell_t> &
movementAnalysator_c::cellsAroundHotspot(unsigned int part, unsigned int trans) {

  bt_assert(part < hotspotCells.size());

  std::vector<std::vector<rotationGeometry::cell_t>> & byTrans = hotspotCells[part];
  if (byTrans.size() <= trans)
    byTrans.resize(trans + 1);

  /* a piece has at least one cube, so empty means not computed yet */
  std::vector<rotationGeometry::cell_t> & cells = byTrans[trans];
  if (cells.empty()) {
    std::unique_ptr<voxel_c> v(rotationProblem->getPuzzle().getGridType()->getVoxel(*partVoxels[part]));
    bt_assert2(v->transform(trans));
    for (unsigned int z = 0; z < v->getZ(); z++)
      for (unsigned int y = 0; y < v->getY(); y++)
        for (unsigned int x = 0; x < v->getX(); x++)
          if (v->isFilled(x, y, z))
            cells.push_back({ (int)x - v->getHx(), (int)y - v->getHy(), (int)z - v->getHz() });
  }

  return cells;
}

void movementAnalysator_c::clearRotations(void) {
  for (disassemblerNode_c * n : rotationNodes)
    if (n && n->decRefCount())
      delete n;
  rotationNodes.clear();
  nextRotation = 0;
  rotationsListed = false;
}

namespace {

/* The cells of all pieces of a search node and which piece owns which cell,
 * as a dense map over their bounding box. */
class ownerMap_c {

  public:

    ownerMap_c(const std::vector<std::vector<rotationGeometry::cell_t>> & world) {
      bool first = true;
      for (const auto & cells : world)
        for (const rotationGeometry::cell_t & c : cells) {
          if (first) {
            x0 = x1 = c.x; y0 = y1 = c.y; z0 = z1 = c.z;
            first = false;
          }
          x0 = std::min(x0, c.x); x1 = std::max(x1, c.x);
          y0 = std::min(y0, c.y); y1 = std::max(y1, c.y);
          z0 = std::min(z0, c.z); z1 = std::max(z1, c.z);
        }
      sx = x1 - x0 + 1;
      sy = y1 - y0 + 1;
      owner.assign((size_t)sx * sy * (z1 - z0 + 1), -1);
      for (size_t i = 0; i < world.size(); i++)
        for (const rotationGeometry::cell_t & c : world[i])
          owner[((size_t)(c.z - z0) * sy + (c.y - y0)) * sx + (c.x - x0)] = (int8_t)i;
    }

    /** the piece at the cell, -1 for none */
    int operator()(int x, int y, int z) const {
      if (x < x0 || y < y0 || z < z0 || x > x1 || y > y1 || z > z1)
        return -1;
      return owner[((size_t)(z - z0) * sy + (y - y0)) * sx + (x - x0)];
    }

  private:

    int x0 = 0, y0 = 0, z0 = 0, x1 = 0, y1 = 0, z1 = 0, sx = 0, sy = 0;
    std::vector<int8_t> owner;
};

}

/* The pivots at which piece j blocks piece i on every path, relative to i's
 * hotspot. Blocking is pairwise between cubes: a static cube s blocks a
 * moving cube m at pivot P iff s - P is in blockedOnAllPaths(m - P), i.e.
 * iff m - P is in pivotsBlockedAt(s - m). So the set is the union, over the
 * cube pairs in a common layer, of m - pivotsBlockedAt(s - m). */
const movementAnalysator_c::pivotSet_t *
movementAnalysator_c::blockingPivots(unsigned int i, unsigned int j, unsigned int axis, unsigned int sense) {

  const unsigned int partI = piecePart[(*pieces)[i]], partJ = piecePart[(*pieces)[j]];
  const unsigned int transI = searchnode->getTrans(i), transJ = searchnode->getTrans(j);
  const int dx = searchnode->getX(j) - searchnode->getX(i);
  const int dy = searchnode->getY(j) - searchnode->getY(i);
  const int dz = searchnode->getZ(j) - searchnode->getZ(i);

  /* parts get 24 bits each, orientations 8, offsets 16 */
  const pairKey_t key = {
    ((uint64_t)partI << 40) | ((uint64_t)partJ << 16) | (transI << 8) | transJ,
    ((uint64_t)(uint16_t)dx << 32) | ((uint64_t)(uint16_t)dy << 16) | (uint16_t)dz |
      ((uint64_t)axis << 48) | ((uint64_t)sense << 50)
  };

  auto found = pairPivots.find(key);
  if (found != pairPivots.end())
    return found->second.get();

  /* a cache, so keep it bounded */
  if (pairPivots.size() >= MAX_PAIR_CACHE)
    pairPivots.clear();

  const planeCells_t & pi = planeCells(partI, transI, axis);
  const planeCells_t & pj = planeCells(partJ, transJ, axis);
  const int reach = 2 * rotations->getRadius();

  /* j's cubes are offset by the hotspots' difference */
  int du, dv;
  rotationGeometry::inPlaneDoubled(dx, dy, dz, axis, &du, &dv);
  const int da = rotationGeometry::axialCoordinate(dx, dy, dz, axis);

  /* Only pivots within the tables' reach of every cube of i matter: at any
   * other pivot i cannot turn at all. A piece wider than that never turns. */
  const int qu0 = pi.umax - reach, qu1 = pi.umin + reach;
  const int qv0 = pi.vmax - reach, qv1 = pi.vmin + reach;
  if (qu0 > qu1 || qv0 > qv1)
    return (pairPivots[key] = nullptr).get();

  std::unique_ptr<pivotSet_t> set;

  /* pair up the cubes layer by layer; both lists are sorted by layer */
  auto mi = pi.cubes.begin(), mj = pj.cubes.begin();
  while (mi != pi.cubes.end() && mj != pj.cubes.end()) {
    if (mi->layer < mj->layer + da) { ++mi; continue; }
    if (mj->layer + da < mi->layer) { ++mj; continue; }

    const int layer = mi->layer;
    auto ei = mi, ej = mj;
    while (ei != pi.cubes.end() && ei->layer == layer) ++ei;
    while (ej != pj.cubes.end() && ej->layer + da == layer) ++ej;

    for (auto m = mi; m != ei; ++m)
      for (auto c = mj; c != ej; ++c)
        for (const auto & [ou, ov] : rotations->pivotsBlockedAt(sense, c->u + du - m->u, c->v + dv - m->v)) {
          const int qu = m->u - ou, qv = m->v - ov;
          if (qu < qu0 || qu > qu1 || qv < qv0 || qv > qv1)
            continue;
          if (!set) {
            set = std::make_unique<pivotSet_t>();
            set->u0 = qu0;
            set->v0 = qv0;
            set->width = qu1 - qu0 + 1;
            set->height = qv1 - qv0 + 1;
            set->bits.assign(((size_t)set->width * set->height + 63) / 64, 0);
          }
          const size_t b = (size_t)(qu - qu0) * set->height + (qv - qv0);
          set->bits[b >> 6] |= uint64_t(1) << (b & 63);
        }

    mi = ei;
    mj = ej;
  }

  return (pairPivots[key] = std::move(set)).get();
}

const movementAnalysator_c::planeCells_t &
movementAnalysator_c::planeCells(unsigned int part, unsigned int trans, unsigned int axis) {

  std::vector<planeCells_t> & byTrans = planeCellCache[part];
  if (byTrans.size() <= 3 * trans + axis)
    byTrans.resize(3 * trans + 3);

  planeCells_t & pc = byTrans[3 * trans + axis];
  if (pc.cubes.empty()) {
    pc.umin = pc.vmin = INT_MAX;
    pc.umax = pc.vmax = INT_MIN;
    for (const rotationGeometry::cell_t & c : cellsAroundHotspot(part, trans)) {
      planeCells_t::cube_t q;
      q.layer = rotationGeometry::axialCoordinate(c.x, c.y, c.z, axis);
      rotationGeometry::inPlaneDoubled(c.x, c.y, c.z, axis, &q.u, &q.v);
      pc.umin = std::min(pc.umin, q.u); pc.umax = std::max(pc.umax, q.u);
      pc.vmin = std::min(pc.vmin, q.v); pc.vmax = std::max(pc.vmax, q.v);
      pc.cubes.push_back(q);
    }
    std::stable_sort(pc.cubes.begin(), pc.cubes.end());
  }

  return pc;
}

namespace {

/* The groups that may turn, given for every piece the pieces that block it
 * on every path: unions of the closures of single pieces under "blocked
 * by", leaving out every closure that needs piece 0 (never turns) or a
 * piece outside covered. With very many distinct closures the pieces are
 * loose and only the single closures are returned. */
std::vector<uint32_t> closedGroups(const std::vector<uint32_t> & blockers, uint32_t covered, unsigned int pn) {

  const uint32_t forbidden = ~covered;
  std::vector<uint32_t> seeds;
  for (unsigned int i = 1; i < pn; i++) {
    if (!(covered & (1u << i)))
      continue;
    uint32_t c = 1u << i, todo = c;
    while (todo && !(c & forbidden)) {
      unsigned int j = std::countr_zero(todo);
      todo &= todo - 1;
      uint32_t add = blockers[j] & ~c;
      c |= add;
      todo |= add;
    }
    if (!(c & forbidden))
      seeds.push_back(c);
  }
  std::sort(seeds.begin(), seeds.end());
  seeds.erase(std::unique(seeds.begin(), seeds.end()), seeds.end());

  if (seeds.size() > 16)
    return seeds;

  std::vector<uint32_t> groups;
  for (uint32_t sel = 1; sel < (1u << seeds.size()); sel++) {
    uint32_t group = 0;
    for (size_t b = 0; b < seeds.size(); b++)
      if (sel & (1u << b))
        group |= seeds[b];
    groups.push_back(group);
  }
  std::sort(groups.begin(), groups.end());
  groups.erase(std::unique(groups.begin(), groups.end()), groups.end());
  return groups;
}

}

/* The node reached by turning the pieces of group about the doubled
 * in-plane pivot (pu, pv) along path number path of the family. */
void movementAnalysator_c::addRotationNode(uint32_t group, unsigned int axis, unsigned int sense,
                                           int pu, int pv, int path) {

  const symmetries_c * sym = rotationProblem->getPuzzle().getGridType()->getSymmetries();
  const unsigned int pn = pieces->size();
  int hx, hy, hz;
  rotationGeometry::pivotOf(axis, pu, pv, &hx, &hy, &hz);

  disassemblerNode_c * n = new disassemblerNode_c(pn, searchnode,
      disassemblerNode_c::ROTATION_DIR_BASE + 2 * axis + sense, path);
  for (unsigned int i = 0; i < pn; i++) {
    int x = searchnode->getX(i), y = searchnode->getY(i), z = searchnode->getZ(i);
    unsigned char t = (unsigned char)searchnode->getTrans(i);
    if (group & (1u << i)) {
      rotationGeometry::turnAbout(&x, &y, &z, hx, hy, hz, axis, sense);
      t = sym->transAdd(t, rotationGeometry::quarterTurnTrans(axis, sense));
      /* the brick grid has all 24 orientations, so a turn always has one */
      bt_assert(t != TND);
    }
    n->set(i, x, y, z, t);
  }
  n->setRotationPivot(pu, pv);
  rotationNodes.push_back(n);
}

/* Generate every rotation move of the search node.
 *
 * Groups: piece 0 never turns (turning a group with it is the same as
 * turning the rest the other way, and keeping it fixed keeps the states
 * canonical). Blocking is pairwise -- along a fixed path every cube pair
 * collides or not on its own -- so, like Bill Cutler's movement matrices
 * for translations, for each axis, pivot and sense we know which pieces
 * block which on every path of the family (this includes landing on
 * another piece; see blockingPivots). A group can only turn if it contains
 * every piece that blocks one of its members, i.e. if it is closed under
 * that relation, and every closed group is a union of closures of single
 * pieces. So only unions of closures are checked path by path against the
 * tables. That yields exactly the moves of an exhaustive enumeration: a
 * group that can turn on some path is closed under "blocks on every path".
 *
 * Pivots: cube centres and cube corners within the turning group's
 * footprint; edge midpoints cannot map the lattice onto itself. */
void movementAnalysator_c::listRotations(void) {

  using rotationGeometry::cell_t;

  const unsigned int pn = pieces->size();

  if (pn < 2 || pn > MAX_ROTATION_PIECES)
    return;

  const int reach = 2 * rotations->getRadius();

  std::vector<std::vector<cell_t>> world(pn);
  for (unsigned int i = 0; i < pn; i++)
    for (const cell_t & c : cellsAroundHotspot(piecePart[(*pieces)[i]], searchnode->getTrans(i)))
      world[i].push_back({ searchnode->getX(i) + c.x, searchnode->getY(i) + c.y, searchnode->getZ(i) + c.z });
  const ownerMap_c ownerAt(world);

  std::vector<cell_t> moving, stat;
  std::vector<unsigned int> owners;
  std::vector<uint32_t> blockers(pn);
  std::vector<const pivotSet_t *> pairs(pn * pn);

  for (unsigned int axis = 0; axis < 3; axis++) {

    /* in-plane doubled footprint and hotspot of every piece */
    std::vector<int> umin(pn, INT_MAX), umax(pn, INT_MIN), vmin(pn, INT_MAX), vmax(pn, INT_MIN), hu(pn), hv(pn);
    for (unsigned int i = 0; i < pn; i++) {
      rotationGeometry::inPlaneDoubled(searchnode->getX(i), searchnode->getY(i), searchnode->getZ(i), axis, &hu[i], &hv[i]);
      for (const cell_t & c : world[i]) {
        int u, v;
        rotationGeometry::inPlaneDoubled(c.x, c.y, c.z, axis, &u, &v);
        umin[i] = std::min(umin[i], u); umax[i] = std::max(umax[i], u);
        vmin[i] = std::min(vmin[i], v); vmax[i] = std::max(vmax[i], v);
      }
    }
    /* pivots: within the footprint of the pieces that may turn */
    const int pumin = *std::min_element(umin.begin() + 1, umin.end()) - 1;
    const int pumax = *std::max_element(umax.begin() + 1, umax.end()) + 1;
    const int pvmin = *std::min_element(vmin.begin() + 1, vmin.end()) - 1;
    const int pvmax = *std::max_element(vmax.begin() + 1, vmax.end()) + 1;

    for (unsigned int sense = 0; sense < 2; sense++) {

      for (unsigned int i = 1; i < pn; i++)
        for (unsigned int j = 0; j < pn; j++)
          pairs[i * pn + j] = (i == j) ? nullptr : blockingPivots(i, j, axis, sense);

      for (int pu = pumin; pu <= pumax; pu++)
        for (int pv = pvmin; pv <= pvmax; pv++) {

          /* edge-midpoint pivots cannot map the lattice onto itself */
          if ((pu ^ pv) & 1)
            continue;

          /* pieces with cubes beyond the tables' radius cannot turn here:
           * skip, never guess */
          uint32_t covered = 0;
          for (unsigned int i = 1; i < pn; i++)
            if (umax[i] - pu <= reach && pu - umin[i] <= reach && vmax[i] - pv <= reach && pv - vmin[i] <= reach)
              covered |= 1u << i;
          if (!covered)
            continue;

          for (unsigned int i = 1; i < pn; i++) {
            blockers[i] = 0;
            for (unsigned int j = 0; j < pn; j++)
              if (pairs[i * pn + j] && pairs[i * pn + j]->test(pu - hu[i], pv - hv[i]))
                blockers[i] |= 1u << j;
          }

          const std::vector<uint32_t> groups = closedGroups(blockers, covered, pn);
          if (groups.empty())
            continue;

          int hx, hy, hz;
          rotationGeometry::pivotOf(axis, pu, pv, &hx, &hy, &hz);

          for (uint32_t group : groups) {

            /* the pivot lies in the group's footprint */
            int gumin = INT_MAX, gumax = INT_MIN, gvmin = INT_MAX, gvmax = INT_MIN;
            for (unsigned int i = 0; i < pn; i++)
              if (group & (1u << i)) {
                gumin = std::min(gumin, umin[i]); gumax = std::max(gumax, umax[i]);
                gvmin = std::min(gvmin, vmin[i]); gvmax = std::max(gvmax, vmax[i]);
              }
            if (pu < gumin - 1 || pu > gumax + 1 || pv < gvmin - 1 || pv > gvmax + 1)
              continue;

            moving.clear();
            owners.clear();
            stat.clear();
            for (unsigned int i = 0; i < pn; i++)
              for (const cell_t & c : world[i])
                if (group & (1u << i)) {
                  moving.push_back(c);
                  owners.push_back(i);
                } else
                  stat.push_back(c);

            /* the end pose is free by closure; it must change something */
            bool unchanged = true;
            for (size_t m = 0; m < moving.size() && unchanged; m++) {
              cell_t e = moving[m];
              rotationGeometry::turnAbout(&e.x, &e.y, &e.z, hx, hy, hz, axis, sense);
              unchanged = ownerAt(e.x, e.y, e.z) == (int)owners[m];
            }
            if (unchanged)
              continue;

            int path = rotations->firstClear(moving, rotationGeometry::occupancy_c(stat), axis, hx, hy, hz, sense);
            if (path < 0)
              continue;

            addRotationNode(group, axis, sense, pu, pv, path);
          }
        }
    }
  }
}
