/*
 * program to analyze packing the following packing problem:
 *
 * n pieces made of board with thicknes t
 * and the other dimension all ranging from t+1 to t+2*n
 * should fit into cube with only a few possible solutions
 *
 */

#include "stdio.h"
#include "math.h"

#include "voxel.h"
#include "assembler_0.h"

#include <fstream.h>

class asm_cb : public assembler_cb {

public:

  int count;

  bool assembly(voxel_c * assm) {
    if (++count >= 1000)
      return false;
    else
      return true;
  }

};

#define MAXPIECES 32

int w[MAXPIECES];
int h[MAXPIECES];

int use[2*MAXPIECES+2];

void pairsearch(int pos, int pieces) {

  if (pos == pieces) {

      /*
       * find smallest possible dimension of square:
       * it's the minimum of max dimension piece and volume
       */

      /* calculate surface of all pieces */
      int vol = 0;

      for (int p = 0; p < pieces; p++)
        vol += w[p] * h[p];

      int mindim = int(exp(log(vol)/2.0) + 0.99999);

      int dim;

      if (mindim < 2*pieces)
        dim = 2*pieces;
      else
        dim = mindim;

      for (int i = 0; i < pieces; i++)
        printf("(%i %i), ", w[i], h[i]);

      printf("area = %6i, dim_vol = %3i, dim_sz = %3i\n", vol, mindim, 2*pieces);

      puzzle_c pz;

      /* set the pieces
       the first piece should be the one with most volume to have te most
       of the optimization
       */
      int max = 0;

      for (int p = 1; p < pieces; p++)
        if (w[max] * h[max] < w[p] * h[p])
          max = p;

      pz.addPiece(new voxel_c(1, w[max], h[max], VX_FILLED));

      for (int p = 1; p < pieces; p++)
        if (p != max)
          pz.addPiece(new voxel_c(1, w[p], h[p], VX_FILLED));

      asm_cb a;

      do {
        /*
         * try to solve puzzle with increased solution size until
         * we have at least one solution
         */
  
        pz.getResult()->resize(1, dim, dim, VX_VARIABLE);
        pz.getResult()->setAll(VX_VARIABLE);

        std::ofstream str("test.puzzle");
        pz.save(&str);
  
        assembler_0_c assm(&pz);
  
//        printf("    reduction by %i placements\n", assm.reduce(1, 1));
  
        a.count = 0;
      
        assm.assemble(&a);
  
        printf("  dim = %3i, sol = %i\n", dim, a.count);

        dim++;

      } while (a.count == 0);

  }

  /*
   * find smallest not used number
   */
  int num = 0;
  while (use[num]) num++;

  /* use this one for first of the pair
   */
  w[pos] = num + 1;
  use[num] = 1;

  /* use all other number bigger for the 2nd
   * number of the pair */

  int p = num + 1;
  while (p < pieces*2) {
    if (!use[p]) {
      use[p] = 1;
      h[pos] = p + 1;
      pairsearch(pos+1, pieces);
      use[p] = 0;
    }
    p++;
  }

  use[num] = 0;
}


int main() {

  /*
   * for first try, lets go from 4 to 32 pieces
   */
  for (int pieces = 9; pieces < 12 ; pieces++) {

    /*
     * find all possible pairs of the numbers 1 to 2*pieces
     */

    for (int i = 0; i < 2*MAXPIECES+2; i++)
      use[i] = 0;

    pairsearch(0, pieces);
    printf("\n");
  }

}
