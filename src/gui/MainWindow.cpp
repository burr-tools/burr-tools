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
#include "MainWindow.h"

#include "config.h"

#include <xmlwrapp/xmlwrapp.h>

#include "gzstream.h"
#include "configuration.h"
#include "GroupsEditor.h"
#include "PlacementBrowser.h"
#include "ImageExport.h"
#include "Images.h"

#include "../config.h"

#include "../lib/ps3dloader.h"

#ifdef HAVE_FLU
#include <FLU/Flu_File_Chooser.h>
#endif

#include <FL/Fl_Color_Chooser.H>
#include <FL/Fl_Tooltip.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Pixmap.h>

static const char * FileSelection(const char * title) {
#ifdef HAVE_FLU
    return flu_file_chooser(title, "*.xmpuzzle", "");
#else
    return fl_file_chooser(title, "*.xmpuzzle", "");
#endif
}

static const char * FileSelection2(const char * title) {
#ifdef HAVE_FLU
    return flu_file_chooser(title, "*.puz", "");
#else
    return fl_file_chooser(title, "*.puz", "");
#endif
}

/* returns true, if file exists, this is not the
 optimal way to do this. it would be better to open
 the dir the file is supposed to be in and look there
 but this is not really portable so this
 */
bool fileExists(const char *n) {

  FILE *f = fopen(n, "r");

  if (f) {
    fclose(f);
    return true;
  } else
    return false;
}

static void cb_AddColor_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_AddColor(); }
void UserInterface::cb_AddColor(void) {

  unsigned char r, g, b;

  if (colorSelector->getSelection() == 0)
    r = g = b = 128;
  else
    puzzle->getColor(colorSelector->getSelection()-1, &r, &g, &b);

  if (fl_color_chooser("New color", r, g, b)) {
    puzzle->addColor(r, g, b);
    colorSelector->setSelection(puzzle->colorNumber());
    changed = true;
    View3D->showColors(puzzle, Status->useColors());
    updateInterface();
  }
}

static void cb_RemoveColor_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_RemoveColor(); }
void UserInterface::cb_RemoveColor(void) {

  if (colorSelector->getSelection() == 0)
    fl_message("Can not delete the Neutral color, this color has to be there");
  else {
    changeColor(colorSelector->getSelection());
    puzzle->removeColor(colorSelector->getSelection());

    unsigned int current = colorSelector->getSelection();

    while ((current > 0) && (current > puzzle->colorNumber()))
      current--;

    colorSelector->setSelection(current);

    changed = true;
    View3D->showColors(puzzle, Status->useColors());
    activateShape(PcSel->getSelection());
    updateInterface();
  }
}

static void cb_ChangeColor_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_ChangeColor(); }
void UserInterface::cb_ChangeColor(void) {

  if (colorSelector->getSelection() == 0)
    fl_message("Can not edit the Neutral color");
  else {
    unsigned char r, g, b;
    puzzle->getColor(colorSelector->getSelection()-1, &r, &g, &b);
    if (fl_color_chooser("Change color", r, g, b)) {
      puzzle->changeColor(colorSelector->getSelection()-1, r, g, b);
      changed = true;
      View3D->showColors(puzzle, Status->useColors());
      updateInterface();
    }
  }
}

static void cb_NewShape_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_NewShape(); }
void UserInterface::cb_NewShape(void) {

  if (PcSel->getSelection() < puzzle->shapeNumber()) {
    const voxel_c * v = puzzle->getShape(PcSel->getSelection());
    PcSel->setSelection(puzzle->addShape(v->getX(), v->getY(), v->getZ()));
  } else
    PcSel->setSelection(puzzle->addShape(6, 6, 6));
  pieceEdit->setZ(0);
  updateInterface();
  StatPieceInfo(PcSel->getSelection());
  changed = true;
}

static void cb_DeleteShape_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_DeleteShape(); }
void UserInterface::cb_DeleteShape(void) {

  unsigned int current = PcSel->getSelection();

  if (current < puzzle->shapeNumber()) {

    changeShape(current);

    puzzle->removeShape(current);

    if (puzzle->shapeNumber() == 0)
      current = (unsigned int)-1;
    else
      while (current >= puzzle->shapeNumber())
        current--;

    activateShape(current);

    PcSel->setSelection(current);
    updateInterface();
    StatPieceInfo(PcSel->getSelection());

    changed = true;

  } else

    fl_message("No shape to delete selected!");

}

static void cb_CopyShape_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_CopyShape(); }
void UserInterface::cb_CopyShape(void) {

  unsigned int current = PcSel->getSelection();

  if (current < puzzle->shapeNumber()) {

    PcSel->setSelection(puzzle->addShape(new voxel_c(puzzle->getShape(current))));
    changed = true;

    updateInterface();
    StatPieceInfo(PcSel->getSelection());

  } else

    fl_message("No shape to copy selected!");

}

static void cb_NameShape_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_NameShape(); }
void UserInterface::cb_NameShape(void) {

  if (PcSel->getSelection() < puzzle->shapeNumber()) {

    const char * name = fl_input("Enter name for the shape", puzzle->getShape(PcSel->getSelection())->getName());

    if (name) {
      puzzle->getShape(PcSel->getSelection())->setName(name);
      changed = true;
      updateInterface();
    }
  }
}

static void cb_TaskSelectionTab_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_TaskSelectionTab((Fl_Tabs*)o); }
void UserInterface::cb_TaskSelectionTab(Fl_Tabs* o) {

  if (o->value() == TabPieces) {
    activateShape(PcSel->getSelection());
    StatPieceInfo(PcSel->getSelection());
    if (!shapeEditorWithBig3DView)
      Small3DView();
    else
      Big3DView();
    ViewSizes[currentTab] = View3D->getZoom();
    if (ViewSizes[0] >= 0)
      View3D->setZoom(ViewSizes[0]);
    currentTab = 0;
  } else if(o->value() == TabProblems) {
    if (problemSelector->getSelection() < puzzle->problemNumber()) {
      activateProblem(problemSelector->getSelection());
    }
    StatProblemInfo(problemSelector->getSelection());
    Big3DView();
    ViewSizes[currentTab] = View3D->getZoom();
    if (ViewSizes[1] >= 0)
      View3D->setZoom(ViewSizes[1]);
    currentTab = 1;
  } else if(o->value() == TabSolve) {
    if ((solutionProblem->getSelection() < puzzle->problemNumber()) &&
        (SolutionSel->value() < puzzle->probSolutionNumber(solutionProblem->getSelection()))) {
      activateSolution(solutionProblem->getSelection(), int(SolutionSel->value()));
    }
    Big3DView();
    Status->setText("");
    ViewSizes[currentTab] = View3D->getZoom();
    if (ViewSizes[2] >= 0)
      View3D->setZoom(ViewSizes[2]);
    currentTab = 2;
  }

  updateInterface();
}

static void cb_TransformPiece_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_TransformPiece(); }
void UserInterface::cb_TransformPiece(void) {

  if (pieceTools->operationToAll()) {
    for (unsigned int i = 0; i < puzzle->shapeNumber(); i++)
      changeShape(i);
  } else {
    changeShape(PcSel->getSelection());
  }

  StatPieceInfo(PcSel->getSelection());
  activateShape(PcSel->getSelection());

  changed = true;
}

static void cb_EditSym_stub(Fl_Widget* o, void* v) {
  ((UserInterface*)v)->cb_EditSym(((ToggleButton*)o)->value(), ((ToggleButton*)o)->ButtonVal());
}
void UserInterface::cb_EditSym(int onoff, int value) {
  if (onoff) {
    editSymmetries |= value;
  } else {
    editSymmetries &= ~value;
  }

  pieceEdit->editSymmetries(editSymmetries);
}

static void cb_EditChoice_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_EditChoice(); }
void UserInterface::cb_EditChoice(void) {
  switch(editChoice->getSelected()) {
    case 0:
      pieceEdit->editChoice(SquareEditor::TSK_SET);
      break;
    case 1:
      pieceEdit->editChoice(SquareEditor::TSK_VAR);
      break;
    case 2:
      pieceEdit->editChoice(SquareEditor::TSK_RESET);
      break;
    case 3:
      pieceEdit->editChoice(SquareEditor::TSK_COLOR);
      break;
  }
}

static void cb_EditMode_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_EditMode(); }
void UserInterface::cb_EditMode(void) {
  switch(editMode->getSelected()) {
    case 0:
      pieceEdit->editType(SquareEditor::EDT_RUBBER);
      config.useRubberband(true);
      break;
    case 1:
      pieceEdit->editType(SquareEditor::EDT_SINGLE);
      config.useRubberband(false);
      break;
  }
}

static void cb_PcSel_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_PcSel((BlockListGroup*)o); }
void UserInterface::cb_PcSel(BlockListGroup* grp) {
  int reason = grp->getReason();

  switch(reason) {
  case PieceSelector::RS_CHANGEDSELECTION:
    activateShape(PcSel->getSelection());
    updateInterface();
    StatPieceInfo(PcSel->getSelection());
    break;
  }
}

static void cb_SolProbSel_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_SolProbSel((BlockListGroup*)o); }
void UserInterface::cb_SolProbSel(BlockListGroup* grp) {
  int reason = grp->getReason();

  switch(reason) {
  case ProblemSelector::RS_CHANGEDSELECTION:

    updateInterface();
    activateSolution(solutionProblem->getSelection(), (int)SolutionSel->value());
    break;
  }
}

static void cb_ColSel_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_ColSel((BlockListGroup*)o); }
void UserInterface::cb_ColSel(BlockListGroup* grp) {
  int reason = grp->getReason();

  switch(reason) {
  case PieceSelector::RS_CHANGEDSELECTION:
    pieceEdit->setColor(colorSelector->getSelection());
    updateInterface();
    activateShape(PcSel->getSelection());
    break;
  }
}

static void cb_ProbSel_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_ProbSel((BlockListGroup*)o); }
void UserInterface::cb_ProbSel(BlockListGroup* grp) {
  int reason = grp->getReason();

  switch(reason) {
  case PieceSelector::RS_CHANGEDSELECTION:
    updateInterface();
    activateProblem(problemSelector->getSelection());
    StatProblemInfo(problemSelector->getSelection());
    break;
  }
}

static void cb_pieceEdit_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_pieceEdit((VoxelEditGroup*)o); }
void UserInterface::cb_pieceEdit(VoxelEditGroup* o) {

  switch (o->getReason()) {
  case SquareEditor::RS_MOUSEMOVE:
    if (o->getMouse())
      View3D->setMarker(o->getMouseX1(), o->getMouseY1(), o->getMouseX2(), o->getMouseY2(), o->getMouseZ(), editSymmetries);
    else
      View3D->hideMarker();
    break;
  case SquareEditor::RS_CHANGESQUARE:
    View3D->showSingleShape(puzzle, PcSel->getSelection());
    StatPieceInfo(PcSel->getSelection());
    changeShape(PcSel->getSelection());
    changed = true;
    break;
  }

  View3D->redraw();
}

static void cb_NewProblem_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_NewProblem(); }
void UserInterface::cb_NewProblem(void) {

  unsigned int prob = puzzle->addProblem();

  problemSelector->setSelection(prob);

  changed = true;
  updateInterface();
  activateProblem(problemSelector->getSelection());
  StatProblemInfo(problemSelector->getSelection());
}

static void cb_DeleteProblem_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_DeleteProblem(); }
void UserInterface::cb_DeleteProblem(void) {

  if (problemSelector->getSelection() < puzzle->problemNumber()) {

    puzzle->removeProblem(problemSelector->getSelection());

    changed = true;

    while ((problemSelector->getSelection() >= puzzle->problemNumber()) &&
           (problemSelector->getSelection() > 0))
      problemSelector->setSelection(problemSelector->getSelection()-1);

    updateInterface();
    if (problemSelector->getSelection() < puzzle->problemNumber())
      activateProblem(problemSelector->getSelection());
    else
      activateClear();
    StatProblemInfo(problemSelector->getSelection());
  }
}

static void cb_CopyProblem_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_CopyProblem(); }
void UserInterface::cb_CopyProblem(void) {

  if (problemSelector->getSelection() < puzzle->problemNumber()) {

    unsigned int prob = puzzle->copyProblem(problemSelector->getSelection());
    problemSelector->setSelection(prob);

    changed = true;
    updateInterface();
    activateProblem(problemSelector->getSelection());
    StatProblemInfo(problemSelector->getSelection());
  }
}

static void cb_RenameProblem_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_RenameProblem(); }
void UserInterface::cb_RenameProblem(void) {

  if (problemSelector->getSelection() < puzzle->problemNumber()) {

    const char * name = fl_input("Enter name for the problem", puzzle->probGetName(problemSelector->getSelection()).c_str());

    if (name) {

      puzzle->probSetName(problemSelector->getSelection(), name);
      changed = true;
      updateInterface();
      activateProblem(problemSelector->getSelection());
    }
  }
}

static void cb_ProblemLeft_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_ProblemExchange(-1); }
static void cb_ProblemRight_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_ProblemExchange(+1); }
void UserInterface::cb_ProblemExchange(int with) {

  unsigned int current = problemSelector->getSelection();
  unsigned int other = current + with;

  if ((current < puzzle->problemNumber()) && (other < puzzle->problemNumber())) {
    puzzle->exchangeProblem(current, other);
    changed = true;
    problemSelector->setSelection(other);
  }
}

static void cb_ShapeLeft_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_ShapeExchange(-1); }
static void cb_ShapeRight_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_ShapeExchange(+1); }
void UserInterface::cb_ShapeExchange(int with) {

  unsigned int current = PcSel->getSelection();
  unsigned int other = current + with;

  if ((current < puzzle->shapeNumber()) && (other < puzzle->shapeNumber())) {
    puzzle->exchangeShape(current, other);
    changed = true;
    PcSel->setSelection(other);
  }
}

static void cb_ProbShapeLeft_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_ProbShapeExchange(-1); }
static void cb_ProbShapeRight_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_ProbShapeExchange(+1); }
void UserInterface::cb_ProbShapeExchange(int with) {

  unsigned int p = problemSelector->getSelection();
  unsigned int s = shapeAssignmentSelector->getSelection();

  // find out the index in the problem table
  unsigned int current;

  for (current = 0; current < puzzle->probShapeNumber(p); current++)
    if (puzzle->probGetShape(p, current) == s)
      break;

  unsigned int other = current + with;

  if ((current < puzzle->probShapeNumber(p)) && (other < puzzle->probShapeNumber(p))) {
    puzzle->probExchangeShape(p, current, other);
    changed = true;
    updateInterface();
    activateProblem(problemSelector->getSelection());
  }
}

static void cb_ColorAssSel_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_ColorAssSel(); }
void UserInterface::cb_ColorAssSel(void) {
  updateInterface();
}

static void cb_ColorConstrSel_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_ColorConstrSel(); }
void UserInterface::cb_ColorConstrSel(void) {
  updateInterface();
}

static void cb_ShapeToResult_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_ShapeToResult(); }
void UserInterface::cb_ShapeToResult(void) {

  if (problemSelector->getSelection() >= puzzle->problemNumber()) {
    fl_message("First create a problem");
    return;
  }

  unsigned int prob = problemSelector->getSelection();

  // check if this shape is already a piece of the problem
  for (unsigned int i = 0; i < puzzle->probShapeNumber(prob); i++) {
    if (puzzle->probGetShape(prob, i) == shapeAssignmentSelector->getSelection()) {
      puzzle->probRemoveShape(prob, i);
      break;
    }
  }

  changeProblem(prob);
  puzzle->probSetResult(prob, shapeAssignmentSelector->getSelection());
  problemResult->setPuzzle(puzzle, prob);
  activateProblem(prob);
  StatProblemInfo(prob);
  updateInterface();

  changed = true;
}

static void cb_ShapeSel_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_SelectProblemShape(); }
void UserInterface::cb_SelectProblemShape(void) {
  updateInterface();
  activateProblem(problemSelector->getSelection());
}

static void cb_PiecesClicked_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_PiecesClicked(); }
void UserInterface::cb_PiecesClicked(void) {

  shapeAssignmentSelector->setSelection(puzzle->probGetShape(problemSelector->getSelection(), PiecesCountList->getClicked()));

  updateInterface();
  activateProblem(problemSelector->getSelection());
}

static void cb_AddShapeToProblem_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_AddShapeToProblem(); }
void UserInterface::cb_AddShapeToProblem(void) {

  if (problemSelector->getSelection() >= puzzle->problemNumber()) {
    fl_message("First create a problem");
    return;
  }

  unsigned int prob = problemSelector->getSelection();

  changed = true;
  PiecesCountList->redraw();
  changeProblem(prob);

  // first see, if there is already a the selected shape inside
  for (unsigned int i = 0; i < puzzle->probShapeNumber(prob); i++)
    if (puzzle->probGetShape(prob, i) == shapeAssignmentSelector->getSelection()) {
      puzzle->probSetShapeCount(prob, i, puzzle->probGetShapeCount(prob, i) + 1);
      PcVis->setPuzzle(puzzle, solutionProblem->getSelection());
      StatProblemInfo(problemSelector->getSelection());
      return;
    }

  puzzle->probAddShape(prob, shapeAssignmentSelector->getSelection(), 1);
  activateProblem(problemSelector->getSelection());
  updateInterface();
  StatProblemInfo(problemSelector->getSelection());
}

static void cb_RemoveShapeFromProblem_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_RemoveShapeFromProblem(); }
void UserInterface::cb_RemoveShapeFromProblem(void) {

  if (problemSelector->getSelection() >= puzzle->problemNumber()) {
    fl_message("First create a problem");
    return;
  }

  unsigned int prob = problemSelector->getSelection();
  changeProblem(prob);

  // first see, find the shape, and only if there is one, we decrement its count out remove it
  for (unsigned int i = 0; i < puzzle->probShapeNumber(prob); i++)
    if (puzzle->probGetShape(prob, i) == shapeAssignmentSelector->getSelection()) {
      if (puzzle->probGetShapeCount(prob, i) == 1)
        puzzle->probRemoveShape(prob, i);
      else
        puzzle->probSetShapeCount(prob, i, puzzle->probGetShapeCount(prob, i) - 1);

      changed = true;
      PiecesCountList->redraw();
      PcVis->setPuzzle(puzzle, solutionProblem->getSelection());
    }

  activateProblem(problemSelector->getSelection());
  StatProblemInfo(problemSelector->getSelection());
}

static void cb_ShapeGroup_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_ShapeGroup(); }
void UserInterface::cb_ShapeGroup(void) {

  unsigned int prob = problemSelector->getSelection();

  groupsEditorWindow * groupEditWin = new groupsEditorWindow(puzzle, prob);

  groupEditWin->show();

  while (groupEditWin->visible())
    Fl::wait();

  if (groupEditWin->changed()) {

    /* as the user may have reset the counts of one shape to zero, go
     * through the list and remove entries of zero count */

    unsigned int i = 0;
    while (i < puzzle->probShapeNumber(prob)) {

      if (puzzle->probGetShapeCount(prob, i))
        i++;
      else
        puzzle->probRemoveShape(prob, i);
    }

    PiecesCountList->redraw();
    PcVis->setPuzzle(puzzle, solutionProblem->getSelection());
    changed = true;
    changeProblem(problemSelector->getSelection());
    activateProblem(problemSelector->getSelection());
    StatProblemInfo(problemSelector->getSelection());
    updateInterface();
  }

  delete groupEditWin;
}

static void cb_BtnPlacementBrowser_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_BtnPlacementBrowser(); }
void UserInterface::cb_BtnPlacementBrowser(void) {

  unsigned int prob = solutionProblem->getSelection();

  PlacementBrowser * plbr = new PlacementBrowser(puzzle, prob);

  plbr->show();

  while (plbr->visible())
    Fl::wait();

  delete plbr;
}

static void cb_BtnAssemblerStep_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_BtnAssemblerStep(); }
void UserInterface::cb_BtnAssemblerStep(void) {

  bt_assert(assmThread == 0);

  assembler_0_c * assm = (assembler_0_c*)puzzle->probGetAssembler(solutionProblem->getSelection());

  bt_assert(assm);

  assm->debug_step();

  if (assm->getFinished() >= 1)
    puzzle->probFinishedSolving(solutionProblem->getSelection());

  updateInterface();

  View3D->showAssemblerState(puzzle, solutionProblem->getSelection(), assm->getAssembly());
}

static void cb_AllowColor_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_AllowColor(); }
void UserInterface::cb_AllowColor(void) {

  if (problemSelector->getSelection() >= puzzle->problemNumber()) {
    fl_message("First create a problem");
    return;
  }

  if (colconstrList->GetSortByResult())
    puzzle->probAllowPlacement(problemSelector->getSelection(),
                               colorAssignmentSelector->getSelection()+1,
                               colconstrList->getSelection()+1);
  else
    puzzle->probAllowPlacement(problemSelector->getSelection(),
                               colconstrList->getSelection()+1,
                               colorAssignmentSelector->getSelection()+1);
  changed = true;
  changeProblem(problemSelector->getSelection());
  updateInterface();
}

static void cb_DisallowColor_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_DisallowColor(); }
void UserInterface::cb_DisallowColor(void) {

  if (problemSelector->getSelection() >= puzzle->problemNumber()) {
    fl_message("First create a problem");
    return;
  }

  if (colconstrList->GetSortByResult())
    puzzle->probDisallowPlacement(problemSelector->getSelection(),
                                  colorAssignmentSelector->getSelection()+1,
                                  colconstrList->getSelection()+1);
  else
    puzzle->probDisallowPlacement(problemSelector->getSelection(),
                                  colconstrList->getSelection()+1,
                                  colorAssignmentSelector->getSelection()+1);

  changed = true;
  changeProblem(problemSelector->getSelection());
  updateInterface();
}

static void cb_CCSortByResult_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_CCSort(1); }
static void cb_CCSortByPiece_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_CCSort(0); }
void UserInterface::cb_CCSort(bool byResult) {
  colconstrList->SetSortByResult(byResult);

  if (byResult) {
    BtnColSrtPc->activate();
    BtnColSrtRes->deactivate();
  } else {
    BtnColSrtPc->deactivate();
    BtnColSrtRes->activate();
  }
}

static void cb_BtnPrepare_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_BtnPrepare(); }
void UserInterface::cb_BtnPrepare(void) {
  cb_BtnStart();

  if (assmThread)
    assmThread->stop();
}

static void cb_BtnStart_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_BtnStart(); }
void UserInterface::cb_BtnStart(void) {

  puzzle->probRemoveAllSolutions(solutionProblem->getSelection());
  SolutionEmpty = true;

  for (unsigned int i = 0; i < puzzle->shapeNumber(); i++) {
    puzzle->getShape(i)->setHotspot(0, 0, 0);
  }

  cb_BtnCont();
}

static void cb_BtnCont_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_BtnCont(); }
void UserInterface::cb_BtnCont(void) {

  unsigned int prob = solutionProblem->getSelection();

  if (prob >= puzzle->problemNumber()) {
    fl_message("First create a problem");
    return;
  }

  if (puzzle->probGetResult(prob) > puzzle->shapeNumber()) {
    fl_message("A result shape must be defined");
    return;
  }

  bt_assert(assmThread == 0);

  if (SolveDisasm->value() != 0)
    if (JustCount->value() != 0)
      assmThread = new assemblerThread(puzzle, prob, assemblerThread::SOL_COUNT_DISASM, true);
    else
      assmThread = new assemblerThread(puzzle, prob, assemblerThread::SOL_DISASM, true);
  else
    if (JustCount->value() != 0)
      assmThread = new assemblerThread(puzzle, prob, assemblerThread::SOL_COUNT_ASM, true);
    else
      assmThread = new assemblerThread(puzzle, prob, assemblerThread::SOL_SAVE_ASM, true);

  if (!assmThread->start()) {
    fl_message("Could not start the solving process, the thread creation failed, sorry.");
    delete assmThread;
    assmThread = 0;

  } else {

    BtnStart->deactivate();
    BtnCont->deactivate();
    BtnStop->activate();
  }
}

static void cb_BtnStop_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_BtnStop(); }
void UserInterface::cb_BtnStop(void) {

  bt_assert(assmThread);

  assmThread->stop();
}

static void cb_SolutionSel_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_SolutionSel((Fl_Value_Slider*)o); }
void UserInterface::cb_SolutionSel(Fl_Value_Slider* o) {
  o->take_focus();
  activateSolution(solutionProblem->getSelection(), int(o->value()));
}

static void cb_SolutionAnim_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_SolutionAnim((Fl_Value_Slider*)o); }
void UserInterface::cb_SolutionAnim(Fl_Value_Slider* o) {
  o->take_focus();
  if (disassemble) {
    disassemble->setStep(o->value());
    View3D->updatePositions(disassemble);
  }
}

static void cb_PcVis_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_PcVis(); }
void UserInterface::cb_PcVis(void) {
  View3D->updateVisibility(PcVis);
}

static void cb_Status_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_Status(); }
void UserInterface::cb_Status(void) {
  View3D->showColors(puzzle, Status->useColors());
}

static void cb_New_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_New(); }
void UserInterface::cb_New(void) {

  if (threadStopped()) {

    if (changed)
      if (fl_ask("Puzzle changed are you shure?") == 0)
        return;

    ReplacePuzzle(new puzzle_c());

    if (fname) {
      delete [] fname;
      fname = 0;
    }

    changed = false;

    Status->setText("");
    updateInterface();
    activateShape(0);
  }
}

static void cb_Load_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_Load(); }
void UserInterface::cb_Load(void) {

  if (threadStopped()) {

    if (changed)
      if (fl_ask("Puzzle changed are you shure?") == 0)
        return;

    const char * f = FileSelection("Load Puzzle");

    tryToLoad(f);
  }
}

static void cb_Load_Ps3d_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_Load_Ps3d(); }
void UserInterface::cb_Load_Ps3d(void) {

  if (threadStopped()) {

    if (changed)
      if (fl_ask("Puzzle changed are you shure?") == 0)
        return;

    const char * f = FileSelection2("Load Puzzle");

    if (f) {

      std::ifstream in(f);

      puzzle_c * newPuzzle = loadPuzzlerSolver3D(&in);
      if (!newPuzzle) {
        fl_alert("Could not load puzzle, sorry!");
        return;
      }

      if (fname) delete [] fname;
      fname = new char[strlen(f)+1];
      strcpy(fname, f);

      char nm[300];
      snprintf(nm, 299, "BurrTools - %s", fname);
      label(nm);

      ReplacePuzzle(newPuzzle);
      updateInterface();

      TaskSelectionTab->value(TabPieces);
      activateShape(PcSel->getSelection());
      StatPieceInfo(PcSel->getSelection());

      changed = false;
    }
  }
}

static void cb_Save_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_Save(); }
void UserInterface::cb_Save(void) {

  if (threadStopped()) {

    if (!fname)
      cb_SaveAs();

    else {
      ogzstream ostr(fname);

      if (ostr)
        ostr << puzzle->save();

      if (!ostr)
        fl_alert("puzzle NOT saved!!");
      else
        changed = false;
    }
  }
}

static void cb_SaveAs_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_SaveAs(); }
void UserInterface::cb_SaveAs(void) {

  if (threadStopped()) {
    const char * f = FileSelection("Save Puzzle as");

    if (f) {

      if (!fileExists(f) || fl_ask("File exists overwrite?")) {

        char f2[1000];

        // check, if the last characters are ".xmpuzzle"
        if (strcmp(f + strlen(f) - strlen(".xmpuzzle"), ".xmpuzzle")) {
          snprintf(f2, 1000, "%s.xmpuzzle", f);

        } else

          snprintf(f2, 1000, "%s", f);

        ogzstream ostr(f2);

        if (ostr)
          ostr << puzzle->save();

        if (!ostr)
          fl_alert("puzzle NOT saved!!!");
        else
          changed = false;

        if (fname) delete [] fname;
        fname = new char[strlen(f2)+1];
        strcpy(fname, f2);

        char nm[300];
        snprintf(nm, 299, "BurrTools - %s", fname);
        label(nm);

      } else {

        fl_message("File not saved!\n");
      }
    }
  }
}

static void cb_Quit_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->hide(); }
void UserInterface::hide(void) {
  if ((!changed) || fl_ask("Puzzle changed do you want to quit and loose the changes?"))
    Fl_Double_Window::hide();
}

static void cb_Config_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_Config(); }
void UserInterface::cb_Config(void) {
  config.dialog();
  activateConfigOptions();
}

static void cb_Comment_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_Coment(); }
void UserInterface::cb_Coment(void) {

  multiLineWindow win("Edit Coment", "Change the comment for the current puzzle", puzzle->getComment().c_str());

  win.show();

  while (win.visible())
    Fl::wait();

  if (win.saveChanges()) {
    puzzle->setComment(win.getText());
    changed = true;
  }
}

static void cb_ImageExport_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_ImageExport(); }
void UserInterface::cb_ImageExport(void) {
  ImageExportWindow w(puzzle);
  w.show();

  while (w.visible()) {
    w.update();
    if (w.isWorking())
      Fl::wait(0);
    else
      Fl::wait(1);
  }
}

static void cb_Toggle3D_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_Toggle3D(); }
void UserInterface::cb_Toggle3D(void) {

  if (TaskSelectionTab->value() == TabPieces) {
    shapeEditorWithBig3DView = !shapeEditorWithBig3DView;
    if (!shapeEditorWithBig3DView)
      Small3DView();
    else
      Big3DView();
  }
}

static void cb_About_stub(Fl_Widget* o, void* v) { ((UserInterface*)v)->cb_About(); }
void UserInterface::cb_About(void) {

  fl_message("This is the GUI for BurrTools version " VERSION "\n"
             "BurrTools (c) 2003-2005 by Andreas Röver\n"
             "The latest version is available at burrtools.sourceforge.net\n"
             "\n"
             "This software is distributed under the GPL\n"
             "You should have received a copy of the GNU General Public License\n"
             "along with this program; if not, write to the Free Software\n"
             "Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.\n"
             "\n"
             "The program uses\n"
             "- Fltk, FLU, libZ, libXml2, XmlWrapp, libpng\n"
             "- gzstream by Deepak Bandyopadhyay, Lutz Kettner\n"
             "- Fl_Table (http://3dsite.com/people/erco/Fl_Table/)\n"
             "- tr by Brian Paul (http://www.mesa3d.org/brianp/TR.html)\n"
            );
}

void UserInterface::StatPieceInfo(unsigned int pc) {

  if (pc < puzzle->shapeNumber()) {
    char txt[100];

    unsigned int fx = puzzle->getShape(pc)->countState(voxel_c::VX_FILLED);
    unsigned int vr = puzzle->getShape(pc)->countState(voxel_c::VX_VARIABLE);

    snprintf(txt, 100, "Shape S%i has %i cubes (%i fixed, %i variable)", pc+1, fx+vr, fx, vr);
    Status->setText(txt);
  }
}

void UserInterface::StatProblemInfo(unsigned int pr) {

  if ((pr < puzzle->problemNumber()) && (puzzle->probGetResult(pr) < puzzle->shapeNumber())) {

    char txt[100];

    unsigned int cnt = 0;

    for (unsigned int i = 0; i < puzzle->probShapeNumber(pr); i++)
      cnt += puzzle->probGetShapeShape(pr, i)->countState(voxel_c::VX_FILLED) * puzzle->probGetShapeCount(pr, i);

    snprintf(txt, 100, "Problem P%i result can contain %i - %i cubes, pieces (n = %i) contain %i cubes", pr+1,
             puzzle->probGetResultShape(pr)->countState(voxel_c::VX_FILLED),
             puzzle->probGetResultShape(pr)->countState(voxel_c::VX_FILLED) +
             puzzle->probGetResultShape(pr)->countState(voxel_c::VX_VARIABLE),
             puzzle->probPieceNumber(pr), cnt);
    Status->setText(txt);

  } else

    Status->setText("");
}

void UserInterface::changeColor(unsigned int nr) {

  for (unsigned int i = 0; i < puzzle->shapeNumber(); i++)
    for (unsigned int j = 0; j < puzzle->getShape(i)->getXYZ(); j++)
      if (puzzle->getShape(i)->getColor(j) == nr) {
        changeShape(i);
        break;
      }
}

void UserInterface::changeShape(unsigned int nr) {
  for (unsigned int i = 0; i < puzzle->problemNumber(); i++)
    if (puzzle->probContainsShape(i, nr))
      puzzle->probRemoveAllSolutions(i);
}

void UserInterface::changeProblem(unsigned int nr) {
  puzzle->probRemoveAllSolutions(nr);
}

bool UserInterface::threadStopped(void) {

  if (assmThread) {

    fl_message("Stop solving process first!");
    return false;
  }

  return true;
}

void UserInterface::tryToLoad(const char * f) {

  // it may well be that the file doesn't exist, if it comed from the command line
  if (f && fileExists(f)) {

    xml::tree_parser parser(f);

    if (!parser) {

      fl_message("Error parsing xml puzzle file, xml-syntax incorrect:\n %s", parser.get_error_message().c_str());

      return;
    }

    xml::document &doc = parser.get_document();

    puzzle_c * newPuzzle;

    try {
      newPuzzle = new puzzle_c(doc.get_root_node());
    }

    catch (load_error e) {
      fl_message(e.getText());
      std::cout << e.getNode();
      return;
    }

    if (fname) delete [] fname;
    fname = new char[strlen(f)+1];
    strcpy(fname, f);

    char nm[300];
    snprintf(nm, 299, "BurrTools - %s", fname);
    label(nm);

    ReplacePuzzle(newPuzzle);
    updateInterface();

    TaskSelectionTab->value(TabPieces);
    activateShape(PcSel->getSelection());
    StatPieceInfo(PcSel->getSelection());
    View3D->showColors(puzzle, Status->useColors());

    changed = false;

    {

      // check for astarted assemblies, and warn user about it
      bool containsStarted = false;

      for (unsigned int p = 0; p < puzzle->problemNumber(); p++) {
        if (puzzle->probGetSolveState(p) == puzzle_c::SS_SOLVING) {
          containsStarted = true;
          break;
        }
      }

      if (containsStarted)
        fl_message("This puzzle file contains started but not finished search for solutions.");

    }
  }
}

void UserInterface::ReplacePuzzle(puzzle_c * NewPuzzle) {

    // inform everybody
    colorSelector->setPuzzle(NewPuzzle);
    PcSel->setPuzzle(NewPuzzle);
    pieceEdit->setPuzzle(NewPuzzle, 0);
    problemSelector->setPuzzle(NewPuzzle);
    colorAssignmentSelector->setPuzzle(NewPuzzle);
    colconstrList->setPuzzle(NewPuzzle, 0);
    problemResult->setPuzzle(NewPuzzle, 0);
    shapeAssignmentSelector->setPuzzle(NewPuzzle);
    PiecesCountList->setPuzzle(NewPuzzle, 0);
    solutionProblem->setPuzzle(NewPuzzle);
    PcVis->setPuzzle(NewPuzzle, 0);

    SolutionSel->value(0);
    SolutionAnim->value(0);

    delete puzzle;
    puzzle = NewPuzzle;
}

Fl_Menu_Item UserInterface::menu_MainMenu[] = {
  { "&File",           0, 0, 0, FL_SUBMENU },
    {"New",            0, cb_New_stub,         0, 0, 0, 0, 14, 56},
    {"Load",    FL_F + 3, cb_Load_stub,        0, 0, 0, 0, 14, 56},
    {"Import",         0, cb_Load_Ps3d_stub,   0, 0, 0, 0, 14, 56},
    {"Save",    FL_F + 2, cb_Save_stub,        0, 0, 0, 0, 14, 56},
    {"Save as",        0, cb_SaveAs_stub,      0, FL_MENU_DIVIDER, 0, 0, 14, 56},
    {"Quit",           0, cb_Quit_stub,        0, 0, 3, 0, 14, 56},
    { 0 },
  {"Toggle 3D", FL_F + 4, cb_Toggle3D_stub,    0, 0, 0, 0, 14, 56},
  {"Export Images",    0, cb_ImageExport_stub, 0, 0, 0, 0, 14, 56},
  {"Edit Comment",     0, cb_Comment_stub,     0, 0, 0, 0, 14, 56},
  {"Config",           0, cb_Config_stub,      0, 0, 0, 0, 14, 56},
  {"About",            0, cb_About_stub,       0, 0, 3, 0, 14, 56},
  {0}
};

void UserInterface::show(int argn, char ** argv) {
  Fl_Double_Window::show();

  if (argn == 2)
#ifdef __APPLE__
  {
    if (argv[1][0]!='-') // hack to get rid of -psn_0_* passed by apple finder
      tryToLoad(argv[1]);
  }
#else
    tryToLoad(argv[1]);
#endif
}

void UserInterface::activateClear(void) {
  View3D->showNothing();
  pieceEdit->clearPuzzle();
  pieceTools->setVoxelSpace(0, 0);

  SolutionEmpty = true;
}

void UserInterface::activateShape(unsigned int number) {

  if ((number < puzzle->shapeNumber())) {

    View3D->showSingleShape(puzzle, number);
    pieceEdit->setPuzzle(puzzle, number);
    pieceTools->setVoxelSpace(puzzle, number);

    PcSel->setSelection(number);

  } else {

    View3D->showNothing();
    pieceEdit->clearPuzzle();
    pieceTools->setVoxelSpace(0, 0);
  }

  SolutionEmpty = true;
}

void UserInterface::activateProblem(unsigned int prob) {

  View3D->showProblem(puzzle, prob, shapeAssignmentSelector->getSelection());

  SolutionEmpty = true;
}

void UserInterface::activateSolution(unsigned int prob, unsigned int num) {

  if (disassemble) {
    delete disassemble;
    disassemble = 0;
  }

  if ((prob < puzzle->problemNumber()) && (num < puzzle->probSolutionNumber(prob))) {

    PcVis->setPuzzle(puzzle, prob);

    if (puzzle->probGetDisassembly(prob, num)) {
      SolutionAnim->show();
      SolutionAnim->range(0, puzzle->probGetDisassembly(prob, num)->sumMoves());

      SolutionsInfo->show();

      MovesInfo->show();

      char levelText[50];
      int len = snprintf(levelText, 50, "%i (", puzzle->probGetDisassembly(prob, num)->sumMoves());
      puzzle->probGetDisassembly(prob, num)->movesText(levelText + len, 50-len);
      levelText[strlen(levelText)+1] = 0;
      levelText[strlen(levelText)] = ')';

      MovesInfo->value(levelText);

      disassemble = new DisasmToMoves(puzzle->probGetDisassembly(prob, num),
                                      2*puzzle->probGetResultShape(prob)->getBiggestDimension());
      disassemble->setStep(SolutionAnim->value());

      View3D->showAssembly(puzzle, prob, num);
      View3D->updatePositions(disassemble);
      View3D->updateVisibility(PcVis);

    } else {

      SolutionAnim->range(0, 0);
      SolutionAnim->hide();
      MovesInfo->value(0);
      MovesInfo->hide();

      View3D->showAssembly(puzzle, prob, num);
      View3D->updateVisibility(PcVis);
    }

    SolutionEmpty = false;

  } else {

    View3D->showNothing();
    SolutionEmpty = true;

    SolutionAnim->hide();
    MovesInfo->hide();

    PcVis->setPuzzle(puzzle, solutionProblem->getSelection());

  }
}

const char * timeToString(float time) {

  static char tmp[50];

  if (time < 60)                               snprintf(tmp, 50, "%i seconds",     int(time/(1                 )));
  else if (time < 60*60)                       snprintf(tmp, 50, "%.1f minutes",   time/(60                    ));
  else if (time < 60*60*24)                    snprintf(tmp, 50, "%.1f hours",     time/(60*60                 ));
  else if (time < 60*60*24*30)                 snprintf(tmp, 50, "%.1f days",      time/(60*60*24              ));
  else if (time < 60*60*24*365.2422)           snprintf(tmp, 50, "%.1f months",    time/(60*60*24*30           ));
  else if (time < 60*60*24*365.2422*1000)      snprintf(tmp, 50, "%.1f years",     time/(60*60*24*365.2422     ));
  else if (time < 60*60*24*365.2422*1000*1000) snprintf(tmp, 50, "%.1f millenia",  time/(60*60*24*365.2422*1000));
  else                                         snprintf(tmp, 50, "ages");

  return tmp;
}

void UserInterface::updateInterface(void) {

  unsigned int prob = solutionProblem->getSelection();

  if (TaskSelectionTab->value() == TabPieces) {
    // shapes tab

    // we can only delete colors, when something valid is selected
    // and no assembler is running
    if ((colorSelector->getSelection() > 0) && !assmThread)
      BtnDelColor->activate();
    else
      BtnDelColor->deactivate();

    // colors can be changed for all colors except the neutral color
    if (colorSelector->getSelection() > 0)
      BtnChnColor->activate();
    else
      BtnChnColor->deactivate();

    // we can only edit and copy shapes, when something valid is selected
    if (PcSel->getSelection() < puzzle->shapeNumber()) {
      BtnCpyShape->activate();
      BtnRenShape->activate();
      pieceEdit->activate();
    } else {
      BtnCpyShape->deactivate();
      BtnRenShape->deactivate();
      pieceEdit->deactivate();
    }

    // shapes can only be moved, when the neibor shape is there
    if ((PcSel->getSelection() > 0) && (PcSel->getSelection() < puzzle->shapeNumber()) && !assmThread)
      BtnShapeLeft->activate();
    else
      BtnShapeLeft->deactivate();
    if ((PcSel->getSelection()+1 < puzzle->shapeNumber()) && !assmThread)
      BtnShapeRight->activate();
    else
      BtnShapeRight->deactivate();

    // we can only delete shapes, when something valid is selected
    // and no assembler is running
    if ((PcSel->getSelection() < puzzle->shapeNumber()) && !assmThread) {
      BtnDelShape->activate();
    } else {
      BtnDelShape->deactivate();
    }

    // we can only edit shapes, when something gvalid is selected and
    // either no assemlber is running or the shape is not in the problem that the assembler works on
    if ((PcSel->getSelection() < puzzle->shapeNumber()) &&
        (!assmThread || !puzzle->probContainsShape(assmThread->getProblem(), PcSel->getSelection()))) {
      pieceTools->activate();
    } else {
      pieceTools->deactivate();
    }

    // when the current shape is in the assembler we lock the editor, only viewing is possible
    if (assmThread && (puzzle->probContainsShape(assmThread->getProblem(), PcSel->getSelection())))
      pieceEdit->deactivate();
    else
      pieceEdit->activate();

  } else if (TaskSelectionTab->value() == TabProblems) {

    // problem tab
    PiecesCountList->setPuzzle(puzzle, problemSelector->getSelection());
    colconstrList->setPuzzle(puzzle, problemSelector->getSelection());
    problemResult->setPuzzle(puzzle, problemSelector->getSelection());

    // problems can only be renames and copied, when something valid is selected
    if (problemSelector->getSelection() < puzzle->problemNumber()) {
      BtnCpyProb->activate();
      BtnRenProb->activate();
    } else {
      BtnCpyProb->deactivate();
      BtnRenProb->deactivate();
    }

    // problems can only be shifted around when the corresponding neibor is
    // available
    if ((problemSelector->getSelection() > 0) && (problemSelector->getSelection() < puzzle->problemNumber()) && !assmThread)
      BtnProbLeft->activate();
    else
      BtnProbLeft->deactivate();
    if ((problemSelector->getSelection()+1 < puzzle->problemNumber()) && !assmThread)
      BtnProbRight->activate();
    else
      BtnProbRight->deactivate();

    if (problemSelector->getSelection() < puzzle->problemNumber() && !assmThread)
    {
      unsigned int current;
      unsigned int p = problemSelector->getSelection();
      unsigned int s = shapeAssignmentSelector->getSelection();

      for (current = 0; current < puzzle->probShapeNumber(p); current++)
        if (puzzle->probGetShape(p, current) == s)
          break;

      if (current && (current < puzzle->probShapeNumber(p)))
        BtnProbShapeLeft->activate();
      else
        BtnProbShapeLeft->deactivate();
      if (current+1 < puzzle->probShapeNumber(p))
        BtnProbShapeRight->activate();
      else
        BtnProbShapeRight->deactivate();

    } else {
      BtnProbShapeRight->deactivate();
      BtnProbShapeLeft->deactivate();
    }

    // problems can only be deleted, something valid is selected and the
    // assembler is not running
    if ((problemSelector->getSelection() < puzzle->problemNumber()) && !assmThread)
      BtnDelProb->activate();
    else
      BtnDelProb->deactivate();

    // we can only edit color constraints when a valid problem is selected
    // the selected color is valid
    // the assembler is not running or not busy with the selected problem
    if ((problemSelector->getSelection() < puzzle->problemNumber()) &&
        (colorAssignmentSelector->getSelection() < puzzle->colorNumber()) &&
        (!assmThread || (assmThread->getProblem() != problemSelector->getSelection()))) {

      // check, if the given color is already added
      if (colconstrList->GetSortByResult()) {
        if (puzzle->probPlacementAllowed(problemSelector->getSelection(),
                                         colorAssignmentSelector->getSelection()+1,
                                         colconstrList->getSelection()+1)) {
          BtnColAdd->deactivate();
          BtnColRem->activate();
        } else {
          BtnColAdd->activate();
          BtnColRem->deactivate();
        }
      } else {
        if (puzzle->probPlacementAllowed(problemSelector->getSelection(),
                                         colconstrList->getSelection()+1,
                                         colorAssignmentSelector->getSelection()+1)) {
          BtnColAdd->deactivate();
          BtnColRem->activate();
        } else {
          BtnColAdd->activate();
          BtnColRem->deactivate();
        }
      }
    } else {
      BtnColAdd->deactivate();
      BtnColRem->deactivate();
    }

    // we can only change shapes, when a valid problem is selected
    // a valid shape is selected
    // the assembler is not running or not busy with out problem
    if ((problemSelector->getSelection() < puzzle->problemNumber()) &&
        (shapeAssignmentSelector->getSelection() < puzzle->shapeNumber()) &&
        (!assmThread || (assmThread->getProblem() != problemSelector->getSelection()))) {
      BtnSetResult->activate();

      // we can only add a shape, when it's not the result of the current problem
      if (puzzle->probGetResult(problemSelector->getSelection()) != shapeAssignmentSelector->getSelection())
        BtnAddShape->activate();
      else
        BtnAddShape->deactivate();

      bool found = false;

      for (unsigned int p = 0; p < puzzle->probShapeNumber(problemSelector->getSelection()); p++)
        if (puzzle->probGetShape(problemSelector->getSelection(), p) == shapeAssignmentSelector->getSelection()) {
          found = true;
          break;
        }

      if (found) {
        BtnRemShape->activate();
      } else {
        BtnRemShape->deactivate();
      }

    } else {
      BtnSetResult->deactivate();
      BtnAddShape->deactivate();
      BtnRemShape->deactivate();
    }

    // we can edit the groups, whe we have a problem with at leat one shape and
    // the assembler is not working on the current problem
    if ((problemSelector->getSelection() < puzzle->problemNumber()) &&
        (puzzle->probShapeNumber(problemSelector->getSelection()) > 0) &&
        (!assmThread || (assmThread->getProblem() != problemSelector->getSelection()))) {
      BtnGroup->activate();
    } else {
      BtnGroup->deactivate();
    }

  } else {

    // solution tab
    PcVis->setPuzzle(puzzle, prob);

    float finished = ((prob < puzzle->problemNumber()) && puzzle->probGetAssembler(prob)) ? puzzle->probGetAssembler(prob)->getFinished() : 0;

    if (prob < puzzle->problemNumber()) {

      // we have a valid problem selected, so update the information visible

      SolvingProgress->value(100*finished);
      SolvingProgress->show();

      {
        static char tmp[100];
        snprintf(tmp, 100, "%.4f%%", 100*finished);
        SolvingProgress->label(tmp);
      }

      unsigned long numSol = puzzle->probSolutionNumber(prob);

      if (numSol > 0) {

        SolutionSel->show();
        SolutionsInfo->show();

        SolutionSel->range(0, numSol-1);
        SolutionsInfo->value(numSol);

        // if we are in the solve tab and have a valid solution
        // we can activate that
        if (SolutionEmpty && (numSol > 0) && (TaskSelectionTab->value() == TabSolve))
          activateSolution(prob, 0);

      } else {

        SolutionSel->range(0, 0);
        SolutionSel->hide();
        SolutionsInfo->hide();
        SolutionAnim->hide();
        MovesInfo->hide();
      }

      if (puzzle->probNumAssembliesKnown(prob)) {
        OutputAssemblies->value(puzzle->probGetNumAssemblies(prob));
        OutputAssemblies->show();
      } else {
        OutputAssemblies->hide();
      }

      if (puzzle->probNumSolutionsKnown(prob)) {
        OutputSolutions->value(puzzle->probGetNumSolutions(prob));
        OutputSolutions->show();
      } else {
        OutputSolutions->hide();
      }

      // the placement browser can only be activated when an assember is available and not assembling is active
      if (puzzle->probGetAssembler(prob) && !assmThread && (puzzle->probGetSolveState(prob) != puzzle_c::SS_SOLVED)) {
        BtnPlacement->activate();
        BtnStep->activate();
      } else {
        BtnPlacement->deactivate();
        BtnStep->deactivate();
      }

      // the prepare button is only available, when a valid problem is selected
      // that problem has no assembler assigned and no thread is running
      if (!puzzle->probGetAssembler(prob) && !assmThread)
        BtnPrepare->activate();
      else
        BtnPrepare->deactivate();

    } else {

      // no valid problem available, hide all information

      SolutionSel->hide();
      SolutionsInfo->hide();
      OutputSolutions->hide();
      SolutionAnim->hide();
      MovesInfo->hide();

      SolvingProgress->hide();
      OutputAssemblies->hide();

      BtnPlacement->deactivate();
      BtnStep->deactivate();
      BtnPrepare->deactivate();
    }


    if (assmThread && (assmThread->getProblem() == prob)) {

      // a thread is currently running

      unsigned int ut;
      if (puzzle->probUsedTimeKnown(prob))
        ut = puzzle->probGetUsedTime(prob) + assmThread->getTime();
      else
        ut = assmThread->getTime();

      TimeUsed->value(timeToString(ut));
      if (finished != 0)
        TimeEst->value(timeToString(ut/finished-ut));
      else
        TimeEst->value("unknown");

      TimeUsed->show();
      TimeEst->show();

    } else {

      if ((prob < puzzle->problemNumber()) && puzzle->probUsedTimeKnown(prob)) {
        TimeUsed->value(timeToString(puzzle->probGetUsedTime(prob)));
        TimeUsed->show();
      } else {
        TimeUsed->hide();
      }

      TimeEst->hide();
    }

    if (assmThread) {

      switch(assmThread->currentAction()) {
      case assemblerThread::ACT_PREPARATION:
        OutputActivity->value("prepare");
        break;
      case assemblerThread::ACT_REDUCE:
        if (puzzle->probGetAssembler(prob)) {
          char tmp[20];
          snprintf(tmp, 20, "optimize piece %i", puzzle->probGetAssembler(prob)->getReducePiece());
          OutputActivity->value(tmp);
        } else {
          char tmp[20];
          snprintf(tmp, 20, "optimize piece %i", assmThread->currentActionParameter());
          OutputActivity->value(tmp);
        }
        break;
      case assemblerThread::ACT_ASSEMBLING:
        OutputActivity->value("assemble");
        break;
      case assemblerThread::ACT_DISASSEMBLING:
        OutputActivity->value("disassemble");
        break;
      case assemblerThread::ACT_PAUSING:
        OutputActivity->value("pause");
        break;
      case assemblerThread::ACT_FINISHED:
        OutputActivity->value("finished");
        break;
      case assemblerThread::ACT_WAIT_TO_STOP:
        OutputActivity->value("please wait");
        break;
      case assemblerThread::ACT_ERROR:
        OutputActivity->value("error");
        break;
      }

      if (assmThread->getProblem() == prob) {

        // for the actually solved problem we enable the stop button
        BtnStart->deactivate();
        BtnCont->deactivate();
        BtnStop->activate();

      } else {

        // all other problems can do nothing
        BtnStart->deactivate();
        BtnCont->deactivate();
        BtnStop->deactivate();
      }

    } else {

      pieceEdit->activate();

      // no thread currently calculating

      // so we can not stop the threas
      BtnStop->deactivate();

      if (prob < puzzle->problemNumber()) {

        // a valid problem is selected

        switch(puzzle->probGetSolveState(prob)) {
        case puzzle_c::SS_UNSOLVED:
          OutputActivity->value("nothing");
          BtnCont->deactivate();
          break;
        case puzzle_c::SS_SOLVED:
          OutputActivity->value("finished");
          BtnCont->deactivate();
          break;
        case puzzle_c::SS_SOLVING:
          OutputActivity->value("pause");
          BtnCont->activate();
          break;
        }

        // if we have a result and at least one piece, we can give it a try
        if ((puzzle->probPieceNumber(prob) > 0) &&
            (puzzle->probGetResult(prob) < puzzle->shapeNumber()))
          BtnStart->activate();
        else
          BtnStart->deactivate();

      } else {

        // no start possible, when no valid problem selected
        BtnStart->deactivate();
        BtnCont->deactivate();
      }
    }
  }

  TaskSelectionTab->redraw();
}

void UserInterface::update(void) {

  if (assmThread) {

    // check, if the thread has thrown an exception, if so rethrow it
    if (assmThread->getAssertException()) {

      assertWindow * aw = new assertWindow("Because of an internal error the current puzzle\n"
                                           "can not be solved\n",
                                           assmThread->getAssertException());

      aw->show();

      while (aw->visible())
        Fl::wait();

      delete aw;
      delete assmThread;
      assmThread = 0;
      updateInterface();
      return;
    }

    // check, if the thread has stopped, if so then delete the object
    if ((assmThread->currentAction() == assemblerThread::ACT_PAUSING) ||
        (assmThread->currentAction() == assemblerThread::ACT_FINISHED)) {

      delete assmThread;
      assmThread = 0;

    } else if (assmThread->currentAction() == assemblerThread::ACT_ERROR) {

      unsigned int selectShape = 0xFFFFFFFF;

      switch(assmThread->getErrorState()) {
      case assembler_c::ERR_TOO_MANY_UNITS:
        fl_message("Pieces contain %i units too many", assmThread->getErrorParam());
        break;
      case assembler_c::ERR_TOO_FEW_UNITS:
        fl_message("Pieces contain %i units less than required", assmThread->getErrorParam());
        break;
      case assembler_c::ERR_CAN_NOT_PLACE:
        fl_message("Piece %i can be placed nowhere within the result", assmThread->getErrorParam()+2);
        selectShape = assmThread->getErrorParam()+1;
        break;
      case assembler_c::ERR_CAN_NOT_RESTORE_VERSION:
        fl_message("Impossible to restore the saved state because the internal format changed.\n"
                   "You either have to start from the beginning or finish with the old version of BurrTools, sorry");
        break;
      case assembler_c::ERR_CAN_NOT_RESTORE_SYNTAX:
        fl_message("Impossible to restore the saved state because something with the data is wrong.\n"
                   "You have to start from the beginning, sorry");
        break;
      case assembler_c::ERR_PIECE_WITH_VARICUBE:
        fl_message("Shape %i is used as piece and contains variable cubes, that is not allowed", assmThread->getErrorParam()+1);
        selectShape = assmThread->getErrorParam();
        break;
      default:
        break;
      }

      if (selectShape < puzzle->shapeNumber()) {
        TaskSelectionTab->value(TabPieces);
        PcSel->setSelection(selectShape);
        activateShape(PcSel->getSelection());
        updateInterface();
        StatPieceInfo(PcSel->getSelection());
      }

      delete assmThread;
      assmThread = 0;
    }

    // update the window, either when the thread stopped and so the buttons need to
    // be updated, or then the thread works for the currently selected problem
    if (!assmThread || assmThread->getProblem() == solutionProblem->getSelection())
      updateInterface();
  }
}

void UserInterface::Toggle3DView(void)
{
  // select the pieces tab, as exchanging widgets while they are invisible
  // didn't work. Save the current tab bevore that
  TaskSelectionTab->when(0);
  Fl_Widget *v = TaskSelectionTab->value();
  if (v != TabPieces) TaskSelectionTab->value(TabPieces);

  // first move the tile so that the widget at the
  // bottom is visible
  Fl_Widget * pos = (is3DViewBig)
    ? (pieceEdit->parent())
    : (View3D->parent());
  Fl_Tile * tile = (Fl_Tile *)(pos->parent());

  int ytile = pos->y();

  tile->position(0, ytile, 0, 200);

  // exchange vidget positions
  Fl_Group * tmp = pieceEdit->parent();
  View3D->parent()->add(pieceEdit);
  tmp->add(View3D);

  int x = pieceEdit->x();
  int y = pieceEdit->y();
  int w = pieceEdit->w();
  int h = pieceEdit->h();

  // exchange sizes
  pieceEdit->resize(View3D->x(), View3D->y(), View3D->w(), View3D->h());
  View3D->resize(x, y, w, h);

  is3DViewBig = !is3DViewBig;

  if (is3DViewBig)
    pieceEdit->parent()->resizable(pieceEdit);
  else
    View3D->parent()->resizable(View3D);

  // now move the tile back to its position
  tile->position(0, 200, 0, ytile);

  // restore the old selected tab
  if (v != TabPieces) TaskSelectionTab->value(v);
  TaskSelectionTab->when(FL_WHEN_CHANGED);
}

void UserInterface::Big3DView(void) {
  if (!is3DViewBig) Toggle3DView();
  View3D->show();
  redraw();
}

void UserInterface::Small3DView(void) {
  if (is3DViewBig) Toggle3DView();
  pieceEdit->show();
  redraw();
}

int UserInterface::handle(int event) {

  if (Fl_Double_Window::handle(event))
    return 1;

  switch(event) {
  case FL_SHORTCUT:
    if (Fl::event_length()) {
      switch (Fl::event_text()[0]) {
      case '+':
        if (TaskSelectionTab->value() == TabPieces) {
          pieceEdit->setZ(pieceEdit->getZ()+1);
          return 1;
        }
        break;
      case '-':
        if (TaskSelectionTab->value() == TabPieces) {
          if (pieceEdit->getZ() > 0)
            pieceEdit->setZ(pieceEdit->getZ()-1);
          return 1;
        }
        break;
      }
    }
    switch(Fl::event_key()) {
      case FL_F + 5:
        if (TaskSelectionTab->value() == TabPieces) {
          editChoice->select(0);
          return 1;
        }
        break;
      case FL_F + 6:
        if (TaskSelectionTab->value() == TabPieces) {
          editChoice->select(1);
          return 1;
        }
        break;
      case FL_F + 7:
        if (TaskSelectionTab->value() == TabPieces) {
          editChoice->select(2);
          return 1;
        }
        break;
      case FL_F + 8:
        if (TaskSelectionTab->value() == TabPieces) {
          editChoice->select(3);
          return 1;
        }
        break;
    }
  }

  return 0;
}

#define SZ_WINDOW_X 800                        // initial size of the window
#define SZ_WINDOW_Y 600
#define SZ_MENU_Y 25                           // hight of the menu
#define SZ_STATUS_Y 25
#define SZ_TOOL_X 325                          // initial width of the toolbar
#define SZ_TAB_Y 20                            // hight of the tabs in a tab
#define SZ_GAP 5                               // gap between elements
#define SZ_CONTENT_START_Y SZ_MENU_Y           // y start of the content area
#define SZ_CONTENT_Y (SZ_WINDOW_Y - SZ_MENU_Y - SZ_STATUS_Y) // initial hight of the content of the window
#define SZ_3DAREA_X (SZ_WINDOW_X - SZ_TOOL_X)
#define SZ_BUTTON_Y 20
#define SZ_BUTTON2_Y 25
#define SZ_TEXT_Y 15
#define SZ_SEPARATOR_Y 10
#define SZ_TOOLTAB_Y (115+20)

void UserInterface::CreateShapeTab(int x, int y, int w, int h) {

  TabPieces = new Fl_Group(x, y, w, h, "Shapes");
  TabPieces->tooltip("Edit shapes");
  TabPieces->clear_visible_focus();

  x += SZ_GAP; y++;  w -= 2*SZ_GAP; h -= SZ_GAP + 1;

  Fl_Group * tile = new Fl_Tile(x, y, w, h);

  // calculate hight of different groups
  const int numGroups = 8;

  const int pieceFixedHight = SZ_SEPARATOR_Y + SZ_BUTTON_Y + SZ_GAP;
  const int colorsFixedHight = SZ_SEPARATOR_Y + SZ_BUTTON_Y + SZ_GAP;
  const int editFixedHight = SZ_SEPARATOR_Y + SZ_TOOLTAB_Y + 2*SZ_GAP + SZ_BUTTON_Y;

  int hi = h - pieceFixedHight - colorsFixedHight - editFixedHight;

  bt_assert(hi > 30);

  int pieceHight = 3*hi/numGroups + pieceFixedHight;
  int colorsHight = hi/numGroups + colorsFixedHight;
  int editHight = hi - (hi/numGroups) * 4 + editFixedHight;

  {
    int lh = pieceHight;

    Fl_Group* group = new Fl_Group(x, y, w, lh);
    group->box(FL_FLAT_BOX);
    new Separator(x, y, w, SZ_SEPARATOR_Y, "Shapes", false);
    y += SZ_SEPARATOR_Y;
    lh -= SZ_SEPARATOR_Y;

    int bw = (w - 5*SZ_GAP - 2*SZ_BUTTON_Y) / 4;
    {
      Fl_Group * o = new Fl_Group(x+0*SZ_GAP+0*bw, y, bw+SZ_GAP, SZ_BUTTON_Y);
      BtnNewShape = new FlatButton(x+0*SZ_GAP+0*bw, y, bw, SZ_BUTTON_Y, "New", " Add another piece ", cb_NewShape_stub, this);
      o->resizable(BtnNewShape);
      o->end();
    }
    {
      Fl_Group * o = new Fl_Group(x+1*SZ_GAP+1*bw, y, bw+SZ_GAP, SZ_BUTTON_Y);
      BtnDelShape = new FlatButton(x+1*SZ_GAP+1*bw, y, bw, SZ_BUTTON_Y, "Delete", " Delete selected piece ", cb_DeleteShape_stub, this);
      o->resizable(BtnDelShape);
      o->end();
    }

    {
      Fl_Group * o = new Fl_Group(x+2*SZ_GAP+2*bw, y, bw+SZ_GAP, SZ_BUTTON_Y);
      BtnCpyShape = new FlatButton(x+2*SZ_GAP+2*bw, y, bw, SZ_BUTTON_Y, "Copy", " Copy selected piece ", cb_CopyShape_stub, this);
      o->resizable(BtnCpyShape);
      o->end();
    }

    {
      Fl_Group * o = new Fl_Group( x+3*SZ_GAP+3*bw, y, w-3*bw+4*SZ_GAP-2*SZ_BUTTON_Y, SZ_BUTTON_Y);
      BtnRenShape = new FlatButton(x+3*SZ_GAP+3*bw, y, w-3*bw-5*SZ_GAP-2*SZ_BUTTON_Y, SZ_BUTTON_Y, "Label", " Give the selected shape a name ", cb_NameShape_stub, this);
      o->resizable(BtnRenShape);
      o->end();
    }

    BtnShapeLeft = new FlatButton(x+w-SZ_GAP-2*SZ_BUTTON_Y, y, SZ_BUTTON_Y, SZ_BUTTON_Y, "@-14->", " Exchange current shape with previous shape ", cb_ShapeLeft_stub, this);
    BtnShapeRight = new FlatButton(x+w-SZ_BUTTON_Y,          y, SZ_BUTTON_Y, SZ_BUTTON_Y, "@-16->", " Exchange current shape with next shape ", cb_ShapeRight_stub, this);

    y += SZ_BUTTON_Y + SZ_GAP;
    lh -= SZ_BUTTON_Y + SZ_GAP;

    PcSel = new PieceSelector(x, y, w, lh, puzzle);
    Fl_Group * selGroup = new BlockListGroup(x, y, w, lh, PcSel);
    selGroup->callback(cb_PcSel_stub, this);
    selGroup->tooltip(" Select the shape that you want to edit ");

    y += lh;

    group->resizable(selGroup);
    group->end();
  }

  {
    int lh = editHight;

    Fl_Group* group = new Fl_Group(x, y, w, lh);
    group->box(FL_FLAT_BOX);

    new Separator(x, y, w, SZ_SEPARATOR_Y, "Edit", true);
    y += SZ_SEPARATOR_Y;
    lh -= SZ_SEPARATOR_Y;

    pieceTools = new ToolTab(x, y, w, SZ_TOOLTAB_Y);
    pieceTools->callback(cb_TransformPiece_stub, this);
    pieceTools->end();
    y += SZ_TOOLTAB_Y + SZ_GAP;
    lh -= SZ_TOOLTAB_Y + SZ_GAP;

    int xpos = x;

    editChoice = new ButtonGroup(xpos, y, 4*SZ_BUTTON2_Y, SZ_BUTTON2_Y);

    Fl_Button * b;
    b = editChoice->addButton(xpos+0*SZ_BUTTON2_Y, y, SZ_BUTTON2_Y, SZ_BUTTON2_Y);
    b->image(new Fl_Pixmap(TB_Color_Pen_Fixed_xpm));
    b->tooltip(" Add normal voxels to the shape F5 ");

    b = editChoice->addButton(xpos+1*SZ_BUTTON2_Y, y, SZ_BUTTON2_Y, SZ_BUTTON2_Y);
    b->image(new Fl_Pixmap(TB_Color_Pen_Variable_xpm));
    b->tooltip(" Add variable voxels to the shape F6 ");

    b = editChoice->addButton(xpos+2*SZ_BUTTON2_Y, y, SZ_BUTTON2_Y, SZ_BUTTON2_Y);
    b->image(new Fl_Pixmap(TB_Color_Eraser_xpm));
    b->tooltip(" Remove voxels from the shape F7 ");

    b = editChoice->addButton(xpos+3*SZ_BUTTON2_Y, y, SZ_BUTTON2_Y, SZ_BUTTON2_Y);
    b->image(new Fl_Pixmap(TB_Color_Brush_xpm));
    b->tooltip(" Change the constrain color of voxels in the shape F8 ");

    editChoice->callback(cb_EditChoice_stub, this);

    xpos += 4*SZ_BUTTON2_Y + SZ_GAP;

    editMode = new ButtonGroup(xpos, y, 2*SZ_BUTTON2_Y, SZ_BUTTON2_Y);
    b = editMode->addButton(xpos+0*SZ_BUTTON2_Y, y, SZ_BUTTON2_Y, SZ_BUTTON2_Y);
    b->image(new Fl_Pixmap(TB_Color_Mouse_Rubber_Band_xpm));
    b->tooltip(" Make changes by dragging rectangular areas in the grid editor ");

    b = editMode->addButton(xpos+1*SZ_BUTTON2_Y, y, SZ_BUTTON2_Y, SZ_BUTTON2_Y);
    b->image(new Fl_Pixmap(TB_Color_Mouse_Drag_xpm));
    b->tooltip(" Make changes by painting in the grid editor ");

    editMode->callback(cb_EditMode_stub, this);

    xpos += 2*SZ_BUTTON2_Y + SZ_GAP;

    ToggleButton * btn;

    btn = new ToggleButton(xpos+0*SZ_BUTTON2_Y, y, SZ_BUTTON2_Y, SZ_BUTTON2_Y, cb_EditSym_stub, this, SquareEditor::TOOL_MIRROR_X);
    btn->image(new Fl_Pixmap(TB_Color_Symmetrical_X_xpm));
    btn->tooltip(" Toggle mirroring along the y-z-plane ");

    btn = new ToggleButton(xpos+1*SZ_BUTTON2_Y, y, SZ_BUTTON2_Y, SZ_BUTTON2_Y, cb_EditSym_stub, this, SquareEditor::TOOL_MIRROR_Y);
    btn->image(new Fl_Pixmap(TB_Color_Symmetrical_Y_xpm));
    btn->tooltip(" Toggle mirroring along the x-z-plane ");

    btn = new ToggleButton(xpos+2*SZ_BUTTON2_Y, y, SZ_BUTTON2_Y, SZ_BUTTON2_Y, cb_EditSym_stub, this, SquareEditor::TOOL_MIRROR_Z);
    btn->image(new Fl_Pixmap(TB_Color_Symmetrical_Z_xpm));
    btn->tooltip(" Toggle mirroring along the x-y-plane ");

    xpos += 3*SZ_BUTTON2_Y + SZ_GAP;

    btn = new ToggleButton(xpos+0*SZ_BUTTON2_Y, y, SZ_BUTTON2_Y, SZ_BUTTON2_Y, cb_EditSym_stub, this, SquareEditor::TOOL_STACK_X);
    btn->image(new Fl_Pixmap(TB_Color_Columns_X_xpm));
    btn->tooltip(" Toggle drawing in all x layers ");

    btn = new ToggleButton(xpos+1*SZ_BUTTON2_Y, y, SZ_BUTTON2_Y, SZ_BUTTON2_Y, cb_EditSym_stub, this, SquareEditor::TOOL_STACK_Y);
    btn->image(new Fl_Pixmap(TB_Color_Columns_Y_xpm));
    btn->tooltip(" Toggle drawing in all y layers ");

    btn = new ToggleButton(xpos+2*SZ_BUTTON2_Y, y, SZ_BUTTON2_Y, SZ_BUTTON2_Y, cb_EditSym_stub, this, SquareEditor::TOOL_STACK_Z);
    btn->image(new Fl_Pixmap(TB_Color_Columns_Z_xpm));
    btn->tooltip(" Toggle drawing in all z layers ");

    xpos += 3*SZ_BUTTON2_Y * SZ_GAP;

    y += SZ_BUTTON2_Y + SZ_GAP;
    lh -= SZ_BUTTON2_Y + SZ_GAP;

    pieceEdit = new VoxelEditGroup(x, y, w, lh, puzzle);
    pieceEdit->callback(cb_pieceEdit_stub, this);
    pieceEdit->end();
    pieceEdit->editType(SquareEditor::EDT_RUBBER);
    y += lh;

    group->resizable(pieceEdit);
    group->end();

    {
      Fl_Box * b = new Fl_Box(group->x(), group->y(), group->w(), group->h());
      b->hide();
      tile->resizable(b);
    }
  }

  {
    int lh = colorsHight;

    Fl_Group* group = new Fl_Group(x, y, w, lh);
    group->box(FL_FLAT_BOX);
    new Separator(x, y, w, SZ_SEPARATOR_Y, "Colors", true);
    y += SZ_SEPARATOR_Y;
    lh -= SZ_SEPARATOR_Y;

    int bw = (w - 2*SZ_GAP) / 3;
    {
      Fl_Group * o = new Fl_Group(x, y, bw+SZ_GAP, SZ_BUTTON_Y);
      BtnNewColor = new FlatButton(x          , y, bw, SZ_BUTTON_Y, "Add", " Add another color ", cb_AddColor_stub, this);
      o->resizable(BtnNewColor);
      o->end();
    }
    {
      Fl_Group * o = new Fl_Group(x+bw+SZ_GAP, y, bw+SZ_GAP, SZ_BUTTON_Y);
      BtnDelColor = new FlatButton(x+SZ_GAP+bw, y, bw, SZ_BUTTON_Y, "Remove", " Remove selected color ", cb_RemoveColor_stub, this);
      o->resizable(BtnDelColor);
      o->end();
    }
    {
      Fl_Group * o = new Fl_Group(x+2*(bw+SZ_GAP), y, bw+SZ_GAP, SZ_BUTTON_Y);
      BtnChnColor = new FlatButton(x+2*(SZ_GAP+bw), y, w-2*SZ_GAP-2*bw, SZ_BUTTON_Y, "Edit", " Change selected color ", cb_ChangeColor_stub, this);
      o->resizable(BtnChnColor);
      o->end();
    }
    y += SZ_BUTTON_Y + SZ_GAP;
    lh -= SZ_BUTTON_Y + SZ_GAP;

    colorSelector = new ColorSelector(x, y, w, lh, puzzle, true);
    Fl_Group * colGroup = new BlockListGroup(x, y, w, lh, colorSelector);
    colGroup->callback(cb_ColSel_stub, this);
    colGroup->tooltip(" Select color to use for all editing operations ");

    y += lh;

    group->resizable(colorSelector);
    group->end();
  }

  tile->end();

  TabPieces->resizable(tile);
  TabPieces->end();

  Fl_Group::current()->resizable(TabPieces);
}

void UserInterface::CreateProblemTab(int x, int y, int w, int h) {

  TabProblems = new Fl_Group(x, y, w, h, "Problems");
  TabProblems->tooltip("Edit problems");
  TabProblems->hide();
  TabProblems->clear_visible_focus();

  x += SZ_GAP; y++; w -= 2*SZ_GAP; h -= SZ_GAP + 1;

  Fl_Group * tile = new Fl_Tile(x, y, w, h);

  // calculate hight of different groups
  const int problemsFixedHight = SZ_SEPARATOR_Y + SZ_BUTTON_Y + SZ_GAP;
  const int colorsFixedHight = SZ_SEPARATOR_Y + SZ_GAP;
  const int matrixFixedHight = SZ_BUTTON_Y + SZ_GAP;
  const int shapesFixedHight = SZ_SEPARATOR_Y + SZ_BUTTON_Y + 2*SZ_GAP;
  const int piecesFixedHight = SZ_SEPARATOR_Y + SZ_BUTTON_Y + SZ_GAP;

  int hi = h - problemsFixedHight - colorsFixedHight - matrixFixedHight - shapesFixedHight - piecesFixedHight;

  bt_assert(hi > 30);

  int problemsHight = hi/5 + problemsFixedHight;
  int colorsHight = hi/5 + colorsFixedHight;
  int matrixHight = hi/5 + matrixFixedHight;
  int shapesHight = hi/5 + shapesFixedHight;
  int piecesHight = hi - (hi/5) * 4 + piecesFixedHight;

  {
    int lh = problemsHight;

    Fl_Group* group = new Fl_Group(x, y, w, lh);
    group->box(FL_FLAT_BOX);
    new Separator(x, y, w, SZ_SEPARATOR_Y, "Problems", false);
    y += SZ_SEPARATOR_Y;
    lh -= SZ_SEPARATOR_Y;

    int bw = (w - 5*SZ_GAP - 2*SZ_BUTTON_Y) / 4;

    {
      Fl_Group * o = new Fl_Group(x+0*SZ_GAP+0*bw, y, bw+SZ_GAP, SZ_BUTTON_Y);
      BtnNewProb = new FlatButton(x+0*SZ_GAP+0*bw, y, bw, SZ_BUTTON_Y, "New", " Add another problem ", cb_NewProblem_stub, this);
      o->resizable(BtnNewProb);
      o->end();
    }
    {
      Fl_Group * o = new Fl_Group(x+1*SZ_GAP+1*bw, y, bw+SZ_GAP, SZ_BUTTON_Y);
      BtnDelProb = new FlatButton(x+1*SZ_GAP+1*bw, y, bw, SZ_BUTTON_Y, "Delete", " Delete selected problem ", cb_DeleteProblem_stub, this);
      o->resizable(BtnDelProb);
      o->end();
    }
    {
      Fl_Group * o = new Fl_Group(x+2*SZ_GAP+2*bw, y, bw+SZ_GAP, SZ_BUTTON_Y);
      BtnCpyProb = new FlatButton(x+2*SZ_GAP+2*bw, y, bw, SZ_BUTTON_Y, "Copy", " Copy selected problem ", cb_CopyProblem_stub, this);
      o->resizable(BtnCpyProb);
      o->end();
    }
    {
      Fl_Group * o = new Fl_Group(x+3*SZ_GAP+3*bw, y, w-4*SZ_GAP-3*bw-2*SZ_BUTTON_Y, SZ_BUTTON_Y);
      BtnRenProb = new FlatButton(x+3*SZ_GAP+3*bw, y, w-5*SZ_GAP-3*bw-2*SZ_BUTTON_Y, SZ_BUTTON_Y, "Label", " Rename selected problem ", cb_RenameProblem_stub, this);
      o->resizable(BtnRenProb);
      o->end();
    }

    BtnProbLeft = new FlatButton(x+w-SZ_GAP-2*SZ_BUTTON_Y, y, SZ_BUTTON_Y, SZ_BUTTON_Y, "@-14->", " Exchange current problem with previous problem ", cb_ProblemLeft_stub, this);
    BtnProbRight = new FlatButton(x+w-SZ_BUTTON_Y,          y, SZ_BUTTON_Y, SZ_BUTTON_Y, "@-16->", " Exchange current problem with next problem ", cb_ProblemRight_stub, this);

    y += SZ_BUTTON_Y + SZ_GAP;
    lh -= SZ_BUTTON_Y + SZ_GAP;

    problemSelector = new ProblemSelector(x, y, w, lh, puzzle);
    Fl_Group * probGroup = new BlockListGroup(x, y, w, lh, problemSelector);
    probGroup->callback(cb_ProbSel_stub, this);
    probGroup->tooltip(" Select problem to edit ");

    group->resizable(probGroup);
    group->end();

    y += lh;
  }

  {
    int lh = shapesHight;

    Fl_Group* group = new Fl_Group(x, y, w, lh);
    group->box(FL_FLAT_BOX);

    int hw = (w - SZ_GAP)/4;

    new Separator(x, y, w, SZ_SEPARATOR_Y, "Piece assigment", true);
    y += SZ_SEPARATOR_Y;
    lh -= SZ_SEPARATOR_Y;

    {
      Fl_Group * o = new Fl_Group(x,           y, 3*hw+SZ_GAP, SZ_BUTTON_Y);
      problemResult = new ResultViewer(x, y, 3*hw, SZ_BUTTON_Y, puzzle);
      problemResult->tooltip(" The result shape for the current problem ");
      o->resizable(problemResult);
      o->end();
    }
    {
      Fl_Group * o = new Fl_Group(x+3*hw+SZ_GAP, y, w-3*hw, SZ_BUTTON_Y);
      BtnSetResult = new FlatButton(x+3*hw+SZ_GAP, y, w-3*hw-SZ_GAP, SZ_BUTTON_Y, "Set Result", " Set selected shape as result ", cb_ShapeToResult_stub, this);
      o->resizable(BtnSetResult);
      o->end();
    }

    y += SZ_BUTTON_Y + SZ_GAP;
    lh -= SZ_BUTTON_Y + SZ_GAP;

    shapeAssignmentSelector = new PieceSelector(x, y, w, lh-SZ_GAP, puzzle);
    Fl_Group * shapeGroup = new BlockListGroup(x, y, w, lh-SZ_GAP, shapeAssignmentSelector);
    shapeGroup->callback(cb_ShapeSel_stub, this);
    shapeGroup->tooltip(" Select a shape to set as result or to add or remove from problem ");

    group->resizable(shapeGroup);
    group->end();

    y += lh;
  }

  {
    int lh = piecesHight;

    Fl_Group* group = new Fl_Group(x, y, w, lh);
    group->box(FL_FLAT_BOX);

    int hw = (w - 4*SZ_GAP-2*SZ_BUTTON_Y)/3;

    {
      Fl_Group * o = new Fl_Group(x,           y, hw+SZ_GAP, SZ_BUTTON_Y);
      BtnAddShape = new FlatButton(x,           y, hw         , SZ_BUTTON_Y, "+1", " Add another one of the selected shape ", cb_AddShapeToProblem_stub, this);
      o->resizable(BtnAddShape);
      o->end();
    }
    {
      Fl_Group * o = new Fl_Group(x+hw+SZ_GAP, y, hw+SZ_GAP, SZ_BUTTON_Y);
      BtnRemShape = new FlatButton(x+hw+SZ_GAP, y, hw, SZ_BUTTON_Y, "-1", " Remove one of the selected shapes ", cb_RemoveShapeFromProblem_stub, this);
      o->resizable(BtnRemShape);
      o->end();
    }
    {

      Fl_Group * o = new Fl_Group(x+2*(hw+SZ_GAP), y, w-2*hw-3*SZ_GAP-2*SZ_BUTTON_Y, SZ_BUTTON_Y);
      BtnGroup = new FlatButton(x+2*(hw+SZ_GAP)  , y, w-2*hw-4*SZ_GAP-2*SZ_BUTTON_Y, SZ_BUTTON_Y, "Group", " Create or edit groups ", cb_ShapeGroup_stub, this);
      o->resizable(BtnGroup);
      o->end();
    }

    BtnProbShapeLeft = new FlatButton(x+w-SZ_GAP-2*SZ_BUTTON_Y, y, SZ_BUTTON_Y, SZ_BUTTON_Y, "@-14->", " Exchange current shape with previous shape ", cb_ProbShapeLeft_stub, this);
    BtnProbShapeRight = new FlatButton(x+w-SZ_BUTTON_Y,          y, SZ_BUTTON_Y, SZ_BUTTON_Y, "@-16->", " Exchange current shape with next shape ", cb_ProbShapeRight_stub, this);

    y += SZ_BUTTON_Y + SZ_GAP;
    lh -= SZ_BUTTON_Y + SZ_GAP;

    PiecesCountList = new PiecesList(x, y, w, lh-SZ_GAP, puzzle);
    Fl_Group * shapeGroup = new BlockListGroup(x, y, w, lh, PiecesCountList);
    shapeGroup->callback(cb_PiecesClicked_stub, this);
    shapeGroup->tooltip(" Show which shapes are used in the current problem and how often they are used, can be used to select shapes ");

    group->resizable(shapeGroup);
    group->end();

    y += lh;
  }

  {
    int lh = colorsHight;

    Fl_Group* group = new Fl_Group(x, y, w, lh);
    group->box(FL_FLAT_BOX);

    new Separator(x, y, w, SZ_SEPARATOR_Y, "Color assigment", true);
    y += SZ_SEPARATOR_Y;
    lh -= SZ_SEPARATOR_Y;

    colorAssignmentSelector = new ColorSelector(x, y, w, lh, puzzle, false);
    Fl_Group * colGroup = new BlockListGroup(x, y, w, lh-SZ_GAP, colorAssignmentSelector);
    colGroup->callback(cb_ColorAssSel_stub, this);
    colGroup->tooltip(" Select color to add or remove from constraints ");

    group->resizable(colGroup);
    group->end();

    y += lh;
  }

  {
    int lh = matrixHight;

    Fl_Group* group = new Fl_Group(x, y, w, lh);
    group->box(FL_FLAT_BOX);

    int hw = (w - 2*SZ_GAP) / 3;

    {
      Fl_Group * o = new Fl_Group(x, y, hw+SZ_GAP, SZ_BUTTON_Y);
      BtnColSrtPc = new FlatButton(x, y, hw, SZ_BUTTON_Y, "Sort by Piece", " Sort color constraints by piece ", cb_CCSortByPiece_stub, this);
      BtnColSrtPc->deactivate();
      o->resizable(BtnColSrtPc);
      o->end();
    }

    BtnColAdd = new FlatButton(x+hw+SZ_GAP     , y, hw/2, SZ_BUTTON_Y, "@-12->", " Add color to constraint ", cb_AllowColor_stub, this);
    BtnColRem = new FlatButton(x+hw+SZ_GAP+hw/2, y, hw/2, SZ_BUTTON_Y, "@-18->", " Add color to constraint ", cb_DisallowColor_stub, this);

    {
      Fl_Group * o = new Fl_Group(x+2*hw+SZ_GAP, y, hw+SZ_GAP, SZ_BUTTON_Y);
      BtnColSrtRes = new FlatButton(x+2*hw+2*SZ_GAP, y, w-2*(hw+SZ_GAP), SZ_BUTTON_Y, "Sort by Result", " Sort Color Constraints by Result ", cb_CCSortByResult_stub, this);
      o->resizable(BtnColSrtRes);
      o->end();
    }

    y += SZ_GAP + SZ_BUTTON_Y;
    lh -= SZ_GAP + SZ_BUTTON_Y;

    colconstrList = new ColorConstraintsEdit(x, y, w, lh, puzzle);
    Fl_Group * colGroup = new ConstraintsGroup(x, y, w, lh, colconstrList);
    colGroup->callback(cb_ColorConstrSel_stub, this);
    colGroup->tooltip(" Color constraints for the current problem ");

    group->resizable(colGroup);
    group->end();

    y += lh;
  }

  tile->end();

  TabProblems->resizable(tile);
  TabProblems->end();
}

void UserInterface::CreateSolveTab(int x, int y, int w, int h) {

  TabSolve = new Fl_Group(x, y, w, h, "Solve");
  TabSolve->tooltip("Solve problems");
  TabSolve->hide();
  TabSolve->clear_visible_focus();

  x += SZ_GAP; y++; w -= 2*SZ_GAP; h -= SZ_GAP + 1;

  Fl_Group * tile = new Fl_Tile(x, y, w, h);

  // calculate hight of different groups
  const int paramsFixedHight = SZ_SEPARATOR_Y + 5*SZ_BUTTON_Y + 5*SZ_GAP +  5*SZ_TEXT_Y;
  const int solutionsFixedHight = SZ_SEPARATOR_Y + 2*SZ_BUTTON_Y + 2*SZ_GAP + 2*SZ_TEXT_Y;

  int hi = h - paramsFixedHight - solutionsFixedHight;

  bt_assert(hi > 30);

  int paramsHight = hi/2 + paramsFixedHight;
  int solutionsHight = hi - (hi/2) + solutionsFixedHight;

  {
    int lh = paramsHight;

    Fl_Group* group = new Fl_Group(x, y, w, lh);
    group->box(FL_FLAT_BOX);
    new Separator(x, y, w, SZ_SEPARATOR_Y, "Parameters", false);
    y += SZ_SEPARATOR_Y;
    lh -= SZ_SEPARATOR_Y;

    solutionProblem = new ProblemSelector(x, y, w, lh-SZ_GAP, puzzle);
    Fl_Group * shapeGroup = new BlockListGroup(x, y, w, lh-(paramsFixedHight-SZ_SEPARATOR_Y), solutionProblem);
    shapeGroup->callback(cb_SolProbSel_stub, this);
    shapeGroup->tooltip(" Select problem to solve ");

    group->resizable(shapeGroup);

    y += lh - (paramsFixedHight-SZ_SEPARATOR_Y);
    lh -= lh - (paramsFixedHight-SZ_SEPARATOR_Y);

    SolveDisasm = new Fl_Check_Button(x, y, w, SZ_BUTTON_Y, "Solve Disassembly");
    SolveDisasm->tooltip(" Do also try to disassemble the assembled puzzles. Only puzzles that can be disassembled will be added to solutions ");
    SolveDisasm->clear_visible_focus();
    y += SZ_BUTTON_Y;
    lh -= SZ_BUTTON_Y;

    JustCount = new Fl_Check_Button(x, y, w, SZ_BUTTON_Y, "Just Count");
    JustCount->tooltip(" Don\'t save the solutions, just count the number of them ");
    JustCount->clear_visible_focus();
    y += SZ_BUTTON_Y + SZ_GAP;
    lh -= SZ_BUTTON_Y + SZ_GAP;

    int bw = w - 3*SZ_GAP;

    int b0 = 8 * bw / 38;
    int b1 = 8 * bw / 38;
    int b2 = 14 * bw / 38;
    int b3 = bw-b0-b1-b2;

    {
      Fl_Group * o = new Fl_Group(x          , y, b0+SZ_GAP  , SZ_BUTTON_Y);
      BtnPrepare = new FlatButton(x, y, b1, SZ_BUTTON_Y, "Prepare", " Do the preparation phase and then stop, this removes old results ", cb_BtnPrepare_stub, this);
      o->resizable(BtnPrepare);
      o->end();
    }
    {
      Fl_Group * o = new Fl_Group(x+b0+SZ_GAP, y, b1+SZ_GAP  , SZ_BUTTON_Y);
      BtnStart = new FlatButton(x+b0+SZ_GAP, y, b1, SZ_BUTTON_Y, "Start", " Start new solving process, removing old result ", cb_BtnStart_stub, this);
      o->resizable(BtnStart);
      o->end();
    }
    {
      Fl_Group * o = new Fl_Group(x+2*SZ_GAP+b0+b1, y, b2+SZ_GAP  , SZ_BUTTON_Y);
      BtnCont = new FlatButton(x+2*SZ_GAP+b0+b1, y, b2, SZ_BUTTON_Y, "Continue", " Continue started process ", cb_BtnCont_stub, this);
      o->resizable(BtnCont);
      o->end();
    }
    {
      Fl_Group * o = new Fl_Group(x+3*SZ_GAP+b0+b1+b2, y, b3+SZ_GAP  , SZ_BUTTON_Y);
      BtnStop = new FlatButton(x+3*SZ_GAP+b0+b1+b2, y, b3, SZ_BUTTON_Y, "Stop", " Stop a currently running solution process ", cb_BtnStop_stub, this);
      o->resizable(BtnStop);
      o->end();
    }

    y += SZ_BUTTON_Y + SZ_GAP;
    lh -= SZ_BUTTON_Y + SZ_GAP;

    BtnPlacement = new FlatButton(x, y, (w-SZ_GAP)/2, SZ_BUTTON_Y, "Browse Placements", " Browse the calculated placement of pieces ", cb_BtnPlacementBrowser_stub, this);
    BtnStep = new FlatButton(x+(w-SZ_GAP)/2+SZ_GAP, y, (w-SZ_GAP)/2, SZ_BUTTON_Y, "Step", " Make one step in the assembler ", cb_BtnAssemblerStep_stub, this);

    y += SZ_BUTTON_Y + SZ_GAP;
    lh -= SZ_BUTTON_Y + SZ_GAP;

    SolvingProgress = new ProgressBar(x, y, w, SZ_BUTTON_Y);
    SolvingProgress->tooltip(" Percentage of solution space searched ");
    SolvingProgress->box(FL_ENGRAVED_BOX);
    SolvingProgress->selection_color((Fl_Color)4);
    SolvingProgress->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);
    SolvingProgress->labelcolor(fl_rgb_color(128, 128, 255));
    y += SZ_BUTTON_Y + SZ_GAP;
    lh -= SZ_BUTTON_Y + SZ_GAP;

    OutputActivity = new Fl_Output(x+w/2, y, w/2, SZ_TEXT_Y, "Activity:");
    OutputActivity->box(FL_FLAT_BOX);
    OutputActivity->color(FL_BACKGROUND_COLOR);
    OutputActivity->tooltip(" What is currently done ");
    OutputActivity->clear_visible_focus();
    y += SZ_TEXT_Y + SZ_GAP;
    lh -= SZ_TEXT_Y + SZ_GAP;

    OutputAssemblies = new Fl_Value_Output(x+w/2, y, w/2, SZ_TEXT_Y, "Assemblies:");
    OutputAssemblies->box(FL_FLAT_BOX);
    OutputAssemblies->step(1);   // make output NOT use scientific presentation for big numbers
    OutputAssemblies->tooltip(" Number of assemblies found so far ");
    y += SZ_TEXT_Y;
    lh -= SZ_TEXT_Y;

    OutputSolutions = new Fl_Value_Output(x+w/2, y, w/2, SZ_TEXT_Y, "Solutions:");
    OutputSolutions->box(FL_FLAT_BOX);
    OutputSolutions->step(1);    // make output NOT use scientific presentation for big numbers
    OutputSolutions->tooltip(" Number of solutions (assemblies that can be disassembled) found so far ");
    y += SZ_TEXT_Y;
    lh -= SZ_TEXT_Y;

    TimeUsed = new Fl_Output(x+w/2, y, w/2, SZ_TEXT_Y, "Time used:");
    TimeUsed->box(FL_NO_BOX);
    y += SZ_TEXT_Y;
    lh -= SZ_TEXT_Y;

    TimeEst = new Fl_Output(x+w/2, y, w/2, SZ_TEXT_Y, "Time left:");
    TimeEst->box(FL_NO_BOX);
    TimeEst->tooltip(" This is a very approximate estimate and can be totally wrong, to take with a grain of salt ");
    y += SZ_TEXT_Y;
    lh -= SZ_TEXT_Y;

    bt_assert(lh == 0);

    group->end();
  }

  {
    int lh = solutionsHight;

    Fl_Group* group = new Fl_Group(x, y, w, lh);
    group->box(FL_FLAT_BOX);
    new Separator(x, y, w, SZ_SEPARATOR_Y, "Solutions", true);
    y += SZ_SEPARATOR_Y;
    lh -= SZ_SEPARATOR_Y;

    SolutionsInfo = new Fl_Value_Output(x+w/2, y, w/2, SZ_TEXT_Y);
    SolutionsInfo->tooltip(" Number of solutions ");
    SolutionsInfo->box(FL_FLAT_BOX);
    y += SZ_TEXT_Y;
    lh -= SZ_TEXT_Y;

    SolutionSel = new Fl_Value_Slider(x, y, w, SZ_BUTTON_Y, "Solution");
    SolutionSel->tooltip(" Select one Solution ");
    SolutionSel->type(1);
    SolutionSel->step(1);
    SolutionSel->callback(cb_SolutionSel_stub, this);
    SolutionSel->align(FL_ALIGN_TOP_LEFT);
    SolutionSel->box(FL_THIN_DOWN_BOX);
    y += SZ_BUTTON_Y + SZ_GAP;
    lh -= SZ_BUTTON_Y + SZ_GAP;

    MovesInfo = new Fl_Output(x+40, y, w-40, SZ_TEXT_Y);
    MovesInfo->tooltip(" Steps for complete disassembly ");
    MovesInfo->box(FL_FLAT_BOX);
    MovesInfo->color(FL_BACKGROUND_COLOR);
    y += SZ_TEXT_Y;
    lh -= SZ_TEXT_Y;

    SolutionAnim = new Fl_Value_Slider(x, y, w, SZ_BUTTON_Y, "Move");
    SolutionAnim->tooltip(" Animate the disassembly ");
    SolutionAnim->type(1);
    SolutionAnim->step(0.1);
    SolutionAnim->callback(cb_SolutionAnim_stub, this);
    SolutionAnim->align(FL_ALIGN_TOP_LEFT);
    SolutionAnim->box(FL_THIN_DOWN_BOX);
    y += SZ_BUTTON_Y + SZ_GAP;
    lh -= SZ_BUTTON_Y + SZ_GAP;

    PcVis = new PieceVisibility(x, y, w, lh, puzzle);
    Fl_Group * shapeGroup = new BlockListGroup(x, y, w, lh, PcVis);
    shapeGroup->callback(cb_PcVis_stub, this);
    shapeGroup->tooltip(" Change appearance of the pieces between normal, grid and invisible ");

    group->resizable(shapeGroup);
    group->end();
  }
  tile->end();

  TabSolve->resizable(tile);
  TabSolve->end();
}

void UserInterface::activateConfigOptions(void) {

  if (config.useTooltips())
    Fl_Tooltip::enable();
  else
    Fl_Tooltip::disable();

  View3D->useLightning(config.useLightning());
}

UserInterface::UserInterface() : Fl_Double_Window(SZ_WINDOW_X, SZ_WINDOW_Y) {

  assmThread = 0;
  fname = 0;
  disassemble = 0;

  puzzle = new puzzle_c();
  changed = false;

  label("BurrTools - unknown");
  user_data((void*)(this));

  MainMenu = new Fl_Menu_Bar(0, 0, SZ_WINDOW_X, SZ_MENU_Y);
  MainMenu->copy(menu_MainMenu, this);
  MainMenu->box(FL_THIN_UP_BOX);

  Status = new StatusLine(0, SZ_MENU_Y + SZ_CONTENT_Y, SZ_WINDOW_X, SZ_STATUS_Y);
  Status->callback(cb_Status_stub, this);

  Fl_Tile * mainTile = new Fl_Tile(0, SZ_CONTENT_START_Y, SZ_WINDOW_X, SZ_CONTENT_Y);
  View3D = new View3dGroup(SZ_TOOL_X, SZ_CONTENT_START_Y, SZ_3DAREA_X, SZ_CONTENT_Y);
  new Fl_Group(0, SZ_CONTENT_START_Y, SZ_TOOL_X, SZ_CONTENT_Y);

  // this box paints the background behind the tab, because the tabs are partly transparent
  (new Fl_Box(FL_FLAT_BOX, 0, SZ_CONTENT_START_Y, SZ_TOOL_X, SZ_CONTENT_Y, 0))->color(FL_BACKGROUND_COLOR);

  // the tab for the tool bar
  TaskSelectionTab = new Fl_Tabs(0, SZ_CONTENT_START_Y, SZ_TOOL_X, SZ_CONTENT_Y);
  TaskSelectionTab->box(FL_THIN_UP_BOX);
  TaskSelectionTab->callback(cb_TaskSelectionTab_stub, this);
  TaskSelectionTab->clear_visible_focus();

  // the three tabs
  CreateShapeTab(  0, SZ_CONTENT_START_Y+SZ_TAB_Y, SZ_TOOL_X, SZ_CONTENT_Y-SZ_TAB_Y);
  CreateProblemTab(0, SZ_CONTENT_START_Y+SZ_TAB_Y, SZ_TOOL_X, SZ_CONTENT_Y-SZ_TAB_Y);
  CreateSolveTab(  0, SZ_CONTENT_START_Y+SZ_TAB_Y, SZ_TOOL_X, SZ_CONTENT_Y-SZ_TAB_Y);

  currentTab = 0;
  ViewSizes[0] = -1;
  ViewSizes[1] = -1;
  ViewSizes[2] = -1;

  resizable(mainTile);

  size_range(250, 400);
  resize(config.windowPosX(), config.windowPosY(), config.windowPosW(), config.windowPosH());

  if (!config.useRubberband())
    editMode->select(1);
  else
    editMode->select(0);

  is3DViewBig = true;
  shapeEditorWithBig3DView = true;

  updateInterface();
  activateClear();

  activateConfigOptions();
}

UserInterface::~UserInterface() {

  config.windowPos(x(), y(), w(), h());

  delete puzzle;

  if (fname) {
    delete [] fname;
    fname = 0;
  }

  if (disassemble) {
    delete disassemble;
    disassemble = 0;
  }
}
