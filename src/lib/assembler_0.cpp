/* Burr Solver
 * Copyright (C) 2003-2009  Andreas Röver
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

#include "bt_assert.h"
#include "problem.h"
#include "voxel.h"
#include "assembly.h"
#include "gridtype.h"
#include <cstdlib>
#include <cstring>

#include <config.h>

#include <xmlwrapp/attributes.h>

#ifdef WIN32
#define snprintf _snprintf
#endif

#define ASSEMBLER_VERSION "1.4"

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

      if (left[right[c]] != c) printf("lr %i\n", c);
      if (right[left[c]] != c) printf("rl %i\n", c);
      if (up(down(c)) != c) printf("ud %i\n", c);
      if (down(up(c)) != c) printf("du %i\n", c);
      cnt++;

      unsigned int r = down(c);

      while (r != c) {

        if (left[right[r]] != r) printf("lr %i\n", r);
        if (right[left[r]] != r) printf("rl %i\n", r);
        if (up(down(r)) != r) printf("ud %i\n", r);
        if (down(up(r)) != r) printf("du %i\n", r);
        cnt++;

        r = down(r);

      }

      c = right[c];
    }

    printf("checked %li nodes for consistency\n", cnt);
  }

  printf("%i %i\n", varivoxelStart, varivoxelEnd);

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
    unsigned int column = 0;
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

void assembler_0_c::getPieceInformation(unsigned int node, unsigned char *tran, int *x, int *y, int *z, unsigned int *piece) {

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

/* find identical columns within the matrix and remove all but one
 * of these identical columns, this will not save us iterations, but will
 * make the iterations much cheaper
 *
 * returns the number of columns that were removed
 */
unsigned int assembler_0_c::clumpify(void) {

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
      columns.push_back(down(c));
      c = right[c];
    }

    unsigned int row = down(col);
    unsigned int line = 0;

    while (row != col) {

      while ((line < piecePositions.size()) && (piecePositions[line+1].row <= row)) line++;

      unsigned int i = 0;

      /* remove all columns that are not in the same
       * line as the column
       */
      while (i < columns.size()) {
        if ((columns[i] < piecePositions[line].row) ||
            (columns[i] >= piecePositions[line+1].row)
           ) {
          columns.erase(columns.begin()+i);
        } else
          i++;
      }

      if (columns.size() == 0)
        break;

      row = down(row);

      for (i = 0; i < columns.size(); i++)
        columns[i] = down(columns[i]);
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

assembler_0_c::assembler_0_c(void) :
  assembler_c(),
  pos(0), rows(0), columns(0),
  avoidTransformedAssemblies(0), avoidTransformedMirror(0)
{
}

assembler_0_c::~assembler_0_c() {
  if (rows) delete [] rows;
  if (columns) delete [] columns;

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


bool assembler_0_c::canPlace(const voxel_c * piece, int x, int y, int z) const {

  if (!piece->onGrid(x, y, z))
    return false;

  const voxel_c * result = puzzle->getResultShape();

  for (unsigned int pz = piece->boundZ1(); pz <= piece->boundZ2(); pz++)
    for (unsigned int py = piece->boundY1(); py <= piece->boundY2(); py++)
      for (unsigned int px = piece->boundX1(); px <= piece->boundX2(); px++)
        if (
            // the piece can not be place if the result is empty and the piece is filled at a given voxel
            ((piece->getState(px, py, pz) != voxel_c::VX_EMPTY) &&
             (result->getState(x+px, y+py, z+pz) == voxel_c::VX_EMPTY)) ||

            // the piece can also not be placed when the colour constraints don't fit
            !puzzle->placementAllowed(piece->getColor(px, py, pz), result->getColor(x+px, y+py, z+pz))

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

  const voxel_c * result = puzzle->getResultShape();

  /* this array contains the column in our matrix that corresponds with
   * the voxel position inside the result. We use this matrix because
   * the calculation of the exact column depends on the number of FILLED
   * and VARIABLE voxels and is relatively expensive to calculate
   * with this lookup I was able to reduce the preparation time
   * from 5 to 0.5 seconds for TheLostDay puzzle
   */
  unsigned int * columns = new unsigned int[result->getXYZ()];
  unsigned int piecenumber = puzzle->pieceNumber();

  /* voxelindex is the inverse of the function column. It returns
   * the index (not x, y, z) of a given column in the matrix
   */
  int * voxelindex = new int[result->getXYZ() + piecenumber + 1];

  for (unsigned int i = 0; i < result->getXYZ() + piecenumber + 1; i++)
    voxelindex[i] = -1;

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
    unsigned int bestFound = sym->countSymmetryIntersection(resultSym, puzzle->getShapeShape(0)->selfSymmetries());
    symBreakerShape = 0;

    for (unsigned int i = 1; i < puzzle->shapeNumber(); i++) {

      unsigned int cnt = sym->countSymmetryIntersection(resultSym, puzzle->getShapeShape(i)->selfSymmetries());

      if (cnt < bestFound) {
        bestFound = cnt;
        symBreakerShape = i;
      }
    }

    if (sym->symmetriesLeft(resultSym, puzzle->getShapeShape(symBreakerShape)->selfSymmetries()))
      checkForTransformedAssemblies(symBreakerShape, 0);

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

      typedef struct {
        unsigned int shape;    // the shape of this piece
        unsigned int mirror;   // the mirror shape of this piece
        unsigned int trans;
      } mm;

      mm * mirror = new mm[puzzle->pieceNumber()];

      // first initialize
      for (unsigned int i = 0; i < puzzle->shapeNumber(); i++) {
        mirror[i].shape = i;
        mirror[i].mirror = (unsigned int)-1;
        mirror[i].trans = 255;
      }

      bool mirrorCheck = true;

      // now go over all shapes
      for (unsigned int i = 0; i < puzzle->pieceNumber(); i++) {

        // we have already found the mirror for this shape
        if (mirror[i].mirror < puzzle->pieceNumber())
          continue;

        if (!sym->symmetryContainsMirror(puzzle->getShapeShape(mirror[i].shape)->selfSymmetries())) {
          /* this shape is not self mirroring, so we need to look out
           * for a shape that is the mirror of this shape
           */
          bool found = false;

          // now see if we can find another shape that is the mirror of the current shape
          for (unsigned int j = i+1; j < puzzle->pieceNumber(); j++) {

            if (mirror[j].mirror < puzzle->pieceNumber())
              continue;

            unsigned int trans = puzzle->getShapeShape(mirror[i].shape)->getMirrorTransform(
                puzzle->getShapeShape(mirror[j].shape));

            if (trans > 0) {
              // found a mirror shape

              mirror[i].mirror = j;
              mirror[i].trans = trans;
              mirror[j].mirror = i;
              mirror[j].trans = puzzle->getShapeShape(mirror[j].shape)->getMirrorTransform(
                  puzzle->getShapeShape(mirror[i].shape));

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

        for (unsigned int i = 0; i < puzzle->pieceNumber(); i++)
          if (mirror[i].trans != 255)
            mir->addPieces(i, mirror[i].mirror, mirror[i].trans);

        checkForTransformedAssemblies(symBreakerShape, mir);
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

  /* nodes 1..n are the columns nodes */
  GenerateFirstRow();

  voxel_c ** cache = new voxel_c *[sym->getNumTransformationsMirror()];

  /* now we insert one shape after another */
  for (unsigned int pc = 0; pc < puzzle->shapeNumber(); pc++) {

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

      voxel_c * rotation = gt->getVoxel(puzzle->getShapeShape(pc));
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
                placements = 1;

                /* now add the used cubes of the piece */
                for (unsigned int pz = rotation->boundZ1(); pz <= rotation->boundZ2(); pz++)
                  for (unsigned int py = rotation->boundY1(); py <= rotation->boundY2(); py++)
                    for (unsigned int px = rotation->boundX1(); px <= rotation->boundX2(); px++)
                      if (rotation->getState(px, py, pz) != voxel_c::VX_EMPTY)
                        AddVoxelNode(columns[result->getIndex(x+px, y+py, z+pz)], piecenode);
              }


        /* for the symmetry breaker piece we also add all symmetries of the box */
        if (pc == symBreakerShape)
          for (unsigned int r = 1; r < sym->getNumTransformations(); r++)
            if (sym->symmetrieContainsTransformation(resultSym, r)) {

              voxel_c * vx = gt->getVoxel(puzzle->getShapeShape(pc));

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
      delete [] voxelindex;
      return -puzzle->getShape(pc);
    }
  }

  delete [] cache;
  delete [] columns;
  delete [] voxelindex;

  return 1;
}



assembler_0_c::errState assembler_0_c::createMatrix(const problem_c * puz, bool keepMirror, bool keepRotations, bool comp) {

  puzzle = puz;
  complete = comp;

  if (!canHandle(puzzle))
    return ERR_PUZZLE_UNHANDABLE;

  /* get and save piece number of puzzle */
  piecenumber = puz->pieceNumber();

  /* count the filled and variable units */
  int res_vari = puz->getResultShape()->countState(voxel_c::VX_VARIABLE);
  int res_filled = puz->getResultShape()->countState(voxel_c::VX_FILLED) + res_vari;

  for (unsigned int i = 0; i < puz->shapeNumber(); i++)
    if (puz->getShapeShape(i)->countState(voxel_c::VX_VARIABLE)) {
      errorsParam = puz->getShape(i);
      errorsState = ERR_PIECE_WITH_VARICUBE;
      return errorsState;
    }

  varivoxelStart = 1 + piecenumber + res_filled - res_vari;
  varivoxelEnd = 1 + piecenumber + res_filled;

  // check if number of voxels in pieces is not bigger than
  // number of voxel in result

  // check if number of filled voxels in result
  // is not bigger than number of voxels in pieces
  int h = res_filled;

  for (unsigned int j = 0; j < puz->shapeNumber(); j++)
    h -= puz->getShapeShape(j)->countState(voxel_c::VX_FILLED);

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
  rows = new unsigned int[piecenumber];
  columns = new unsigned int [piecenumber];

  /* fill the nodes arrays */
  int error = prepare();

  // check, if there is one piece not placeable
  if (error <= 0) {
    errorsState = ERR_CAN_NOT_PLACE;
    errorsParam = -error;
    return errorsState;
  }

  memset(rows, 0, piecenumber * sizeof(int));
  memset(columns, 0, piecenumber * sizeof(int));
  pos = 0;
  iterations = 0;

  if (keepMirror)
    avoidTransformedMirror = 0;

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
void assembler_0_c::cover_row(register unsigned int r) {
  for (unsigned int j = right[r]; j != r; j = right[j])
    cover(colCount[j]);
}

bool assembler_0_c::try_cover_row(register unsigned int r, unsigned int * columns) {

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

void assembler_0_c::uncover_row(register unsigned int r) {
  for (unsigned int j = left[r]; j != r; j = left[j])
    uncover(colCount[j]);
}

void assembler_0_c::remove_row(register unsigned int r) {
  register unsigned int j = r;
  do {
    register unsigned int u, d;

    colCount[colCount[j]]--;

    u = up(j);
    d = down(j);

    up(d) = u;
    down(u) = d;

    j = right[j];
  } while (j != r);
}

void assembler_0_c::remove_column(register unsigned int c) {
  register unsigned int j = c;
  do {
    right[left[j]] = right[j];
    left[right[j]] = left[j];

    j = down(j);
  } while (j != c);
}

void assembler_0_c::reinsert_row(register unsigned int r) {
  register unsigned int j = r;
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
  unsigned int *columns = new unsigned int[varivoxelEnd];
  unsigned int removed = 0;
  unsigned int remCol = 0;
  bool rem_sth;

  // we first collect all the rows that we finally want to
  // remove and only remove them after the complete check
  std::vector<unsigned int> rowsToRemove;

  reducePiece = 0;

  for (unsigned int col = right[0]; col; col = right[col]) {

    memset(columns, 0, varivoxelEnd * sizeof(unsigned int));

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
      // infillable holes or unplaceable pieces or whatever
      // conditions that make a solution impossible
      cover(p+1);

      rowsToRemove.clear();

      // go over all the placements of the piece and check, if
      // each for possibility
      for (unsigned int r = down(p+1); r != p+1; r = down(r)) {

        // try to do this placement, if the placing goes
        // wrong already, we don't need to do the deep check
        if (!try_cover_row(r, columns)) {
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
      memset(columns, 0, varivoxelEnd * sizeof(unsigned int));
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

  delete [] columns;

  remCol += clumpify();

  fprintf(stderr, "removed %i rows and %i columns\n", removed, remCol);
}

assembly_c * assembler_0_c::getAssembly(void) {

  assembly_c * assembly = new assembly_c(puzzle->getGridType());

  // if no pieces are placed, or we finished return an empty assembly
  if (pos > piecenumber) {
    for (unsigned int i = 0; i < getPiecenumber(); i++)
      assembly->addNonPlacement();
    return assembly;
  }

  bt_assert(getPos() <= getPiecenumber());

  /* first we need to find the order the piece are in */
  unsigned int * pieces = new unsigned int[getPiecenumber()];
  unsigned char * trans = new unsigned char[getPiecenumber()];
  int * xs = new int[getPiecenumber()];
  int * ys = new int[getPiecenumber()];
  int * zs = new int[getPiecenumber()];

  /* fill the array with 0xff, so that we can distinguish between
   * placed and unplaced pieces
   */
  memset(pieces, 0xff, sizeof(unsigned int) * getPiecenumber());

  for (unsigned int i = 0; i < getPos(); i++) {
    unsigned char tran;
    int x, y, z;
    unsigned int piece;

    getPieceInformation(getRows(i), &tran, &x, &y, &z, &piece);

    pieces[piece] = i;
    trans[piece] = tran;
    xs[piece] = x;
    ys[piece] = y;
    zs[piece] = z;
  }

  for (unsigned int i = 0; i < getPiecenumber(); i++)
    if (pieces[i] >= getPos())
      assembly->addNonPlacement();
    else
      assembly->addPlacement(trans[i], xs[i], ys[i], zs[i]);

  delete [] pieces;
  delete [] trans;
  delete [] xs;
  delete [] ys;
  delete [] zs;

  // sort is not necessary because there is only one of each piece
  // assembly->sort(puzzle, problem);

  return assembly;
}

void assembler_0_c::checkForTransformedAssemblies(unsigned int pivot, mirrorInfo_c * mir) {
  avoidTransformedAssemblies = true;
  avoidTransformedPivot = pivot;
  avoidTransformedMirror = mir;
}

/* this function handles the assemblies found by the assembler engine
 */
void assembler_0_c::solution(void) {

  if (getCallback()) {

    assembly_c * assembly = getAssembly();

    if (avoidTransformedAssemblies && assembly->smallerRotationExists(puzzle, avoidTransformedPivot, avoidTransformedMirror, complete))
      delete assembly;
    else {
      getCallback()->assembly(assembly);
    }
  }
}

/* to understand this function you need to first completely understand the
 * dancing link algorithm.
 */
void assembler_0_c::iterativeMultiSearch(void) {

  abbort = false;
  running = true;

  // this variable is used to store if we continue with our loop over
  // the rows or have finished
  bool cont;

  while (!abbort) {

    // we have finished if pos negative (or greater than piecenumber because of the
    // overflow
    if (pos > piecenumber)
      break;

    // check, if all pieces are placed and all voxels are filled
    // careful here abort is also modified by another thread
    // i once used the expression
    //   abort |= !solution()
    // and search halve a day why it didn't work. The value of abort was read
    // then the function called then the new value calculated then the new value
    // written. Meanwhile abort was pressed and abort was changed. This new value got lost.
    if (!right[0])
      solution();

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
    iterations++;

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
        register unsigned int j = right[c];

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
        register unsigned int j = right[varivoxelEnd];

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

  running = false;
}

void assembler_0_c::assemble(assembler_cb * callback) {

  debug = false;

  if (errorsState == ERR_NONE) {
    asm_bc = callback;
    iterativeMultiSearch();
  }
}

float assembler_0_c::getFinished(void) {

  /* we don't need locking, as I hope that I have written the
   * code in a way that updated the data so, that it will never
   * be in an inconsistent state. The thing that will happen is that
   * the value may jump
   */

  if (!rows || !columns || !upDown.size())
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

assembler_c::errState assembler_0_c::setPosition(const char * string, const char * version) {

  /* we assert that the matrix is in the initial position
   * otherwise we would have to clean the stack
   */
  bt_assert(pos == 0);

  /* check for the right version */
  if (strcmp(version, ASSEMBLER_VERSION))
    return ERR_CAN_NOT_RESTORE_VERSION;

  unsigned int len = strlen(string);
  unsigned int spos = 0;

  /* get the values from the string.
   */
  spos += getInt(string+spos, &pos);
  if (spos >= len) return ERR_CAN_NOT_RESTORE_SYNTAX;

  spos += getLong(string+spos, &iterations);
  if (spos >= len) return ERR_CAN_NOT_RESTORE_SYNTAX;

  if (pos <= piecenumber)
    for (unsigned int i = 0; i <= pos; i++) {
      while ((spos < len) && (*(string+spos) != '(')) spos++;
      spos++;
      if (spos >= len) return ERR_CAN_NOT_RESTORE_SYNTAX;

      spos += getInt(string+spos, &rows[i]);      if (spos >= len) return ERR_CAN_NOT_RESTORE_SYNTAX;
      spos += getInt(string+spos, &columns[i]);   if (spos >= len) return ERR_CAN_NOT_RESTORE_SYNTAX;

      while ((spos < len) && (*(string+spos) != ')')) spos++;
      spos++;
      // after the last ')' we might be at the end of the string, so here
      // it's ok to have a pos at the end
      if ((i < pos) && (spos >= len)) return ERR_CAN_NOT_RESTORE_SYNTAX;
    }

  /* check for integrity last read data may contain only preparation for next, so don't check that one */
  for (unsigned int i = 0; i < pos; i++) {
    // for the last piece we can check for the number of nodes
    if (rows[i] > left.size())
      return ERR_CAN_NOT_RESTORE_SYNTAX;
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

xml::node assembler_0_c::save(void) const {

  xml::node nd("assembler");

  nd.get_attributes().insert("version", ASSEMBLER_VERSION);

  std::string cont;

  char tmp[100];

  snprintf(tmp, 100, "%i ", pos);
  cont += tmp;

  snprintf(tmp, 100, "%li ", iterations);
  cont += tmp;

  if (pos <= piecenumber)
    for (unsigned int j = 0; j <= pos; j++) {

      snprintf(tmp, 100, "(%i %i)", rows[j], columns[j]);
      cont += tmp;

      if (j < pos)
        cont += " ";
    }

  nd.set_content(cont.c_str());

  return nd;
}

unsigned int assembler_0_c::getPiecePlacement(unsigned int node, int delta, unsigned int piece, unsigned char *tran, int *x, int *y, int *z) {

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

unsigned int assembler_0_c::getPiecePlacementCount(unsigned int piece) {

  return colCount[piece+1];
}


void assembler_0_c::debug_step(unsigned long num) {
  debug = true;
  debug_loops = num;
  asm_bc = 0;
  iterativeMultiSearch();
}

bool assembler_0_c::canHandle(const problem_c * p) {

  // we can not handle if there is one shape having not a counter of 1
  for (unsigned int s = 0; s < p->shapeNumber(); s++)
    if ((p->getShapeMax(s) > 1) ||
        (p->getShapeMax(s) != p->getShapeMin(s)))

      return false;

  return true;
}

