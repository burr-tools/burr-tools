/* Tests for the redesigned GUI's command table (src/uicore/commands).
 *
 * The drift check at the bottom is the new GUI's counterpart of
 * mainmenu::assertTablesConsistent(): every callback in the legacy FLTK menu
 * tables must have a command here, or be on the deliberate drop list. It reads
 * src/gui/mainmenu.cpp as text, so it needs neither FLTK nor Qt; the test runs
 * from the project root (meson sets workdir), where that path resolves.
 */
#include <catch2/catch_test_macros.hpp>

#include "../src/uicore/commands.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>

using namespace btui;

TEST_CASE("command keys are unique and every command is findable by key", "[ui][commands]") {
  std::set<std::string_view> keys;
  for (const auto & c : commandTable()) {
    INFO(c.key);
    CHECK(keys.insert(c.key).second);
    REQUIRE(findCommand(c.key) == &c);
    CHECK(&commandInfo(c.id) == &c);
    CHECK_FALSE(c.label.empty());
  }
  CHECK(findCommand("no.such.command") == nullptr);
}

TEST_CASE("no two commands share a shortcut on the same platform", "[ui][commands]") {
  for (bool mac : { false, true }) {
    std::set<std::string_view> seen;
    for (const auto & c : commandTable()) {
      auto list = (mac && !c.macShortcuts.empty()) ? c.macShortcuts : c.shortcuts;
      for (auto s : list) {
        INFO((mac ? "mac " : "portable ") << s << " on " << c.key);
        CHECK(seen.insert(s).second);
      }
    }
  }
}

TEST_CASE("the legacy shortcuts survive and Toggle 3D's F4 is gone", "[ui][commands]") {
  auto has = [](Command c, std::string_view s) {
    auto l = commandInfo(c).shortcuts;
    return std::find(l.begin(), l.end(), s) != l.end();
  };
  CHECK(has(Command::Save, "F2"));
  CHECK(has(Command::Open, "F3"));
  CHECK(has(Command::Undo, "Ctrl+Z"));
  CHECK(has(Command::Redo, "Ctrl+Shift+Z"));
  CHECK(has(Command::Redo, "Ctrl+Y"));

  for (const auto & c : commandTable())
    for (auto s : c.shortcuts)
      CHECK(s != "F4");
}

TEST_CASE("full screen and Focus 3D have separate keys, full screen per platform", "[ui][commands]") {
  const auto & fs = commandInfo(Command::FullScreen);
  const auto & f3 = commandInfo(Command::Focus3d);
  REQUIRE(fs.shortcuts.size() == 1);
  CHECK(fs.shortcuts[0] == "F11");
  REQUIRE(fs.macShortcuts.size() == 1);
  CHECK(fs.macShortcuts[0] == "Ctrl+Meta+F");
  REQUIRE(f3.shortcuts.size() == 1);
  CHECK(f3.shortcuts[0] != fs.shortcuts[0]);
}

TEST_CASE("the menu bar holds only menus, every legacy command in one", "[ui][commands]") {
  std::vector<std::string_view> keys;
  for (const auto & m : menuBar())
    keys.push_back(m.key);
  CHECK(keys == std::vector<std::string_view>{ "file", "edit", "view", "export", "help" });

  for (const auto & m : menuBar()) {
    INFO(m.key);
    CHECK(std::any_of(commandTable().begin(), commandTable().end(),
                      [&](const CommandInfo & c) { return c.menu == m.menu; }));
  }

  // legacy's direct menu-bar buttons became items; Settings (legacy Config) is in File
  CHECK(commandInfo(Command::Status).menu == Menu::View);
  CHECK(commandInfo(Command::EditComment).menu == Menu::Edit);
  CHECK(commandInfo(Command::About).menu == Menu::Help);
  CHECK(commandInfo(Command::Settings).menu == Menu::File);

  // macOS moves Settings, About and Quit into the application menu by title
  CHECK(plainLabel(commandInfo(Command::Settings).label).rfind("Settings", 0) == 0);
  CHECK(plainLabel(commandInfo(Command::About).label).rfind("About", 0) == 0);
  CHECK(plainLabel(commandInfo(Command::Quit).label) == "Quit");
  CHECK(plainLabel(commandInfo(Command::Quit).winLabel) == "Exit");
}

TEST_CASE("access keys are unique within each menu", "[ui][commands]") {
  auto accessKey = [](std::string_view label) {
    const size_t i = label.find('&');
    return i == std::string_view::npos || i + 1 >= label.size() ? '\0' : char(std::tolower(label[i + 1]));
  };
  std::set<char> bar;
  for (const auto & m : menuBar()) {
    INFO(m.key);
    CHECK(bar.insert(accessKey(m.label)).second);
    std::set<char> items;
    for (const auto & c : commandTable()) {
      if (c.menu != m.menu)
        continue;
      INFO(c.key);
      const char k = accessKey(c.label);
      CHECK(k != '\0');
      CHECK(items.insert(k).second);
    }
  }
  CHECK(plainLabel("Save &as…") == "Save as…");
  CHECK(plainLabel("Fish && chips") == "Fish & chips");
}

namespace {

  std::vector<std::string_view> keysOf(Menu m, Platform p) {
    std::vector<std::string_view> out;
    for (const auto & e : menuEntries(m, p))
      out.push_back(e.command->key);
    return out;
  }

  const MenuEntry * entry(Menu m, Platform p, std::string_view key, std::vector<MenuEntry> & keep) {
    keep = menuEntries(m, p);
    for (const auto & e : keep)
      if (e.command->key == key)
        return &e;
    return nullptr;
  }
}

TEST_CASE("macOS gets legacy's Close, Status and Help keys in its own places", "[ui][commands]") {
  // File on macOS: New, Open, Import | Close, Save, Save as | ... | Status (Get Info's place)
  const auto macFile = keysOf(Menu::File, Platform::Mac);
  const auto at = [&](std::string_view k) { return std::find(macFile.begin(), macFile.end(), k) - macFile.begin(); };
  REQUIRE(at("file.close") < std::ptrdiff_t(macFile.size()));
  CHECK(at("file.import") + 1 == at("file.close"));
  CHECK(at("file.close") + 1 == at("file.save"));
  CHECK(at("status") == std::ptrdiff_t(macFile.size()) - 1);

  std::vector<MenuEntry> keep;
  CHECK(entry(Menu::File, Platform::Mac, "file.close", keep)->separatorBefore);
  CHECK_FALSE(entry(Menu::File, Platform::Mac, "file.save", keep)->separatorBefore);

  const auto & close = commandInfo(Command::Close);
  const auto & status = commandInfo(Command::Status);
  const auto & guide = commandInfo(Command::HelpGuide);
  CHECK(shortcutsOn(close, Platform::Mac)[0] == "Ctrl+W");      // Cmd+W
  CHECK(shortcutsOn(status, Platform::Mac)[0] == "Ctrl+I");     // Cmd+I
  CHECK(shortcutsOn(guide, Platform::Mac)[0] == "Ctrl+?");      // Cmd+?
  CHECK(labelOn(guide, Platform::Mac) == "BurrTools Help");
  CHECK(keysOf(Menu::Help, Platform::Mac).front() == "help.guide");   // first in Help

  // macOS has no View > Show menu bar, and Status is not in View there
  const auto macView = keysOf(Menu::View, Platform::Mac);
  CHECK(std::find(macView.begin(), macView.end(), "view.menuBar") == macView.end());
  CHECK(std::find(macView.begin(), macView.end(), "status") == macView.end());
}

TEST_CASE("Windows and Linux keep their menus without the macOS additions", "[ui][commands]") {
  for (Platform p : { Platform::Windows, Platform::Linux }) {
    const auto file = keysOf(Menu::File, p);
    CHECK(std::find(file.begin(), file.end(), "file.close") == file.end());
    CHECK(std::find(file.begin(), file.end(), "status") == file.end());
    const auto view = keysOf(Menu::View, p);
    CHECK(std::find(view.begin(), view.end(), "status") != view.end());
    CHECK(std::find(view.begin(), view.end(), "view.menuBar") != view.end());
    // Close's divider still separates Import from Save
    std::vector<MenuEntry> keep;
    CHECK(entry(Menu::File, p, "file.save", keep)->separatorBefore);
    CHECK(shortcutsOn(commandInfo(Command::Status), p).empty());
    CHECK(labelOn(commandInfo(Command::HelpGuide), p) == "&User guide");
  }
  CHECK(labelOn(commandInfo(Command::Quit), Platform::Windows) == "E&xit");
  CHECK(labelOn(commandInfo(Command::Quit), Platform::Linux) == "&Quit");
}

TEST_CASE("every menu starts without a divider and never shows two in a row", "[ui][commands]") {
  for (Platform p : { Platform::Windows, Platform::Mac, Platform::Linux })
    for (const auto & m : menuBar()) {
      const auto entries = menuEntries(m.menu, p);
      REQUIRE_FALSE(entries.empty());
      CHECK_FALSE(entries.front().separatorBefore);
    }
}

namespace {

  /* The cb_<name>_stub callbacks inside one named Fl_Menu_Item array. */
  std::set<std::string> legacyCallbacks(const std::string & src, const std::string & table) {
    std::set<std::string> out;
    size_t b = src.find("Fl_Menu_Item " + table + "[]");
    if (b == std::string::npos)
      return out;
    size_t e = src.find("};", b);
    std::string body = src.substr(b, e - b);
    std::regex re("cb_([A-Za-z0-9_]+?)_stub");
    for (auto it = std::sregex_iterator(body.begin(), body.end(), re); it != std::sregex_iterator(); ++it)
      out.insert((*it)[1].str());
    return out;
  }
}

TEST_CASE("every legacy menu callback has a command or is dropped on purpose", "[ui][commands]") {
  std::ifstream in("src/gui/mainmenu.cpp");
  REQUIRE(in);
  std::stringstream ss;
  ss << in.rdbuf();
  const std::string src = ss.str();

  std::set<std::string> mapped;
  for (const auto & c : commandTable())
    if (!c.legacyCallback.empty())
      mapped.insert(std::string(c.legacyCallback));
  for (auto d : droppedLegacyCallbacks())
    mapped.insert(std::string(d));

  for (const char * table : { "menu_Portable", "menu_Mac" }) {
    auto cbs = legacyCallbacks(src, table);
    INFO(table);
    REQUIRE_FALSE(cbs.empty());
    for (const auto & cb : cbs) {
      INFO("legacy callback cb_" << cb << "_stub");
      CHECK(mapped.count(cb) == 1);
    }
  }

  // and the legacyCallback names point at callbacks that really exist,
  // so a typo cannot satisfy the check above
  auto all = legacyCallbacks(src, "menu_Portable");
  auto mac = legacyCallbacks(src, "menu_Mac");
  all.insert(mac.begin(), mac.end());
  for (const auto & c : commandTable())
    if (!c.legacyCallback.empty()) {
      INFO(c.key);
      CHECK(all.count(std::string(c.legacyCallback)) == 1);
    }
}
