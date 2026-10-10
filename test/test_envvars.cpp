/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/* The --help lists of environment variables (tools/envvars.h) against what
 * the code and the scripts actually read. Run from the project root, as the
 * suite is. */

#include "../src/tools/envvars.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>

namespace {

  // BURRTOOLS_ names that are not environment variables
  const std::set<std::string> notVariables = {
    "BURRTOOLS_VERSION",      // the version macro
    "BURRTOOLS_DXBC",         // a compile definition of the Qt tests
    "BURRTOOLS_QT_EXE",       // a placeholder in qtgui_smoke.py's usage
    "BURRTOOLS_NO_",          // a prefix in a comment
  };

  std::set<std::string> namesInSources(void) {
    namespace fs = std::filesystem;
    const std::set<std::string> exts = { ".cpp", ".h", ".hpp", ".mm", ".qml", ".py", ".sh" };
    const std::regex name("BURRTOOLS_[A-Z0-9_]+");
    std::set<std::string> out;
    for (const char * dir : { "src", "test", "scripts" })
      for (const auto & e : fs::recursive_directory_iterator(dir)) {
        if (!e.is_regular_file() || !exts.count(e.path().extension().string()))
          continue;
        // the table itself, and this test, name every variable
        const std::string file = e.path().filename().string();
        if (file == "envvars.h" || file == "test_envvars.cpp")
          continue;
        std::ifstream in(e.path(), std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        const std::string text = ss.str();
        for (auto it = std::sregex_iterator(text.begin(), text.end(), name); it != std::sregex_iterator(); ++it)
          if (!notVariables.count(it->str()))
            out.insert(it->str());
      }
    return out;
  }
}

TEST_CASE("every BURRTOOLS_ environment variable is in the --help table, and only those", "[envvars]") {
  REQUIRE(std::filesystem::exists("src/tools/envvars.h"));
  const std::set<std::string> used = namesInSources();
  REQUIRE(used.size() > 15);

  std::set<std::string> listed;
  for (const btenv::Variable & v : btenv::all()) {
    INFO(v.name);
    CHECK(listed.insert(std::string(v.name)).second);      // once each
    CHECK_FALSE(v.value.empty());
    CHECK_FALSE(v.effect.empty());
    CHECK(used.count(std::string(v.name)) == 1);            // something reads it
  }
  for (const std::string & u : used) {
    INFO(u << " is read but not listed in src/tools/envvars.h");
    CHECK(listed.count(u) == 1);
  }
}

TEST_CASE("the help prints each scope's variables under its heading", "[envvars]") {
  std::ostringstream out;
  btenv::print(out, { btenv::Scope::Solver });
  const std::string s = out.str();
  CHECK(s.find("Environment variables:") != std::string::npos);
  CHECK(s.find("BURRTOOLS_THREADS=<n>") != std::string::npos);
  CHECK(s.find("BURRTOOLS_RHI") == std::string::npos);     // another scope's
}

TEST_CASE("the help fits in 80 columns", "[envvars]") {
  std::ostringstream out;
  btenv::print(out, { btenv::Scope::Solver, btenv::Scope::QtGui, btenv::Scope::LegacyGui, btenv::Scope::Tests });
  std::istringstream in(out.str());
  std::string line;
  while (std::getline(in, line)) {
    INFO(line);
    CHECK(line.size() < 80);
  }
}
