/* Burr Solver
 * Copyright (C) 2003  Andreas Röver
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

#include "disassembler_0.h"

#include "disassembly.h"
#include "voxel.h"

#include <queue>
#include <set>
#include <functional>

/* a macro to calculate the minimum and maximum
 * of two values in one step
 */
#define minmax(a, b, min, max) { \
  if (a > b) {                   \
    min = b;                     \
    max = a;                     \
  } else {                       \
    min = a;                     \
    max = b;                     \
  }                              \
}


/* so, this isn't the function as described by Bill but rather a
 * bit optimized. For each pair of 2 different pieces and for
 * each of the three dimensions I do the following:
 *  - check the intersection area area in this direction
 *  - if it's empty the pieces do not interlock and the matrix
 *    is initialized to infinity (32000)
 *  - if we have an intersection we check each column inside this area
 *    and find the shortest distance the the pirst piece follows
 *    the second and the second piece follows the first
 */
void disassembler_0_c::prepare(int pn, voxel_type * pieces, node_c * searchnode) {

  for (int i = 0; i < pn; i++)
    for (int d = 0; d < 3; d++)
      matrix[d][i + piecenumber * i] = 0;

  for (int i = 0; i < pn-1; i++)
    for (int j = i + 1; j < pn; j++) {

      voxel_type pi = pieces[i];
      voxel_type pj = pieces[j];

      /* calc the potential intersection area */

      /* c.. contains the bounding box bounding boxth pieces
       * d.. contains the intersection box of the bounding boxes of both pieces
       */
      int cx1, cx2, dx1, dx2, cy1, cy2, dy1, dy2, cz1, cz2, dz1, dz2;

      minmax(bx1[pi] + searchnode->getX(i), bx1[pj] + searchnode->getX(j), cx1, dx1);
      minmax(bx2[pi] + searchnode->getX(i), bx2[pj] + searchnode->getX(j), dx2, cx2);

      minmax(by1[pi] + searchnode->getY(i), by1[pj] + searchnode->getY(j), cy1, dy1);
      minmax(by2[pi] + searchnode->getY(i), by2[pj] + searchnode->getY(j), dy2, cy2);

      minmax(bz1[pi] + searchnode->getZ(i), bz1[pj] + searchnode->getZ(j), cz1, dz1);
      minmax(bz2[pi] + searchnode->getZ(i), bz2[pj] + searchnode->getZ(j), dz2, cz2);

      int min1, min2;

      /* x-axis */
      min1 = min2 = 32000;


      for (int y = dy1; y <= dy2; y++)
        for (int z = dz1; z <= dz2; z++) {
          int val1, val2;
          val1 = val2 = 32000;

          for (int x = cx1; x <= cx2; x++) {

            if ((x >= bx1[pi] + searchnode->getX(i)) && (x <= bx2[pi] + searchnode->getX(i)) &&
                assm->get(x - searchnode->getX(i), y - searchnode->getY(i), z - searchnode->getZ(i)) == pi) {
              if (val1 < min1) min1 = val1;
              val2 = 0;
            } else if ((x >= bx1[pj] + searchnode->getX(j)) && (x <= bx2[pj] + searchnode->getX(j)) &&
                       assm->get(x - searchnode->getX(j), y - searchnode->getY(j), z - searchnode->getZ(j)) == pj) {
              if (val2 < min2) min2 = val2;
              val1 = 0;
            } else {
              val1++;
              val2++;
            }
          }
        }


      matrix[0][i + piecenumber * j] = min2;
      matrix[0][j + piecenumber * i] = min1;

      /* y-axis */
      min1 = min2 = 32000;

      for (int x = dx1; x <= dx2; x++)
        for (int z = dz1; z <= dz2; z++) {
          int val1, val2;
          val1 = val2 = 32000;

          for (int y = cy1; y <= cy2; y++) {

            if ((y >= by1[pi] + searchnode->getY(i)) && (y <= by2[pi] + searchnode->getY(i)) &&
                assm->get(x - searchnode->getX(i), y - searchnode->getY(i), z - searchnode->getZ(i)) == pi) {
              if (val1 < min1) min1 = val1;
              val2 = 0;
            } else if ((y >= by1[pj] + searchnode->getY(j)) && (y <= by2[pj] + searchnode->getY(j)) &&
                       assm->get(x - searchnode->getX(j), y - searchnode->getY(j), z - searchnode->getZ(j)) == pj) {
              if (val2 < min2) min2 = val2;
              val1 = 0;
            } else {
              val1++;
              val2++;
            }
          }
          if ((min1 == 0) && (min2 == 0)) {
            x = dx2+1;
            z = dz2+1;
          }
        }

      matrix[1][i + piecenumber * j] = min2;
      matrix[1][j + piecenumber * i] = min1;

      /* z-axis */
      min1 = min2 = 32000;

      for (int x = dx1; x <= dx2; x++)
        for (int y = dy1; y <= dy2; y++) {
          int val1, val2;
          val1 = val2 = 32000;

          for (int z = cz1; z <= cz2; z++) {

            if ((z >= bz1[pi] + searchnode->getZ(i)) && (z <= bz2[pi] + searchnode->getZ(i)) &&
                assm->get(x - searchnode->getX(i), y - searchnode->getY(i), z - searchnode->getZ(i)) == pi) {
              if (val1 < min1) min1 = val1;
              val2 = 0;
            } else if ((z >= bz1[pj] + searchnode->getZ(j)) && (z <= bz2[pj] + searchnode->getZ(j)) &&
                       assm->get(x - searchnode->getX(j), y - searchnode->getY(j), z - searchnode->getZ(j)) == pj) {
              if (val2 < min2) min2 = val2;
              val1 = 0;
            } else {
              val1++;
              val2++;
            }
          }
          if ((min1 == 0) && (min2 == 0)) {
            y = dy2+1;
            x = dx2+1;
          }
        }

      matrix[2][i + piecenumber * j] = min2;
      matrix[2][j + piecenumber * i] = min1;

    }


  /* second part of Bills algorithm. again a bit different from what he used to
   * describe it instead of repeatedly going over the array I recursively go down
   * and check the column and line of the modified field, if they do need adaption.
   * this prevents us from checking the whole array again if there was only one
   * cell changed the last time
   */
  for (int d = 0; d < 3; d++)
    for (int i = 0; i < pn; i++)
      for (int j = 0; j < pn; j++)
        for (int k = 0; k < pn; k++) {
          int l = matrix[d][i + piecenumber * j] + matrix[d][j + piecenumber * k];
          if (matrix[d][i + piecenumber * k] > l) {
            matrix[d][i + piecenumber * k] = l;
            prepare_rec(pn, d, i, k);
          }
        }
}

/* the recursive part: of the second part of Bills alg. */
void disassembler_0_c::prepare_rec(int pn, int d, int a, int b) {

  int l;

  for (int i = 0; i < pn; i++) {
    l = matrix[d][a + piecenumber * b] + matrix[d][b + piecenumber * i];
    if (matrix[d][a + piecenumber * i] > l) {
      matrix[d][a + piecenumber * i] = l;
      prepare_rec(pn, d, a, i);
    }
    l = matrix[d][i + piecenumber * a] + matrix[d][a + piecenumber * b];
    if (matrix[d][i + piecenumber * b] > l) {
      matrix[d][i + piecenumber * b] = l;
      prepare_rec(pn, d, i, b);
    }
  }
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
 * to distinguish "good" and "bad" moves the function returns true, if less than halve of
 * the pieces need to be moved. This prevents the movement of the whole puzzle which
 * is rubbish.
 */
bool disassembler_0_c::checkmovement(void) {

  for (int i = 0; i < next_pn; i++)
    check[i] = movement[i] != 0;

  bool finished;

  /* we count the number of pieces that need to be moved, if this number
   * get's bigger than halve of the pices of the current problem we
   * stop and return that this movement is rubbish
   */
  int moved_pieces = 1;
  int nd = nextdir >> 1;

  if (nextdir & 1) {

    do {
  
      finished = true;
  
      for (int i = 0; i < next_pn; i++)
        if (check[i]) {
          for (int j = 0; j < next_pn; j++)
            if ((i != j) && (movement[i] > matrix[nd][j + piecenumber * i]))
              if (movement[i] - matrix[nd][j + piecenumber * i] > movement[j]) {
                if (movement[j] == 0) {
                  moved_pieces++;
                  if (moved_pieces > (next_pn / 2))
                    return false;
                }
                movement[j] = movement[i] - matrix[nd][j + piecenumber * i];
                check[j] = true;
                finished = false;
              }
          check[i] = false;
        }
  
    } while (!finished);

  } else {

    do {
  
      finished = true;
  
      for (int i = 0; i < next_pn; i++)
        if (check[i]) {
          for (int j = 0; j < next_pn; j++)
            if ((i != j) && (movement[i] > matrix[nd][i + piecenumber * j]))
              if (movement[i] - matrix[nd][i + piecenumber * j] > movement[j]) {
                if (movement[j] == 0) {
                  moved_pieces++;
                  if (moved_pieces > (next_pn / 2))
                    return false;
                }
                movement[j] = movement[i] - matrix[nd][i + piecenumber * j];
                check[j] = true;
                finished = false;
              }
          check[i] = false;
        }
  
    } while (!finished);

  }

  return true;
}

void disassembler_0_c::init_find(node_c * nd, int piecenumber, voxel_type * pieces) {

  /* when a new search has been started we need to first calculate
   * the movement matrixes, this is a table that contains one 2 dimensional
   * matrix for each of the 6 directions where movement is possible
   *
   * the matrixes contains possible movement of one piece if other pieces
   * are not moved. So a one in column 2 row 4 means that piece nr. 2 can
   * be moved one unit it we fix piece nr. 4
   *
   * the algorithm used here is describes in Bill Cuttlers booklet
   * "Computer Analysis of All 6 Piece Burrs"
   */
  prepare(piecenumber, pieces, nd);

  /* first we try to remove the piece completely by specifying
   * a very large distance, if it is possible to move this piece that
   * far, it is probably removed
   */
  nextdir = 0;
  nextpiece = 0;
  nextstep = 1;
  next_pn = piecenumber;
}

/* at first we check if movement is possible at all in the current direction, if so
 * the next thing to do is to check if something can be removed, and finally we look for longer
 * movements in the actual direction
 *
 * FIXME: we should first try to remove a single piece, then to remoe groups of pieces
 * and then check movement of pieces
 */
node_c * disassembler_0_c::find(node_c * searchnode) {

  while (nextdir < 6) {

    /* go through all directions */
    while (nextpiece < next_pn) {

      /* initialize the movement matrix. we want to move 'nextpiece' 'nextstep' units
       * into the current direction, so we initialize the matrix with all
       * zero except for our piece
       */
      for (int i = 0; i < next_pn; i++) movement[i] = 0;
      movement[nextpiece] = nextstep;

      /* checkmovement increases the values for all the other pieces so much that we can move
       * our selected piece as far as we want, if this results in more than halve of the
       * pieces beeing moved we don't do this because this would be stupid
       */
      if (checkmovement()) {

        node_c * n = new node_c(next_pn);

        /* create a new state with the pieces moved */
        for (int i = 0; i < next_pn; i++)
          switch(nextdir) {
          case 0: n->set(i, searchnode->getX(i) + movement[i], searchnode->getY(i), searchnode->getZ(i)); break;
          case 1: n->set(i, searchnode->getX(i) - movement[i], searchnode->getY(i), searchnode->getZ(i)); break;
          case 2: n->set(i, searchnode->getX(i), searchnode->getY(i) + movement[i], searchnode->getZ(i)); break;
          case 3: n->set(i, searchnode->getX(i), searchnode->getY(i) - movement[i], searchnode->getZ(i)); break;
          case 4: n->set(i, searchnode->getX(i), searchnode->getY(i), searchnode->getZ(i) + movement[i]); break;
          case 5: n->set(i, searchnode->getX(i), searchnode->getY(i), searchnode->getZ(i) - movement[i]); break;
          }

        if (nextstep == 1)
          nextstep = 30000;
        else
          nextstep ++;

        return n;
      }

      /* if we can not remove the piece we try to move it */
      if (nextstep == 30000)
        nextstep = 2;
      else {
        nextstep = 1;
        nextpiece++;
      }
    }

    nextpiece = 0;
    nextdir++;
  }

  return 0;
}

/* create all the necessary parameters for one of the two possible subproblems
 * our current problems divides into
 */
static void create_new_params(node_c * st, node_c ** n, voxel_type ** pn, int piecenumber, voxel_type * pieces, int part, bool cond) {

  *n = new node_c(part);
  *pn = new voxel_type[part];

  int num = 0;
  int dx, dy, dz;

  dx = dy = dz = 0;

  for (int i = 0; i < piecenumber; i++)
    if (st->is_piece_removed(i) == cond) {
      if (num == 0) {
        /* find the direction, the first piece was moved out of the puzzle
         * and shift it back along this avis */
        if ((st->getX(i) > 10000) || (st->getX(i) < -10000)) dx = st->getX(i);
        if ((st->getY(i) > 10000) || (st->getY(i) < -10000)) dy = st->getY(i);
        if ((st->getZ(i) > 10000) || (st->getZ(i) < -10000)) dz = st->getZ(i);
      }
      (*n)->set(num,
                st->getX(i) - dx,
                st->getY(i) - dy,
                st->getZ(i) - dz);
      (*pn)[num] = pieces[i];
      num++;
    }

  assert(num == part);
}

/* this is a bredth first search function that analyzes the movement of
 * an assembled problem. when the problem falls apart into 2 pieces the function
 * calls itself recursively. It returns null if the problem can not be taken apart
 * completely and otherwise the disassembly tree
 *
 * the parameters are:
 *  - piecenumber: the number of pieces of the current problem. because we can have
 *                 subproblems this number is not identical to the number of pieces
 *                 in the assembly voxel space
 *  - pieces: defines which pieces of the assemly voxel space are acually really present
 *            in the current subproblem
 *  - start: the start position of each piece
 *
 * the function takes over the ownership of the node and pieces. They are deleted at the end
 * of the function, so you must allocate them with new
 */
separation_c * disassembler_0_c::disassemble_rec(int piecenumber, voxel_type * pieces, node_c * start) {

  std::queue<node_c *> openlist;
  std::set<node_c *, node_ptr_less> closed;

  closed.insert(start);
  openlist.push(start);

  separation_c * erg = 0;

  /* while there are nodes left we should look at and we have not found a solution */
  while (!openlist.empty()) {

    /* remove the node from the open list and start examining */
    node_c * node = openlist.front();
    openlist.pop();

    init_find(node, piecenumber, pieces);

    node_c * st;

    while ((st = find(node))) {

      if (closed.find(st) != closed.end()) {

        /* the new node is already here. We have found a new longer way to that
         * node, so we can savely delete the new node and continue to the next
         */
        delete st;
        continue;
      }

      closed.insert(st);
      st->setComefrom(node);

      if (!st->is_separation()) {

        /* the new node is no solution so insert the
         * new state into the known state table
         * and the open list for later examination and go on to the next node
         */
        openlist.push(st);
        continue;
      }

      /* if we get here we have found a node that separated the puzzle into
       * 2 pieces. So we recoursively solve the subpuzzles and create a tree
       * with them that needs to be returned
       */

      /* count the pieces in both parts */
      int part1 = 0, part2 = 0;

      for (int i = 0; i < piecenumber; i++)
        if (st->is_piece_removed(i))
          part2++;
        else
          part1++;

      /* each subpart must contain at least 1 piece,
       * otherwise there is something wrong
       */
      assert((part1 > 0) && (part2 > 0));

      separation_c * left, *remove;

      /* check each subproblem, if it's a problem */
      if (part1 > 1) {

        node_c *n;
        voxel_type * pn;
        create_new_params(st, &n, &pn, piecenumber, pieces, part1, false);
        remove = disassemble_rec(part1, pn, n);

      } else
        remove = 0;

      if (part2 > 1) {

        node_c *n;
        voxel_type * pn;
        create_new_params(st, &n, &pn, piecenumber, pieces, part2, true);
        left = disassemble_rec(part2, pn, n);

      } else
        left = 0;

      /* if poth subproblems are either trivial or solvable, return the
       * result, otherwise return 0
       */
      if ((remove || (part1 == 1)) && (left || (part2 == 1))) {

        /* both subproblems are solvable -> construct tree */
        erg = new separation_c(left, remove, piecenumber, pieces);

        do {
          state_c *s = new state_c(piecenumber);

          for (int i = 0; i < piecenumber; i++)
            s->set(i, st->getX(i), st->getY(i), st->getZ(i));

          erg->addstate(s);

          st = st->getComefrom();
        } while (st);

      } else {

        /* one of the subproblems was unsolvable in this case the whole
         * puzzle is unsolvable, so we can as well stop here
         */
        if (left) delete left;
        if (remove) delete remove;
      }

      /* if we get here we can stop, even if we didn't find a solution
       * so we empty the openlist and se stop the currently running
       * search process
       */

      delete [] pieces;
    
      std::set<node_c *, node_ptr_less>::iterator i;
      for (i = closed.begin(); i != closed.end(); i++)
        delete *i;

      return erg;
    }
  }

  /* free all the allocated nodes */
  delete [] pieces;

  std::set<node_c *, node_ptr_less>::iterator i;
  for (i = closed.begin(); i != closed.end(); i++)
    delete *i;

  return 0;
}

void disassembler_0_c::calcbounds(void) {

  for (int i = 0; i < piecenumber+1; i++)
    bx1[i] = -1;

  for (int x = 0; x < assm->getX(); x++)
    for (int y = 0; y < assm->getY(); y++)
      for (int z = 0; z < assm->getZ(); z++) {
        voxel_type c = assm->get(x, y, z);
        if (c != VX_EMPTY) {

          if (bx1[c] == -1) {
            bx1[c] = bx2[c] = x;
            by1[c] = by2[c] = y;
            bz1[c] = bz2[c] = z;
          } else {
            if (x < bx1[c]) bx1[c] = x;
            if (x > bx2[c]) bx2[c] = x;

            if (y < by1[c]) by1[c] = y;
            if (y > by2[c]) by2[c] = y;

            if (z < bz1[c]) bz1[c] = z;
            if (z > bz2[c]) bz2[c] = z;
          }
        }
      }
}


disassembler_0_c::disassembler_0_c(voxel_c * problem, int piecenum) : assm(problem), piecenumber(piecenum) {

  /* allocate the necessary arrays */
  movement = new int[piecenumber];
  check = new bool[piecenumber];

  bx1 = new int[piecenumber+1];
  bx2 = new int[piecenumber+1];
  by1 = new int[piecenumber+1];
  by2 = new int[piecenumber+1];
  bz1 = new int[piecenumber+1];
  bz2 = new int[piecenumber+1];

  for (int i = 0; i < 3; i++)
    matrix[i] = new int[piecenumber * piecenumber];

  calcbounds();
}

disassembler_0_c::~disassembler_0_c() {
  delete [] movement;
  delete [] check;
  for (int i = 0; i < 3; i++)
    delete [] matrix[i];
  delete [] bx1;
  delete [] bx2;
  delete [] by1;
  delete [] by2;
  delete [] bz1;
  delete [] bz2;
}


disassembly_c * disassembler_0_c::disassemble(void) {

  /* create the first node with the start state
   * here all pieces are at position (0; 0; 0)
   */
  node_c * start = new node_c(piecenumber);

  for (int i = 0; i < piecenumber; i++)
    start->set(i, 0, 0, 0);

  /* create pieces field. this field contains the
   * names of all present pieces. because at the start
   * all pieces are still there we fill the array
   * with all the numbers
   */
  voxel_type * pieces = new voxel_type[piecenumber];

  for (int i = 0; i < piecenumber; i++)
    pieces[i] = i+1;

  separation_c * dis = disassemble_rec(piecenumber, pieces, start);

  if (dis)
    return new disassembly_c(assm, dis);
  else
    return 0;
}

