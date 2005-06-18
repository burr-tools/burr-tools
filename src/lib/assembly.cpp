#include "assembly.h"

assemblyVoxel_c * assembly_c::getVoxelSpace(const puzzle_c * puz, unsigned int prob) {

  assemblyVoxel_c * res = new assemblyVoxel_c(puz->probGetResultShape(prob)->getX(),
                                              puz->probGetResultShape(prob)->getY(),
                                              puz->probGetResultShape(prob)->getZ());

  unsigned int idx = 0;

  for (unsigned int pc = 0; pc < puz->probShapeNumber(prob); pc++)
    for (unsigned int inst = 0; inst < puz->probGetShapeCount(prob, pc); inst++) {
      pieceVoxel_c p(puz->probGetShapeShape(prob, pc), placements[idx].transformation);
      int dx = placements[idx].xpos;
      int dy = placements[idx].ypos;
      int dz = placements[idx].zpos;

//      printf("%i %i %i, t: %i\n", dx, dy, dz, placements[idx].transformation);
//      print(&p);

      for (unsigned int x = 0; x < p.getX(); x++)
        for (unsigned int y = 0; y < p.getY(); y++)
          for (unsigned int z = 0; z < p.getZ(); z++)
            if (p.getState(x, y, z) == pieceVoxel_c::VX_FILLED) {

              assert(res->isEmpty2(x+dx, y+dy, z+dz));
              assert(puz->probGetResultShape(prob)->getState2(x+dx, y+dy, z+dz) != pieceVoxel_c::VX_EMPTY);

              res->setPiece(x+dx, y+dy, z+dz, idx);
            }

      idx++;
    }

  return res;

}



