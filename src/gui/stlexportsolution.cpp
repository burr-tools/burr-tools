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

#include "../lib/puzzle.h"
#include "../lib/problem.h"
#include "../lib/solution.h"
#include "../lib/assembly.h"
#include "../lib/voxel.h"
#include "../lib/bt_assert.h"

#include "../halfedge/polyhedron.h"
#include "../halfedge/face.h"
#include "../halfedge/halfedge.h"
#include "../halfedge/vertex.h"
#include "../halfedge/volume.h"

#include "../tools/fileexists.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define GL_SILENCE_DEPRECATION 1
#include <FL/Fl.H>
#include <FL/fl_ask.H>
#include <FL/Fl_File_Chooser.H>
#pragma GCC diagnostic pop

namespace {

  /* Return a filesystem-safe version of a shape name: replace anything
   * that isn't alphanumeric, '-', or '_' with '_'.                      */
  std::string safeName(const std::string & s)
  {
    std::string out = s;
    for (size_t i = 0; i < out.size(); i++) {
      char c = out[i];
      if (!isalnum((unsigned char)c) && c != '-' && c != '_')
        out[i] = '_';
    }
    return out;
  }

  /* The file names, without folder and extension, of the per-piece export:
   * one per entry of names, the shapes' names, numbers being their shape
   * numbers.
   *
   * A file is named after its shape as in 0.8.0: the shape's name made
   * safe, or S<n> for a shape without one. Two shapes can come out as the
   * same file, though: safeName() maps "a b" and "a_b" to one name, and a
   * file system may ignore case. Named shapes that clash get their shape
   * number in front (S<n>_<name>), which keeps them apart.               */
  std::vector<std::string> pieceFileNames(const std::vector<std::string> & names,
                                          const std::vector<unsigned int> & numbers)
  {
    auto key = [](std::string s) {
      for (char & c : s) c = (char)tolower((unsigned char)c);
      return s;
    };
    auto num = [&numbers](size_t i) { return "S" + std::to_string(numbers[i]); };

    std::vector<std::string> out;
    std::map<std::string, int> count;
    for (size_t i = 0; i < names.size(); i++) {
      out.push_back(names[i].empty() ? num(i) : safeName(names[i]));
      count[key(out[i])]++;
    }

    for (size_t i = 0; i < names.size(); i++)
      if (!names[i].empty() && count[key(out[i])] > 1)
        out[i] = num(i) + "_" + out[i];

    /* a name can still be taken when a shape is called like another one's
     * numbered name ("S5_a"); the later one gets its number in front until
     * it is free */
    std::set<std::string> used;
    for (size_t i = 0; i < out.size(); i++) {
      while (used.count(key(out[i])))
        out[i] = num(i) + "_" + out[i];
      used.insert(key(out[i]));
    }

    return out;
  }

  /* A menu item label that shows text as it is: Fl_Menu_::add() splits
   * at '/' and takes '\\' as an escape, and the menu draws '&' as a
   * shortcut underline.                                                 */
  std::string menuLabel(const std::string & text)
  {
    std::string out;
    for (char c : text) {
      if (c == '/' || c == '\\') out += '\\';
      else if (c == '&') out += '&';
      out += c;
    }
    return out;
  }

  /* Build a map of shapeId -> count for the pieces actually placed in
   * the given solution's assembly (not just the parts defined in the
   * problem with non-zero max counts).                                  */
  std::map<unsigned int, unsigned int> placedShapeCounts(
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

  /* Append a copy of all faces of src to dst. Every piece keeps vertices
   * of its own, even where it touches another piece (no gap), so the
   * pieces stay separate solids in the combined preview mesh.            */
  void appendPolyhedron(Polyhedron & dst, const Polyhedron & src)
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

} // namespace

static void cb_Mode_stub(Fl_Widget*, void* v) { ((stlExportSolution_c*)v)->cb_Mode(); }
static void cb_PieceSelect_stub(Fl_Widget*, void* v) { ((stlExportSolution_c*)v)->cb_NewPreviewSubject(); }

bool stlExportSolution_c::assembled(void) const { return ModeAssembled->value() != 0; }

void stlExportSolution_c::activateModeFields(void)
{
  if (assembled()) {
    Fname->activate();
    PieceSelect->deactivate();
  } else {
    Fname->deactivate();
    PieceSelect->activate();
  }
}

void stlExportSolution_c::cb_Mode(void)
{
  activateModeFields();
  cb_NewPreviewSubject();
}

void stlExportSolution_c::cb_FileChooser(void)
{
  if (assembled()) {
    stlExportDialog_c::cb_FileChooser();
    return;
  }

  const char * f = fl_file_chooser(
      "Choose output folder (navigate into it and pick any file)",
      "", folder().c_str(), 0);
  if (!f) return;

  Pname->value(std::filesystem::path(f).parent_path().string().c_str());
}

std::unique_ptr<Polyhedron> stlExportSolution_c::previewMesh(std::string & text)
{
  char txt[100];

  if (assembled()) {
    if (pieceSpaces.empty()) {
      text = "This solution has no assembly to export";
      return nullptr;
    }

    std::unique_ptr<Polyhedron> all = std::make_unique<Polyhedron>();
    double vol = 0;
    for (const std::unique_ptr<voxel_c> & s : pieceSpaces) {
      std::unique_ptr<Polyhedron> p = mesh(*s);
      vol += volume(*p);
      appendPolyhedron(*all, *p);
    }
    all->finalize();

    snprintf(txt, sizeof txt, "%u pieces, volume: %1.1f cubic-units", (unsigned int)pieceSpaces.size(), vol);
    text = txt;
    return all;
  }

  if (shapeIds.empty()) {
    text = "No piece shapes to export for this solution";
    return nullptr;
  }

  std::unique_ptr<Polyhedron> p = mesh(*puzzle->getShape(shapeIds[PieceSelect->value()]));
  snprintf(txt, sizeof txt, "Volume: %1.1f cubic-units", volume(*p));
  text = txt;
  return p;
}

void stlExportSolution_c::cb_Export(void)
{
  applyParameters();

  if (assembled())
    exportAssembled();
  else
    exportSeparate();
}

void stlExportSolution_c::exportAssembled(void)
{
  const std::string fname = outputFile();
  if (fname.empty()) {
    fl_message("Please enter a file name.");
    return;
  }

  if (pieceSpaces.empty()) {
    fl_message("This solution has no assembly to export.");
    return;
  }

  if (!mayWrite(fname)) return;

  std::vector<const voxel_c *> shapes;
  for (const std::unique_ptr<voxel_c> & s : pieceSpaces) shapes.push_back(s.get());

  if (!write(fname, shapes)) return;

  char msg[300];
  snprintf(msg, sizeof msg, "Done — exported %u pieces into one file.", (unsigned int)shapes.size());
  status->copy_label(msg);
}

void stlExportSolution_c::exportSeparate(void)
{
  if (shapeIds.empty()) {
    fl_message("No piece shapes to export for this solution.");
    return;
  }

  /* one file per piece type, named after its shape */
  std::vector<std::string> names;
  std::vector<unsigned int> numbers;
  for (unsigned int shapeId : shapeIds) {
    names.push_back(puzzle->getShape(shapeId)->getName());
    numbers.push_back(shapeId + 1);
  }

  std::vector<std::string> fnames;
  for (const std::string & name : pieceFileNames(names, numbers))
    fnames.push_back((std::filesystem::path(folder()) / (name + ".stl")).string());

  /* check for existing files once before starting */
  int existing = 0;
  for (const std::string & f : fnames)
    if (fileExists(f)) existing++;
  if (existing > 0 &&
      fl_choice("%d output file(s) already exist. Overwrite?", "Cancel", "Overwrite", 0, existing) == 0)
    return;

  int exported = 0, errors = 0;
  for (size_t i = 0; i < shapeIds.size(); i++) {
    if (write(fnames[i], { puzzle->getShape(shapeIds[i]) }))
      exported++;
    else
      errors++;
  }

  char msg[300];
  if (errors == 0)
    snprintf(msg, sizeof msg, "Done — exported %d piece type(s).", exported);
  else
    snprintf(msg, sizeof msg, "Done — exported %d piece type(s), %d error(s).", exported, errors);
  status->copy_label(msg);
}

stlExportSolution_c::stlExportSolution_c(puzzle_c * p,
                                         const std::string & puzzleFile,
                                         unsigned int prob,
                                         unsigned int sol)
  : stlExportDialog_c(p, puzzleFile, "Export Solution to STL")
{
  const problem_c * pr = p->getProblem(prob);

  std::map<unsigned int, unsigned int> placed = placedShapeCounts(pr, sol);
  for (std::map<unsigned int, unsigned int>::const_iterator it = placed.begin(); it != placed.end(); ++it) {
    shapeIds.push_back(it->first);
    shapeCounts.push_back(it->second);
  }

  /* the pieces can't change while the modal dialog is open */
  const solution_c * sav = pr ? pr->getSavedSolution(sol) : nullptr;
  if (sav && sav->getAssembly())
    pieceSpaces = sav->getAssembly()->createPieceSpaces(*pr);

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

  /* --- Output (row 1): the default name is the puzzle's file name and
   * the solution number --- */
  {
    std::string stem = std::filesystem::path(puzzleFile).stem().string();
    char defName[300];
    if (stem.empty())
      snprintf(defName, sizeof defName, "solution%u", sol + 1);
    else
      snprintf(defName, sizeof defName, "%s-solution%u", stem.c_str(), sol + 1);

    addOutputFrame(1, defName);
  }

  addParameterFrame(2);

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
      PieceSelect->add(menuLabel(line).c_str());
      total += shapeCounts[i];
    }
    if (!shapeIds.empty()) PieceSelect->value(0);
    PieceSelect->callback(cb_PieceSelect_stub, this);

    char line[200];
    snprintf(line, sizeof line, "Solution %u: %u piece types, %u pieces", sol + 1, (unsigned int)shapeIds.size(), total);
    LFl_Box * info = new LFl_Box(0, 1, 3, 1);
    info->copy_label(line);
    /* at its own width, like the other labels: an inside-aligned label is
     * drawn with a margin its minimum size doesn't count, and ran into the
     * frame's right edge */
    info->stretchLeft();

    fr->end();
  }

  addButtons(4);
  addStatusAndPreview(5);

  activateModeFields();

  set_modal();
}

stlExportSolution_c::~stlExportSolution_c(void) = default;
