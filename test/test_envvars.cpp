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

  enum class Lang { Cpp, Python, Shell };

  /* The BURRTOOLS_ variables `text` reads: a name counts only where it is
   * handed to something that reads the environment -- not in a comment, a
   * test's ScopedEnv, or an environment a script sets up for a child.
   * C and C++ read through these functions, the name given as a literal --
   * getenv, Qt's qgetenv and qEnvironmentVariable*, test/qtgui's envDir
   * (snapshots.cpp) and the rotation settings' fromEnv; a variable read some other way needs its reader
   * added here. */
  const char * const cppReaders = "getenv|qgetenv|qEnvironmentVariable(?:IsSet|IsEmpty|IntValue)?|envDir|fromEnv";

  std::set<std::string> readsIn(const std::string & text, Lang lang) {
    static const std::regex cpp(std::string(R"(\b(?:)") + cppReaders + R"()\s*\(\s*"(BURRTOOLS_[A-Z0-9_]+)\")");
    // os.environ.get('X'), os.getenv('X'), os.environ['X'] (but not os.environ['X'] = ...)
    static const std::regex python(R"((?:os\.environ\.get|os\.getenv)\s*\(\s*['"](BURRTOOLS_[A-Z0-9_]+)['"]|os\.environ\[\s*['"](BURRTOOLS_[A-Z0-9_]+)['"]\s*\](?!\s*=[^=]))");
    // $X, ${X}, ${X:-default}
    static const std::regex shell(R"(\$\{?(BURRTOOLS_[A-Z0-9_]+))");
    const std::regex & re = lang == Lang::Cpp ? cpp : lang == Lang::Python ? python : shell;
    std::set<std::string> out;
    for (auto it = std::sregex_iterator(text.begin(), text.end(), re); it != std::sregex_iterator(); ++it)
      for (std::size_t g = 1; g < it->size(); g++)
        if ((*it)[g].matched)
          out.insert((*it)[g].str());
    return out;
  }

  std::set<std::string> readsInSources(void) {
    namespace fs = std::filesystem;
    std::set<std::string> out;
    for (const char * dir : { "src", "test", "scripts", "bench" }) {
      INFO(dir << ": not found -- the test runs from the project root, as meson runs it");
      REQUIRE(fs::is_directory(dir));
      for (const auto & e : fs::recursive_directory_iterator(dir)) {
        if (!e.is_regular_file())
          continue;
        const std::string ext = e.path().extension().string();
        Lang lang;
        if (ext == ".c" || ext == ".cc" || ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".mm")
          lang = Lang::Cpp;
        else if (ext == ".py")
          lang = Lang::Python;
        else if (ext == ".sh")
          lang = Lang::Shell;
        else
          continue;
        // the table itself, and this test, name every variable
        const std::string file = e.path().filename().string();
        if (file == "envvars.h" || file == "test_envvars.cpp")
          continue;
        std::ifstream in(e.path(), std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        for (const std::string & n : readsIn(ss.str(), lang))
          out.insert(n);
      }
    }
    return out;
  }
}

TEST_CASE("only a read counts: not a comment, a test's setup, or a child's environment", "[envvars]") {
  const std::string cpp =
    "// BURRTOOLS_A in a comment\n"
    "ScopedEnv env(\"BURRTOOLS_B\", \"1\");\n"
    "const char * v = std::getenv(\"BURRTOOLS_C\");\n"
    "if (getenv( \"BURRTOOLS_D\" ) != nullptr) {}\n"
    "if (qEnvironmentVariableIsSet(\"BURRTOOLS_E\")) {}\n"
    "qputenv(\"BURRTOOLS_F\", \"1\");\n";
  CHECK(readsIn(cpp, Lang::Cpp) == std::set<std::string>{ "BURRTOOLS_C", "BURRTOOLS_D", "BURRTOOLS_E" });

  const std::string python =
    "env = {'BURRTOOLS_A': '1'}\n"
    "os.environ['BURRTOOLS_B'] = '1'\n"
    "x = os.environ.get('BURRTOOLS_C')\n"
    "y = os.getenv(\"BURRTOOLS_D\", '')\n"
    "z = os.environ['BURRTOOLS_E']\n";
  CHECK(readsIn(python, Lang::Python) == std::set<std::string>{ "BURRTOOLS_C", "BURRTOOLS_D", "BURRTOOLS_E" });

  const std::string shell =
    "# BURRTOOLS_A in a comment\n"
    "BURRTOOLS_B=1 ./burrTxt x\n"
    "P=\"${BURRTOOLS_C:-x}\"\n"
    "echo $BURRTOOLS_D\n";
  CHECK(readsIn(shell, Lang::Shell) == std::set<std::string>{ "BURRTOOLS_C", "BURRTOOLS_D" });
}

TEST_CASE("every BURRTOOLS_ environment variable read is in the --help table, and only those", "[envvars]") {
  REQUIRE(std::filesystem::exists("src/tools/envvars.h"));
  const std::set<std::string> read = readsInSources();
  REQUIRE(read.size() > 10);

  std::set<std::string> listed;
  for (const btenv::Variable & v : btenv::all()) {
    INFO(v.name);
    CHECK(listed.insert(std::string(v.name)).second);      // once each
    CHECK_FALSE(v.effect.empty());
    CHECK(read.count(std::string(v.name)) == 1);            // something reads it
  }
  for (const std::string & r : read) {
    INFO(r << " is read but not listed in src/tools/envvars.h");
    CHECK(listed.count(r) == 1);
  }
}

TEST_CASE("the help prints each scope's variables under its heading", "[envvars]") {
  std::ostringstream out;
  btenv::print(out, { btenv::Scope::Solver });
  const std::string s = out.str();
  CHECK(s.find("Environment variables:") != std::string::npos);
  CHECK(s.find("BURRTOOLS_THREADS=<n>") != std::string::npos);
  CHECK(s.find("BURRTOOLS_UPDATE_VERSION_OVERRIDE") == std::string::npos);     // another scope's
}

TEST_CASE("a switch is shown by its name, any value turning it on", "[envvars]") {
  // the code asks whether a switch is set, not what it says: =1 would
  // suggest that =0 turns it off
  std::ostringstream out;
  btenv::print(out, { btenv::Scope::Solver });
  const std::string s = out.str();
  CHECK(s.find("    BURRTOOLS_NO_SIMD\n") != std::string::npos);
  CHECK(s.find("BURRTOOLS_NO_SIMD=") == std::string::npos);
  CHECK(s.find("set to any value (even 0)") != std::string::npos);
}

TEST_CASE("wrapped text counts a run of spaces as one", "[envvars]") {
  std::ostringstream out;
  btenv::printWrapped(out, "one  two   three", 4, 12);
  CHECK(out.str() == "    one two\n    three\n");
}

TEST_CASE("the help fits in 80 columns", "[envvars]") {
  std::ostringstream out;
  btenv::print(out, { btenv::Scope::Solver, btenv::Scope::Gui, btenv::Scope::QtGui, btenv::Scope::Tests });
  std::istringstream in(out.str());
  std::string line;
  while (std::getline(in, line)) {
    INFO(line);
    CHECK(line.size() < 80);
  }
}
