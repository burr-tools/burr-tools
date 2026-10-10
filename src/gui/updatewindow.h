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
#ifndef __UPDATEWINDOW_H__
#define __UPDATEWINDOW_H__

#include <FL/Fl_Double_Window.H>

#include <string>

class Fl_Text_Buffer;
class Fl_Text_Display;

/* Announces a newer release: a heading, the release notes as plain text,
 * and the choice of what to do about it. It only reports the choice; the
 * caller opens the browser or records a skipped version.
 */
class updateWindow_c : public Fl_Double_Window {

  public:

    enum class Choice { OpenPage, Later, Skip };

    /* offerSkip adds "Skip This Version", which only makes sense for the
     * launch check -- a manual check is an explicit request to see it.
     */
    updateWindow_c(const std::string & heading, const std::string & notes, bool offerSkip);
    ~updateWindow_c();

    /* Shows the window modally and returns the button pressed; closing the
     * window or pressing Escape counts as Later.
     */
    Choice run(void);

  private:

    Fl_Text_Buffer * buffer;
    Fl_Text_Display * display;
    Choice choice;

    static void cb_open(Fl_Widget *, void * v);
    static void cb_later(Fl_Widget *, void * v);
    static void cb_skip(Fl_Widget *, void * v);
    void finish(Choice c);
};

#endif
