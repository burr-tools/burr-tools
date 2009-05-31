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
    
}

