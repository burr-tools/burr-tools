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
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02110-1307, USA.
 */
#ifndef __SOLVER_RESUMEDATA_H__
#define __SOLVER_RESUMEDATA_H__

#include "../assembly.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/** Opaque search position plus pending (not yet disassembled) assemblies. */
struct resumeData_t {
  std::string engine;  ///< engine/format version tag (e.g. "2.2")
  std::string state;   ///< opaque engine search state
  std::vector<std::unique_ptr<assembly_c>> pending;
  /** seq number (solverResult_t::seq) the resumed session hands out next, so
   *  numbering continues across a save and a new session */
  uint64_t nextSeq = 0;

  /** True when an engine search state is stored. */
  bool hasEngineState(void) const { return !state.empty(); }

  /** True when neither engine state nor pending assemblies are stored. */
  bool empty(void) const { return engine.empty() && state.empty() && pending.empty(); }

  /** Clear engine state, version tag and pending assemblies. */
  void clear(void) {
    engine.clear();
    state.clear();
    pending.clear();
    nextSeq = 0;
  }

  /** Deep-copy this resume snapshot (cloning pending assemblies). */
  resumeData_t clone(void) const;
};

#endif // __SOLVER_RESUMEDATA_H__
