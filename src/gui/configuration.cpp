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
#include "configuration.h"
#include <string.h>
#include <stdlib.h>
#include <algorithm>
#include <memory>
#include <ranges>
#include <thread>

#include "../lib/bt_assert.h"

#include "../tools/homedir.h"

#include "Layouter.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define GL_SILENCE_DEPRECATION 1
#include <FL/Fl.H>
#include <FL/filename.H>
#pragma GCC diagnostic pop

#include "../lua/luaclass.h"


static FILE *create_local_config_file(void) {

  char n[200];
  snprintf(n, 199, "%s.burrtools.rc", homedir().c_str());
  return fopen(n, "w");

}

static void open_local_config_file(luaClass_c & L) {

  char n[200];
  snprintf(n, 199, "%s.burrtools.rc", homedir().c_str());
  L.doFile(n);

}

void configuration_c::parse() {

  luaClass_c L;

  /* first initialize the variables with their default value
   * Note: in legacy BurrTools, register_entry prepended to a linked list,
   * so all traversals ran in reverse registration order. We preserve that
   * order across all operations (parsing, serialization, dialog layout).
   */
  for (const auto & t : std::views::reverse(data)) {
    char command[200];
    snprintf(command, 200, "%s = %s", t.cnf_name, t.defaultValue);
    L.doString(command);
  }

  /* parse config file */
  open_local_config_file(L);

  /* read out data */
  for (auto & t : std::views::reverse(data)) {
    try {
      switch (t.cnf_typ) {
        case CT_BOOL:
          *(bool *)t.cnf_var = L.getBool(t.cnf_name);
          break;
        case CT_INT:
          *(int *)t.cnf_var = (int)L.getNumber(t.cnf_name);
          break;
        default: bt_assert(0);
      }
    }
    catch (...) {
    }
  }
}

void configuration_c::register_entry(const char *cnf_name, cnf_type cnf_typ, void *cnf_var, long maxlen, bool dialog, const char * dtext, const char * dhelp, const char * def, int minVal, int maxVal) {
  data.push_back({cnf_name, cnf_typ, cnf_var, maxlen, dialog, dtext, dhelp, nullptr, def, minVal, maxVal});
}

/* std::thread::hardware_concurrency is the portable query on all three
 * platforms; it is allowed to return 0 when it can not tell, so fall back to 1
 */
unsigned int configuration_c::maxThreads(void) {
  unsigned int hw = std::thread::hardware_concurrency();
  return hw ? hw : 1;
}

unsigned int configuration_c::solverThreads(void) const {
  return (unsigned int)std::clamp(i_solver_threads, 1, (int)maxThreads());
}

#define CNF_BOOL(a,b, def) register_entry(a, CT_BOOL, b, 0, false, 0, 0, def)
#define CNF_CHAR(a,b,c, def) register_entry(a, CT_STRING, b, c, false, 0, 0, def)
#define CNF_INT(a,b, def) register_entry(a, CT_INT, b, 0, false, 0, 0, def)

#define CNF_BOOL_D(a,b,text,help, def) register_entry(a, CT_BOOL, b, 0, true, text, help, def)
#define CNF_CHAR_D(a,b,c,text,help, def) register_entry(a, CT_STRING, b, c, true, text, help, def)
#define CNF_INT_D(a,b,text,help, def, lo, hi) register_entry(a, CT_INT, b, 0, true, text, help, def, lo, hi)

configuration_c::configuration_c(void) {

  /* default to 60% of the cores, rounded down, but never below one: leaves
   * the machine responsive while solving
   */
  const unsigned int maxThr = maxThreads();
  i_solver_threads_default = std::to_string(std::max(1u, (unsigned int)(maxThr * 6 / 10)));

  /* registered first so that it ends up last in the dialogue, which walks
   * `data` in reverse (see parse())
   */
  CNF_INT_D("solverthreads",      &i_solver_threads, "Solver Threads",
            "Number of worker threads the assembler and the disassembler use while solving. "
            "Fewer threads leave more of the machine free for other work; more threads solve faster. "
            "1 disables parallel solving.",
            i_solver_threads_default.c_str(), 1, (int)maxThr);

  CNF_BOOL_D("tooltips",          &i_use_tooltips, "Use Tooltips",
             "Show short help text when the mouse rests on buttons and other controls.",
             "true");
  CNF_BOOL_D("lightning",         &i_use_lightning, "Use Lights in 3D View",
             "Light the 3D preview so pieces look solid and shaded. Turn this off for a flatter, unlit appearance.",
             "true");
  CNF_BOOL_D("fadeout",           &i_use_blendedRemoving, "Fade Out Pieces",
             "During a disassembly animation, pieces that have been removed fade away instead of vanishing at once.",
             "true");
  CNF_BOOL_D("displaylists",      &i_use_displayLists, "Use openGL display lists",
             "Cache 3D geometry in OpenGL display lists. This can speed up redraws on some systems, but it is off by default because it can cause display problems.",
             "false");
  CNF_BOOL_D("rotator",           &i_rotationMethod, "Use new rotation method",
             "Use the newer click-and-drag method for rotating the 3D preview. Turn this off to use the original arc-ball rotation.",
             "true");
  CNF_BOOL_D("reversescrollzoom", &i_reverseScrollZoom, "Reverse scroll zoom direction",
             "Reverse the direction of the preview zoom when the mouse wheel is used.",
             "false");
  CNF_BOOL("rubberband",          &i_use_rubberband, "false");
  CNF_INT("renderstyle",          &i_render_style, "0");
  CNF_INT("windowposx",           &i_window_pos_x, "30");
  CNF_INT("windowposy",           &i_window_pos_y, "30");
  CNF_INT("windowposw",           &i_window_pos_w, "800");
  CNF_INT("windowposh",           &i_window_pos_h, "600");

  parse();
}

configuration_c::~configuration_c(void) {

  FILE * f = create_local_config_file();
  if (!f) return;

  fseek(f, 0, SEEK_SET);

  for (const auto & t : std::views::reverse(data)) {
    fprintf(f, "%s = ", t.cnf_name);

    switch (t.cnf_typ) {
    case CT_BOOL:
      fprintf(f, "%s", (*(bool *)t.cnf_var)?("true"):("false"));
      break;
    case CT_STRING:
      fprintf(f, "\"%s\"", (char *)(t.cnf_var));
      break;
    case CT_INT:
      fprintf(f, "%i", *(int *)t.cnf_var);
      break;
    default: bt_assert(0);
    }

    fprintf(f, "\n");
  }

  fclose(f);
}

static void cb_ConfigDialog_stub(Fl_Widget* /*o*/, void* v) { ((Fl_Double_Window*)v)->hide(); }

static void cb_RestoreDefaults_stub(Fl_Widget* /*o*/, void* v) {
  ((configuration_c*)v)->restoreDialogDefaults();
}

class SettingsWrapBox : public LFl_Box {

  int wrapW;

public:

  SettingsWrapBox(const char *txt, int x, int y, int wrapWidth)
    : LFl_Box(txt, x, y, 1, 1), wrapW(wrapWidth) {
    align(FL_ALIGN_WRAP | FL_ALIGN_TOP_LEFT | FL_ALIGN_INSIDE);
    labelcolor(FL_INACTIVE_COLOR);
    weight(1, 0);
  }

  virtual void getMinSize(int *width, int *height) const {
    int ww = wrapW;
    int hh = 0;
    fl_font(labelfont(), labelsize());
    if (label() && label()[0])
      fl_measure(label(), ww, hh);
    *width = wrapW;
    *height = hh;
  }
};

void configuration_c::restoreDialogDefaults(void) {

  for (auto & t : std::views::reverse(data)) {
    if (!t.dialog || !t.widget) continue;

    if (t.cnf_typ == CT_BOOL) {
      bool enable = (strcmp(t.defaultValue, "true") == 0);
      ((Fl_Check_Button*)t.widget)->value(enable ? 1 : 0);
    } else if (t.cnf_typ == CT_INT) {
      ((Fl_Value_Slider*)t.widget)->value(atoi(t.defaultValue));
    }
  }
}

void configuration_c::dialog(void) {

  static const int WINDOW_W = 500;
  static const int MARGIN = 16;
  static const int TEXT_PAD = 12;
  static const int TEXT_PAD_TOP = 8;
  const int wrapW = WINDOW_W - 2 * MARGIN - 2 * TEXT_PAD;

  auto win = std::make_unique<LFl_Double_Window>(true);

  layouter_c * body = new layouter_c(0, 0, 1, 1);
  body->pitch(MARGIN);
  body->weight(1, 1);

  (new LFl_Box(0, 0, 1, 1))->setMinimumSize(0, TEXT_PAD_TOP);

  int y = 1;

  for (auto & t : std::views::reverse(data)) {
    if (t.dialog) {

      switch (t.cnf_typ) {
      case CT_BOOL:
        {
          LFl_Check_Button *w = new LFl_Check_Button(t.dialogText, 0, y, 1, 1);
          t.widget = w;
          w->value(*((bool*)t.cnf_var) ? 1 : 0);
          w->weight(1, 0);

          y++;

          if (t.dialogHelp && t.dialogHelp[0]) {
            (new LFl_Box(0, y, 1, 1))->setMinimumSize(0, TEXT_PAD_TOP);
            y++;

            layouter_c * helpRow = new layouter_c(0, y, 1, 1);
            helpRow->weight(1, 0);
            (new LFl_Box(0, 0))->setMinimumSize(TEXT_PAD, 0);
            new SettingsWrapBox(t.dialogHelp, 1, 0, wrapW);
            (new LFl_Box(2, 0))->setMinimumSize(TEXT_PAD, 0);
            helpRow->end();
            y++;
          }

          LFl_Box * spacer = new LFl_Box(0, y, 1, 1);
          spacer->setMinimumSize(wrapW, 8);
          y++;
        }
        break;
      case CT_INT:
        {
          layouter_c * row = new layouter_c(0, y, 1, 1);
          row->weight(1, 0);

          LFl_Box * lbl = new LFl_Box(t.dialogText, 0, 0, 1, 1);
          lbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

          (new LFl_Box(1, 0))->setMinimumSize(TEXT_PAD, 0);

          LFl_Value_Slider * w = new LFl_Value_Slider(2, 0, 1, 1);
          w->type(FL_HOR_NICE_SLIDER);
          w->box(FL_THIN_DOWN_BOX);
          w->bounds(t.minVal, t.maxVal);
          w->step(1);
          w->value(std::clamp(*((int*)t.cnf_var), t.minVal, t.maxVal));
          w->weight(1, 0);
          w->setMinimumSize(120, 22);
          if (t.minVal >= t.maxVal) w->deactivate();   // single-core machine: nothing to choose
          t.widget = w;

          row->end();
          y++;

          if (t.dialogHelp && t.dialogHelp[0]) {
            (new LFl_Box(0, y, 1, 1))->setMinimumSize(0, TEXT_PAD_TOP);
            y++;

            layouter_c * helpRow = new layouter_c(0, y, 1, 1);
            helpRow->weight(1, 0);
            (new LFl_Box(0, 0))->setMinimumSize(TEXT_PAD, 0);
            new SettingsWrapBox(t.dialogHelp, 1, 0, wrapW);
            (new LFl_Box(2, 0))->setMinimumSize(TEXT_PAD, 0);
            helpRow->end();
            y++;
          }

          LFl_Box * spacer = new LFl_Box(0, y, 1, 1);
          spacer->setMinimumSize(wrapW, 8);
          y++;
        }
        break;
      case CT_STRING:
        break;
      default: bt_assert(0);
      }
    }
  }

  layouter_c * btnRow = new layouter_c(0, y, 1, 1);
  btnRow->pitch(5);

  LFl_Box * leftPad = new LFl_Box(0, 0);
  leftPad->weight(1, 0);

  int tw = 0, th = 0;
  fl_font(FL_HELVETICA, FL_NORMAL_SIZE);
  fl_measure("Restore Defaults", tw, th);
  int bw = 2 * (tw + 4);

  LFl_Button * restore = new LFl_Button("Restore Defaults", 1, 0);
  restore->tooltip("Restore all settings to their default values");
  restore->callback(cb_RestoreDefaults_stub, this);
  restore->setMinimumSize(bw, th + 10);

  (new LFl_Box(2, 0))->setMinimumSize(12, 0);

  LFl_Button * btn = new LFl_Button("Close", 3, 0);
  btn->tooltip("Close window");
  btn->callback(cb_ConfigDialog_stub, win.get());
  btn->setMinimumSize(bw, th + 10);

  btnRow->end();
  body->end();

  win->end();
  win->label("Settings");

  win->set_modal();
  win->show();

  while (win->visible())
    Fl::wait();

  for (auto & t : std::views::reverse(data)) {
    if (t.dialog) {

      switch (t.cnf_typ) {
      case CT_BOOL:

        if (((Fl_Check_Button*)t.widget)->value())
          *((bool*)t.cnf_var) = true;
        else
          *((bool*)t.cnf_var) = false;
        break;
      case CT_INT:
        *((int*)t.cnf_var) = std::clamp((int)((Fl_Value_Slider*)t.widget)->value(), t.minVal, t.maxVal);
        break;
      case CT_STRING:
        break;
      default: bt_assert(0);
      }
    }
  }
}

configuration_c config;
