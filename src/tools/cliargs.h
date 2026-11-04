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
#ifndef BT_TOOLS_CLIARGS_H
#define BT_TOOLS_CLIARGS_H

#include "gzstream.h"

#include <cstdio>
#include <cstring>
#include <istream>
#include <memory>
#include <string>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

/* Command-line helpers shared by the programs: the solvers burrTxt and
 * burrTxt2, and for standard input the GUIs. */
namespace btcli {

  /* Whether the command line asks for the help: -h or --help, anywhere in
   * it, so that `burrTxt file -h` shows it too. */
  inline bool wantsHelp(int argc, char * argv[]) {
    for (int i = 1; i < argc; i++)
      if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0)
        return true;
    return false;
  }

  /* The file name "-" stands for standard input. */
  inline bool namesStdin(const char * file) {
    return std::strcmp(file, "-") == 0;
  }

  inline int stdinDescriptor(void) {
#ifdef _WIN32
    return _fileno(stdin);
#else
    return fileno(stdin);
#endif
  }

  inline bool isTerminal(int fd) {
#ifdef _WIN32
    return _isatty(fd);
#else
    return isatty(fd);
#endif
  }

  /* Whether a solver reads its puzzle from standard input: asked for with
   * "-", or with no file (a null pointer) when something is piped or
   * redirected in. The GUIs only go by namesStdin(): the stdin a desktop
   * starts them with is no terminal either, and holds no puzzle.
   */
  inline bool solverReadsStdin(const char * file) {
    return file ? namesStdin(file) : !isTerminal(stdinDescriptor());
  }

  /* A puzzle, compressed or plain, from an open descriptor: the stream, or
   * nullptr and in error why there is none. A terminal is refused, as nobody
   * types a puzzle file in and a program would just sit there waiting for it.
   */
  inline std::unique_ptr<std::istream> openPuzzleInput(int fd, std::string & error) {

    if (fd >= 0 && isTerminal(fd)) {
      error = "standard input is a terminal, pipe or redirect a puzzle into it";
      return nullptr;
    }

    auto str = openGzDescriptor(fd);

    if (!str)
      error = "could not read standard input";
    else if (str->peek() == EOF) {
      // also what compressed data that is damaged from the start reads as
      error = "no puzzle on standard input";
      str.reset();
    }

    return str;
  }

  inline std::unique_ptr<std::istream> openStdin(std::string & error) {
    return openPuzzleInput(stdinDescriptor(), error);
  }
}

#endif
