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
#ifndef BTUI_PUZZLETOOLS_H
#define BTUI_PUZZLETOOLS_H

#include "../lib/gridtype.h"

#include <memory>
#include <string>
#include <vector>

class puzzle_c;
class voxel_c;

/* The work behind three legacy menu dialogs, without the dialogs: Convert
 * (convertwindow.cpp, mainWindow_c::cb_Convert), Import assemblies
 * (assmimportwindow.cpp, mainWindow_c::cb_AssembliesToShapes) and the
 * Status window's shape table (statuswindow.cpp). Each does exactly what the
 * legacy code does; the redesigned GUI only puts a new face on them
 * (OQ-34: these dialogs keep their content).
 */
namespace btui {

  // --- Convert ---------------------------------------------------------------

  /* the grid types a puzzle of this type can be converted to, in enum order */
  std::vector<gridType_c::gridType> convertTargets(gridType_c::gridType from);

  // --- Import assemblies ------------------------------------------------------

  struct ImportAssembliesOptions {
    unsigned int sourceProblem = 0;
    enum class Destination { JustAddShapes, NewProblem, ExistingProblem } destination = Destination::JustAddShapes;
    unsigned int destinationProblem = 0;
    unsigned int rangeMin = 0, rangeMax = 1;         ///< the count range given to the new shapes in a problem
    bool dropDisconnected = true;
    bool dropMirror = false;                         ///< shapes with a mirror symmetry
    bool dropSymmetric = false;                      ///< shapes with any symmetry
    bool dropNonMillable = false;                    ///< bricks only
    bool dropNonNotchable = false;                   ///< bricks only
    bool dropIdentical = true;
    unsigned int shapeMin = 0, shapeMax = 1000000;   ///< allowed number of fixed voxels
  };

  /* Turn the saved assemblies of a problem into new shapes, filtered as the
   * options say. Returns how many shapes were added. The caller records the
   * undo step. */
  unsigned int importAssemblies(puzzle_c & p, const ImportAssembliesOptions & opt);

  // --- Status window ---------------------------------------------------------

  /* one row of the legacy Shape Information table */
  struct ShapeStatus {
    unsigned int fixed = 0, variable = 0;
    /* the earlier shape this one equals, -1 if none: allowing mirrors, as a
     * shape, and including colours without mirroring */
    int identicalMirror = -1, identicalShape = -1, identicalComplete = -1;
    bool connectedFace = false, connectedEdge = false, connectedCorner = false;
    bool holes2d = false, holes3d = false;
    bool notchable = false, millable = false;   ///< computed for bricks only
    bool symmetryKnown = false;
    long long symmetry = 0;
  };

  /* the table row for shape s; call in shape order -- `table` remembers the
   * shapes before it for the "identical" columns */
  class ShapeStatusCalculator {
    public:
      explicit ShapeStatusCalculator(const puzzle_c & p);
      ~ShapeStatusCalculator(void);
      ShapeStatusCalculator(const ShapeStatusCalculator &) = delete;
      ShapeStatusCalculator & operator=(const ShapeStatusCalculator &) = delete;
      ShapeStatus next(void);
      bool done(void) const;
      unsigned int position(void) const { return pos; }
    private:
      struct Impl;
      const puzzle_c & puz;
      unsigned int pos = 0;
      std::unique_ptr<Impl> d;
  };

  /* Remove shapes (indices in any order) the way the Status window's
   * "Remove selected" does: every problem that uses a removed shape loses its
   * solutions first. */
  void removeShapes(puzzle_c & p, std::vector<unsigned int> indices);
}

#endif
