/* Burr Solver
 * Copyright (C) 2003-2005  Andreas Röver
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


#include "assm_0_frontend_0.h"

/* helper function to check if a piece an go at a position */
static bool pieceFits(const pieceVoxel_c * piece, const pieceVoxel_c * result, const puzzle_c * puz, int x, int y, int z, unsigned int problemNum) {

  for (unsigned int pz = 0; pz < piece->getZ(); pz++)
    for (unsigned int py = 0; py < piece->getY(); py++)
      for (unsigned int px = 0; px < piece->getX(); px++)
        if (
            // the piece can not be place if the result is empty and the piece is fileed at a given voxel
            ((piece->getState(px, py, pz) != pieceVoxel_c::VX_EMPTY) &&
             (result->getState(x+px, y+py, z+pz) == pieceVoxel_c::VX_EMPTY)) ||

            // the piece can also not be placed when the color constraints don't fit
            !puz->probPlacementAllowed(problemNum, piece->getColor(px, py, pz), result->getColor(x+px, y+py, z+pz))

           )
          return false;

  return true;
}

/* add a piece to the cache, but only if it is not already there. If it is added return the
 * piece pointer otherwise return null
 */
static pieceVoxel_c * addToCache(pieceVoxel_c * cache[24], unsigned int * fill, pieceVoxel_c * piece) {

  for (unsigned int i = 0; i < *fill; i++)
    if (*cache[i] == *piece) {
      delete piece;
      return 0;
    }

  cache[*fill] = piece;
  (*fill)++;
  return piece;
}

/**
 * this function counts the number of nodes required to acommodate all pieces
 * the title line is missing from the returned number
 */
unsigned long assm_0_frontend_0_c::countNodes(puzzle_c * puz, unsigned int problemNum) {

  unsigned long nodes = 0;

  const pieceVoxel_c * result = puz->probGetResultShape(problemNum);

  /* now we insert one shape after another */
  for (unsigned int pc = 0; pc < puz->probShapeNumber(problemNum); pc++) {

    int placements = 0;

    /* this array contains all the pieces found so far, this will help us
     * to not add two times the same piece to the structur */
    pieceVoxel_c * cache[24];
    unsigned int cachefill = 0;

    /* go through all possible rotations of the piece */
    /* didn't find piece, so it's new shape, add to cache and add to
     * node structure */
    /* find all possible translations of piece and add them, if they fit */
    for (unsigned int rot = 0; rot < 24; rot++)
      if (pieceVoxel_c * rotation = addToCache(cache, &cachefill, new pieceVoxel_c(puz->probGetShapeShape(problemNum, pc), rot)))
        for (unsigned int x = 0; x < result->getX() - rotation->getX() + 1; x++)
          for (unsigned int y = 0; y < result->getY() - rotation->getY() + 1; y++)
            for (unsigned int z = 0; z < result->getZ() - rotation->getZ() + 1; z++)
              if (pieceFits(rotation, result, puz, x, y, z, problemNum))
                placements++;

    nodes += placements * (puz->probGetShapeShape(problemNum, pc)->countState(pieceVoxel_c::VX_FILLED) + 1) *
      puz->probGetShapeCount(problemNum, pc);

    for (unsigned int i = 0; i < cachefill; i++) delete cache[i];

    // if one piece has no possible position in the result shape return error
    if (placements == 0)
      return -pc;
  }

  return nodes;
}

/**
 * this function prepares the matrix of nodes for the recursive function
 * I've done some additions to knuths algorithm to implement variable
 * voxels (empty spaces in the solution) and multiple instances of the
 * same piece. Empty voxels in the result are done by removing columns
 * from the matrix. This will prevent the algorithm from filling the
 * corresponding voxels. But we need to have the constraints that these
 * columns place on the solution. This is done by adding these columns
 * to the matrix but behind the normal columns. These additional columns
 * wont be searched by the alg if it looks for the next task to achive.
 *
 * Multiple instances of the same piece is handles in a similar way. To
 * prevent finding the same solution again and again with just the
 * pieces swapping places we enumber the pieces and their possible
 * placements and disallow that the position number of piece n is lower
 * than the position number of piece n-1. This can be achived by adding
 * more constraint columns. There need to be one column for each
 *
 * negative result show there is something wrong: the place -result has not
 * possible position inside the result
 */
void assm_0_frontend_0_c::prepare(puzzle_c * puz, int res_filled, int res_vari, unsigned int problemNum) {

  pieceVoxel_c * result = puz->probGetResultShape(problemNum);

  /* this array contains the column in our matrix that corresponds with
   * the voxel position inside the result. We use this matrix because
   * the calculation of the exact column depends on the number of FILLED
   * and VARIABLE voxels and is relatively expensive to calculate
   * with this lookup I was able to reduce the preparation time
   * from 5 to 0.5 seconds for TheLostDay puzzle
   */
  unsigned int * columns = new unsigned int[result->getXYZ()];
  unsigned int piecenumber = puz->probPieceNumber(problemNum);
  voxelindex = new int[result->getXYZ() + piecenumber + 1];

  for (unsigned int i = 0; i < result->getXYZ() + piecenumber + 1; i++)
    voxelindex[i] = -1;

  {
    int v = 0;
    int c = 0;
  
    for (unsigned int i = 0; i < result->getXYZ(); i++) {
      switch(result->getState(i)) {
      case pieceVoxel_c::VX_VARIABLE:
        voxelindex[getVarivoxelStart() + v] = i;
        columns[i] = getVarivoxelStart() + v++;
        break;
      case pieceVoxel_c::VX_FILLED:
        voxelindex[1 + piecenumber + c] = i;
        columns[i] = 1 + piecenumber + c++;
        break;
      default:
        columns[i] = 0;
      }
    }
  }

  /* find the symmetry breaker */
  symmetries_t resultSym = result->selfSymmetries();
  unsigned int symBreakerPiece = 0xFFFFFFFF;

  // we only need to do this, if the result shape has some symmetries */
  if (resultSym != 1) {

    unsigned int bestFound = 25;

    for (unsigned int i = 0; i < puz->probShapeNumber(problemNum); i++) {

      symmetries_t multSym = resultSym & puz->probGetShapeShape(problemNum, i)->selfSymmetries();

      if ((numSymmetries(multSym) < bestFound) ||
          (numSymmetries(multSym) == bestFound) && (puz->probGetShapeCount(problemNum, i) < puz->probGetShapeCount(problemNum, symBreakerPiece))) {
        bestFound = numSymmetries(multSym);
        symBreakerPiece = i;
      }
    }

    symmetries_t tmp = resultSym & puz->probGetShapeShape(problemNum, symBreakerPiece)->selfSymmetries() & ~((symmetries_t)1);

    if (tmp || (puz->probGetShapeCount(problemNum, symBreakerPiece) > 1))
      printf("oops I wont be able to avoid all sorts of symmetries (%llx)\n", tmp);

    resultSym = multiplySymmetries(resultSym, puz->probGetShapeShape(problemNum, symBreakerPiece)->selfSymmetries());
  }

  /* node 0 is the start node for everything */

  /* even thou the matrix has a column for each result voxel and each piece we leave out
   * the VARIABLE voxels in the ring list of the header. This is to avoid selecting these
   * columns for filling. The columns for the VARIABLE voxels are only there to make sure
   * these voxels are only used once
   */

  /* nodes 1..n are the columns nodes */
  GenerateFirstRow(res_filled);

  int piece = 0;

  /* now we insert one shape after another */
  for (unsigned int pc = 0; pc < puz->probShapeNumber(problemNum); pc++)
    for (unsigned int piececount = 0; piececount < puz->probGetShapeCount(problemNum, pc); piececount++, piece++) {

      nextPiece(piece, puz->probGetShapeCount(problemNum, pc), piececount);

      /* this array contains all the pieces found so far, this will help us
       * to not add two times the same piece to the structur */
      pieceVoxel_c * cache[24];
      unsigned int cachefill = 0;
  
      /* go through all possible rotations of the piece
       * if shape is new to cache, add it to the cache and also
       * add the shape to the matrix, in all positions that it fits
       */
      for (unsigned int rot = 0; rot < 24; rot++) {
        bool skipRotation = ((pc == symBreakerPiece) && (piececount == 0) && symmetrieContainsTransformation(resultSym, rot));
        if (pieceVoxel_c * rotation = addToCache(cache, &cachefill, new pieceVoxel_c(puz->probGetShapeShape(problemNum, pc), rot))) {
          for (unsigned int x = 0; x < result->getX() - rotation->getX() + 1; x++)
            for (unsigned int y = 0; y < result->getY() - rotation->getY() + 1; y++)
              for (unsigned int z = 0; z < result->getZ() - rotation->getZ() + 1; z++)
                if (pieceFits(rotation, result, puz, x, y, z, problemNum)) {

                  int piecenode;

                  if (!skipRotation)
                    piecenode = AddPieceNode(piece, rot, x, y, z);
                  else
                    AddFillerNode();

                  /* now add the used cubes of the piece */
                  for (unsigned int pz = 0; pz < rotation->getZ(); pz++)
                    for (unsigned int py = 0; py < rotation->getY(); py++)
                      for (unsigned int px = 0; px < rotation->getX(); px++)
                        if (rotation->getState(px, py, pz) != pieceVoxel_c::VX_EMPTY) {
  
                          if (!skipRotation)
                            AddVoxelNode(columns[result->getIndex(x+px, y+py, z+pz)], piecenode);
                          else
                            AddFillerNode();
                        }
                }
        }
      }
      for (unsigned int i = 0; i < cachefill; i++)  delete cache[i];
    }

  delete [] columns;

  assm = new assemblyVoxel_c(result->getX(), result->getY(), result->getZ(), assemblyVoxel_c::VX_EMPTY);
}

assm_0_frontend_0_c::~assm_0_frontend_0_c() {
  if (voxelindex) delete [] voxelindex;
  if (assm) delete assm;
}

typedef unsigned int uint32_t;

bool assm_0_frontend_0_c::solution(void) {

  if (getCallback()) {
    /* clean voxel space */
    assm->setAll(assemblyVoxel_c::VX_EMPTY);

    /* put all the pieces at their places
     * be going through all the selected rows and finding out
     * where there is a one in that row and then find the corresponding
     * voxel space index and place the piece number in there
     */
    for (int i = 0; i < getPos(); i++) {
      unsigned int r = getRows(i);

      // go over all columns and that columns that
      // are for result shape will be set inside the result
      do {

        int vi = voxelindex[getColCount(r)];

        if (vi >= 0)
          assm->setPiece(vi, getPiece(i));

        r = getRight(r);

      } while (r != getRows(i));
    }


    // the new version, we do this double and check, if the results are identical, for the moment

    assembly_c * assembly = new assembly_c();

    /* first we need to find the order the piece are in */
    uint32_t * pieces = new uint32_t[getPiecenumber()];

    /* fill the array with 0xff, so that we can distinguish between
     * places and unplaces pieces
     */
    memset(pieces, 0xff, sizeof(unsigned int) * getPos());

    for (unsigned int i = 0; i < getPos(); i++) {
      assert(getPiece(i) < getPiecenumber());
      pieces[getPiece(i)] = i;
    }

    for (unsigned int i = 0; i < getPiecenumber(); i++)
      if (pieces[i] > getPos())
        assembly->addNonPlacement();
      else {
        unsigned char tran;
        int x, y, z;

        getPieceInformation(getRows(i), &tran, &x, &y, &z);
        assembly->addPlacement(tran, x, y, z);
      }

    delete [] pieces;

    return getCallback()->assembly(assembly, assm);
  }

  return true;
}

