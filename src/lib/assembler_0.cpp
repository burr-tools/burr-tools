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
#include "assembler_0.h"
#include "assembler_pool.h"
#include "simd_exact_cover.h"
#include "simd_config.h"

#include "bt_assert.h"
#include "problem.h"
#include "puzzle.h"
#include "voxel.h"
#include "assembly.h"
#include "gridtype.h"

#include "../tools/xml.h"

#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <climits>
#include <iomanip>
#include <limits>
#include <unordered_map>
#include <thread>
#include <stop_token>

#ifdef _WIN32
#define snprintf _snprintf
#endif

#define ASSEMBLER_VERSION "1.7"

/* print out the current matrix */
void printMatrix(
    const std::vector<unsigned int> & upDown,
    const std::vector<unsigned int> & left,
    const std::vector<unsigned int> & right,
    const std::vector<unsigned int> & colCount,
    unsigned int varivoxelStart,
    unsigned int varivoxelEnd
  ) {

  // check consistency of matrix
  {

    unsigned long cnt = 0;

    unsigned int c = right[0];
    while (c) {

      if (left[right[c]] != c) printf("lr %u\n", c);
      if (right[left[c]] != c) printf("rl %u\n", c);
      if (up(down(c)) != c) printf("ud %u\n", c);
      if (down(up(c)) != c) printf("du %u\n", c);
      cnt++;

      unsigned int r = down(c);

      while (r != c) {

        if (left[right[r]] != r) printf("lr %u\n", r);
        if (right[left[r]] != r) printf("rl %u\n", r);
        if (up(down(r)) != r) printf("ud %u\n", r);
        if (down(up(r)) != r) printf("du %u\n", r);
        cnt++;

        r = down(r);

      }

      c = right[c];
    }

    printf("checked %lu nodes for consistency\n", cnt);
  }

  printf("%u %u\n", varivoxelStart, varivoxelEnd);

  /* first find all the columns */
  std::vector<unsigned int> columns;

  unsigned int c = right[0];

  while (c) {
    columns.push_back(c);
    c = right[c];

    printf("n");
  }

  // not the variable columns

  if (varivoxelStart != varivoxelEnd) {

    c = varivoxelStart;
    do {
      columns.push_back(c);
      c = right[c];

      printf("v");
    } while (c != varivoxelStart);
  }

  printf("\n");

  std::vector<unsigned int> rows;

  for (unsigned int i = 0; i < columns.size(); i++)
    rows.push_back(down(columns[i]));

  /* all nodes are now in the first row below the header */
  while (true) {

    /* find the smallest real node ( must be > varivoxelEnd */
    int r = -1;

    for (unsigned int i = 0; i < rows.size(); i++)
      if (rows[i] > varivoxelEnd)
        if (r == -1 || rows[i] < rows[r])
          r = i;

    if (r == -1) break;

    unsigned int nodeS = rows[r];
    unsigned int node = nodeS;

    std::vector<int>matrixline;

    matrixline.resize(rows.size());

    do {

      unsigned int col = colCount[node]-1;

      for (unsigned int i = 0; i < columns.size(); i++) {
        if (columns[i] == col) {
          col = i;
          break;
        }
      }

      matrixline[col] = 1;

      node = right[node];

      rows[col] = down(rows[col]);

    } while (node != nodeS);

    for (unsigned int i = 0; i < matrixline.size(); i++)
      if (matrixline[i] == 1)
        printf("1");
      else
        printf(" ");

    printf("\n");
  }
}



void assembler_0_c::GenerateFirstRow(void) {

  for (unsigned int i = 0; i < varivoxelStart; i++) {
    right.push_back(i+1);
    left.push_back(i-1);
    upDown.push_back(i);
    upDown.push_back(i);
    colCount.push_back(0);
  }

  /* make the linked list cyclic */
  left[0] = varivoxelStart - 1;
  right[varivoxelStart - 1] = 0;

  /* create column nodes for vari columns, these
   * are not inside the column header list and so
   * are not forced to be filled but otherwise behave
   * like normal columns
   */
  for (unsigned int j = varivoxelStart; j <= varivoxelEnd; j++) {
    left.push_back(j-1);
    right.push_back(j+1);
    upDown.push_back(j);
    upDown.push_back(j);
    colCount.push_back(0);
  }
  left[varivoxelStart] = varivoxelEnd;
  right[varivoxelEnd] = varivoxelStart;
}

int assembler_0_c::AddPieceNode(unsigned int piece, unsigned int rot, unsigned int x, unsigned int y, unsigned int z) {
  unsigned long piecenode = left.size();

  left.push_back(piecenode);
  right.push_back(piecenode);
  upDown.push_back(up(piece+1));
  upDown.push_back(piece+1);

  down(up(piece+1)) = piecenode;
  up(piece+1) = piecenode;

  colCount.push_back(piece+1);
  colCount[piece+1]++;

  piecePositions.push_back(piecePosition(x, y, z, rot, piecenode, piece));

  return piecenode;
}

void assembler_0_c::getPieceInformation(unsigned int node, unsigned char *tran, int *x, int *y, int *z, unsigned int *piece) const {

  // piecePositions is sorted by row: AddPieceNode appends rows in strictly
  // increasing node order (piecenode == left.size() at creation), so binary
  // search finds the last entry with row <= node. Called once per piece of
  // every found assembly (e.g. kangaroo, 9831 assemblies); the logarithmic
  // scan keeps that affordable.
  // The not-found path throws unconditionally (not via bt_assert): besides
  // reporting matrix corruption loudly in release builds, [[noreturn]]
  // proves to all compilers that the outputs below are always set, which
  // an early return would not (-Wmaybe-uninitialized, MinGW).
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

/* find identical columns within the matrix and remove all but one
 * of these identical columns, this will not save us iterations, but will
 * make the iterations much cheaper
 *
 * returns the number of columns that were removed
 */
unsigned int assembler_0_c::clumpify(void) {

  /* two columns are identical, and one of them can be removed, when they
   * are contained in exactly the same placement rows.
   *
   * instead of comparing all pairs of columns we compute a signature for
   * each column (the sequence of placement rows, the down links enumerate
   * the nodes in row order because rows are appended in increasing order)
   * and group the columns by a hash of that signature. Only columns within
   * the same group are then compared exactly. This is linear in the number
   * of matrix nodes while the pairwise comparison was quadratic in the
   * number of columns
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

    unsigned int line = 0;

    for (unsigned int row = down(col); row != col; row = down(row)) {

      /* find the placement row this node belongs to, the nodes come in
       * increasing order so the line only ever advances */
      while ((line+1 < piecePositions.size()) && (piecePositions[line+1].row <= row))
        line++;

      sigData.push_back(line);

      h = (h ^ line) * 1099511628211ull;
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

void assembler_0_c::AddVoxelNode(unsigned int col, unsigned int piecenode) {
  unsigned long newnode = left.size();

  right.push_back(piecenode);
  left.push_back(left[piecenode]);
  right[left[piecenode]] = newnode;
  left[piecenode] = newnode;

  upDown.push_back(up(col));
  upDown.push_back(col);
  down(up(col)) = newnode;
  up(col) = newnode;

  colCount.push_back(col);
  colCount[col]++;
}

assembler_0_c::assembler_0_c(const problem_c & prob) :
  assembler_c(),
  problem(prob),
  pos(0),
  reducePiece(0),
  avoidTransformedAssemblies(false), avoidTransformedMirror(nullptr)
{
}

assembler_0_c::~assembler_0_c() = default;

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


bool assembler_0_c::canPlace(const voxel_c * piece, int x, int y, int z) const {

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
 * wont be searched by the alg. if it looks for the next task to achieve.
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
int assembler_0_c::prepare(void) {

  const voxel_c * result = getResultShape(problem);

  /* this array contains the column in our matrix that corresponds with
   * the voxel position inside the result. We use this matrix because
   * the calculation of the exact column depends on the number of FILLED
   * and VARIABLE voxels and is relatively expensive to calculate
   * with this lookup I was able to reduce the preparation time
   * from 5 to 0.5 seconds for TheLostDay puzzle
   */
  std::vector<unsigned int> columns(result->getXYZ());
  unsigned int piecenumber = problem.getNumberOfPieces();

  /* voxelindex is the inverse of the function column. It returns
   * the index (not x, y, z) of a given column in the matrix
   */
  std::vector<int> voxelindex(result->getXYZ() + piecenumber + 1, -1);

  {
    int v = 0;
    int c = 0;

    for (unsigned int i = 0; i < result->getXYZ(); i++) {
      switch(result->getState(i)) {
      case voxel_c::VX_VARIABLE:
        voxelindex[getVarivoxelStart() + v] = i;
        columns[i] = getVarivoxelStart() + v++;
        break;
      case voxel_c::VX_FILLED:
        voxelindex[1 + piecenumber + c] = i;
        columns[i] = 1 + piecenumber + c++;
        break;
      default:
        columns[i] = 0;
        break;
      }
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
    unsigned int bestFound = sym->countSymmetryIntersection(resultSym, problem.getPartShape(0)->selfSymmetries());
    symBreakerShape = 0;

    for (unsigned int i = 1; i < problem.getNumberOfParts(); i++) {

      unsigned int cnt = sym->countSymmetryIntersection(resultSym, problem.getPartShape(i)->selfSymmetries());

      if (cnt < bestFound) {
        bestFound = cnt;
        symBreakerShape = i;
      }
    }

    if (sym->symmetriesLeft(resultSym, problem.getPartShape(symBreakerShape)->selfSymmetries()))
      checkForTransformedAssemblies(symBreakerShape, nullptr);

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

      typedef struct {
        unsigned int shape;    // the shape of this piece
        unsigned int mirror;   // the mirror shape of this piece
        unsigned int trans;
      } mm;

      std::vector<mm> mirror(problem.getNumberOfPieces());

      // first initialize
      for (unsigned int i = 0; i < problem.getNumberOfParts(); i++) {
        mirror[i].shape = i;
        mirror[i].mirror = (unsigned int)-1;
        mirror[i].trans = 255;
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

      if (mirrorCheck) {
        /* all the shapes are either self mirroring or have a mirror pair
         * so we create the mirror structure and we do the mirror check
         */
        auto mir = std::make_unique<mirrorInfo_c>();

        for (unsigned int i = 0; i < problem.getNumberOfPieces(); i++)
          if (mirror[i].trans != 255)
            mir->addPieces(i, mirror[i].mirror, mirror[i].trans);

        checkForTransformedAssemblies(symBreakerShape, std::move(mir));
      }
    }
  }

  /* node 0 is the start node for everything */

  /* even though the matrix has a column for each result voxel and each piece we leave out
   * the VARIABLE voxels in the ring list of the header. This is to avoid selecting these
   * columns for filling. The columns for the VARIABLE voxels are only there to make sure
   * these voxels are only used once
   */

  /* nodes 1..n are the columns nodes */
  GenerateFirstRow();

  std::vector<voxel_c*> cache(sym->getNumTransformationsMirror(), nullptr);

  placementFinder_c finder(problem, result);
  std::vector<long> voxelOffsets;
  std::vector<int> positions;

  const long rsx = result->getX();
  const long rsy = result->getY();

  /* now we insert one shape after another */
  for (unsigned int pc = 0; pc < problem.getNumberOfParts(); pc++) {

    reducePiece = pc;

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
          placements = 1;

          /* now add the used cubes of the piece */
          const long base = x + rsx * (y + rsy * (long)z);
          for (unsigned int v = 0; v < voxelOffsets.size(); v++)
            AddVoxelNode(columns[base + voxelOffsets[v]], piecenode);
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
    if (placements == 0) {
      return -problem.getShapeIdOfPart(pc);
    }
  }

  return 1;
}



assembler_0_c::errState assembler_0_c::createMatrix(bool keepMirror, bool keepRotations, bool comp) {

  bt_assert(problem.resultValid());

  complete = comp;
  parallelTasks.clear();
  prunedTaskShare = 0.0f;
  remainderEngine = ENGINE_NONE;

  if (!canHandle(problem))
    return ERR_PUZZLE_UNHANDABLE;

  /* get and save piece number of puzzle */
  piecenumber = problem.getNumberOfPieces();

  /* count the filled and variable units */
  int res_vari = getResultShape(problem)->countState(voxel_c::VX_VARIABLE);
  int res_filled = getResultShape(problem)->countState(voxel_c::VX_FILLED) + res_vari;

  varivoxelStart = 1 + piecenumber + res_filled - res_vari;
  varivoxelEnd = 1 + piecenumber + res_filled;

  // check if number of voxels in pieces is not bigger than
  // number of voxel in result

  // check if number of filled voxels in result
  // is not bigger than number of voxels in pieces
  int h = res_filled;

  for (unsigned int j = 0; j < problem.getNumberOfParts(); j++)
    h -= problem.getPartShape(j)->countState(voxel_c::VX_FILLED);

  if (h < 0) {
    errorsState = ERR_TOO_MANY_UNITS;
    errorsParam = -h;
    return errorsState;
  }

  if (h > res_vari) {
    errorsState = ERR_TOO_FEW_UNITS;
    errorsParam = h-res_vari;
    return errorsState;
  }

  holes = h;

  /* allocate all the required memory */
  rows.assign(piecenumber, 0);
  columns.assign(piecenumber, 0);

  /* fill the nodes arrays */
  int error = prepare();

  // check, if there is one piece not placeable
  if (error <= 0) {
    errorsState = ERR_CAN_NOT_PLACE;
    errorsParam = -error;
    return errorsState;
  }
  pos = 0;
  iterations.store(0, std::memory_order_relaxed);
  resetTaskProgress();
  completedShare.store(0.0, std::memory_order_relaxed);
  searchComplete.store(false, std::memory_order_relaxed);

  if (keepMirror) {
    /* prepare() may already have allocated the mirror info via
     * checkForTransformedAssemblies; free it before dropping the pointer
     * (assembler_1_c::createMatrix does the same)
     */
    avoidTransformedMirror.reset();
  }

  if (keepRotations)
    avoidTransformedAssemblies = false;

  errorsState = ERR_NONE;
  return errorsState;
}

/* remove column from array, and also all the rows, where the column is one */
void assembler_0_c::cover(unsigned int col)
{
  {
    unsigned int l = left[col];
    unsigned int r = right[col];

    left[r] = l;
    right[l] = r;
  }

#if 0
  // the assembly code below is ca 20% faster than the gcc code
  // but not really portable, so if you feel adventourous
  // you can try it

  unsigned int * upDown_ptr = &(upDown[0]);
  unsigned int * right_ptr = &(right[0]);
  unsigned int * colCount_ptr = &(colCount[0]);

  unsigned int tmp;

  __asm__ (
      "push %%ebx                     \n"
      "movl %1, %%esi                 \n"           // esi = upDown
      "movl %2, %%edx                 \n"           // edx = right
      "movl %3, %%ebx                 \n"           // ebx = colCount
      "                               \n"
      "movl %0, %%ecx                 \n"           // read col from stack into eax
      "movl 4(%%esi,%%ecx,8), %%eax   \n"           // ax = i = down[col]
      "cmpl %%eax, %%ecx              \n"           // if ax == col
      "je 4f                          \n"           //  cendloop1
      "                               \n"
"1:                                   \n"           // cagainloop1:
      "                               \n"
      "movl (%%edx,%%eax,4), %%ecx    \n"           // j = right[i]
      "cmpl %%ecx, %%eax              \n"           // if (j(cx) == i
      "je 3f                          \n"           // cendloop2
      "                               \n"
      "movl %%eax, %4                 \n"           // put i onto stack
      "                               \n"
"2:                                   \n"           // cagainloop2
      "                               \n"
      "movl 0(%%esi,%%ecx,8), %%eax   \n"           // eax = up[j]
      "movl 4(%%esi,%%ecx,8), %%edi   \n"           // edx = down[ax]
      "movl %%edi, 4(%%esi,%%eax,8)   \n"           // ax = down[j]
      "movl %%eax, 0(%%esi,%%edi,8)   \n"           // ax = down[j]
      "                               \n"
      "movl (%%ebx,%%ecx,4), %%eax    \n"           // ax = colCount[j]
      "movl (%%edx,%%ecx,4), %%ecx    \n"           // cx = right[cx]
      "decl (%%ebx,%%eax,4)           \n"           // inc(colCount[ax])
      "cmpl %%ecx, %4                 \n"
      "jne  2b                        \n"           // cagainloop2
      "                               \n"           //      we know ecx == %5, so we don't need to load it
"3:                                   \n"           // cendloop2:
      "                               \n"
      "movl 4(%%esi,%%ecx,8), %%eax   \n"
      "cmpl %%eax, %0                 \n"
      "jne 1b                         \n"           // cagainloop1
      "                               \n"
"4:                                   \n"           // cendloop1
      "pop %%ebx                      \n"
     :
     : "m" (col), "m" (upDown_ptr), "m" (right_ptr), "m" (colCount_ptr), "m" (tmp)
     : "eax", "ecx", "edx", "esi", "edi"
   );

#else

  for (unsigned int i = down(col); i != col; i = down(i)) {
    for (unsigned int j = right[i]; j != i; j = right[j]) {

      unsigned int u = up(j);
      unsigned int d = down(j);

      up(d) = u;
      down(u) = d;

      colCount[colCount[j]]--;
    }
  }

#endif

}

void assembler_0_c::uncover(unsigned int col) {

#if 0
  // the assembly code below is ca 20% faster than the gcc code
  // but not really portable, so if you feel adventourous
  // you can try it

  unsigned int * upDown_ptr = &(upDown[0]);
  unsigned int * left_ptr = &(left[0]);
  unsigned int * colCount_ptr = &(colCount[0]);

  unsigned int tmp;

  __asm__ (
      "push %%ebx                     \n"
      "movl %1, %%esi                 \n"           // esi = up
      "movl %2, %%edx                 \n"           // edx = left
      "movl %3, %%ebx                 \n"           // ebx = colCount
      "                               \n"
      "movl %0, %%ecx                 \n"           // read col from stack into eax
      "movl (%%esi,%%ecx,8), %%eax    \n"           // ax = i = up[col]
      "cmpl %%eax, %%ecx              \n"           // if ax == col
      "je 4f                          \n"           // endloop1
      "                               \n"
"1:                                   \n"           // againloop1:
      "                               \n"
      "movl (%%edx,%%eax,4), %%ecx    \n"           // j = left[i]
      "cmpl %%ecx, %%eax              \n"           // if (j(cx) == i
      "je 3f                          \n"           // endloop2
      "                               \n"
      "movl %%eax, %4                 \n"           // put i onto stack
      "                               \n"
"2:                                   \n"           // againloop2:
      "                               \n"
      "movl 0(%%esi,%%ecx,8), %%eax   \n"           // ax = up[j]
      "movl 4(%%esi,%%ecx,8), %%edi   \n"           // ax = down[j]
      "movl %%ecx, 4(%%esi,%%eax,8)   \n"           // down[ax] = j
      "movl %%ecx, 0(%%esi,%%edi,8)   \n"           // up[ax] = j;
      "                               \n"
      "movl (%%ebx,%%ecx,4), %%eax    \n"           // ax = colCount[j]
      "movl (%%edx,%%ecx,4), %%ecx    \n"           // cx = left[cx]
      "incl (%%ebx,%%eax,4)           \n"           // inc(colCount[ax])
      "cmpl %%ecx, %4                 \n"
      "jne 2b                         \n"           // againloop2
      "                               \n"           // we know that %%ecx == %5, so we don't need to load it
"3:                                   \n"           // endloop2:
      "                               \n"
      "movl (%%esi,%%ecx,8), %%eax    \n"
      "cmpl %%eax, %0                 \n"
      "jne 1b                         \n"           // againloop1
      "                               \n"
"4:                                   \n"           // endloop1:
     "pop %%ebx                       \n"
     :
     : "m" (col), "m" (upDown_ptr), "m" (left_ptr), "m" (colCount_ptr), "m" (tmp)
     : "eax", "ecx", "edx", "esi", "edi"
   );

#else

  for (unsigned int i = up(col); i != col; i = up(i)) {
    for (unsigned int j = left[i]; j != i; j = left[j]) {

      colCount[colCount[j]]++;

      up(down(j)) = j;
      down(up(j)) = j;
    }
  }

#endif

  left[right[col]] = col;
  right[left[col]] = col;
}

/* remove all the columns from the matrix in which the given
 * row contains ones
 */
void assembler_0_c::cover_row(unsigned int r) {
  for (unsigned int j = right[r]; j != r; j = right[j])
    cover(colCount[j]);
}

bool assembler_0_c::try_cover_row(unsigned int r, unsigned int * columns) {

  memset(columns, 0, varivoxelEnd * sizeof(unsigned int));

  for (unsigned int j = right[r]; j != r; j = right[j])
    columns[colCount[j]] = 1;

  for (unsigned int j = right[r]; j != r; j = right[j]) {

    cover(colCount[j]);

    for (unsigned int k = right[0]; k; k = right[k]) {

      if ((columns[k] == 0) && (colCount[k] == 0)) {
        do {
          uncover(colCount[j]);
          j = left[j];
        } while (j != r);

        return false;
      }
    }
  }

  return true;
}

void assembler_0_c::uncover_row(unsigned int r) {
  for (unsigned int j = left[r]; j != r; j = left[j])
    uncover(colCount[j]);
}

void assembler_0_c::remove_row(unsigned int r) {
  unsigned int j = r;
  do {
    unsigned int u, d;

    colCount[colCount[j]]--;

    u = up(j);
    d = down(j);

    up(d) = u;
    down(u) = d;

    j = right[j];
  } while (j != r);
}

void assembler_0_c::remove_column(unsigned int c) {
  unsigned int j = c;
  do {
    right[left[j]] = right[j];
    left[right[j]] = left[j];

    j = down(j);
  } while (j != c);
}

void assembler_0_c::reinsert_row(unsigned int r) {
  unsigned int j = r;
  do {
    up(down(j)) = j;
    down(up(j)) = j;

    colCount[colCount[j]]++;

    j = right[j];
  } while (j != r);
}

bool assembler_0_c::checkmatrix(void) {

  /* check the number of holes, if they are larger than allowed return */
  unsigned int count = holes;
  for (unsigned int j = right[varivoxelEnd]; j != varivoxelEnd; j = right[j])
    if (colCount[j] == 0) {
      if (!count)
        return true;
      count--;
    }

  return false;
}

void assembler_0_c::reduce(void) {

  /* this array is used in several occasions, where we need to
   * keep some information for all columns
   */
  std::vector<unsigned int> columns(varivoxelEnd);
  unsigned int removed = 0;
  unsigned int remCol = 0;
  bool rem_sth;

  // we first collect all the rows that we finally want to
  // remove and only remove them after the complete check
  std::vector<unsigned int> rowsToRemove;

  reducePiece = 0;

  for (unsigned int col = right[0]; col; col = right[col]) {

    memset(columns.data(), 0, varivoxelEnd * sizeof(unsigned int));

    unsigned int placements = 0;
    for (unsigned int r = down(col); r != col; r = down(r)) {
      for (unsigned int j = right[r]; j != r; j = right[j])
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

        rowsToRemove.clear();

        for (unsigned int r = down(c); r != c; r = down(r)) {

          /* find the column col */
          unsigned int c2 = right[r];
          while ((colCount[c2] != col) && (c2 != r)) c2 = right[c2];

          if (c2 == r)
            rowsToRemove.push_back(r);
        }

        /* remove the rows found */
        for (unsigned int rem = 0; rem < rowsToRemove.size(); rem++) {
          remove_row(rowsToRemove[rem]);
        }

        removed += rowsToRemove.size();
      }
    }
  }

  remCol += clumpify();

  do {

    rem_sth = false;

    /* check all the pieces */
    for (unsigned int p = 0; p < piecenumber; p++) {

      reducePiece = p;

      // place the piece and check, if this leads to some
      // unfillable holes or unplaceable pieces or whatever
      // conditions that make a solution impossible
      cover(p+1);

      rowsToRemove.clear();

      // go over all the placements of the piece and check, if
      // each for possibility
      for (unsigned int r = down(p+1); r != p+1; r = down(r)) {

        // try to do this placement, if the placing goes
        // wrong already, we don't need to do the deep check
        if (!try_cover_row(r, columns.data())) {
          rowsToRemove.push_back(r);
        } else {

          /* and check if that results in a dead end */
          if (checkmatrix())
            rowsToRemove.push_back(r);

          uncover_row(r);
        }
      }

      uncover(p+1);

      for (unsigned int rem = 0; rem < rowsToRemove.size(); rem++)
        remove_row(rowsToRemove[rem]);

      rem_sth |= (rowsToRemove.size() > 0);
      removed += rowsToRemove.size();

      /* find all columns that are always filled with the given piece in each of it's possible
       * placements, no other piece can be there, all other pieces placements that fill
       * this cube can be removed
       */
      memset(columns.data(), 0, varivoxelEnd * sizeof(unsigned int));
      unsigned int placements = 0;
      for (unsigned int r = down(p+1); r != p+1; r = down(r)) {
        for (unsigned int j = right[r]; j != r; j = right[j])
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

          rowsToRemove.clear();

          unsigned int i = 0;
          for (unsigned int r = down(c); r != c; r = down(r)) {

            /* find out to which piece this row belongs */
            while ((i+1) < piecePositions.size() && piecePositions[i+1].row <= r) i++;

            if (piecePositions[i].piece != p)
              rowsToRemove.push_back(r);
          }

          /* remove the rows found */
          for (unsigned int rem = 0; rem < rowsToRemove.size(); rem++) {
            remove_row(rowsToRemove[rem]);
          }

          rem_sth |= (rowsToRemove.size() > 0);
          removed += rowsToRemove.size();

        }
      }
    }
  } while (rem_sth);

  remCol += clumpify();

  fprintf(stderr, "removed %u rows and %u columns\n", removed, remCol);
}

std::unique_ptr<assembly_c> assembler_0_c::buildAssembly(const unsigned int *row_nodes, unsigned int count) const {
  auto assembly = std::make_unique<assembly_c>(problem.getPuzzle().getGridType());

  if (count > piecenumber) {
    for (unsigned int i = 0; i < piecenumber; i++)
      assembly->addNonPlacement();
    return assembly;
  }

  std::vector<unsigned int> pieces(piecenumber, 0xFFFFFFFF);
  std::vector<unsigned char> trans(piecenumber);
  std::vector<int> xs(piecenumber);
  std::vector<int> ys(piecenumber);
  std::vector<int> zs(piecenumber);

  for (unsigned int i = 0; i < count; i++) {
    unsigned char tran;
    int x, y, z;
    unsigned int piece;

    getPieceInformation(row_nodes[i], &tran, &x, &y, &z, &piece);

    pieces[piece] = i;
    trans[piece] = tran;
    xs[piece] = x;
    ys[piece] = y;
    zs[piece] = z;
  }

  for (unsigned int i = 0; i < piecenumber; i++) {
    if (pieces[i] >= count)
      assembly->addNonPlacement();
    else
      assembly->addPlacement(trans[i], xs[i], ys[i], zs[i]);
  }

  return assembly;
}

std::unique_ptr<assembly_c> assembler_0_c::getAssembly(void) {
  return buildAssembly(rows.data(), pos);
}

void assembler_0_c::checkForTransformedAssemblies(unsigned int pivot, std::unique_ptr<mirrorInfo_c> mir) {
  avoidTransformedAssemblies = true;
  avoidTransformedPivot = pivot;
  avoidTransformedMirror = std::move(mir);
}

/* Delivers one raw solution. didConsume reports whether the solution is
 * done with -- delivered or filtered as a rotation -- as opposed to
 * declined because the stop arrived first: a declined solution must be
 * found again on resume (see assemblerWorker_c::solution()). Returns false
 * when the callback asked to stop.
 */
bool assembler_0_c::handleSolution(const unsigned int *row_nodes, unsigned int count, std::stop_token stopTok, bool & didConsume) {
  didConsume = false;
  if (getCallback()) {
    std::unique_ptr<assembly_c> assembly = buildAssembly(row_nodes, count);

    if (avoidTransformedAssemblies &&
        assembly->smallerRotationExists(problem, avoidTransformedPivot, avoidTransformedMirror.get(), complete)) {
      didConsume = true;
      return true;
    }

    {
      std::lock_guard<std::mutex> lock(callbackMutex);
      if (stopTok.stop_requested())
        return false;
      didConsume = true;
      if (!getCallback()->assembly(std::move(assembly))) {
        stop();
        return false;
      }
    }
  }
  return true;
}

void assembler_0_c::solution(std::stop_token stop) {
  bool didConsume = false;
  handleSolution(rows.data(), pos, stop, didConsume);
}

/* to understand this function you need to first completely understand the
 * dancing link algorithm.
 */
void assembler_0_c::iterativeMultiSearch(void) {

  // A serial run supersedes any saved parallel remainder: it resumes from
  // pos/rows, so stale pool tasks must not linger into a later save().
  // pos/rows are a valid resume point from here on; a parallelInterrupted
  // flag left over from an earlier parallel stop would make save() mark
  // them unresumable.
  parallelTasks.clear();
  parallelInterrupted = false;
  remainderEngine = ENGINE_NONE;

  // Snapshot of the run token: assemble()/debug_step() refreshed the source
  // before calling here, so this stays valid for the whole serial search.
  std::stop_token runTok = currentRunToken();
  running.store(true, std::memory_order_relaxed);

  // this variable is used to store if we continue with our loop over
  // the rows or have finished
  bool cont;

  while (!runTok.stop_requested()) {

    // we have finished if pos negative (or greater than piecenumber because of the
    // overflow
    if (pos > piecenumber)
      break;

    // check, if all pieces are placed and all voxels are filled
    // careful here: the stop flag is also modified by another thread, but
    // the stop_token check is a single atomic load, so no update can be lost
    if (!right[0])
      solution(runTok);

    // the debugger
    if (debug) {
      if (debug_loops <= 0)
        break;

      debug_loops --;
    }

    // if all pieces are placed we can not go on even if there are
    // more columns that need attention, all of them should be
    // empty any ways, so we backtrack once and continue there
    if (pos == piecenumber)
      pos--;

    cont = false;
    iterations.store(iterations.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);

    if (!rows[pos]) {

      // start with a new column

      /* search the best column for the next recursion step
       *
       * we actually do a bit more:
       * we find the column with the smallest column count. This column is selected
       * from the piece columns and the normal result columns
       *
       * we also look for piece and result columns that have a count of 0 that value
       * will lead to impossible arrangements
       */
      unsigned int c = right[0];
      unsigned int s = colCount[c];

      if (s) {
        unsigned int j = right[c];

        while (j) {

          if (colCount[j] < s) {
            c = j;
            s = colCount[c];

            if (!s)
              break;
          }

          j = right[j];
        }
      }

      // now check for the holes, only the variable columns can be unfillable
      // but there must not be more unfillable voxels than there are holes
      // if that is the case we can backtrack
      // sometimes this doesn't help much, but it also seems like
      // it doesn't cost a lot of time, so let's keep it in for the moment
      if (s) {
        unsigned int currentHoles = holes;
        unsigned int j = right[varivoxelEnd];

        while (j != varivoxelEnd) {
          if (colCount[j] == 0) {
            if (currentHoles == 0) {
              s = 0;
              break;
            }
            currentHoles--;
          }
          j = right[j];
        }
      }

      if (s) {

        // we have found a valid column, start a search
        columns[pos] = c;
        rows[pos] = down(columns[pos]);

        cont = true;
      }

      // found no fitting row, or column with zero count
      if (!cont) {
        rows[pos] = 0;
        pos--;
        continue;
      }

      cover(columns[pos]);

    } else {

      // continue on a column we have already started, this is inside the loop in the
      // recursive function, after we return from the recursive call
      // we uncover our row, find the next one and continue, if there is a new row
      uncover_row(rows[pos]);
      cont = true;

      rows[pos] = down(rows[pos]);

      if (rows[pos] == columns[pos])
        cont = false;
    }

    // check, if we continue, if we have found a new row to cover
    if (cont) {

      // cover the row
      cover_row(rows[pos]);

      pos++;

    } else {

      // OK finished this column, uncover it and backtrack
      uncover(columns[pos]);

      rows[pos] = 0;
      pos--;
    }
  }

  /* debug-stepping can break out via debug_loops <= 0 with the stop token
   * still unfired; that is not completion, so gate the flag on !debug too.
   */
  if (!runTok.stop_requested() && !debug)
    searchComplete.store(true, std::memory_order_relaxed);

  running.store(false, std::memory_order_relaxed);
}

class assemblerWorker_c {
public:
  assembler_0_c & parent;
  // The run's stop token, fixed for the worker's lifetime (captured once at
  // construction, so the hot search loop never touches shared run state).
  std::stop_token stop;
  std::vector<unsigned int> left;
  std::vector<unsigned int> right;
  std::vector<unsigned int> upDown;
  std::vector<unsigned int> colCount;
  std::vector<unsigned int> rows;
  std::vector<unsigned int> columns;
  unsigned int pos;
  unsigned long local_iterations;
  unsigned long flushed_iterations;
  /* high-water mark of localFraction() within the current task; see the
   * publication site in searchSubtree()
   */
  float publishedFraction;
  /* Full paths of solutions found but never delivered (stop arrived
   * between discovery and delivery -- see solution()). Emitted as exact
   * single-leaf pieces by divideRemainder; cleared per task at
   * searchSubtree() entry so nothing leaks across tasks.
   */
  std::vector<assembler_0_c::SubtreeTask> missedTasks;
  /* cut[d] != 0: the rows after rows[d] at level d were donated (see
   * donate()), so the advance at d treats the level as exhausted. Cleared
   * whenever level d is dropped, so a fresh pick at d starts uncut.
   */
  std::vector<unsigned char> cut;
  /* Donation target (null: no donation, e.g. the splitter-only copy in the
   * SIMD loop). Donations poll every donatePollInterval nodes: a relaxed
   * flag load on demand, or unconditionally when forced (tests).
   */
  AssemblyTaskPool<assembler_0_c::SubtreeTask> * donatePool = nullptr;
  unsigned long donatePollInterval = 128;
  bool donateForced = false;
  unsigned long nextDonatePoll = 0;

  assemblerWorker_c(assembler_0_c & p, std::stop_token stop_ = {}) :
    parent(p),
    stop(stop_),
    left(p.left),
    right(p.right),
    upDown(p.upDown),
    colCount(p.colCount),
    rows(p.piecenumber, 0),
    columns(p.piecenumber, 0),
    pos(0),
    local_iterations(0),
    flushed_iterations(0),
    publishedFraction(0),
    cut(p.piecenumber, 0)
  {}

  /* Route donations to pool (see donate()). every > 0 forces one every
   * that many nodes, hungry siblings or not (tests); 0 polls the pool's
   * hunger flag every 128 nodes -- a relaxed load, about a hundred times
   * rarer than the iteration flush right next to it.
   */
  void enableDonation(AssemblyTaskPool<assembler_0_c::SubtreeTask> & pool,
                      unsigned long every) {
    donatePool = &pool;
    donateForced = every > 0;
    donatePollInterval = every > 0 ? every : 128;
  }

  void cover(unsigned int col) {
    right[left[col]] = right[col];
    left[right[col]] = left[col];

    for (unsigned int i = down(col); i != col; i = down(i)) {
      for (unsigned int j = right[i]; j != i; j = right[j]) {
        unsigned int u = up(j);
        unsigned int d = down(j);

        up(d) = u;
        down(u) = d;

        colCount[colCount[j]]--;
      }
    }
  }

  void uncover(unsigned int col) {
    for (unsigned int i = up(col); i != col; i = up(i)) {
      for (unsigned int j = left[i]; j != i; j = left[j]) {
        colCount[colCount[j]]++;

        up(down(j)) = j;
        down(up(j)) = j;
      }
    }

    left[right[col]] = col;
    right[left[col]] = col;
  }

  void cover_row(unsigned int r) {
    for (unsigned int j = right[r]; j != r; j = right[j])
      cover(colCount[j]);
  }

  void uncover_row(unsigned int r) {
    for (unsigned int j = left[r]; j != r; j = left[j])
      uncover(colCount[j]);
  }

  void solution() {
    flushIterations();
    bool didConsume = false;
    parent.handleSolution(rows.data(), pos, stop, didConsume);
    if (!didConsume && parent.getCallback()) {
      // Stop arrived between discovery and delivery: found but never
      // handed over (and uncounted). Divide drops searched regions, so
      // record its full path here; divideRemainder re-emits it as an
      // exact single-leaf piece.
      // (Without a callback there is nobody to deliver to; skip it.)
      assembler_0_c::SubtreeTask missed;
      for (unsigned int i = 0; i < pos && i < parent.piecenumber; i++)
        missed.prefix.push_back({columns[i], rows[i]});
      missedTasks.push_back(std::move(missed));
    }
  }

  /* Cursor naming, shared by every producer of cursor tasks (donate,
   * divideRemainder, splitPrefix's rest) and by splitPrefix, which consumes
   * them: cursorSkip counts rows from the column head, on resume state
   * rebuilt identically from the prefix, and skips through the row it
   * names. Producers and consumer must count the same way, or rows are
   * lost or searched twice.
   *
   * Index of row r below the column head of col; UINT_MAX when r is not
   * linked in col.
   */
  unsigned int rowIndexInColumn(unsigned int col, unsigned int r) const {
    unsigned int m = 0;
    for (unsigned int x = down(col); x != col; x = down(x), m++)
      if (x == r)
        return m;
    return UINT_MAX;
  }

  /* The row of col a cursor with this skip resumes at (see
   * rowIndexInColumn); col itself when the cursor is exhausted. */
  unsigned int rowAtIndex(unsigned int col, unsigned int skip) const {
    unsigned int r = down(col);
    for (unsigned int i = 0; i < skip && r != col; i++)
      r = down(r);
    return r;
  }

  /* task's prefix extended by the live placements at levels
   * prefix..d-1: the above-path of level d, which every piece carved out
   * of level d (cursor, singleton) starts from. */
  std::vector<assembler_0_c::PrefixStep> abovePathPrefix(
      const assembler_0_c::SubtreeTask & task, unsigned int d) const {
    std::vector<assembler_0_c::PrefixStep> prefix = task.prefix;
    for (unsigned int k = static_cast<unsigned int>(task.prefix.size()); k < d; k++)
      prefix.push_back({columns[k], rows[k]});
    return prefix;
  }

  /* Split on demand: hand the unexplored rows of the shallowest open level
   * of the running task to the pool, as one cursor task.
   *
   * Level d (prefix <= d <= pos, rows[d] placed) is open when it is not cut
   * yet and its current row has a successor in the column. The rows after
   * the current one are exactly the level's unexplored alternatives under
   * the live above-path, and a cursor over the above-path names them: the
   * cursor's resume state (root + the same prefix covers, splitPrefix) is
   * rebuilt identically, and a placed level's column list never changes
   * while deeper levels search (cover(col) unlinked its rows from every
   * other column, so no deeper cover reaches them), so counting from the
   * column head here and there agrees. Setting cut[d] makes the donor's
   * advance at d stop after the current row's subtree -- the donor keeps
   * what it is in, the pool gets everything after it, nothing is shared.
   *
   * The shallowest level is the biggest unexplored chunk the task holds
   * (it contains every deeper alternative's whole level structure), which
   * keeps donations rare: a hungry sibling gets a large piece, and the
   * donor's own territory shrinks towards its current path. Levels within
   * two of the bottom are not donated: their subtrees cost about as much
   * as the handoff.
   *
   * Progress: the piece takes its structural estimate of the task's share,
   * weight(above-path) x remaining / colCount -- the same nested estimate
   * localFraction() walks, so at that level the donor's explored part
   * (rows before the current one, plus a fraction of the current one) and
   * the donated part add up to at most the level's weight. The donor's
   * task.share drops by the same amount (what finishTask() folds in), but
   * its published slot keeps the task's original share: slot share x
   * localFraction() is the absolute share explored so far, which a
   * donation does not change -- so progress stays monotone (no transient
   * dip while the piece is queued), and the donor's in-flight term stays
   * below its reduced share, so the handoff at finish never over-reports.
   * A placed level's colCount is stable while it is placed (see above),
   * so the estimate and the later walks agree.
   *
   * Unless forced (tests), the donor first claims the pool's hunger signal,
   * so one waiter draws one donation, not one from every busy worker that
   * polls before the first push lands. Returns false when no level is open
   * or another donor won the claim.
   */
  bool donate(assembler_0_c::SubtreeTask & task, float rootShare) {
    const unsigned int prefixDepth = static_cast<unsigned int>(task.prefix.size());
    if (parent.piecenumber < 3)
      return false;
    const unsigned int last = std::min(pos, parent.piecenumber - 3);
    float weight = rootShare;
    for (unsigned int d = prefixDepth; d <= last; d++) {
      const unsigned int col = columns[d];
      const unsigned int cur = rows[d];
      if (!cur)
        return false;
      const unsigned int n = colCount[col];
      if (!cut[d] && down(cur) != col) {
        if (!donateForced && !donatePool->claim_work())
          return false;
        unsigned int remaining = 0;
        for (unsigned int r = down(cur); r != col; r = down(r))
          remaining++;

        assembler_0_c::SubtreeTask piece;
        piece.prefix = abovePathPrefix(task, d);
        piece.cursorCol = col;
        piece.cursorSkip = rowIndexInColumn(col, cur) + 1;
        // Exact arithmetic keeps the estimate below what the donor holds
        // (earlier donations were the shallower levels' later rows); the
        // clamp only absorbs float rounding.
        const float share = n ? weight * static_cast<float>(remaining) / static_cast<float>(n) : 0.0f;
        piece.share = std::min(share, task.share);
        task.share -= piece.share;
        cut[d] = 1;
        parent.donations.fetch_add(1, std::memory_order_relaxed);
        parent.totalTasks.fetch_add(1, std::memory_order_relaxed);
        std::vector<assembler_0_c::SubtreeTask> one;
        one.push_back(std::move(piece));
        donatePool->push_tasks(std::move(one));
        return true;
      }
      if (n)
        weight /= static_cast<float>(n);
    }
    return false;
  }

  /* fraction of this worker's own subtree explored so far, in [0,1].
   *
   * This is the nested-path walk getFinished() uses for the single-threaded
   * search, stopping at the task's prefix depth instead of at the root: the
   * prefix placements are fixed for the whole task, so everything below them
   * is what this worker still has to get through. The up()/down() macros
   * resolve against this worker's own upDown vector, and rows/columns/colCount
   * are worker-private too, so nothing here touches shared state.
   */
  float localFraction(unsigned int prefixDepth) const {
    float erg = 0;
    for (int i = static_cast<int>(pos) - 1; i >= static_cast<int>(prefixDepth); i--) {
      unsigned int r = rows[i];
      unsigned int l = colCount[columns[i]];

      while (l && r && (r != down(columns[i]))) {
        erg += 1;
        r = up(r);
        l--;
      }

      /* a zero count would put inf/NaN into the fraction, which clamps to 1.0
       * and reads as a finished search while workers are still live
       */
      if (colCount[columns[i]] == 0)
        continue;
      erg /= colCount[columns[i]];
    }
    return erg;
  }

  /* Returns true when the loop itself ran out of territory (retreated
   * below the prefix, or overflowed past the last piece) -- the task is
   * fully searched, whatever the stop token says afterwards. False means
   * the stop interrupted it: the live state is the resume point (see
   * divideRemainder). Callers must use this rather than re-reading the
   * token: a stop landing after exhaustion would otherwise read as an
   * interruption of an already-unwound (or unwinding) task.
   */
  bool searchSubtree(assembler_0_c::SubtreeTask & task,
                     assembler_0_c::WorkerProgress * slot) {
    unsigned int prefixDepth = task.prefix.size();
    // share the task started with: donations estimate against this
    // structural weight, not against what earlier donations left over
    const float rootShare = task.share;

    /* the ratchet is per task -- the caller has already reset the slot's
     * published fraction to 0 for this task
     */
    publishedFraction = 0;
    missedTasks.clear();
    std::fill(cut.begin(), cut.end(), 0);
    nextDonatePoll = local_iterations + donatePollInterval;

    // Apply prefix
    for (unsigned int d = 0; d < prefixDepth; d++) {
      cover(task.prefix[d].col);
      cover_row(task.prefix[d].row);
      columns[d] = task.prefix[d].col;
      rows[d] = task.prefix[d].row;
    }
    pos = prefixDepth;

    bool cont;
    bool exhausted = false;

    // Exhaustion is checked before the stop: with an empty prefix a
    // retreat past level 0 wraps pos to UINT_MAX and is only recognized
    // here, so testing the token first would misread a finished task as
    // an interrupted one.
    for (;;) {
      if (pos < prefixDepth || pos > parent.piecenumber) {
        exhausted = true;
        break;
      }
      if (stop.stop_requested())
        break;

      if (!right[0])
        solution();

      if (pos == parent.piecenumber) {
        pos--;
        if (pos < prefixDepth) {
          exhausted = true;
          break;
        }
      }

      cont = false;
      local_iterations++;
      if ((local_iterations - flushed_iterations) >= 128) {
        parent.iterations.fetch_add(local_iterations - flushed_iterations, std::memory_order_relaxed);
        flushed_iterations = local_iterations;
      }
      if (local_iterations >= nextDonatePoll && donatePool) {
        nextDonatePoll = local_iterations + donatePollInterval;
        if (donateForced || donatePool->wants_work())
          donate(task, rootShare);
      }

      /* The walk is O(depth x colCount), so it must not run per node.
       *
       * The interval was measured rather than guessed. On Burr-Glar (4
       * workers, the DLX path) the search runs at essentially the same node
       * rate with publication compiled out, publishing every 64K nodes, and
       * publishing every 4K nodes -- all three sit inside the run-to-run
       * noise. 4K is kept because 64K is too coarse to be useful here: at
       * ~180K nodes/s per worker it would publish once every ~360 ms, longer
       * than the 250 ms GUI refresh, so the bar would still visibly step. 4K
       * publishes roughly every 23 ms per worker.
       *
       * Only the high-water mark is published. The nested-path estimate is
       * not itself monotone -- the covering and uncovering of columns moves
       * the colCount denominators under it, and there is a window during a
       * backtrack where the level just left has been dropped from the sum but
       * the level above it has not yet been advanced -- so publishing it raw
       * makes the bar visibly run backwards. Progress that goes backwards is
       * the defect this whole change exists to remove, so the estimate is
       * ratcheted.
       */
      if ((local_iterations & 0xFFF) == 0 && slot) {
        float f = localFraction(prefixDepth);
        if (f > publishedFraction) {
          publishedFraction = f;
          slot->fraction.store(f, std::memory_order_relaxed);
        }
      }

      if (!rows[pos]) {
        // Pick best column
        unsigned int c = right[0];
        unsigned int s = colCount[c];

        if (s) {
          unsigned int j = right[c];
          while (j) {
            if (colCount[j] < s) {
              c = j;
              s = colCount[c];
              if (!s) break;
            }
            j = right[j];
          }
        }

        // Check holes in variable voxels
        if (s) {
          unsigned int currentHoles = parent.holes;
          unsigned int j = right[parent.varivoxelEnd];
          while (j != parent.varivoxelEnd) {
            if (colCount[j] == 0) {
              if (currentHoles == 0) {
                s = 0;
                break;
              }
              currentHoles--;
            }
            j = right[j];
          }
        }

        if (s) {
          columns[pos] = c;
          rows[pos] = down(columns[pos]);
          cont = true;
        }

        if (!cont) {
          rows[pos] = 0;
          pos--;
          if (pos < prefixDepth) {
            exhausted = true;
            break;
          }
          continue;
        }

        cover(columns[pos]);
      } else {
        uncover_row(rows[pos]);
        cont = true;
        rows[pos] = down(rows[pos]);
        // a cut level's later rows belong to a donated task
        if (rows[pos] == columns[pos] || cut[pos])
          cont = false;
      }

      if (cont) {
        cover_row(rows[pos]);
        pos++;
      } else {
        uncover(columns[pos]);
        rows[pos] = 0;
        cut[pos] = 0;
        pos--;
        if (pos < prefixDepth) {
          exhausted = true;
          break;
        }
      }
    }

    // Unwind prefix if search finished normally (an interrupted search
    // keeps its live state for divideRemainder)
    if (exhausted) {
      for (int d = (int)prefixDepth - 1; d >= 0; d--) {
        uncover_row(task.prefix[d].row);
        uncover(task.prefix[d].col);
        rows[d] = 0;
        columns[d] = 0;
      }
      pos = 0;
    }
    return exhausted;
  }

  /* Remainder of an interrupted task as exact continuation tasks.
   *
   * Runs on the stopped worker's live state: rows[]/columns[]/pos and the
   * matrix links are exactly as the search left them (single-use workers
   * never touch another task after a stop, so no cleanup is needed
   * afterwards, and nothing here mutates shared state).
   *
   * The unexplored territory at a stop is, by the DLX DFS shape, two
   * disjoint sets: for every placed level, the rows after the current one
   * (each becomes a task over the live above-path plus that row, with the
   * full subtree below it searched fresh); plus the completions of the
   * live path itself below the deepest placed level, when the worker
   * arrived by descent and had not gone below yet (one "continue" task
   * over the full live path). A retreat landing has those levels fully
   * searched already, so no continue (it would duplicate).
   * The already-searched regions (everything before the current path) are
   * simply absent: dropped, not stored. That is the whole point -- the
   * remainder holds future work only, so every piece starts with a clean
   * skip and resume re-walks nothing.
   *
   * The continue task is emitted only for a fresh descent; retreat
   * landings have the levels below fully searched -- delivered or
   * missed-recorded (see solution()) -- so emitting it again would
   * duplicate. Solutions found but never delivered (stop arriving between
   * discovery and delivery) are re-emitted from the recorded paths (see
   * takeMissed): they lie on dropped current paths no divergent piece can
   * contain. Fat levels chunk like runtime splits (up to K singletons
   * plus a cursor rest, same splitFanout rule), so the piece count stays
   * bounded; the cursor resumes through the existing cursor machinery
   * with no format changes.
   *
   * rows[d] != 0 means covered at every level: descent covers before
   * advancing, retreat zeroes before dropping below. The stop is only
   * noticed at the loop head, so exactly two states reach here: a fresh
   * descent (rows[pos] == 0, covered range [prefix, pos-1]) or a retreat
   * landing (covered range [prefix, pos]). A solution's own level is
   * never a loop-head state: the pos == piecenumber branch retreats and
   * advances in the same iteration. The covered range is scanned, not
   * assumed from pos.
   *
   * Only for interrupted tasks (searchSubtree returned false); exhausted
   * ones go through takeMissed. requeueWhole asks the caller to requeue
   * the task itself: exact for an untouched task (nothing placed past the
   * prefix, nothing delivered). Its other uses are invariant breaches
   * (asserted): requeueing whole there re-delivers, so release builds
   * degrade to duplicates rather than losses. An empty result WITHOUT
   * requeueWhole is a real outcome, not a fallback: a retreat landing on
   * the last row of every covered level has nothing left -- the task was
   * fully searched and must count as done.
   */
  std::vector<assembler_0_c::SubtreeTask> divideRemainder(
      const assembler_0_c::SubtreeTask & task, bool & requeueWhole) {
    const unsigned int prefixDepth = static_cast<unsigned int>(task.prefix.size());
    requeueWhole = false;

    // searchSubtree reports exhaustion itself (checked before the stop),
    // so an interrupted loop always sits inside the territory.
    bt_assert(pos >= prefixDepth && pos <= parent.piecenumber);
    if (pos < prefixDepth || pos > parent.piecenumber) {
      requeueWhole = true;
      return {};
    }

    // Covered range scan: the topmost covered level, contiguous down to
    // the prefix. Anything else contradicts the descent/retreat pairing
    // above and falls back rather than risk a misread range.
    unsigned int topCovered = prefixDepth;
    bool anyCovered = false;
    for (unsigned int d = prefixDepth; d < parent.piecenumber; d++) {
      if (!rows[d])
        break;
      topCovered = d;
      anyCovered = true;
    }
    if (!anyCovered) {
      // Nothing placed past the prefix (untouched, or stopped exactly at
      // the prefix before its first pick): nothing searched, nothing
      // delivered, so the whole task is the exact remainder.
      // A donation needs a placed level, which stays placed until the
      // task exhausts, so an untouched task never donated anything.
      bt_assert(missedTasks.empty());
      bt_assert(std::find(cut.begin(), cut.end(), 1) == cut.end());
      requeueWhole = true;
      return {};
    }
    // Nothing covered above the range, and the range ends at pos (retreat
    // landing) or just below it (fresh descent).
    for (unsigned int d = topCovered + 1; d < parent.piecenumber; d++)
      bt_assert(rows[d] == 0);
    bt_assert(topCovered == pos || topCovered + 1 == pos);

    std::vector<assembler_0_c::SubtreeTask> out;
    // Continue task, only for a fresh descent (topCovered + 1 == pos): the
    // worker had not descended below the deepest placed level, so this
    // whole subtree is unexplored. A retreat landing (topCovered == pos)
    // has the levels below fully searched -- delivered live or
    // missed-recorded -- so emitting it would re-search them into
    // duplicates. Captured before the unwind below zeroes rows[].
    if (topCovered + 1 == pos) {
      // Continue task over the full live path.
      assembler_0_c::SubtreeTask cont;
      cont.prefix = abovePathPrefix(task, topCovered + 1);
      out.push_back(std::move(cont));
    }
    // Unwind deepest-first: when level d is enumerated, all levels below
    // it are already uncovered, so its column links hold exactly the rows
    // compatible with the above-path. The current row and everything
    // before it were searched -- only rows after it become pieces.
    // rows[d] != 0 for d <= topCovered is established by the scan above.
    for (unsigned int top = topCovered + 1; top-- > prefixDepth;) {
      const unsigned int d = top;
      bt_assert(rows[d] != 0);
      if (!rows[d]) {
        requeueWhole = true;
        return {};
      }
      const unsigned int col = columns[d];
      const unsigned int cur = rows[d];

      // Uncover the current row BEFORE enumerating: its placement masks
      // rows that conflict only with it but are valid alternatives under
      // the above-path. Deeper levels are already uncovered (descending)
      // and the above-path stays covered, so the chain now holds exactly
      // the compatible set, including the current row itself (a placed
      // row's own column node is never unlinked: the column choice strips
      // its other nodes first, shielding it from its own row covers, and
      // no deeper cover can touch it). Enumerating before this uncover
      // silently drops those masked alternatives.
      uncover_row(cur);
      // Index of the current row from the column head, then up to K
      // following rows; anything beyond becomes one cursor rest (same
      // K-singletons-plus-rest rule as splitPrefix).
      const unsigned int m = rowIndexInColumn(col, cur);
      // The current row is always linked in its own column (see above).
      bt_assert(m != UINT_MAX);
      if (m == UINT_MAX) {
        requeueWhole = true;
        return {};
      }
      unsigned int r = cur;
      // A cut level's later rows were donated (see donate()): already
      // someone else's, so only the current path's pieces remain here.
      std::vector<unsigned int> following;
      if (!cut[d])
        for (r = down(r); r != col && following.size() < parent.splitFanout; r = down(r))
          following.push_back(r);
      const bool exhaustedLevel = cut[d] || (r == col);
      const auto above = abovePathPrefix(task, d);
      for (unsigned int alt : following) {
        assembler_0_c::SubtreeTask piece;
        piece.prefix = above;
        piece.prefix.push_back({col, alt});
        out.push_back(std::move(piece));
      }
      if (!exhaustedLevel) {
        assembler_0_c::SubtreeTask rest;
        rest.prefix = above;
        rest.cursorCol = col;
        // Skip through the current row and the emitted singletons
        // (absolute indices 0..m+s), counted from the column head on
        // the identically rebuilt resume state -- the same convention
        // as splitPrefix.
        rest.cursorSkip = m + static_cast<unsigned int>(following.size()) + 1;
        out.push_back(std::move(rest));
      }

      // The row covers are already undone above; now drop the level
      // itself before moving up.
      uncover(col);
      rows[d] = 0;
      columns[d] = 0;
      cut[d] = 0;
    }

    // Missed solutions (found but never delivered -- see solution()):
    // they lie on dropped current paths, so no divergent piece can
    // contain them.
    for (auto & missed : missedTasks)
      out.push_back(std::move(missed));
    missedTasks.clear();

    assignShares(out, task.share);
    return out;
  }

  /* Remainder of an exhausted task: only its missed solutions (found but
   * never delivered because the stop arrived between discovery and
   * delivery -- see solution()), each as an exact single-leaf piece. The
   * rest of the territory was searched with everything else delivered.
   * Usually empty; non-empty only when a stop lands inside the very last
   * deliveries of a task, e.g. on a single-leaf task (a re-emitted missed
   * piece, or a continue task over a full path) -- whose territory is
   * nothing but that leaf.
   */
  std::vector<assembler_0_c::SubtreeTask> takeMissed(
      const assembler_0_c::SubtreeTask & task) {
    std::vector<assembler_0_c::SubtreeTask> out = std::move(missedTasks);
    missedTasks.clear();
    assignShares(out, task.share);
    return out;
  }

  // Shares divide evenly over a remainder (progress accounting only); the
  // caller keeps the task-count total consistent with the transfer.
  static void assignShares(std::vector<assembler_0_c::SubtreeTask> & pieces,
                           float share) {
    if (pieces.empty())
      return;
    const float unitShare = share / static_cast<float>(pieces.size());
    for (auto & piece : pieces)
      piece.share = unitShare;
  }

  void flushIterations() {
    unsigned long unflushed = local_iterations - flushed_iterations;
    if (unflushed > 0) {
      parent.iterations.fetch_add(unflushed, std::memory_order_relaxed);
      flushed_iterations = local_iterations;
    }
  }

  /**
   * Split a queued subtree task one level deeper so starving siblings can
   * steal the children.
   *
   * Runs on this worker's PRIVATE matrix copy, which must be at root state
   * (true right after construction and after every searchSubtree() that ran
   * to completion without abort -- searchSubtree unwinds its prefix there).
   * The parent's live matrix is never touched, so this may run concurrently
   * with other workers searching.
   *
   * Returns the child tasks P u {(c,r)} for the same MRV column choice the
   * search would make, or an empty vector when splitting buys nothing
   * (dead end, already a solution, depth cap reached, or a single child).
   * Callers execute the original task when the result has fewer than 2
   * entries; all returned children must be executed or pushed, never dropped.
   */
  std::vector<assembler_0_c::SubtreeTask> splitPrefix(
      const assembler_0_c::SubtreeTask & task) {
    std::vector<assembler_0_c::SubtreeTask> children;

    // SIMD rests are never split (shouldSplit): their skip indexes the
    // SIMD frame's candidate order, which this DLX enumeration does not
    // share. Checked in release too: splitting one would search it whole.
    if (task.restSkip != 0)
      bt_te("assembler_0_c::splitPrefix: SIMD rest task");

    unsigned int maxPrefix =
        parent.piecenumber > 1 ? parent.piecenumber - 1 : 1u;
    // Plain tasks past the cap run whole (the caller searches the
    // original); cursors bypass it -- a cursor has no whole to run
    // (searching its prefix would duplicate the emitted singletons), so
    // refusing to split one can only lose its remaining rows.
    if (task.prefix.size() >= maxPrefix && !task.hasCursor())
      return children;

    for (const auto &step : task.prefix) {
      cover(step.col);
      cover_row(step.row);
    }

    // structuring the unwind as a lambda keeps every early return balanced:
    // all applied prefix covers are always undone before returning.
    auto unwind = [&] {
      for (int d = (int)task.prefix.size() - 1; d >= 0; d--) {
        uncover_row(task.prefix[d].row);
        uncover(task.prefix[d].col);
      }
    };

    if (!right[0]) {
      unwind();
      return children;
    }

    // Same MRV column choice as searchSubtree, holes check included
    // (plain tasks; cursor tasks re-enumerate their fixed column below).
    unsigned int c = right[0];
    unsigned int skip = 0;
    unsigned int s;
    if (task.hasCursor()) {
      // Fixed column and skip: re-enumerate the same rows the cursor was
      // created from. The scratch state is rebuilt identically every time
      // (same base, same prefix covers), so the links -- and the cursor --
      // are still valid. Selection (s = 1) only enters the enumeration
      // below; MRV scan and holes check are skipped.
      c = task.cursorCol;
      skip = task.cursorSkip;
      s = 1;
    } else {
      s = colCount[c];
    }

    if (s && !task.hasCursor()) {
      unsigned int j = right[c];
      while (j) {
        if (colCount[j] < s) {
          c = j;
          s = colCount[c];
          if (!s) break;
        }
        j = right[j];
      }
    }

    if (s && !task.hasCursor()) {
      unsigned int currentHoles = parent.holes;
      unsigned int j = right[parent.varivoxelEnd];
      while (j != parent.varivoxelEnd) {
        if (colCount[j] == 0) {
          if (currentHoles == 0) {
            s = 0;
            break;
          }
          currentHoles--;
        }
        j = right[j];
      }
    }

    if (s) {
      /* Enumerate the column's rows, skipping the cursor's share on rest
       * tasks. At most splitFanout units leave here: singletons plus, when
       * rows remain, one cursor for the rest. A column with at most K rows
       * yields exactly one task per row.
       */
      unsigned int r = rowAtIndex(c, skip);
      std::vector<unsigned int> rows;
      while (r != c && rows.size() < parent.splitFanout) {
        rows.push_back(r);
        r = down(r);
      }
      const bool exhausted = (r == c);

      if (rows.empty()) {
        // Dead column, or an exhausted cursor (defensive only -- cursors
        // are created with rows remaining, on an identically rebuilt
        // matrix). Either way there is nothing to split off.
        unwind();
        return children;
      }
      if (!task.hasCursor() && rows.size() < 2 && exhausted) {
        // Single live child, plain task: splitting is pointless; the
        // worker searches the task itself. A cursor in the
        // same shape must still emit its one row -- dropping it would lose
        // the subtree -- so cursors never take this path.
        unwind();
        return children;
      }

      /* Share is divided evenly over the emitted units (singletons plus
       * the rest cursor, if any), so the children's shares sum back to
       * task.share exactly -- the split conserves share the same way the
       * original generation does. (Progress accounting only.)
       */
      const float unitShare = task.share /
          static_cast<float>(rows.size() + (exhausted ? 0u : 1u));
      for (unsigned int row : rows) {
        assembler_0_c::SubtreeTask child = task;
        child.prefix.push_back({c, row});
        child.cursorCol = 0;
        child.cursorSkip = 0;
        child.share = unitShare;
        children.push_back(std::move(child));
      }
      if (!exhausted) {
        assembler_0_c::SubtreeTask rest = task;
        rest.cursorCol = c;
        rest.cursorSkip = skip + static_cast<unsigned int>(rows.size());
        rest.share = unitShare;
        children.push_back(std::move(rest));
      }
    }

    unwind();
    return children;
  }
};

std::vector<assembler_0_c::PrefixStep> assembler_0_c::prefixFromNodes(
    const SubtreeTask & task, const std::vector<unsigned int> & nodes) const {
  // Checked in release too: a shorter prefix would drop part of the
  // task's own and widen its territory.
  if (nodes.size() < task.prefix.size())
    bt_te("assembler_0_c::prefixFromNodes: nodes shorter than the task's prefix");
  std::vector<PrefixStep> prefix;
  prefix.reserve(nodes.size());
  for (size_t k = 0; k < nodes.size(); k++) {
    if (k < task.prefix.size())
      prefix.push_back(task.prefix[k]);
    else
      prefix.push_back({colCount[nodes[k]], nodes[k]});
  }
  return prefix;
}

void assembler_0_c::generateSubtreeTasks(
    std::vector<SubtreeTask> & tasks,
    unsigned int targetTasks,
    unsigned int maxDepth,
    float & prunedShare)
{
  tasks.clear();
  prunedShare = 0.0f;
  SubtreeTask rootTask;
  rootTask.share = 1.0f;
  tasks.push_back(rootTask);

  unsigned int currentDepth = 0;

  while (tasks.size() < targetTasks && currentDepth < maxDepth) {
    std::vector<SubtreeTask> nextTasks;
    bool anyExpanded = false;

    for (const auto & t : tasks) {
      if (t.prefix.size() >= (piecenumber > 1 ? piecenumber - 1 : 1u)) {
        nextTasks.push_back(t);
        continue;
      }

      for (const auto & step : t.prefix) {
        cover(step.col);
        cover_row(step.row);
      }

      if (!right[0]) {
        nextTasks.push_back(t);
        for (int d = (int)t.prefix.size() - 1; d >= 0; d--) {
          uncover_row(t.prefix[d].row);
          uncover(t.prefix[d].col);
        }
        continue;
      }

      unsigned int c = right[0];
      unsigned int s = colCount[c];

      if (s) {
        unsigned int j = right[c];
        while (j) {
          if (colCount[j] < s) {
            c = j;
            s = colCount[c];
            if (!s) break;
          }
          j = right[j];
        }
      }

      if (s) {
        unsigned int currentHoles = holes;
        unsigned int j = right[varivoxelEnd];
        while (j != varivoxelEnd) {
          if (colCount[j] == 0) {
            if (currentHoles == 0) {
              s = 0;
              break;
            }
            currentHoles--;
          }
          j = right[j];
        }
      }

      /* s == 0 here (a dead column, or the holes prune above) means this
       * subtree has zero solutions: there is no work left to do in it, so it
       * is genuinely finished, not merely unrepresented. Its share must still
       * be accounted for, or the shares of the tasks that do get published
       * would sum to less than 1 and getFinished() would top out below 100%
       * for the whole run. Fold it into prunedShare instead of dropping it,
       * so the caller can seed completedShare with it.
       */
      if (s == 0)
        prunedShare += t.share;

      if (s > 0) {
        anyExpanded = true;
        for (unsigned int r = down(c); r != c; r = down(r)) {
          SubtreeTask child = t;
          child.prefix.push_back({ c, r });

          /* colCount is stable at this point: every ancestor column in
           * child.prefix was covered in prefix order by the loop above, so
           * cover() has frozen each one's colCount at the row count it had
           * when that step was chosen (cover() only decrements OTHER,
           * still-active columns' counts, never the one it just removed).
           * The just-chosen column c has not been covered yet, so colCount[c]
           * still reads live as s. The product below therefore reproduces
           * each step's true local branching factor, and since this loop
           * creates exactly s children, the s children's shares sum back to
           * t.share exactly: shares are conserved, so they sum to 1 by
           * construction rather than merely being normalised to it.
           */
          float share = 1.0f;
          for (const auto & step : child.prefix) {
            unsigned int stepCount = colCount[step.col];
            if (stepCount) share /= static_cast<float>(stepCount);
          }
          child.share = share;

          nextTasks.push_back(child);
        }
      }

      for (int d = (int)t.prefix.size() - 1; d >= 0; d--) {
        uncover_row(t.prefix[d].row);
        uncover(t.prefix[d].col);
      }
    }

    /* nextTasks must replace tasks unconditionally, even when nothing
     * expanded this round: a task pruned this round (its share already folded
     * into prunedShare above) is excluded from nextTasks, and the stale tasks
     * list still contains it. Discarding nextTasks here would silently
     * double-count that task's share (once in prunedShare, once still sitting
     * in the returned task list).
     */
    tasks = std::move(nextTasks);

    if (!anyExpanded)
      break;

    currentDepth++;
  }
}

void assembler_0_c::parallelMultiSearch(unsigned int workers) {
  // Snapshot of the run token: assemble() refreshed the source. Captured
  // once here and handed to workers/covers by value; nothing below touches
  // shared run state in a hot path.
  std::stop_token runTok = currentRunToken();
  running.store(true, std::memory_order_relaxed);
  // Fan-out for splits and stop-time divides, fixed for the whole run.
  splitFanout = SplitFanout::fromEnv();
  splitBreadthFirst = SplitFanout::breadthFirstFromEnv();
  donate = !SplitFanout::noDonateFromEnv();
  donateEvery = SplitFanout::donateEveryFromEnv();

  /* Workers call smallerRotationExists() outside callbackMutex, which reaches
   * the lazy mutable caches on the result shape AND on every part shape.
   * Warm them all here, on the master, before any worker exists.
   */
  if (avoidTransformedAssemblies)
    prewarmSharedShapeCaches(problem);

  // Only a list generated by this call may be discarded on a stop below; a
  // restored remainder is the sole record of what earlier runs left open.
  const bool generated = parallelTasks.empty();
  if (generated) {
    unsigned int targetTasks = std::max(16u, workers * 16);
    unsigned int maxDepth = std::min(piecenumber > 1 ? piecenumber - 1 : 1u, 5u);
    generateSubtreeTasks(parallelTasks, targetTasks, maxDepth, prunedTaskShare);
    completedTasks.store(0, std::memory_order_relaxed);

    /* Fresh task list: the only work already finished is what generation
     * itself proved empty (prunedTaskShare). A resumed run skips this
     * whole block -- parallelTasks is non-empty, holding exactly the
     * remainder pool.drain() saved back below -- so completedShare is left
     * untouched here and keeps exactly what the paused run had already
     * folded into it.
     */
    completedShare.store(prunedTaskShare, std::memory_order_release);

    /* Published with release ordering, matched by getFinished()'s acquire
     * load of totalTasks: a reader that sees a nonzero total always sees the
     * matching share. Stored only here, inside the fresh-generation branch,
     * not on every call: although totalTasks is just a >0 gate for this
     * engine (the actual fraction comes from completedShare and the worker
     * slots, not from dividing by it), re-storing it on a resumed run would
     * still publish the pool remainder's smaller count in place of the
     * original total, which assembler_1_c's copy of this block does divide
     * by -- the two engines' copies are kept identical here so the pattern
     * does not silently diverge into a real bug in one of them.
     */
    totalTasks.store(parallelTasks.size(), std::memory_order_release);
  }

  runPoolSeedHook();

  if (runTok.stop_requested()) {
    // Stopped before searching, mirroring the assembler_1 path. A list
    // generated by this call may be partial (stopped mid-generation) and
    // would silently drop subtrees on continue/save: discard it; nothing
    // was searched yet, so regenerating it is exact. A resumed remainder
    // is kept as is: regenerating instead would search, and report, the
    // territory earlier runs already covered a second time.
    if (generated) {
      parallelTasks.clear();
      remainderEngine = ENGINE_NONE;
    }
    parallelInterrupted = true;
    running.store(false, std::memory_order_relaxed);
    return;
  }

  if (parallelTasks.empty()) {
    if (!runTok.stop_requested())
      searchComplete.store(true, std::memory_order_relaxed);
    running.store(false, std::memory_order_relaxed);
    return;
  }

  // Dynamic work-stealing pool: workers that run out of tasks wait for new
  // work or global quiescence, so a dense subtree can be split onto idle
  // siblings (see splitPrefix) and workers blocked in
  // disassemblerPool_c::submit still have a live crew searching when permits
  // return and they wake up.
  AssemblyTaskPool<SubtreeTask> pool;
  pool.seed(std::move(parallelTasks));
  parallelTasks.clear();
  ThreadBudget *runBudget = nullptr;
  if (assembler_cb *cb = getCallback()) {
    runBudget = cb->threadBudget();
    pool.setBudget(runBudget);
  }

  // Deliver stop promptly to parked workers: pop_task() and the budget gate
  // observe the run token only in their predicates, so firing runStop alone
  // would leave them asleep until the next release() or task_done(). This
  // only wakes -- in-flight tasks, retry pushes and drain() are unaffected,
  // so in-session resume keeps working.
  std::stop_callback wakeParkedOnStop(runTok, [&] {
    pool.notify();
    if (runBudget != nullptr)
      runBudget->notify();
  });

  std::exception_ptr workerException = nullptr;
  std::mutex exceptionMutex;

  // Pop-time split decision, evaluated at task granularity (the two locked
  // reads cost nothing against a millisecond-scale task; splitPrefix
  // itself is microseconds on the worker-local scratch copy).
  //
  // Cursors always split: they have no whole to search. Beyond that, by
  // default nothing splits at pop -- running tasks split themselves when
  // siblings starve (donation, see assemblerWorker_c::donate and
  // design/2026-10-17-split-on-demand.md). Only with BURRTOOLS_NO_DONATE
  // do the eager heuristics below decide, with two independent triggers,
  // either of which suffices:
  // - Shallow prefix (depth < 3): such a task can hold an enormous subtree,
  //   and splitting it is always worth it -- by the time siblings starve on
  //   it, it is already running and unsplittable (see the concurrency
  //   architecture note in thread_budget.h for why preemption is out of
  //   scope). This
  //   also refines narrow seeds at runtime that never reached depth 3.
  // - Starving siblings and a short queue: classic work stealing; the 2N
  //   bound only needs to cover the race window, because a waiting sibling
  //   implies the queue ran dry just now -- with a deep queue there would be
  //   no waiters to begin with. Depth 3+ tasks therefore split only here.
  // splitPrefix itself returns empty at the depth cap, on dead ends, and
  // for single children, so neither trigger can over-refine.
  auto shouldSplit = [&](const SubtreeTask &task) {
    // Cursor tasks are never searchable -- they split again (or drop when
    // exhausted) wherever they are popped.
    if (task.hasCursor())
      return true;
    if (donate)
      return false;
    // SIMD rests index the SIMD frame's own order: searched, never split.
    if (task.restSkip > 0)
      return false;
    if (task.prefix.size() < 3)
      return true;
    return pool.has_waiting_workers() && pool.queued() < 2u * workers;
  };

  /* One published slot per worker, sized here -- before any worker starts --
   * and left alone until they have all joined. The GUI thread is already
   * polling getFinished() at this point, so the vector is built under
   * progressMutex; without it a reader can walk the vector while push_back is
   * reallocating it. (The naive assumption that the reader cannot see a
   * resize is false: totalTasks, which opens the branch that walks this
   * vector, is published above, before the vector exists.)
   */
  {
    std::lock_guard<std::mutex> lock(progressMutex);
    workerProgress.clear();
    workerProgress.reserve(workers);
    for (unsigned int i = 0; i < workers; i++)
      workerProgress.push_back(std::make_unique<WorkerProgress>());
  }

  /* The single task-accounting path, shared by the SIMD and the DLX worker
   * loop. Every task passes through exactly one beginTask/finishTask pair, so
   * its share reaches completedShare exactly once; adding it in more than one
   * place would drive completedShare -- and with it getFinished() -- toward
   * 2.0. Splitting a task does not go through this pair: it replaces one
   * live task with several smaller ones whose shares sum back to the
   * original (see splitPrefix), so nothing is completed or begun by a split.
   *
   * `completed` mirrors what this base already does with the retry-on-stop
   * path below: a task the run token cut short is not done, so its share is
   * not folded in. The slot is cleared either way, so a stop never leaves a
   * dead worker's in-flight term standing.
   *
   * The order inside finishTask matters, and program order alone does not
   * deliver it. getFinished() reads completedShare first and the slots
   * afterwards, and the slot's share is cleared *before* the share is folded
   * into completedShare, so the only value a reader can catch part-way
   * through the handoff is a momentarily low one. With both accesses relaxed
   * the two are unordered, and on arm64 -- the development and CI platform --
   * a reader really can observe the incremented accumulator while still
   * seeing the slot's old share, which double-counts that task. The two
   * publications a reader depends on are therefore releases, paired with
   * acquire loads in getFinished(), and each carries the slot store that
   * precedes it:
   *
   *   finishTask  slot->share = 0        then  completedShare += share
   *   beginTask   slot->fraction = 0     then  slot->share = task.share
   *
   * beginTask needs it for the same reason at the other end of the handoff:
   * getFinished() reports share * fraction, so a reader that caught the new
   * share alongside the finishing task's fraction -- which sits just below 1
   * -- would over-report by very nearly a whole task's share. That matters
   * because solvethread.cpp treats getFinished() >= 1 as a finished search: a
   * transient over-report there would end the solve with workers still live.
   */
  auto beginTask = [](WorkerProgress * slot, const SubtreeTask & task) {
    if (!slot) return;
    slot->fraction.store(0.0f, std::memory_order_relaxed);
    slot->share.store(task.share, std::memory_order_release);
  };
  auto finishTask = [this](WorkerProgress * slot, const SubtreeTask & task,
                           bool completed) {
    if (slot)
      slot->share.store(0.0f, std::memory_order_relaxed);
    if (!completed)
      return;
    completedTasks.fetch_add(1, std::memory_order_relaxed);
    completedShare.fetch_add(task.share, std::memory_order_release);
  };

  // A saved remainder pins its engine (SIMD rests only make sense there);
  // a fresh run picks by eligibility.
  bool useSimd = (remainderEngine == ENGINE_SIMD) || (remainderEngine == ENGINE_NONE && canUseSimd());
  if (useSimd) {
    remainderEngine = ENGINE_SIMD;
    auto solver = createSimdSolver();

    /* The SIMD backend has no hook to report how far into a subtree it has
     * got, so beginTask() publishes its task's share and leaves the fraction
     * at 0 -- i.e. it keeps the old whole-task granularity. In-flight
     * reporting is a property of the DLX path below, which is what runs for
     * puzzles above the 2048-column cap in canUseSimd().
     */
    auto simdWorkerFunc = [this, &pool, &solver, &workerException, &exceptionMutex, &shouldSplit,
                           &beginTask, &finishTask, runTok](std::stop_token st, unsigned int workerIdx) {
      try {
        // Scratch DLX copy at root state, used only for splitting prefixes.
        // The SIMD solver itself is shared read-only and never mutated here.
        assemblerWorker_c splitter(*this, runTok);
        WorkerProgress * slot = workerProgress[workerIdx].get();
        SubtreeTask task;
        while (pool.pop_task(task, runTok, st)) {
          try {
            if (shouldSplit(task)) {
              auto children = splitter.splitPrefix(task);
              if (!children.empty()) {
                task = std::move(children.front());
                children.erase(children.begin());
                size_t pushed = 0;
                if (splitBreadthFirst)
                  pushed = pool.push_tasks(std::move(children));
                else
                  pushed = pool.push_tasks_front(std::move(children));
                totalTasks.fetch_add(pushed, std::memory_order_relaxed);
              } else if (task.hasCursor()) {
                // Exhausted cursor (defensive only -- live cursors always
                // enumerate): its rows are fully covered by already-emitted
                // singletons, so count it with zero share instead of
                // searching the whole prefix (which would duplicate
                // completed siblings' solutions).
                task.share = 0;
                finishTask(slot, task, true);
                pool.finishTask();
                continue;
              }
            }

            beginTask(slot, task);

            std::vector<unsigned int> prefix_nodes;
            prefix_nodes.reserve(task.prefix.size());
            for (const auto &step : task.prefix) {
              prefix_nodes.push_back(step.row);
            }

            std::atomic<uint64_t> task_iter{0};
            // Split on demand (see ISimdExactCover::DonationSink): donated
            // rests are ordinary pool tasks, carved out of this task's
            // share (its slot keeps the original; the SIMD path publishes
            // no in-task fraction, so the slot term is 0 anyway).
            struct Sink : ISimdExactCover::DonationSink {
              assembler_0_c & self;
              AssemblyTaskPool<SubtreeTask> & pool;
              SubtreeTask & task;
              float rootShare;
              bool forced;
              Sink(assembler_0_c & s, AssemblyTaskPool<SubtreeTask> & p,
                   SubtreeTask & t, unsigned long every)
                : self(s), pool(p), task(t), rootShare(t.share), forced(every > 0) {
                interval = every > 0 ? every : 256;
              }
              bool wanted() override { return forced || pool.wants_work(); }
              bool give(const std::vector<unsigned int> & nodes, uint32_t restSkip,
                        double fraction) override {
                // one donation per waiter (see assemblerWorker_c::donate)
                if (!forced && !pool.claim_work())
                  return false;
                SubtreeTask piece;
                piece.prefix = self.prefixFromNodes(task, nodes);
                piece.restSkip = restSkip;
                piece.share = std::min(static_cast<float>(fraction * rootShare), task.share);
                task.share -= piece.share;
                self.donations.fetch_add(1, std::memory_order_relaxed);
                self.totalTasks.fetch_add(1, std::memory_order_relaxed);
                std::vector<SubtreeTask> one;
                one.push_back(std::move(piece));
                pool.push_tasks(std::move(one));
                return true;
              }
            } sink(*this, pool, task, donateEvery);
            // Remainder of this task if the stop interrupts it (see
            // ISimdExactCover::SubtreeRemainder): filled only on the stop
            // path, empty when the task runs to completion.
            ISimdExactCover::SubtreeRemainder remainder;
            remainder.fanout = splitFanout;

            // Drain per-solution remainders to the shared counter as they
            // arise, so live readers (GUI progress, iterator API) observe
            // advancing iterations; exact totals preserved (exchange drains).
            auto flushingCb = [this, &st, runTok, &task_iter, &remainder](const std::vector<unsigned int> &solution_nodes) -> bool {
              this->iterations.fetch_add(task_iter.exchange(0, std::memory_order_relaxed),
                                         std::memory_order_relaxed);
              bool didConsume = false;
              bool cont = handleSolution(solution_nodes.data(), solution_nodes.size(), runTok, didConsume);
              // Found but declined by the stop (never delivered): the
              // frames above drop this path as searched, so keep it as a
              // leaf piece. (Without a callback nobody is delivered to.)
              if (!didConsume && getCallback() && runTok.stop_requested())
                remainder.addPiece(solution_nodes);
              return cont && !runTok.stop_requested() && !st.stop_requested();
            };
            solver->solveSubtree(prefix_nodes, flushingCb, runTok, task_iter,
                                 &remainder, task.restSkip,
                                 donate ? &sink : nullptr);

            iterations.fetch_add(task_iter.load(std::memory_order_relaxed), std::memory_order_relaxed);
            // The remainder decides completion, not the token: a stop that
            // landed after the last branch leaves it empty.
            const bool done = remainder.pieces.empty();
            if (!done) {
              // Interrupted: the pieces partition the task's unexplored
              // territory (searched regions dropped, solutions delivered),
              // so the task's unit transfers to them and resume re-walks
              // nothing. The pieces are ordinary tasks (see
              // prefixFromNodes; splittable, unless rests).
              std::vector<SubtreeTask> pieces;
              pieces.reserve(remainder.pieces.size());
              const float unitShare =
                  task.share / static_cast<float>(remainder.pieces.size());
              for (const auto &rp : remainder.pieces) {
                SubtreeTask piece;
                piece.prefix = prefixFromNodes(task, rp.nodes);
                piece.restSkip = rp.restSkip;
                piece.share = unitShare;
                pieces.push_back(std::move(piece));
              }
              totalTasks.fetch_add(pieces.size() - 1, std::memory_order_relaxed);
              pool.push_tasks(std::move(pieces));
            }
            finishTask(slot, task, done);
          } catch (...) {
            pool.finishTask();
            throw;
          }
          pool.finishTask();
        }
      } catch (...) {
        std::lock_guard<std::mutex> lock(exceptionMutex);
        if (!workerException)
          workerException = std::current_exception();
        // Fire the run source so siblings stop promptly; stop() is safe
        // from any thread (mutex-guarded source swap).
        stop();
      }
    };

    std::vector<std::jthread> threads;
    threads.reserve(workers - 1);

    for (unsigned int i = 1; i < workers; i++) {
      threads.emplace_back(simdWorkerFunc, i);
    }

    simdWorkerFunc(std::stop_token{}, 0);

    for (auto & t : threads) {
      if (t.joinable())
        t.join();
    }
  } else {
    remainderEngine = ENGINE_DLX;
    auto dlxWorkerFunc = [this, &pool, &workerException, &exceptionMutex, &shouldSplit,
                          &beginTask, &finishTask, runTok](std::stop_token st, unsigned int workerIdx) {
      try {
        assemblerWorker_c worker(*this, runTok);
        WorkerProgress * slot = workerProgress[workerIdx].get();
        if (donate)
          worker.enableDonation(pool, donateEvery);

        SubtreeTask task;
        while (pool.pop_task(task, runTok, st)) {
          try {
            if (shouldSplit(task)) {
              auto children = worker.splitPrefix(task);
              if (!children.empty()) {
                task = std::move(children.front());
                children.erase(children.begin());
                size_t pushed = 0;
                if (splitBreadthFirst)
                  pushed = pool.push_tasks(std::move(children));
                else
                  pushed = pool.push_tasks_front(std::move(children));
                totalTasks.fetch_add(pushed, std::memory_order_relaxed);
              } else if (task.hasCursor()) {
                // Exhausted cursor, see the SIMD branch above: count with
                // zero share, never search the whole prefix.
                task.share = 0;
                finishTask(slot, task, true);
                pool.finishTask();
                continue;
              }
            }
            // SIMD rests exist only under a pinned SIMD remainder (and
            // setPosition() refuses them otherwise). Checked in release
            // too: the DLX search would take the rest as a whole subtree.
            if (task.restSkip != 0)
              bt_te("assembler_0_c: SIMD rest task in a DLX run");
            beginTask(slot, task);
            const bool exhausted = worker.searchSubtree(task, slot);
            // An interrupted task is divided into exact continuation tasks
            // from the stopped live state (see divideRemainder) instead of
            // being requeued whole: resume re-walks nothing. An exhausted
            // one leaves only its missed solutions (see takeMissed), if
            // the stop landed inside its last deliveries.
            std::vector<SubtreeTask> toPush;
            if (exhausted) {
              toPush = worker.takeMissed(task);
            } else {
              bool requeueWhole = false;
              toPush = worker.divideRemainder(task, requeueWhole);
              // Untouched: the task itself, which delivered nothing, so a
              // plain requeue resumes exactly. (No pieces without this
              // flag means the stop landed after the last branch.)
              if (requeueWhole)
                toPush.assign(1, task);
            }
            // The pieces partition the parent's remaining territory, so
            // the parent's unit in totalTasks transfers to them.
            const bool done = toPush.empty();
            if (!done) {
              totalTasks.fetch_add(toPush.size() - 1,
                                   std::memory_order_relaxed);
              pool.push_tasks(std::move(toPush));
            }
            finishTask(slot, task, done);
          } catch (...) {
            pool.finishTask();
            throw;
          }
          pool.finishTask();
        }

        worker.flushIterations();
      } catch (...) {
        std::lock_guard<std::mutex> lock(exceptionMutex);
        if (!workerException)
          workerException = std::current_exception();
        // See SIMD branch: fire the run source so siblings stop promptly.
        stop();
      }
    };

    std::vector<std::jthread> threads;
    threads.reserve(workers - 1);

    for (unsigned int i = 1; i < workers; i++) {
      threads.emplace_back(dlxWorkerFunc, i);
    }

    dlxWorkerFunc(std::stop_token{}, 0);

    for (auto & t : threads) {
      if (t.joinable())
        t.join();
    }
  }

  /* Every worker has joined, so the slots are dead: drop them rather than
   * leave the last published in-flight terms standing for a later poll to add
   * to completedShare. Under progressMutex for the same reason they were
   * built under it -- the GUI thread may still be walking the vector.
   */
  {
    std::lock_guard<std::mutex> lock(progressMutex);
    workerProgress.clear();
  }

  if (workerException) {
    running.store(false, std::memory_order_relaxed);
    std::rethrow_exception(workerException);
  }

  /* A stop that lands after the pool fully drained (every task counted,
   * nothing left queued) means the search genuinely finished: complete
   * exactly as an unstopped run. Saving that empty remainder as interrupted
   * would make a continue regenerate everything from scratch (duplicating
   * every assembly) or refuse the restore. drainedPoolComplete() pins the
   * condition; its truth table is unit-tested directly.
   */
  runPoolDrainedHook();
  const bool stopped = runTok.stop_requested();
  if (stopped)
    parallelTasks = pool.drain();
  if (!stopped || drainedPoolComplete(stopped, parallelTasks.size(),
        totalTasks.load(std::memory_order_relaxed),
        completedTasks.load(std::memory_order_relaxed))) {
    pos = piecenumber + 1;
    parallelTasks.clear();
    prunedTaskShare = 0.0f;
    parallelInterrupted = false;
    remainderEngine = ENGINE_NONE;
    searchComplete.store(true, std::memory_order_relaxed);
  } else {
    /* Stopped part way. In-memory continue is fine -- the pool remainder is
     * saved back into parallelTasks (divided continuations of interrupted
     * tasks, untouched tasks); save() persists all of it (format 1.7), so
     * cross-session continue resumes from the same remainder.
     */
    parallelInterrupted = true;
  }

  running.store(false, std::memory_order_relaxed);
}

bool assembler_0_c::canUseSimd(void) const {
  if (pos != 0)
    return false;
  if (!SimdConfig::isBitParallelSolverEnabled())
    return false;
  if (debug)
    return false;

  // Holes and variable voxels are supported via optional columns plus the
  // hole budget (see SimdExactCover::setOptionalColumn/setHoleBudget), so
  // they don't disqualify the SIMD path. Only the width cap remains, now
  // measured over all columns including the variable ring.
  int res_filled = getResultShape(problem)->countState(voxel_c::VX_FILLED);
  int res_vari = getResultShape(problem)->countState(voxel_c::VX_VARIABLE);
  unsigned int total_cols = (res_vari > 0 && varivoxelEnd > 0)
    ? varivoxelEnd
    : piecenumber + static_cast<unsigned int>(res_filled);
  // Cap Assembler 0 at 2048 columns:
  // 1. Thread stack safety: recursion passes BitsetType by value on the stack.
  //    At 32768 columns (4 KB/frame), deep recursion risks overflowing 512 KB thread stacks
  //    (default on macOS std::thread). At 2048 columns, it is only 256 B/frame.
  // 2. Cache locality: every search node clears col_counts. At 2048 columns (8 KB) it fits
  //    in L1 cache; at 32768 columns (128 KB) it blows out L1/L2 caches on every node visit.
  // 3. Memory per row: each row stores BitsetType by value (~320 B at 2048 vs ~4,160 B at 32k).
  // Above 2048 columns, classical sparse DLX is faster and uses significantly less memory.
  // (Note: Assembler 1 supports up to 32768 columns because its search loop is indexed by shapes
  // and unplaced voxels rather than sweeping dense column counts at every node.)
  // The cap applies to the total width including variable columns, for the
  // same stack/cache reasons.
  if (total_cols > 2048)
    return false;

  return true;
}

std::unique_ptr<ISimdExactCover> assembler_0_c::createSimdSolver(void) const {
  int res_filled = getResultShape(problem)->countState(voxel_c::VX_FILLED);
  int res_vari = getResultShape(problem)->countState(voxel_c::VX_VARIABLE);
  unsigned int max_col = piecenumber + res_filled;
  // Solver width must cover the variable ring too (DLX column c maps to
  // solver column c-1, highest index varivoxelEnd-1). Without variable
  // voxels this is exactly max_col, preserving existing tier choices.
  unsigned int num_columns = (res_vari > 0) ? varivoxelEnd : max_col;

  std::unique_ptr<ISimdExactCover> solver;
  if (num_columns <= 256) {
    solver = std::make_unique<SimdExactCover256>(num_columns, piecenumber);
  } else if (num_columns <= 512) {
    solver = std::make_unique<SimdExactCover512>(num_columns, piecenumber);
  } else if (num_columns <= 1024) {
    solver = std::make_unique<SimdExactCover1024>(num_columns, piecenumber);
  } else if (num_columns <= 2048) {
    solver = std::make_unique<SimdExactCover2048>(num_columns, piecenumber);
  } else if (num_columns <= 4096) {
    solver = std::make_unique<SimdExactCover4096>(num_columns, piecenumber);
  } else if (num_columns <= 8192) {
    solver = std::make_unique<SimdExactCover8192>(num_columns, piecenumber);
  } else if (num_columns <= 16384) {
    solver = std::make_unique<SimdExactCover16384>(num_columns, piecenumber);
  } else {
    solver = std::make_unique<SimdExactCover32768>(num_columns, piecenumber);
  }

  for (unsigned int c = right[0]; c != 0; c = right[c]) {
    if (c <= max_col) {
      solver->setRequiredColumn(c - 1);
    }
  }

  // Variable voxel columns live on their own ring (never pivoted by DLX
  // either) and are optional: at most once each, leftovers governed by the
  // hole budget. Empty loop when the puzzle has no variable voxels.
  for (unsigned int c = right[varivoxelEnd]; c != varivoxelEnd; c = right[c]) {
    solver->setOptionalColumn(c - 1);
  }
  solver->setHoleBudget(static_cast<unsigned int>(holes));

  for (unsigned int p = 1; p <= piecenumber; p++) {
    for (unsigned int row = down(p); row != p; row = down(row)) {
      std::vector<unsigned int> cols;
      std::vector<unsigned int> nodes_in_row;
      unsigned int curr = row;
      do {
        nodes_in_row.push_back(curr);
        unsigned int col = colCount[curr];
        if (col > 0 && col < varivoxelEnd) {
          cols.push_back(col - 1);
        }
        curr = right[curr];
      } while (curr != row);

      uint32_t row_idx = solver->addRow(row, p - 1, cols);
      for (unsigned int n : nodes_in_row) {
        solver->registerNodeAlias(n, row_idx);
      }
    }
  }

  return solver;
}

bool assembler_0_c::willRunParallel(unsigned int threads) const {
  /* the condition assemble() dispatches on, in one place. pos != 0 is a
   * resumed search that carries a partial DLX stack the task generator cannot
   * start from, so it always runs serially however many threads are
   * configured. SIMD-eligible fresh runs take the task-based parallel path
   * even with a single thread: parallelMultiSearch() runs worker 0 inline
   * with no extra thread, and its pool remainder makes an interrupted run
   * resumable.
   */
  if ((errorsState != ERR_NONE) || (pos != 0))
    return false;
  // A restored DLX or SIMD remainder must keep running the task pool even
  // single-threaded: the serial path resumes from pos/rows (root after a
  // parallel run) and would re-search everything without dedup. Mirrors
  // assembler_1_c::willRunParallel(), which keys off its two remainders.
  if (remainderEngine != ENGINE_NONE || !parallelTasks.empty())
    return true;
  return (threads > 1) || canUseSimd();
}

unsigned int assembler_0_c::getRunThreads(void) const {
  const unsigned int threads = getEffectiveThreads();
  return willRunParallel(threads) ? threads : 1;
}

void assembler_0_c::assemble(assembler_cb * callback) {

  debug = false;

  if (errorsState != ERR_NONE)
    return;

  // Canonical per-run refresh: everything below (serial or parallel,
  // SIMD or DLX) observes this run's token; a second assemble() starts
  // unstopped.
  std::stop_token runTok = beginRun();

  asm_bc = callback;
  const unsigned int threads = getEffectiveThreads();
  /* Single dispatch source: willRunParallel() covers the multi-thread case
   * and SIMD-eligible fresh runs (single-threaded ones included). Only serial
   * DLX resumes reach the iterative path below.
   */
  const bool runParallel = willRunParallel(threads);

  /* The task counters are switched OFF for a non-parallel run, not merely
   * left alone. A solve aborted on the parallel path leaves totalTasks and
   * completedShare holding that run's numbers, and totalTasks is what gates
   * the task-based branch of getFinished() -- so a resume that falls to the
   * serial path would otherwise report the abandoned run's constant share for
   * the whole of the serial search.
   *
   * totalTasks is stored first, closing the branch, before the values that
   * branch reads are cleared: a concurrent getFinished() then sees either the
   * whole of the old state or none of it, never a zeroed share paired with a
   * live total.
   */
  if (!runParallel) {
    totalTasks.store(0, std::memory_order_release);
    completedTasks.store(0, std::memory_order_relaxed);
    completedShare.store(0.0, std::memory_order_relaxed);
  }

  if (runParallel) {
    parallelMultiSearch(threads);
  } else {
    // Serial DLX path (non-SIMD fresh runs and resumes): hold a budget
    // token too, so the cap covers resume runs as well. Skipped only when
    // stopping (no token with a callback set).
    BudgetGuard budget(callback != nullptr ? callback->threadBudget() : nullptr,
                       runTok);
    if (callback == nullptr || budget.holds()) {
      iterativeMultiSearch();
    }
  }
}

float assembler_0_c::getFinished(void) const {

  if (searchComplete.load(std::memory_order_relaxed))
    return 1.0f;

  size_t total = totalTasks.load(std::memory_order_acquire);
  if (total > 0) {
    /* Completed tasks plus how far each running worker has got into its own
     * task. Without the second term a task contributes nothing until it
     * finishes, which is what made the bar sit still on the tail task.
     *
     * Only published atomics are read here. A worker's matrix is private to
     * its thread and is never touched from this (GUI) thread.
     *
     * completedShare is read before the slots on purpose, and both it and
     * each slot's share are read with acquire ordering -- see the ordering
     * note on beginTask()/finishTask() in parallelMultiSearch().
     *
     * The sum is accumulated in double, not float: see completedShare's
     * declaration.
     */
    /* The accumulator and the slots are read as a SNAPSHOT, seqlock style:
     * read completedShare, walk the slots, read it again, and accept the pair
     * only if it did not move. This is what makes the value monotone, and it
     * is the same code assembler_1_c::getFinished() runs -- the two engines'
     * progress blocks have diverged four times over this work, so they are
     * kept line for line alike. The long-form argument lives there; in short:
     *
     * finishTask() hands a task over in two steps -- clear my slot, then fold
     * my share into completedShare -- and a reader must never see both the
     * fold AND the slot's outgoing share, because that counts the task twice
     * and an over-report is what pushes the sum to 1.0 and ends a solve with
     * work left. Reading the accumulator first makes a torn read LOW rather
     * than high, which is safe but not monotone. Re-reading the accumulator
     * and taking the larger answer narrows the window but does not close it:
     * it recovers the finished task only by discarding every other worker's
     * in-flight term, and loses whenever those sum to less than one task's
     * share. Retaking the snapshot recovers both. Giving up after a few tries
     * falls back to the accumulator alone, which is monotone and cannot
     * over-report.
     */
    double done = completedShare.load(std::memory_order_acquire);
    double best = 0;

    for (unsigned int attempt = 0; attempt < 4; attempt++) {
      double sum = 0;
      {
        std::lock_guard<std::mutex> lock(progressMutex);
        for (const auto & w : workerProgress)
          if (w)
            sum += static_cast<double>(w->share.load(std::memory_order_acquire))
                 * static_cast<double>(w->fraction.load(std::memory_order_relaxed));
      }

      /* Every attempt's pair is kept as a floor, not just the consistent one;
       * see the long note in assembler_1_c::getFinished(). `done` was read
       * before this walk, so a slot still showing a task folded in since is
       * not a second copy of it and the pair can only under-report. The
       * exhaustion path needs this: four failed attempts move `done` by at
       * most four tasks while discarding in-flight terms worth up to one per
       * worker, and here even four workers can do it -- four tiny shares can
       * be folded in while one large share is still running.
       */
      const double candidate = done + sum;
      if (candidate > best)
        best = candidate;

      const double after = completedShare.load(std::memory_order_acquire);
      if (after == done)
        break;

      /* a handoff landed inside the walk: `sum` and `after` disagree about
       * that task, so retake the pair
       */
      done = after;
    }

    return (best > 1.0) ? 1.0f : static_cast<float>(best);
  }

  /* we don't need locking, as I hope that I have written the
   * code in a way that updated the data so, that it will never
   * be in an inconsistent state. The thing that will happen is that
   * the value may jump
   */

  if (rows.empty() || columns.empty() || upDown.empty())
    return 0;

  float erg = 0;

  if (pos > piecenumber)
    return 1;

  for (int i = pos - 1; i >= 0; i--) {

    unsigned int r = rows[i];
    unsigned int l = colCount[columns[i]];

    while (l && r && (r != down(columns[i]))) {
      erg += 1;
      r = up(r);
      l--;
    }

    /* see localFraction(): a zero count must not reach the divide */
    if (colCount[columns[i]] == 0)
      continue;
    erg /= colCount[columns[i]];
  }

  return erg;
}

static unsigned int getInt(const char * s, unsigned int * i) {

  char * s2;

  *i = std::strtol (s, &s2, 10);

  if (s2)
    return s2-s;
  else
    return 500000;
}

static unsigned int getLong(const char * s, unsigned long * i) {

  char * s2;

  *i = std::strtol (s, &s2, 10);

  if (s2)
    return s2-s;
  else
    return 500000;
}

static unsigned int getDouble(const char * s, double * d) {

  char * s2;

  *d = std::strtod (s, &s2);

  if (s2)
    return s2-s;
  else
    return 500000;
}

assembler_c::errState assembler_0_c::setPosition(const char * string, const char * version) {

  /* we assert that the matrix is in the initial position
   * otherwise we would have to clean the stack
   */
  bt_assert(pos == 0);
  parallelTasks.clear();
  prunedTaskShare = 0.0f;
  remainderEngine = ENGINE_NONE;

  /* Only the current version resumes: a paused search saved by another
   * BurrTools version is refused (the found assemblies stay, the search
   * starts over -- see the README, "Pausing a search and continuing
   * later"). That keeps a single
   * reader and no legacy resume paths. */
  if (version == nullptr || strcmp(version, ASSEMBLER_VERSION) != 0)
    return ERR_CAN_NOT_RESTORE_VERSION;

  unsigned int len = strlen(string);
  unsigned int spos = 0;

  /* leading flag written by save(): 1 marks an interrupted search */
  unsigned int interrupted = 0;
  spos += getInt(string+spos, &interrupted);
  if (spos >= len) return ERR_CAN_NOT_RESTORE_SYNTAX;

  unsigned int engineInt = 0;
  spos += getInt(string+spos, &engineInt);
  if (spos >= len) return ERR_CAN_NOT_RESTORE_SYNTAX;
  if (engineInt > ENGINE_SIMD) return ERR_CAN_NOT_RESTORE_SYNTAX;

  /* get the values from the string.
   */
  spos += getInt(string+spos, &pos);
  if (spos >= len) return ERR_CAN_NOT_RESTORE_SYNTAX;

  {
    unsigned long it = 0;
    spos += getLong(string+spos, &it);
    iterations.store(it, std::memory_order_relaxed);
  }
  if (spos >= len) return ERR_CAN_NOT_RESTORE_SYNTAX;

  if (pos <= piecenumber)
    for (unsigned int i = 0; i <= pos; i++) {
      while ((spos < len) && (*(string+spos) != '(')) spos++;
      spos++;
      if (spos >= len) return ERR_CAN_NOT_RESTORE_SYNTAX;

      /* Slot number piecenumber is the inert dummy pair save() writes when
       * a stop lands with everything placed (pos == piecenumber has no
       * rows/columns entry of its own): parsed for format alignment, never
       * stored. The restore loop below only covers p < piecenumber.
       */
      unsigned int r = 0, c = 0;
      spos += getInt(string+spos, &r);
      if (spos >= len) return ERR_CAN_NOT_RESTORE_SYNTAX;
      spos += getInt(string+spos, &c);
      if (spos >= len) return ERR_CAN_NOT_RESTORE_SYNTAX;
      if (i < piecenumber) {
        rows[i] = r;
        columns[i] = c;
      }

      while ((spos < len) && (*(string+spos) != ')')) spos++;
      spos++;
      // after the last ')' we might be at the end of the string, so here
      // it's ok to have a pos at the end
      if ((i < pos) && (spos >= len)) return ERR_CAN_NOT_RESTORE_SYNTAX;
    }

  /* check for integrity last read data may contain only preparation for next, so don't check that one */
  if (pos <= piecenumber) {
    for (unsigned int i = 0; i < pos; i++) {
      // for the last piece we can check for the number of nodes
      if (rows[i] > left.size())
        return ERR_CAN_NOT_RESTORE_SYNTAX;
    }
  }

  /* Parallel remainder: pool tasks as flat prefix steps with per-task
   * share, cursor position and SIMD rest start, then the completed
   * share. The remainder holds only unexplored territory. */
  std::vector<SubtreeTask> newTasks;
  unsigned int taskCount = 0;
  spos += getInt(string+spos, &taskCount);
  if (spos >= len && taskCount > 0) return ERR_CAN_NOT_RESTORE_SYNTAX;
  for (unsigned int i = 0; i < taskCount; i++) {
    SubtreeTask t;
    unsigned int steps = 0;
    spos += getInt(string+spos, &steps);
    if (spos >= len && steps > 0) return ERR_CAN_NOT_RESTORE_SYNTAX;
    for (unsigned int k = 0; k < steps; k++) {
      PrefixStep step;
      unsigned int col = 0, row = 0;
      spos += getInt(string+spos, &col);
      if (spos >= len) return ERR_CAN_NOT_RESTORE_SYNTAX;
      spos += getInt(string+spos, &row);
      if (spos >= len && k + 1 < steps) return ERR_CAN_NOT_RESTORE_SYNTAX;
      step.col = col;
      step.row = row;
      t.prefix.push_back(step);
    }
    double share = 0;
    spos += getDouble(string+spos, &share);
    if (spos >= len) return ERR_CAN_NOT_RESTORE_SYNTAX;
    t.share = static_cast<float>(share);

    unsigned int cCol = 0, cSkip = 0, rest = 0;
    spos += getInt(string+spos, &cCol);
    if (spos >= len) return ERR_CAN_NOT_RESTORE_SYNTAX;
    spos += getInt(string+spos, &cSkip);
    if (spos >= len) return ERR_CAN_NOT_RESTORE_SYNTAX;
    spos += getInt(string+spos, &rest);
    if (spos >= len && i + 1 < taskCount) return ERR_CAN_NOT_RESTORE_SYNTAX;
    t.cursorCol = cCol;
    t.cursorSkip = cSkip;
    t.restSkip = rest;
    // A SIMD rest indexes the SIMD frame's order: only valid under a
    // SIMD remainder, and never combined with a DLX cursor.
    if (rest != 0 && (engineInt != ENGINE_SIMD || cCol != 0))
      return ERR_CAN_NOT_RESTORE_SYNTAX;

    newTasks.push_back(std::move(t));
  }

  double restoredShare = 0;
  // The completed share is always written by save(), even with zero
  // tasks: a payload ending here is truncated, not valid.
  if (spos >= len) return ERR_CAN_NOT_RESTORE_SYNTAX;
  spos += getDouble(string+spos, &restoredShare);

  if (interrupted && newTasks.empty())
    return ERR_CAN_NOT_RESTORE_INTERRUPTED;

  // Commit the remainder. A parallel continue resumes from these tasks
  // (parallelMultiSearch skips generation when non-empty); the serial
  // position above is root after parallel runs and restores as no-op.
  parallelTasks = std::move(newTasks);
  // The engine is pinned only by a remainder to resume: without tasks the
  // next run is fresh and picks its engine by eligibility (and the env
  // toggles), as willRunParallel() then sees no remainder either.
  remainderEngine = parallelTasks.empty() ? ENGINE_NONE
                                          : static_cast<RemainderEngine>(engineInt);
  if (!parallelTasks.empty()) {
    totalTasks.store(parallelTasks.size(), std::memory_order_relaxed);
    completedTasks.store(0, std::memory_order_relaxed);
    completedShare.store(restoredShare, std::memory_order_relaxed);
    parallelInterrupted = false;
  }

  /* here we need to get the matrix into this exact position as it has been, when we
   * saved the position that means we need to cover all rows and columns in the same
   * order as it happened in the original process
   */
  unsigned int p = 0;

  if (pos <= piecenumber) {

    while (p <= pos) {

      if ((p < piecenumber) && rows[p]) {
        cover(columns[p]);
        cover_row(rows[p]);
      }

      p++;
    }
  }

  return ERR_NONE;
}

void assembler_0_c::save(xmlWriter_c & xml) const
{
  xml.newTag("assembler");
  xml.newAttrib("version", ASSEMBLER_VERSION);

  std::ostream & str = xml.addContent();

  /* leading flag: 1 marks a search that stopped before finishing.
   * Reloading such a position resumes it only when task data follows
   * (format 1.7, see below); a task-less stop is refused with
   * ERR_CAN_NOT_RESTORE_INTERRUPTED. State saved by any other version
   * never gets here: setPosition() refuses it with
   * ERR_CAN_NOT_RESTORE_VERSION.
   */
  str << (parallelInterrupted ? 1 : 0) << " ";
  str << static_cast<unsigned int>(remainderEngine) << " ";

  str << pos << " " << iterations << " ";

  if (pos <= piecenumber)
    for (unsigned int j = 0; j <= pos; j++)
    {
      /* rows/columns hold one entry per placed piece (indices below
       * piecenumber). A stop that lands with everything placed leaves
       * pos == piecenumber, a transient the search loop normalizes
       * (pos--) before any vector access. There is no rows[piecenumber]
       * entry, so write an inert dummy pair for that slot; the restore
       * loop parses it for alignment and ignores it.
       */
      const unsigned int r = (j < piecenumber) ? rows[j] : 0;
      const unsigned int c = (j < piecenumber) ? columns[j] : 0;
      str << "(" << r << " " << c << ")";

      if (j < pos) str << " ";
    }

  /* Parallel remainder for cross-session resume: pool tasks as
   * flat column/row prefix steps, with per-task share, cursor position,
   * SIMD rest start, and the completed share. The remainder holds only
   * unexplored territory. Cursor
   * tasks (see SubtreeTask) persist their column and skip so a rest
   * continues splitting where it left off.
   */
  str << " " << parallelTasks.size() << " ";
  for (const auto &t : parallelTasks) {
    str << t.prefix.size() << " ";
    for (const auto &step : t.prefix)
      str << step.col << " " << step.row << " ";
    // Per-task share, round-trip exact (float max_digits10), so a reload
    // reproduces the progress state instead of sitting at 0%.
    str << std::setprecision(std::numeric_limits<float>::max_digits10)
        << t.share << " ";
    str << t.cursorCol << " " << t.cursorSkip << " " << t.restSkip << " ";
  }
  // Completed share with double precision for the same reason.
  str << std::setprecision(std::numeric_limits<double>::max_digits10)
      << completedShare.load(std::memory_order_relaxed);

  xml.endTag("assembler");
}

unsigned int assembler_0_c::getPiecePlacement(unsigned int node, int delta, unsigned int piece, unsigned char *tran, int *x, int *y, int *z) const {

  unsigned int pi;

  if (!node)
    node = down(piece+1);

  while (delta > 0) {
    node = down(node);
    delta--;
  }

  while (delta < 0) {
    node = up(node);
    delta++;
  }

  getPieceInformation(node, tran, x, y, z, &pi);

  bt_assert(pi == piece);

  return node;
}

unsigned int assembler_0_c::getPiecePlacementCount(unsigned int piece) const {

  return colCount[piece+1];
}


void assembler_0_c::debug_step(unsigned long num) {
  debug = true;
  debug_loops = num;
  asm_bc = 0;
  // Own run entry (does not go through assemble()): refresh first.
  beginRun();
  iterativeMultiSearch();
}

bool assembler_0_c::canHandle(const problem_c & p) {

  // we can not handle if there is one shape having not a counter of 1
  for (unsigned int s = 0; s < p.getNumberOfParts(); s++)
    if ((p.getPartMaximum(s) > 1) ||
        (p.getPartMaximum(s) != p.getPartMinimum(s)))

      return false;

  return true;
}

