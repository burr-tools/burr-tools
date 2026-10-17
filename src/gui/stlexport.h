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

/* stlexport outputs the puzzle shape in stl format
 * for 3d-printing the puzzle
 */

/* following parameters can be chosen:
 * cube size in mm
 * bevel (amount of mm to round the edges of cubes)
 * offset (in mm - tolerance of the 3d printer,
 *         shrinks all solids by that amount)
 * file name
 */
#ifndef __STL_EXPORT_H__
#define __STL_EXPORT_H__

#include "stlexportdialog.h"

#include <string>

class PieceSelector;

class stlExport_c : public stlExportDialog_c {

  private:

    PieceSelector * ShapeSelect;

  protected:

    std::unique_ptr<Polyhedron> previewMesh(std::string & text) override;

  public:

    /* puzzleFile is the path the current puzzle was loaded from, or empty
     * when it has never been saved; it supplies the default output folder */
    stlExport_c(puzzle_c * p, const std::string & puzzleFile);

    void cb_Export(void) override;
};

#endif
