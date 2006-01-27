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

  bt_assert(nr < NUM_TRANSFORMATIONS);

  static int rotx[NUM_TRANSFORMATIONS] = { 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3 };
  static int roty[NUM_TRANSFORMATIONS] = { 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0 };
  static int rotz[NUM_TRANSFORMATIONS] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 3, 3, 3, 3 };

  voxel_c *erg = new voxel_c(*p);

  for (int i = 0; i < rotx[nr]; i++) erg->rotatex();
  for (int i = 0; i < roty[nr]; i++) erg->rotatey();
  for (int i = 0; i < rotz[nr]; i++) erg->rotatez();

  return erg;
}

unsigned long long foundSym[200] = {

0x000000000001LL,
0x000000000005LL,
0x00000000000FLL,
0x000000000041LL,
0x000000000101LL,
0x000000000201LL,
0x000000000401LL,
0x000000000505LL,
0x000000000801LL,
0x000000000A05LL,
0x000000001111LL,
0x000000004001LL,
0x000000004141LL,
0x000000020081LL,
0x000000040001LL,
0x000000082001LL,
0x000000110401LL,
0x000000208001LL,
0x000000248241LL,
0x000000400001LL,
0x000000424281LL,
0x000000440401LL,
0x000000482841LL,
0x000000800021LL,
0x000000844821LL,
0x000000AAA5A5LL,
0x000001000001LL,
0x000004000001LL,
0x000005000005LL,
0x00000F00000FLL,
0x000010000001LL,
0x000014000041LL,
0x000100000001LL,
0x000101000101LL,
0x000104000401LL,
0x000200000001LL,
0x000201000201LL,
0x000204000801LL,
0x000400000001LL,
0x000401000401LL,
0x000404000101LL,
0x000410004001LL,
0x000500000005LL,
0x000505000505LL,
0x00050A000A05LL,
0x000800000001LL,
0x000801000801LL,
0x000804000201LL,
0x000A00000005LL,
0x000A05000A05LL,
0x000A0A000505LL,
0x000F0000000FLL,
0x000F0F000F0FLL,
0x001000000001LL,
0x001004004001LL,
0x001010000101LL,
0x001111001111LL,
0x001400000041LL,
0x001414004141LL,
0x004141004141LL,
0x004444001111LL,
0x005050000505LL,
0x005555005555LL,
0x010000000001LL,
0x010004040001LL,
0x010100400001LL,
0x010810208001LL,
0x011200800021LL,
0x028004082001LL,
0x080024020081LL,
0x100000000001LL,
0x100004400001LL,
0x100100040001LL,
0x100210082001LL,
0x101800020081LL,
0x110000000401LL,
0x110104440401LL,
0x110401110401LL,
0x128214482841LL,
0x181824424281LL,
0x200084800021LL,
0x211284844821LL,
0x440104110401LL,
0x440401440401LL,
0x550000000505LL,
0x550505550505LL,
0x555A5AAAA5A5LL,
0x802004208001LL,
0x812814248241LL,
0xAAA5A5AAA5A5LL,
0xFFFFFFFFFFFFLL,

  0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

unsigned int syms = 91;

const char * longlong2string(unsigned long long s) {
  char hex[17] = "0123456789ABCDEF";
  static char output[13];


  for (int i = 0; i < 12; i++)
     output[11-i] = hex[(s >> (4*i)) & 0xF];

  output[12] = 0;

  return output;
}

void search(voxel_c * piece) {

#if 0
  unsigned long long s = piece->selfSymmetries();
  bool found = false;

  for (int j = 0; j < syms; j++)
    if (s == foundSym[j]) {
      found = true;
      break;
    }

  if (!found) {
    printf("%s\n", longlong2string(s));
    foundSym[syms++] = s;

    char fn[200];

    snprintf(fn, 200, "symmetries/%s.xmpuzzle", longlong2string(s));

    ofstream o(fn);

    o << piece->save();
  }
#endif
}

unsigned char ssss(unsigned int trans, unsigned long long s) {
  for (unsigned char t = 0; t < trans; t++)
    for (unsigned char t2 = 0; t2 < NUM_TRANSFORMATIONS_MIRROR; t2++)
      if (s & (((unsigned long long)1) << t2)) {
	unsigned char trrr = transAdd(t2, t);
	if (trrr == trans)
	  return t;
      }

  return trans;
}


/* this function creates the lookup table for the function in the symmetry c-file */
void outputMinimumSymmetries(void) {

  printf("{\n");
  for (int sy = 0; sy < syms; sy++) {
    printf("  {");
    for (int trans = 0; trans < NUM_TRANSFORMATIONS_MIRROR; trans++) {
      printf("%2i", ssss(trans, foundSym[sy]));
      if (trans < NUM_TRANSFORMATIONS_MIRROR-1)
	printf(",");
    }
    if (sy < syms -1)
      printf("},\n");
    else
      printf("}},\n");
  }
}


/* this function creates a decision tree for symmetry creation trying to optimize for the
 * lowest number of checks (6-7 should be possible, if we can subdivide each time#
 * with nearly equal subparts
 */
void makeSymmetryTree(unsigned long long taken, unsigned long long val) {

  /* greedy implementation: find the subdivision that is most equal */
  int best_div = -100;
  int best_bit = 0;
  int b1, b2;

  for (int t = 0; t < NUM_TRANSFORMATIONS_MIRROR; t++)
    if (!(taken & (((unsigned long long)1) << t))) {
      b1 = 0;
      b2 = 0;
      int lastfound;
      for (int s = 0; s < syms; s++) {
        if ((foundSym[s] & taken) == val) {
          b1++;
          lastfound = s;
        }
        if ((foundSym[s] & (taken | (((unsigned long long)1) << t))) == val)
          b2++;
      }

//      printf(" b1 %i b2 %i\n", b1 ,b2);

      if (b1 == 1) {
        printf("bt_assert(s == %i);\nreturn (symmetries_t)%i; //%s\n",lastfound, lastfound, longlong2string(foundSym[lastfound]));
        return;
      }

      if ((b2 > 0) && (b1-b2>0) && (abs(b1/2 - best_div) > abs(b1/2 - b2))) {
        best_div = b2;
        best_bit = t;
      }
    }

//  printf("/* separating into 2 groups of size %i and %i */\n", best_div, b1-best_div);

  printf("voxel_c v(pp, %i);\nif (pp->identicalInBB(&v)) {\n", best_bit);

  makeSymmetryTree(taken | ((unsigned long long)1 << best_bit), val | ((unsigned long long)1 << best_bit));

  printf("} else {\n");

  makeSymmetryTree(taken | ((unsigned long long)1 << best_bit), val);

  printf("}\n");

}





void findsymmetries(void) {

  voxel_c v(5, 5, 5);

  unsigned int indizes[100];
  unsigned int indizeCount = 0;

  printf("preparation\n");
  for (int x = 0; x < 5; x++)
    for (int y = 0; y < 5; y++)
      for (int z = 0; z < 5; z++)
        if ((x == 0) || (y == 0) || (z == 0) || (x == 4) || (y == 4) || (z == 4))
          indizes[indizeCount++] = v.getIndex(x, y, z);

  v.setAll(voxel_c::VX_FILLED);
  search(&v);

  printf("1 leerraum\n");

  for (int a = 0; a < indizeCount; a++) {

    v.setAll(voxel_c::VX_FILLED);
    v.setState(indizes[a], voxel_c::VX_EMPTY);

    search(&v);
  }

  printf("2 leerraum\n");

  for (int a = 0; a < indizeCount; a++)
    for (int b = a+1; b < indizeCount; b++) {

      v.setAll(voxel_c::VX_FILLED);
      v.setState(indizes[a], voxel_c::VX_EMPTY);
      v.setState(indizes[b], voxel_c::VX_EMPTY);

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
	      v.setAll(voxel_c::VX_FILLED);
	      v.setState(indizes[a], voxel_c::VX_EMPTY);
	      v.setState(indizes[b], voxel_c::VX_EMPTY);
	      v.setState(indizes[c], voxel_c::VX_EMPTY);
	      v.setState(indizes[d], voxel_c::VX_EMPTY);
	      v.setState(indizes[e], voxel_c::VX_EMPTY);
	      v.setState(indizes[f], voxel_c::VX_EMPTY);

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

//  srand48(time(0));
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

      voxel_c v(3, 3, 3);
      v.setState(1, 1, 1, voxel_c::VX_FILLED); v.setState(0, 1, 1, voxel_c::VX_FILLED);
      v.setState(0, 0, 1, voxel_c::VX_FILLED); v.setState(0, 0, 0, voxel_c::VX_FILLED);

      int i;

      v.transform(tr1);
      v.transform(tr2);

      for (int t = 0; t < NUM_TRANSFORMATIONS_MIRROR; t++) {
        voxel_c w(3, 3, 3);
        w.setState(1, 1, 1, voxel_c::VX_FILLED); w.setState(0, 1, 1, voxel_c::VX_FILLED);
        w.setState(0, 0, 1, voxel_c::VX_FILLED); w.setState(0, 0, 0, voxel_c::VX_FILLED);

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

  voxel_c v(3, 3, 3);
  v.setState(1, 1, 1, voxel_c::VX_FILLED); v.setState(0, 1, 1, voxel_c::VX_FILLED);
  v.setState(0, 0, 1, voxel_c::VX_FILLED); v.setState(0, 0, 0, voxel_c::VX_FILLED);

  for (int tr = 0; tr < NUM_TRANSFORMATIONS_MIRROR; tr++) {

    voxel_c w(v, tr);

    for (int t = 0; t < NUM_TRANSFORMATIONS_MIRROR; t++) {

      voxel_c x(w, t);

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

//  grow(argv, args);
//  solve(argv, agrs);
//  findsymmetries();
  outputMinimumSymmetries();
  makeSymmetryTree(0, 0);

//  savetoXML(argv, args);

//  testNewRots();

//  convert(argv, args);
}



