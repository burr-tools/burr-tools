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
#include "lib/disassembler_3.h"
#include "lib/disassembler_2.h"
#include "lib/disassembler_1.h"
#include "lib/disassembler_0.h"

#include "lib/burrgrower.h"
#include "lib/pieceGenerator.h"

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

  bool assembly(assemblyVoxel_c * assm) {

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
      assm->print();
      printf("level: %i\n", da->getMoves());
//      da->print();
      delete da;
    }

    return true;
  }
};

static voxel_c * transform(const voxel_c * p, int nr) {

  assert(nr < 24);

  static int rotx[24] = { 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3 };
  static int roty[24] = { 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0 };
  static int rotz[24] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 3, 3, 3, 3 };

  voxel_c *erg = new voxel_c(*p);

  for (int i = 0; i < rotx[nr]; i++) erg->rotatex();
  for (int i = 0; i < roty[nr]; i++) erg->rotatey();
  for (int i = 0; i < rotz[nr]; i++) erg->rotatez();

  return erg;
}


unsigned int findSelfSymmetry(voxel_c * piece) {

  unsigned int result = 1;

  for (int i = 1; i < 24; i++) {

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


void search(voxel_c * piece) {

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

    piece->save(&o);
    o << "0" << endl;
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
  printf("start reduce\n");
  assm->reduce();
  printf("finished reduce\n");

  if (assm->errors()) {
    printf("%s\n", assm->errors());
    delete assm;
    return;
  }

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

  puzzle_c p/*( FIXME&str)*/;

  burrGrower_c grow(&p, 10);

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

  for (int tr1 = 0; tr1 < 48; tr1++) {
    printf("{");

    for (int tr2 = 0; tr2 < 48; tr2++) {

      pieceVoxel_c v(3, 3, 3);
      v.setState(1, 1, 1, pieceVoxel_c::VX_FILLED); v.setState(0, 1, 1, pieceVoxel_c::VX_FILLED);
      v.setState(0, 0, 1, pieceVoxel_c::VX_FILLED); v.setState(0, 0, 0, pieceVoxel_c::VX_FILLED);

      int i;

      v.transform(tr1);
      v.transform(tr2);

      for (int t = 0; t < 48; t++) {
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

int main(int argv, char* args[]) {

//  multTranformationsMatrix();

//  grow(argv, args);
//  solve(argv, agrs);
//  findsymmetries();
  savetoXML(argv, args);

//  convert(argv, args);
}



