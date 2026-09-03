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

  /* first initialize the variables with their default value */
  config_data * t = first_data;

  while (t) {

    char command[200];

    snprintf(command, 200, "%s = %s", t->cnf_name, t->defaultValue);
    L.doString(command);

    t = t->next;
  }

  /* parse config file */
  open_local_config_file(L);

  /* read out data */
  t = first_data;

  while (t) {

    try {

      switch (t->cnf_typ) {
        case CT_BOOL:
          *(bool *)t->cnf_var = L.getBool(t->cnf_name);
          break;
          /*
             case CT_STRING:
             if (strlen(param) > 1) {
             param[strlen(param)-1] = '\0';
             strncpy((char *)t->cnf_var, param+1, t->maxlen);
             }
             break;
             */
        case CT_INT:
          *(int *)t->cnf_var = (int)L.getNumber(t->cnf_name);
          break;
        default: bt_assert(0);
      }
    }

    catch (...) {
    };
    t = t->next;
  }
}

void configuration_c::register_entry(const char *cnf_name, cnf_type cnf_typ, void *cnf_var, long maxlen, bool dialog, const char * dtext, const char * dhelp, const char * def) {

  config_data *t = new config_data;

  t->next = first_data;
  first_data = t;

  t->cnf_name = cnf_name;
  t->cnf_typ = cnf_typ;
  t->cnf_var = cnf_var;
  t->maxlen = maxlen;
  t->dialog = dialog;
  t->dialogText = dtext;
  t->dialogHelp = dhelp;

  t->defaultValue = def;
}

#define CNF_BOOL(a,b, def) register_entry(a, CT_BOOL, b, 0, false, 0, 0, def)
#define CNF_CHAR(a,b,c, def) register_entry(a, CT_STRING, b, c, false, 0, 0, def)
#define CNF_INT(a,b, def) register_entry(a, CT_INT, b, 0, false, 0, 0, def)

#define CNF_BOOL_D(a,b,text,help, def) register_entry(a, CT_BOOL, b, 0, true, text, help, def)
#define CNF_CHAR_D(a,b,c,text,help, def) register_entry(a, CT_STRING, b, c, true, text, help, def)
#define CNF_INT_D(a,b,text,help, def) register_entry(a, CT_INT, b, 0, true, text, help, def)

configuration_c::configuration_c(void) {

  first_data = 0;

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

  bt_assert(f);

  fseek(f, 0, SEEK_SET);

  config_data *t = first_data;

  while(t) {
    fprintf(f, "%s = ", t->cnf_name);

    switch (t->cnf_typ) {
    case CT_BOOL:
      fprintf(f, "%s", (*(bool *)t->cnf_var)?("true"):("false"));
      break;
    case CT_STRING:
      fprintf(f, "\"%s\"", (char *)(t->cnf_var));
      break;
    case CT_INT:
      fprintf(f, "%i", *(int *)t->cnf_var);
      break;
    default: bt_assert(0);
    }

    fprintf(f, "\n");

    t = t->next;
  }

  fclose(f);

  t = first_data;

  while (t) {
    t = t->next;
    delete first_data;
    first_data = t;
  }
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

  config_data *t = first_data;

  while (t) {
    if (t->dialog && t->cnf_typ == CT_BOOL && t->widget) {
      bool enable = (strcmp(t->defaultValue, "true") == 0);
      ((Fl_Check_Button*)t->widget)->value(enable ? 1 : 0);
    }
    t = t->next;
  }
}

void configuration_c::dialog(void) {

  static const int WINDOW_W = 500;
  static const int MARGIN = 16;
  static const int TEXT_PAD = 12;
  static const int TEXT_PAD_TOP = 8;
  const int wrapW = WINDOW_W - 2 * MARGIN - 2 * TEXT_PAD;

  LFl_Double_Window * win = new LFl_Double_Window(true);

  layouter_c * body = new layouter_c(0, 0, 1, 1);
  body->pitch(MARGIN);
  body->weight(1, 1);

  (new LFl_Box(0, 0, 1, 1))->setMinimumSize(0, TEXT_PAD_TOP);

  config_data *t = first_data;
  int y = 1;

  while(t) {
    if (t->dialog) {

      switch (t->cnf_typ) {
      case CT_BOOL:
        {
          LFl_Check_Button *w = new LFl_Check_Button(t->dialogText, 0, y, 1, 1);
          t->widget = w;
          w->value(*((bool*)t->cnf_var) ? 1 : 0);
          w->weight(1, 0);

          y++;

          if (t->dialogHelp && t->dialogHelp[0]) {
            (new LFl_Box(0, y, 1, 1))->setMinimumSize(0, TEXT_PAD_TOP);
            y++;

            layouter_c * helpRow = new layouter_c(0, y, 1, 1);
            helpRow->weight(1, 0);
            (new LFl_Box(0, 0))->setMinimumSize(TEXT_PAD, 0);
            new SettingsWrapBox(t->dialogHelp, 1, 0, wrapW);
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
      case CT_INT:
        break;
      default: bt_assert(0);
      }
    }

    t = t->next;
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
  btn->callback(cb_ConfigDialog_stub, win);
  btn->setMinimumSize(bw, th + 10);

  btnRow->end();
  body->end();

  win->end();
  win->label("Settings");

  win->set_modal();
  win->show();

  while (win->visible())
    Fl::wait();

  t = first_data;

  while(t) {

    if (t->dialog) {

      switch (t->cnf_typ) {
      case CT_BOOL:

        if (((Fl_Check_Button*)t->widget)->value())
          *((bool*)t->cnf_var) = true;
        else
          *((bool*)t->cnf_var) = false;
        break;
      case CT_STRING:
        break;
      case CT_INT:
        break;
      default: bt_assert(0);
      }
    }
    t = t->next;
  }

  delete win;
}

configuration_c config;
