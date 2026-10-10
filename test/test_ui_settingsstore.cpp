/* Tests for btui::SettingsStore, the redesigned GUI's settings file. */
#include <catch2/catch_test_macros.hpp>

#include "../src/uicore/settingsstore.h"

#include <filesystem>
#include <fstream>
#include <random>
#include <string>

using btui::SettingsStore;

namespace {

  /* A file name in the temp directory that no other test run uses; removed
   * when the guard goes out of scope.
   */
  struct TempFile {
    std::filesystem::path path;
    TempFile() {
      std::random_device rd;
      path = std::filesystem::temp_directory_path() /
             ("bt_settings_" + std::to_string(rd()) + "_" + std::to_string(rd()) + ".rc");
    }
    ~TempFile() { std::error_code ec; std::filesystem::remove(path, ec); }
  };

  void write(const std::filesystem::path & p, const std::string & text) {
    std::ofstream out(p);
    out << text;
  }
}

TEST_CASE("values round-trip through the file with their types", "[ui][settings]") {
  TempFile f;
  SettingsStore s(f.path);
  s.set("view.lighting", false);
  s.set("ui.undoDepth", 100LL);
  s.set("ui.theme", std::string("dark"));
  s.set("odd", std::string("quote \" and back\\slash"));
  REQUIRE(s.save());

  SettingsStore t(f.path);
  REQUIRE(t.load());
  CHECK(t.getBool("view.lighting") == false);
  CHECK(t.getInt("ui.undoDepth") == 100);
  CHECK(t.getString("ui.theme") == "dark");
  CHECK(t.getString("odd") == "quote \" and back\\slash");
  CHECK(t.values().size() == 4);
}

TEST_CASE("saving replaces the file whole, through a temporary beside it", "[ui][settings]") {
  // written beside the file and renamed over it: a crash part way never
  // leaves half a settings file, and nothing is left behind
  TempFile f;
  std::filesystem::path tmp = f.path;
  tmp += ".tmp";
  SettingsStore s(f.path);
  s.set("ui.theme", std::string("dark"));
  s.set("view.lighting", true);
  REQUIRE(s.save());
  s.remove("view.lighting");
  s.set("ui.theme", std::string("light"));
  REQUIRE(s.save());                               // over an existing file
  CHECK_FALSE(std::filesystem::exists(tmp));

  SettingsStore t(f.path);
  REQUIRE(t.load());
  CHECK(t.getString("ui.theme") == "light");
  CHECK_FALSE(t.contains("view.lighting"));
  CHECK(t.values().size() == 1);

  // a place it cannot write: it says so, and the old file stays
  SettingsStore u(f.path.parent_path() / "no such folder" / "s.rc");
  u.set("ui.theme", std::string("dark"));
  CHECK_FALSE(u.save());
  SettingsStore v(f.path);
  REQUIRE(v.load());
  CHECK(v.getString("ui.theme") == "light");
}

TEST_CASE("the legacy .burrtools.rc format parses", "[ui][settings]") {
  TempFile f;
  // what configuration_c's destructor writes
  write(f.path,
        "windowposh = 600\n"
        "renderstyle = 0\n"
        "tooltips = true\n"
        "lightning = false\n"
        "numthreads = 12\n"
        "somestring = \"C:\\Users\\me\"\n");
  auto v = SettingsStore::parseFile(f.path);
  CHECK(v.size() == 6);
  CHECK(std::get<bool>(v.at("tooltips")) == true);
  CHECK(std::get<bool>(v.at("lightning")) == false);
  CHECK(std::get<long long>(v.at("numthreads")) == 12);
  // legacy never escapes, so a lone backslash is literal
  CHECK(std::get<std::string>(v.at("somestring")) == "C:\\Users\\me");
}

TEST_CASE("unparsable lines are skipped, not fatal", "[ui][settings]") {
  TempFile f;
  write(f.path,
        "good = 3\n"
        "no equals sign here\n"
        "= 5\n"
        "bad = not_a_value\n"
        "unterminated = \"abc\n"
        "-- a lua comment\n"
        "\n"
        "alsogood = true\n");
  auto v = SettingsStore::parseFile(f.path);
  CHECK(v.size() == 2);
  CHECK(v.count("good") == 1);
  CHECK(v.count("alsogood") == 1);
}

TEST_CASE("a missing file loads as empty and a wrong type reads as absent", "[ui][settings]") {
  TempFile f;
  SettingsStore s(f.path);
  CHECK_FALSE(s.load());
  CHECK(s.values().empty());

  s.set("n", 5LL);
  CHECK_FALSE(s.getBool("n").has_value());
  CHECK(s.getBool("n", true) == true);
  CHECK(s.getString("missing", "fallback") == "fallback");

  s.remove("n");
  CHECK_FALSE(s.contains("n"));
}
