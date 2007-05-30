/* Burr Solver
 * Copyright (C) 2003-2007  Andreas Röver
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
#include "groupseditor.h"
#include "piececolor.h"
#include "WindowWidgets.h"

#include "../lib/voxel.h"
#include "../lib/puzzle.h"

#include "Fl_Table.h"

#include <FL/fl_draw.H>
#include <FL/Fl_Int_Input.H>

/* this is the groups editor table
 */
class groupsEditorTab_c : public Fl_Table, public layoutable_c {

  /* the puzzle and the problem to edit */
  puzzle_c * puzzle;
  unsigned int prob;

  /* the input line where the user inputs the value
   * it is places over the cell the user clicks onto
   */
  Fl_Int_Input* input;

  /* the position inside the table where the input line is
   */
  int editShape, editGroup;

  /* the current maximum group number
   */
  unsigned int maxGroup;

  /* was something changed? */
  bool changed;

  /* this is the virtual function called by the table widget to
   * draw a single cell
   */
  void draw_cell(TableContext context, int r = 0, int c = 0,
                 int x = 0, int y = 0, int w = 0, int h = 0);

public:

  groupsEditorTab_c(int x, int y, int w, int h, puzzle_c * puzzle, unsigned int problem);

  /* add a group to the puzzle and a column to the table
   */
  void addGroup(void);

  void cb_input(void);
  void cb_tab(void);

  bool getChanged(void) { return changed; }

  /* take over the currently edited value (if there is one) and
   * close the edit line
   */
  void finishEdit(void);

  virtual void getMinSize(int *width, int *height) const {
    *width = 300;
    *height = 200;
  }

};

/* draw a cell of the table */
void groupsEditorTab_c::draw_cell(TableContext context, int r, int c, int x, int y, int w, int h) {

  switch(context) {

  case CONTEXT_STARTPAGE:
    return;

  case CONTEXT_ROW_HEADER:
  case CONTEXT_COL_HEADER:
    fl_push_clip(x, y, w, h);
    {
      /* draw the header */

      /* clear the background and paint a raised box */
      fl_draw_box(FL_UP_BOX, x, y, w, h, color());

      /* create the text of the cell */
      static char s[40];

      /* first column is the shape columns
       * second contains the number of that shape that is in the puzzle
       * the other columns are group columns
       */
      if (c == 0)
        snprintf(s, 40, "Shape");
      else if (c == 1)
        snprintf(s, 40, "Min");
      else if (c == 2)
        snprintf(s, 40, "Max");
      else
        snprintf(s, 40, "Gr %i", c-2);

      /* output the text */
      fl_color(FL_BLACK);
      fl_draw(s, x, y, w, h, FL_ALIGN_CENTER);

    }
    fl_pop_clip();
    return;

  case CONTEXT_CELL:

    // don't redraw the cell where the input line is activated
    if ((c == editGroup+1) && (r == editShape) && (input->visible())) return;

    fl_push_clip(x, y, w, h);
    {
      /* this will contain the text to display */
      static char s[40];

      /* this contains what to draw, 0: empty cell
       * 1: the string s inside a coloured cell
       */
      int type = 0;

      if (c == 0) {

        /* the shape column, find out the shape number and display that one */

        type = 1;

        int num = puzzle->probGetShape(prob, r);
        if (puzzle->getShape(num)->getName().length())
          snprintf(s, 40, " S%i - %s", num+1, puzzle->getShape(num)->getName().c_str());
        else
          snprintf(s, 40, " S%i", num+1);

      } else if (c == 1) {

        /* the 2nd column, display the min count for this shape */

        type = 1;
        snprintf(s, 40, "%i", puzzle->probGetShapeMin(prob, r));

      } else if (c == 2) {

        /* the 3rd column, display the max count for this shape */

        type = 1;
        snprintf(s, 40, "%i", puzzle->probGetShapeMax(prob, r));

      } else {

        /* the group columns, only display, when an entry for this
         * group exists
         */

        for (unsigned int j = 0; j < puzzle->probGetShapeGroupNumber(prob, r); j++)
          if (puzzle->probGetShapeGroup(prob, r, j) == (c - 2)) {
            type = 1;
            snprintf(s, 40, "%i", puzzle->probGetShapeGroupCount(prob, r, j));
            break;
          }

      }

      /* draw the cell */
      switch (type) {
        case 0:

          /* an empty cell */
          fl_color(color());
          fl_rectf(x, y, w, h);

          break;

        case 1:

          /* a cell with a background corresponding with the shape colour */
          {
            /* get the colour for the cell */
            int p = puzzle->probGetShape(prob, r);
            unsigned char r, g, b;
            r = pieceColorRi(p);
            g = pieceColorGi(p);
            b = pieceColorBi(p);

            /* draw background */
            fl_color(r, g, b);
            fl_rectf(x, y, w, h);
            if ((int)3*r + 6*g + 1*b > 1275)
              fl_color(0, 0, 0);
            else
              fl_color(255, 255, 255);

            /* draw the text */
            if (c == 0)
              fl_draw(s, x, y, w, h, FL_ALIGN_LEFT);
            else
              fl_draw(s, x, y, w, h, FL_ALIGN_CENTER);
          }

          break;
      }

      /* draw a black frame around the cell to make it visible */
      fl_color(FL_BLACK);
      fl_rect(x, y, w, h);
    }

    fl_pop_clip();
    return;

  default:

    return;

  }
}

/* called when the input line has been completed */
static void cb_input_stub(Fl_Widget*, void* v) { ((groupsEditorTab_c*)v)->cb_input(); }
void groupsEditorTab_c::cb_input(void) {

  /* we have either changed the count for the shape, or the group count */
  if (editGroup == 0)
    puzzle->probSetShapeMin(prob, editShape, atoi(input->value()));
  else if (editGroup == 1)
    puzzle->probSetShapeMax(prob, editShape, atoi(input->value()));
  else
    puzzle->probSetShapeGroup(prob, editShape, editGroup-1, atoi(input->value()));

  /* hide the edit line */
  input->hide();
  redraw();
  changed = true;

  do_callback(CONTEXT_NONE, 0, 0);
}

/* handle mouse events for the tables */
static void cb_tab_stub(Fl_Widget*, void *v) { ((groupsEditorTab_c*)v)->cb_tab(); }
void groupsEditorTab_c::cb_tab(void)
{
  int row = callback_row();
  int col = callback_col();
  TableContext context = callback_context();

  switch ( context ) {
  case CONTEXT_CELL:
    {
      /* no editing in column 0 */
      if (col == 0) return;

      /* if there is an active edit line, finish it */
      if (input->visible()) input->do_callback();

      editShape = row;
      editGroup = col-1;

      /* calculate the cell the user has clicked into */
      int x, y, w, h;

      find_cell(CONTEXT_CELL, row, col, x, y, w, h);

      /* place the input line on that cell */
      input->resize(x, y, w, h);

      /* the text that should be initially in the input line */
      char s[30];
      int count = 0;

      if (editGroup == 0)
        count = puzzle->probGetShapeMin(prob, row);
      else if (editGroup == 1)
        count = puzzle->probGetShapeMax(prob, row);
      else
        for (unsigned int j = 0; j < puzzle->probGetShapeGroupNumber(prob, row); j++)
          if (puzzle->probGetShapeGroup(prob, row, j) == (col - 2))
            count = puzzle->probGetShapeGroupCount(prob, row, j);

      snprintf(s, 30, "%d", count);

      input->value(s);
      input->show();
      input->take_focus();

      return;
    }
  default:
    return;
  }
}

groupsEditorTab_c::groupsEditorTab_c(int x, int y, int w, int h, puzzle_c * puzzle, unsigned int problem) :
  Fl_Table(10, 10, 200, 200), layoutable_c(x, y, w, h), changed(false) {

  this->puzzle = puzzle;
  this->prob = problem;

  rows(puzzle->probShapeNumber(problem));

  /* find out the number of groups */
  maxGroup = 0;

  for (unsigned int i = 0; i < puzzle->probShapeNumber(problem); i++)
    for (unsigned int j = 0; j < puzzle->probGetShapeGroupNumber(prob, i); j++)
      if (puzzle->probGetShapeGroup(prob, i, j) > maxGroup)
        maxGroup = puzzle->probGetShapeGroup(prob, i, j);

  cols(maxGroup + 3);
  col_header(1);
  row_height_all(20);

  /* set the widths of the columns */
  col_width(0, 150);
  col_width(1, 35);
  col_width(2, 35);
  for (unsigned int i = 0; i < maxGroup; i++)
    col_width(i+3, 35);

  when(FL_WHEN_RELEASE);
  callback(cb_tab_stub, this);

  /* initialize the input line that is going to be used when asking for values */
  input = new Fl_Int_Input(0, 0, 0, 0);
  input->hide();
  input->when(FL_WHEN_ENTER_KEY_ALWAYS);
  input->callback(cb_input_stub, this);

  end();
}

void groupsEditorTab_c::addGroup(void) {

  maxGroup++;

  cols(maxGroup + 3);
  row_height_all(20);

  col_width(maxGroup+2, 35);
}

void groupsEditorTab_c::finishEdit(void) {
  if (input->visible()) {
    cb_input();
    redraw();
  }
}

static void cb_AddGroup_stub(Fl_Widget* o, void* v) { ((groupsEditor_c*)v)->cb_AddGroup(); }
void groupsEditor_c::cb_AddGroup(void) {
  tab->addGroup();
}

static void cb_CloseWindow_stub(Fl_Widget* o, void* v) { ((groupsEditor_c*)v)->cb_CloseWindow(); }
void groupsEditor_c::cb_CloseWindow(void) {
  hide();
}

static void cb_UpdateInterface_stub(Fl_Widget* o, void *v) { ((groupsEditor_c*)v)->cb_UpdateInterface(); }
void groupsEditor_c::cb_UpdateInterface(void) {

  if (tab->callback_context() != groupsEditorTab_c::CONTEXT_NONE) {
    tab->cb_tab();
    return;
  }

  /* check, if the maxHoles field makes any sense */
  bool useMaxHoles = false;

  for (unsigned int i = 0; i < puzzle->probShapeNumber(problem); i++)
    if (puzzle->probGetShapeMin(problem, i) != puzzle->probGetShapeMax(problem, i)) {
      useMaxHoles = true;
      break;
    }

  if (useMaxHoles)
    maxHoles->activate();
  else
    maxHoles->deactivate();
}

static void cb_MaxHoles_stub(Fl_Widget* o, void* v) { ((groupsEditor_c*)v)->cb_MaxHoles(); }
void groupsEditor_c::cb_MaxHoles(void) {

  if (maxHoles->value()[0])
    puzzle->probSetMaxHoles(problem, atoi(maxHoles->value()));
  else
    puzzle->probSetMaxHoles(problem, 0, true);

  if (puzzle->probMaxHolesDefined(problem)) {
    char tmp[20];
    snprintf(tmp, 20, "%i", puzzle->probGetMaxHoles(problem));
    maxHoles->value(tmp);
  } else
    maxHoles->value("");

  _changed = true;
}

/* when hiding the window, first close active editing tasks */
void groupsEditor_c::hide(void) {
  tab->finishEdit();

  if (maxHoles->value()[0])
    puzzle->probSetMaxHoles(problem, atoi(maxHoles->value()));
  else
    puzzle->probSetMaxHoles(problem, 0, true);

  Fl_Double_Window::hide();
}

#define SZ_GAP 5                               // gap between elements

groupsEditor_c::groupsEditor_c(puzzle_c * p, unsigned int pr) : LFl_Double_Window(true), puzzle(p), problem(pr), _changed(false) {

  tab = new groupsEditorTab_c(0, 0, 1, 1, p, pr);
  tab->pitch(SZ_GAP);
  tab->weight(1, 1);
  tab->callback(cb_UpdateInterface_stub, this);

  layouter_c * o = new layouter_c(0, 1, 1, 1);
  o->pitch(SZ_GAP);

  new LFl_Box("Maximum Number of Holes:", 0, 0, 1, 1);
  maxHoles = new LFl_Input(1, 0, 1, 1);
  maxHoles->weight(1, 0);
  maxHoles->tooltip(" Use to define the maximum number of holes alowed in the final solution. This is only"
                    "useful, when having pieceranges and will be disabled if you don't. ");
  maxHoles->callback(cb_MaxHoles_stub, this);
  if (p->probMaxHolesDefined(pr)) {
    char tmp[20];
    snprintf(tmp, 20, "%i", p->probGetMaxHoles(pr));
    maxHoles->value(tmp);
  } else
    maxHoles->value("");

  o->end();

  o = new layouter_c(0, 2, 2, 1);

  LFl_Button * btn;

  btn = new LFl_Button("Add Group", 0, 0, 1, 1);
  btn->callback(cb_AddGroup_stub, this);
  btn->tooltip("Add another group");
  btn->pitch(SZ_GAP);
  btn = new LFl_Button("Close", 1, 0, 1, 1);
  btn->callback(cb_CloseWindow_stub, this);
  btn->tooltip("Close Window");
  btn->pitch(SZ_GAP);

  o->end();

  label("Problem Details");

  set_modal();

  cb_UpdateInterface();
}


bool groupsEditor_c::changed(void) {
  return tab->getChanged() || _changed;
}

