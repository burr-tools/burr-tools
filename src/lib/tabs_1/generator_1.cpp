/* Burr Solver
 * Copyright (C) 2003-2009  Andreas Röver
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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "tablesizes.inc"

double matrix[NUM_TRANSFORMATIONS_MIRROR][9] = {
#include "rotmatrix.inc"
};

/* this matix contains the concatenation of 2 transformations
 * if you first transform the piece around t1 and then around t2
 * you can as well transform around transMult[t1][t2]
 */
static unsigned int transMult[NUM_TRANSFORMATIONS_MIRROR][NUM_TRANSFORMATIONS_MIRROR];

/* this array contains all possible symmetry groups, meaning bitmasks with exactly the bits set
 * that correspond to transformations tha reorient the piece so that it looks identical
 */
static const unsigned long long symmetries[NUM_SYMMETRY_GROUPS] = {
#include "symmetries.inc"
};

const char * longlong2string(unsigned long long s) {
  char hex[17] = "0123456789ABCDEF";
  static char output[100];


  for (int i = 0; i < 12; i++)
     output[11-i] = hex[(s >> (4*i)) & 0xF];

  output[12] = 0;

  return output;
}

unsigned char ssss(unsigned int trans, unsigned long long s) {
  for (unsigned char t = 0; t < trans; t++)
    for (unsigned char t2 = 0; t2 < NUM_TRANSFORMATIONS_MIRROR; t2++)
      if (s & (((unsigned long long)1) << t2)) {
	unsigned char trrr = transMult[t2][t];
	if (trrr == trans)
	  return t;
      }

  return trans;
}


/* this function creates the lookup table for the function in the symmetry c-file */
void outputMinimumSymmetries(void) {

  FILE * out = fopen("transformmini.inc", "w");

  for (int sy = 0; sy < NUM_SYMMETRY_GROUPS; sy++) {
    fprintf(out, "  {");
    for (int trans = 0; trans < NUM_TRANSFORMATIONS_MIRROR; trans++) {
      fprintf(out,"%2i", ssss(trans, symmetries[sy]));
      if (trans < NUM_TRANSFORMATIONS_MIRROR-1)
	fprintf(out, ",");
    }
    if (sy < NUM_SYMMETRY_GROUPS-1)
      fprintf(out, "},\n");
    else
      fprintf(out, "}\n");
  }

  fclose(out);
}

/* this function creates a table that contains ALL bits for the symmetries
* instead of only the ones for the current rotation, that means all bits
* are set that where the shape reproduces itself in one of the 48 possible
* rotations
*/
void outputCompleteSymmetries(void) {

  FILE * fout = fopen("unifiedsym.inc", "w");

  for (int sy = 0; sy < NUM_SYMMETRY_GROUPS; sy++) {

    unsigned long long s = symmetries[sy];

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
    fprintf(fout, "0x%012llXLL,\n", out);
  }

  fclose(fout);
}

/* this function creates a decision tree for symmetry creation trying to optimize for the
 * lowest number of checks (6-7 should be possible, if we can subdivide each time#
 * with nearly equal subparts
 */
void makeSymmetryTree(unsigned long long taken, unsigned long long val, FILE * out) {

  /* greedy implementation: find the subdivision that is most equal */
  int best_div = -100;
  int best_bit = 0;
  int b1, b2;

  for (int t = 0; t < NUM_TRANSFORMATIONS_MIRROR; t++)
    if (!(taken & (((unsigned long long)1) << t))) {
      b1 = 0;
      b2 = 0;
      int lastfound = 0;
      for (int s = 0; s < NUM_SYMMETRY_GROUPS; s++) {
        if ((symmetries[s] & taken) == val) {
          b1++;
          lastfound = s;
        }
        if ((symmetries[s] & (taken | (((unsigned long long)1) << t))) == val)
          b2++;
      }

      if (b1 == 1) {
        fprintf(out, "return (symmetries_t)%i; //%s\n", lastfound, longlong2string(symmetries[lastfound]));
        return;
      }

      if ((b2 > 0) && (b1-b2>0) && (abs(b1/2 - best_div) > abs(b1/2 - b2))) {
        best_div = b2;
        best_bit = t;
      }
    }

  fprintf(out, "voxel_1_c v(pp);\nv.transform(%i);\nif (pp->identicalInBB(&v)) {\n", best_bit);

  makeSymmetryTree(taken | ((unsigned long long)1 << best_bit), val | ((unsigned long long)1 << best_bit), out);

  fprintf(out, "} else {\n");

  makeSymmetryTree(taken | ((unsigned long long)1 << best_bit), val, out);

  fprintf(out, "}\n");

}

/* this function only one bit is set for each possible transformation that results in one possible orientation
 * of the shape
 */
void outputUniqueSymmetries(void) {

  FILE * fout = fopen("uniquesym.inc", "w");

  for (int sy = 0; sy < NUM_SYMMETRY_GROUPS; sy++) {

    unsigned long long s = symmetries[sy];
    unsigned long long ttt = 0;
    unsigned long long out = 0;

    for (int r = 0; r < NUM_TRANSFORMATIONS_MIRROR; r++)
    {
      if (!((ttt >> r) & 1))
      {
        out |= 1ll << r;

	for (int r2 = 0; r2 < NUM_TRANSFORMATIONS_MIRROR; r2++)
	{
	  if ((s >> r2) & 1)
	    ttt |= 1ll << transMult[r2][r];
	}
      }
    }
    fprintf(fout, "0x%012llXLL,\n", out);
  }

  fclose(fout);
}

void mmult(double * m, double * matrix) {

  double n[9];

  for (int x = 0; x < 3; x++)
    for (int y = 0; y < 3; y++) {
      n[y*3+x] = 0;
      for (int c = 0; c < 3; c++) {
        n[y*3+x] += m[c*3+x]*matrix[y*3+c];
      }
    }

  for (int i = 0; i < 9; i++)
    m[i] = n[i];
}

void mmult(double * m, int num) {

  mmult(m, matrix[num]);
}

bool mequal(double *m, int num) {

  for (int i = 0; i < 9; i++)
    if (fabs(m[i]-matrix[num][i]) > 0.00001)
      return false;

  return true;
}


void multTranformationsMatrix(void) {

  FILE * out = fopen("transmult.inc", "w");

  for (int tr1 = 0; tr1 < NUM_TRANSFORMATIONS_MIRROR; tr1++) {
    fprintf(out, "{");


    for (int tr2 = 0; tr2 < NUM_TRANSFORMATIONS_MIRROR; tr2++) {

      double m[9];
      m[0] = m[4] = m[8] = 1;
      m[1] = m[2] = m[3] = m[5] = m[6] = m[7] = 0;

      mmult(m, tr1);
      mmult(m, tr2);

      bool found = false;

      for (int t = 0; t < NUM_TRANSFORMATIONS_MIRROR; t++) {

        if (mequal(m, t)) {
          if (tr2 < NUM_TRANSFORMATIONS_MIRROR-1)
            fprintf(out, "%3i,", t);
          else
            fprintf(out, "%3i", t);
          transMult[tr1][tr2] = t;
          found = true;
          break;
        }
      }

      if (!found) {
        if (tr2 < NUM_TRANSFORMATIONS_MIRROR-1)
          fprintf(out, "TND,");
        else
          fprintf(out, "TND");
        transMult[tr1][tr2] = (unsigned int)-1;
      }

    }
    if (tr1 < NUM_TRANSFORMATIONS_MIRROR-1)
      fprintf(out, "},\n");
    else
      fprintf(out, "}\n");
  }
  fclose(out);
}

int main(int /*argv*/, char** /*args[]*/) {

  multTranformationsMatrix();
  outputMinimumSymmetries();
  outputCompleteSymmetries();
  outputUniqueSymmetries();

  FILE * out = fopen("symcalc.inc", "w");
  makeSymmetryTree(0, 0, out);
  fclose(out);
}

