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
#include "dl_assembler.h"

class asm_cb : public assembler_cb {

public:

  int count;

  bool assembly(voxel_c * assm) {
    count++;
    return true;
  }

};

#define MAXPIECES 32

int w[MAXPIECES];
int h[MAXPIECES];

int use[2*MAXPIECES+2];

void pairsearch(int pos, int pieces) {

  if (pos == pieces) {

    /* try serveral thicknesses */
    for (int th = 1; th < 5; th++) {

      /*
       * find smallest possible dimension of box:
       * it's the minimum of max dimension piece and volume
       */

      /* calculate volume of all pieces */
      int vol = 0;

      for (int p = 0; p < pieces; p++)
        vol += th * (th + w[p]) * (th + h[p]);

      int mindim = int(exp(log(vol)/3.0) + 0.99999);

      int dim;

      if (mindim < th+2*pieces)
        dim = th+2*pieces;
      else
        dim = mindim;

#if 1

      if (dim*dim*dim == vol) {

        printf("dim = %3i, th = %2i, pieces = %2i, ( ", dim, th, pieces);
        for (int i = 0; i < pieces; i++)
          printf("(%3i%3i)", w[i], h[i]);
        printf(" )\n");

#if 0
        puzzle_c pz;
  
        /* set the pieces
         the first piece should be the one with most volume to have te most
         of the optimization
         */
        int max = 0;
  
        for (int p = 1; p < pieces; p++)
          if (th * (th+w[max]) * (th+h[max]) < th * (th+w[p]) * (th+h[p]))
            max = p;
  
        pz.addPiece(new voxel_c(th, th+w[max], th+h[max], '#'));
  
        for (int p = 1; p < pieces; p++)
          if (p != max)
            pz.addPiece(new voxel_c(th, th+w[p], th+h[p], '#'));
  
        /*
         * try to solve puzzle with increased solution size until
         * we have at least one solution
         */
        asm_cb a;
  
        pz.setResult(new voxel_c(dim, dim, dim, '#'));

        printf("preparing...\n");
        dancing_link_assembler_c assm(&pz);

        printf("    reduction by %i placements\n", assm.reduce(1, 1));
  
        a.count = 0;

        printf("solving...\n");
        assm.assemble(&a);
  
        printf("  dim = %3i, sol = %i\n", dim, a.count);
#endif
      }

#else

      printf(" th = %i, vol = %6i, dim_vol = %3i, dim_sz = %3i\n", th, vol, mindim, th+2*pieces);

      puzzle_c pz;

      /* set the pieces
       the first piece should be the one with most volume to have te most
       of the optimization
       */
      int max = 0;

      for (int p = 1; p < pieces; p++)
        if (th * (th+w[max]) * (th+h[max]) < th * (th+w[p]) * (th+h[p]))
          max = p;

      pz.addPiece(new voxel_c(th, th+w[max], th+h[max], '#'));

      for (int p = 1; p < pieces; p++)
        if (p != max)
          pz.addPiece(new voxel_c(th, th+w[p], th+h[p], '#'));


      for

      /*
       * try to solve puzzle with increased solution size until
       * we have at least one solution
       */
      asm_cb a;

      pz.setResult(new voxel_c(dim, dim, dim, '#'));

      printf(".\n");

      dancing_link_assembler_c assm(&pz);

      printf(".\n");

      printf("    reduction by %i placements\n", assm.reduce(1, 1));

      a.count = 0;
    
      assm.assemble(&a);

      printf("  dim = %3i, sol = %i\n", dim, a.count);

#endif

    }
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
  for (int pieces = 2; pieces < 12 ; pieces++) {

    /*
     * find all possible pairs of the numbers 1 to 2*pieces
     */

    for (int i = 0; i < 2*MAXPIECES+2; i++)
      use[i] = 0;

    pairsearch(0, pieces);
    printf("\n");
  }

}
