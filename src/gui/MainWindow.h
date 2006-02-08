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

class UserInterface : public Fl_Double_Window {

  puzzle_c * puzzle;
  char * fname;
  DisasmToMoves * disassemble;
  assemblerThread *assmThread;
  bool SolutionEmpty;
  bool changed;
  int editSymmetries;

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
  FlatLightButton *editLayersButton;
  ButtonGroup *editChoice;
  ButtonGroup *editMode;

  Fl_Group *TabSolve;
  Fl_Check_Button *SolveDisasm, *JustCount;

  FlatButton *BtnStart, *BtnCont, *BtnStop, *BtnPlacement;
  FlatButton *BtnNewShape, *BtnDelShape, *BtnCpyShape, *BtnRenShape, *BtnShapeLeft, *BtnShapeRight;
  FlatButton *BtnNewColor, *BtnDelColor, *BtnChnColor;
  FlatButton *BtnNewProb, *BtnDelProb, *BtnCpyProb, *BtnRenProb, *BtnProbLeft, *BtnProbRight;
  FlatButton *BtnColSrtPc, *BtnColSrtRes, *BtnColAdd, *BtnColRem;
  FlatButton *BtnSetResult, *BtnAddShape, *BtnRemShape, *BtnGroup, *BtnProbShapeLeft, *BtnProbShapeRight;

  ProgressBar *SolvingProgress;
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

  VoxelEditGroup *pieceEdit;

  int progressLabelDigits;


  // the zoom levels for all 3 tabs independent, so that the problem
  // tab can have a wider view
  double ViewSizes[3];
  int currentTab;

  void tryToLoad(const char *fname);

  void CreateShapeTab(int x, int y, int w, int h);
  void CreateProblemTab(int x, int y, int w, int h);
  void CreateSolveTab(int x, int y, int w, int h);


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

  void activateShape(unsigned int number);
  void activateProblem(unsigned int prob);
  void activateSolution(unsigned int prob, unsigned int num);
  void activateClear(void);

  bool threadStopped(void);

  void updateInterface(void);

public:

  UserInterface();
  virtual ~UserInterface();

  int handle(int event);

  void show(int argn, char ** argv);

  // overwrite hide to check for changes in all possible exit situations
  void hide(void);

  /* this is used on assert to save the current puzzle */
  const puzzle_c * getPuzzle(void) const { return puzzle; }

  /* update the interface to represent the latest state of
   * the solving progress, thas works in background
   */
  void update(void);

  /* the callback functions, as they are called from normal functions we need
   * to make them public, even though they should not be used from the outside
   */
  void cb_AddColor(void);
  void cb_RemoveColor(void);
  void cb_ChangeColor(void);

  void cb_NewShape(void);
  void cb_DeleteShape(void);
  void cb_CopyShape(void);
  void cb_NameShape(void);
  void cb_ShapeExchange(int with);

  void cb_NewProblem(void);
  void cb_DeleteProblem(void);
  void cb_CopyProblem(void);
  void cb_RenameProblem(void);
  void cb_ProblemExchange(int with);

  void cb_ColorAssSel(void);
  void cb_ColorConstrSel(void);

  void cb_ShapeToResult(void);

  void cb_TaskSelectionTab(Fl_Tabs*);

  void cb_SelectProblemShape(void);
  void cb_AddShapeToProblem(void);
  void cb_RemoveShapeFromProblem(void);
  void cb_ProbShapeExchange(int with);

  void cb_PcSel(BlockListGroup* reason);
  void cb_ColSel(BlockListGroup* reason);
  void cb_ProbSel(BlockListGroup* reason);

  void cb_PiecesClicked(void);

  void cb_TransformPiece(void);
  void cb_pieceEdit(VoxelEditGroup* o);
  void cb_EditChoice(void);
  void cb_EditSym(int onoff, int value);
  void cb_EditMode(void);

  void cb_TransformResult(void);

  void cb_AllowColor(void);
  void cb_DisallowColor(void);
  void cb_CCSort(bool byResult);

  void cb_BtnStart(void);
  void cb_BtnCont(void);
  void cb_BtnStop(void);
  void cb_BtnPlacementBrowser(void);
  void cb_BtnAssemblerStep(void);

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
  void cb_Config(void);
  void cb_Coment(void);
  void cb_Toggle3D(void);
  void cb_SolProbSel(BlockListGroup* reason);

  void cb_ShapeGroup(void);
  void cb_ImageExport(void);

  void activateConfigOptions(void);
};

#endif
