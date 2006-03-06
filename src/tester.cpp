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
#include <iostream>

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
0x000000FFFFFFLL,
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

  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

unsigned int syms = 92;

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


static const unsigned int transMult[NUM_TRANSFORMATIONS_MIRROR][NUM_TRANSFORMATIONS_MIRROR] = {
  { 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47},
  { 1,  2,  3,  0,  5,  6,  7,  4,  9, 10, 11,  8, 13, 14, 15, 12, 17, 18, 19, 16, 21, 22, 23, 20, 25, 26, 27, 24, 29, 30, 31, 28, 33, 34, 35, 32, 37, 38, 39, 36, 41, 42, 43, 40, 45, 46, 47, 44},
  { 2,  3,  0,  1,  6,  7,  4,  5, 10, 11,  8,  9, 14, 15, 12, 13, 18, 19, 16, 17, 22, 23, 20, 21, 26, 27, 24, 25, 30, 31, 28, 29, 34, 35, 32, 33, 38, 39, 36, 37, 42, 43, 40, 41, 46, 47, 44, 45},
  { 3,  0,  1,  2,  7,  4,  5,  6, 11,  8,  9, 10, 15, 12, 13, 14, 19, 16, 17, 18, 23, 20, 21, 22, 27, 24, 25, 26, 31, 28, 29, 30, 35, 32, 33, 34, 39, 36, 37, 38, 43, 40, 41, 42, 47, 44, 45, 46},
  { 4, 21, 14, 19,  8, 22,  2, 18, 12, 23,  6, 17,  0, 20, 10, 16,  5,  1, 13,  9,  7, 11, 15,  3, 36, 41, 30, 47, 24, 40, 34, 44, 28, 43, 38, 45, 32, 42, 26, 46, 39, 35, 31, 27, 37, 25, 29, 33},
  { 5, 22, 15, 16,  9, 23,  3, 19, 13, 20,  7, 18,  1, 21, 11, 17,  6,  2, 14, 10,  4,  8, 12,  0, 37, 42, 31, 44, 25, 41, 35, 45, 29, 40, 39, 46, 33, 43, 27, 47, 36, 32, 28, 24, 38, 26, 30, 34},
  { 6, 23, 12, 17, 10, 20,  0, 16, 14, 21,  4, 19,  2, 22,  8, 18,  7,  3, 15, 11,  5,  9, 13,  1, 38, 43, 28, 45, 26, 42, 32, 46, 30, 41, 36, 47, 34, 40, 24, 44, 37, 33, 29, 25, 39, 27, 31, 35},
  { 7, 20, 13, 18, 11, 21,  1, 17, 15, 22,  5, 16,  3, 23,  9, 19,  4,  0, 12,  8,  6, 10, 14,  2, 39, 40, 29, 46, 27, 43, 33, 47, 31, 42, 37, 44, 35, 41, 25, 45, 38, 34, 30, 26, 36, 24, 28, 32},
  { 8, 11, 10,  9, 12, 15, 14, 13,  0,  3,  2,  1,  4,  7,  6,  5, 22, 21, 20, 23, 18, 17, 16, 19, 32, 35, 34, 33, 36, 39, 38, 37, 24, 27, 26, 25, 28, 31, 30, 29, 46, 45, 44, 47, 42, 41, 40, 43},
  { 9,  8, 11, 10, 13, 12, 15, 14,  1,  0,  3,  2,  5,  4,  7,  6, 23, 22, 21, 20, 19, 18, 17, 16, 33, 32, 35, 34, 37, 36, 39, 38, 25, 24, 27, 26, 29, 28, 31, 30, 47, 46, 45, 44, 43, 42, 41, 40},
  {10,  9,  8, 11, 14, 13, 12, 15,  2,  1,  0,  3,  6,  5,  4,  7, 20, 23, 22, 21, 16, 19, 18, 17, 34, 33, 32, 35, 38, 37, 36, 39, 26, 25, 24, 27, 30, 29, 28, 31, 44, 47, 46, 45, 40, 43, 42, 41},
  {11, 10,  9,  8, 15, 14, 13, 12,  3,  2,  1,  0,  7,  6,  5,  4, 21, 20, 23, 22, 17, 16, 19, 18, 35, 34, 33, 32, 39, 38, 37, 36, 27, 26, 25, 24, 31, 30, 29, 28, 45, 44, 47, 46, 41, 40, 43, 42},
  {12, 17,  6, 23,  0, 16, 10, 20,  4, 19, 14, 21,  8, 18,  2, 22, 15, 11,  7,  3, 13,  1,  5,  9, 28, 45, 38, 43, 32, 46, 26, 42, 36, 47, 30, 41, 24, 44, 34, 40, 29, 25, 37, 33, 31, 35, 39, 27},
  {13, 18,  7, 20,  1, 17, 11, 21,  5, 16, 15, 22,  9, 19,  3, 23, 12,  8,  4,  0, 14,  2,  6, 10, 29, 46, 39, 40, 33, 47, 27, 43, 37, 44, 31, 42, 25, 45, 35, 41, 30, 26, 38, 34, 28, 32, 36, 24},
  {14, 19,  4, 21,  2, 18,  8, 22,  6, 17, 12, 23, 10, 16,  0, 20, 13,  9,  5,  1, 15,  3,  7, 11, 30, 47, 36, 41, 34, 44, 24, 40, 38, 45, 28, 43, 26, 46, 32, 42, 31, 27, 39, 35, 29, 33, 37, 25},
  {15, 16,  5, 22,  3, 19,  9, 23,  7, 18, 13, 20, 11, 17,  1, 21, 14, 10,  6,  2, 12,  0,  4,  8, 31, 44, 37, 42, 35, 45, 25, 41, 39, 46, 29, 40, 27, 47, 33, 43, 28, 24, 36, 32, 30, 34, 38, 26},
  {16,  5, 22, 15, 19,  9, 23,  3, 18, 13, 20,  7, 17,  1, 21, 11, 10,  6,  2, 14,  0,  4,  8, 12, 44, 37, 42, 31, 45, 25, 41, 35, 46, 29, 40, 39, 47, 33, 43, 27, 24, 36, 32, 28, 34, 38, 26, 30},
  {17,  6, 23, 12, 16, 10, 20,  0, 19, 14, 21,  4, 18,  2, 22,  8, 11,  7,  3, 15,  1,  5,  9, 13, 45, 38, 43, 28, 46, 26, 42, 32, 47, 30, 41, 36, 44, 34, 40, 24, 25, 37, 33, 29, 35, 39, 27, 31},
  {18,  7, 20, 13, 17, 11, 21,  1, 16, 15, 22,  5, 19,  3, 23,  9,  8,  4,  0, 12,  2,  6, 10, 14, 46, 39, 40, 29, 47, 27, 43, 33, 44, 31, 42, 37, 45, 35, 41, 25, 26, 38, 34, 30, 32, 36, 24, 28},
  {19,  4, 21, 14, 18,  8, 22,  2, 17, 12, 23,  6, 16,  0, 20, 10,  9,  5,  1, 13,  3,  7, 11, 15, 47, 36, 41, 30, 44, 24, 40, 34, 45, 28, 43, 38, 46, 32, 42, 26, 27, 39, 35, 31, 33, 37, 25, 29},
  {20, 13, 18,  7, 21,  1, 17, 11, 22,  5, 16, 15, 23,  9, 19,  3,  0, 12,  8,  4, 10, 14,  2,  6, 40, 29, 46, 39, 43, 33, 47, 27, 42, 37, 44, 31, 41, 25, 45, 35, 34, 30, 26, 38, 24, 28, 32, 36},
  {21, 14, 19,  4, 22,  2, 18,  8, 23,  6, 17, 12, 20, 10, 16,  0,  1, 13,  9,  5, 11, 15,  3,  7, 41, 30, 47, 36, 40, 34, 44, 24, 43, 38, 45, 28, 42, 26, 46, 32, 35, 31, 27, 39, 25, 29, 33, 37},
  {22, 15, 16,  5, 23,  3, 19,  9, 20,  7, 18, 13, 21, 11, 17,  1,  2, 14, 10,  6,  8, 12,  0,  4, 42, 31, 44, 37, 41, 35, 45, 25, 40, 39, 46, 29, 43, 27, 47, 33, 32, 28, 24, 36, 26, 30, 34, 38},
  {23, 12, 17,  6, 20,  0, 16, 10, 21,  4, 19, 14, 22,  8, 18,  2,  3, 15, 11,  7,  9, 13,  1,  5, 43, 28, 45, 38, 42, 32, 46, 26, 41, 36, 47, 30, 40, 24, 44, 34, 33, 29, 25, 37, 27, 31, 35, 39},
  {24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23},
  {25, 26, 27, 24, 29, 30, 31, 28, 33, 34, 35, 32, 37, 38, 39, 36, 41, 42, 43, 40, 45, 46, 47, 44,  1,  2,  3,  0,  5,  6,  7,  4,  9, 10, 11,  8, 13, 14, 15, 12, 17, 18, 19, 16, 21, 22, 23, 20},
  {26, 27, 24, 25, 30, 31, 28, 29, 34, 35, 32, 33, 38, 39, 36, 37, 42, 43, 40, 41, 46, 47, 44, 45,  2,  3,  0,  1,  6,  7,  4,  5, 10, 11,  8,  9, 14, 15, 12, 13, 18, 19, 16, 17, 22, 23, 20, 21},
  {27, 24, 25, 26, 31, 28, 29, 30, 35, 32, 33, 34, 39, 36, 37, 38, 43, 40, 41, 42, 47, 44, 45, 46,  3,  0,  1,  2,  7,  4,  5,  6, 11,  8,  9, 10, 15, 12, 13, 14, 19, 16, 17, 18, 23, 20, 21, 22},
  {28, 45, 38, 43, 32, 46, 26, 42, 36, 47, 30, 41, 24, 44, 34, 40, 29, 25, 37, 33, 31, 35, 39, 27, 12, 17,  6, 23,  0, 16, 10, 20,  4, 19, 14, 21,  8, 18,  2, 22, 15, 11,  7,  3, 13,  1,  5,  9},
  {29, 46, 39, 40, 33, 47, 27, 43, 37, 44, 31, 42, 25, 45, 35, 41, 30, 26, 38, 34, 28, 32, 36, 24, 13, 18,  7, 20,  1, 17, 11, 21,  5, 16, 15, 22,  9, 19,  3, 23, 12,  8,  4,  0, 14,  2,  6, 10},
  {30, 47, 36, 41, 34, 44, 24, 40, 38, 45, 28, 43, 26, 46, 32, 42, 31, 27, 39, 35, 29, 33, 37, 25, 14, 19,  4, 21,  2, 18,  8, 22,  6, 17, 12, 23, 10, 16,  0, 20, 13,  9,  5,  1, 15,  3,  7, 11},
  {31, 44, 37, 42, 35, 45, 25, 41, 39, 46, 29, 40, 27, 47, 33, 43, 28, 24, 36, 32, 30, 34, 38, 26, 15, 16,  5, 22,  3, 19,  9, 23,  7, 18, 13, 20, 11, 17,  1, 21, 14, 10,  6,  2, 12,  0,  4,  8},
  {32, 35, 34, 33, 36, 39, 38, 37, 24, 27, 26, 25, 28, 31, 30, 29, 46, 45, 44, 47, 42, 41, 40, 43,  8, 11, 10,  9, 12, 15, 14, 13,  0,  3,  2,  1,  4,  7,  6,  5, 22, 21, 20, 23, 18, 17, 16, 19},
  {33, 32, 35, 34, 37, 36, 39, 38, 25, 24, 27, 26, 29, 28, 31, 30, 47, 46, 45, 44, 43, 42, 41, 40,  9,  8, 11, 10, 13, 12, 15, 14,  1,  0,  3,  2,  5,  4,  7,  6, 23, 22, 21, 20, 19, 18, 17, 16},
  {34, 33, 32, 35, 38, 37, 36, 39, 26, 25, 24, 27, 30, 29, 28, 31, 44, 47, 46, 45, 40, 43, 42, 41, 10,  9,  8, 11, 14, 13, 12, 15,  2,  1,  0,  3,  6,  5,  4,  7, 20, 23, 22, 21, 16, 19, 18, 17},
  {35, 34, 33, 32, 39, 38, 37, 36, 27, 26, 25, 24, 31, 30, 29, 28, 45, 44, 47, 46, 41, 40, 43, 42, 11, 10,  9,  8, 15, 14, 13, 12,  3,  2,  1,  0,  7,  6,  5,  4, 21, 20, 23, 22, 17, 16, 19, 18},
  {36, 41, 30, 47, 24, 40, 34, 44, 28, 43, 38, 45, 32, 42, 26, 46, 39, 35, 31, 27, 37, 25, 29, 33,  4, 21, 14, 19,  8, 22,  2, 18, 12, 23,  6, 17,  0, 20, 10, 16,  5,  1, 13,  9,  7, 11, 15,  3},
  {37, 42, 31, 44, 25, 41, 35, 45, 29, 40, 39, 46, 33, 43, 27, 47, 36, 32, 28, 24, 38, 26, 30, 34,  5, 22, 15, 16,  9, 23,  3, 19, 13, 20,  7, 18,  1, 21, 11, 17,  6,  2, 14, 10,  4,  8, 12,  0},
  {38, 43, 28, 45, 26, 42, 32, 46, 30, 41, 36, 47, 34, 40, 24, 44, 37, 33, 29, 25, 39, 27, 31, 35,  6, 23, 12, 17, 10, 20,  0, 16, 14, 21,  4, 19,  2, 22,  8, 18,  7,  3, 15, 11,  5,  9, 13,  1},
  {39, 40, 29, 46, 27, 43, 33, 47, 31, 42, 37, 44, 35, 41, 25, 45, 38, 34, 30, 26, 36, 24, 28, 32,  7, 20, 13, 18, 11, 21,  1, 17, 15, 22,  5, 16,  3, 23,  9, 19,  4,  0, 12,  8,  6, 10, 14,  2},
  {40, 29, 46, 39, 43, 33, 47, 27, 42, 37, 44, 31, 41, 25, 45, 35, 34, 30, 26, 38, 24, 28, 32, 36, 20, 13, 18,  7, 21,  1, 17, 11, 22,  5, 16, 15, 23,  9, 19,  3,  0, 12,  8,  4, 10, 14,  2,  6},
  {41, 30, 47, 36, 40, 34, 44, 24, 43, 38, 45, 28, 42, 26, 46, 32, 35, 31, 27, 39, 25, 29, 33, 37, 21, 14, 19,  4, 22,  2, 18,  8, 23,  6, 17, 12, 20, 10, 16,  0,  1, 13,  9,  5, 11, 15,  3,  7},
  {42, 31, 44, 37, 41, 35, 45, 25, 40, 39, 46, 29, 43, 27, 47, 33, 32, 28, 24, 36, 26, 30, 34, 38, 22, 15, 16,  5, 23,  3, 19,  9, 20,  7, 18, 13, 21, 11, 17,  1,  2, 14, 10,  6,  8, 12,  0,  4},
  {43, 28, 45, 38, 42, 32, 46, 26, 41, 36, 47, 30, 40, 24, 44, 34, 33, 29, 25, 37, 27, 31, 35, 39, 23, 12, 17,  6, 20,  0, 16, 10, 21,  4, 19, 14, 22,  8, 18,  2,  3, 15, 11,  7,  9, 13,  1,  5},
  {44, 37, 42, 31, 45, 25, 41, 35, 46, 29, 40, 39, 47, 33, 43, 27, 24, 36, 32, 28, 34, 38, 26, 30, 16,  5, 22, 15, 19,  9, 23,  3, 18, 13, 20,  7, 17,  1, 21, 11, 10,  6,  2, 14,  0,  4,  8, 12},
  {45, 38, 43, 28, 46, 26, 42, 32, 47, 30, 41, 36, 44, 34, 40, 24, 25, 37, 33, 29, 35, 39, 27, 31, 17,  6, 23, 12, 16, 10, 20,  0, 19, 14, 21,  4, 18,  2, 22,  8, 11,  7,  3, 15,  1,  5,  9, 13},
  {46, 39, 40, 29, 47, 27, 43, 33, 44, 31, 42, 37, 45, 35, 41, 25, 26, 38, 34, 30, 32, 36, 24, 28, 18,  7, 20, 13, 17, 11, 21,  1, 16, 15, 22,  5, 19,  3, 23,  9,  8,  4,  0, 12,  2,  6, 10, 14},
  {47, 36, 41, 30, 44, 24, 40, 34, 45, 28, 43, 38, 46, 32, 42, 26, 27, 39, 35, 31, 33, 37, 25, 29, 19,  4, 21, 14, 18,  8, 22,  2, 17, 12, 23,  6, 16,  0, 20, 10,  9,  5,  1, 13,  3,  7, 11, 15}
};


/* this function creates a table that contains ALL bits for the symmetries
* instead of only the ones for the current rotation, that means all bits
* are set that where the shape reproduces itself in one of the 48 possible
* rotations
*/
void outputCompleteSymmetries(void) {


#if 0
  ifstream str("symmetric.xmpuzzle");

  if (!str) {
    cout << "oope file not opened\n";
    return;
  }

  xml::tree_parser parser("symmetric.xmpuzzle");
  puzzle_c p(parser.get_document().get_root_node());

  for (int i = 0; i < p.shapeNumber(); i++) {
    printf("st: %012llx  ", foundSym[p.getShape(i)->selfSymmetries()]);

    for (int j = 1; j < /*NUM_TRANSFORMATIONS_MIRROR*/9; j++) {
      voxel_c t(p.getShape(i), j);
      printf("%012llx ", foundSym[t.selfSymmetries()]);
    }

    printf("\n");
  }
#endif

  printf("{\n");
  for (int sy = 0; sy < syms; sy++) {

    unsigned long long s = foundSym[sy];

    unsigned long long out = s;

    for (int r = 1; r < NUM_TRANSFORMATIONS_MIRROR; r++) {

      int rinv = 0;

      while (transMult[r][rinv]) {
        rinv++;
      }

      // if r + x + rinv one of the symmetries of the initial shape
      // then x is a symmetriy of the by r rotated shape

      for (int x = 0; x < NUM_TRANSFORMATIONS_MIRROR; x++) {
        int res = transMult[r][x];
        res = transMult[res][rinv];

        if ((s >> res) & 1) {
        out |= (1ll << x);
        }
      }

    }
    printf("0x%012llXLL,\n", out);
  }
  printf("}\n");
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
  outputCompleteSymmetries();
  makeSymmetryTree(0, 0);

//  savetoXML(argv, args);

//  testNewRots();

//  convert(argv, args);
}



