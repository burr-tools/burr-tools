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
#include "piecegenerator.h"

#include "puzzle.h"
#include "voxel.h"

typedef struct pieceTreeNode {

  // the position to check
  unsigned int pos;

  // pointer to the filled and empty tree node
  struct pieceTreeNode *filled, *empty;

  // we have either this one filled, or if it's empty then
  // we have a valid pos, filled and empty pointers;
  voxel_c *piece;

} pieceTreeNode;

static void deleteTree(pieceTreeNode *t) {

  if (t) {

    if (t->piece == 0) {

      deleteTree(t->filled);
      deleteTree(t->empty);
    }

    delete t;
  }
}

static bool containsPiece(pieceTreeNode *t, voxel_c *p) {

  if (!t) return false;

  if (t->piece) {

    return *p == *(t->piece);

  } else {

    if (p->getState(t->pos) == voxel_c::VX_EMPTY)
      return containsPiece(t->empty, p);
    else
      return containsPiece(t->filled, p);
  }
}

static pieceTreeNode * addPiece(pieceTreeNode *t, voxel_c *p) {

  if (!t) {
    t = new pieceTreeNode;

    t->piece = p;
  } else if (t->piece) {

    for (unsigned int i = 0; i < t->piece->getXYZ(); i++) {

      voxel_type a = t->piece->get(i);
      voxel_type b = p->get(i);
      if (a != b) {

        pieceTreeNode * e = new pieceTreeNode;
        pieceTreeNode * f = new pieceTreeNode;

        if (p->getState(i) == voxel_c::VX_FILLED) {
          f->piece = p;
          e->piece = t->piece;
        } else {
          e->piece = p;
          f->piece = t->piece;
        }

        t->piece = 0;
        t->filled = f;
        t->empty = e;
        t->pos = i;

        break;
      }
    }

  } else {
      if (p->getState(t->pos) == voxel_c::VX_EMPTY)
        addPiece(t->empty, p);
      else
        addPiece(t->filled, p);
  }


  return t;
}

pieceGenerator_c::pieceGenerator_c (const voxel_c * p) {

  voxel_c * pt = new voxel_c(p);

  for (unsigned int z = 0; z < pt->getXYZ(); z++)
    if (pt->getState(z) == voxel_c::VX_VARIABLE)
      pt->setState(z, voxel_c::VX_EMPTY);

  pieces.push_back(pt);

//  char name[200];
//  puzzle_c puz;
//  puz.getResult()->resize(p->getX(), p->getY(), p->getZ(), VX_EMPTY);
//  for (int t = 0; t < pt->getXYZ(); t++)
//    puz.getResult()->set(t, pt->get(t));
//  sprintf(name, "grow/pc%06i.puzzle", pieces.size());
//  puz.save(name);

  int start = 0;

  int adder = 0;

  while (true) {

    printf("pieces added %i, pieces %i\n", adder, pieces.size());

    unsigned int nextStart = pieces.size();

    pieceTreeNode *t = 0;

    // go over all pieces with n added voxels
    for (unsigned int i = start; i < nextStart; i++) {
      if (((i-start) % 10000) == 0)
        printf("%i / %i\n", i-start, nextStart-start);

      // add in if there is an variable voxel, that is not yet filled
      // but has a filled voxel
      for (unsigned int v = 0; v < p->getXYZ(); v++) {
        if ((p->getState(v) == voxel_c::VX_VARIABLE) &&
            (pieces[i]->getState(v) == voxel_c::VX_EMPTY) //&&
            // FIXME (pieces[i]->neighbour(v, VX_FILLED))
           ) {

          // create a new piece where this voxel is filled
          voxel_c * ps = new voxel_c(pieces[i]);
          ps->setState(v, voxel_c::VX_FILLED);

          voxel_c sym(ps);

          sym.rotatez();
          sym.rotatez();
          sym.rotatey();

          if (containsPiece(t, ps) || containsPiece(t, &sym)) {
            delete ps;
          } else {
            pieces.push_back(ps);
            t = addPiece(t, ps);
//            sprintf(name, "grow/pc%06i.puzzle", pieces.size());
//            for (int t = 0; t < ps->getXYZ(); t++)
//              puz.getResult()->set(t, ps->get(t));
//            puz.save(name);
          }
        }
      }
    }
    deleteTree(t);

    // if we could not add one more piece, finished
    if (nextStart == pieces.size())
      break;

    start = nextStart;
    adder++;
  }
}
