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
#include "updatewindow.h"

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>

#define SZ_WIN_X 560
#define SZ_WIN_Y 420
#define SZ_GAP 10
#define SZ_HEADING 40
#define SZ_BUTTON_Y 25

updateWindow_c::updateWindow_c(const std::string & heading, const std::string & notes, bool offerSkip)
  : Fl_Double_Window(SZ_WIN_X, SZ_WIN_Y, "Software Update"), choice(Choice::Later) {

  Fl_Box * head = new Fl_Box(SZ_GAP, SZ_GAP, SZ_WIN_X - 2*SZ_GAP, SZ_HEADING);
  head->copy_label(heading.c_str());
  head->align(FL_ALIGN_INSIDE | FL_ALIGN_LEFT | FL_ALIGN_WRAP);
  head->labelfont(FL_HELVETICA_BOLD);

  int notesY = SZ_GAP + SZ_HEADING + SZ_GAP;
  int buttonsY = SZ_WIN_Y - SZ_GAP - SZ_BUTTON_Y;

  buffer = new Fl_Text_Buffer();
  buffer->text(notes.empty() ? "No release notes provided." : notes.c_str());

  display = new Fl_Text_Display(SZ_GAP, notesY, SZ_WIN_X - 2*SZ_GAP, buttonsY - SZ_GAP - notesY);
  display->buffer(buffer);
  display->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);

  int x = SZ_WIN_X - SZ_GAP;

  x -= 150;
  Fl_Return_Button * open = new Fl_Return_Button(x, buttonsY, 150, SZ_BUTTON_Y, "Open Release Page");
  open->callback(cb_open, this);

  x -= SZ_GAP + 130;
  Fl_Button * later = new Fl_Button(x, buttonsY, 130, SZ_BUTTON_Y, "Remind Me Later");
  later->callback(cb_later, this);

  if (offerSkip) {
    Fl_Button * skip = new Fl_Button(SZ_GAP, buttonsY, 130, SZ_BUTTON_Y, "Skip This Version");
    skip->callback(cb_skip, this);
  }

  end();

  /* Escape and the close box both arrive as the window callback. */
  callback(cb_later, this);
  resizable(display);
  size_range(470, 250);
  set_modal();
}

updateWindow_c::~updateWindow_c() {
  /* Fl_Text_Display does not own its buffer, and the display is destroyed
   * by Fl_Group's destructor after this body runs; detach it first so it
   * never touches a deleted buffer.
   */
  display->buffer(nullptr);
  delete buffer;
}

updateWindow_c::Choice updateWindow_c::run(void) {
  show();
  while (shown())
    Fl::wait();
  return choice;
}

void updateWindow_c::finish(Choice c) {
  choice = c;
  hide();
}

void updateWindow_c::cb_open(Fl_Widget *, void * v)  { static_cast<updateWindow_c *>(v)->finish(Choice::OpenPage); }
void updateWindow_c::cb_later(Fl_Widget *, void * v) { static_cast<updateWindow_c *>(v)->finish(Choice::Later); }
void updateWindow_c::cb_skip(Fl_Widget *, void * v)  { static_cast<updateWindow_c *>(v)->finish(Choice::Skip); }
