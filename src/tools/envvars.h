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
#ifndef BT_TOOLS_ENVVARS_H
#define BT_TOOLS_ENVVARS_H

#include <cstddef>
#include <initializer_list>
#include <ostream>
#include <span>
#include <string>
#include <string_view>

/* Every environment variable BurrTools reads, in one place. `--help` of
 * burrtools-qt, burrTxt and burrTxt2 prints the ones that concern it, and
 * test_envvars.cpp fails when the code reads a BURRTOOLS_ variable this
 * table does not list (or lists one nothing reads), so the help stays
 * complete.
 */
namespace btenv {

  enum class Scope {
    Solver,      ///< the solver library: every program, the GUIs and the CLIs
    Gui,         ///< both GUIs (burrtools, burrtools-qt)
    QtGui,       ///< burrtools-qt
    Tests,       ///< the test programs and scripts
  };

  struct Variable {
    std::string_view name;
    Scope scope = Scope::Solver;
    /* what to set it to; empty for a switch, which only has to be set:
     * the code asks whether it is there, not what it says, so even 0 counts */
    std::string_view value;
    std::string_view effect;
  };

  inline constexpr Variable variables[] = {
    // the solver, read by every program that solves
    { "BURRTOOLS_THREADS", Scope::Solver, "<n>", "worker threads (default: the number of cores), read by the assembler and the disassembler pool each, so up to 2n run in all; a program's own setting (-t, Settings) wins" },
    { "BURRTOOLS_NO_SIMD", Scope::Solver, "", "no bit-parallel or vector solver paths: the plain scalar code (benchmarking)" },
    { "BURRTOOLS_NO_VECTOR", Scope::Solver, "", "no vector (AVX2/AVX-512/NEON) kernels; the bit-parallel paths stay (benchmarking)" },
    { "BURRTOOLS_NO_AVX2", Scope::Solver, "", "x86-64: no AVX2 or AVX-512 kernels (benchmarking)" },
    { "BURRTOOLS_NO_AVX512", Scope::Solver, "", "x86-64: no AVX-512 kernels; AVX2 stays (benchmarking)" },
    { "BURRTOOLS_NO_NEON", Scope::Solver, "", "ARM: no NEON kernels (benchmarking)" },
    { "BURRTOOLS_NO_DISASM_SIMD", Scope::Solver, "", "the disassembler's movement analysis without vector kernels (benchmarking)" },
    { "BURRTOOLS_NO_DISASM_POOL", Scope::Solver, "", "disassemble on the solving thread instead of a pool of workers (benchmarking)" },
    { "BURRTOOLS_NO_BUDGET", Scope::Solver, "", "no shared cap on the solver's threads: each pool takes all it asks for (benchmarking)" },
    { "BURRTOOLS_HUANG_MEM_MB", Scope::Solver, "<MB>", "memory budget for assembler 1's SIMD path; 0 turns that path off (benchmarking)" },
    { "BURRTOOLS_SPLIT_FANOUT", Scope::Solver, "<n>", "assembler 0 divides a search level into at most n tasks plus one for the rest (default 16); any n >= 1 gives the same result (testing)" },
    { "BURRTOOLS_SPLIT_BREADTH_FIRST", Scope::Solver, "", "assembler 0 queues the tasks a split makes at the back of the pool instead of the front (benchmarking)" },

    // both GUIs
    { "BURRTOOLS_UPDATE_VERSION_OVERRIDE", Scope::Gui, "<version>", "the update check (Help > Check for updates) compares against this version instead of the program's (testing)" },

    // burrtools-qt
    { "BURRTOOLS_QT_SETTINGS", Scope::QtGui, "<file>", "use this settings file instead of the user's; the pipeline caches go beside it and Qt's own disk cache is off -- for scripted runs" },
    { "BURRTOOLS_RHI", Scope::QtGui, "d3d11|d3d12|vulkan|opengl|metal", "the 3D graphics API (default: the platform's -- Direct3D 11, Metal, OpenGL)" },
    { "BURRTOOLS_OFFSCREEN_RHI", Scope::QtGui, "d3d11|vulkan|opengl|metal", "the graphics API of the image export (and the render tests)" },
    { "BURRTOOLS_EARLY_DEVICE", Scope::QtGui, "0", "Windows: leave the window's Direct3D 11 device to Qt instead of making it at start-up on a thread of its own" },
    { "BURRTOOLS_STARTUP_TRACE", Scope::QtGui, "1|quit", "print on stderr how long start-up took to each step, up to app ready; quit also quits there" },
    { "BURRTOOLS_QML_OUTLINES", Scope::QtGui, "", "outline every named item; the one under the mouse is highlighted and labelled with its objectName and QML file:line, its parents printed on stderr (for reading the QML)" },

    // tests and scripts
    { "BURRTOOLS_TEST_QPA", Scope::Tests, "<platform>", "test_qtgui's Qt platform plugin (default offscreen; Linux CI uses xcb on Xvfb for Vulkan)" },
    { "BURRTOOLS_REQUIRE_GPU_TESTS", Scope::Tests, "", "render tests fail instead of skipping when no graphics backend can be made" },
    { "BURRTOOLS_REQUIRE_SNAPSHOTS", Scope::Tests, "", "a missing gallery reference image fails instead of skipping" },
    { "BURRTOOLS_UPDATE_SNAPSHOTS", Scope::Tests, "1", "gallery checks write their grabs as the new references, where missing or no longer matching" },
    { "BURRTOOLS_SNAPSHOTS_ONLY", Scope::Tests, "<row,row>", "only these gallery rows (e.g. button,switch); unset: all" },
    { "BURRTOOLS_SNAPSHOT_DIR", Scope::Tests, "<dir>", "where the gallery reference images are (set by meson)" },
    { "BURRTOOLS_SNAPSHOT_OUT", Scope::Tests, "<dir>", "where grabs that did not match go (set by meson)" },
  };

  inline std::span<const Variable> all(void) { return variables; }

  inline std::string_view scopeTitle(Scope s) {
    switch (s) {
      case Scope::Solver: return "Solver (every program)";
      case Scope::Gui:    return "The GUIs (burrtools, burrtools-qt)";
      case Scope::QtGui:  return "burrtools-qt";
      case Scope::Tests:  return "Tests and scripts";
    }
    return "";
  }

  /* `text` in lines of at most `width` columns, each indented by `indent`
   * spaces (a word longer than a line gets one of its own); runs of spaces
   * count as one */
  inline void printWrapped(std::ostream & out, std::string_view text, std::size_t indent, std::size_t width = 79) {
    std::size_t col = 0;
    while (!text.empty()) {
      const std::size_t space = text.find(' ');
      const std::string_view word = text.substr(0, space);
      text = (space == std::string_view::npos) ? std::string_view() : text.substr(space + 1);
      if (word.empty())
        continue;
      if (col > 0 && col + 1 + word.size() > width) {
        out << "\n";
        col = 0;
      }
      if (col == 0) {
        out << std::string(indent, ' ');
        col = indent;
      } else {
        out << ' ';
        col++;
      }
      out << word;
      col += word.size();
    }
    out << "\n";
  }

  /* "Environment variables:" and the variables of the given scopes, under
   * one heading per scope, for a --help. A switch is printed by its name
   * alone, its effect saying that any value turns it on. */
  inline void print(std::ostream & out, std::initializer_list<Scope> scopes) {
    out << "\nEnvironment variables:\n";
    for (Scope s : scopes) {
      out << "\n  " << scopeTitle(s) << "\n";
      for (const Variable & v : variables)
        if (v.scope == s) {
          if (v.value.empty()) {
            out << "    " << v.name << "\n";
            printWrapped(out, "set to any value (even 0): " + std::string(v.effect), 8);
          } else {
            out << "    " << v.name << "=" << v.value << "\n";
            printWrapped(out, v.effect, 8);
          }
        }
    }
  }
}

#endif
