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
#ifndef __GROUPS_EDITOR_H__
#define __GROUPS_EDITOR_H__

#include "Fl_Table.h"
#include "../lib/puzzle.h"

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Int_Input.H>

class GroupsEditor : public Fl_Table {

  puzzle_c * puzzle;
  unsigned int prob;

  Fl_Int_Input* input;
  int editShape, editGroup;

  unsigned int maxGroup;

  bool changed;

  void draw_cell(TableContext context, int r = 0, int c = 0,
                 int x = 0, int y = 0, int w = 0, int h = 0);

public:

  GroupsEditor(int x, int y, int w, int h, puzzle_c * puzzle, unsigned int problem);

  void addGroup(void);

  void cb_input(void);
  void cb_tab(void);

  bool getChanged(void) { return changed; }

};

class groupsEditorWindow : public Fl_Double_Window {

  GroupsEditor * tab;

public:

  groupsEditorWindow(puzzle_c * p, unsigned int pr);

  void cb_AddColor(void);
  void cb_CloseWindow(void);

  bool changed(void) { return tab->getChanged(); }
};

#endif
