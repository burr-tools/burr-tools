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
#include "stlexport.h"

#include "BlockList.h"
#include "blocklistgroup.h"

#include "../lib/puzzle.h"
#include "../lib/voxel.h"

#include "../halfedge/polyhedron.h"
#include "../halfedge/volume.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <FL/fl_ask.H>
#pragma GCC diagnostic pop

static void cb_ShapeSelect_stub(Fl_Widget* /*o*/, void* v) { ((stlExport_c*)(v))->cb_NewPreviewSubject(); }

stlExport_c::stlExport_c(puzzle_c * p, const std::string & puzzleFile) : stlExportDialog_c(p, puzzleFile, "Export STL") {

  addOutputFrame(0, "test");
  addParameterFrame(1);

  {
    ShapeSelect = new PieceSelector(0, 0, 20, 20, puzzle);

    ShapeSelect->setSelection(0);

    LBlockListGroup_c * gr = new LBlockListGroup_c(0, 2, 1, 1, ShapeSelect);
    gr->callback(cb_ShapeSelect_stub, this);
    gr->setMinimumSize(200, 100);
    gr->stretch();
    gr->weight(0, 1);
  }

  addButtons(3);
  addStatusAndPreview(4);

  set_modal();
}

std::unique_ptr<Polyhedron> stlExport_c::previewMesh(std::string & text)
{
  std::unique_ptr<Polyhedron> p = mesh(*puzzle->getShape(ShapeSelect->getSelection()));

  char txt[100];
  snprintf(txt, sizeof txt, "Volume: %1.1f cubic-units", volume(*p));
  text = txt;
  return p;
}

void stlExport_c::cb_Export(void) {

  applyParameters();

  const std::string fname = outputFile();
  if (fname.empty()) {
    fl_message("Please enter a file name.");
    return;
  }

  if (!mayWrite(fname)) return;

  write(fname, { puzzle->getShape(ShapeSelect->getSelection()) });
}
