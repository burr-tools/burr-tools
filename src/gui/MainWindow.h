/* Burr Solver
 * Copyright (C) 2003-2005  Andreas Röver
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


#ifndef MainWindow_h
#define MainWindow_h

#include "../lib/puzzle.h"
#include "AssemblyCallbacks.h"
#include "DisasmToMoves.h"
#include "PieceVisibility.h"
#include "WindowWidgets.h"

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Tile.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Value_Output.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Progress.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Value_Slider.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/fl_ask.H>

#include <fstream>

using namespace std;

class VoxelEditGroup;
class ChangeSize;
class ToolTab;
class SelectorGroup;
class View3dGroup;

class UserInterface {
  puzzle_c * puzzle;
  int activePiece;
  char * fname;
  float shifting[100];
  char visibility[33];
  int colors[66];
  DisasmToMoves * disassemble;
  assemblerThread *assmThread;
  bool SolutionEmpty;
  bool changed;

  Fl_Double_Window *mainWindow;

  Fl_Tabs *TaskSelectionTab;
  Fl_Group *TabPieces;
  Fl_Group *MinSizeSelector;

  SelectorGroup *PcSel2;

  Fl_Group *TabResult;

  ToolTab * pieceTools;
  ToolTab * resultTools;

  Fl_Group *TabSolve;
  Fl_Check_Button *SolveDisasm;
  FlatButton *BtnStart;
  FlatButton *BtnCont;
  FlatButton *BtnStop;
  Fl_Progress *SolvingProgress;
  Fl_Value_Output *OutputAssemblies;
  Fl_Value_Output *OutputSolutions;
  Fl_Output *OutputActivity;
  Fl_Check_Button *ReducePositions;
  Fl_Value_Output *OutputIterations;
  Fl_Value_Slider *SolutionSel;
  Fl_Value_Slider *SolutionAnim;
  PieceVisibility *PcVis;
  Fl_Value_Output *SolutionsInfo;
  Fl_Value_Output *MovesInfo;

  View3dGroup * View3D;

  Fl_Group *MinSizeTools;
  Fl_Menu_Bar *MainMenu;
  static Fl_Menu_Item menu_MainMenu[];

  VoxelEditGroup *pieceEdit, *resultEdit;

  void tryToLoad(const char *fname);

public:
  UserInterface();

  void cb_TaskSelectionTab(Fl_Tabs*);

  void cb_NewPiece(void);
  void cb_Delete(void);
  void cb_Copy(void);

  void cb_PcSel(long reason);

  void cb_TransformPiece(void);
  void cb_pieceEdit(VoxelEditGroup* o);

  void cb_TransformResult(void);

  void cb_BtnStart(void);
  void cb_BtnCont(void);
  void cb_BtnStop(void);

  void cb_SolutionSel(Fl_Value_Slider*);
  void cb_SolutionAnim(Fl_Value_Slider*);

  void cb_PcVis(void);

  void cb_New(void);
  void cb_Load(void);
  void cb_Save(void);
  void cb_SaveAs(void);
  void cb_Quit(void);

  void show(int argn, char ** argv);
  void activatePiece(int number);
  void activateResult(void);
  void activateSolution(unsigned int num);
  void update(void);
  void removeAssmThread(void);
};
#endif
