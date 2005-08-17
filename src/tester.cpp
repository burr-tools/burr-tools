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


#include "lib/puzzle.h"
#include "lib/assm_0_frontend_0.h"
#include "lib/disassembler_0.h"

#include "lib/burrgrower.h"
#include "lib/pieceGenerator.h"

#include "lib/print.h"

#include "lib/symmetries.h"

#include <time.h>

#include <fstream>

#include <xmlwrapp/xmlwrapp.h>

using namespace std;

class asm_cb : public assembler_cb {

public:

  int count;
  int pn;

  asm_cb(int pnum) : count(0), pn(pnum) {}

  bool assembly(assembly_c * a) {

#if 0
    count++;

    if ((count & 0x3f) == 0)
      printf("%i\n", count);

//    assm->print('a');

    return true;

    fflush(stdout);


    disassembler_3_c d(assm, pn);

    clock_t start = clock();
    separation_c * da = d.disassemble();
    printf(" timing = %f\n", ((double)(clock() - start))/CLOCKS_PER_SEC);

    if (da) {
//      assm->print();
      printf("level: %i\n", da->getMoves());
//      da->print();
      delete da;
    }

    return true;
#endif
  }
};

static voxel_c * transform(const voxel_c * p, int nr) {

  assert(nr < NUM_TRANSFORMATIONS);

  static int rotx[NUM_TRANSFORMATIONS] = { 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3 };
  static int roty[NUM_TRANSFORMATIONS] = { 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0 };
  static int rotz[NUM_TRANSFORMATIONS] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 3, 3, 3, 3 };

  voxel_c *erg = new voxel_c(*p);

  for (int i = 0; i < rotx[nr]; i++) erg->rotatex();
  for (int i = 0; i < roty[nr]; i++) erg->rotatey();
  for (int i = 0; i < rotz[nr]; i++) erg->rotatez();

  return erg;
}


unsigned int findSelfSymmetry(voxel_c * piece) {

  unsigned int result = 1;

  for (int i = 1; i < NUM_TRANSFORMATIONS; i++) {

    voxel_c * rot = transform(piece, i);

    if (*rot == *piece)
      result |= (1 << i);

    delete rot;
  }

  return result;
}

unsigned int foundSym[200] = {
  0x20081, 0x208001, 0x800021, 0x82001, 0x801, 0x41, 0x401, 0x40001, 0x101, 0x4001,
  0x400001, 0x201, 0x5, 0x844821, 0x482841, 0x424281, 0x248241, 0x110401, 0x1111,
  0x4141, 0xf, 0xa05, 0x440401, 0x505, 0xaaa5a5, 0x1,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

unsigned int syms = 26;


void search(pieceVoxel_c * piece) {

  unsigned int s = findSelfSymmetry(piece);
  bool found = false;

  for (int j = 0; j < syms; j++)
    if (s == foundSym[j]) {
      found = true;
      break;
    }

  if (!found) {
    printf("%x\n", s);
    foundSym[syms++] = s;

    char fn[200];

    snprintf(fn, 200, "symmetries/%06x.puzzle", s);

    ofstream o(fn);

    o << piece->save();
  }
}


void findsymmetries(void) {

  pieceVoxel_c v(4, 4, 4);

  unsigned int indizes[100];
  unsigned int indizeCount = 0;

  printf("preparation\n");
  for (int x = 0; x < 4; x++)
    for (int y = 0; y < 4; y++)
      for (int z = 0; z < 4; z++)
        if ((x == 0) || (y == 0) || (z == 0) || (x == 3) || (y == 3) || (z == 3))
          indizes[indizeCount++] = v.getIndex(x, y, z);

  printf("1 leerraum\n");

  for (int a = 0; a < indizeCount; a++) {

// FIXME    v.setAll(VX_FILLED);
    v.setState(a, pieceVoxel_c::VX_EMPTY);

    search(&v);
  }

  printf("2 leerraum\n");

  for (int a = 0; a < indizeCount; a++)
    for (int b = a+1; b < indizeCount; b++) {

//FIXME      v.setAll(VX_FILLED);
      v.setState(a, pieceVoxel_c::VX_EMPTY);
      v.setState(b, pieceVoxel_c::VX_EMPTY);
  
      search(&v);
    }

  printf("5 leerraum\n");

  for (int a = 0; a < indizeCount; a++)
    for (int b = a+1; b < indizeCount; b++)
      for (int c = b+1; c < indizeCount; c++) {
        printf("%i\n", c);
        for (int d = c+1; d < indizeCount; d++)
          for (int e = d+1; e < indizeCount; e++)
            for (int f = e+1; f < indizeCount; f++)

        {
// FIXME          v.setAll(VX_FILLED);
          v.setState(a, pieceVoxel_c::VX_EMPTY);
          v.setState(b, pieceVoxel_c::VX_EMPTY);
          v.setState(c, pieceVoxel_c::VX_EMPTY);
          v.setState(d, pieceVoxel_c::VX_EMPTY);
          v.setState(e, pieceVoxel_c::VX_EMPTY);
          v.setState(f, pieceVoxel_c::VX_EMPTY);

          search(&v);
        }
      }
}


void solve(int argv, char* args[]) {
  ifstream str(args[1]);

  if (!str) {
    cout << "oope file not opened\n";
    return;
  }

  puzzle_c p; /* FIXME &str*/

//  p.print();

  assembler_0_c * assm = new assm_0_frontend_0_c();
  assm->createMatrix(&p, 0);

  if (assm->createMatrix(&p, 0) != assm_0_frontend_0_c::ERR_NONE) {
    printf("errors\n");
    delete assm;
    return;
  }

  printf("start reduce\n");
  assm->reduce();
  printf("finished reduce\n");

  asm_cb a(p.probPieceNumber(0));

  a.count = 0;

  assm->assemble(&a);

  printf("%i sol found with %i iterations\n", a.count, assm->getIterations());

  delete assm;

  return;
}


void grow(int argv, char* args[]) {

  srand48(time(0));
  srand(time(0));

  ifstream str(args[1]);

  if (!str) {
    cout << "oope file not opened\n";
    return;
  }

  xml::tree_parser parser(args[1]);
  puzzle_c p(parser.get_document().get_root_node());

  cout << " The puzzle:\n\n";

  print(&p);

  burrGrower_c grow(&p, 10, atoi(args[2]));

  std::vector<puzzleSol_c*> v;

  printf("start\n");
  grow.grow(v);
  printf("finished\n");
}

void convert(int argv, char* args[]) {
  ifstream str(args[1]);

  if (!str) {
    cout << "oope file not opened\n";
    return;
  }

  puzzle_c p(/* FIXME &str*/);

  ofstream ostr(args[2]);

  if (!ostr) {
    cout << "oops outfile not opened\n";
    return;
  }
  
//  p.PS3Dsave(&ostr);
}

void savetoXML(int argv, char* args[]) {

  ifstream str(args[1]);

  if (!str) {
    cout << "oops file not opened\n";
    return;
  }

  puzzle_c p/* FIXME (&str)*/;

  xml::init init;

  xml::document xmldoc("burrTools");
  xml::node &root = xmldoc.get_root_node();

  xml::node::iterator it = root.insert(root.begin(), p.save());

  std::cout << xmldoc;
}

void multTranformationsMatrix(void) {

  for (int tr1 = 0; tr1 < NUM_TRANSFORMATIONS_MIRROR; tr1++) {
    printf("{");

    for (int tr2 = 0; tr2 < NUM_TRANSFORMATIONS_MIRROR; tr2++) {

      pieceVoxel_c v(3, 3, 3);
      v.setState(1, 1, 1, pieceVoxel_c::VX_FILLED); v.setState(0, 1, 1, pieceVoxel_c::VX_FILLED);
      v.setState(0, 0, 1, pieceVoxel_c::VX_FILLED); v.setState(0, 0, 0, pieceVoxel_c::VX_FILLED);

      int i;

      v.transform(tr1);
      v.transform(tr2);

      for (int t = 0; t < NUM_TRANSFORMATIONS_MIRROR; t++) {
        pieceVoxel_c w(3, 3, 3);
        w.setState(1, 1, 1, pieceVoxel_c::VX_FILLED); w.setState(0, 1, 1, pieceVoxel_c::VX_FILLED);
        w.setState(0, 0, 1, pieceVoxel_c::VX_FILLED); w.setState(0, 0, 0, pieceVoxel_c::VX_FILLED);

        w.transform(t);

        if (v == w) {
          printf("%2i, ", t);
          break;
        }
      }
    }
    printf("},\n");
  }
}


void inverseTranformationsMatrix(void) {

  pieceVoxel_c v(3, 3, 3);
  v.setState(1, 1, 1, pieceVoxel_c::VX_FILLED); v.setState(0, 1, 1, pieceVoxel_c::VX_FILLED);
  v.setState(0, 0, 1, pieceVoxel_c::VX_FILLED); v.setState(0, 0, 0, pieceVoxel_c::VX_FILLED);

  for (int tr = 0; tr < NUM_TRANSFORMATIONS_MIRROR; tr++) {

    pieceVoxel_c w(v, tr);

    for (int t = 0; t < NUM_TRANSFORMATIONS_MIRROR; t++) {

      pieceVoxel_c x(w, t);

      if (v == x) {
        printf("%2i, ", t);
        break;
      }
    }
  }
}

void comparison(voxel_c * a, voxel_c * b) {

  printf("sx: %i %i \n", a->getX(), b->getX());
  printf("sy: %i %i \n", a->getY(), b->getY());
  printf("sz: %i %i \n", a->getZ(), b->getZ());

  printf("bx: %i %i %i %i \n", a->boundX1(), a->boundX2(), b->boundX1(), b->boundX2());
  printf("by: %i %i %i %i \n", a->boundY1(), a->boundY2(), b->boundY1(), b->boundY2());
  printf("bz: %i %i %i %i \n", a->boundZ1(), a->boundZ2(), b->boundZ1(), b->boundZ2());

  print(a);
  print(b);
}

void testNewRots(void) {

  for (int i = 0; i < 1000; i++) {

    voxel_c *a = new voxel_c(16, 8, 4);

    for (int p = 0; p < 10; p++)
      a->set(rand() % 0x1FF, 1);

    voxel_c *b0 = new voxel_c(a);
    voxel_c *b1 = new voxel_c(a);
    voxel_c *b2 = new voxel_c(a);
    voxel_c *b3 = new voxel_c(a);
    voxel_c *b4 = new voxel_c(a);
    b0->rotatex(0);
    b1->rotatex(1);
    b2->rotatex(2);
    b3->rotatex(3);
    b4->rotatex(4);

    if (!(*a == *b0)
        || (a->boundX1() != b0->boundX1())
        || (a->boundX2() != b0->boundX2())
        || (a->boundY1() != b0->boundY1())
        || (a->boundY2() != b0->boundY2())
        || (a->boundZ1() != b0->boundZ1())
        || (a->boundZ2() != b0->boundZ2())
       ) comparison(a, b0);

    a->rotatex(1);

    if (!(*a == *b1)
        || (a->boundX1() != b1->boundX1())
        || (a->boundX2() != b1->boundX2())
        || (a->boundY1() != b1->boundY1())
        || (a->boundY2() != b1->boundY2())
        || (a->boundZ1() != b1->boundZ1())
        || (a->boundZ2() != b1->boundZ2())
       ) comparison(a, b1);

    a->rotatex(1);

    if (!(*a == *b2)
        || (a->boundX1() != b2->boundX1())
        || (a->boundX2() != b2->boundX2())
        || (a->boundY1() != b2->boundY1())
        || (a->boundY2() != b2->boundY2())
        || (a->boundZ1() != b2->boundZ1())
        || (a->boundZ2() != b2->boundZ2())
       ) comparison(a, b2);

    a->rotatex(1);

    if (!(*a == *b3)
        || (a->boundX1() != b3->boundX1())
        || (a->boundX2() != b3->boundX2())
        || (a->boundY1() != b3->boundY1())
        || (a->boundY2() != b3->boundY2())
        || (a->boundZ1() != b3->boundZ1())
        || (a->boundZ2() != b3->boundZ2())
       ) comparison(a, b3);

    a->rotatex(1);

    if (!(*a == *b4)
        || (a->boundX1() != b4->boundX1())
        || (a->boundX2() != b4->boundX2())
        || (a->boundY1() != b4->boundY1())
        || (a->boundY2() != b4->boundY2())
        || (a->boundZ1() != b4->boundZ1())
        || (a->boundZ2() != b4->boundZ2())
       ) comparison(a, b4);

  }

  for (int i = 0; i < 1000; i++) {

    voxel_c *a = new voxel_c(16, 8, 4);

    for (int p = 0; p < 10; p++)
      a->set(rand() % 0x1FF, 1);

    voxel_c *b0 = new voxel_c(a);
    voxel_c *b1 = new voxel_c(a);
    voxel_c *b2 = new voxel_c(a);
    voxel_c *b3 = new voxel_c(a);
    voxel_c *b4 = new voxel_c(a);
    b0->rotatey(0);
    b1->rotatey(1);
    b2->rotatey(2);
    b3->rotatey(3);
    b4->rotatey(4);

    printf("0\n");

    if (!(*a == *b0)
        || (a->boundX1() != b0->boundX1())
        || (a->boundX2() != b0->boundX2())
        || (a->boundY1() != b0->boundY1())
        || (a->boundY2() != b0->boundY2())
        || (a->boundZ1() != b0->boundZ1())
        || (a->boundZ2() != b0->boundZ2())
       ) comparison(a, b0);

    a->rotatey(1);

    printf("1\n");
    if (!(*a == *b1)
        || (a->boundX1() != b1->boundX1())
        || (a->boundX2() != b1->boundX2())
        || (a->boundY1() != b1->boundY1())
        || (a->boundY2() != b1->boundY2())
        || (a->boundZ1() != b1->boundZ1())
        || (a->boundZ2() != b1->boundZ2())
       ) comparison(a, b1);

    a->rotatey(1);

    printf("2\n");
    if (!(*a == *b2)
        || (a->boundX1() != b2->boundX1())
        || (a->boundX2() != b2->boundX2())
        || (a->boundY1() != b2->boundY1())
        || (a->boundY2() != b2->boundY2())
        || (a->boundZ1() != b2->boundZ1())
        || (a->boundZ2() != b2->boundZ2())
       ) comparison(a, b2);

    a->rotatey(1);

    printf("3\n");
    if (!(*a == *b3)
        || (a->boundX1() != b3->boundX1())
        || (a->boundX2() != b3->boundX2())
        || (a->boundY1() != b3->boundY1())
        || (a->boundY2() != b3->boundY2())
        || (a->boundZ1() != b3->boundZ1())
        || (a->boundZ2() != b3->boundZ2())
       ) comparison(a, b3);

    a->rotatey(1);

    printf("4\n");
    if (!(*a == *b4)
        || (a->boundX1() != b4->boundX1())
        || (a->boundX2() != b4->boundX2())
        || (a->boundY1() != b4->boundY1())
        || (a->boundY2() != b4->boundY2())
        || (a->boundZ1() != b4->boundZ1())
        || (a->boundZ2() != b4->boundZ2())
       ) comparison(a, b4);

  }


  for (int i = 0; i < 1000; i++) {

    voxel_c *a = new voxel_c(16, 8, 4);

    for (int p = 0; p < 10; p++)
      a->set(rand() % 0x1FF, 1);

    voxel_c *b0 = new voxel_c(a);
    voxel_c *b1 = new voxel_c(a);
    voxel_c *b2 = new voxel_c(a);
    voxel_c *b3 = new voxel_c(a);
    voxel_c *b4 = new voxel_c(a);
    b0->rotatez(0);
    b1->rotatez(1);
    b2->rotatez(2);
    b3->rotatez(3);
    b4->rotatez(4);

    printf("0\n");

    if (!(*a == *b0)
        || (a->boundX1() != b0->boundX1())
        || (a->boundX2() != b0->boundX2())
        || (a->boundY1() != b0->boundY1())
        || (a->boundY2() != b0->boundY2())
        || (a->boundZ1() != b0->boundZ1())
        || (a->boundZ2() != b0->boundZ2())
       ) comparison(a, b0);

    a->rotatez(1);

    printf("1\n");
    if (!(*a == *b1)
        || (a->boundX1() != b1->boundX1())
        || (a->boundX2() != b1->boundX2())
        || (a->boundY1() != b1->boundY1())
        || (a->boundY2() != b1->boundY2())
        || (a->boundZ1() != b1->boundZ1())
        || (a->boundZ2() != b1->boundZ2())
       ) comparison(a, b1);

    a->rotatez(1);

    printf("2\n");
    if (!(*a == *b2)
        || (a->boundX1() != b2->boundX1())
        || (a->boundX2() != b2->boundX2())
        || (a->boundY1() != b2->boundY1())
        || (a->boundY2() != b2->boundY2())
        || (a->boundZ1() != b2->boundZ1())
        || (a->boundZ2() != b2->boundZ2())
       ) comparison(a, b2);

    a->rotatez(1);

    printf("3\n");
    if (!(*a == *b3)
        || (a->boundX1() != b3->boundX1())
        || (a->boundX2() != b3->boundX2())
        || (a->boundY1() != b3->boundY1())
        || (a->boundY2() != b3->boundY2())
        || (a->boundZ1() != b3->boundZ1())
        || (a->boundZ2() != b3->boundZ2())
       ) comparison(a, b3);

    a->rotatez(1);

    printf("4\n");
    if (!(*a == *b4)
        || (a->boundX1() != b4->boundX1())
        || (a->boundX2() != b4->boundX2())
        || (a->boundY1() != b4->boundY1())
        || (a->boundY2() != b4->boundY2())
        || (a->boundZ1() != b4->boundZ1())
        || (a->boundZ2() != b4->boundZ2())
       ) comparison(a, b4);

  }
}

int main(int argv, char* args[]) {

//  multTranformationsMatrix();
//  inverseTranformationsMatrix();

  grow(argv, args);
//  solve(argv, agrs);
//  findsymmetries();
//  savetoXML(argv, args);

//  testNewRots();

//  convert(argv, args);
}



