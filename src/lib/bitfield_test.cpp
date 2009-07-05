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

