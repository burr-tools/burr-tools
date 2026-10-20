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
#ifndef __ROTATION_TABLES_H__
#define __ROTATION_TABLES_H__

#include "rotationgeometry.h"

#include <memory>
#include <vector>

class problem_c;

/**
 * Solver-side setup of rotation moves: which physical model and which
 * rotate-and-shift paths a solver uses, and where its tables come from.
 *
 * Rotation moves are optional. A solver that does not want them never asks
 * for tables and passes an empty pointer down to its disassemblers, so it
 * neither builds nor touches anything here.
 */
struct rotationSettings_t {
  rotationGeometry::model_t model;
  std::vector<double> amounts;   ///< shift amounts of the wiggle and slide paths
};

/** The settings used when the caller has no others: PR #50's implicit
 *  model (play 0.081, sharp edges) until physical calibration provides
 *  better defaults, and shift amounts 0.1 to 0.4. */
rotationSettings_t defaultRotationSettings(void);

/** Table radius large enough for every rotation move of this problem:
 *  the largest dimension of its result shape plus one. */
int rotationRadiusFor(const problem_c & problem);

/**
 * The rotation tables for a problem, or nullptr when rotation moves are not
 * possible for it (only cube grids with a valid result shape have them).
 *
 * Tables are immutable and shared: all solvers asking for the same settings
 * get the same object, built on the first request and freed when the last
 * user drops it; a table with a larger radius also serves smaller ones.
 * Safe to call from several threads. Building takes up to a few seconds, so
 * callers do it once, before starting worker threads.
 */
std::shared_ptr<const rotationGeometry::rotationTables_c>
rotationTablesFor(const problem_c & problem, const rotationSettings_t & settings);

/** Same, with an explicit radius; for callers without a problem and tests. */
std::shared_ptr<const rotationGeometry::rotationTables_c>
sharedRotationTables(const rotationSettings_t & settings, int radius);

#endif
