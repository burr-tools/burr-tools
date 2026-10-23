/* Tests for btui::pieceColor, the copy of the legacy piece colours. The
 * values are the legacy table's (src/gui/piececolor.cpp): both GUIs must
 * colour a shape the same way.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "../src/uicore/palette.h"

#include <algorithm>
#include <cmath>

using namespace btui;
using Catch::Matchers::WithinAbs;

namespace {
  void checkRgb(Rgb c, float r, float g, float b) {
    CHECK_THAT(c.r, WithinAbs(r, 1e-6));
    CHECK_THAT(c.g, WithinAbs(g, 1e-6));
    CHECK_THAT(c.b, WithinAbs(b, 1e-6));
  }
}

TEST_CASE("the first shapes have the legacy colours", "[ui][palette]") {
  checkRgb(pieceColor(0), 0, 0, 1);   // S1 blue
  checkRgb(pieceColor(1), 0, 1, 0);   // S2 green
  checkRgb(pieceColor(2), 1, 0, 0);   // S3 red
  checkRgb(pieceColor(3), 0, 1, 1);   // S4 cyan
  checkRgb(pieceColor(17), 1, 0, 0.6f);
}

TEST_CASE("beyond the table the colours follow the legacy sine sequence", "[ui][palette]") {
  for (int x : { 18, 25, 100 }) {
    INFO("shape " << x);
    checkRgb(pieceColor(x), float((1 + std::sin(0.7 * x)) / 2), float((1 + std::sin(1.3 * x + 1.5)) / 2),
             float((1 + std::sin(3.5 * x + 2.3)) / 2));
  }
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

TEST_CASE("chip text is whichever of white and black contrasts more, always at least 4.5:1", "[ui][palette]") {
  CHECK(prefersWhiteText(0));        // blue
  CHECK_FALSE(prefersWhiteText(1));  // green
  CHECK_FALSE(prefersWhiteText(3));  // cyan
  CHECK_FALSE(prefersWhiteText(2));  // red: black 5.3:1 (legacy's white was 4.0)
  const Rgb white{ 1, 1, 1 }, black{ 0, 0, 0 };
  for (int shape = 0; shape < 120; shape++) {
    INFO("shape " << shape);
    const Rgb c = pieceColor(shape);
    const float onWhite = contrastRatio(c, white), onBlack = contrastRatio(c, black);
    CHECK(prefersWhiteText(shape) == (onWhite > onBlack));
    CHECK(std::max(onWhite, onBlack) >= 4.5f);
  }
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
