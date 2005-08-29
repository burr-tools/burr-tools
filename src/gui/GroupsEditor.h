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

  void draw_cell(TableContext context, int r = 0, int c = 0,
                 int x = 0, int y = 0, int w = 0, int h = 0);

public:

  GroupsEditor(int x, int y, int w, int h, puzzle_c * puzzle, unsigned int problem);

  void addGroup(void);

  void cb_input(void);
  void cb_tab(void);

};

class groupsEditorWindow : public Fl_Double_Window {

  GroupsEditor * tab;

public:

  groupsEditorWindow(puzzle_c * p, unsigned int pr);

  void cb_AddColor(void);
  void cb_CloseWindow(void);
};

#endif
