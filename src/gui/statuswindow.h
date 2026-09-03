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
#ifndef __STATUS_WINDOW__
#define __STATUS_WINDOW__

#include "Layouter.h"

#include <vector>

class puzzle_c;

class statusWindow_c : public layouter_c {

  private:

    puzzle_c * puz;
    void * cbUser;
    Fl_Callback * closeCb;
    Fl_Callback * changedCb;

    std::vector<LFl_Check_Button*> selection;
    std::vector<bool> identicalMirror;
    std::vector<bool> identicalShape;
    std::vector<bool> identicalComplete;

    void clearChildren(void);

  public:

    statusWindow_c(int x, int y, int w, int h);

    void setCallbacks(Fl_Callback * onClose, Fl_Callback * onChanged, void * user);
    void populate(puzzle_c * p);

    virtual void getMinSize(int *width, int *height) const;

    void cb_removeSelected(void);
    void cb_selectHoles(void);
    void cb_selectIdenticalShapes(void);
    void cb_selectIdenticalComplete(void);
    void cb_selectIdenticalMirror(void);
    void cb_close(void);
    void cb_refresh(void);
};

#endif
