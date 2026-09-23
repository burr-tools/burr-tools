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

void movementAnalysator_c::closureFull(void) {

  const unsigned int n = pieces->size();
  const unsigned int dirs = cache->numDirections();
  const unsigned int rowStep = dirs * piecenumber;

  if (planar_block.size() < (size_t)n * n) {
    planar_block.resize((size_t)n * n);
  }

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  const bool has_avx512 = __builtin_cpu_supports("avx512f") && !simdDisabled() && SimdConfig::isAvx512Allowed();
  const bool has_avx2 = __builtin_cpu_supports("avx2") && !simdDisabled();
#endif

  /* Roy-Floyd-Warshall all-pairs shortest paths on movement constraints.
   * Planar memory layout enables contiguous vector loads/stores/mins. */
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
    if (has_avx512 && n >= 16) {
      rfw_avx512(planar_block.data(), n);
    } else if (has_avx2) {
      rfw_avx2(planar_block.data(), n);
    } else {
      rfw_scalar(planar_block.data(), n);
    }
#elif defined(__aarch64__) || defined(__ARM_NEON)
    if (!simdDisabled()) {
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

movementAnalysator_c::movementAnalysator_c(const problem_c & problem) :
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
  maxstep((unsigned int) -1) {

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
}

movementAnalysator_c::~movementAnalysator_c() {
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

