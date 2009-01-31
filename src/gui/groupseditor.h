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
#ifndef __GROUPS_EDITOR_H__
#define __GROUPS_EDITOR_H__

#include "Layouter.h"

class puzzle_c;

class groupsEditorTab_c;

/* the window that contains the group edit table */
class groupsEditor_c : public LFl_Double_Window {

  groupsEditorTab_c * tab;
  LFl_Input * maxHoles;
  puzzle_c * puzzle;
  unsigned int problem;
  bool _changed;

public:

  groupsEditor_c(puzzle_c * p, unsigned int problem);

  void cb_AddGroup(void);
  void cb_CloseWindow(void);
  void cb_MaxHoles(void);
  void cb_UpdateInterface(void);

  bool changed(void);

  /* finish editing and close window */
  void hide(void);
};

#endif
