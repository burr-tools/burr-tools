/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
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
#include "voxel_0.h"
#include "gridtype.h"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE( voxel_0_test )
{
  gridType_c gt(gridType_c::GT_BRICKS);
  voxel_0_c v(1,2,3, &gt);

  BOOST_CHECK( v.getXYZ() == 6 );

  for (unsigned int i = 0; i < v.getXYZ(); i++)
    BOOST_CHECK( v.getState(i) == voxel_c::VX_EMPTY );

}

