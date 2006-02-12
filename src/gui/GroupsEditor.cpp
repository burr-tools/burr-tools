/* Burr Solver
 * Copyright (C) 2003-2006  Andreas Röver
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

#include "GroupsEditor.h"
#include "pieceColor.h"
#include "WindowWidgets.h"

#include <FL/fl_draw.h>

void GroupsEditor::draw_cell(TableContext context, int r, int c, int x, int y, int w, int h) {

  switch(context) {

  case CONTEXT_STARTPAGE:
    return;

  case CONTEXT_ROW_HEADER:
  case CONTEXT_COL_HEADER:
    fl_push_clip(x, y, w, h);
    {
      fl_draw_box(FL_THIN_UP_BOX, x, y, w, h, color());

      static char s[40];

      if (c == 0)
        snprintf(s, 40, "Shape");
      else if (c == 1)
        snprintf(s, 40, "n");
      else
        snprintf(s, 40, "Gr %i", c-1);

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
      static char s[40];

      int cnt = -1;

      if (c == 0) {

        cnt = puzzle->probGetShape(prob, r);
        if (puzzle->getShape(cnt)->getName())
          snprintf(s, 40, "S%i - %s", cnt+1, puzzle->getShape(cnt)->getName());
        else
          snprintf(s, 40, "S%i", cnt+1);

      } else if (c == 1) {
        cnt = puzzle->probGetShapeCount(prob, r);
        snprintf(s, 40, "%i", cnt);

      } else {

        for (unsigned int j = 0; j < puzzle->probGetShapeGroupNumber(prob, r); j++)
          if (puzzle->probGetShapeGroup(prob, r, j) == (c - 1))
            cnt = puzzle->probGetShapeGroupCount(prob, r, j);
        snprintf(s, 40, "%i", cnt);

      }

      if (cnt >= 0) {

        int p = puzzle->probGetShape(prob, r);
        unsigned char r, g, b;
        r = (int)(255*pieceColorR(p));
        g = (int)(255*pieceColorG(p));
        b = (int)(255*pieceColorB(p));

        fl_color(r, g, b);
        fl_rectf(x, y, w, h);
        if ((int)3*r + 6*g + 1*b > 1275)
          fl_color(0, 0, 0);
        else
          fl_color(255, 255, 255);
        fl_draw(s, x, y, w, h, FL_ALIGN_CENTER);
      } else {
        fl_color(color());
        fl_rectf(x, y, w, h);
      }

      fl_color(FL_BLACK);
      fl_rect(x, y, w, h);
    }
    fl_pop_clip();
    return;

  default:

    return;

  }
}

static void cb_input_stub(Fl_Widget*, void* v) { ((GroupsEditor*)v)->cb_input(); }
void GroupsEditor::cb_input(void) {

  if (editGroup == 0)
    puzzle->probSetShapeCount(prob, editShape, atoi(input->value()));
  else
    puzzle->probSetShapeGroup(prob, editShape, editGroup, atoi(input->value()));

  input->hide();
  changed = true;
}


static void cb_tab_stub(Fl_Widget*, void *v) { ((GroupsEditor*)v)->cb_tab(); }
void GroupsEditor::cb_tab(void)
{
  int row = callback_row();
  int col = callback_col();
  TableContext context = callback_context();

  switch ( context ) {
  case CONTEXT_CELL:
    {
      if (col == 0) return;
      if (input->visible()) input->do_callback();

      editShape = row;
      editGroup = col-1;

      int x, y, w, h;

      find_cell(CONTEXT_CELL, row, col, x, y, w, h);

      input->resize(x, y, w, h);

      char s[30];
      int count = 0;

      if (editGroup == 0)
        count = puzzle->probGetShapeCount(prob, row);
      else
        for (unsigned int j = 0; j < puzzle->probGetShapeGroupNumber(prob, row); j++)
          if (puzzle->probGetShapeGroup(prob, row, j) == (col - 1))
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


GroupsEditor::GroupsEditor(int x, int y, int w, int h, puzzle_c * puzzle, unsigned int problem) : Fl_Table(x, y, w, h), changed(false) {

  this->puzzle = puzzle;
  this->prob = problem;

  rows(puzzle->probShapeNumber(problem));

  maxGroup = 0;

  for (unsigned int i = 0; i < puzzle->probShapeNumber(problem); i++)
    for (unsigned int j = 0; j < puzzle->probGetShapeGroupNumber(prob, i); j++)
      if (puzzle->probGetShapeGroup(prob, i, j) > maxGroup)
        maxGroup = puzzle->probGetShapeGroup(prob, i, j);

  cols(maxGroup + 2);
  col_header(1);
  row_height_all(20);

  col_width(0, 150);
  col_width(1, 35);
  for (unsigned int i = 0; i < maxGroup; i++)
    col_width(i+2, 35);

  when(FL_WHEN_RELEASE);
  callback(cb_tab_stub, this);

  input = new Fl_Int_Input(0, 0, 0, 0);
  input->hide();
  input->when(FL_WHEN_ENTER_KEY_ALWAYS);
  input->callback(cb_input_stub, this);

  end();
}

void GroupsEditor::addGroup(void) {

  maxGroup++;

  cols(maxGroup + 2);
  row_height_all(20);

  col_width(maxGroup+1, 35);
}

static void cb_AddGroup_stub(Fl_Widget* o, void* v) { ((groupsEditorWindow*)v)->cb_AddColor(); }
void groupsEditorWindow::cb_AddColor(void) {
  tab->addGroup();
}

static void cb_CloseWindow_stub(Fl_Widget* o, void* v) { ((groupsEditorWindow*)v)->cb_CloseWindow(); }
void groupsEditorWindow::cb_CloseWindow(void) {
  hide();
}


#define SZ_WINDOW_X 300                        // initial size of the window
#define SZ_WINDOW_Y 200
#define SZ_GAP 5                               // gap between elements
#define SZ_BUTTON_Y 20

groupsEditorWindow::groupsEditorWindow(puzzle_c * p, unsigned int pr) : Fl_Double_Window(SZ_WINDOW_X, SZ_WINDOW_Y) {

  tab = new GroupsEditor(SZ_GAP, SZ_GAP, SZ_WINDOW_X-2*SZ_GAP, SZ_WINDOW_Y-3*SZ_GAP-SZ_BUTTON_Y, p, pr);

  new FlatButton(SZ_GAP, SZ_WINDOW_Y-SZ_GAP-SZ_BUTTON_Y, (SZ_WINDOW_X-3*SZ_GAP)/2, SZ_BUTTON_Y, "Add Group", "Add another group", cb_AddGroup_stub, this);
  new FlatButton((SZ_GAP+SZ_WINDOW_X)/2, SZ_WINDOW_Y-SZ_GAP-SZ_BUTTON_Y, (SZ_WINDOW_X-3*SZ_GAP)/2, SZ_BUTTON_Y, "Close", "Close Window", cb_CloseWindow_stub, this);

  label("Group Editor");

  set_modal();
  size_range(SZ_WINDOW_X, SZ_WINDOW_Y, 0, 0);
  resizable(tab);
}

