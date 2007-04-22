/* Burr Solver
 * Copyright (C) 2003-2007  Andreas R�ver
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

/* output the whole table for debug reasons */
static void printtable(const std::vector<unsigned int> & left,
    const std::vector<unsigned int> & right,
    const std::vector<unsigned int> & up,
    const std::vector<unsigned int> & down,
    const std::vector<unsigned int> & colCount,
    const std::vector<unsigned int> & weight) {

  printf("============\n");

  printf("        ");
  int cc = 1;
  for (int c = right[0]; c != 0; c = right[c]) {
    while (cc < c) {
      printf("  d");
      cc++;
    }
    cc++;
    printf("%3i", c);
  }
  printf("\n        ");
  cc = 1;
  for (int c = right[0]; c != 0; c = right[c]) {
    while (cc < c) {
      printf("  d");
      cc++;
    }
    cc++;
    printf("%3i", colCount[c]);
  }
  printf("\n        ");
  cc = 1;
  for (int c = right[0]; c != 0; c = right[c]) {
    while (cc < c) {
      printf("  d");
      cc++;
    }
    cc++;
    printf("%3i", weight[c]);
  }
  printf("\n");
  printf("------------\n");

  for (int c = right[0]; c != 0; c = right[c]) {
    for (int r = down[c]; r != c; r = down[r]) {

      if (left[r] < r) return;

      cc = 1;
      int c2 = r;

      printf("%8i", r);
      do {

        int ccn = colCount[c2];

        while (cc < ccn) {
          printf("   ");
          cc++;
        }

        printf("%3i", weight[c2]);
        c2 = right[c2];
        cc++;
      } while (c2 != r);
      printf("\n");
    }
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

assembler_1_c::assembler_1_c(assemblerFrontend_c * fe) :
  assembler_c(fe),
  avoidTransformedAssemblies(0), avoidTransformedMirror(0)
{
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
int assembler_1_c::prepare(int res_filled, int res_vari) {

  const voxel_c * result = puzzle->probGetResultShape(problem);

  /* nodes 1..n are the columns nodes */
  GenerateFirstRow(result->countState(voxel_c::VX_FILLED)+result->countState(voxel_c::VX_VARIABLE)+puzzle->probShapeNumber(problem));

  /* this array contains the column in our matrix that corresponds with
   * the voxel position inside the result. We use this matrix because
   * the calculation of the exact column depends on the number of FILLED
   * and VARIABLE voxels and is relatively expensive to calculate
   * with this lookup I was able to reduce the preparation time
   * from 5 to 0.5 seconds for TheLostDay puzzle
   */
  unsigned int * columns = new unsigned int[result->getXYZ()];

  {
    int c = 1 + puzzle->probShapeNumber(problem);

    for (unsigned int i = 0; i < result->getXYZ(); i++) {
      switch(result->getState(i)) {
      case voxel_c::VX_VARIABLE:
        min[c] = 0;
      case voxel_c::VX_FILLED:
        columns[i] = c++;
        break;
      default:
        columns[i] = 0;
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
    unsigned int pc = puzzle->probGetShapeCount(problem, 0);
    unsigned int bestFound = sym->countSymmetryIntersection(resultSym, puzzle->probGetShapeShape(problem, 0)->selfSymmetries());
    symBreakerShape = 0;

    for (unsigned int i = 1; i < puzzle->probShapeNumber(problem); i++) {

      unsigned int cnt = sym->countSymmetryIntersection(resultSym, puzzle->probGetShapeShape(problem, i)->selfSymmetries());

      if ((puzzle->probGetShapeCount(problem, i) < puzzle->probGetShapeCount(problem, symBreakerShape)) ||
          (puzzle->probGetShapeCount(problem, i) == puzzle->probGetShapeCount(problem, symBreakerShape)) && (cnt < bestFound)) {
        bestFound = cnt;
        symBreakerShape = i;
        symBreakerPiece = pc;
      }

      pc += puzzle->probGetShapeCount(problem, i);
    }

    bool tmp = sym->symmetriesLeft(resultSym, puzzle->probGetShapeShape(problem, symBreakerShape)->selfSymmetries());


    if (tmp || (puzzle->probGetShapeCount(problem, symBreakerShape) > 1)) {

      // we can not use the symmetry breaker shape, if there is more than one piece
      // of this shape in the problem
      if (puzzle->probGetShapeCount(problem, symBreakerShape) > 1) {
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
        for (unsigned int p = 0; p < puzzle->probGetShapeCount(problem, i); p++) {
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

      if (mirrorCheck) {
        /* all the shapes are either self mirroring or have a mirror pair
         * so we create the mirror structure and we do the mirror check
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

//  printf("matrix creation\n");

  /* now we insert one shape after another */
  for (unsigned int pc = 0; pc < puzzle->probShapeNumber(problem); pc++) {

    // setup weight values so that they do fit the number of pieces for this
    // shape
    min[pc+1] = max[pc+1] = puzzle->probGetShapeCount(problem, pc);
//    printf("set min max for column %i to %i\n", pc+1, max[pc+1]);

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

//    printf("piece %i has %i placements\n", pc, placements);

    /* check, if the current piece has at least one placement */
    if (placements == 0) {
      delete [] cache;
      delete [] columns;
      return -puzzle->probGetShape(problem, pc);
    }
  }

  delete [] cache;
  delete [] columns;

//  printf("%i %i %i %i %i %i %i %i\n", up.size(), down.size(), left.size(), right.size(), colCount.size(), weight.size(), min.size(), max.size());

//  printf("%i %i %i\n", colCount[1], colCount[2], colCount[3]);

  return 1;
}

assembler_1_c::errState assembler_1_c::createMatrix(const puzzle_c * puz, unsigned int prob) {

  puzzle = puz;
  problem = prob;

  /* get and save piece number of puzzle */
  piecenumber = puz->probPieceNumber(prob);

  /* count the filled and variable units */
  int res_vari = puz->probGetResultShape(prob)->countState(voxel_c::VX_VARIABLE);
  int res_filled = puz->probGetResultShape(prob)->countState(voxel_c::VX_FILLED) + res_vari;

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
  int h = res_filled;

  for (unsigned int j = 0; j < puz->probShapeNumber(prob); j++)
    h -= puz->probGetShapeShape(prob, j)->countState(voxel_c::VX_FILLED) * puz->probGetShapeCount(prob, j);

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

  /* fill the nodes arrays */
  int error = prepare(res_filled, res_vari);

  // check, if there is one piece not placeable
  if (error <= 0) {
    errorsState = ERR_CAN_NOT_PLACE;
    errorsParam = -error;
    return errorsState;
  }

//  printtable(left, right, up, down, colCount, weight);

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

      while ((line < piecePositions.size()) && (piecePositions[line+1].row <= row)) line++;

      unsigned int i = 0;

      /* remove all columns that are not in the same
       * line as the column
       */
      while (i < columns.size()) {
        if ((columns[i] < piecePositions[line].row) ||
            (columns[i] >= piecePositions[line+1].row) ||
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

//  printtable(left, right, up, down, colCount, weight);
  std::vector<unsigned int> toRemove;

  unsigned int col_rem = clumpify();

  printf("checking %i lines\n", piecePositions.size());

  for (unsigned int pp = 0; pp < piecePositions.size(); pp++) {

    unsigned int row = piecePositions[pp].row;

    weight[colCount[row]] += weight[row];

    unsigned int r;

    // add row to rowset
    for (r = right[row]; r != row; r = right[r]) {
      int col = colCount[r];

      weight[col] += weight[r];
    }

    std::vector<unsigned int>hidden_rows;

    for (r = right[row]; r != row; r = right[r]) {
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

//    printtable(left, right, up, down, colCount, weight);

    if (!open_column_conditions_fulfillable())
      toRemove.push_back(row);

    while (!hidden_rows.empty()) {
      unhiderow(hidden_rows.back());
      hidden_rows.pop_back();
    }

    r = left[row];

    // remove row from rowset
    // we only need to restart adding the row from the place we stopped
    // when we added
    for (; r != row; r = left[r]) {
      int col = colCount[r];

      weight[col] -= weight[r];
    }

    weight[colCount[row]] -= weight[row];
  }

  printf("removing %i rows\n", toRemove.size());

  while (!toRemove.empty()) {
    remove_row(toRemove.back());
    toRemove.pop_back();
  }
//  printtable(left, right, up, down, colCount, weight);

  col_rem += clumpify();

  printf("%i columns removed\n", col_rem);

}

void assembler_1_c::checkForTransformedAssemblies(unsigned int pivot, mirrorInfo_c * mir) {
  avoidTransformedAssemblies = true;
  avoidTransformedPivot = pivot;
  avoidTransformedMirror = mir;
}

assembly_c * assembler_1_c::getAssembly(void) {

  assembly_c * assembly = new assembly_c(puzzle->getGridType());

  // if no pieces are placed, or we finished return an empty assembly
  if (rows.size() < getPiecenumber()) {
    for (unsigned int i = 0; i < getPiecenumber(); i++)
      assembly->addNonPlacement();
    return assembly;
  }

  /* fill the array with 0xff, so that we can distinguish between
   * placed and unplaced pieces
   */

  for (unsigned int pc = 0; pc < getPiecenumber(); pc++)
    for (unsigned int i = 0; i < rows.size(); i++) {
      unsigned char tran;
      int x, y, z;
      unsigned int piece;

      getPieceInformation(rows[i], &piece, &tran, &x, &y, &z);

      if (piece == pc) {
        assembly->addPlacement(tran, x, y, z);
      }
    }

  assembly->sort(puzzle, problem);

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

int assembler_1_c::find_best_unclosed_column(void) {
  int col = right[0];

  // if we have no column -> return no column
  if (col == 0) return -1;

  // first column is best column for the beginning
  int bestcol = col;
  col = right[col];

  while (col) {
    if (colCount[col] < colCount[bestcol])
      bestcol = col;
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

bool assembler_1_c::column_condition_fulfilled(int col) {
  return (weight[col] >= min[col]) && (weight[col] <= max[col]);
}

void assembler_1_c::rec1(void) {

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

  // remove this column from the column list
  // do not yet remove the rows of this column, this will be done
  // shortly before we recursively call this function again
  cover_column_only(col);

  // try to find all row sets that fulfill this columns condition
  rec2(down[col]);

  // reinsert this column and all its rows
  uncover_column_only(col);
}

void assembler_1_c::rec2(int next_row) {

  unsigned int col = next_row;
  if (col >= headerNodes)
    col = colCount[next_row];

  unsigned int cnt = 0;

  // it might be that the condition for this column is already fulfilled, without adding a single
  // line to the column that is why we do this check here at the start of the function
  if (column_condition_fulfilled(col)) {

    finished_b.push_back(colCount[colCount[next_row]]+1);
    finished_a.push_back(cnt);

    // remove all rows that are left within this column
    // this way we make sure we are _not_ changing this columns value any more
    cover_column_rows(col);

    rec1();

    // reinsert rows of this column
    uncover_column_rows(col);

    finished_a.pop_back();
    cnt++;

  } else {

    finished_b.push_back(colCount[colCount[next_row]]);

  }


  for (unsigned int row = next_row; ; row = down[row]) {

    if (up[row] >= row) break;

    finished_a.push_back(cnt);
    rows.push_back(row);

    weight[colCount[row]] += weight[row];

    unsigned int r;

    // add row to rowset
    for (r = right[row]; r != row; r = right[r]) {
      int col = colCount[r];

      weight[col] += weight[r];

      // if we find a unfulfillable column, we stop the loop
      // this hoefully saves us some hiderow and unhiderow
      if (weight[col] > max[col]) break;
      if (weight[col] + colCount[col] < min[col]) break;
    }

    // if we stopped the loop there is a column that is not
    // fulfillable, so we don't even need to check
    if (r == row) {

      // but we need to check all columns now, as it might be
      // possible that another column, where we have removed a row
      // is not fulfillable any longer
      if (open_column_conditions_fulfillable()) {

        std::vector<unsigned int>hidden_rows;

        for (r = right[row]; r != row; r = right[r]) {
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

        unsigned int newrow = down[row];

        // do gown until we hit a row that is still inside the matrix
        while ((down[newrow] >= headerNodes) && up[down[newrow]] != newrow) newrow = down[newrow];

        if (newrow < headerNodes)
          newrow = row;

        rec2(newrow);

        while (!hidden_rows.empty()) {
          unhiderow(hidden_rows.back());
          hidden_rows.pop_back();
        }
      }

      // when we have completely finished adding the row, we need to
      // set this row counter back one step to make sure we undo properly
      r = left[r];
    }

    // remove row from rowset
    // we only need to restart adding the row from the place we stopped
    // when we added
    for (; r != row; r = left[r]) {
      int col = colCount[r];

      weight[col] -= weight[r];
    }

    weight[colCount[row]] -= weight[row];

    rows.pop_back();

    finished_a.pop_back();
    cnt++;
  }

  finished_b.pop_back();
}

void assembler_1_c::assemble(assembler_cb * callback) {

  running = true;

  if (errorsState == ERR_NONE) {
    asm_bc = callback;
    if (open_column_conditions_fulfillable()) {
      rec1();
    }
  }

  running = false;
}

float assembler_1_c::getFinished(void) {

  float erg = 0;

  for (int r = finished_a.size()-1; r >= 0; r--) {

    erg += finished_a[r];
    erg /= finished_b[r];
  }

  return erg;
}

assembler_c::errState assembler_1_c::setPosition(const char * string, const char * version) {

  return ERR_CAN_NOT_RESTORE_VERSION;
}

xml::node assembler_1_c::save(void) const {

  xml::node nd;

  return nd;
}

bool assembler_1_c::canHandle(const puzzle_c * p, unsigned int problem) {

  // right now there are no limits

  return true;
}

