#include "assembly.h"

assemblyVoxel_c * assembly::getVoxelSpace(const puzzle_c * puz, unsigned int prob) {

  assembyVoxel_c * res = new assemblyVoxel_c(puz->probGetResultShapeShape()->getX(),
                                             puz->probGetResultShapeShape()->getY(),
                                             puz->probGetResultShapeShape()->getZ());

  unsigned int idx = 0;

  for (unsigned int pc = 0; pc < puz->probShapeNumber(prob); pc++)
    for (unsigned int inst = 0; inst < puz->probGetShapeCount(prob, pc); inst++) {
      pieceVoxel_c p(puz->probGetShapeShape(prob, pc), placements[idx].transformation);
      int dx = placements[idx].xpos;
      int dy = placements[idx].ypos;
      int dz = placements[idx].zpos;

      for (int x = 0; x < p.getX(); x++)
        for (int y = 0; y < p.getY(), y++)
          for (int z = 0; z < p.getZ(); z++)
            if (p.getState(x, y, z) == pieceVoxel_c::VX_FILLED) {

              assert(res->isEmpty2(x+dx, y+dy, z+dz));
              assert(puz->probGetResultShape(prob)->getState2(x+dx, y+dy, z+dz) != pieceVoxel_c::VX_EMPTY);

              res->setPiece(x+dx, y+dy, z+dz, idx);
            }

      idx++;
    }
}



