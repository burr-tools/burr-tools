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

/* What the STL export dialogs share: the grid's exporter with a field for
 * each of its parameters (offset, bevel, ...), the file name and folder
 * fields, the Export and Close buttons, and the 3D preview with its status
 * line and view mode buttons.
 *
 * A dialog builds these parts with the add... calls, at the rows of its
 * own layout, and supplies the mesh to preview and the export itself.
 */
#ifndef __STL_EXPORT_DIALOG_H__
#define __STL_EXPORT_DIALOG_H__

#include "Images.h"
#include "Layouter.h"

#include "../lib/stl.h"

#include <vector>
#include <memory>
#include <string>

class puzzle_c;
class voxel_c;
class Polyhedron;
class LView3dGroup;
class ButtonGroup_c;

class stlExportDialog_c : public LFl_Double_Window {

  protected:

    /* the puzzle whose shapes are exported */
    puzzle_c * puzzle;

    /* the default output folder: the one the puzzle was loaded from, else
     * the home directory */
    std::string exportDir;

    std::unique_ptr<stlExporter_c> stl;

    LFl_Input * Fname = nullptr;
    LFl_Input * Pname = nullptr;
    LFl_Check_Button * Binary = nullptr;
    LFl_Box * status = nullptr;

    /* puzzleFile is the path the current puzzle was loaded from, or empty
     * when it has never been saved. It supplies the default output folder;
     * the folder field used to default to ".", which resolves to the
     * working directory -- "/" for an application launched from a macOS
     * bundle, which is read-only.
     */
    stlExportDialog_c(puzzle_c * p, const std::string & puzzleFile, const char * title);

    /* the file name and folder fields with their chooser button, and the
     * binary switch */
    void addOutputFrame(int row, const char * defaultName);

    /* a field for each of the exporter's parameters */
    void addParameterFrame(int row);

    /* the Export and Close buttons */
    void addButtons(int row);

    /* the status line and the view mode buttons in the given row, and the
     * preview right of all rows above it */
    void addStatusAndPreview(int row);

    /* the folder field, or the default folder when that is empty */
    std::string folder(void) const;

    /* the file named by the file name field in folder(), with ".stl"
     * appended unless the name already ends in it (in any case); empty
     * when the name field is empty */
    std::string outputFile(void) const;

    /* true when fname may be written: it doesn't exist, the native save
     * dialog already asked about it, or the user agrees to replace it */
    bool mayWrite(const std::string & fname) const;

    /* set the exporter's parameters and binary mode from the fields */
    void applyParameters(void);

    /* the exporter's mesh of a shape, throws when it can't make one */
    std::unique_ptr<Polyhedron> mesh(const voxel_c & v) const;

    /* write shapes into fname, each a solid of its own, telling the user
     * about a failure; true when the file was written */
    bool write(const std::string & fname, const std::vector<const voxel_c *> & shapes);

    /* the mesh to preview, nothing to show nothing, and the text for the
     * status line; may throw stlException_c */
    virtual std::unique_ptr<Polyhedron> previewMesh(std::string & text) = 0;

  public:

    virtual ~stlExportDialog_c(void);

    /* the first preview is made once the window is up, so an error from
     * meshing is reported over the dialog instead of before it appears */
    void show(void) override;

    virtual void cb_Export(void) = 0;
    virtual void cb_FileChooser(void);
    void cb_Close(void);
    void cb_Update3DView(void);
    void cb_Update3DViewParams(void);

  private:

    struct parameterField_c;
    std::vector<std::unique_ptr<parameterField_c>> params;

    LView3dGroup * view3D = nullptr;
    ButtonGroup_c * viewMode = nullptr;

    pixmapList_c pm;
};

#endif
