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
#include "stlexportdialog.h"

#include "filechooser.h"
#include "view3dgroup.h"
#include "voxelframe.h"
#include "buttongroup.h"

#include "../lib/puzzle.h"
#include "../lib/gridtype.h"
#include "../lib/bt_assert.h"

#include "../halfedge/polyhedron.h"

#include "../tools/fileexists.h"
#include "../tools/homedir.h"

#include <stdlib.h>
#include <ctype.h>
#include <filesystem>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define GL_SILENCE_DEPRECATION 1
#include <FL/Fl.H>
#include <FL/fl_ask.H>
#pragma GCC diagnostic pop

/* the field for one of the exporter's parameters */
struct stlExportDialog_c::parameterField_c {
  stlExporter_c::parameterTypes type;
  Fl_Widget * w;
};

static void cb_Export_stub      (Fl_Widget*, void* v) { ((stlExportDialog_c*)v)->cb_Export(); }
static void cb_Close_stub       (Fl_Widget*, void* v) { ((stlExportDialog_c*)v)->cb_Close(); }
static void cb_FileChooser_stub (Fl_Widget*, void* v) { ((stlExportDialog_c*)v)->cb_FileChooser(); }
static void cb_Update3DView_stub(Fl_Widget*, void* v) { ((stlExportDialog_c*)v)->cb_Update3DView(); }
static void cb_ViewParams_stub  (Fl_Widget*, void* v) { ((stlExportDialog_c*)v)->cb_Update3DViewParams(); }

stlExportDialog_c::stlExportDialog_c(puzzle_c * p, const std::string & puzzleFile, const char * title) :
  LFl_Double_Window(true), puzzle(p), exportDir(std::filesystem::path(puzzleFile).parent_path().string())
{
  /* An unsaved puzzle has no folder, so the home directory stands in: it
   * exists and is writable, neither of which holds for the working
   * directory.
   *
   * homedir() ends in a separator, so it goes through parent_path() as well,
   * leaving both branches in the same shape.
   */
  if (exportDir.empty())
    exportDir = std::filesystem::path(homedir()).parent_path().string();

  label(title);

  stl = std::unique_ptr<stlExporter_c>(p->getGridType()->getStlExporter());
  bt_assert(stl);
}

stlExportDialog_c::~stlExportDialog_c(void) = default;

void stlExportDialog_c::addOutputFrame(int row, const char * defaultName)
{
  LFl_Frame * fr = new LFl_Frame(0, row, 1, 1);

  (new LFl_Box("File name", 0, 0))->stretchLeft();
  (new LFl_Box("Folder", 0, 1))->stretchLeft();

  (new LFl_Box(1, 0))->setMinimumSize(5, 0);
  (new LFl_Box(3, 0))->setMinimumSize(5, 0);

  Fname = new LFl_Input(2, 0, 1, 1);
  Fname->value(defaultName);
  Fname->weight(1, 0);
  Fname->setMinimumSize(50, 0);
  Pname = new LFl_Input(2, 1, 1, 1);
  Pname->value(exportDir.c_str());
  Pname->weight(1, 0);
  Pname->setMinimumSize(50, 0);

  (new LFl_Button("...", 4, 0, 1, 2))->callback(cb_FileChooser_stub, this);

  Binary = new LFl_Check_Button("Binary STL", 0, 2, 3, 1);
  Binary->value(stl->getBinaryMode() ? 1 : 0);

  fr->end();
}

void stlExportDialog_c::addParameterFrame(int row)
{
  LFl_Frame * fr = new LFl_Frame(0, row, 1, 1);

  for (unsigned int i = 0; i < stl->numParameters(); i++) {
    auto inp = std::make_unique<parameterField_c>();
    inp->type = stl->getParameterType(i);

    switch (inp->type) {
      case stlExporter_c::PAR_TYP_DOUBLE:
      case stlExporter_c::PAR_TYP_POS_DOUBLE: {
        (new LFl_Box(stl->getParameterName(i), 0, i))->stretchRight();
        (new LFl_Box(1, i))->setMinimumSize(5, 0);
        LFl_Float_Input * in = new LFl_Float_Input(2, i, 1, 1);
        char val[16]; snprintf(val, sizeof val, "%2.2f", stl->getParameter(i));
        in->value(val); in->weight(1, 0);
        inp->w = in;
        break;
      }
      case stlExporter_c::PAR_TYP_POS_INTEGER: {
        (new LFl_Box(stl->getParameterName(i), 0, i))->stretchRight();
        (new LFl_Box(1, i))->setMinimumSize(5, 0);
        LFl_Int_Input * in = new LFl_Int_Input(2, i, 1, 1);
        char val[16]; snprintf(val, sizeof val, "%i", (int)stl->getParameter(i));
        in->value(val); in->weight(1, 0);
        inp->w = in;
        break;
      }
      case stlExporter_c::PAR_TYP_SWITCH: {
        LFl_Check_Button * in = new LFl_Check_Button(stl->getParameterName(i), 0, i, 3, 1);
        in->value(stl->getParameter(i) ? 1 : 0);
        in->weight(1, 0);
        inp->w = in;
        break;
      }
      default:
        bt_assert(0);
    }
    inp->w->callback(cb_Update3DView_stub, this);
    inp->w->tooltip(stl->getParameterTooltip(i));
    params.push_back(std::move(inp));
  }

  fr->end();
}

void stlExportDialog_c::addButtons(int row)
{
  LFl_Frame * fr = new LFl_Frame(0, row, 1, 1);

  layouter_c * o = new layouter_c(0, 0, 1, 1);
  o->pitch(5);

  (new LFl_Button("Export STL", 0, 0, 1, 1))->callback(cb_Export_stub, this);

  (new LFl_Box(0, 1))->setMinimumSize(0, 5);

  (new LFl_Button("Close", 0, 2, 1, 1))->callback(cb_Close_stub, this);

  o->end();
  fr->end();
}

void stlExportDialog_c::addStatusAndPreview(int row)
{
  layouter_c * l = new layouter_c(0, row, 2, 1);

  status = new LFl_Box(0, 0, 1, 1);
  status->box(FL_UP_BOX);
  status->weight(1, 0);
  status->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

  viewMode = new ButtonGroup_c(1, 0, 1, 1);

  Fl_Button * b = viewMode->addButton();
  b->image(pm.get(ViewModeNormal_xpm));
  b->tooltip(" Display STL object normally ");

  b = viewMode->addButton();
  b->image(pm.get(ViewModeInsides_xpm));
  b->tooltip(" Display the insides of the STL object ");

  viewMode->callback(cb_ViewParams_stub, this);

  l->end();

  view3D = new LView3dGroup(1, 0, 1, row);
  view3D->setMinimumSize(400, 400);
  view3D->weight(1, 0);
  /* the preview shows the exporter's own mesh: plain faces, whatever
   * style the main window is in (the edge-line style would draw a line
   * along every bevel boundary) */
  view3D->getView()->setRenderStyle(voxelFrame_c::styleVoxel);
}

std::string stlExportDialog_c::folder(void) const
{
  const char * f = Pname->value();
  return (f && f[0]) ? std::string(f) : exportDir;
}

std::string stlExportDialog_c::outputFile(void) const
{
  std::string name = Fname->value();
  if (name.empty()) return name;

  std::string ext = std::filesystem::path(name).extension().string();
  for (char & c : ext) c = (char)tolower((unsigned char)c);
  if (ext != ".stl")
    name += ".stl";

  return (std::filesystem::path(folder()) / name).string();
}

bool stlExportDialog_c::mayWrite(const std::string & fname) const
{
  // no need to ask when the file is the one the user picked in a native
  // save dialog, that dialog already asked
  return !fileExists(fname) || fileChooserConfirmedOverwrite(fname.c_str()) ||
         fl_choice("File exists overwrite?", "Cancel", "Overwrite", 0) != 0;
}

void stlExportDialog_c::applyParameters(void)
{
  for (unsigned int i = 0; i < stl->numParameters(); i++) {
    switch (params[i]->type) {
      case stlExporter_c::PAR_TYP_DOUBLE:
      case stlExporter_c::PAR_TYP_POS_DOUBLE:
        stl->setParameter(i, atof(static_cast<LFl_Float_Input*>(params[i]->w)->value()));
        break;
      case stlExporter_c::PAR_TYP_POS_INTEGER:
        stl->setParameter(i, atoi(static_cast<LFl_Int_Input*>(params[i]->w)->value()));
        break;
      case stlExporter_c::PAR_TYP_SWITCH:
        stl->setParameter(i, static_cast<LFl_Check_Button*>(params[i]->w)->value());
        break;
      default:
        bt_assert(0);
    }
  }

  stl->setBinaryMode(Binary->value() != 0);
}

std::unique_ptr<Polyhedron> stlExportDialog_c::mesh(const voxel_c & v) const
{
  std::unique_ptr<Polyhedron> p(stl->getMesh(v));
  if (!p) throw stlException_c("Something went wrong when generating the STL polyhedron");
  return p;
}

bool stlExportDialog_c::write(const std::string & fname, const std::vector<const voxel_c *> & shapes)
{
  try {
    stl->write(fname.c_str(), shapes);
    return true;
  } catch (stlException_c & e) {
    fl_message("Error exporting %s:\n%s", fname.c_str(), e.comment);
  } catch (...) {
    fl_message("Unexpected error exporting %s", fname.c_str());
  }
  return false;
}

void stlExportDialog_c::show(void)
{
  bool first = !shown();
  LFl_Double_Window::show();
  if (first) cb_Update3DView();
}

void stlExportDialog_c::cb_Close(void) { hide(); }

void stlExportDialog_c::cb_FileChooser(void)
{
  const std::string cur = (std::filesystem::path(folder()) / Fname->value()).string();

  const char * f = fileChooser("Choose STL File to write", "STL", "*.stl", cur.c_str(), true);
  if (!f) return;

  const std::filesystem::path chosen(f);
  Fname->value(chosen.filename().string().c_str());
  Pname->value(chosen.parent_path().string().c_str());
}

void stlExportDialog_c::cb_Update3DView(void)
{
  /* nothing to preview before the window is up, the first preview comes
   * from show() */
  if (!shown()) return;

  applyParameters();

  try {
    std::string text;
    std::unique_ptr<Polyhedron> p = previewMesh(text);
    if (p)
      view3D->getView()->showMesh(p.release());
    else
      view3D->getView()->showNothing();
    status->copy_label(text.c_str());
  }
  catch (stlException_c & e) {
    /* nothing to show: the preview must not keep the last mesh */
    view3D->getView()->showNothing();
    status->label("");
    fl_message("%s", e.comment);
  }
  catch (...) {
    view3D->getView()->showNothing();
    status->label("");
    fl_message("The generated mesh is faulty in some way, try to tweak the parameter");
  }
}

void stlExportDialog_c::cb_Update3DViewParams(void)
{
  switch (viewMode->getSelected()) {
    case 0: view3D->getView()->setInsideVisible(false); break;
    case 1: view3D->getView()->setInsideVisible(true); break;
    default: break;
  }
}
