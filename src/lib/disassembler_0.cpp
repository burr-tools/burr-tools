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
#include "disassembler_0.h"

#include "disassembly.h"
#include "bt_assert.h"
#include "movementcache.h"
#include "puzzle.h"
#include "grouping.h"
#include "assembly.h"

#include <queue>
#include <set>
#include <functional>

/* node is used to build the search tree for the breadth first search of the
 * state tree each node contains the position of all the pieces relative
 * to their position in the assembled puzzle
 */
class node0_c {

private:

  /* the nodes are used to save the shortest way from the start to each
   * node. So each node saves where the way to the start is
   */
  node0_c * comefrom;

  /* number of pieces this node is handling */
  int piecenumber;

  /* the position of the piece inside the cube
   * and the transformation of that piece
   */
  int *dx, *dy, *dz;
  unsigned int *trans;

public:

  node0_c(int pn, node0_c * comf) : comefrom(comf), piecenumber(pn) {
    dx = new int[piecenumber];
    dy = new int[piecenumber];
    dz = new int[piecenumber];
    trans = new unsigned int[piecenumber];
  }

  ~node0_c() {
    delete [] dx;
    delete [] dy;
    delete [] dz;
    delete [] trans;
  }

  /* the comparison operations use "normalized" positions,
   * meaning all the pieces are shifted so, that the position
   * of piece 0 is (0; 0; 0). This prevents us from shifting
   * the whole set around without movement of the pieces
   * relativ to each other because all nodes with all pieces
   * shifted by the same mount do are equal
   */
  bool operator == (const node0_c &b) const {

    for (int i = 1; i < piecenumber; i++) {
      if (dx[i] - dx[0] != b.dx[i] - b.dx[0]) return false;
      if (dy[i] - dy[0] != b.dy[i] - b.dy[0]) return false;
      if (dz[i] - dz[0] != b.dz[i] - b.dz[0]) return false;
      // FIXME: transformation is missing
    }

    return true;
  }

  /* this opeartion is required for the container. It brings
   * the nodes into an arbitraty but deterministic order
   */
  bool operator < (const node0_c &b) const {

    for (int i = 1; i < piecenumber; i++) {
      if (dx[i] - dx[0] < b.dx[i] - b.dx[0]) return true;
      if (dx[i] - dx[0] > b.dx[i] - b.dx[0]) return false;

      if (dy[i] - dy[0] < b.dy[i] - b.dy[0]) return true;
      if (dy[i] - dy[0] > b.dy[i] - b.dy[0]) return false;

      if (dz[i] - dz[0] < b.dz[i] - b.dz[0]) return true;
      if (dz[i] - dz[0] > b.dz[i] - b.dz[0]) return false;
      // FIXME: transformation is missing
    }

    return false;
  }

  int getX(int i) const {
    bt_assert(i < piecenumber);
    return dx[i];
  }
  int getY(int i) const {
    bt_assert(i < piecenumber);
    return dy[i];
  }
  int getZ(int i) const {
    bt_assert(i < piecenumber);
    return dz[i];
  }
  unsigned int getTrans(int i) const {
    bt_assert(i < piecenumber);
    return trans[i];
  }
  void set(int i, int x, int y, int z, unsigned int tr) {
    bt_assert(i < piecenumber);
    dx[i] = x;
    dy[i] = y;
    dz[i] = z;
    trans[i] = tr;
  }

  /* check if the given piece is at a position outside
   * of the rest of the puzzle
   */
  bool is_piece_removed(int nr) const {
    bt_assert(nr < piecenumber);
    return ((abs(dx[nr]) > 10000) || (abs(dy[nr]) > 10000) || (abs(dz[nr]) > 10000));
  }

  /* check if this node is for a state that separates
   * the puzzle into 2 pieces. this is the case if there
   * is one piece that is removed
   */
  bool is_separation() const {
    for (int i = 0; i < piecenumber; i++)
      if (is_piece_removed(i))
        return true;

    return false;
  }

  node0_c * getComefrom(void) const {
    return comefrom;
  }
};

/* because we save pointers to nodes inside our nodes set, we need a special
 * operation for the comparison, the standard one would compare the pointers
 * and not the things the pointers point to
 */
class node_ptr_less : public std::binary_function<node0_c *, node0_c *, bool> {

public:

  bool operator()(const node0_c * a, const node0_c * b) const {
    return *a < *b;
  }
};

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
void disassembler_0_c::prepare(int pn, voxel_type * pieces, node0_c * searchnode) {

  for (int j = 0; j < pn; j++)
    for (int i = 0; i < pn; i++) {
      if (i != j)
        cache->getValue(searchnode->getX(j) - searchnode->getX(i),
                        searchnode->getY(j) - searchnode->getY(i),
                        searchnode->getZ(j) - searchnode->getZ(i),
                        searchnode->getTrans(i), searchnode->getTrans(j),
                        pieces[i], pieces[j],
                        &matrix[0][i + piecenumber * j],
                        &matrix[1][i + piecenumber * j],
                        &matrix[2][i + piecenumber * j]);
      else
        matrix[0][i + piecenumber * j] = matrix[1][i + piecenumber * j] = matrix[2][i + piecenumber * j] = 0;
    }
}

void disassembler_0_c::prepare2(int pn) {

  /* second part of Bills algorithm. */
  bool again;

  for (int d = 0; d < 3; d++)
    do {

      again = false;

      int * pos2 = matrix[d];     // j + piecenumber * k
      int * pos3 = matrix[d];     // i + piecenumber * k

      for (int k = 0; k < pn; k++) {

        int * pos1 = matrix[d];   // i + piecenumber * j

        for (int j = 0; j < pn; j++) {

          for (int i = 0; i < pn; i++) {

            int l = *pos1 + *pos2;

            if (*pos3 > l) {
              *pos3 = l;
              again = true;
            }

            pos3++;
            pos1++;
          }

          pos1 += piecenumber - pn;
          pos2++;
          pos3 -= pn;
        }

        pos2 += piecenumber - pn;
        pos3 += piecenumber;
      }

    } while (again);
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
bool disassembler_0_c::checkmovement(unsigned int maxPieces, int nextdir, int next_pn, int nextpiece, int nextstep, int piece2) {

  /* we count the number of pieces that need to be moved, if this number
   * get's bigger than halve of the pices of the current problem we
   * stop and return that this movement is rubbish
   */
  unsigned int moved_pieces = 1;

  /* initialize the movement matrix. we want to move 'nextpiece' 'nextstep' units
   * into the current direction, so we initialize the matrix with all
   * zero except for our piece
   */
  for (int i = 0; i < next_pn; i++) {
    movement[i] = 0;
    check[i] = false;
  }
  movement[nextpiece] = nextstep;
  check[nextpiece] = true;

  if (piece2 >= 0) {
    movement[piece2] = nextstep;
    check[piece2] = true;
    moved_pieces++;
  }

  bool finished;
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
                  if (moved_pieces > maxPieces)
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
                  if (moved_pieces > maxPieces)
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

void disassembler_0_c::init_find(node0_c * nd, int piecenumber, voxel_type * pieces) {

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
  prepare2(piecenumber);

  /* first we try to remove the piece completely by specifying
   * a very large distance, if it is possible to move this piece that
   * far, it is probably removed
   */
  nextdir = 0;
  nextpiece = 0;
  nextstep = 1;
  nextstate = 0;
  next_pn = piecenumber;
}

static node0_c * newNode(int next_pn, int nextdir, node0_c * searchnode, int * movement) {

  // we only take this new node, when all pieces are either not moved at all
  // or moved by the same amount

  int amount = 0;

  for (int i = 0; i < next_pn; i++) {
    if (movement[i]) {
      if (amount == 0)
        amount = movement[i];

      if (amount != movement[i])
        return 0;
    }
  }

  node0_c * n = new node0_c(next_pn, searchnode);

  /* create a new state with the pieces moved */
  for (int i = 0; i < next_pn; i++)
    switch(nextdir) {
      case 0: n->set(i, searchnode->getX(i) + movement[i], searchnode->getY(i), searchnode->getZ(i), searchnode->getTrans(i)); break;
      case 1: n->set(i, searchnode->getX(i) - movement[i], searchnode->getY(i), searchnode->getZ(i), searchnode->getTrans(i)); break;
      case 2: n->set(i, searchnode->getX(i), searchnode->getY(i) + movement[i], searchnode->getZ(i), searchnode->getTrans(i)); break;
      case 3: n->set(i, searchnode->getX(i), searchnode->getY(i) - movement[i], searchnode->getZ(i), searchnode->getTrans(i)); break;
      case 4: n->set(i, searchnode->getX(i), searchnode->getY(i), searchnode->getZ(i) + movement[i], searchnode->getTrans(i)); break;
      case 5: n->set(i, searchnode->getX(i), searchnode->getY(i), searchnode->getZ(i) - movement[i], searchnode->getTrans(i)); break;
    }

  return n;
}

/* at first we check if movement is possible at all in the current direction, if so
 * the next thing to do is to check if something can be removed, and finally we look for longer
 * movements in the actual direction
 */
node0_c * disassembler_0_c::find(node0_c * searchnode) {

  node0_c * n = 0;

  // repeat until we either find a movement or have checked everything
  while (!n && (nextstate < 4)) {

    switch (nextstate) {
      case 0:
        // check, if a single piece can be removed
        if (checkmovement(1, nextdir, next_pn, nextpiece, 30000))
          n = newNode(next_pn, nextdir, searchnode, movement);

        nextpiece++;
        if (nextpiece >= next_pn) {
          nextpiece = 0;
          nextdir++;
          if (nextdir >= 6) {
            nextstate++;
            nextdir = 0;
          }
        }
        break;
      case 1:
        // check, if a group of pieces can be removed
        if (checkmovement(next_pn/2, nextdir, next_pn, nextpiece, 30000))
          n = newNode(next_pn, nextdir, searchnode, movement);

        nextpiece++;
        if (nextpiece >= next_pn) {
          nextpiece = 0;
          nextdir++;
          if (nextdir >= 6) {
            nextstate++;
            nextdir = 0;
          }
        }
        break;
      case 2:
        // check, if a single piece can be moved
        if (checkmovement(next_pn/2, nextdir, next_pn, nextpiece, nextstep)) {
          n = newNode(next_pn, nextdir, searchnode, movement);

          // if we can move something, we try larger steps
          nextstep++;

        } else {

          // if not, lets try the next piece
          nextstep = 1;
          nextpiece++;
          if (nextpiece >= next_pn) {
            nextpiece = 0;
            nextdir++;
            if (nextdir >= 6) {

              // next state is the 2 piece at once moving state,
              // it is only useful to try this, when we have at least 4 pieces
              // otherwise it doesn't make any sense and we skip that state
              if (next_pn >= 4) {
                nextstate++;
                nextdir = 0;

                nextpiece2 = 1;

              } else {
                nextstate += 2;
              }
            }
          }
        }
        break;
      case 3:
        // check, 2 pieces can be moved into the same direction
        if (checkmovement(next_pn/2, nextdir, next_pn, nextpiece, nextstep, nextpiece2)) {
          n = newNode(next_pn, nextdir, searchnode, movement);

          // if we can move something, we try larger steps
          nextstep++;

        } else {

          // if not, lets try the next piece
          nextstep = 1;
          nextpiece2++;
          if (nextpiece2 >= next_pn) {
            nextpiece++;
            nextpiece2 = nextpiece + 1;
            if (nextpiece2 >= next_pn) {
              nextpiece = 0;
              nextpiece2 = 1;
              nextdir++;
              if (nextdir >= 6) {
                nextstate++;
                nextdir = 0;
              }
            }
          }
        }
        break;
      default:
        // endstate, do nothing
        break;
    }
  }

  return n;
}

/* create all the necessary parameters for one of the two possible subproblems
 * our current problems divides into
 */
static void create_new_params(node0_c * st, node0_c ** n, voxel_type ** pn, int piecenumber, voxel_type * pieces, int part, bool cond) {

  *n = new node0_c(part, 0);
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
                st->getZ(i) - dz,
                st->getTrans(i));
      (*pn)[num] = pieces[i];
      num++;
    }

  bt_assert(num == part);
}

unsigned short disassembler_0_c::subProbGroup(node0_c * st, voxel_type * pn, bool cond, int piecenumber) {

  unsigned short group = 0;

  for (int i = 0; i < piecenumber; i++)
    if (st->is_piece_removed(i) == cond)
      if (puzzle->probGetShapeGroupNumber(problem, piece2shape[pn[i]]) != 1)
        return 0;
      else if (group == 0)
        group = puzzle->probGetShapeGroup(problem, piece2shape[pn[i]], 0);
      else if (group != puzzle->probGetShapeGroup(problem, piece2shape[pn[i]], 0))
        return 0;

  return group;
}

bool disassembler_0_c::subProbGrouping(voxel_type * pn, int piecenumber) {

  groups->newSet();

  for (int i = 0; i < piecenumber; i++)
    if (!groups->addPieceToSet(piece2shape[pn[i]]))
      return false;

  return true;
}

separation_c * disassembler_0_c::checkSubproblem(int pieceCount, voxel_type * pieces, int piecenumber, node0_c * st, bool left, bool * ok) {

  separation_c * res = 0;

  if (pieceCount == 1) {
    *ok = true;
  } else if (subProbGroup(st, pieces, left, piecenumber)) {
    *ok = true;
  } else {

    node0_c *n;
    voxel_type * pn;
    create_new_params(st, &n, &pn, piecenumber, pieces, pieceCount, left);
    res = disassemble_rec(pieceCount, pn, n);

    *ok = res || subProbGrouping(pn, pieceCount);

    delete [] pn;
  }

  return res;
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
separation_c * disassembler_0_c::disassemble_rec(int piecenumber, voxel_type * pieces, node0_c * start) {

  std::queue<node0_c *> openlist;
  std::set<node0_c *, node_ptr_less> closed;

  closed.insert(start);
  openlist.push(start);

  separation_c * erg = 0;

  /* while there are nodes left we should look at and we have not found a solution */
  while (!openlist.empty()) {

    /* remove the node from the open list and start examining */
    node0_c * node = openlist.front();
    openlist.pop();

    init_find(node, piecenumber, pieces);

    node0_c * st;

    while ((st = find(node))) {

      if (closed.find(st) != closed.end()) {

        /* the new node is already here. We have found a new longer way to that
         * node, so we can savely delete the new node and continue to the next
         */
        delete st;
        continue;
      }

      closed.insert(st);

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
      bt_assert((part1 > 0) && (part2 > 0));

      separation_c * left, *remove;
      bool left_ok = false;
      bool remove_ok = false;
      left = remove = 0;

      /* all right, the following thing come twice, maybe I should
       * put it into a function, anyways:
       * if the subproblem to check has only one piece, it's solved
       * if all the pieces belong to the same group, we can stop
       * else try to disassemble, if that fails, try to
       * group the involved pieces into an identical group
       */
      remove = checkSubproblem(part1, pieces, piecenumber, st, false, &remove_ok);

      /* only check the left over part, when the removed part is ok */
      if (remove_ok)
        left = checkSubproblem(part2, pieces, piecenumber, st, true, &left_ok);

      /* if both subproblems are either trivial or solvable, return the
       * result, otherwise return 0
       */
      if (remove_ok && left_ok) {

        /* both subproblems are solvable -> construct tree */
        erg = new separation_c(left, remove, piecenumber, pieces);

        do {
          state_c *s = new state_c(piecenumber);

          for (int i = 0; i < piecenumber; i++)
            s->set(i, st->getX(i), st->getY(i), st->getZ(i));

          erg->addstate(s);

          st = (node0_c*)st->getComefrom();
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

      std::set<node0_c *, node_ptr_less>::iterator it;
      for (it = closed.begin(); it != closed.end(); it++)
        delete *it;

      return erg;
    }

    /* we have checked all the successors of this node, so we don't need the matrix
     * any longer
     */
  }

  // free all the allocated nodes
  std::set<node0_c *, node_ptr_less>::iterator i;
  for (i = closed.begin(); i != closed.end(); i++)
    delete *i;

  return 0;
}

disassembler_0_c::disassembler_0_c(const puzzle_c * puz, unsigned int prob) :
  piecenumber(puz->probPieceNumber(prob)), puzzle(puz), problem(prob) {

  /* allocate the necessary arrays */
  movement = new int[piecenumber];
  check = new bool[piecenumber];

  for (int j = 0; j < 3; j++)
    matrix[j] = new int[piecenumber * piecenumber];

  cache = new movementCache(puzzle, problem);

  /* initialize the grouping class */
  groups = new grouping_c();
  for (unsigned int i = 0; i < puz->probShapeNumber(prob); i++)
    for (unsigned int j = 0; j < puz->probGetShapeGroupNumber(prob, i); j++)
      groups->addPieces(puz->probGetShape(prob, i),
                        puz->probGetShapeGroup(prob, i, j),
                        puz->probGetShapeGroupCount(prob, i, j));

  /* initialize piece 2 shape transformation */
  piece2shape = new unsigned short[piecenumber];
  int p = 0;
  for (unsigned int i = 0; i < puz->probShapeNumber(prob); i++)
    for (unsigned int j = 0; j < puz->probGetShapeCount(prob, i); j++)
      piece2shape[p++] = i;

}

disassembler_0_c::~disassembler_0_c() {
  delete [] movement;
  delete [] check;
  for (unsigned int k = 0; k < 3; k++)
    delete [] matrix[k];

  delete cache;
}

separation_c * disassembler_0_c::disassemble(const assembly_c * assembly) {

  bt_assert(piecenumber == assembly->placementCount());

  /* create the first node with the start state
   * here all pieces are at position (0; 0; 0)
   */
  node0_c * start = new node0_c(piecenumber, 0);

  for (unsigned int i = 0; i < piecenumber; i++)
    start->set(i, assembly->getX(i), assembly->getY(i), assembly->getZ(i), assembly->getTransformation(i));

  /* create pieces field. this field contains the
   * names of all present pieces. because at the start
   * all pieces are still there we fill the array
   * with all the numbers
   */
  voxel_type * pieces = new voxel_type[piecenumber];

  /* reset the grouping class */
  groups->reSet();

  for (unsigned int j = 0; j < piecenumber; j++)
    pieces[j] = j;

  separation_c * s = disassemble_rec(piecenumber, pieces, start);

  delete [] pieces;

  return s;
}
