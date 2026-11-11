/* Tests for btui::pieceColor, the piece colours of both GUIs: the FLTK
 * GUI's src/gui/piececolor.cpp is a wrapper over it, so these tests cover
 * its colours too.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "../src/uicore/palette.h"

#include <algorithm>
#include <cmath>

using namespace btui;
using Catch::Matchers::WithinAbs;

namespace {
  /* the eight anchors: Okhsl at full saturation, hue and lightness as in the
   * syntax-highlighting sets the palette is built from, to the reference
   * conversion's rounding (a unit or two in 255) */
  void checkHex(int shape, unsigned int r, unsigned int g, unsigned int b) {
    const Rgb c = pieceColor(shape);
    INFO("shape " << shape);
    CHECK_THAT(c.r * 255, WithinAbs(r, 2.01));
    CHECK_THAT(c.g * 255, WithinAbs(g, 2.01));
    CHECK_THAT(c.b * 255, WithinAbs(b, 2.01));
  }
}

TEST_CASE("the first eight shapes are the anchors, in the legacy table's order", "[ui][palette]") {
  checkHex(0, 0x30, 0x64, 0xff);  // blue, 265 degrees
  checkHex(1, 0x00, 0x8e, 0x50);  // green, 155
  checkHex(2, 0xdf, 0x00, 0x25);  // red, 25
  checkHex(3, 0x00, 0x9d, 0xd8);  // cyan, 235
  checkHex(4, 0xa5, 0x92, 0x00);  // mustard, 100, where legacy had yellow
  checkHex(5, 0xe0, 0x00, 0xb4);  // magenta, 340
  checkHex(6, 0xa2, 0x01, 0xfd);  // purple, 305 (the set's #aa02f0 is a touch under full saturation)
  checkHex(7, 0xc6, 0x66, 0x00);  // brown, 55
}

TEST_CASE("the dark-background set is the same hues, lighter", "[ui][palette]") {
  struct Restore { ~Restore() { setPieceColorsForDarkBackground(false); } } restore;
  REQUIRE_FALSE(pieceColorsForDarkBackground());
  setPieceColorsForDarkBackground(true);
  REQUIRE(pieceColorsForDarkBackground());
  checkHex(0, 0x44, 0x78, 0xff);  // blue, 265 degrees at 0.55
  checkHex(1, 0x00, 0xbc, 0x6c);  // green, 155 at 0.65
  checkHex(2, 0xf6, 0x00, 0x2a);  // red, 25 at 0.55
  checkHex(3, 0x08, 0xb9, 0xff);  // cyan, 235 at 0.70
  checkHex(4, 0xc3, 0xac, 0x00);  // yellow, 100 at 0.70
  checkHex(5, 0xff, 0x3e, 0xd0);  // pink, 340 at 0.65 (the set says #ff41d0: on the gamut edge, three units apart)
  checkHex(6, 0xb3, 0x5e, 0xff);  // purple, 305 at 0.60
  checkHex(7, 0xed, 0x7b, 0x00);  // orange, 55 at 0.65
  // every dark-set colour is lighter than its light-set counterpart
  for (int shape = 0; shape < 32; shape++) {
    setPieceColorsForDarkBackground(true);
    const float dark = relativeLuminance(pieceColor(shape));
    setPieceColorsForDarkBackground(false);
    const float light = relativeLuminance(pieceColor(shape));
    INFO("shape " << shape);
    CHECK(dark > light);
  }
}

TEST_CASE("the anchors sit at one apparent brightness", "[ui][palette]") {
  // Okhsl lightness 0.5 to 0.6: the WCAG luminance of all eight lies in a
  // narrow band, where the legacy primaries spanned 0.07 (blue) to 0.93 (yellow)
  float lo = 1, hi = 0;
  for (int shape = 0; shape < 8; shape++) {
    const float l = relativeLuminance(pieceColor(shape));
    lo = std::min(lo, l);
    hi = std::max(hi, l);
  }
  CHECK(lo > 0.12f);
  CHECK(hi < 0.35f);
}

TEST_CASE("later shapes halve the gaps between the hues before them", "[ui][palette]") {
  // shapes 9 to 16 are halfway between neighbouring anchors, 17 to 32 a
  // quarter of the way: no colour repeats among the first 64 shapes
  for (int x = 0; x < 64; x++)
    for (int y = 0; y < x; y++) {
      const Rgb a = pieceColor(x), b = pieceColor(y);
      INFO("shapes " << x << " and " << y);
      CHECK((std::fabs(a.r - b.r) + std::fabs(a.g - b.g) + std::fabs(a.b - b.b)) > 0.02f);
    }
  // shape 9 lies between red (25) and brown (55) in hue: redder than brown, browner than red
  const Rgb red = pieceColor(2), brown = pieceColor(7), mid = pieceColor(8);
  CHECK(mid.g > red.g);
  CHECK(mid.g < brown.g);
}

TEST_CASE("copies of a shape get distinct colours inside [0, 1]", "[ui][palette]") {
  for (int shape : { 0, 4, 9 }) {
    Rgb a = pieceColor(shape, 1);
    Rgb b = pieceColor(shape, 2);
    INFO("shape " << shape);
    CHECK((a.r != b.r || a.g != b.g || a.b != b.b));
    for (Rgb c : { a, b })
      for (float v : { c.r, c.g, c.b }) {
        CHECK(v >= 0.0f);
        CHECK(v <= 1.0f);
      }
  }
}

TEST_CASE("chip text is white on the light-background colours and black on the dark-background ones", "[ui][palette]") {
  const Rgb white{ 1, 1, 1 }, black{ 0, 0, 0 };
  for (int shape = 0; shape < 120; shape++) {
    INFO("shape " << shape);
    CHECK(prefersWhiteText(shape));
    CHECK(contrastRatio(pieceColor(shape), white) >= 3.0f);
  }
  setPieceColorsForDarkBackground(true);
  for (int shape = 0; shape < 120; shape++) {
    INFO("shape " << shape << ", dark background");
    CHECK_FALSE(prefersWhiteText(shape));
    CHECK(contrastRatio(pieceColor(shape), black) >= 4.5f);
  }
  setPieceColorsForDarkBackground(false);
  CHECK_THAT(contrastRatio(white, black), Catch::Matchers::WithinAbs(21.0, 1e-3));
}

TEST_CASE("an index a view hands over while its rows go (-1) gives a colour too", "[ui][palette]") {
  // a QML delegate being torn down can ask for row -1: the table must not
  // be read outside its bounds (the sanitizers job caught it), and the
  // answer is still a colour, from the formula
  for (int shape : { -1, -2, -100 }) {
    const Rgb c = pieceColor(shape);
    for (float ch : { c.r, c.g, c.b }) {
      CHECK(ch >= 0.0f);
      CHECK(ch <= 1.0f);
    }
    CHECK_NOTHROW(prefersWhiteText(shape));
    const Rgb j = pieceColor(shape, 1);
    CHECK(std::isfinite(j.r));
  }
}

TEST_CASE("the 8-bit colours are the floats cut off, as the FLTK GUI reads them", "[ui][palette]") {
  for (int shape : { 0, 7, 20, 100 }) {
    const Rgb c = pieceColor(shape);
    const Rgb8 c8 = pieceColor8(shape);
    CHECK(c8.r == (unsigned int)(c.r * 255));
    CHECK(c8.g == (unsigned int)(c.g * 255));
    CHECK(c8.b == (unsigned int)(c.b * 255));
    const Rgb j = pieceColor(shape, 3);
    const Rgb8 j8 = pieceColor8(shape, 3);
    CHECK(j8.r == (unsigned int)(j.r * 255));
    CHECK(j8.g == (unsigned int)(j.g * 255));
    CHECK(j8.b == (unsigned int)(j.b * 255));
  }
  CHECK(darkShade(0.7f) == 0.629999995f);
  CHECK(lightShade(0.7f) == 0.730000019f);
}
