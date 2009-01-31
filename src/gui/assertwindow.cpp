/* Burr Solver
 * Copyright (C) 2003-2008  Andreas Röver
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
#include "assertwindow.h"
#include <stdio.h>

#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Multiline_Output.H>

#define ASSERT_WINDOW_X 500
#define ASSERT_WINDOW_Y 400

#define ASSERT_TXT1 40
#define ASSERT_TXT2 60

#define ASSERT_BTN_X 100

static void cb_assertClose_stub(Fl_Widget* /*o*/, void* v) { ((assertWindow_c*)v)->hide(); }

assertWindow_c::assertWindow_c(const char * text, assert_exception * a) : Fl_Double_Window(0, 0, ASSERT_WINDOW_X, ASSERT_WINDOW_Y) {

  char txt[100000];

  // first the text given
  (new Fl_Box(5, 5, ASSERT_WINDOW_X-10, ASSERT_TXT1, text))->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_LEFT);

  // now some standard text
  (new Fl_Box(5, 10+ASSERT_TXT1, ASSERT_WINDOW_X-10, ASSERT_TXT2,
             "Please send the text in the following box to roever@@users.sf.net\n"
             "if possible include a file of the puzzle that failed and a description\n"
             "of what you did until this error occurred,   thank you."
            ))->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_LEFT);

  // finally the text box
  int pos = snprintf(txt, 1000,
                     "Assert failed in\n"
                     "file: %s\n"
                     "function: %s\n"
                     "line: %i\n"
                     "condition: %s\n",
                     a->file, a->function, a->line, a->expr);

  // append the log
  if (assert_log->lines()) {

    pos += snprintf(txt+pos, 100000-pos, "log:\n");

    for (unsigned int l = 0; l < assert_log->lines(); l++)
      pos += snprintf(txt+pos, 100000-pos, "%s\n", assert_log->line(l));
  }

  (new Fl_Multiline_Output(5, 15+ASSERT_TXT1+ASSERT_TXT2, ASSERT_WINDOW_X-10, ASSERT_WINDOW_Y - 25 - 20 - ASSERT_TXT1 - ASSERT_TXT2))->value(txt);

  (new Fl_Button((ASSERT_WINDOW_X-ASSERT_BTN_X)/2, ASSERT_WINDOW_Y-25, ASSERT_BTN_X, 20, "Close"))->callback(cb_assertClose_stub, this);

  label("Error");
}

