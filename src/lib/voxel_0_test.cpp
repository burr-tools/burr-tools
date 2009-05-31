#include "voxel_0.h"
#include "gridtype.h"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE( voxel_0_test )
{
  gridType_c gt(gridType_c::GT_BRICKS);
  voxel_0_c v(1,2,3, &gt);
  
  BOOST_CHECK( v.getXYZ() == 6 );
  
  for (int i = 0; i < v.getXYZ(); i++)
    BOOST_CHECK( v.getState(i) == voxel_c::VX_EMPTY );
  
}

