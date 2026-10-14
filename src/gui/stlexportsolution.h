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

/* Export the pieces of the currently selected solution to STL, in one of
 * two ways:
 *  - each distinct piece type to its own file, in its own orientation;
 *  - all pieces into one file, each a separate solid in its place in the
 *    assembled solution.
 * Both show a live 3D preview and the grid's exporter parameters (offset,
 * bevel, ...), like the single-shape STL export.
 */
#ifndef __STL_EXPORT_SOLUTION_H__
#define __STL_EXPORT_SOLUTION_H__

#include "stlexportdialog.h"

#include <vector>
#include <memory>
#include <string>

class stlExportSolution_c : public stlExportDialog_c {

  private:

    /* the distinct piece shapes placed in the solution, in the order of
     * the preview choice, and how often each is placed */
    std::vector<unsigned int> shapeIds;
    std::vector<unsigned int> shapeCounts;

    /* one space per placed piece, each the size of the whole assembly so
     * the pieces stay in place; empty when the solution has no assembly */
    std::vector<std::unique_ptr<voxel_c>> pieceSpaces;

    LFl_Radio_Button * ModeSeparate;
    LFl_Radio_Button * ModeAssembled;
    LFl_Choice       * PieceSelect;

    bool assembled(void) const;

    /* the file name only for the one assembled file, the preview piece
     * only when each piece type goes to its own file */
    void activateModeFields(void);

    void exportAssembled(void);
    void exportSeparate(void);

  protected:

    std::unique_ptr<Polyhedron> previewMesh(std::string & text) override;

  public:

    /* puzzleFile is the path the current puzzle was loaded from, or empty
     * when it has never been saved; it supplies the default output folder
     * and the default file name */
    stlExportSolution_c(puzzle_c * p, const std::string & puzzleFile, unsigned int prob, unsigned int sol);
    virtual ~stlExportSolution_c(void);

    void cb_Export(void) override;
    void cb_FileChooser(void) override;
    void cb_Mode(void);
};

#endif
