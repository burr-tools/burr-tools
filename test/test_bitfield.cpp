#include <catch2/catch_test_macros.hpp>

#include "lib/bitfield.h"

TEST_CASE("bitfield: a fresh field is all zero", "[bitfield]") {
  bitfield_c<240> bits;

  for (int i = 0; i < 240; i++)
    REQUIRE_FALSE(bits.get(i));

  REQUIRE_FALSE(bits.notNull());
}

TEST_CASE("bitfield: set and reset touch exactly one bit", "[bitfield]") {
  bitfield_c<240> bits;

  bits.set(1);
  bits.set(2);
  bits.set(6);

  for (int i = 0; i < 240; i++)
    REQUIRE(bits.get(i) == (i == 1 || i == 2 || i == 6));

  bits.clear();
  for (int i = 0; i < 240; i++)
    REQUIRE_FALSE(bits.get(i));

  for (int i = 0; i < 240; i++)
    bits.set(i);

  bits.reset(1);
  bits.reset(5);

  for (int i = 0; i < 240; i++)
    REQUIRE(bits.get(i) == !(i == 1 || i == 5));
}

TEST_CASE("bitfield: notNull sees a bit in any word", "[bitfield]") {
  bitfield_c<240> bits;

  REQUIRE_FALSE(bits.notNull());

  bits.set(100);
  REQUIRE(bits.notNull());

  bits.reset(100);
  REQUIRE_FALSE(bits.notNull());
}

TEST_CASE("bitfield: bits survive the 64-bit word boundaries", "[bitfield]") {
  bitfield_c<240> bits;

  /* the field is stored as four uint64_t words; check every seam */
  const int seams[] = { 63, 64, 127, 128, 191, 192, 239 };

  for (int pos : seams) {
    bits.clear();
    bits.set(pos);

    REQUIRE(bits.get(pos));
    REQUIRE(bits.notNull());

    for (int i = 0; i < 240; i++)
      REQUIRE(bits.get(i) == (i == pos));
  }
}

TEST_CASE("bitfield: the hex string constructor fills from the low bits up", "[bitfield]") {
  /* "1" sets only bit 0 */
  bitfield_c<240> one("1");
  REQUIRE(one.get(0));
  for (int i = 1; i < 240; i++)
    REQUIRE_FALSE(one.get(i));

  /* "f" sets bits 0..3 */
  bitfield_c<240> nibble("f");
  for (int i = 0; i < 240; i++)
    REQUIRE(nibble.get(i) == (i < 4));

  /* "abc" is the number 0xabc, lowest digit last: 'c' lands in bits 0-3,
     'b' in 4-7, 'a' in 8-11.

     Pinned against the VALUE, not against "ABC". Comparing the two spellings
     to each other says only that the two letter branches agree with one
     another -- shift both by the same amount and every hex string containing
     a-e parses wrong while the comparison still holds. 'f' was the only
     letter this case ever tied to a value. */
  bitfield_c<240> lower("abc");
  for (int i = 0; i < 240; i++)
    REQUIRE(lower.get(i) == (i < 12 && ((0xabcULL >> i) & 1) != 0));

  /* and the upper-case branch maps to the same values */
  bitfield_c<240> upper("ABC");
  for (int i = 0; i < 240; i++)
    REQUIRE(upper.get(i) == lower.get(i));

  /* each hex digit is four bits, so "10" is bit 4 only */
  bitfield_c<240> shifted("10");
  for (int i = 0; i < 240; i++)
    REQUIRE(shifted.get(i) == (i == 4));
}

TEST_CASE("bitfield: copy construction preserves every bit", "[bitfield]") {
  bitfield_c<240> src;
  src.set(0);
  src.set(64);
  src.set(239);

  bitfield_c<240> copy(src);

  for (int i = 0; i < 240; i++)
    REQUIRE(copy.get(i) == src.get(i));

  /* the copy is independent */
  copy.set(100);
  REQUIRE(copy.get(100));
  REQUIRE_FALSE(src.get(100));
}

TEST_CASE("bitfield: countbits counts full 64-bit words", "[bitfield]") {
  /* regression: the old parallel bit-count finished each word with
   * `& 0x3f`, so a fully-set word (parallel count 64) contributed 0
   * instead of 64. std::popcount has no such truncation. Entry 240 of
   * tabs_2/symmetries.inc is such an all-ones word, and its count feeds
   * symmetries_2_c::countSymmetryIntersection() and from there the
   * assembler_1 symmetry-breaker choice. */
  bitfield_c<64> full;
  for (int i = 0; i < 64; i++)
    full.set(i);
  REQUIRE(full.countbits() == 64);

  bitfield_c<128> two;
  for (int i = 0; i < 65; i++)
    two.set(i);
  REQUIRE(two.countbits() == 65);

  bitfield_c<240> three;
  for (int i = 0; i < 128; i++)
    three.set(i);
  REQUIRE(three.countbits() == 128);

  bitfield_c<240> empty;
  REQUIRE(empty.countbits() == 0);
}
