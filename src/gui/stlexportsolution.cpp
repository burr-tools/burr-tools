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
#include "separator.h"

#include "../lib/puzzle.h"
#include "../lib/problem.h"
#include "../lib/solution.h"
#include "../lib/assembly.h"
#include "../lib/gridtype.h"
#include "../lib/voxel.h"
#include "../lib/bt_assert.h"

#include <stdlib.h>
#include <string.h>
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
                          const std::vector<Param*> & params)
  {
    for (unsigned int i = 0; i < stl->numParameters(); i++) {
      switch (params[i]->type) {
        case stlExporter_c::PAR_TYP_DOUBLE:
        case stlExporter_c::PAR_TYP_POS_DOUBLE:
          stl->setParameter(i, atof(((LFl_Float_Input*)(params[i]->w))->value()));
          break;
        case stlExporter_c::PAR_TYP_POS_INTEGER:
          stl->setParameter(i, atoi(((LFl_Int_Input*)(params[i]->w))->value()));
          break;
        case stlExporter_c::PAR_TYP_SWITCH:
          stl->setParameter(i, ((LFl_Check_Button*)(params[i]->w))->value());
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

} // namespace stlExportSolutionImpl

using namespace stlExportSolutionImpl;

/* ---------- callbacks -------------------------------------------------- */

static void cb_Export_stub    (Fl_Widget*, void* v) { ((stlExportSolution_c*)v)->cb_Export(); }
static void cb_Abort_stub     (Fl_Widget*, void* v) { ((stlExportSolution_c*)v)->cb_Abort(); }
static void cb_FileChooser_stub(Fl_Widget*, void* v){ ((stlExportSolution_c*)v)->cb_FileChooser(); }

void stlExportSolution_c::cb_Abort(void) { hide(); }

void stlExportSolution_c::cb_FileChooser(void)
{
  const char * f = fl_file_chooser(
      "Choose output folder (navigate into it and pick any file)",
      "", Pname->value(), 0);

  if (f) {
    const char * div = strrchr(f, '/');
    if (div) {
      char dir[500];
      int len = (int)(div - f);
      if (len >= 499) len = 498;
      strncpy(dir, f, len);
      dir[len] = 0;
      Pname->value(dir);
    }
  }
}

void stlExportSolution_c::cb_Export(void)
{
  applyParams(stl, params);
  stl->setBinaryMode(Binary->value() != 0);

  problem_c * pr = puzzle->getProblem(prob);

  const char * folder = Pname->value();
  if (!folder || folder[0] == '\0') folder = ".";

  /* build list of (shapeId, count, path) for pieces actually placed in
   * the selected solution's assembly                                    */
  struct Entry { unsigned int shapeId; unsigned int count; std::string fname; };
  std::vector<Entry> entries;

  std::map<unsigned int, unsigned int> placed = placedShapeCounts(pr, sol);
  for (std::map<unsigned int, unsigned int>::const_iterator it = placed.begin();
       it != placed.end(); ++it) {
    unsigned int shapeId = it->first;
    unsigned int count   = it->second;

    const voxel_c * v = puzzle->getShape(shapeId);
    std::string raw = v->getName();
    if (raw.empty()) {
      char buf[32];
      snprintf(buf, 32, "S%u", shapeId + 1);
      raw = buf;
    }

    char pathbuf[1200];
    if (folder[strlen(folder)-1] != '/')
      snprintf(pathbuf, 1200, "%s/%s.stl", folder, safeName(raw).c_str());
    else
      snprintf(pathbuf, 1200, "%s%s.stl",  folder, safeName(raw).c_str());

    Entry e;
    e.shapeId = shapeId;
    e.count   = count;
    e.fname   = pathbuf;
    entries.push_back(e);
  }

  if (entries.empty()) {
    fl_message("No piece shapes to export for this solution.");
    return;
  }

  /* check for existing files once before starting */
  int existing = 0;
  for (size_t i = 0; i < entries.size(); i++) {
    FILE * f = fopen(entries[i].fname.c_str(), "r");
    if (f) { fclose(f); existing++; }
  }
  if (existing > 0) {
    if (fl_choice("%d output file(s) already exist. Overwrite?",
                  "Cancel", "Overwrite", 0, existing) == 0)
      return;
  }

  /* export */
  int exported = 0, errors = 0;
  faceList_c holes; /* empty — batch export has no manually-marked holes */

  for (size_t i = 0; i < entries.size(); i++) {
    voxel_c * v = puzzle->getShape(entries[i].shapeId);
    try {
      stl->write(entries[i].fname.c_str(), *v, holes);
      exported++;
    } catch (stlException_c e) {
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
                                         unsigned int probIdx,
                                         unsigned int solIdx)
  : LFl_Double_Window(false), puzzle(p), prob(probIdx), sol(solIdx)
{
  label("Export Solution to STL");

  stl = p->getGridType()->getStlExporter();
  bt_assert(stl);

  /* --- STL parameters frame (row 0) --- */
  {
    LFl_Frame * fr = new LFl_Frame(0, 0, 1, 1);

    for (unsigned int i = 0; i < stl->numParameters(); i++) {
      Param * inp = new Param;
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
      inp->w->tooltip(stl->getParameterTooltip(i));
      params.push_back(inp);
    }

    fr->end();
  }

  /* --- Output folder frame (row 1) --- */
  {
    LFl_Frame * fr = new LFl_Frame(0, 1, 1, 1);

    (new LFl_Box("Output folder", 0, 0))->stretchLeft();
    (new LFl_Box(1, 0))->setMinimumSize(5, 0);

    Pname = new LFl_Input(2, 0, 1, 1);
    Pname->value(".");
    Pname->weight(1, 0);
    Pname->setMinimumSize(80, 0);

    (new LFl_Button("...", 3, 0, 1, 1))->callback(cb_FileChooser_stub, this);

    Binary = new LFl_Check_Button("Binary STL", 0, 1, 4, 1);
    Binary->value(stl->getBinaryMode() ? 1 : 0);

    fr->end();
  }

  /* --- Piece list info box (row 2) --- */
  {
    problem_c * pr = p->getProblem(prob);

    std::string info;
    char line[300];

    snprintf(line, 300, "Pieces placed in solution %u:\n", sol + 1);
    info += line;

    /* use actual assembly placements rather than the problem's max counts */
    std::map<unsigned int, unsigned int> placed = placedShapeCounts(pr, sol);

    unsigned int total = 0;
    for (std::map<unsigned int, unsigned int>::const_iterator it = placed.begin();
         it != placed.end(); ++it) {
      unsigned int shapeId = it->first;
      unsigned int count   = it->second;

      const voxel_c * v = p->getShape(shapeId);
      const std::string & name = v->getName();

      if (name.empty())
        snprintf(line, 300, "  S%u  \xc3\x97%u\n", shapeId + 1, count);
      else
        snprintf(line, 300, "  S%u  %s  \xc3\x97%u\n", shapeId + 1, name.c_str(), count);

      info += line;
      total += count;
    }

    snprintf(line, 300, "Total instances: %u", total);
    info += line;

    LFl_Box * infoBox = new LFl_Box("", 0, 2, 1, 1);
    infoBox->copy_label(info.c_str());
    infoBox->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_TOP);
    infoBox->box(FL_ENGRAVED_BOX);
    infoBox->setMinimumSize(0, 60);
    infoBox->pitch(5);
    infoBox->weight(1, 1);
  }

  /* --- Status line (row 3) --- */
  {
    status = new LFl_Box("", 0, 3, 1, 1);
    status->box(FL_UP_BOX);
    status->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    status->setMinimumSize(0, 20);
    status->pitch(3);
    status->weight(1, 0);
  }

  /* --- Buttons (row 4) --- */
  {
    layouter_c * o = new layouter_c(0, 4);
    o->pitch(5);

    LFl_Button * btn = new LFl_Button("Export All", 0, 0, 1, 1);
    btn->callback(cb_Export_stub, this);
    btn->weight(1, 0);

    (new LFl_Box(1, 0))->setMinimumSize(5, 0);

    btn = new LFl_Button("Close", 2, 0, 1, 1);
    btn->callback(cb_Abort_stub, this);
    btn->weight(1, 0);

    o->end();
  }

  set_modal();
}

/* ---------- destructor ------------------------------------------------- */

stlExportSolution_c::~stlExportSolution_c(void)
{
  if (stl) delete stl;
  for (size_t i = 0; i < params.size(); i++)
    delete params[i];
}
