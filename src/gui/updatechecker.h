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
#ifndef __UPDATECHECKER_H__
#define __UPDATECHECKER_H__

#include "../tools/updatecheck.h"

#include <optional>
#include <string>

struct HttpResult;

namespace updatechecker {

  /* "BurrTools/<version>", sent with every request. */
  std::string userAgent(void);

  /* The running build's version string: BURRTOOLS_VERSION, unless the
   * environment variable BURRTOOLS_UPDATE_VERSION_OVERRIDE is set, in
   * which case that value is used instead. The override exists so the
   * update dialog can be exercised against the live API by pretending to
   * be an older release, e.g. BURRTOOLS_UPDATE_VERSION_OVERRIDE=0.7.0.
   */
  std::string installedVersionString(void);
  std::optional<updatecheck::Version> installedVersion(void);

  /* One-line, user-facing reason for a failed fetch. */
  std::string describeFailure(const HttpResult & r);

  /* `burrtools --check-for-updates`: fetch, evaluate as a manual check,
   * print the outcome, and return the process exit code (0 when the check
   * completed, 1 when it could not). No window is created.
   */
  int runCli(void);
}

#endif
