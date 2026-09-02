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

/* Export every unique piece shape of the currently selected solution to
 * individual STL files, one file per distinct piece type.
 */
#ifndef __STL_EXPORT_SOLUTION_H__
#define __STL_EXPORT_SOLUTION_H__

#include "Layouter.h"
#include "../lib/stl.h"
#include "../halfedge/modifiers.h"

#include <vector>

class puzzle_c;

/* Internal helper — forward-declared so the vector member compiles without
 * pulling in the full definition from the anonymous namespace in the .cpp */
namespace stlExportSolutionImpl { struct Param; }

class stlExportSolution_c : public LFl_Double_Window {

  private:

    puzzle_c    * puzzle;
    unsigned int  prob;
    unsigned int  sol;

    stlExporter_c * stl;

    std::vector<stlExportSolutionImpl::Param*> params;

    LFl_Input        * Pname;
    LFl_Check_Button * Binary;
    LFl_Box          * status;

  public:

    stlExportSolution_c(puzzle_c * p, unsigned int prob, unsigned int sol);
    virtual ~stlExportSolution_c(void);

    void cb_Export(void);
    void cb_Abort(void);
    void cb_FileChooser(void);
};

#endif
