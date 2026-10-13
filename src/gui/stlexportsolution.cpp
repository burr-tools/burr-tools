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

#include "stlexportsolution.h"

#include "Layouter.h"
#include "filechooser.h"
#include "view3dgroup.h"
#include "voxelframe.h"
#include "buttongroup.h"

#include "../lib/puzzle.h"
#include "../lib/problem.h"
#include "../lib/solution.h"
#include "../lib/assembly.h"
#include "../lib/gridtype.h"
#include "../lib/voxel.h"
#include "../lib/bt_assert.h"

#include "../halfedge/polyhedron.h"
#include "../halfedge/face.h"
#include "../halfedge/halfedge.h"
#include "../halfedge/vertex.h"
#include "../halfedge/volume.h"

#include "../tools/fileexists.h"
#include "../tools/homedir.h"

#include <stdlib.h>
#include <string.h>
#include <filesystem>
#include <map>
#include <string>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define GL_SILENCE_DEPRECATION 1
#include <FL/Fl.H>
#include <FL/fl_ask.H>
#include <FL/Fl_File_Chooser.H>
#pragma GCC diagnostic pop

/* ---------- internal helpers in anonymous namespace -------------------- */
namespace stlExportSolutionImpl {

  /* mirrors the inputField_c struct in stlexport.cpp but lives in its
   * own namespace to avoid ODR issues at link time                       */
  struct Param {
    stlExporter_c::parameterTypes type;
    Fl_Widget * w;
  };

  static void applyParams(stlExporter_c * stl,
                          const std::vector<std::unique_ptr<Param>> & params)
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
  }

  /* Return a filesystem-safe version of a shape name: replace anything
   * that isn't alphanumeric, '-', or '_' with '_'.                      */
  static std::string safeName(const std::string & s)
  {
    std::string out = s;
    for (size_t i = 0; i < out.size(); i++) {
      char c = out[i];
      if (!isalnum((unsigned char)c) && c != '-' && c != '_')
        out[i] = '_';
    }
    return out;
  }

  /* Build a map of shapeId -> count for the pieces actually placed in
   * the given solution's assembly (not just the parts defined in the
   * problem with non-zero max counts).                                  */
  static std::map<unsigned int, unsigned int> placedShapeCounts(
      const problem_c * pr, unsigned int sol)
  {
    std::map<unsigned int, unsigned int> counts;
    if (!pr) return counts;
    const solution_c * sav = pr->getSavedSolution(sol);
    if (!sav) return counts;
    const assembly_c * assm = sav->getAssembly();
    if (!assm) return counts;
    for (unsigned int i = 0; i < assm->placementCount(); i++) {
      if (!assm->isPlaced(i)) continue;
      unsigned int partId  = pr->getPartIdToPieceId(i);
      unsigned int shapeId = pr->getShapeIdOfPart(partId);
      counts[shapeId]++;
    }
    return counts;
  }

  /* one voxel space per placed piece of the solution, each the size of the
   * whole assembly so the pieces stay in place; empty without an assembly */
  static std::vector<std::unique_ptr<voxel_c>> pieceSpaces(
      const problem_c * pr, unsigned int sol)
  {
    if (!pr) return {};
    const solution_c * sav = pr->getSavedSolution(sol);
    if (!sav || !sav->getAssembly()) return {};
    return sav->getAssembly()->createPieceSpaces(*pr);
  }

  /* Append a copy of all faces of src to dst. Every piece keeps vertices
   * of its own, even where it touches another piece (no gap), so the
   * pieces stay separate solids in the combined preview mesh.            */
  static void appendPolyhedron(Polyhedron & dst, const Polyhedron & src)
  {
    std::map<const Vertex *, int> index;
    std::vector<int> corners;

    for (Polyhedron::const_face_iterator it = src.fBegin(); it != src.fEnd(); ++it) {
      const Face * f = *it;
      if (f->hole()) continue;

      corners.clear();
      Face::const_edge_circulator e = f->begin();
      Face::const_edge_circulator sentinel = e;
      do {
        const Vertex * v = (*e)->dst();
        std::map<const Vertex *, int>::const_iterator i = index.find(v);
        if (i == index.end())
          i = index.insert(std::make_pair(v, dst.addVertex(v->position())->index())).first;
        corners.push_back(i->second);
        ++e;
      } while (e != sentinel);

      Face * f2 = dst.addFace(corners);
      f2->_flags    = f->_flags;
      f2->_color    = f->_color;
      f2->_fb_index = f->_fb_index;
      f2->_fb_face  = f->_fb_face;
    }
  }

} // namespace stlExportSolutionImpl

using namespace stlExportSolutionImpl;

/* ---------- callbacks -------------------------------------------------- */

static void cb_Export_stub      (Fl_Widget*, void* v) { ((stlExportSolution_c*)v)->cb_Export(); }
static void cb_Abort_stub       (Fl_Widget*, void* v) { ((stlExportSolution_c*)v)->cb_Abort(); }
static void cb_FileChooser_stub (Fl_Widget*, void* v) { ((stlExportSolution_c*)v)->cb_FileChooser(); }
static void cb_Mode_stub        (Fl_Widget*, void* v) { ((stlExportSolution_c*)v)->cb_Mode(); }
static void cb_Update3DView_stub(Fl_Widget*, void* v) { ((stlExportSolution_c*)v)->cb_Update3DView(); }
static void cb_ViewParams_stub  (Fl_Widget*, void* v) { ((stlExportSolution_c*)v)->cb_Update3DViewParams(); }

bool stlExportSolution_c::assembled(void) const { return ModeAssembled->value() != 0; }

void stlExportSolution_c::cb_Abort(void) { hide(); }

void stlExportSolution_c::cb_Mode(void)
{
  /* a file name only for the one assembled file, the preview piece only
   * when each piece type goes to its own file */
  if (assembled()) {
    Fname->activate();
    PieceSelect->deactivate();
  } else {
    Fname->deactivate();
    PieceSelect->activate();
  }
  cb_Update3DView();
}

void stlExportSolution_c::cb_FileChooser(void)
{
  if (assembled()) {
    char curFile[1100];
    snprintf(curFile, sizeof curFile, "%s/%s", Pname->value(), Fname->value());

    const char * f = fileChooser("Choose STL File to write", "STL", "*.stl", curFile, true);
    if (!f) return;

    const std::filesystem::path chosen(f);
    Fname->value(chosen.filename().string().c_str());
    Pname->value(chosen.parent_path().string().c_str());
  } else {
    const char * f = fl_file_chooser(
        "Choose output folder (navigate into it and pick any file)",
        "", Pname->value(), 0);
    if (!f) return;

    Pname->value(std::filesystem::path(f).parent_path().string().c_str());
  }
}

void stlExportSolution_c::cb_Update3DViewParams(void)
{
  switch (viewMode->getSelected()) {
    case 0: view3D->getView()->setInsideVisible(false); break;
    case 1: view3D->getView()->setInsideVisible(true); break;
    default: break;
  }
}

void stlExportSolution_c::cb_Update3DView(void)
{
  applyParams(stl.get(), params);

  try {
    if (assembled()) {
      std::vector<std::unique_ptr<voxel_c>> spaces = pieceSpaces(puzzle->getProblem(prob), sol);
      if (spaces.empty()) {
        view3D->getView()->showNothing();
        status->label("This solution has no assembly to export");
        return;
      }

      std::unique_ptr<Polyhedron> all = std::make_unique<Polyhedron>();
      double vol = 0;
      for (const std::unique_ptr<voxel_c> & s : spaces) {
        std::unique_ptr<Polyhedron> p(stl->getMesh(*s));
        if (!p) throw stlException_c("Something went wrong when generating the STL polyhedron");
        vol += volume(*p);
        appendPolyhedron(*all, *p);
      }
      all->finalize();

      view3D->getView()->showMesh(all.release());
      char txt[100];
      snprintf(txt, sizeof txt, "%u pieces, volume: %1.1f cubic-units", (unsigned int)spaces.size(), vol);
      status->copy_label(txt);
    } else {
      if (shapeIds.empty()) {
        view3D->getView()->showNothing();
        status->label("No piece shapes to export for this solution");
        return;
      }

      std::unique_ptr<Polyhedron> p(stl->getMesh(*puzzle->getShape(shapeIds[PieceSelect->value()])));
      if (!p) throw stlException_c("Something went wrong when generating the STL polyhedron");

      char txt[100];
      snprintf(txt, sizeof txt, "Volume: %1.1f cubic-units", volume(*p));
      view3D->getView()->showMesh(p.release());
      status->copy_label(txt);
    }
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

void stlExportSolution_c::cb_Export(void)
{
  applyParams(stl.get(), params);
  stl->setBinaryMode(Binary->value() != 0);

  problem_c * pr = puzzle->getProblem(prob);

  const char * folder = Pname->value();
  if (!folder || folder[0] == '\0') folder = exportDir.c_str();

  if (assembled()) {
    std::string name = Fname->value();
    if (name.empty()) {
      fl_message("Please enter a file name.");
      return;
    }
    if (std::filesystem::path(name).extension() != ".stl")
      name += ".stl";
    const std::string fname = (std::filesystem::path(folder) / name).string();

    std::vector<std::unique_ptr<voxel_c>> spaces = pieceSpaces(pr, sol);
    if (spaces.empty()) {
      fl_message("This solution has no assembly to export.");
      return;
    }

    // no need to ask when the file is the one the user picked in a native
    // save dialog, that dialog already asked
    if (fileExists(fname) && !fileChooserConfirmedOverwrite(fname.c_str()) &&
        fl_choice("File exists overwrite?", "Cancel", "Overwrite", 0) == 0)
      return;

    std::vector<const voxel_c *> shapes;
    for (const std::unique_ptr<voxel_c> & s : spaces) shapes.push_back(s.get());

    try {
      stl->write(fname.c_str(), shapes);
    } catch (stlException_c & e) {
      fl_message("Error exporting %s:\n%s", fname.c_str(), e.comment);
      return;
    } catch (...) {
      fl_message("Unexpected error exporting %s", fname.c_str());
      return;
    }

    char msg[300];
    snprintf(msg, sizeof msg, "Done — exported %u pieces into one file.", (unsigned int)shapes.size());
    status->copy_label(msg);
    return;
  }

  /* build list of (shapeId, path) for pieces actually placed in the
   * selected solution's assembly                                        */
  struct SolPieceEntry { unsigned int shapeId = 0; std::string fname; };
  std::vector<SolPieceEntry> entries;

  for (unsigned int shapeId : shapeIds) {
    std::string raw = puzzle->getShape(shapeId)->getName();
    if (raw.empty()) {
      char buf[32];
      snprintf(buf, 32, "S%u", shapeId + 1);
      raw = buf;
    }

    SolPieceEntry e;
    e.shapeId = shapeId;
    e.fname   = (std::filesystem::path(folder) / (safeName(raw) + ".stl")).string();
    entries.push_back(e);
  }

  if (entries.empty()) {
    fl_message("No piece shapes to export for this solution.");
    return;
  }

  /* check for existing files once before starting */
  int existing = 0;
  for (size_t i = 0; i < entries.size(); i++)
    if (fileExists(entries[i].fname)) existing++;
  if (existing > 0) {
    if (fl_choice("%d output file(s) already exist. Overwrite?",
                  "Cancel", "Overwrite", 0, existing) == 0)
      return;
  }

  /* export */
  int exported = 0, errors = 0;

  for (size_t i = 0; i < entries.size(); i++) {
    voxel_c * v = puzzle->getShape(entries[i].shapeId);
    try {
      stl->write(entries[i].fname.c_str(), *v);
      exported++;
    } catch (stlException_c & e) {
      fl_message("Error exporting %s:\n%s", entries[i].fname.c_str(), e.comment);
      errors++;
    } catch (...) {
      fl_message("Unexpected error exporting %s", entries[i].fname.c_str());
      errors++;
    }
  }

  char msg[300];
  if (errors == 0)
    snprintf(msg, 300, "Done — exported %d piece type(s).", exported);
  else
    snprintf(msg, 300, "Done — exported %d piece type(s), %d error(s).", exported, errors);
  status->copy_label(msg);
}

/* ---------- constructor ------------------------------------------------ */

stlExportSolution_c::stlExportSolution_c(puzzle_c * p,
                                         const std::string & puzzleFile,
                                         unsigned int probIdx,
                                         unsigned int solIdx)
  : LFl_Double_Window(true), puzzle(p), prob(probIdx), sol(solIdx),
    exportDir(std::filesystem::path(puzzleFile).parent_path().string())
{
  /* the folder the puzzle was loaded from, else the home directory, as in
   * the single-shape STL export */
  if (exportDir.empty())
    exportDir = std::filesystem::path(homedir()).parent_path().string();

  label("Export Solution to STL");

  stl = std::unique_ptr<stlExporter_c>(p->getGridType()->getStlExporter());
  bt_assert(stl);

  std::map<unsigned int, unsigned int> placed = placedShapeCounts(p->getProblem(prob), sol);
  for (std::map<unsigned int, unsigned int>::const_iterator it = placed.begin(); it != placed.end(); ++it) {
    shapeIds.push_back(it->first);
    shapeCounts.push_back(it->second);
  }

  /* --- What to export (row 0) --- */
  {
    LFl_Frame * fr = new LFl_Frame(0, 0, 1, 1);

    ModeSeparate = new LFl_Radio_Button("Each piece type to its own file", 0, 0);
    ModeSeparate->tooltip(" Every distinct piece shape of the solution into a file of its own, in its own orientation ");
    ModeSeparate->callback(cb_Mode_stub, this);
    ModeAssembled = new LFl_Radio_Button("Assembled, all pieces in one file", 0, 1);
    ModeAssembled->tooltip(" All pieces into one file, each a separate solid in its place in the assembled solution ");
    ModeAssembled->callback(cb_Mode_stub, this);
    ModeSeparate->value(1);

    fr->end();
  }

  /* --- Output (row 1) --- */
  {
    LFl_Frame * fr = new LFl_Frame(0, 1, 1, 1);

    (new LFl_Box("File name", 0, 0))->stretchLeft();
    (new LFl_Box("Folder", 0, 1))->stretchLeft();

    (new LFl_Box(1, 0))->setMinimumSize(5, 0);
    (new LFl_Box(3, 0))->setMinimumSize(5, 0);

    /* the default name: the puzzle's file name and the solution number */
    std::string stem = std::filesystem::path(puzzleFile).stem().string();
    char defName[300];
    if (stem.empty())
      snprintf(defName, sizeof defName, "solution%u", sol + 1);
    else
      snprintf(defName, sizeof defName, "%s-solution%u", stem.c_str(), sol + 1);

    Fname = new LFl_Input(2, 0, 1, 1);
    Fname->value(defName);
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

  /* --- STL parameters (row 2) --- */
  {
    LFl_Frame * fr = new LFl_Frame(0, 2, 1, 1);

    for (unsigned int i = 0; i < stl->numParameters(); i++) {
      auto inp = std::make_unique<Param>();
      inp->type = stl->getParameterType(i);

      switch (inp->type) {
        case stlExporter_c::PAR_TYP_DOUBLE:
        case stlExporter_c::PAR_TYP_POS_DOUBLE: {
          (new LFl_Box(stl->getParameterName(i), 0, i))->stretchRight();
          (new LFl_Box(1, i))->setMinimumSize(5, 0);
          LFl_Float_Input * in = new LFl_Float_Input(2, i, 1, 1);
          char val[16]; snprintf(val, 16, "%2.2f", stl->getParameter(i));
          in->value(val); in->weight(1, 0);
          inp->w = in;
          break;
        }
        case stlExporter_c::PAR_TYP_POS_INTEGER: {
          (new LFl_Box(stl->getParameterName(i), 0, i))->stretchRight();
          (new LFl_Box(1, i))->setMinimumSize(5, 0);
          LFl_Int_Input * in = new LFl_Int_Input(2, i, 1, 1);
          char val[16]; snprintf(val, 16, "%i", (int)stl->getParameter(i));
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

  /* --- The pieces (row 3): the one to preview when each type gets its own
   * file --- */
  {
    LFl_Frame * fr = new LFl_Frame(0, 3, 1, 1);

    (new LFl_Box("Preview piece", 0, 0))->stretchLeft();
    (new LFl_Box(1, 0))->setMinimumSize(5, 0);

    PieceSelect = new LFl_Choice(2, 0, 1, 1);
    PieceSelect->weight(1, 0);
    unsigned int total = 0;
    for (size_t i = 0; i < shapeIds.size(); i++) {
      const std::string & name = p->getShape(shapeIds[i])->getName();
      char line[300];
      if (name.empty())
        snprintf(line, 300, "S%u  \xc3\x97%u", shapeIds[i] + 1, shapeCounts[i]);
      else
        snprintf(line, 300, "S%u  %s  \xc3\x97%u", shapeIds[i] + 1, name.c_str(), shapeCounts[i]);
      /* a menu item label: a slash would make a submenu */
      for (char * c = line; *c; c++) if (*c == '/') *c = '_';
      PieceSelect->add(line);
      total += shapeCounts[i];
    }
    if (!shapeIds.empty()) PieceSelect->value(0);
    PieceSelect->callback(cb_Update3DView_stub, this);

    char line[200];
    snprintf(line, sizeof line, "Solution %u: %u piece types, %u pieces", sol + 1, (unsigned int)shapeIds.size(), total);
    LFl_Box * info = new LFl_Box(0, 1, 3, 1);
    info->copy_label(line);
    info->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    fr->end();
  }

  /* --- Buttons (row 4) --- */
  {
    LFl_Frame * fr = new LFl_Frame(0, 4, 1, 1);

    layouter_c * o = new layouter_c(0, 0, 1, 1);
    o->pitch(5);

    LFl_Button * btn = new LFl_Button("Export STL", 0, 0, 1, 1);
    btn->callback(cb_Export_stub, this);

    (new LFl_Box(0, 1))->setMinimumSize(0, 5);

    btn = new LFl_Button("Close", 0, 2, 1, 1);
    btn->callback(cb_Abort_stub, this);

    o->end();
    fr->end();
  }

  /* --- Status line and view mode (row 5) --- */
  {
    layouter_c * l = new layouter_c(0, 5, 2, 1);

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
  }

  view3D = new LView3dGroup(1, 0, 1, 5);
  view3D->setMinimumSize(400, 400);
  view3D->weight(1, 0);
  /* the preview shows the exporter's own mesh: plain faces, whatever
   * style the main window is in (the edge-line style would draw a line
   * along every bevel boundary) */
  view3D->getView()->setRenderStyle(voxelFrame_c::styleVoxel);

  cb_Mode();

  set_modal();
}

/* ---------- destructor ------------------------------------------------- */

stlExportSolution_c::~stlExportSolution_c(void) = default;
