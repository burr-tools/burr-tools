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
#include "bitfield.h"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE( bitfield_test )
{
  bitfield_c<240> bits;

  for (int i = 0; i < 240; i++)
    BOOST_CHECK( !bits.get(i) );

  bits.set(1);

  for (int i = 0; i < 240; i++)
    if (i == 1)
      BOOST_CHECK( bits.get(i) );
    else
      BOOST_CHECK( !bits.get(i) );


  bits.set(2);

  for (int i = 0; i < 240; i++)
    if (i == 1 || i == 2)
      BOOST_CHECK( bits.get(i) );
    else
      BOOST_CHECK( !bits.get(i) );

  bits.set(6);

  for (int i = 0; i < 240; i++)
    if (i == 1 || i == 2 || i == 6)
      BOOST_CHECK( bits.get(i) );
    else
      BOOST_CHECK( !bits.get(i) );

  bits.clear();

  for (int i = 0; i < 240; i++)
    BOOST_CHECK( !bits.get(i) );

  for (int i = 0; i < 240; i++)
    bits.set(i);

  for (int i = 0; i < 240; i++)
    BOOST_CHECK( bits.get(i) );

  bits.reset(1);

  for (int i = 0; i < 240; i++)
    if (i == 1)
      BOOST_CHECK( !bits.get(i) );
    else
      BOOST_CHECK( bits.get(i) );

  bits.reset(5);

  for (int i = 0; i < 240; i++)
    if (i == 1 || i == 5)
      BOOST_CHECK( !bits.get(i) );
    else
      BOOST_CHECK( bits.get(i) );

  bits.clear();

  BOOST_CHECK( !bits.notNull() );

  bits.set(100);

  BOOST_CHECK( bits.notNull() );

  bits.clear();
  bits.set(239);

  BOOST_CHECK( bits.notNull() );
}

