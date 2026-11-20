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
#include "assembler_1.h"
#include "cells_cover.h"
#include "assembler_pool.h"
#include "simd_huang_cover.h"

#include "bt_assert.h"
#include "problem.h"
#include "puzzle.h"
#include "voxel.h"
#include "assembly.h"
#include "gridtype.h"

#include "../tools/xml.h"

#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#define snprintf _snprintf
#endif

/* Definitions for huang_memory::tierRowBytes/fitsMemoryBudget (declared in
 * simd_huang_cover.h). They live here rather than in the header because
 * naming every tier's Row type from that header breaks GCC's
 * target-attribute handling for the AVX kernels. Thresholds mirror
 * createSimdSolver() and tierMaskBytes(): keep all three in sync
 * (tierMaskBytes() itself has no production caller left -- it exists so
 * the unit test pins the ladder -- but its thresholds must still match). */
size_t huang_memory::tierRowBytes(unsigned int num_cols) {
  if (num_cols <= 256) return sizeof(SimdHuangCover256::Row);
  if (num_cols <= 512) return sizeof(SimdHuangCover512::Row);
  if (num_cols <= 1024) return sizeof(SimdHuangCover1024::Row);
  if (num_cols <= 2048) return sizeof(SimdHuangCover2048::Row);
  if (num_cols <= 4096) return sizeof(SimdHuangCover4096::Row);
  if (num_cols <= 8192) return sizeof(SimdHuangCover8192::Row);
  if (num_cols <= 16384) return sizeof(SimdHuangCover16384::Row);
  return sizeof(SimdHuangCover32768::Row);
}

bool huang_memory::fitsMemoryBudget(unsigned int num_cols, uint64_t num_rows, uint64_t num_nodes, uint64_t budget) {
  return num_rows * tierRowBytes(num_cols) + num_nodes * kPerNodeOverheadBytes <= budget;
}

#define ASSEMBLER_VERSION "2.2"

void printMatrix(
    const std::vector<unsigned int> & up,
    const std::vector<unsigned int> & down,
    const std::vector<unsigned int> & left,
    const std::vector<unsigned int> & right,
    const std::vector<unsigned int> & colCount,
    const std::vector<unsigned int> & weight,
    const std::vector<unsigned int> & min,
    const std::vector<unsigned int> & max
  ) {

  // check consistency of matrix
  {

    unsigned long cnt = 0;

    unsigned int c = right[0];
    while (c) {

      if (left[right[c]] != c) printf("lr %u\n", c);
      if (right[left[c]] != c) printf("rl %u\n", c);
      if (up[down[c]] != c) printf("ud %u\n", c);
      if (down[up[c]] != c) printf("du %u\n", c);
      cnt++;

      unsigned int r = down[c];

      while (r != c) {

        if (left[right[r]] != r) printf("lr %u\n", r);
        if (right[left[r]] != r) printf("rl %u\n", r);
        if (up[down[r]] != r) printf("ud %u\n", r);
        if (down[up[r]] != r) printf("du %u\n", r);
        cnt++;

        r = down[r];

      }

      c = right[c];
    }

    printf("checked %lu nodes for consistency\n", cnt);
  }

  /* first find all the columns */
  std::vector<unsigned int> columns;

  unsigned int c = right[0];

  while (c) {
    columns.push_back(c);
    c = right[c];

    printf("n");
  }

  printf("\n");

  for (unsigned int i = 0; i < columns.size(); i++) if (columns[i] > 100) printf("%u", columns[i] / 100); else printf(" ");
  printf("\n");
  for (unsigned int i = 0; i < columns.size(); i++) if (columns[i] > 10)  printf("%u", (columns[i] / 10)%10); else printf(" ");
  printf("\n");
  for (unsigned int i = 0; i < columns.size(); i++) printf("%u", columns[i] % 10);
  printf("\n");

  for (unsigned int i = 0; i < columns.size(); i++) printf("%u", min[columns[i]]);
  printf("\n");
  for (unsigned int i = 0; i < columns.size(); i++) printf("%u", max[columns[i]]);
  printf("\n");

  std::vector<unsigned int> rows;

  for (unsigned int i = 0; i < columns.size(); i++)
    rows.push_back(down[columns[i]]);

  /* all nodes are now in the first row below the header */
  while (true) {

    /* find the smallest real node ( must be > varivoxelEnd */
    int r = -1;

    for (unsigned int i = 0; i < rows.size(); i++)
      if (rows[i] > left[0])
        if (r == -1 || rows[i] < rows[r])
          r = i;

    if (r == -1) break;

    unsigned int nodeS = rows[r];
    unsigned int node = nodeS;

    std::vector<int>matrixline;

    matrixline.resize(rows.size());

    do {

      unsigned int col = colCount[node];

      for (unsigned int i = 0; i < columns.size(); i++) {
        if (columns[i] == col) {
          col = i;
          break;
        }
      }

      matrixline[col] = weight[node];

      node = right[node];

      rows[col] = down[rows[col]];

    } while (node != nodeS);

    for (unsigned int i = 0; i < matrixline.size(); i++)
      printf("%i", matrixline[i]);

    printf("\n");

  }
}

void assembler_1_c::GenerateFirstRow(unsigned int columns) {

  for (unsigned int i = 0; i < columns+1; i++) {
    right.push_back(i+1);
    left.push_back(i-1);
    up.push_back(i);
    down.push_back(i);
    colCount.push_back(0);
    min.push_back(1);
    max.push_back(1);
    weight.push_back(0);
  }

  /* make the linked list cyclic */
  left[0] = columns;
  right[columns] = 0;

  headerNodes = columns + 1;
}

int assembler_1_c::AddPieceNode(unsigned int piece, unsigned int rot, unsigned int x, unsigned int y, unsigned int z) {
  unsigned long piecenode = left.size();

  left.push_back(piecenode);
  right.push_back(piecenode);
  up.push_back(up[piece+1]);
  down.push_back(piece+1);
  weight.push_back(1);

  down[up[piece+1]] = piecenode;
  up[piece+1] = piecenode;

  colCount.push_back(piece+1);
  colCount[piece+1]++;

  piecePositions.push_back(piecePosition(piece, x, y, z, rot, piecenode));

  return piecenode;
}

void assembler_1_c::buildNodePositions(void) {
  nodePositions.assign(left.size(), UINT32_MAX);
  for (size_t k = 0; k < piecePositions.size(); k++) {
    const size_t end = k + 1 < piecePositions.size() ? piecePositions[k+1].row : left.size();
    for (size_t n = piecePositions[k].row; n < end; n++)
      nodePositions[n] = (uint32_t)k;
  }
}

void assembler_1_c::getPieceInformation(unsigned int node, unsigned int * piece, unsigned char *tran, int *x, int *y, int *z) const {

  if (node < nodePositions.size() && nodePositions[node] != UINT32_MAX) {
    const piecePosition &pp = piecePositions[nodePositions[node]];
    *tran = pp.transformation;
    *x = pp.x;
    *y = pp.y;
    *z = pp.z;
    *piece = pp.piece;
    return;
  }

  // piecePositions is sorted by row: AddPieceNode appends rows in strictly
  // increasing node order (piecenode == left.size() at creation), so binary
  // search finds the last entry with row <= node in O(log n) per call.
  // The not-found path throws unconditionally (bt_te, not bt_assert or an
  // early return): a node that maps to no placement is a corrupt matrix or
  // seed, and must not turn into a wrong assembly in release builds.
  unsigned int lo = 0, hi = static_cast<unsigned int>(piecePositions.size());
  while (lo < hi) {
    unsigned int mid = lo + (hi - lo) / 2;
    if (piecePositions[mid].row <= node)
      lo = mid + 1;
    else
      hi = mid;
  }

  if (lo == 0)
    bt_te("getPieceInformation: no piece position for node");

  const piecePosition &pp = piecePositions[lo - 1];
  *tran = pp.transformation;
  *x = pp.x;
  *y = pp.y;
  *z = pp.z;
  *piece = pp.piece;
}

void assembler_1_c::AddVoxelNode(unsigned int col, unsigned int piecenode) {

  bt_assert(left.size() == right.size() &&
      left.size() == up.size() &&
      left.size() == down.size() &&
      left.size() == colCount.size() &&
      left.size() == weight.size());

  unsigned long newnode = left.size();

  right.push_back(piecenode);
  left.push_back(left[piecenode]);
  right[left[piecenode]] = newnode;
  left[piecenode] = newnode;

  up.push_back(up[col]);
  down.push_back(col);
  down[up[col]] = newnode;
  up[col] = newnode;

  colCount.push_back(col);

  weight.push_back(1);
  colCount[col]++;
}

void assembler_1_c::AddRangeNode(unsigned int col, unsigned int piecenode, unsigned int wg) {

  unsigned long newnode = left.size();

  right.push_back(piecenode);
  left.push_back(left[piecenode]);
  right[left[piecenode]] = newnode;
  left[piecenode] = newnode;

  up.push_back(up[col]);
  down.push_back(col);
  down[up[col]] = newnode;
  up[col] = newnode;

  colCount.push_back(col);

  weight.push_back(wg);
  colCount[col] += wg;
}

assembler_1_c::assembler_1_c(const problem_c & prob, const solverSettings_t & s) :
  assembler_c(s),
  problem(prob),
  avoidTransformedAssemblies(false), avoidTransformedMirror(nullptr),
  iterations(0),
  reducePiece(0)
{
}

assembler_1_c::~assembler_1_c() = default;

/* add a piece to the cache, but only if it is not already there. If it is added return the
 * piece pointer otherwise return null
 */
static voxel_c * addToCache(voxel_c * cache[], unsigned int * fill, voxel_c * piece) {

  for (unsigned int i = 0; i < *fill; i++)
    if (cache[i]->identicalInBB(piece)) {
      delete piece;
      return 0;
    }

  cache[*fill] = piece;
  (*fill)++;
  return piece;
}

bool assembler_1_c::canPlace(const voxel_c * piece, int x, int y, int z) const {

  if (!piece->onGrid(x, y, z))
    return false;

  const voxel_c * result = getResultShape(problem);

  for (unsigned int pz = piece->boundZ1(); pz <= piece->boundZ2(); pz++)
    for (unsigned int py = piece->boundY1(); py <= piece->boundY2(); py++)
      for (unsigned int px = piece->boundX1(); px <= piece->boundX2(); px++)
        if (
            // the piece can not be place if the result is empty and the piece is filled at a given voxel
            ((piece->getState(px, py, pz) == voxel_c::VX_FILLED) &&
             (result->getState(x+px, y+py, z+pz) == voxel_c::VX_EMPTY)) ||

            // the piece can also not be placed when the colour constraints don't fit
            !problem.placementAllowed(piece->getColor(px, py, pz), result->getColor(x+px, y+py, z+pz))

           )
          return false;

  return true;
}

/**
 * this function prepares the matrix of nodes for the recursive function
 * I've done some additions to Knuths algorithm to implement variable
 * voxels (empty spaces in the solution) and multiple instances of the
 * same piece. Empty voxels in the result are done by removing columns
 * from the matrix. This will prevent the algorithm from filling the
 * corresponding voxels. But we need to have the constraints that these
 * columns place on the solution. This is done by adding these columns
 * to the matrix but behind the normal columns. These additional columns
 * wont be searched by the algorithm if it looks for the next task to achieve.
 *
 * Multiple instances of the same piece is handles in a similar way. To
 * prevent finding the same solution again and again with just the
 * pieces swapping places we number the pieces and their possible
 * placements and disallow that the position number of piece n is lower
 * than the position number of piece n-1. This can be achieved by adding
 * more constraint columns. There need to be one column for each
 *
 * negative result show there is something wrong: the place -result has not
 * possible position inside the result
 */
int assembler_1_c::prepare(bool hasRange, unsigned int rangeMin, unsigned int rangeMax) {

  if (hasRange) {
    fprintf(stderr, "range optimisation used min %u, max %u\n", rangeMin, rangeMax);
  }

  const voxel_c * result = getResultShape(problem);

  /* nodes 1..n are the columns nodes */
  GenerateFirstRow(result->countState(voxel_c::VX_FILLED)+
      result->countState(voxel_c::VX_VARIABLE)+
      problem.getNumberOfParts()+
      (hasRange?1:0));

  /* this array contains the column in our matrix that corresponds with
   * the voxel position inside the result. We use this matrix because
   * the calculation of the exact column depends on the number of FILLED
   * and VARIABLE voxels and is relatively expensive to calculate
   * with this lookup I was able to reduce the preparation time
   * from 5 to 0.5 seconds for TheLostDay puzzle
   */
  std::vector<unsigned int> columns(result->getXYZ());

  // only used, when hasRange is true, so we need to initialise it to get rid of warnings
  // the variable contains the column number where the range checks are done
  unsigned int rangeColumn = 0;

  {
    int c = 1 + problem.getNumberOfParts();

    for (unsigned int i = 0; i < result->getXYZ(); i++) {
      switch(result->getState(i)) {
      case voxel_c::VX_VARIABLE:
        min[c] = 0;
        holeColumns.push_back(c);
        // fall through
      case voxel_c::VX_FILLED:
        columns[i] = c++;
        break;
      default:
        columns[i] = 0;
        break;
      }
    }

    if (hasRange) {
      min[c] = rangeMin;
      max[c] = rangeMax;
      rangeColumn = c;
    }
  }

  /* find the symmetry breaker
   *
   * OK, what idea is behind this: we try to find as few double solutions as possible
   * because we don't want to fist search them and later on discard them because they are
   * double, so what do we do to prevent double solutions?
   *
   * Select one piece and remove rotations from this piece so that we don't even try to
   * place this piece in all possible positions. But which rotations need to be removed?
   * This depends on the symmetries that are present in the result and the symmetries
   * that are present in the piece
   */
  symmetries_t resultSym = result->selfSymmetries();
  const gridType_c * gt = problem.getPuzzle().getGridType();
  const symmetries_c * sym = problem.getPuzzle().getGridType()->getSymmetries();
  unsigned int symBreakerShape = 0xFFFFFFFF;

  /* so, if we have just the self-symmetry in the result, everything needs to be tried
   * and not rotations can be removed
   */
  if (!unSymmetric(resultSym)) {

    /* now we try to find the most "suitable" piece for our rotation removal. What is
     * suitable? Suitable is the piece shape that has the least common symmetries with
     * the result and that has the fewest pieces
     *
     * FIXME: if there is more than one suitable piece, select the one with the most
     * placements, this will gain us a little (or even bigger) speed-up
     * as its a difference if we select a piece that has only one placement anyway
     * or select one with 400 placements of which 23/24th can be dropped
     */
    unsigned int symBreakerPiece = 0;
    unsigned int pc = problem.getPartMaximum(0);
    unsigned int bestFound = sym->countSymmetryIntersection(resultSym, problem.getPartShape(0)->selfSymmetries());
    symBreakerShape = 0;

    for (unsigned int i = 1; i < problem.getNumberOfParts(); i++) {

      unsigned int cnt = sym->countSymmetryIntersection(resultSym, problem.getPartShape(i)->selfSymmetries());

      if ((problem.getPartMaximum(i) < problem.getPartMaximum(symBreakerShape)) ||
          ((problem.getPartMaximum(i) == problem.getPartMaximum(symBreakerShape)) && (cnt < bestFound))) {
        bestFound = cnt;
        symBreakerShape = i;
        symBreakerPiece = pc;
      }

      pc += problem.getPartMaximum(i);
    }

    bool pieceRanges = false;
    for (unsigned int i = 0; i < problem.getNumberOfParts(); i++)
      if (problem.getPartMinimum(i) != problem.getPartMaximum(i)) {
        pieceRanges = true;
        break;
      }

    /* With piece ranges the shape above may be one that an assembly leaves
     * out, and such a shape cannot break the symmetry. One that is in every
     * assembly exactly once can, whatever ranges the other shapes have:
     * turning an assembly by a symmetry of the result gives an assembly of
     * the same pieces, so every assembly has a turned copy with this one
     * piece in an orientation we keep. Take the one of those shapes with the
     * fewest symmetries in common with the result.
     */
    bool rangeBreaker = false;
    if (pieceRanges && settings.rangeSymmetryBreaker) {
      unsigned int at = 0;
      for (unsigned int i = 0; i < problem.getNumberOfParts(); i++) {
        if (problem.getPartMinimum(i) == 1 && problem.getPartMaximum(i) == 1) {
          unsigned int cnt = sym->countSymmetryIntersection(resultSym, problem.getPartShape(i)->selfSymmetries());
          if (!rangeBreaker || cnt < bestFound) {
            bestFound = cnt;
            symBreakerShape = i;
            symBreakerPiece = at;
            rangeBreaker = true;
          }
        }
        at += problem.getPartMaximum(i);
      }
    }

    bool tmp = sym->symmetriesLeft(resultSym, problem.getPartShape(symBreakerShape)->selfSymmetries());

    if (tmp || (problem.getPartMaximum(symBreakerShape) > 1) || pieceRanges) {

      // we can not use the symmetry breaker shape, if there is more than one piece
      // of this shape in the problem
      if (!rangeBreaker && (pieceRanges || problem.getPartMaximum(symBreakerShape) > 1)) {
        symBreakerShape = 0xFFFFFFFF;
        symBreakerPiece = 0xFFFFFFFF;
      }

      checkForTransformedAssemblies(symBreakerPiece, nullptr);
    }

    if (sym->symmetryContainsMirror(resultSym)) {
      /* we need to to the mirror check here, and initialise the mirror
       * structure, otherwise no mirror check will be done
       */

      /* so, we need to find out which case this puzzle is, depending on the pieces
       * 1) all pieces contain mirror symmetries -> check mirrors, but no pairs
       * 2) as least one piece has no mirror symmetries
       *   2a) all pieces with no mirror symmetries have a mirror partner -> check mirrors, find pairs
       *   2b) at least one piece with no mirror symmetries has no partner -> no mirror check
       */
      pc = 0;

      typedef struct {
        unsigned int shape;    // the shape of this piece
        unsigned int mirror;   // the mirror shape of this piece
        unsigned int trans;
      } mm;

      std::vector<mm> mirror(problem.getNumberOfPieces());

      // first initialize
      for (unsigned int i = 0; i < problem.getNumberOfParts(); i++)
        for (unsigned int p = 0; p < problem.getPartMaximum(i); p++) {
          mirror[pc].shape = i;
          mirror[pc].mirror = (unsigned int)-1;
          mirror[pc].trans = 255;
          pc++;
        }

      bool mirrorCheck = true;

      // now go over all shapes
      for (unsigned int i = 0; i < problem.getNumberOfPieces(); i++) {

        // we have already found the mirror for this shape
        if (mirror[i].mirror < problem.getNumberOfPieces())
          continue;

        if (!sym->symmetryContainsMirror(problem.getPartShape(mirror[i].shape)->selfSymmetries())) {
          /* this shape is not self mirroring, so we need to look out
           * for a shape that is the mirror of this shape
           */
          bool found = false;

          // now see if we can find another shape that is the mirror of the current shape
          for (unsigned int j = i+1; j < problem.getNumberOfPieces(); j++) {

            if (mirror[j].mirror < problem.getNumberOfPieces())
              continue;

            unsigned int trans = problem.getPartShape(mirror[i].shape)->getMirrorTransform(
                problem.getPartShape(mirror[j].shape));

            if (trans > 0) {
              // found a mirror shape

              mirror[i].mirror = j;
              mirror[i].trans = trans;
              mirror[j].mirror = i;
              mirror[j].trans = problem.getPartShape(mirror[j].shape)->getMirrorTransform(
                  problem.getPartShape(mirror[i].shape));

              found = true;
              break;
            }
          }

          // when we could not find a mirror transformation for the non mirrorable piece
          // we can stop and we don't need to make mirror checks
          if (!found) {
            mirrorCheck = false;
            break;
          }
        }
      }

      // TODO: also add when piece ranges are used
      if (mirrorCheck || pieceRanges) {
        /* all the shapes are either self mirroring or have a mirror pair
         * so we create the mirror structure and we do the mirror check
         * we also need to that when ranges are used because the final solution
         * might use only mirrorable pieces and then we need this information
         */
        auto mir = std::make_unique<mirrorInfo_c>();

        for (unsigned int i = 0; i < problem.getNumberOfPieces(); i++)
          if (mirror[i].trans != 255)
            mir->addPieces(i, mirror[i].mirror, mirror[i].trans);

        checkForTransformedAssemblies(symBreakerPiece, std::move(mir));
      }
    }
  }

  /* node 0 is the start node for everything */

  /* even though the matrix has a column for each result voxel and each piece we leave out
   * the VARIABLE voxels in the ring list of the header. This is to avoid selecting these
   * columns for filling. The columns for the VARIABLE voxels are only there to make sure
   * these voxels are only used once
   */

  std::vector<voxel_c *> cache(sym->getNumTransformationsMirror(), nullptr);

  placementFinder_c finder(problem, result);
  std::vector<long> voxelOffsets;
  std::vector<int> positions;

  const long rsx = result->getX();
  const long rsy = result->getY();

  /* now we insert one shape after another */
  for (unsigned int pc = 0; pc < problem.getNumberOfParts(); pc++) {

    reducePiece = pc;

    // setup weight values so that they do fit the number of pieces for this
    // shape
    max[pc+1] = problem.getPartMaximum(pc);
    min[pc+1] = problem.getPartMinimum(pc);

    unsigned int voxels = problem.getPartShape(pc)->countState(voxel_c::VX_FILLED);

    /* this array contains all the pieces found so far, this will help us
     * to not add two times the same piece to the structure */
    unsigned int cachefill = 0;
    unsigned int placements = 0;

    /* go through all possible rotations of the piece
     * if shape is new to cache, add it to the cache and also
     * add the shape to the matrix, in all positions that it fits
     */
    for (unsigned int rot = 0; rot < sym->getNumTransformations(); rot++) {

      voxel_c * rotation = gt->getVoxel(problem.getPartShape(pc));
      if (!rotation->transform(rot)) {
        delete rotation;
        continue;
      }

      rotation = addToCache(cache.data(), &cachefill, rotation);

      if (rotation) {
        finder.find(rotation, voxelOffsets, positions);

        for (unsigned int pos = 0; pos < positions.size(); pos += 3) {

          const int x = positions[pos];
          const int y = positions[pos+1];
          const int z = positions[pos+2];

          int piecenode = AddPieceNode(pc, rot, x+rotation->getHx(), y+rotation->getHy(), z+rotation->getHz());
          placements++;

          /* now add the used cubes of the piece */
          const long base = x + rsx * (y + rsy * (long)z);
          for (unsigned int v = 0; v < voxelOffsets.size(); v++)
            AddVoxelNode(columns[base + voxelOffsets[v]], piecenode);

          // if we use the range counting and the piece is using a range, add it to
          // the column
          if (hasRange && (min[pc+1] != max[pc+1]))
            AddRangeNode(rangeColumn, piecenode, voxels);
        }
        /* for the symmetry breaker piece we also add all symmetries of the box */
        if (pc == symBreakerShape)
          for (unsigned int r = 1; r < sym->getNumTransformations(); r++)
            if (sym->symmetrieContainsTransformation(resultSym, r)) {

              voxel_c * vx = gt->getVoxel(problem.getPartShape(pc));

              if (!vx->transform(rot) || !vx->transform(r)) {
                delete vx;
                continue;
              }

              addToCache(cache.data(), &cachefill, vx);
            }
      }
    }

    for (unsigned int i = 0; i < cachefill; i++)  delete cache[i];

    /* check, if the current piece has at least one placement */
    if (placements == 0 && problem.getPartMinimum(pc) > 0)
    {
      return -problem.getShapeIdOfPart(pc);
    }
  }

  return 1;
}

assembler_1_c::errState assembler_1_c::createMatrix(bool keepMirror, bool keepRotations, bool comp) {

  bt_assert(problem.resultValid());

  complete = comp;
  pendingHuangPrefixes.clear();
  restoredSimdSolver.reset();

  if (!canHandle(problem))
    return ERR_PUZZLE_UNHANDABLE;

  /* get and save piece number of puzzle */
  piecenumber = problem.getNumberOfPieces();

  /* count the filled and variable units */
  unsigned int res_vari = getResultShape(problem)->countState(voxel_c::VX_VARIABLE);
  unsigned int res_filled = getResultShape(problem)->countState(voxel_c::VX_FILLED) + res_vari;

  // check if number of voxels in pieces is not bigger than
  // number of voxel in result

  // check if number of filled voxels in result
  // is not bigger than number of voxels in pieces
  unsigned int min = 0;
  unsigned int max = 0;

  for (unsigned int j = 0; j < problem.getNumberOfParts(); j++) {
    min += problem.getPartShape(j)->countState(voxel_c::VX_FILLED) * problem.getPartMinimum(j);
    max += problem.getPartShape(j)->countState(voxel_c::VX_FILLED) * problem.getPartMaximum(j);
  }

  if (min == max)
    holes = res_filled - min;
  else if (problem.maxHolesDefined())
    holes = problem.getMaxHoles();
  else
    holes = 0xFFFFFF;

  if (min > res_filled) {
    errorsState = ERR_TOO_MANY_UNITS;
    errorsParam = min-res_filled;
    return errorsState;
  }

  if (max < res_filled-res_vari) {
    errorsState = ERR_TOO_FEW_UNITS;
    errorsParam = res_filled-res_vari-max;
    return errorsState;
  }

  /* in some cases it is useful to count the number of blocks the range pieces contribute
   * this is calculated here
   *
   * the result will contain between res_filled-res_vari   and res_filled voxels
   * now we can subtract all voxels contributed by fixed placed pieces (with no range)
   * and get the range of voxels that must be occupied by the range pieces
   *
   * This additional check prevents adding too many pieces from the ranges and so making placement of
   * pieces that have to be placed impossible
   */
  int RangeMin = res_filled-res_vari;
  int RangeMax = res_filled;

  for (unsigned int j = 0; j < problem.getNumberOfParts(); j++) {
    if (problem.getPartMinimum(j) == problem.getPartMaximum(j)) {
      RangeMin -= problem.getPartShape(j)->countState(voxel_c::VX_FILLED) * problem.getPartMinimum(j);
      RangeMax -= problem.getPartShape(j)->countState(voxel_c::VX_FILLED) * problem.getPartMinimum(j);
    }
  }

  if (RangeMin < 0) RangeMin = 0;

  /* fill the nodes arrays */
  int error = prepare(min != max, RangeMin, RangeMax);

  // check, if there is one piece not placeable
  if (error <= 0) {
    errorsState = ERR_CAN_NOT_PLACE;
    errorsParam = -error;
    return errorsState;
  }

  if (keepMirror) {
    avoidTransformedMirror.reset();
  }

  if (keepRotations)
    avoidTransformedAssemblies = false;

  /* back to the state the constructor left, so getFinished() does not read a
   * previous run's leftovers */
  totalTasks.store(0, std::memory_order_relaxed);
  completedTasks.store(0, std::memory_order_relaxed);
  searchComplete.store(false, std::memory_order_relaxed);

  errorsState = ERR_NONE;
  return errorsState;
}

void assembler_1_c::remove_column(unsigned int c) {
  unsigned int j = c;
  do {
    right[left[j]] = right[j];
    left[right[j]] = left[j];

    j = down[j];
  } while (j != c);
}

/* find identical columns within the matrix and remove all but one
 * of these identical columns, this will not save us iterations, but will
 * make the iterations much cheaper
 *
 * returns the number of columns that were removed
 */
unsigned int assembler_1_c::clumpify(void) {

  /* two columns are identical, and one of them can be removed, when they
   * have the same min and max values and are contained in exactly the same
   * placement rows with the same node weights.
   *
   * instead of comparing all pairs of columns we compute a signature for
   * each column (the sequence of (placement row, node weight) pairs, the
   * down links enumerate the nodes in row order because rows are appended
   * in increasing order) and group the columns by a hash of that signature.
   * only columns within the same group are then compared exactly. This
   * is linear in the number of matrix nodes while the pairwise comparison
   * was quadratic in the number of columns
   */

  /* the columns in header ring order, the first column of each group of
   * identical columns is the one that survives, like before */
  std::vector<unsigned int> cols;
  for (unsigned int c = right[0]; c; c = right[c])
    cols.push_back(c);

  /* all signatures are stored back to back in one vector, per column we
   * keep the start of its signature within that vector */
  std::vector<unsigned long> sigData;
  std::vector<unsigned long> sigStart;
  std::vector<unsigned long long> sigHash;

  sigStart.reserve(cols.size()+1);
  sigHash.reserve(cols.size());

  for (unsigned int i = 0; i < cols.size(); i++) {

    const unsigned int col = cols[i];

    sigStart.push_back(sigData.size());

    unsigned long long h = 14695981039346656037ull;
    h = (h ^ min[col]) * 1099511628211ull;
    h = (h ^ max[col]) * 1099511628211ull;

    unsigned int line = 0;

    for (unsigned int row = down[col]; row != col; row = down[row]) {

      /* find the placement row this node belongs to, the nodes come in
       * increasing order so the line only ever advances */
      while ((line+1 < piecePositions.size()) && (piecePositions[line+1].row <= row))
        line++;

      sigData.push_back(line);
      sigData.push_back(weight[row]);

      h = (h ^ line) * 1099511628211ull;
      h = (h ^ (unsigned long long)weight[row]) * 1099511628211ull;
    }

    sigHash.push_back(h);
  }

  sigStart.push_back(sigData.size());

  /* group by hash, keep the first column of each group, exactly verify
   * the others against it before removing them */
  typedef std::unordered_map<unsigned long long, std::vector<unsigned int> > hashMap;
  hashMap groups;

  unsigned int removed = 0;

  for (unsigned int i = 0; i < cols.size(); i++) {

    std::vector<unsigned int> & group = groups[sigHash[i]];

    bool dup = false;

    for (unsigned int g = 0; g < group.size(); g++) {

      const unsigned int j = group[g];

      if (min[cols[i]] != min[cols[j]] || max[cols[i]] != max[cols[j]])
        continue;

      const unsigned long leni = sigStart[i+1]-sigStart[i];

      if (leni != sigStart[j+1]-sigStart[j])
        continue;

      if ((leni == 0) ||
          (memcmp(&sigData[sigStart[i]], &sigData[sigStart[j]], leni * sizeof(unsigned long)) == 0)) {
        dup = true;
        break;
      }
    }

    if (dup) {
      remove_column(cols[i]);
      removed++;
    } else
      group.push_back(i);
  }

  return removed;
}

void assembler_1_c::remove_row(unsigned int r) {
  unsigned int j = r;
  do {
    unsigned int u, d;

    colCount[colCount[j]] -= weight[j];

    u = up[j];
    d = down[j];

    up[d] = u;
    down[u] = d;

    j = right[j];
  } while (j != r);
}

void assembler_1_c::reduce(void) {

  std::vector<unsigned int> toRemove;

  unsigned int row_rem = 0;
  unsigned int col_rem = clumpify();

  /* this is a quick impossible row removal code, is is not at thorough
   * as the code below but way faster and it is used to quickly remove
   * obviously impossible rows
   *
   * what it does is:
   * check for each column c, where at least one row must be selected (min > 0)
   *    check each row r that might be selected to fulfil this column c conditions
   *       if this row results for another column c2 that that column reaches the
   *       maximum possible value (max) then increase counter
   *    also count the number of rows in this column
   *
   *    now check for all columns c2 != c if the number if the counter is equal
   *    to number of rows. If this is the case then that means that
   *    all possible rows that make column c condition true, also make
   *    column c2 condition so that no other row will come there, so all
   *    rows that are not in the c set but contribute to c2 can be removed
   */
  std::vector<unsigned int> columns(headerNodes);

  for (unsigned int col = right[0]; col; col = right[col]) {

    // this was taken from the reduce of assembler 0 (removed 2026-11-16)
    // as I don't know how to generalise the approach
    // we do it only on columns with min = max = 1
    if (min[col] == 0) continue;

    memset(columns.data(), 0, headerNodes * sizeof(unsigned int));

    unsigned int placements = 0;
    for (unsigned int r = down[col]; r != col; r = down[r]) {
      for (unsigned int j = right[r]; j != r; j = right[j])
        if (weight[j] == max[colCount[j]])
          columns[colCount[j]]++;
      placements++;
    }

    /* now columns contains the number of times the voxel is filled
     * with this piece and placements contains the number of placements
     * for the current piece. If the number of times a voxel is filled
     * equal to the total number of placements for that piece the
     * piece fills that unit in every possible of its placements, so no other
     * piece can fill that unit and all placements of other pieces that fill
     * that unit can be removed
     */
    for (unsigned int c = right[0]; c; c = right[c]) {
      if (columns[c] == placements) {

        toRemove.clear();

        for (unsigned int r = down[c]; r != c; r = down[r]) {

          /* find the column col */
          unsigned int c2 = right[r];
          while ((colCount[c2] != col) && (c2 != r)) c2 = right[c2];

          if (c2 == r)
            toRemove.push_back(r);
        }

        row_rem += toRemove.size();

        /* remove the rows found */
        for (unsigned int rem = 0; rem < toRemove.size(); rem++) {
          remove_row(toRemove[rem]);
        }
      }
    }
  }

  toRemove.clear();

  col_rem += clumpify();

  bool dosth;

  do {

    for (unsigned int pp = 0; pp < piecePositions.size(); pp++) {

      unsigned int row = piecePositions[pp].row;
      reducePiece = piecePositions[pp].piece;

      // if row is no longer in there skip
      if (up[down[row]] != row || down[up[row]] != row) continue;


      // add row to rowset
      weight[colCount[row]] += weight[row];
      for (unsigned int r = right[row]; r != row; r = right[r])
        weight[colCount[r]] += weight[r];

      std::vector<unsigned int>hidden_rows;

      for (unsigned int r = right[row]; r != row; r = right[r]) {
        int col = colCount[r];

        // remove all rows from the matrix that would exceed the maximum weight
        // in an open column when added to the current weight
        // this can be sped up by sorting the entries and stopping removal
        // as soon as we reach a valid value, but we have to start from the top
        // we only need to check columns that have a non zero value in the current row
        // as only those weights have changed

        // now check all rows of this column for too big weights
        for (int rr = down[col]; rr != col; rr = down[rr])
          if (weight[rr] + weight[col] > max[col]) {
            hiderow(rr);
            hidden_rows.push_back(rr);
          }
      }

      if (!open_column_conditions_fulfillable())
        toRemove.push_back(row);

      while (!hidden_rows.empty()) {
        unhiderow(hidden_rows.back());
        hidden_rows.pop_back();
      }

      // remove row from rowset
      // we only need to restart adding the row from the place we stopped
      // when we added
      for (unsigned int r = left[row]; r != row; r = left[r])
        weight[colCount[r]] -= weight[r];
      weight[colCount[row]] -= weight[row];
    }

    dosth = toRemove.size() != 0;
    row_rem += toRemove.size();

    while (!toRemove.empty()) {
      remove_row(toRemove.back());
      toRemove.pop_back();
    }

  } while (dosth);

  col_rem += clumpify();

  fprintf(stderr, "removed %u rows and %u columns\n", row_rem, col_rem);
}

void assembler_1_c::checkForTransformedAssemblies(unsigned int pivot, std::unique_ptr<mirrorInfo_c> mir) {
  avoidTransformedAssemblies = true;
  avoidTransformedPivot = pivot;
  avoidTransformedMirror = std::move(mir);
}

bool assembler_1_c::open_column_conditions_fulfillable(void) {

  for (int col = right[0]; col; col = right[col]) {

    if (weight[col] > max[col]) return false;
    if (weight[col] + colCount[col] < min[col]) return false;
  }

  return true;
}


void assembler_1_c::hiderow(int r) {

  for (int rr = right[r]; rr != r; rr = right[rr]) {
    up[down[rr]] = up[rr];
    down[up[rr]] = down[rr];

    colCount[colCount[rr]] -= weight[rr];
  }
  up[down[r]] = up[r];
  down[up[r]] = down[r];

  colCount[colCount[r]] -= weight[r];
}

void assembler_1_c::unhiderow(int r) {

  up[down[r]] = r;
  down[up[r]] = r;

  colCount[colCount[r]] += weight[r];

  for (int rr = left[r]; rr != r; rr = left[rr]) {
    up[down[rr]] = rr;
    down[up[rr]] = rr;

    colCount[colCount[rr]] += weight[rr];
  }
}


bool assembler_1_c::willRunParallel(unsigned int threads) const {
  /* every run with work left takes the task pool, even with one thread (the
   * pool runs worker 0 inline): its remainder is what makes a stopped run
   * resumable */
  (void)threads;
  return errorsState == ERR_NONE && !searchComplete.load(std::memory_order_relaxed);
}

unsigned int assembler_1_c::getRunThreads(void) const {
  const unsigned int threads = getEffectiveThreads();
  return willRunParallel(threads) ? threads : 1;
}

void assembler_1_c::search(unsigned int workers) {
  // Snapshot of the run token: assemble() refreshed the source. Captured
  // once here and handed to the solver/workers by value; nothing below
  // touches shared run state in a hot path.
  std::stop_token runTok = currentRunToken();
  running.store(true, std::memory_order_relaxed);

  /* Workers call smallerRotationExists() outside callbackMutex, which reaches
   * the lazy mutable caches on the result shape AND on every part shape.
   * Warm them all here, on the master, before any worker exists.
   */
  if (avoidTransformedAssemblies)
    prewarmSharedShapeCaches(problem);

  // Seeds are only valid for the engine that left them, so a remainder
  // keeps its engine whatever the settings say now.
  const bool useCells = !pendingHuangPrefixes.empty() ? pendingSeedsCells : !canUseSimd();
  buildNodePositions();
  std::unique_ptr<ISimdHuangCover> solver = std::move(restoredSimdSolver);
  if (!solver)
    solver = createSimdSolver(useCells);

  // Incremental in-session resume (issue #111): consume previously
  // salvaged prefixes as seeds; collect fresh salvage for a possible next
  // run. Separate vectors: seeds are read up front while salvage is
  // appended during the run.
  std::vector<HuangSeed> huangSeeds;
  huangSeeds.swap(pendingHuangPrefixes);
  std::vector<HuangSeed> huangSalvaged;
  // The solver makes the pre-pool stop check itself (salvaging its
  // seeds), so the hook fires right before it.
  runPoolSeedHook();
  solver->parallelSolve(
    workers,
    // consumed: done with this solution (delivered, filtered, or nobody
    // to deliver to); false only when the stop
    // declined it, which the solver then keeps as a remainder piece (see
    // SimdHuangCover::StopTrace).
    [this, runTok](const std::vector<unsigned int> &solution_nodes, bool &consumed) -> bool {
      consumed = (getCallback() == nullptr);
      if (getCallback()) {
        auto assembly = std::make_unique<assembly_c>(problem.getPuzzle().getGridType());

        /* The placements by shape, in the order found within a shape.
         * Problems with millions of assemblies spend their time here, so
         * one pass and one small sort, no allocation per assembly.
         */
        struct placed_t {
          unsigned int piece;
          unsigned char tran;
          int x, y, z;
        };
        static thread_local std::vector<placed_t> placed;
        placed.resize(solution_nodes.size());
        for (unsigned int i = 0; i < solution_nodes.size(); i++)
          getPieceInformation(solution_nodes[i], &placed[i].piece, &placed[i].tran,
                              &placed[i].x, &placed[i].y, &placed[i].z);
        std::stable_sort(placed.begin(), placed.end(),
                         [](const placed_t & a, const placed_t & b) { return a.piece < b.piece; });

        size_t at = 0;
        const unsigned int parts = problem.getNumberOfParts();
        for (unsigned int pc = 0; pc < parts; pc++) {
          unsigned int count = 0;
          for (; at < placed.size() && placed[at].piece == pc; at++, count++)
            assembly->addPlacement(placed[at].tran, placed[at].x, placed[at].y, placed[at].z);
          for (const unsigned int max = problem.getPartMaximum(pc); count < max; count++)
            assembly->addNonPlacement();
        }

        assembly->sort(problem);

        if (avoidTransformedAssemblies &&
            assembly->smallerRotationExists(problem, avoidTransformedPivot,
                                            avoidTransformedMirror.get(), complete)) {
          consumed = true;
          return true;
        }

        {
          std::lock_guard<std::mutex> lock(callbackMutex);
          if (runTok.stop_requested())
            return false;
          consumed = true;
          if (!getCallback()->assembly(std::move(assembly))) {
            stop();
            return false;
          }
        }
      }
      return !runTok.stop_requested();
    },
    // The run source (not just its token): a throwing cover worker fires
    // it so siblings stop promptly. Safe to borrow: no refresh happens
    // while this run's workers exist.
    runStop,
    iterations,
    totalTasks,
    completedTasks,
    (getCallback() != nullptr) ? getCallback()->threadBudget() : nullptr,
    huangSeeds.empty() ? nullptr : &huangSeeds,
    &huangSalvaged,
    &donations
  );

  /* The stop-after-drain rule (see drainedPoolComplete): a stop that lands after every seeded task was
   * counted with nothing salvaged completes exactly as an unstopped run
   * instead of saving an empty interrupted remainder.
   */
  runPoolDrainedHook();
  const bool huangStopped = runTok.stop_requested();
  if (huangStopped) {
    pendingHuangPrefixes = std::move(huangSalvaged);
    pendingSeedsCells = useCells;
  }
  if (!huangStopped || drainedPoolComplete(huangStopped, pendingHuangPrefixes.size(),
        totalTasks.load(std::memory_order_relaxed),
        completedTasks.load(std::memory_order_relaxed))) {
    /* searchComplete is what keeps a completed search from running again:
     * save() records it, so a reload does not replay everything and
     * re-report every assembly. */
    pendingHuangPrefixes.clear();
    restoredSimdSolver.reset();
    searchComplete.store(true, std::memory_order_relaxed);
  }
  // Otherwise the salvaged in-flight prefixes plus pool remainder stay; the
  // next assemble() resumes from them instead of regenerating everything.

}

/* The bit-parallel engine (SimdHuangCover) where it fits; Dancing Cells
 * (CellsCover) takes every other matrix, and every matrix with
 * settings.bitParallel off. */
bool assembler_1_c::canUseSimd(void) const {
  if (!settings.bitParallel)
    return false;

  if (!simdEngineSupports())
    return false;

  // Memory footprint guard (issue #92): voxel_mask is tier-sized, so a
  // high-tier puzzle with many placement rows would allocate hundreds of MB
  // where Dancing Cells copes in tens. Refuse the SIMD path past the budget;
  // the fallback stays correct, just slower. Counts rows and matrix nodes in
  // one pass over the shape columns (node count feeds the alias-table and
  // vector-payload part of the estimate).
  {
    uint64_t estRows = 0;
    uint64_t estNodes = 0;
    const unsigned int nShapes = problem.getNumberOfParts();
    for (unsigned int s = 1; s <= nShapes; s++)
      for (unsigned int r = down[s]; r != s; r = down[r]) {
        estRows++;
        for (unsigned int c = right[r]; c != r; c = right[c])
          estNodes++;
      }
    if (!huang_memory::fitsMemoryBudget(headerNodes - 1, estRows, estNodes, settings.huangMemoryBytes))
      return false;
  }

  return true;
}

bool assembler_1_c::simdEngineSupports(void) const {

  // the widest tier (SimdHuangCover32768)
  if (headerNodes - 1 > 32768)
    return false;

  // NOTE: range puzzles (matrix has one more column than shapes + filled +
  // variable voxels: the piece-count range column) are deliberately NOT
  // excluded here. SimdHuangCover models the range column explicitly
  // (is_range bounds, per-row range_weight, max pruning in every filterRows
  // kernel, min/max goal check in search()) and the engine equivalence
  // tests cover range puzzles, so falling back would only cost speed
  // (SolidSix: 0.45 s against 2.0 s on Dancing Cells).
  const voxel_c * result = getResultShape(problem);
  unsigned int res_vari = result ? result->countState(voxel_c::VX_VARIABLE) : 0;

  if (res_vari > 0) {
    // If there are variable voxels AND any shape has min < max,
    // solutions can have variable number of pieces filling variable voxels: Dancing Cells.
    for (unsigned int s = 0; s < problem.getNumberOfParts(); s++) {
      if (problem.getPartMinimum(s) != problem.getPartMaximum(s))
        return false;
    }
  }

  return true;
}

std::unique_ptr<ISimdHuangCover> assembler_1_c::createSimdSolver(bool cells) const {
  const voxel_c * result = getResultShape(problem);
  unsigned int num_cols = headerNodes - 1;
  unsigned int num_shapes = problem.getNumberOfParts();
  unsigned int res_filled = result->countState(voxel_c::VX_FILLED);
  unsigned int res_vari = result->countState(voxel_c::VX_VARIABLE);
  bool hasRange = (num_cols == (num_shapes + res_filled + res_vari + 1));
  unsigned int rangeColumn = hasRange ? num_cols : 0;

  std::unique_ptr<ISimdHuangCover> solver;
  if (cells) {
    solver = std::make_unique<CellsCover>(num_cols, num_shapes, settings);
  } else if (num_cols <= 256) {
    solver = std::make_unique<SimdHuangCover256>(num_cols, num_shapes, settings);
  } else if (num_cols <= 512) {
    solver = std::make_unique<SimdHuangCover512>(num_cols, num_shapes, settings);
  } else if (num_cols <= 1024) {
    solver = std::make_unique<SimdHuangCover1024>(num_cols, num_shapes, settings);
  } else if (num_cols <= 2048) {
    solver = std::make_unique<SimdHuangCover2048>(num_cols, num_shapes, settings);
  } else if (num_cols <= 4096) {
    solver = std::make_unique<SimdHuangCover4096>(num_cols, num_shapes, settings);
  } else if (num_cols <= 8192) {
    solver = std::make_unique<SimdHuangCover8192>(num_cols, num_shapes, settings);
  } else if (num_cols <= 16384) {
    solver = std::make_unique<SimdHuangCover16384>(num_cols, num_shapes, settings);
  } else {
    solver = std::make_unique<SimdHuangCover32768>(num_cols, num_shapes, settings);
  }
  solver->setHoles(holes);

  /* only the columns still linked into the header ring, not 1..num_cols.
   * reduce() ends in clumpify(), which drops every column covered by exactly
   * the same rows with the same weights as an earlier one, and remove_column()
   * unlinks those columns' nodes from the rows along with it. The solver has to
   * see the same matrix the rows describe: a column no row can reach reads as
   * an unsatisfiable constraint and ends the search at the root.
   */
  for (unsigned int c = right[0]; c != 0; c = right[c]) {
    bool is_shape = (c <= num_shapes);
    bool is_range = (hasRange && c == rangeColumn);
    bool is_voxel = (!is_shape && !is_range);
    bool is_hole = false;
    for (unsigned int hc : holeColumns) {
      if (hc == c) {
        is_hole = true;
        break;
      }
    }
    solver->setColumnBounds(c, min[c], max[c], is_voxel, is_shape, is_range, is_hole);
  }

  for (unsigned int pc = 0; pc < num_shapes; pc++) {
    unsigned int shape_col = pc + 1;
    unsigned int shape_row_idx = 0;
    for (int r = down[shape_col]; r != (int)shape_col; r = down[r]) {
      std::vector<unsigned int> cols;
      std::vector<unsigned int> weights;
      std::vector<unsigned int> nodes_in_row;
      unsigned int curr = r;
      unsigned int range_w = 0;
      do {
        nodes_in_row.push_back(curr);
        unsigned int col = colCount[curr];
        unsigned int w = weight[curr];
        cols.push_back(col);
        weights.push_back(w);
        if (hasRange && col == rangeColumn) {
          range_w = w;
        }
        curr = right[curr];
      } while (curr != (unsigned int)r);

      uint32_t row_idx = solver->addRow(r, pc, shape_col, shape_row_idx++, range_w, cols, weights);
      for (unsigned int n : nodes_in_row) {
        solver->registerNodeAlias(n, row_idx);
      }
    }
  }

  return solver;
}

void assembler_1_c::assemble(assembler_cb * callback) {

  // Canonical per-run refresh: everything below observes this run's token;
  // a second assemble() starts unstopped.
  beginRun();

  if (willRunParallel(getEffectiveThreads())) {
    asm_bc = callback;
    running.store(true, std::memory_order_relaxed);
    search(getEffectiveThreads());
  }

  running.store(false, std::memory_order_relaxed);
}

float assembler_1_c::getFinished(void) const {

  if (searchComplete.load(std::memory_order_relaxed))
    return 1.0f;

  /* Completed tasks over all tasks, in progress units (see TaskUnits): a
   * task that donates part of its territory, or is divided at a stop,
   * splits its units among the pieces, so the total never moves and the
   * bar never steps back. totalTasks is 0 until a run has its task list.
   *
   * acquire, paired with the release stores that publish the denominator:
   * seeing this run's total guarantees seeing this run's completed count,
   * never the previous run's.
   */
  const size_t n = totalTasks.load(std::memory_order_acquire);
  if (n == 0) return 0;
  const size_t done = completedTasks.load(std::memory_order_acquire);
  const double erg = static_cast<double>(done) / static_cast<double>(n);
  return (erg > 1.0) ? 1.0f : static_cast<float>(erg);
}

static unsigned int getInt(const char * s, unsigned int * i) {

  char * s2;

  *i = std::strtol (s, &s2, 10);

  if (s2)
    return s2-s;
  else
    return 500000;
}

static int stringToVector(const char * string, std::vector<unsigned int> & v) {

  unsigned int pos = 0;

  unsigned int count;

  // first get number of entries
  pos += getInt(string+pos, &count);

  v.clear();

  // now get vector content

  for (unsigned int i = 0; i < count; i++) {
    unsigned int val;
    pos += getInt(string+pos, &val);
    v.push_back(val);
  }

  return pos;
}

assembler_c::errState assembler_1_c::setPosition(const char * string, const char * version) {

  unsigned int len = strlen(string);
  pendingHuangPrefixes.clear();
  restoredSimdSolver.reset();
  resetTaskProgress();

  unsigned int pos = 0;

  /* Only the current version resumes: a paused search saved by another
   * BurrTools version is refused (the found assemblies stay, the search
   * starts over -- see the README, "Pausing a search and continuing
   * later"). That keeps a single reader and no legacy resume paths.
   */
  if (version == nullptr || strcmp(version, ASSEMBLER_VERSION) != 0)
    return ERR_CAN_NOT_RESTORE_VERSION;

  /* the state written by save(): 0 not started, 1 stopped part way (the
   * engine and its seeds follow), 2 complete */
  unsigned int state = 0;
  pos += getInt(string+pos, &state);
  if (state > 2) return ERR_CAN_NOT_RESTORE_SYNTAX;

  if (state == 2) {
    searchComplete.store(true, std::memory_order_relaxed);
    return ERR_NONE;
  }
  if (state == 0) {
    searchComplete.store(false, std::memory_order_relaxed);
    return ERR_NONE;
  }

  /* the engine of the seeds (2 the bit-parallel engine, 3 Dancing Cells),
   * then the seeds: prefix, a step kind per step (a pivot bit for 2, a
   * kind from 0 to 2 for 3, see HuangSeed and CellsCover), rest start */
  if (pos >= len) return ERR_CAN_NOT_RESTORE_INTERRUPTED;
  unsigned int ptype = 0;
  pos += getInt(string+pos, &ptype);
  if (ptype != 2 && ptype != 3) return ERR_CAN_NOT_RESTORE_SYNTAX;
  if (pos >= len) return ERR_CAN_NOT_RESTORE_SYNTAX;

  // Parse into locals first: a syntax failure must not leave partial seeds
  // behind.
  std::vector<HuangSeed> newPrefixes;
  unsigned int count = 0;
  pos += getInt(string+pos, &count);
  if (pos >= len && count > 0) return ERR_CAN_NOT_RESTORE_SYNTAX;
  for (unsigned int i = 0; i < count; i++) {
    HuangSeed seed;
    pos += stringToVector(string+pos, seed.prefix);      if (pos >= len) return ERR_CAN_NOT_RESTORE_SYNTAX;
    std::vector<unsigned int> pivots;
    pos += stringToVector(string+pos, pivots);           if (pos >= len) return ERR_CAN_NOT_RESTORE_SYNTAX;
    if (pivots.size() != seed.prefix.size()) return ERR_CAN_NOT_RESTORE_SYNTAX;
    for (unsigned int b : pivots) {
      if (b > (ptype == 3 ? 2u : 1u)) return ERR_CAN_NOT_RESTORE_SYNTAX;
      seed.pivotShape.push_back(static_cast<uint8_t>(b));
    }
    unsigned int restSkip = 0;
    pos += getInt(string+pos, &restSkip);                if (pos >= len && i + 1 < count) return ERR_CAN_NOT_RESTORE_SYNTAX;
    seed.restSkip = restSkip;
    newPrefixes.push_back(std::move(seed));
  }

  // save() writes a stopped search only with seeds behind it: an empty list
  // would restore as a fresh search over the solutions already stored.
  if (newPrefixes.empty())
    return ERR_CAN_NOT_RESTORE_INTERRUPTED;

  // The seeds must replay on this matrix: parallelSolve() treats one that
  // does not as an error, since skipping it would lose its territory. The
  // matrix is as createMatrix() left it, exactly as the run will build the
  // solver, and the run takes this solver over (restoredSimdSolver), so a
  // resume converts the matrix only once.
  // A remainder of the bit-parallel engine needs a matrix that engine can
  // take (settings.bitParallel aside: the seeds keep their engine).
  if (ptype == 2 && !simdEngineSupports())
    return ERR_CAN_NOT_RESTORE_SYNTAX;
  std::unique_ptr<ISimdHuangCover> seedSolver = createSimdSolver(ptype == 3);
  for (const auto & seed : newPrefixes)
    if (!seedSolver->seedReplays(seed))
      return ERR_CAN_NOT_RESTORE_SYNTAX;

  pendingHuangPrefixes = std::move(newPrefixes);
  pendingSeedsCells = ptype == 3;
  restoredSimdSolver = std::move(seedSolver);
  // completed first, then the total with release (see getFinished())
  completedTasks.store(0, std::memory_order_relaxed);
  totalTasks.store(pendingHuangPrefixes.size() * TaskUnits::kWhole, std::memory_order_release);
  searchComplete.store(false, std::memory_order_relaxed);

  return ERR_NONE;
}

static void vectorToStream(const std::vector<unsigned int> & v, std::ostream & str) {

  str << v.size() << " ";

  for (unsigned int i = 0; i < v.size(); i++)
    str << v[i] << " ";
}

void assembler_1_c::save(xmlWriter_c & xml) const
{
  xml.newTag("assembler");
  xml.newAttrib("version", ASSEMBLER_VERSION);

  std::ostream & str = xml.addContent();

  /* the state: 0 not started, 1 stopped part way, 2 complete. A stopped
   * search is followed by its remainder: the engine (2 the bit-parallel
   * engine, 3 Dancing Cells) and its seeds, prefix, step kinds and rest
   * start each. The remainder holds only unexplored territory. A stop
   * without seeds (before the pool was seeded) saves as not started: nothing
   * was searched.
   */
  if (searchComplete.load(std::memory_order_relaxed)) {
    str << "2 ";
  } else if (pendingHuangPrefixes.empty()) {
    str << "0 ";
  } else {
    str << "1 " << (pendingSeedsCells ? 3 : 2) << " " << pendingHuangPrefixes.size() << " ";
    for (const auto &seed : pendingHuangPrefixes) {
      vectorToStream(seed.prefix, str);
      // setPosition() requires one step kind per step: a seed without them
      // is a bug here, not something to pad.
      if (seed.pivotShape.size() != seed.prefix.size())
        bt_te("assembler_1_c::save: seed without one step kind per prefix step");
      std::vector<unsigned int> pivots(seed.pivotShape.begin(), seed.pivotShape.end());
      vectorToStream(pivots, str);
      str << seed.restSkip << " ";
    }
  }

  xml.endTag("assembler");
}

unsigned int assembler_1_c::getPiecePlacement(unsigned int node, int delta, unsigned int piece, unsigned char *tran, int *x, int *y, int *z) const {

  /* piece 2 shape */
  unsigned int pp = 0;
  unsigned int shape = 0;
  while (pp + problem.getPartMaximum(shape) <= piece) {
    pp += problem.getPartMaximum(shape);
    shape++;
  }

  if (!node)
    node = down[shape+1];

  while (delta > 0) {
    node = down[node];
    delta--;
  }

  while (delta < 0) {
    node = up[node];
    delta++;
  }

  unsigned int p;
  getPieceInformation(node, &p, tran, x, y, z);

  bt_assert(p == shape);

  return node;
}

unsigned int assembler_1_c::getPiecePlacementCount(unsigned int piece) const {

  /* piece 2 shape */
  unsigned int pp = 0;
  unsigned int shape = 0;
  while (pp + problem.getPartMaximum(shape) <= piece) {
    pp += problem.getPartMaximum(shape);
    shape++;
  }

  return colCount[shape+1];
}

bool assembler_1_c::canHandle(const problem_c &) {

  // right now there are no limits

  return true;
}

