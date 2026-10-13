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

#include "Images.h"
#include "Layouter.h"
#include "../lib/stl.h"
#include "../halfedge/modifiers.h"

#include <vector>
#include <memory>
#include <string>

class puzzle_c;
class LView3dGroup;
class ButtonGroup_c;

/* Internal helper — forward-declared so the vector member compiles without
 * pulling in the full definition from the anonymous namespace in the .cpp */
namespace stlExportSolutionImpl { struct Param; }

class stlExportSolution_c : public LFl_Double_Window {

  private:

    puzzle_c    * puzzle;
    unsigned int  prob;
    unsigned int  sol;

    /* the default output directory, offered in the folder field */
    std::string exportDir;

    std::unique_ptr<stlExporter_c> stl;

    std::vector<std::unique_ptr<stlExportSolutionImpl::Param>> params;

    /* the distinct piece shapes placed in the solution, in the order of
     * the preview choice, and how often each is placed */
    std::vector<unsigned int> shapeIds;
    std::vector<unsigned int> shapeCounts;

    LFl_Radio_Button * ModeSeparate;
    LFl_Radio_Button * ModeAssembled;
    LFl_Input        * Fname;
    LFl_Input        * Pname;
    LFl_Check_Button * Binary;
    LFl_Choice       * PieceSelect;
    LFl_Box          * status;
    LView3dGroup     * view3D;
    ButtonGroup_c    * viewMode;

    pixmapList_c pm;

    bool assembled(void) const;

  public:

    /* puzzleFile is the path the current puzzle was loaded from, or empty
     * when it has never been saved; it supplies the default output folder,
     * as in the single-shape STL export */
    stlExportSolution_c(puzzle_c * p, const std::string & puzzleFile, unsigned int prob, unsigned int sol);
    virtual ~stlExportSolution_c(void);

    void cb_Export(void);
    void cb_Abort(void);
    void cb_FileChooser(void);
    void cb_Mode(void);
    void cb_Update3DView(void);
    void cb_Update3DViewParams(void);
};

#endif
