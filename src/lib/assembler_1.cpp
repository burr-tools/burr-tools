/* Burr Solver
 * Copyright (C) 2003-2008  Andreas Röver
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

#include "bt_assert.h"
#include "puzzle.h"
#include "voxel.h"
#include "assembly.h"
#include "gridtype.h"

#include <xmlwrapp/attributes.h>

#ifdef WIN32
#define snprintf _snprintf
#endif

#define ASSEMBLER_VERSION "2.0"

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

void assembler_1_c::getPieceInformation(unsigned int node, unsigned int * piece, unsigned char *tran, int *x, int *y, int *z) {

  for (int i = piecePositions.size()-1; i >= 0; i--)
    if (piecePositions[i].row <= node) {
      *tran = piecePositions[i].transformation;
      *x = piecePositions[i].x;
      *y = piecePositions[i].y;
      *z = piecePositions[i].z;
      *piece = piecePositions[i].piece;

      return;
    }

  bt_assert(0);
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

assembler_1_c::assembler_1_c(assemblerFrontend_c * fe) :
  assembler_c(fe),
  avoidTransformedAssemblies(0), avoidTransformedMirror(0),
  reducePiece(0)
{
  next_row_stack.push_back(0);
  task_stack.push_back(0);
}

assembler_1_c::~assembler_1_c() {
  if (avoidTransformedMirror) delete avoidTransformedMirror;
}

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

  if (!pieceFits(x, y, z))
    return false;

  const voxel_c * result = puzzle->probGetResultShape(problem);

  for (unsigned int pz = piece->boundZ1(); pz <= piece->boundZ2(); pz++)
    for (unsigned int py = piece->boundY1(); py <= piece->boundY2(); py++)
      for (unsigned int px = piece->boundX1(); px <= piece->boundX2(); px++)
        if (
            // the piece can not be place if the result is empty and the piece is filled at a given voxel
            ((piece->getState(px, py, pz) != voxel_c::VX_EMPTY) &&
             (result->getState(x+px, y+py, z+pz) == voxel_c::VX_EMPTY)) ||

            // the piece can also not be placed when the colour constraints don't fit
            !puzzle->probPlacementAllowed(problem, piece->getColor(px, py, pz), result->getColor(x+px, y+py, z+pz))

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
int assembler_1_c::prepare(bool hasRange, unsigned int rangeMin, unsigned int rangeMax) {

  if (hasRange) {
    fprintf(stderr, "range optimisation used min %i, max %i\n", rangeMin, rangeMax);
  }

  const voxel_c * result = puzzle->probGetResultShape(problem);

  /* nodes 1..n are the columns nodes */
  GenerateFirstRow(result->countState(voxel_c::VX_FILLED)+
      result->countState(voxel_c::VX_VARIABLE)+
      puzzle->probShapeNumber(problem)+
      (hasRange?1:0));

  /* this array contains the column in our matrix that corresponds with
   * the voxel position inside the result. We use this matrix because
   * the calculation of the exact column depends on the number of FILLED
   * and VARIABLE voxels and is relatively expensive to calculate
   * with this lookup I was able to reduce the preparation time
   * from 5 to 0.5 seconds for TheLostDay puzzle
   */
  unsigned int * columns = new unsigned int[result->getXYZ()];
  unsigned int rangeColumn;

  {
    int c = 1 + puzzle->probShapeNumber(problem);

    for (unsigned int i = 0; i < result->getXYZ(); i++) {
      switch(result->getState(i)) {
      case voxel_c::VX_VARIABLE:
        min[c] = 0;
        holeColumns.push_back(c);
      case voxel_c::VX_FILLED:
        columns[i] = c++;
        break;
      default:
        columns[i] = 0;
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
  const gridType_c * gt = puzzle->getGridType();
  const symmetries_c * sym = puzzle->getGridType()->getSymmetries();
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
    unsigned int pc = puzzle->probGetShapeMax(problem, 0);
    unsigned int bestFound = sym->countSymmetryIntersection(resultSym, puzzle->probGetShapeShape(problem, 0)->selfSymmetries());
    symBreakerShape = 0;

    for (unsigned int i = 1; i < puzzle->probShapeNumber(problem); i++) {

      unsigned int cnt = sym->countSymmetryIntersection(resultSym, puzzle->probGetShapeShape(problem, i)->selfSymmetries());

      if ((puzzle->probGetShapeMax(problem, i) < puzzle->probGetShapeMax(problem, symBreakerShape)) ||
          (puzzle->probGetShapeMax(problem, i) == puzzle->probGetShapeMax(problem, symBreakerShape)) && (cnt < bestFound)) {
        bestFound = cnt;
        symBreakerShape = i;
        symBreakerPiece = pc;
      }

      pc += puzzle->probGetShapeMax(problem, i);
    }

    bool tmp = sym->symmetriesLeft(resultSym, puzzle->probGetShapeShape(problem, symBreakerShape)->selfSymmetries());

    bool pieceRanges = false;
    for (unsigned int i = 0; i < puzzle->probShapeNumber(problem); i++)
      if (puzzle->probGetShapeMin(problem, i) != puzzle->probGetShapeMax(problem, i)) {
        pieceRanges = true;
        break;
      }

    if (tmp || (puzzle->probGetShapeMax(problem, symBreakerShape) > 1) || pieceRanges) {

      // we can not use the symmetry breaker shape, if there is more than one piece
      // of this shape in the problem
      if (pieceRanges || puzzle->probGetShapeMax(problem, symBreakerShape) > 1) {
        symBreakerShape = 0xFFFFFFFF;
        symBreakerPiece = 0xFFFFFFFF;
      }

      checkForTransformedAssemblies(symBreakerPiece, 0);
    }

    if (sym->symmetryContainsMirror(resultSym)) {
      /* we need to to the mirror check here, and initialize the mirror
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

      mm * mirror = new mm[puzzle->probPieceNumber(problem)];

      // first initialize
      for (unsigned int i = 0; i < puzzle->probShapeNumber(problem); i++)
        for (unsigned int p = 0; p < puzzle->probGetShapeMax(problem, i); p++) {
          mirror[pc].shape = i;
          mirror[pc].mirror = (unsigned int)-1;
          mirror[pc].trans = 255;
          pc++;
        }

      bool mirrorCheck = true;

      // now go over all shapes
      for (unsigned int i = 0; i < puzzle->probPieceNumber(problem); i++) {

        // we have already found the mirror for this shape
        if (mirror[i].mirror < puzzle->probPieceNumber(problem))
          continue;

        if (!sym->symmetryContainsMirror(puzzle->probGetShapeShape(problem, mirror[i].shape)->selfSymmetries())) {
          /* this shape is not self mirroring, so we need to look out
           * for a shape that is the mirror of this shape
           */
          bool found = false;

          // now see if we can find another shape that is the mirror of the current shape
          for (unsigned int j = i+1; j < puzzle->probPieceNumber(problem); j++) {

            if (mirror[j].mirror < puzzle->probPieceNumber(problem))
              continue;

            unsigned int trans = puzzle->probGetShapeShape(problem, mirror[i].shape)->getMirrorTransform(
                puzzle->probGetShapeShape(problem, mirror[j].shape));

            if (trans > 0) {
              // found a mirror shape

              mirror[i].mirror = j;
              mirror[i].trans = trans;
              mirror[j].mirror = i;
              mirror[j].trans = puzzle->probGetShapeShape(problem, mirror[j].shape)->getMirrorTransform(
                  puzzle->probGetShapeShape(problem, mirror[i].shape));

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
        mirrorInfo_c * mir = new mirrorInfo_c();

        for (unsigned int i = 0; i < puzzle->probPieceNumber(problem); i++)
          if (mirror[i].trans != 255)
            mir->addPieces(i, mirror[i].mirror, mirror[i].trans);

        checkForTransformedAssemblies(symBreakerPiece, mir);
      }

      delete [] mirror;
    }
  }

  /* node 0 is the start node for everything */

  /* even thou the matrix has a column for each result voxel and each piece we leave out
   * the VARIABLE voxels in the ring list of the header. This is to avoid selecting these
   * columns for filling. The columns for the VARIABLE voxels are only there to make sure
   * these voxels are only used once
   */

  voxel_c ** cache = new voxel_c *[sym->getNumTransformationsMirror()];

  /* now we insert one shape after another */
  for (unsigned int pc = 0; pc < puzzle->probShapeNumber(problem); pc++) {

    // setup weight values so that they do fit the number of pieces for this
    // shape
    max[pc+1] = puzzle->probGetShapeMax(problem, pc);
    min[pc+1] = puzzle->probGetShapeMin(problem, pc);

    unsigned int voxels = puzzle->probGetShapeShape(problem, pc)->countState(voxel_c::VX_FILLED);

    /* this array contains all the pieces found so far, this will help us
     * to not add two times the same piece to the structure */
    unsigned int cachefill = 0;
    unsigned int placements = 0;

    /* go through all possible rotations of the piece
     * if shape is new to cache, add it to the cache and also
     * add the shape to the matrix, in all positions that it fits
     */
    for (unsigned int rot = 0; rot < sym->getNumTransformations(); rot++) {

      voxel_c * rotation = gt->getVoxel(puzzle->probGetShapeShape(problem, pc));
      if (!rotation->transform(rot)) {
        delete rotation;
        continue;
      }

      rotation = addToCache(cache, &cachefill, rotation);

      if (rotation) {
        for (int x = (int)result->boundX1()-(int)rotation->boundX1(); x <= (int)result->boundX2()-(int)rotation->boundX2(); x++)
          for (int y = (int)result->boundY1()-(int)rotation->boundY1(); y <= (int)result->boundY2()-(int)rotation->boundY2(); y++)
            for (int z = (int)result->boundZ1()-(int)rotation->boundZ1(); z <= (int)result->boundZ2()-(int)rotation->boundZ2(); z++)
              if (canPlace(rotation, x, y, z)) {

                int piecenode = AddPieceNode(pc, rot, x+rotation->getHx(), y+rotation->getHy(), z+rotation->getHz());
                placements++;

                /* now add the used cubes of the piece */
                for (unsigned int pz = rotation->boundZ1(); pz <= rotation->boundZ2(); pz++)
                  for (unsigned int py = rotation->boundY1(); py <= rotation->boundY2(); py++)
                    for (unsigned int px = rotation->boundX1(); px <= rotation->boundX2(); px++)
                      if (rotation->getState(px, py, pz) != voxel_c::VX_EMPTY) {
                        AddVoxelNode(columns[result->getIndex(x+px, y+py, z+pz)], piecenode);
                      }

                // if we use the range counting and the piece is using a range, add it to
                // the column
                if (hasRange && (min[pc+1] != max[pc+1]))
                  AddRangeNode(rangeColumn, piecenode, voxels);
              }
        /* for the symmetry breaker piece we also add all symmetries of the box */
        if (pc == symBreakerShape)
          for (unsigned int r = 1; r < sym->getNumTransformationsMirror(); r++)
            if (sym->symmetrieContainsTransformation(resultSym, r)) {

              voxel_c * vx = gt->getVoxel(puzzle->probGetShapeShape(problem, pc));

              if (!vx->transform(rot) || !vx->transform(r)) {
                delete vx;
                continue;
              }

              addToCache(cache, &cachefill, vx);
            }
      }
    }

    for (unsigned int i = 0; i < cachefill; i++)  delete cache[i];

    /* check, if the current piece has at least one placement */
    if (placements == 0) {
      delete [] cache;
      delete [] columns;
      return -puzzle->probGetShape(problem, pc);
    }
  }

  delete [] cache;
  delete [] columns;

  return 1;
}

assembler_1_c::errState assembler_1_c::createMatrix(const puzzle_c * puz, unsigned int prob, bool keepMirror, bool keepRotations) {

  puzzle = puz;
  problem = prob;

  if (!canHandle(puzzle, problem))
    return ERR_PUZZLE_UNHANDABLE;

  /* get and save piece number of puzzle */
  piecenumber = puz->probPieceNumber(prob);

  /* count the filled and variable units */
  unsigned int res_vari = puz->probGetResultShape(prob)->countState(voxel_c::VX_VARIABLE);
  unsigned int res_filled = puz->probGetResultShape(prob)->countState(voxel_c::VX_FILLED) + res_vari;

  for (unsigned int i = 0; i < puz->probShapeNumber(prob); i++)
    if (puz->probGetShapeShape(prob, i)->countState(voxel_c::VX_VARIABLE)) {
      errorsParam = puz->probGetShape(prob, i);
      errorsState = ERR_PIECE_WITH_VARICUBE;
      return errorsState;
    }

  // check if number of voxels in pieces is not bigger than
  // number of voxel in result

  // check if number of filled voxels in result
  // is not bigger than number of voxels in pieces
  unsigned int min = 0;
  unsigned int max = 0;

  for (unsigned int j = 0; j < puz->probShapeNumber(prob); j++) {
    min += puz->probGetShapeShape(prob, j)->countState(voxel_c::VX_FILLED) * puz->probGetShapeMin(prob, j);
    max += puz->probGetShapeShape(prob, j)->countState(voxel_c::VX_FILLED) * puz->probGetShapeMax(prob, j);
  }

  if (min == max)
    holes = res_filled - min;
  else if (puz->probMaxHolesDefined(prob))
    holes = puz->probGetMaxHoles(prob);
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
   * this is calulated here
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

  for (unsigned int j = 0; j < puz->probShapeNumber(prob); j++) {
    if (puz->probGetShapeMin(prob, j) == puz->probGetShapeMax(prob, j)) {
      RangeMin -= puz->probGetShapeShape(prob, j)->countState(voxel_c::VX_FILLED) * puz->probGetShapeMin(prob, j);
      RangeMax -= puz->probGetShapeShape(prob, j)->countState(voxel_c::VX_FILLED) * puz->probGetShapeMin(prob, j);
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
    if (avoidTransformedMirror)
      delete avoidTransformedMirror;
    avoidTransformedMirror = 0;
  }

  if (keepRotations)
    avoidTransformedAssemblies = false;

  errorsState = ERR_NONE;
  return errorsState;
}

void assembler_1_c::remove_column(register unsigned int c) {
  register unsigned int j = c;
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

  unsigned int col = right[0];
  unsigned int removed = 0;

  while (col) {

    /* find all columns that are identical to col
     */

    /* this vector will contain all the columns that are not
     * yet ruled out to be different from col
     * the vector contains the node index
     */
    std::vector<unsigned int>columns;

    unsigned int c = right[col];

    while (c) {
      if (min[c] == min[col] && max[c] == max[col])
        columns.push_back(down[c]);
      c = right[c];
    }

    unsigned int row = down[col];
    unsigned int line = 0;

    while (row != col) {

      while ((line+1 < piecePositions.size()) && (piecePositions[line+1].row <= row))
        line++;

      unsigned int i = 0;

      /* remove all columns that are not in the same
       * line as the column
       */
      while (i < columns.size()) {
        if ((columns[i] < piecePositions[line].row) ||
            ((line+1 < piecePositions.size()) && (columns[i] >= piecePositions[line+1].row)) ||
            (weight[row] != weight[columns[i]])  // also remove row, if weights differ
           ) {
          columns.erase(columns.begin()+i);
        } else
          i++;
      }

      if (columns.size() == 0)
        break;

      row = down[row];

      for (i = 0; i < columns.size(); i++)
        columns[i] = down[columns[i]];
    }

    /* now all the columns need to be in the header again */
    unsigned int i = 0;
    while (i < columns.size()) {
      if (columns[i] >= piecePositions[0].row)
        columns.erase(columns.begin()+i);
      else
        i++;
    }

    for (unsigned int i = 0; i < columns.size(); i++)
      remove_column(columns[i]);

    removed += columns.size();

    col = right[col];
  }

  return removed;
}

void assembler_1_c::remove_row(register unsigned int r) {
  register unsigned int j = r;
  do {
    register unsigned int u, d;

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
   *    check each row r that might be selected to fulfill this column c conditions
   *       if this row results for another column c2 that that column reaches the
   *       maximum possible value (max) then increase counter
   *    also count the number of rows in this column
   *
   *    now check for all columns c2 != c if the number if the counter is equal
   *    to number of rows. If this is the case then that means that
   *    all possibile rows that make column c condition true, also make
   *    column c2 condition so that no other row will come there, so all
   *    rows that are not in the c set but contribute to c2 can be removed
   */
  unsigned int *columns = new unsigned int[headerNodes];

  for (unsigned int col = right[0]; col; col = right[col]) {

    // this is taken from the assembler 0 reduce
    // as I don't know how to generalize the approach
    // we do it only on columns with min = max = 1
    if (min[col] == 0) continue;

    memset(columns, 0, headerNodes * sizeof(unsigned int));

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
  delete [] columns;

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
        // as ony those weights have changed

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

  fprintf(stderr, "removed %i rows and %i columns\n", row_rem, col_rem);
}

void assembler_1_c::checkForTransformedAssemblies(unsigned int pivot, mirrorInfo_c * mir) {
  avoidTransformedAssemblies = true;
  avoidTransformedPivot = pivot;
  avoidTransformedMirror = mir;
}

assembly_c * assembler_1_c::getAssembly(void) {

  assembly_c * assembly = new assembly_c(puzzle->getGridType());

  /* fill the array with 0xff, so that we can distinguish between
   * placed and unplaced pieces
   */
  unsigned int *piece = new unsigned int[rows.size()];
  unsigned char *tran = new unsigned char[rows.size()];
  int *x = new int[rows.size()];
  int *y = new int[rows.size()];
  int *z = new int[rows.size()];

  for (unsigned int i = 0; i < rows.size(); i++)
    getPieceInformation(rows[i], piece+i, tran+i, x+i, y+i, z+i);

  for (unsigned int pc = 0; pc < puzzle->probShapeNumber(problem); pc++) {
    unsigned int placed = 0;
    for (unsigned int i = 0; i < rows.size(); i++) {

      if (piece[i] == pc) {
        assembly->addPlacement(tran[i], x[i], y[i], z[i]);
        placed++;
      }
    }

    while (placed < puzzle->probGetShapeMax(problem, pc)) {
      assembly->addNonPlacement();
      placed++;
    }
  }

  assembly->sort(puzzle, problem);

  delete [] piece;
  delete [] tran;
  delete [] x;
  delete [] y;
  delete [] z;

  return assembly;
}

/* this function handles the assemblies found by the assembler engine
 */
void assembler_1_c::solution(void) {

  if (getCallback()) {

    assembly_c * assembly = getAssembly();

    if (avoidTransformedAssemblies && assembly->smallerRotationExists(puzzle, problem, avoidTransformedPivot, avoidTransformedMirror))
      delete assembly;
    else {
      getCallback()->assembly(assembly);
    }
  }
}

bool assembler_1_c::open_column_conditions_fulfillable(void) {

  for (int col = right[0]; col; col = right[col]) {

    if (weight[col] > max[col]) return false;
    if (weight[col] + colCount[col] < min[col]) return false;
  }

  return true;
}


static bool betterParams(int n_sum, int /*n_min*/, int n_max, int o_sum, int /*o_min*/, int o_max) {

  // we need to find that column that
  // will result in the fewest number of possibilities fo
  // fulfill the column condition. It is not simple to find that
  // in the old version it was simple the number of rows (sum)
  // what was used, now something a bit more scientific


//  if (n_min < 0) n_min = 0;
//  if (o_min < 0) o_min = 0;

  return n_sum*n_max < o_sum*o_max;
}

int assembler_1_c::find_best_unclosed_column(void) {
  int col = right[0];

  // if we have no column -> return no column
  if (col == 0) return -1;

  // first column is best column for the beginning
  int bestcol = col;
  col = right[col];

  while (col) {
    if (betterParams(colCount[col], min[col]-weight[col], max[col]-weight[col],
          colCount[bestcol], min[bestcol]-weight[bestcol], max[bestcol]-weight[bestcol]))

      bestcol = col;

    if (colCount[col] == 0)
      return col;

    col = right[col];
  }

  return bestcol;
}

void assembler_1_c::cover_column_only(int col) {
  right[left[col]] = right[col];
  left[right[col]] = left[col];
}

void assembler_1_c::uncover_column_only(int col) {
  right[left[col]] = col;
  left[right[col]] = col;
}

void assembler_1_c::cover_column_rows(int col) {

  for (int r = down[col]; r != col; r = down[r]) {
    colCount[col] -= weight[r];
    for (int c = right[r]; c != r; c = right[c]) {
      up[down[c]] = up[c];
      down[up[c]] = down[c];
      colCount[colCount[c]] -= weight[c];
    }
  }
}

void assembler_1_c::uncover_column_rows(int col) {

  for (int r = up[col]; r != col; r = up[r]) {
    for (int c = left[r]; c != r; c = left[c]) {
      colCount[colCount[c]] += weight[c];
      up[down[c]] = c;
      down[up[c]] = c;
    }
    colCount[col] += weight[r];
  }
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

// remove all rows from the matrix that would exceed the maximum weight
// in an open column when added to the current weight
// the columns to check are defined by the riven node, all columns
// that are occupiec by this row are checked
//
// each time the function is called the hidden rows are saved and
// can be unhidden by calling the unhiderows function below
// they must come in pairs but can be stacked
void assembler_1_c::hiderows(unsigned int row) {

  // put in the separator
  hidden_rows.push_back(0);

  for (unsigned int r = right[row]; r != row; r = right[r]) {
    int col = colCount[r];

    // this can be sped up by sorting the entries and stopping removal
    // as soon as we reach a valid value, but we have to start from the top
    // we only need to check columns that have a non zero value in the current row
    // as ony those weights have changed

    // now check all rows of this column for too big weights
    for (int rr = down[col]; rr != col; rr = down[rr])
      if (weight[rr] + weight[col] > max[col]) {
        hiderow(rr);
        hidden_rows.push_back(rr);
      }
  }
}

void assembler_1_c::unhiderows(void) {

  while (hidden_rows.back()) {
    unhiderow(hidden_rows.back());
    hidden_rows.pop_back();
  }

  // remvoe separator
  hidden_rows.pop_back();
}

bool assembler_1_c::column_condition_fulfilled(int col) {
  return (weight[col] >= min[col]) && (weight[col] <= max[col]);
}

bool assembler_1_c::column_condition_fulfillable(int col) {
  if (weight[col] > max[col]) return false;
  if (weight[col] + colCount[col] < min[col]) return false;
  return true;
}


#if 0

// this is the recursive version of the method below. I will keep it
// here and also try to keep it up to date to the changes I do to the
// real function because this version is a bit more easy to understand

// this function gets the number of a node
// if the node is 0 then a new column must be selected and covered
// if the node is not 0 then it is the node in the selected column and
// in the row that has to be worked on next

void assembler_1_c::rec(unsigned int next_row) {

  //-------> case 0

  // check holes, if there are too many holes, we return
  if (holes < holeColumns.size()) {
    unsigned int cnt = holes;
    for (int i = 0; i < holeColumns.size(); i++)
      if (colCount[holeColumns[i]] == 0 && weight[holeColumns[i]] == 0)
        if (cnt == 0)
          return;
        else
          cnt--;
  }

  // when we get called with a header node (including 0)
  // we have done all rows and need to select a new column
  if (next_row < headerNodes) {
    // no column selected, so find a new one


    // when no column is left we have found a solution
    if (right[0] == 0) {
      solution();
      return;
    }


    /* probably the same criterium as in the old dancing link algorithm
     * leads to good guesses here, so find the column, with the least
     * number of nodes > 0 in it
     */
    int col = find_best_unclosed_column();

    if (col == -1) {
      return;
    }

    // when there are no rows in the selected column, we don't need to find
    // any row set and can continue right on with a new column
    if (colCount[col] == 0) {

      if (column_condition_fulfilled(col)) {
        // remove this column from the column list
        // we don not need to remove the rows, as there are no
        // and we start a new column
        cover_column_only(col);

        rec(0);

        //-------> case 1

        // reinsert this column
        uncover_column_only(col);
      }

    } else {

      // we can assume here that the columns condition is fulfillable
      // because whenever we call this function all columns that are left
      // must be fulfillable

      // remove this column from the column list
      // do not yet remove the rows of this column, this will be done
      // shortly before we recursively call this function again
      cover_column_only(col);

      // try to find all row sets that fulfill this columns condition
      rec(down[col]);

      //-------> case 1

      // reinsert this column
      uncover_column_only(col);
    }

    return;
  }

  unsigned int col = colCount[next_row];

  // make sure wan can actualla achieve something, the false case sould
  // have been checked before calling this function
  bt_assert(column_condition_fulfillable(col));


  // it might be that the condition for this column is already fulfilled, without adding a single
  // line to the column that is why we do this check here at the start of the function
  if (column_condition_fulfilled(col)) {

    finished_b.push_back(colCount[colCount[next_row]]+1);
    finished_a.push_back(0);

    // remove all rows that are left within this column
    // this way we make sure we are _not_ changing this columns value any more
    cover_column_rows(col);

    if (open_column_conditions_fulfillable())
      rec(0);

    //-------> case 2

    // reinsert rows of this column
    uncover_column_rows(col);

    (finished_a.back())++;

  } else {

    finished_b.push_back(colCount[colCount[next_row]]);
    finished_a.push_back(0);

  }

  //-------> case 3

  // add a unhiderows marker, so that the rows hidden in the loop
  // below can be unhidden properly
  hidden_rows.push_back(0);

  // now try all rows starting with the row given as parametet
  // and go down until we are in the header. When we go up
  // from the header we actually end in a node with a higher
  // number, that's the end check
  //-------> case 4
  for (unsigned int row = next_row; up[row] < row ; row = down[row]) {

    rows.push_back(row);

    // add row to rowset
    weight[colCount[row]] += weight[row];
    for (unsigned int r = right[row]; r != row; r = right[r])
      weight[colCount[r]] += weight[r];

    // if there are unfulfillable columns we don't even need to check any further
    if (open_column_conditions_fulfillable()) {

      // remove useless rows (that are rows that have too much weight
      // in one of their nodes that would overflow the expected weight
      hiderows(row);

      if (open_column_conditions_fulfillable()) {

        if (colCount[col] == 0) {

          // when there are no more rows in the current column
          // we can immeadetly start a new column
          // if the current column condition is really fulfilled
          if (column_condition_fulfilled(col))
            rec(0);

        } else {

          // we need to recurse, if there are rows left and the current
          // column condition is still fulfillable, we need to check
          // the current column again because this column is no longer open,
          // is was removed on selection
          if (column_condition_fulfillable(col)) {

            unsigned int newrow = row;

            // do gown until we hit a row that is still inside the matrix
            // this works because rows are hidden one by one and so the double link
            // to the row above or below is no longer intact, when the row is gone, the down
            // pointer still points to the row that is was below before the row was hidden, but
            // the pointer from the row below doesn't point up to us, so we do down until
            // the link down-up points back to us
            while ((down[newrow] >= headerNodes) && up[down[newrow]] != newrow) newrow = down[newrow];

            rec(newrow);
          }
        }
      }

  //-------> case 5
      // reinsert the rows removed above
      unhiderows();
    }

  //-------> case 6
    // remove row from rowset
    for (unsigned int r = left[row]; r != row; r = left[r])
      weight[colCount[r]] -= weight[r];
    weight[colCount[row]] -= weight[row];

    rows.pop_back();

    (finished_a.back())++;

    // after we finished with this row, we will never use it again, so
    // remove it from the matrix
    hiderow(row);
    hidden_rows.push_back(row);
  }

  //-------> case 7

  // reinsert all the rows that were remove over the course of the
  // row by row inspection
  unhiderows();

  finished_a.pop_back();
  finished_b.pop_back();
}

#endif

void assembler_1_c::iterative(void) {

  unsigned int row, col;

  while (task_stack.size() > 0) {

    // wan can only restore the states 1, 2 and 5. Internal states will alway
    // be one of those, but the last state might differ, so continue looping
    // until the final state is 1, 2 or 5
    if (abbort) {
      if (task_stack.back() == 1 ||
          task_stack.back() == 2 ||
          task_stack.back() == 5)
        break;
    }

    // the debugger
    if (debug) {
      if (task_stack.back() == 1 ||
          task_stack.back() == 2 ||
          task_stack.back() == 5) {
        if (debug_loops <= 0)
          break;

        debug_loops --;
      }
    }

    // the cases in this switch are marked in the function above
    switch (task_stack.back()) {

      case 0:
        // standard entry into function

        // check holes, if there are too many holes, we return
        if (holes < holeColumns.size()) {
          unsigned int cnt = holes;
          bool ret = false;
          unsigned int i;
          for (i = 0; i < holeColumns.size(); i++) {
            if (colCount[holeColumns[i]] == 0 && weight[holeColumns[i]] == 0) {
              if (cnt == 0) {
                next_row_stack.pop_back();
                task_stack.pop_back();
                ret = true;
                break;
              } else {
                cnt--;
              }
            }
          }
          bt_assert(ret == (i < holeColumns.size()));
          if (ret) {
            if (debug) fprintf(stderr, "backtrack because of too many holes\n");
            break;
          }
        }

        // when we get called with a header node (including 0)
        // we have done all rows and need to select a new column
        if (next_row_stack.back() < headerNodes) {
          // no column selected, so find a new one


          // when no column is left we have found a solution
          if (right[0] == 0) {
            solution();
            next_row_stack.pop_back();
            task_stack.pop_back();
            break;
          }

          /* probably the same criterium as in the old dancing link algorithm
           * leads to good guesses here, so find the column, with the least
           * number of nodes > 0 in it
           */
          int col = find_best_unclosed_column();

          if (col == -1) {
            if (debug) fprintf(stderr, "backtrack because could not find column\n");
            next_row_stack.pop_back();
            task_stack.pop_back();
            break;
          }

          if (debug) fprintf(stderr, "found column %i with count %i\n", col, colCount[col]);

          // when there are no rows in the selected column, we don't need to find
          // any row set and can continue right on with a new column
          if (colCount[col] == 0) {

            if (column_condition_fulfilled(col)) {
              // remove this column from the column list
              // we don not need to remove the rows, as there are no
              // and we start a new column
              cover_column_only(col);

              column_stack.push_back(col);
              task_stack.back() = 1;
              task_stack.push_back(0);
              next_row_stack.push_back(0);

              break;
            }

            if (debug) fprintf(stderr, "backtrack because columns condition not fulfilled\n");

          } else {

            // we can assume here that the columns condition is fulfillable
            // because whenever we call this function all columns that are left
            // must be fulfillable

            // remove this column from the column list
            // do not yet remove the rows of this column, this will be done
            // shortly before we recursively call this function again
            cover_column_only(col);

            column_stack.push_back(col);
            task_stack.back() = 1;
            task_stack.push_back(0);
            next_row_stack.push_back(down[col]);

            break;
          }

          next_row_stack.pop_back();
          task_stack.pop_back();
          break;
        }

        col = colCount[next_row_stack.back()];

        // make sure wan can actualla achieve something, the false case sould
        // have been checked before calling this function
        bt_assert(column_condition_fulfillable(col));

        // it might be that the condition for this column is already fulfilled, without adding a single
        // line to the column that is why we do this check here at the start of the function
        if (column_condition_fulfilled(col)) {

          if (debug) fprintf(stderr, "column %i condition fulfilled, recurse\n", col);

          finished_b.push_back(colCount[colCount[next_row_stack.back()]]+1);
          finished_a.push_back(0);

          // remove all rows that are left within this column
          // this way we make sure we are _not_ changing this columns value any more
          cover_column_rows(col);

          if (open_column_conditions_fulfillable()) {
            task_stack.back() = 2;
            task_stack.push_back(0);
            next_row_stack.push_back(0);
            break;
          }

          task_stack.back() = 2;
          break;

        } else {

          finished_b.push_back(colCount[colCount[next_row_stack.back()]]);
          finished_a.push_back(0);
        }

        task_stack.back() = 3;
        break;

      case 1:

        // reinsert this column
        if (debug) fprintf(stderr, "reinserting column %i\n", column_stack.back());

        uncover_column_only(column_stack.back());

        column_stack.pop_back();
        next_row_stack.pop_back();
        task_stack.pop_back();
        break;

      case 2:

        // reinsert rows of this column
        uncover_column_rows(colCount[next_row_stack.back()]);

        (finished_a.back())++;

        // fall through to state 3

      case 3:

        // add a unhiderows marker, so that the rows hidden in the loop
        // below can be unhidden properly
        hidden_rows.push_back(0);

        row = next_row_stack.back();

        if (up[row] < row) {
          rows.push_back(row);
          // fall through to state 4
        } else {
          task_stack.back() = 7;
          break;
        }

        // fall through to state 4

      case 4:

        row = rows.back();
        col = colCount[next_row_stack.back()];

        if (debug) fprintf(stderr, "add row %i for columns %i\n", row, col);

        // add row to rowset
        weight[colCount[row]] += weight[row];
        for (unsigned int r = right[row]; r != row; r = right[r])
          weight[colCount[r]] += weight[r];

        // if there are unfulfillable columns we don't even need to check any further
        if (open_column_conditions_fulfillable()) {

          // remove useless rows (that are rows that have too much weight
          // in one of their nodes that would overflow the expected weight
          hiderows(row);

          if (open_column_conditions_fulfillable()) {

            if (colCount[col] == 0) {

              // when there are no more rows in the current column
              // we can immeadetly start a new column
              // if the current column condition is really fulfilled
              if (column_condition_fulfilled(col)) {

                if (debug) fprintf(stderr, "recurse because columns %i condition fulfilled\n", col);

                task_stack.back() = 5;
                task_stack.push_back(0);
                next_row_stack.push_back(0);
                break;
              }

            } else {

              // we need to recurse, if there are rows left and the current
              // column condition is still fulfillable, we need to check
              // the current column again because this column is no longer open,
              // is was removed on selection
              if (column_condition_fulfillable(col)) {

                unsigned int newrow = row;

                // do gown until we hit a row that is still inside the matrix
                // this works because rows are hidden one by one and so the double link
                // to the row above or below is no longer intact, when the row is gone, the down
                // pointer still points to the row that is was below before the row was hidden, but
                // the pointer from the row below doesn't point up to us, so we do down until
                // the link down-up points back to us
                while ((down[newrow] >= headerNodes) && up[down[newrow]] != newrow) newrow = down[newrow];

                task_stack.back() = 5;
                task_stack.push_back(0);
                next_row_stack.push_back(newrow);
                break;
              }

              if (debug) fprintf(stderr, "no recurse because columns %i condition fulfillable\n", col);
            }
          }

          // fall through to state 5

        } else {

          if (debug) fprintf(stderr, "no recurse because one columns condition fulfillable\n");
          task_stack.back() = 6;
          break;
        }

        // fall through to state 5

      case 5:

        unhiderows();

        // fall through to state 6

      case 6:

        // remove row from rowset
        row = rows.back();

        if (debug) fprintf(stderr, "remove row %i\n", row);

        for (unsigned int r = left[row]; r != row; r = left[r])
          weight[colCount[r]] -= weight[r];
        weight[colCount[row]] -= weight[row];

        rows.pop_back();

        (finished_a.back())++;

        // after we finished with this row, we will never use it again, so
        // remove it from the matrix
        hiderow(row);
        hidden_rows.push_back(row);

        row = down[row];

        if (up[row] < row) {
          rows.push_back(row);
          task_stack.back() = 4;
          break;
        }
          // else fall through to state 7

        // fall through to state 7

      case 7:

        // reinsert all the rows that were remove over the course of the
        // row by row inspection
        unhiderows();

        finished_a.pop_back();
        finished_b.pop_back();

        next_row_stack.pop_back();
        task_stack.pop_back();
        break;

      default:

        bt_assert(0);
        break;
    }
  }
}

void assembler_1_c::assemble(assembler_cb * callback) {

  running = true;
  abbort = false;
  debug = false;

  if (errorsState == ERR_NONE) {

    // run, when something to do
    if (next_row_stack.size()) {
      asm_bc = callback;
      iterative();
    }
  }

  running = false;
}

float assembler_1_c::getFinished(void) {

  if (next_row_stack.size() == 0) return 1;

  float erg = 0;

  for (int r = finished_a.size()-1; r >= 0; r--) {

    erg += finished_a[r];
    erg /= finished_b[r];
  }

  return erg;
}

static unsigned int getInt(const char * s, unsigned int * i) {

  char * s2;

  *i = strtol (s, &s2, 10);

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

  // now get vetor content

  for (unsigned int i = 0; i < count; i++) {
    unsigned int val;
    pos += getInt(string+pos, &val);
    v.push_back(val);
  }

  return pos;
}

assembler_c::errState assembler_1_c::setPosition(const char * string, const char * /* version*/) {

  unsigned int len = strlen(string);

  unsigned int pos = 0;

  pos += stringToVector(string+pos, rows);           if (pos >= len) return ERR_CAN_NOT_RESTORE_SYNTAX;
  pos += stringToVector(string+pos, task_stack);     if (pos >= len) return ERR_CAN_NOT_RESTORE_SYNTAX;
  pos += stringToVector(string+pos, next_row_stack); if (pos >= len) return ERR_CAN_NOT_RESTORE_SYNTAX;
  pos += stringToVector(string+pos, column_stack);   if (pos >= len) return ERR_CAN_NOT_RESTORE_SYNTAX;
  pos += stringToVector(string+pos, hidden_rows);    if (pos >= len) return ERR_CAN_NOT_RESTORE_SYNTAX;
  pos += stringToVector(string+pos, finished_a);     if (pos >= len) return ERR_CAN_NOT_RESTORE_SYNTAX;
  pos += stringToVector(string+pos, finished_b);

  // not we need to restore the matrix to the right state

  unsigned int column_stack_pos = 0;
  unsigned int col, row;
  unsigned int hiderows_pos = 0;
  unsigned int row_pos = 0;

  for (unsigned int i = 0; i < task_stack.size(); i++) {

    switch (task_stack[i]) {

      case 1:
        cover_column_only(column_stack[column_stack_pos++]);
        break;
      case 2:
        if (column_stack_pos == 0) return ERR_CAN_NOT_RESTORE_SYNTAX;
        col = column_stack[column_stack_pos-1];
        cover_column_rows(col);
        break;
      case 5:
        hiderows_pos++;
        while (hiderows_pos < hidden_rows.size() && hidden_rows[hiderows_pos] > 0) {
          hiderow(hidden_rows[hiderows_pos++]);
        }

        row = rows[row_pos++];
        // add row to rowset
        weight[colCount[row]] += weight[row];
        for (unsigned int r = right[row]; r != row; r = right[r])
          weight[colCount[r]] += weight[r];

        hiderows_pos++;
        while (hiderows_pos < hidden_rows.size() && hidden_rows[hiderows_pos] > 0) {
          hiderow(hidden_rows[hiderows_pos++]);
        }

        break;

      default:
        return ERR_CAN_NOT_RESTORE_SYNTAX;
    }
  }

  return ERR_NONE;
}

static std::string vectorToString(const std::vector<unsigned int> & v) {

  std::string cont;

  char tmp[100];

  snprintf(tmp, 100, "%i ", v.size());
  cont += tmp;

  for (unsigned int i = 0; i < v.size(); i++) {
    snprintf(tmp, 100, "%i ", v[i]);
    cont += tmp;
  }

  return cont;
}

xml::node assembler_1_c::save(void) const {

  xml::node nd("assembler");

  nd.get_attributes().insert("version", ASSEMBLER_VERSION);

  std::string cont;

  cont += vectorToString(rows);
  cont += vectorToString(task_stack);
  cont += vectorToString(next_row_stack);
  cont += vectorToString(column_stack);
  cont += vectorToString(hidden_rows);
  cont += vectorToString(finished_a);
  cont += vectorToString(finished_b);

  nd.set_content(cont.c_str());

  return nd;
}

unsigned int assembler_1_c::getPiecePlacement(unsigned int node, int delta, unsigned int piece, unsigned char *tran, int *x, int *y, int *z) {

  /* piece 2 shape */
  unsigned int pp = 0;
  unsigned int shape = 0;
  while (pp + puzzle->probGetShapeMax(problem, shape) <= piece) {
    pp += puzzle->probGetShapeMax(problem, shape);
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

unsigned int assembler_1_c::getPiecePlacementCount(unsigned int piece) {

  /* piece 2 shape */
  unsigned int pp = 0;
  unsigned int shape = 0;
  while (pp + puzzle->probGetShapeMax(problem, shape) <= piece) {
    pp += puzzle->probGetShapeMax(problem, shape);
    shape++;
  }

  return colCount[shape+1];
}

void assembler_1_c::debug_step(unsigned long num) {
  debug = true;
  debug_loops = num;
  abbort = false;
  asm_bc = 0;
  iterative();
  debug = false;
}

bool assembler_1_c::canHandle(const puzzle_c *, unsigned int /*problem*/) {

  // right now there are no limits

  return true;
}

