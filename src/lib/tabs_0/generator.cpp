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

#include <stdio.h>

#include "voxel_0.h"
#include "gridtype.h"

#include "tablesizes.inc"

static const int _rotx[NUM_TRANSFORMATIONS] = {
#include "rotx.inc"
};
static const int _roty[NUM_TRANSFORMATIONS] = {
#include "roty.inc"
};
static const int _rotz[NUM_TRANSFORMATIONS] = {
#include "rotz.inc"
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
      int lastfound;
      for (int s = 0; s < NUM_SYMMETRY_GROUPS; s++) {
        if ((symmetries[s] & taken) == val) {
          b1++;
          lastfound = s;
        }
        if ((symmetries[s] & (taken | (((unsigned long long)1) << t))) == val)
          b2++;
      }

      if (b1 == 1) {
        fprintf(out, "bt_assert(s == %i);\nreturn (symmetries_t)%i; //%s\n",lastfound, lastfound, longlong2string(symmetries[lastfound]));
        return;
      }

      if ((b2 > 0) && (b1-b2>0) && (abs(b1/2 - best_div) > abs(b1/2 - b2))) {
        best_div = b2;
        best_bit = t;
      }
    }

  fprintf(out, "voxel_0_c v(pp);\nv.transform(%i);\nif (pp->identicalInBB(&v)) {\n", best_bit);

  makeSymmetryTree(taken | ((unsigned long long)1 << best_bit), val | ((unsigned long long)1 << best_bit), out);

  fprintf(out, "} else {\n");

  makeSymmetryTree(taken | ((unsigned long long)1 << best_bit), val, out);

  fprintf(out, "}\n");

}

void multTranformationsMatrix(void) {

  FILE * out = fopen("transmult.inc", "w");

  gridType_c * gt = new gridType_c();

  voxel_c * w[NUM_TRANSFORMATIONS_MIRROR];

  for (int t = 0; t < NUM_TRANSFORMATIONS_MIRROR; t++) {
    w[t] = new voxel_0_c(3, 3, 3, gt);
    w[t]->setState(1, 1, 1, voxel_c::VX_FILLED); w[t]->setState(0, 1, 1, voxel_c::VX_FILLED);
    w[t]->setState(0, 0, 1, voxel_c::VX_FILLED); w[t]->setState(0, 0, 0, voxel_c::VX_FILLED);

    w[t]->transform(t);
  }

  for (int tr1 = 0; tr1 < NUM_TRANSFORMATIONS_MIRROR; tr1++) {
    fprintf(out, "{");

    voxel_0_c v(3, 3, 3, gt);
    v.setState(1, 1, 1, voxel_c::VX_FILLED); v.setState(0, 1, 1, voxel_c::VX_FILLED);
    v.setState(0, 0, 1, voxel_c::VX_FILLED); v.setState(0, 0, 0, voxel_c::VX_FILLED);
    v.transform(tr1);

    for (int tr2 = 0; tr2 < NUM_TRANSFORMATIONS_MIRROR; tr2++) {

      voxel_0_c v2(v);
      v2.transform(tr2);

      for (int t = 0; t < NUM_TRANSFORMATIONS_MIRROR; t++) {
        if (v2 == *w[t]) {
          if (tr2 < NUM_TRANSFORMATIONS_MIRROR-1)
            fprintf(out, "%2i, ", t);
          else
            fprintf(out, "%2i", t);
          transMult[tr1][tr2] = t;
          break;
        }
      }
    }
    if (tr1 < NUM_TRANSFORMATIONS_MIRROR-1)
      fprintf(out, "},\n");
    else
      fprintf(out, "}\n");
  }
  fclose(out);
}

#if 0
void inverseTranformationsMatrix(void) {

  voxel_0_c v(3, 3, 3);
  v.setState(1, 1, 1, voxel_c::VX_FILLED); v.setState(0, 1, 1, voxel_c::VX_FILLED);
  v.setState(0, 0, 1, voxel_c::VX_FILLED); v.setState(0, 0, 0, voxel_c::VX_FILLED);

  for (int tr = 0; tr < NUM_TRANSFORMATIONS_MIRROR; tr++) {

    voxel_0_c w(v, tr);

    for (int t = 0; t < NUM_TRANSFORMATIONS_MIRROR; t++) {

      voxel_0_c x(w, t);

      if (v == x) {
        printf("%2i, ", t);
        break;
      }
    }
  }
}
#endif

int main(int argv, char* args[]) {

  multTranformationsMatrix();
  outputMinimumSymmetries();
  outputCompleteSymmetries();

  FILE * out = fopen("symcalc.inc", "w");
  makeSymmetryTree(0, 0, out);
  fclose(out);

//  inverseTranformationsMatrix(gt);
}

