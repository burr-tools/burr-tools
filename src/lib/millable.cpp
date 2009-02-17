#include "millable.h"

#include "voxel.h"
#include "voxel_0.h"
#include "gridtype.h"

/* a piece is notchable, if it can be made purely by cutting */
bool isNotchable(const voxel_c * v)
{
  gridType_c gt(gridType_c::GT_BRICKS);

  voxel_0_c vxy(v->getX(), v->getY(), 1u, &gt, 0);
  voxel_0_c vxz(v->getX(), 1u, v->getZ(), &gt, 0);
  voxel_0_c vyz(1u, v->getY(), v->getZ(), &gt, 0);

  for (unsigned int x = 0; x < v->getX(); x++)
    for (unsigned int y = 0; y < v->getY(); y++)
      for (unsigned int z = 0; z < v->getZ(); z++)
        if (v->isFilled(x, y, z))
        {
          vxy.set(x, y, 0, voxel_c::VX_FILLED);
          vxz.set(x, 0, z, voxel_c::VX_FILLED);
          vyz.set(0, y, z, voxel_c::VX_FILLED);
        }

  for (unsigned int x = 0; x < v->getX(); x++)
    for (unsigned int y = 0; y < v->getY(); y++)
      for (unsigned int z = 0; z < v->getZ(); z++)
        if (v->isFilled(x, y, z) !=
            (
              vxy.isFilled(x, y, 0) &&
              vxz.isFilled(x, 0, z) &&
              vyz.isFilled(0, y, z)
            ))
          return false;


  return true;
}

/* a piece is millable, if it contains no inside corners */
bool isMillable(const voxel_c * v)
{
  for (int x = 0; x < (int)v->getX(); x++)
    for (int y = 0; y < (int)v->getY(); y++)
      for (int z = 0; z < (int)v->getZ(); z++)
        if (v->isEmpty(x, y, z))
          if ( (v->isFilled2(x-1, y, z) || v->isFilled2(x+1, y, z)) &&
               (v->isFilled2(x, y-1, z) || v->isFilled2(x, y+1, z)) &&
               (v->isFilled2(x, y, z-1) || v->isFilled2(x, y, z+1))
             )
            return false;

  return true;
}

