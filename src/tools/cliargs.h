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

#include <cstring>

/* Command-line helpers shared by burrTxt and burrTxt2. */
namespace btcli {

  /* Whether the command line asks for the help: -h or --help, anywhere in
   * it, so that `burrTxt file -h` shows it too. */
  inline bool wantsHelp(int argc, char * argv[]) {
    for (int i = 1; i < argc; i++)
      if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0)
        return true;
    return false;
  }
}

#endif
