/* The desktop's arrow on Linux (PR-20, OQ-17): Xcursor files and themes
 * as the desktop keeps them, made here byte by byte. */
#include <catch2/catch_test_macros.hpp>

#include "../src/uicore/xcursor.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <string>

using namespace btui;

namespace {

  void put(std::string & b, uint32_t v) {
    for (int k = 0; k < 4; k++)
      b.push_back(char((v >> (8 * k)) & 0xff));
  }

  /* an Xcursor file with one square image per nominal size, each pixel
   * its size in the blue channel, hotspot at (size / 8, size / 4) */
  std::string cursorFile(const std::vector<uint32_t> & sizes) {
    std::string b;
    put(b, 0x72756358);
    put(b, 16);
    put(b, 1);
    put(b, uint32_t(sizes.size()));
    size_t at = 16 + 12 * sizes.size();
    for (uint32_t s : sizes) {
      put(b, 0xfffd0002);
      put(b, s);
      put(b, uint32_t(at));
      at += 36 + 4 * size_t(s) * s;
    }
    for (uint32_t s : sizes) {
      put(b, 36);
      put(b, 0xfffd0002);
      put(b, s);
      put(b, 1);
      put(b, s);
      put(b, s);
      put(b, s / 8);
      put(b, s / 4);
      put(b, 0);
      for (size_t k = 0; k < size_t(s) * s; k++)
        put(b, 0xff000000u | s);
    }
    return b;
  }

  struct TempDir {
    std::filesystem::path path;
    TempDir() {
      std::mt19937 rng(std::random_device{}());
      path = std::filesystem::temp_directory_path() / ("bt-xcursor-" + std::to_string(rng()));
      std::filesystem::create_directories(path);
    }
    ~TempDir() { std::error_code ec; std::filesystem::remove_all(path, ec); }
    void write(const std::filesystem::path & rel, const std::string & text) const {
      std::filesystem::create_directories((path / rel).parent_path());
      std::ofstream(path / rel, std::ios::binary) << text;
    }
  };

  xcursor::Env envOf(const std::map<std::string, std::string> & vars) {
    return [vars](const char * name) -> const char * {
      auto it = vars.find(name);
      return it == vars.end() ? nullptr : it->second.c_str();
    };
  }
}

TEST_CASE("an Xcursor file gives the image nearest the size asked for", "[ui][xcursor]") {
  const std::string f = cursorFile({ 24, 32, 48 });
  auto img = xcursor::parse(f, 24);
  REQUIRE(img);
  CHECK(img->width == 24);
  CHECK(img->height == 24);
  CHECK(img->nominal == 24);
  CHECK(img->xhot == 3);
  CHECK(img->yhot == 6);
  REQUIRE(img->argb.size() == 24u * 24u);
  CHECK(img->argb.front() == 0xff000018u);
  CHECK(xcursor::parse(f, 36)->nominal == 32);
  CHECK(xcursor::parse(f, 40)->nominal == 48);            // a tie takes the larger
  CHECK(xcursor::parse(f, 96)->nominal == 48);
  CHECK(xcursor::parse(f, 1)->nominal == 24);
}

TEST_CASE("what is not an Xcursor file, or is cut short, gives nothing", "[ui][xcursor]") {
  const std::string f = cursorFile({ 32 });
  CHECK_FALSE(xcursor::parse("", 32));
  CHECK_FALSE(xcursor::parse("Xcur", 32));
  CHECK_FALSE(xcursor::parse(std::string("PNG\x0d\x0a") + f.substr(5), 32));
  for (size_t cut : { size_t(10), size_t(20), size_t(40), f.size() - 1 })
    CHECK_FALSE(xcursor::parse(f.substr(0, cut), 32));
  // an entry pointing past the end, a side too large, a hotspot outside
  std::string far = f;
  far[24] = char(0xff); far[25] = char(0xff);
  CHECK_FALSE(xcursor::parse(far, 32));
  std::string wide = f;
  wide[28 + 16 + 2] = char(0x80);                         // width 0x800020
  CHECK_FALSE(xcursor::parse(wide, 32));
  std::string hot = f;
  hot[28 + 24] = char(0x40);                              // xhot 64 > 32
  CHECK_FALSE(xcursor::parse(hot, 32));
}

TEST_CASE("the cursor theme and size come from the environment, then KDE's and GTK's settings", "[ui][xcursor]") {
  TempDir home;
  // nothing set: the default theme at 24
  auto s = xcursor::settings(envOf({ { "HOME", home.path.string() } }));
  CHECK(s.theme.empty());
  CHECK(s.size == 24);

  home.write(".config/gtk-3.0/settings.ini", "[Settings]\ngtk-theme-name=Adwaita\ngtk-cursor-theme-name = Bibata\ngtk-cursor-theme-size=32\n");
  s = xcursor::settings(envOf({ { "HOME", home.path.string() } }));
  CHECK(s.theme == "Bibata");
  CHECK(s.size == 32);

  home.write(".config/kcminputrc", "[Mouse]\ncursorTheme=breeze_cursors\ncursorSize=48\n");
  s = xcursor::settings(envOf({ { "HOME", home.path.string() } }));
  CHECK(s.theme == "breeze_cursors");
  CHECK(s.size == 48);

  s = xcursor::settings(envOf({ { "HOME", home.path.string() }, { "XCURSOR_THEME", "DMZ-White" }, { "XCURSOR_SIZE", "64" } }));
  CHECK(s.theme == "DMZ-White");
  CHECK(s.size == 64);
  // a size that is no number is passed over
  s = xcursor::settings(envOf({ { "HOME", home.path.string() }, { "XCURSOR_SIZE", "big" } }));
  CHECK(s.size == 48);
}

TEST_CASE("a cursor is found in its theme or in a theme it inherits", "[ui][xcursor]") {
  TempDir root;
  // XCURSOR_PATH's folders, ':' apart, '~' the home folder
  const auto listed = xcursor::searchPath(envOf({ { "HOME", "/home/u" }, { "XCURSOR_PATH", "/x/a::~/b" } }));
  REQUIRE(listed.size() == 2);
  CHECK(listed[0] == std::filesystem::path("/x/a"));
  CHECK(listed[1] == std::filesystem::path("/home/u/b"));
  const std::vector<std::filesystem::path> dirs{ root.path / "a", root.path / "b" };
  root.write("b/Plain/cursors/left_ptr", cursorFile({ 24 }));
  root.write("a/Fancy/index.theme", "[Icon Theme]\nName=Fancy\nInherits=Missing;Plain\n");
  root.write("a/default/index.theme", "[Icon Theme]\nInherits=Fancy\n");
  root.write("a/Loop/index.theme", "[Icon Theme]\nInherits=Loop\n");

  CHECK(xcursor::find("Plain", { "default", "left_ptr" }, dirs) == root.path / "b/Plain/cursors/left_ptr");
  CHECK(xcursor::find("Fancy", { "default", "left_ptr" }, dirs) == root.path / "b/Plain/cursors/left_ptr");
  CHECK(xcursor::find("", { "default", "left_ptr" }, dirs) == root.path / "b/Plain/cursors/left_ptr");
  CHECK(xcursor::find("Loop", { "left_ptr" }, dirs).empty());
  CHECK(xcursor::find("../b/Plain", { "left_ptr" }, dirs).empty());

  // the default list: the user's folders, then the system's
  const auto def = xcursor::searchPath(envOf({ { "HOME", "/home/u" } }));
  REQUIRE(def.size() == 4);
  CHECK(def[0] == std::filesystem::path("/home/u/.local/share/icons"));
  CHECK(def[1] == std::filesystem::path("/home/u/.icons"));
  CHECK(def[2] == std::filesystem::path("/usr/share/icons"));
}
