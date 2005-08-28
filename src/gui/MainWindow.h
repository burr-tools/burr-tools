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
#include "../lib/AssemblyCallbacks.h"
#include "DisasmToMoves.h"
#include "WindowWidgets.h"
#include "BlockList.h"

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

class VoxelEditGroup;
class ChangeSize;
class ToolTab;
class View3dGroup;

class UserInterface {
  puzzle_c * puzzle;
  char * fname;
  DisasmToMoves * disassemble;
  assemblerThread *assmThread;
  bool SolutionEmpty;
  bool changed;

  Fl_Double_Window *mainWindow;

  Fl_Tabs *TaskSelectionTab;
  Fl_Group *TabPieces;
  Fl_Group *MinSizeSelector;

  PieceSelector * PcSel;
  ProblemSelector * problemSelector;
  ProblemSelector * solutionProblem;
  ColorSelector * colorAssignmentSelector;
  PieceSelector * shapeAssignmentSelector;
  ResultViewer * problemResult;
  PiecesList * PiecesCountList;
  PieceVisibility * PcVis;
  ColorConstraintsEdit * colconstrList;

  Fl_Group *TabProblems;

  ToolTab * pieceTools;
  ToolTab * resultTools;

  Fl_Group *TabSolve;
  Fl_Check_Button *SolveDisasm, *JustCount;

  FlatButton *BtnStart, *BtnCont, *BtnStop;
  FlatButton *BtnNewShape, *BtnDelShape, *BtnCpyShape;
  FlatButton *BtnNewColor, *BtnDelColor, *BtnChnColor;
  FlatButton *BtnNewProb, *BtnDelProb, *BtnCpyProb, *BtnRenProb;
  FlatButton *BtnColSrtPc, *BtnColSrtRes, *BtnColAdd, *BtnColRem;
  FlatButton *BtnSetResult, *BtnAddShape, *BtnRemShape, *BtnAddGroup, *BtnSubGroup;

  Fl_Progress *SolvingProgress;
  Fl_Value_Output *OutputAssemblies;
  Fl_Value_Output *OutputSolutions;
  Fl_Output *OutputActivity;
  Fl_Check_Button *ReducePositions;
  Fl_Value_Slider *SolutionSel;
  Fl_Value_Slider *SolutionAnim;
  Fl_Value_Output *SolutionsInfo;
  Fl_Output *MovesInfo;

  Fl_Output *TimeUsed, *TimeEst;

  View3dGroup * View3D;

  Fl_Group *MinSizeTools;
  Fl_Menu_Bar *MainMenu;
  StatusLine *Status;
  static Fl_Menu_Item menu_MainMenu[];

  ColorSelector * colorSelector;

  VoxelEditGroup *pieceEdit, *resultEdit;

  void tryToLoad(const char *fname);

public:
  void CreateShapeTab(int x, int y, int w, int h);
  void CreateProblemTab(int x, int y, int w, int h);
  void CreateSolveTab(int x, int y, int w, int h);

  UserInterface();

  void cb_AddColor(void);
  void cb_RemoveColor(void);
  void cb_ChangeColor(void);

  void cb_NewShape(void);
  void cb_DeleteShape(void);
  void cb_CopyShape(void);

  void cb_NewProblem(void);
  void cb_DeleteProblem(void);
  void cb_CopyProblem(void);
  void cb_RenameProblem(void);

  void cb_ColorAssSel(void);
  void cb_ColorConstrSel(void);

  void cb_ShapeToResult(void);

  void cb_TaskSelectionTab(Fl_Tabs*);

  void cb_SelectProblemShape(void);
  void cb_AddShapeToProblem(void);
  void cb_RemoveShapeFromProblem(void);

  void cb_PcSel(long reason);
  void cb_ColSel(long reason);
  void cb_ProbSel(long reason);

  void cb_PiecesClicked(void);

  void cb_TransformPiece(void);
  void cb_pieceEdit(VoxelEditGroup* o);

  void cb_TransformResult(void);

  void cb_AllowColor(void);
  void cb_DisallowColor(void);
  void cb_CCSort(bool byResult);

  void cb_BtnStart(void);
  void cb_BtnCont(void);
  void cb_BtnStop(void);

  void cb_SolutionSel(Fl_Value_Slider*);
  void cb_SolutionAnim(Fl_Value_Slider*);

  void cb_PcVis(void);

  void cb_Status(void);

  void cb_New(void);
  void cb_Load(void);
  void cb_Load_Ps3d(void);
  void cb_Save(void);
  void cb_SaveAs(void);
  void cb_Quit(void);
  void cb_About(void);
  void cb_Toggle3D(void);
  void cb_SolProbSel(long reason);

  void cb_IncShapeGroup(void);
  void cb_DecShapeGroup(void);

  bool is3DViewBig;
  bool shapeEditorWithBig3DView;

  void Toggle3DView(void);
  void Big3DView(void);
  void Small3DView(void);

  void StatPieceInfo(unsigned int pc);
  void StatProblemInfo(unsigned int pr);

  void changeShape(unsigned int nr);
  void changeProblem(unsigned int nr);
  void changeColor(unsigned int nr);

  void ReplacePuzzle(puzzle_c * newPuzzle);

  void show(int argn, char ** argv);

  void activateShape(unsigned int number);
  void activateProblem(unsigned int prob);
  void activateSolution(unsigned int prob, unsigned int num);
  void activateClear(void);

  bool threadStopped(void);

  void update(void);
  void updateInterface(void);
};

#endif
