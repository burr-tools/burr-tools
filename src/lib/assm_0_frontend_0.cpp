/* Burr Solver
 * Copyright (C) 2003-2006  Andreas Röver
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
static bool pieceFits(const voxel_c * piece, const voxel_c * result, const puzzle_c * puz, int x, int y, int z, unsigned int problemNum) {

  for (unsigned int pz = piece->boundZ1(); pz <= piece->boundZ2(); pz++)
    for (unsigned int py = piece->boundY1(); py <= piece->boundY2(); py++)
      for (unsigned int px = piece->boundX1(); px <= piece->boundX2(); px++)
        if (
            // the piece can not be place if the result is empty and the piece is fileed at a given voxel
            ((piece->getState(px, py, pz) != voxel_c::VX_EMPTY) &&
             (result->getState(x+px, y+py, z+pz) == voxel_c::VX_EMPTY)) ||

            // the piece can also not be placed when the color constraints don't fit
            !puz->probPlacementAllowed(problemNum, piece->getColor(px, py, pz), result->getColor(x+px, y+py, z+pz))

           )
          return false;

  return true;
}

/* add a piece to the cache, but only if it is not already there. If it is added return the
 * piece pointer otherwise return null
 */
static voxel_c * addToCache(voxel_c * cache[NUM_TRANSFORMATIONS], unsigned int * fill, voxel_c * piece) {

  for (unsigned int i = 0; i < *fill; i++)
    if (cache[i]->identicalInBB(piece)) {
      delete piece;
      return 0;
    }

  cache[*fill] = piece;
  (*fill)++;
  return piece;
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
int assm_0_frontend_0_c::prepare(const puzzle_c * puz, int res_filled, int res_vari, unsigned int problemNum) {

  const voxel_c * result = puz->probGetResultShape(problemNum);

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
   * ok, what idea is behind this: we try to find as few double solutions as possible
   * because we don't want to fist search them and later on discard them because they are
   * double, so what do we do to prevent double solutions?
   *
   * Select one piece and remove rotations from this piece so that we don't even try to
   * place this piece in all possible positions. But which rotations need to be removed?
   * This depends on the symmetries that are present in the result and the symmetries
   * that are present in the piece
   */
  symmetries_t resultSym = result->selfSymmetries();
  unsigned int symBreakerShape = 0xFFFFFFFF;

  /* so, if we have just the self-symmetry in the result, everything needs to be tried
   * and not rotations can be removed
   */
  if (!unSymmetric(resultSym)) {

    /* now we try to find the most "suitable" piece for our rotation removal. What is
     * suitable? Suitable is the piece shape that has the least common symmetries with
     * the result and that has the fiewest pieces
     */
    unsigned int bestFound = NUM_TRANSFORMATIONS_MIRROR + 1;
    unsigned int symBreakerPiece = 0;
    unsigned int pc = 0;

    for (unsigned int i = 0; i < puz->probShapeNumber(problemNum); i++) {

      unsigned int cnt = countSymmetryIntersection(resultSym, puz->probGetShapeShape(problemNum, i)->selfSymmetries());

      if ((cnt < bestFound) ||
          (cnt == bestFound) &&
          (puz->probGetShapeCount(problemNum, i) < puz->probGetShapeCount(problemNum, symBreakerShape))) {
        bestFound = cnt;
        symBreakerShape = i;
        symBreakerPiece = pc;
      }

      pc += puz->probGetShapeCount(problemNum, i);
    }

    bool tmp = symmetriesLeft(resultSym, puz->probGetShapeShape(problemNum, symBreakerShape)->selfSymmetries());

    if (tmp || (puz->probGetShapeCount(problemNum, symBreakerShape) > 1))
      checkForTransformedAssemblies(symBreakerPiece);
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
      voxel_c * cache[NUM_TRANSFORMATIONS_MIRROR];
      unsigned int cachefill = 0;
      unsigned int placements = 0;

      /* go through all possible rotations of the piece
       * if shape is new to cache, add it to the cache and also
       * add the shape to the matrix, in all positions that it fits
       */
      for (unsigned int rot = 0; rot < NUM_TRANSFORMATIONS; rot++)
        if (voxel_c * rotation = addToCache(cache, &cachefill, new voxel_c(puz->probGetShapeShape(problemNum, pc), rot))) {
          for (int x = (int)result->boundX1()-(int)rotation->boundX1(); x <= (int)result->boundX2()-(int)rotation->boundX2(); x++)
            for (int y = (int)result->boundY1()-(int)rotation->boundY1(); y <= (int)result->boundY2()-(int)rotation->boundY2(); y++)
              for (int z = (int)result->boundZ1()-(int)rotation->boundZ1(); z <= (int)result->boundZ2()-(int)rotation->boundZ2(); z++)
                if (pieceFits(rotation, result, puz, x, y, z, problemNum)) {

                  int piecenode = AddPieceNode(piece, rot, x+rotation->getHx(), y+rotation->getHy(), z+rotation->getHz());
                  placements = 1;

                  /* now add the used cubes of the piece */
                  for (unsigned int pz = rotation->boundZ1(); pz <= rotation->boundZ2(); pz++)
                    for (unsigned int py = rotation->boundY1(); py <= rotation->boundY2(); py++)
                      for (unsigned int px = rotation->boundX1(); px <= rotation->boundX2(); px++)
                        if (rotation->getState(px, py, pz) != voxel_c::VX_EMPTY)
                          AddVoxelNode(columns[result->getIndex(x+px, y+py, z+pz)], piecenode);
                }


        /* for the symmetry breaker piece we also add all symmetries of the box */
        if ((pc == symBreakerShape) && (piececount == 0))
          for (unsigned int r = 1; r < NUM_TRANSFORMATIONS_MIRROR; r++)
            if (symmetrieContainsTransformation(resultSym, r))
              addToCache(cache, &cachefill, new voxel_c(puz->probGetShapeShape(problemNum, pc), transAdd(rot, r)));
        }

      for (unsigned int i = 0; i < cachefill; i++)  delete cache[i];

      /* check, if the current piece has at least one placement */
      if (placements == 0) {
        delete [] columns;
        return -pc;
      }
    }

  delete [] columns;

  return 1;
}

assm_0_frontend_0_c::~assm_0_frontend_0_c() {
  if (voxelindex)
    delete [] voxelindex;
}

