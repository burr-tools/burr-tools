#include "pieceGenerator.h"

#include "puzzle.h"

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

    if (p->get(t->pos) == VX_EMPTY)
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

    for (int i = 0; i < t->piece->getXYZ(); i++) {

      voxel_type a = t->piece->get(i);
      voxel_type b = p->get(i);
      if (a != b) {

        pieceTreeNode * e = new pieceTreeNode;
        pieceTreeNode * f = new pieceTreeNode;

        if (p->get(i) == VX_FILLED) {
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
      if (p->get(t->pos) == VX_EMPTY)
        addPiece(t->empty, p);
      else
        addPiece(t->filled, p);
  }


  return t;
}

pieceGenerator_c::pieceGenerator_c (const voxel_c * p) {

  voxel_c * pt = new voxel_c(p);

  for (int z = 0; z < pt->getXYZ(); z++)
    if (pt->get(z) == VX_VARIABLE)
      pt->set(z, VX_EMPTY);

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

    int nextStart = pieces.size();

    pieceTreeNode *t = 0;

    // go over all pieces with n added voxels
    for (int i = start; i < nextStart; i++) {
      if (((i-start) % 10000) == 0)
        printf("%i / %i\n", i-start, nextStart-start);

      // add in if there is an variable voxel, that is not yet filled
      // but has a filled voxel
      for (int v = 0; v < p->getXYZ(); v++) {
        if ((p->get(v) == VX_VARIABLE) &&
            (pieces[i]->get(v) == VX_EMPTY) &&
            (pieces[i]->neighbour(v, VX_FILLED))) {

          // create a new piece where this voxel is filled
          voxel_c * ps = new voxel_c(pieces[i]);
          ps->set(v, VX_FILLED);

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


