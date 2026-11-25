/* Tests for btui's vocabularies: every word read back as its value, an
 * unknown word as the table's fallback, no word or value twice.
 */
#include <catch2/catch_test_macros.hpp>

#include "../src/uicore/vocabulary.h"

#include <set>
#include <string>

using namespace btui;

namespace {
  template <class V> void roundTrips(const V & v) {
    std::set<std::string_view> words;
    for (const auto & t : v.terms) {
      INFO(std::string(t.word));
      CHECK(v.find(t.word) == t.value);
      CHECK(v.word(t.value) == t.word);
      CHECK(words.insert(t.word).second);
      CHECK_FALSE(t.word.empty());
    }
    CHECK(v.parse("no such word") == v.fallback);
    CHECK(v.parse("") == v.fallback);
    CHECK_FALSE(v.find("no such word").has_value());
  }
}

TEST_CASE("every vocabulary reads its words back and falls back on others", "[ui][vocabulary]") {
  roundTrips(kThemes);
  roundTrips(kDensities);
  roundTrips(kRotationMethods);
  roundTrips(kVoxelStyles);
  roundTrips(kAntialiasing);
  roundTrips(kNavModes);
  roundTrips(kProjections);
  roundTrips(kColourViews);
  roundTrips(kImageModes);
  roundTrips(kPapers);
}

TEST_CASE("the settings file's words stay those the earlier versions wrote", "[ui][vocabulary]") {
  CHECK(kThemes.word(ThemeChoice::Light) == "light");
  CHECK(kDensities.word(Density::Minimal) == "minimal");
  CHECK(kRotationMethods.word(Camera::RotationMethod::Arcball) == "arcball");
  CHECK(kVoxelStyles.word(VoxelStyle::Legacy) == "legacy");
  CHECK(kAntialiasing.word(Antialiasing::X8) == "8x");
  CHECK(kProjections.word(Camera::Projection::Orthographic) == "orthographic");
  CHECK(kColourViews.word(ColorMode::Voxel) == "voxel");
  CHECK(kPapers.word(Paper::LetterLandscape) == "letterl");
}

TEST_CASE("the fallbacks are the defaults the settings had", "[ui][vocabulary]") {
  CHECK(kThemes.fallback == ThemeChoice::System);
  CHECK(kDensities.fallback == Density::Standard);
  CHECK(kVoxelStyles.fallback == VoxelStyle::Flat);
  CHECK(kAntialiasing.fallback == Antialiasing::X4);
  CHECK(samples(Antialiasing::Off) == 1);
  CHECK(samples(Antialiasing::X8) == 8);
  CHECK(kImageModes.fallback == ImageMode::Solution);
}
