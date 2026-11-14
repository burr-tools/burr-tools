/* Tests for btui::ShapeColours: what a shape is shown in. Classic is the
 * legacy table, untouched; Muted keeps each hue and must hold the contrast
 * the shapes need against the panels, in both themes. */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "../src/uicore/shapecolours.h"
#include "../src/uicore/vocabulary.h"

#include <cmath>

using namespace btui;
using Catch::Matchers::WithinAbs;

namespace {

  const Rgb darkPanel{ 0x1a / 255.0f, 0x1d / 255.0f, 0x24 / 255.0f };
  const Rgb lightPanel{ 1, 1, 1 };
  constexpr int tableSize = 18;

  float textContrast(const Rgb & c) {
    return std::max(contrastRatio(c, { 1, 1, 1 }), contrastRatio(c, { 0, 0, 0 }));
  }

  /* the angle of a colour's hue on the sRGB colour wheel */
  float hue(const Rgb & c) {
    return std::atan2(std::sqrt(3.0f) * (c.g - c.b), 2 * c.r - c.g - c.b);
  }

  float spread(const Rgb & c) {
    return std::max({ c.r, c.g, c.b }) - std::min({ c.r, c.g, c.b });
  }
}

TEST_CASE("Classic is the legacy table to the bit", "[ui][shapecolours]") {
  const ShapeColours classic{ ShapePalette::Classic, true };
  for (int s = 0; s < 40; s++) {
    CHECK(classic.of(s).r == pieceColor(s).r);
    CHECK(classic.of(s).g == pieceColor(s).g);
    CHECK(classic.of(s).b == pieceColor(s).b);
    CHECK(classic.of(s, 3).r == pieceColor(s, 3).r);
    CHECK(classic.whiteText(s) == prefersWhiteText(s));
  }
}

TEST_CASE("the default is the table: an export must not follow the screen", "[ui][shapecolours]") {
  const ShapeColours none;
  CHECK(none.palette == ShapePalette::Classic);
}

TEST_CASE("Muted takes the edge off pure red", "[ui][shapecolours]") {
  const Rgb onDark = ShapeColours{ ShapePalette::Muted, true }.of(2);
  CHECK_THAT(onDark.r, WithinAbs(0xe5 / 255.0, 0.01));
  CHECK_THAT(onDark.g, WithinAbs(0x7f / 255.0, 0.01));
  CHECK_THAT(onDark.b, WithinAbs(0x70 / 255.0, 0.01));
}

TEST_CASE("every shape reaches 3:1 against its panel in Muted, in both themes", "[ui][shapecolours]") {
  for (int s = 0; s < tableSize; s++) {
    INFO("S" << s + 1);
    CHECK(contrastRatio(ShapeColours{ ShapePalette::Muted, true }.of(s), darkPanel) >= 3);
    CHECK(contrastRatio(ShapeColours{ ShapePalette::Muted, false }.of(s), lightPanel) >= 3);
  }
}

TEST_CASE("text on a Muted shape keeps 4.5:1", "[ui][shapecolours]") {
  for (int s = 0; s < tableSize; s++)
    for (const bool dark : { true, false }) {
      INFO("S" << s + 1 << (dark ? " dark" : " light"));
      CHECK(textContrast(ShapeColours{ ShapePalette::Muted, dark }.of(s)) >= 4.5f);
    }
}

TEST_CASE("Muted keeps each hue and takes saturation away", "[ui][shapecolours]") {
  for (int s = 0; s < tableSize; s++)
    for (const bool dark : { true, false }) {
      INFO("S" << s + 1 << (dark ? " dark" : " light"));
      const Rgb classic = pieceColor(s);
      const Rgb muted = ShapeColours{ ShapePalette::Muted, dark }.of(s);
      const float turn = std::fabs(std::remainder(hue(classic) - hue(muted), 2 * 3.14159265f));
      CHECK(turn < 0.45f);
      CHECK(spread(muted) < spread(classic));
    }
}

TEST_CASE("the light and dark tiers of a hue stay apart in Muted", "[ui][shapecolours]") {
  // blue and dark blue, red and dark red: the table's own pairs
  for (const auto & [bright, deep] : { std::pair{ 0, 6 }, std::pair{ 2, 8 }, std::pair{ 1, 7 } })
    for (const bool dark : { true, false }) {
      const ShapeColours c{ ShapePalette::Muted, dark };
      CHECK(relativeLuminance(c.of(bright)) > relativeLuminance(c.of(deep)));
    }
}

TEST_CASE("Muted gives a colour for any shape and copy", "[ui][shapecolours]") {
  // past the table the colours come from a formula, the copies are jittered
  for (const bool dark : { true, false })
    for (int s = 0; s < 200; s += 7)
      for (int copy = 0; copy < 5; copy++) {
        const Rgb c = ShapeColours{ ShapePalette::Muted, dark }.of(s, copy);
        for (const float v : { c.r, c.g, c.b }) {
          CHECK(v >= 0);
          CHECK(v <= 1);
        }
      }
}

TEST_CASE("text follows the colour on show, not the table's", "[ui][shapecolours]") {
  // blue takes white text as a pure colour; muted on the dark panel it is light enough for black
  const ShapeColours muted{ ShapePalette::Muted, true };
  CHECK(prefersWhiteText(0));
  CHECK(muted.whiteText(0) == prefersWhiteText(muted.of(0)));
  CHECK_FALSE(muted.whiteText(0));
}

TEST_CASE("the setting's words", "[ui][shapecolours]") {
  CHECK(kShapePalettes.word(ShapePalette::Muted) == "muted");
  CHECK(kShapePalettes.word(ShapePalette::Classic) == "classic");
  CHECK(kShapePalettes.parse("classic") == ShapePalette::Classic);
}

TEST_CASE("a muted colour stays inside sRGB even from the most saturated ones", "[ui][shapecolours]") {
  for (const Rgb & c : { Rgb{ 0, 0, 1 }, Rgb{ 1, 0, 1 }, Rgb{ 0, 1, 0 }, Rgb{ 1, 0, 0 }, Rgb{ 0, 1, 1 } })
    for (bool dark : { false, true }) {
      const Rgb m = mutedColour(c, dark);
      for (float x : { m.r, m.g, m.b }) {
        CHECK(x >= -0.001f);
        CHECK(x <= 1.001f);
      }
    }
}
