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
#ifndef __MAINWINDOW_H__
#define __MAINWINDOW_H__

#include "Images.h"

#include "Layouter.h"

class VoxelEditGroup_c;
class ChangeSize;
class ToolTab;
class puzzle_c;
class solveThread_c;
class disasmToMoves_c;
class gridType_c;
class guiGridType_c;
class layouter_c;
class UndoManager_c;

class PieceSelector;
class ProblemSelector;
class ColorSelector;
class ResultViewer_c;
class PiecesList;
class PieceVisibility;
class ColorConstraintsEdit;
class ToolTabContainer;
class ButtonGroup_c;
class FlatButton;
class LStatusLine;
class LBlockListGroup_c;
class LView3dGroup;

class Fl_Tabs;
class Fl_Group;
class Fl_Check_Button;
class Fl_Value_Output;
class Fl_Output;
class Fl_Value_Slider;
class Fl_Value_Input;
class Fl_Menu_Bar;
class Fl_Choice;
class Fl_Progress;

class mainWindow_c : public LFl_Double_Window {

  puzzle_c * puzzle;
  guiGridType_c * ggt;  // this is the guigridtype for the puzzle, is must always be in sync
  char * fname;
  disasmToMoves_c * disassemble;
  solveThread_c *assmThread;
  bool SolutionEmpty;
  int editSymmetries;
  UndoManager_c * undoManager;

  bool expertMode;

  pixmapList_c pm;

  Fl_Tabs *TaskSelectionTab;
  layouter_c *TabPieces;
  Fl_Group *MinSizeSelector;

  PieceSelector * PcSel;
  ProblemSelector * problemSelector;
  ProblemSelector * solutionProblem;
  ColorSelector * colorAssignmentSelector;
  PieceSelector * shapeAssignmentSelector;
  ResultViewer_c * problemResult;
  PiecesList * PiecesCountList;
  PieceVisibility * PcVis;
  ColorConstraintsEdit * colconstrList;

  layouter_c *TabProblems;

  ToolTabContainer * pieceTools;
  ButtonGroup_c *editChoice;
  ButtonGroup_c *editMode;

  layouter_c *TabSolve;
  Fl_Check_Button *SolveDisasm, *JustCount, *DropDisassemblies, *KeepMirrors, *KeepRotations, *CompleteRotations;

  FlatButton *BtnPrepare, *BtnStart, *BtnCont, *BtnStop, *BtnPlacement, *BtnStep, *BtnMovement;
  FlatButton *BtnNewShape, *BtnDelShape, *BtnCpyShape, *BtnRenShape, *BtnShapeLeft, *BtnShapeRight, *BtnWeightInc, *BtnWeightDec;
  FlatButton *BtnNewColor, *BtnDelColor, *BtnChnColor;
  FlatButton *BtnNewProb, *BtnDelProb, *BtnCpyProb, *BtnRenProb, *BtnProbLeft, *BtnProbRight;
  FlatButton *BtnColSrtPc, *BtnColSrtRes, *BtnColAdd, *BtnColRem;
  FlatButton *BtnSetResult, *BtnAddShape, *BtnRemShape, *BtnMinZero, *BtnAddAll, *BtnRemAll, *BtnGroup, *BtnProbShapeLeft, *BtnProbShapeRight;

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

  LView3dGroup * View3D;

  Fl_Group *MinSizeTools;
  Fl_Menu_Bar *MainMenu;
  LStatusLine *StatusLine;
  static Fl_Menu_Item menu_MainMenu[];

  ColorSelector * colorSelector;

  VoxelEditGroup_c *pieceEdit;

  Fl_Choice * sortMethod;
  Fl_Value_Input *solDrop, *solLimit;

  Fl_Value_Output *SolutionNumber, *AssemblyNumber;

  FlatButton *BtnSrtFind, *BtnSrtLevel, *BtnSrtMoves, *BtnSrtPieces;
  FlatButton *BtnDelAll, *BtnDelBefore, *BtnDelAt, *BtnDelAfter, *BtnDelDisasm;
  FlatButton *BtnDisasmDel, *BtnDisasmDelAll, *BtnDisasmAdd, *BtnDisasmAddAll, *BtnDisasmAddMissing;

  // the zoom levels for all 3 tabs independent, so that the problem
  // tab can have a wider view
  double ViewSizes[3];
  int currentTab;

  bool tryToLoad(const char *fname);

  void CreateShapeTab(void);
  void CreateProblemTab(void);
  void CreateSolveTab(void);


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

  void ReplacePuzzle(puzzle_c * newPuzzle, bool keepSelections);

  void activateShape(unsigned int number);
  void activateProblem(unsigned int prob);
  void activateSolution(unsigned int prob, unsigned int num);
  void activateClear(void);

  bool threadStopped(void);

  void updateInterface(void);

public:

  mainWindow_c(gridType_c * gt);
  virtual ~mainWindow_c();

  int handle(int event);

  void show(int argn, char ** argv);

  // overwrite hide to check for changes in all possible exit situations
  void hide(void);

  /* this is used on assert to save the current puzzle */
  const puzzle_c * getPuzzle(void) const { return puzzle; }

  /* update the interface to represent the latest state of
   * the solving progress, that works in background
   */
  void update(void);

  /* return an index into the main menu array with the given text */
  int findMenuEntry(const char * txt);

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
  void cb_WeightChange(int by);

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
  void cb_SetShapeMinimumToZero(void);
  void cb_AddAllShapesToProblem(void);
  void cb_RemoveShapeFromProblem(void);
  void cb_RemoveAllShapesFromProblem(void);
  void cb_ProbShapeExchange(int with);

  void cb_PcSel(LBlockListGroup_c* reason);
  void cb_ColSel(LBlockListGroup_c* reason);
  void cb_ProbSel(LBlockListGroup_c* reason);

  void cb_PiecesClicked(void);

  void cb_TransformPiece(void);
  void cb_pieceEdit(VoxelEditGroup_c* o);
  void cb_EditChoice(void);
  void cb_EditSym(int onoff, int value);
  void cb_EditMode(void);

  void cb_TransformResult(void);

  void cb_AllowColor(void);
  void cb_DisallowColor(void);
  void cb_CCSort(bool byResult);

  void cb_BtnPrepare(void);
  void cb_BtnStart(bool prep_only);
  void cb_BtnCont(bool prep_only);
  void cb_BtnStop(void);
  void cb_BtnPlacementBrowser(void);
  void cb_BtnMovementBrowser(void);
  void cb_BtnAssemblerStep(void);

  void cb_SolutionSel(Fl_Value_Slider*);
  void cb_SolutionAnim(Fl_Value_Slider*);

  void cb_PcVis(void);

  void cb_Status(void);
  void cb_3dClick(void);

  void cb_New(void);
  void cb_Load(void);
  void cb_Load_Ps3d(void);
  void cb_Save(void);
  void cb_SaveAs(void);
  void cb_Undo(void);
  void cb_Redo(void);
  void cb_Convert(void);
  void cb_AssembliesToShapes(void);
  void cb_Quit(void);
  void cb_About(void);
  void cb_Help(void);
  void cb_Config(void);
  void cb_Coment(void);
  void cb_Toggle3D(void);
  void cb_SolProbSel(LBlockListGroup_c* reason);

  void cb_ShapeGroup(void);
  void cb_ImageExport(void);
  void cb_ImageExportVector(void);
  void cb_STLExport(void);
  void cb_StatusWindow(void);

  void cb_SortSolutions(unsigned int by);
  void cb_DeleteSolutions(unsigned int which);

  void cb_DeleteDisasm(void);
  void cb_DeleteAllDisasm(void);
  void cb_AddDisasm(void);
  void cb_AddAllDisasm(bool all);

  void activateConfigOptions(void);
};

#endif
