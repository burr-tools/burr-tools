#include "MainWindow.h"

#include "config.h"

#ifdef HAVE_FLU
#include <FLU/Flu_File_Chooser.h>
#endif

static UserInterface * ui;

static void cb_TaskSelectionTab_stub(Fl_Widget* o, void* v) { ui->cb_TaskSelectionTab((Fl_Tabs*)o); }

static void cb_NewPiece_stub(Fl_Widget* o, void* v) { ui->cb_NewPiece(); }
static void cb_Delete_stub(Fl_Widget* o, void* v) { ui->cb_Delete(); }
static void cb_Copy_stub(Fl_Widget* o, void* v) { ui->cb_Copy(); }

static void cb_PcSel_stub(Fl_Widget* o, long v) { ui->cb_PcSel(v); }

static void cb_TransformPiece_stub(Fl_Widget* o, long v) { ui->cb_TransformPiece(); }

static void cb_pieceEdit_stub(Fl_Widget* o, void* v) { ui->cb_pieceEdit((VoxelEditGroup*)o); }

static void cb_TransformResult_stub(Fl_Widget* o, long v) { ui->cb_TransformResult(); }

static void cb_BtnStart_stub(Fl_Widget* o, void* v) { ui->cb_BtnStart(); }
static void cb_BtnCont_stub(Fl_Widget* o, void* v) { ui->cb_BtnCont(); }
static void cb_BtnStop_stub(Fl_Widget* o, void* v) { ui->cb_BtnStop(); }
static void cb_SolutionSel_stub(Fl_Widget* o, void* v) { ui->cb_SolutionSel((Fl_Value_Slider*)o); }
static void cb_SolutionAnim_stub(Fl_Widget* o, void* v) { ui->cb_SolutionAnim((Fl_Value_Slider*)o); }
static void cb_PcVis_stub(Fl_Widget* o, void* v) { ui->cb_PcVis(); }

static void cb_New_stub(Fl_Widget* o, void* v) { ui->cb_New(); }
static void cb_Load_stub(Fl_Widget* o, void* v) { ui->cb_Load(); }
static void cb_Save_stub(Fl_Widget* o, void* v) { ui->cb_Save(); }
static void cb_SaveAs_stub(Fl_Widget* o, void* v) { ui->cb_SaveAs(); }
static void cb_Quit_stub(Fl_Widget* o, void* v) { ui->cb_Quit(); }

void UserInterface::cb_TaskSelectionTab(Fl_Tabs* o) {
  if (o->value() == TabPieces) {
    activatePiece(activePiece);
  } else if(o->value() == TabResult) {
    activateResult();
  } else if(o->value() == TabSolve) {
    if (assmThread)
      activateSolution(int(SolutionSel->value()));
  }
}

void UserInterface::cb_NewPiece(void) {
  puzzle->addShape(6, 6, 6);
  activatePiece(puzzle->getShapeNumber()-1);
  PcSel2->setPuzzle(puzzle);
  changed = true;
}

void UserInterface::cb_Delete(void) {
  if (puzzle->getShapeNumber() > 0) {

    puzzle->removeShape(activePiece);

    if (activePiece >= puzzle->getShapeNumber())
      activatePiece(activePiece-1);
    else
      activatePiece(activePiece);

    PcSel2->setPuzzle(puzzle);

    changed = true;
  }
}

void UserInterface::cb_Copy(void) {
  puzzle->addShape(new voxel_c(puzzle->getShape(activePiece)), 1);
  activatePiece(puzzle->getShapeNumber()-1);
  PcSel2->setPuzzle(puzzle);
  changed = true;
}

void UserInterface::cb_PcSel(long reason) {

  switch(reason) {
  case PieceSelector::RS_CHANGEDSELECTION:
    activatePiece(PcSel2->getSelectedPiece());
    break;
  case PieceSelector::RS_CHANGEDNUMBER:
    changed = true;
    break;
  }
}

void UserInterface::cb_TransformPiece(void) { activatePiece(activePiece); }
void UserInterface::cb_TransformResult(void) { activateResult(); }

void UserInterface::cb_pieceEdit(VoxelEditGroup* o) {
  switch (o->getReason()) {
  case SquareEditor::RS_MOUSEMOVE:
    if (o->getMouse())
      View3D->setMarker(o->getMouseX(), o->getMouseY(), o->getMouseZ());
    else
      View3D->hideMarker();
    break;
  case SquareEditor::RS_CHANGESQUARE:
    changed = true;
    break;
  }

  View3D->redraw();
}


void UserInterface::removeAssmThread(void) {

  if(TaskSelectionTab->value() == TabSolve)
    activateSolution(-1);

  if (assmThread) {
    delete assmThread;
    assmThread = 0;
  }
}

void UserInterface::cb_BtnStart(void) {

  removeAssmThread();

  if (SolveDisasm->value() != 0)
    assmThread = new assemblerThread(puzzle, assemblerThread::SOL_DISASM, ReducePositions->value() != 0);
  else
    assmThread = new assemblerThread(puzzle, assemblerThread::SOL_SAVE_ASM, ReducePositions->value() != 0);

  if (assmThread->errors()) {

    fl_alert(assmThread->errors());
    delete assmThread;
    assmThread = 0;

  } else {

    assmThread->start();

    activateSolution(0);

    BtnStart->deactivate();
    BtnCont->deactivate();
    BtnStop->activate();
  }
}

void UserInterface::cb_BtnCont(void) {

  if (assmThread)
    assmThread->start();

  BtnStart->deactivate();
  BtnCont->deactivate();
  BtnStop->activate();
}

void UserInterface::cb_BtnStop(void) {

  if (assmThread)
    assmThread->stop();

  BtnStart->activate();
  BtnCont->activate();
  BtnStop->deactivate();
}

void UserInterface::cb_SolutionSel(Fl_Value_Slider* o) {
  activateSolution(int(o->value()));
}

void UserInterface::cb_SolutionAnim(Fl_Value_Slider* o) {
  o->take_focus();
  if (disassemble) {
    disassemble->setStep(o->value());
    View3D->redraw();
  }
}

void UserInterface::cb_PcVis(void) {
  View3D->redraw();
}

void UserInterface::cb_New(void) {
  if (!assmThread || assmThread->stopped()) {

    if (changed)
      if (fl_ask("Puzzle changed are you shure?") == 0)
        return;

    if (puzzle)
      delete puzzle;
    puzzle = new puzzle_c();

    if (fname) {
      delete [] fname;
      fname = 0;
    }
    PcSel2->setPuzzle(puzzle);
    changed = false;
    activatePiece(0);
  }
}

void UserInterface::tryToLoad(const char * f) {
  if (f) {
    ifstream instr(f);

    puzzle_c * p2 = new puzzle_c(&instr);

    if (!p2) {
      fl_alert("Could not load file, maybe not a puzzle?");
    } else {
      if (fname) delete [] fname;
      fname = new char[strlen(f)+1];
      strcpy(fname, f);

      char nm[300];
      snprintf(nm, 299, "BurrTools - %s", fname);
      mainWindow->label(nm);

      delete puzzle;
      puzzle = p2;

      activatePiece(0);
      PcSel2->setPuzzle(puzzle);
      TaskSelectionTab->value(TabPieces);

      removeAssmThread();

      changed = false;
    }
  }
}

void UserInterface::cb_Load(void) {
  if (!assmThread || assmThread->stopped()) {

    if (changed)
      if (fl_ask("Puzzle changed are you shure?") == 0)
        return;

#ifdef HAVE_FLU
    const char * f = flu_file_chooser("Load Puzzle", "*.puzzle", "");
#else    
    const char * f = fl_file_chooser("Load Puzzle", "*.puzzle", "");
#endif

    tryToLoad(f);
  }
}

void UserInterface::cb_Save(void) {

  if (!fname)
    cb_SaveAs();

  else {

    ofstream ostr(fname);
  
    if (ostr)
      puzzle->save(&ostr);
  
    if (!ostr)
      fl_alert("puzzle NOT saved!!");
  }
}

void UserInterface::cb_SaveAs(void) {

#ifdef HAVE_FLU
  const char * f = flu_file_chooser("Save Puzzle as", "*.puzzle", "");
#else  
  const char * f = fl_file_chooser("Save Puzzle as", "*.puzzle", "");
#endif  

  if (f) {
    ofstream ostr(f);

    if (ostr)
      puzzle->save(&ostr);

    if (!ostr)
      fl_alert("puzzle NOT saved!!!");

    if (fname) delete [] fname;
    fname = new char[strlen(f)+1];
    strcpy(fname, f);

    char nm[300];
    snprintf(nm, 299, "BurrTools - %s", fname);
    mainWindow->label(nm);

  }
}

void UserInterface::cb_Quit(void) {
  if (changed)
    if (fl_ask("Puzzle changed are you shure?") == 0)
      return;
  mainWindow->hide();
}

Fl_Menu_Item UserInterface::menu_MainMenu[] = {
  {"New",     0, cb_New_stub, 0, 0, 0, 0, 14, 56},
  {"Load",    0, cb_Load_stub, 0, 0, 0, 0, 14, 56},
  {"Save",    0, cb_Save_stub, 0, 0, 0, 0, 14, 56},
  {"Save as", 0, cb_SaveAs_stub, 0, 128, 0, 0, 14, 56},
  {"Quit",    0, cb_Quit_stub, 0, 0, 3, 0, 14, 1},
  {0}
};


void UserInterface::show(int argn, char ** argv) {
  mainWindow->show();

  if (argn == 2)
    tryToLoad(argv[1]);
}

void UserInterface::activatePiece(int number) {
  if ((number < puzzle->getShapeNumber()) && (number >= 0)) {

    voxel_c * p = puzzle->getShape(number);

    View3D->setVoxelSpace(p, number);
    pieceEdit->setVoxelSpace(p, number);

    pieceTools->setVoxelSpace(p);

    activePiece = number;

    PcSel2->setSelectedPiece(number);

  } else {

    View3D->setVoxelSpace(0, 0);
    pieceEdit->setVoxelSpace(0, 0);

    pieceTools->setVoxelSpace(0);

    activePiece = -1;
  }
}

void UserInterface::activateResult(void) {
  voxel_c * p = puzzle->getResult();

  View3D->setVoxelSpace(p, 255);
  resultEdit->setVoxelSpace(p, 255);
  resultTools->setVoxelSpace(p);
}

void UserInterface::activateSolution(unsigned int num) {

  if (disassemble) {
    delete disassemble;
    disassemble = 0;
  }

  if (assmThread && (assmThread->number() > num)) {

    int * pcNum = new int[puzzle->getShapeNumber()];

    int piece = 0;

    for (int i = 0; i < puzzle->getShapeNumber(); i++) {
      pcNum[i] = puzzle->getShapeCount(i);
      for (int j = 0; j < puzzle->getShapeCount(i); j++) {
        colors[2*piece] = i;
        colors[2*piece+1] = j;

        shifting[4*piece] = 0;
        shifting[4*piece+1] = 0;
        shifting[4*piece+2] = 0;
        shifting[4*piece+3] = 1;

        piece++;
      }
    }

    View3D->setVoxelSpace(assmThread->getAssm(num), shifting, visibility, 30, colors);

    PcVis->setPieceNumber(puzzle->getShapeNumber(), pcNum, visibility);

    if (assmThread->getDisasm(num)) {
      SolutionAnim->show();
      SolutionAnim->range(0, assmThread->getDisasm(num)->sumlevel());

      SolutionsInfo->show();
      MovesInfo->value(assmThread->getDisasm(num)->sumlevel());

      disassemble = new DisasmToMoves(assmThread->getDisasm(num), shifting, piece);
      disassemble->setStep(SolutionAnim->value());
    } else {
      SolutionAnim->range(0, 0);
      SolutionAnim->hide();
      MovesInfo->value(0);
    }

    SolutionEmpty = false;

  } else {

    View3D->setVoxelSpace(0, 0);
    SolutionEmpty = true;

    SolutionSel->hide();
    SolutionAnim->hide();
    SolutionsInfo->hide();
    MovesInfo->hide();
  }
}

void UserInterface::update(void) {

  if (assmThread) {

    SolvingProgress->value(100*assmThread->getFinished());

    if (assmThread->number() > 0) {
      SolutionSel->show();
      SolutionSel->range(0, assmThread->number()-1);
    } else {
      SolutionSel->hide();
      SolutionSel->range(0, 0);
    }

    SolutionsInfo->value(assmThread->number());
    OutputSolutions->value(assmThread->number());
    OutputIterations->value(assmThread->getIterations());

    if (SolutionEmpty && (assmThread->number() > 0))
      activateSolution(0);

    switch(assmThread->currentAction()) {
    case assemblerThread::ACT_REDUCE:
    case assemblerThread::ACT_ASSEMBLING:
    case assemblerThread::ACT_DISASSEMBLING:
      BtnStart->deactivate();
      BtnCont->deactivate();
      BtnStop->activate();
      break;

    case assemblerThread::ACT_PREPARATION:
    case assemblerThread::ACT_PAUSING:
    case assemblerThread::ACT_FINISHED:
      BtnStart->activate();
      BtnCont->activate();
      BtnStop->deactivate();
      break;
    }

    switch(assmThread->currentAction()) {
    case assemblerThread::ACT_PREPARATION:
      OutputActivity->value("prep.");
      break;
    case assemblerThread::ACT_REDUCE:
      OutputActivity->value("red.");
      break;
    case assemblerThread::ACT_ASSEMBLING:
      OutputActivity->value("assm.");
      break;
    case assemblerThread::ACT_DISASSEMBLING:
      OutputActivity->value("disassm.");
      break;
    case assemblerThread::ACT_PAUSING:
      OutputActivity->value("pause");
      break;
    case assemblerThread::ACT_FINISHED:
      OutputActivity->value("finished");
      break;
    }

    if (assmThread)
      OutputAssemblies->value(assmThread->getAssemblies());

  } else {

    if (SolvingProgress->value() != 0)
      SolvingProgress->value(0);

    BtnStart->activate();
    BtnCont->deactivate();
    BtnStop->deactivate();

    OutputActivity->value("nothing");
  }
}

UserInterface::UserInterface() {
  ui = this;

  assmThread = 0;
  fname = 0;
  disassemble = 0;

  puzzle = new puzzle_c();
  changed = false;


  for (int i = 0; i < 100; i++)
    shifting[i] = 0;

  for (int j = 0; j < 33; j++)
    visibility[j] = 0;

  Fl_Double_Window* w;
  {
    Fl_Double_Window* o = mainWindow = new Fl_Double_Window(540, 483);
    w = o;
    o->user_data((void*)(this));
    {
      Fl_Group* o = new Fl_Group(0, 25, 540, 455);
      {
        Fl_Tile* o = new Fl_Tile(0, 25, 540, 455);
        {
          Fl_Group* o = new Fl_Group(0, 30, 180, 450);
          o->box(FL_FLAT_BOX);
          {
            Fl_Tabs* o = TaskSelectionTab = new Fl_Tabs(0, 30, 180, 450);
            o->callback(cb_TaskSelectionTab_stub);
            {
              Fl_Group* o = TabPieces = new Fl_Group(0, 50, 180, 430, "Pieces");
              {
                Fl_Tile* o = new Fl_Tile(5, 55, 170, 420);
                {
                  Fl_Group* o = MinSizeSelector = new Fl_Group(5, 140, 170, 165);
                  o->end();
                }
                {
                  Fl_Group* o = new Fl_Group(5, 55, 170, 85);

                  new FlatButton(5, 55, 45, 20, "New", "Add another piece", cb_NewPiece_stub);
                  new FlatButton(55, 55, 65, 20, "Delete", "Delete selected piece", cb_Delete_stub);
                  new FlatButton(125, 55, 50, 20, "Copy", "Copy selected piece", cb_Copy_stub);

                  PcSel2 = new SelectorGroup(5, 80, 170, 60);
                  PcSel2->callback(cb_PcSel_stub, 0);
                  PcSel2->end();
                  Fl_Group::current()->resizable(PcSel2);

                  o->end();
                }
                {
                  Fl_Group* o = new Fl_Group(5, 140, 170, 335);
                  o->box(FL_FLAT_BOX);

                  pieceTools = new ToolTab(5, 145, 170, 135, 0);
                  pieceTools->callback(cb_TransformPiece_stub);
                  pieceTools->end();

                  pieceEdit = new VoxelEditGroup(5, 285, 170, 190);
                  pieceEdit->callback(cb_pieceEdit_stub);
                  pieceEdit->end();

                  Fl_Group::current()->resizable(pieceEdit);

                  o->end();
                  Fl_Group::current()->resizable(o);
                }
                o->resizable(MinSizeSelector);
                o->end();
              }
              o->end();
              Fl_Group::current()->resizable(o);
            }
            {
              Fl_Group* o = TabResult = new Fl_Group(0, 50, 180, 430, "Result");
              o->tooltip("Edit result shape");
              o->hide();

              resultEdit = new VoxelEditGroup(5, 200, 170, 275);
              resultEdit->callback(cb_pieceEdit_stub);
              resultEdit->end();

              resultTools = new ToolTab(5, 55, 170, 140, 1);
              resultTools->callback(cb_TransformResult_stub);
              resultTools->end();

              o->end();
            }
            {
              Fl_Group* o = TabSolve = new Fl_Group(0, 50, 180, 430, "Solve");
              o->tooltip("Solve puzzle");
              o->hide();
              {
                Fl_Tabs* o = new Fl_Tabs(0, 55, 180, 425);
                {
                  Fl_Group* o = new Fl_Group(0, 75, 180, 405, "Solving");
                  {
                    Fl_Check_Button* o = SolveDisasm = new Fl_Check_Button(5, 90, 170, 25, "Solve Disassembly");
                    o->tooltip("Do also try to disassemble the assembled puzzles. Only puzzles that can be disassembled will be added to solutions.");
                  }
                  {
                    BtnStart = new FlatButton(5, 155, 45, 25, "Start", "Start new solving process, removing old result.", cb_BtnStart_stub);
                    BtnCont = new FlatButton(55, 155, 70, 25, "Continue", "Continue started process.", cb_BtnCont_stub);
                    BtnStop = new FlatButton(130, 155, 45, 25, "Stop", "Stop a currently running solution process.", cb_BtnStop_stub);
                  }
                  {
                    Fl_Check_Button* o = new Fl_Check_Button(5, 110, 170, 25, "Just Count");
                    o->tooltip("Don\'t save the solutions, just count the number of them.");
                  }
                  {
                    Fl_Progress* o = SolvingProgress = new Fl_Progress(5, 215, 170, 15, "Progress");
                    o->tooltip("Percentage of solution space searched.");
                    o->box(FL_ENGRAVED_BOX);
                    o->selection_color((Fl_Color)4);
                    o->align(FL_ALIGN_TOP_LEFT);
                  }
                  {
                    Fl_Value_Output* o = OutputAssemblies = new Fl_Value_Output(90, 265, 85, 15, "Assemblies:");
                    o->box(FL_FLAT_BOX);
                  }
                  {
                    Fl_Value_Output* o = OutputSolutions = new Fl_Value_Output(90, 280, 85, 15, "Solutions:");
                    o->box(FL_FLAT_BOX);
                  }
                  {
                    Fl_Output* o = OutputActivity = new Fl_Output(90, 240, 85, 20, "Activity:");
                    o->box(FL_FLAT_BOX);
                    o->color(FL_BACKGROUND_COLOR);
                  }
                  {
                    Fl_Output* o = new Fl_Output(90, 325, 85, 15, "Time used:");
                    o->box(FL_NO_BOX);
                  }
                  {
                    Fl_Output* o = new Fl_Output(90, 340, 85, 15, "Time left:");
                    o->box(FL_NO_BOX);
                  }
                  {
                    Fl_Box* o = new Fl_Box(10, 345, 165, 125);
                    Fl_Group::current()->resizable(o);
                  }
                  {
                    Fl_Check_Button* o = ReducePositions = new Fl_Check_Button(5, 130, 170, 25, "Reduce");
                    o->tooltip("This might reduce the number of possible placements for pieces and so improveassembly speed. But it might also be a waste of time. It\'s useful for complex puzzles.");
                  }
                  {
                    Fl_Value_Output* o = OutputIterations = new Fl_Value_Output(90, 295, 85, 15, "Iterations:");
                    o->box(FL_FLAT_BOX);
                  }
                  o->end();
                  Fl_Group::current()->resizable(o);
                }
                {
                  Fl_Group* o = new Fl_Group(0, 80, 180, 400, "Solution");
                  o->hide();
                  {
                    Fl_Value_Slider* o = SolutionSel = new Fl_Value_Slider(5, 100, 170, 20, "Solution");
                    o->tooltip("Select one Solution.");
                    o->type(1);
                    o->step(1);
                    o->callback(cb_SolutionSel_stub);
                    o->align(FL_ALIGN_TOP_LEFT);
                  }
                  {
                    Fl_Value_Slider* o = SolutionAnim = new Fl_Value_Slider(5, 140, 170, 20, "Move");
                    o->tooltip("Animate the disassembly.");
                    o->type(1);
                    o->callback(cb_SolutionAnim_stub);
                    o->align(FL_ALIGN_TOP_LEFT);
                  }
                  {
                    PieceVisibility* o = PcVis = new PieceVisibility(5, 165, 170, 310, "PieceVisibilitySelector");
                    o->tooltip("Change appearance of the pieces between normal, grid and invisible.");
                    o->box(FL_NO_BOX);
                    o->callback(cb_PcVis_stub);
                    Fl_Group::current()->resizable(o);
                  }
                  {
                    Fl_Value_Output* o = SolutionsInfo = new Fl_Value_Output(80, 85, 95, 15);
                    o->tooltip("Number of solutions");
                    o->box(FL_FLAT_BOX);
                  }
                  {
                    Fl_Value_Output* o = MovesInfo = new Fl_Value_Output(80, 125, 95, 15);
                    o->tooltip("Steps for complete disassembly");
                    o->box(FL_FLAT_BOX);
                  }
                  o->end();
                }
                o->end();
                Fl_Group::current()->resizable(o);
              }
              o->end();
            }
            o->end();
            Fl_Group::current()->resizable(o);
          }
          o->end();
        }

        View3D = new View3dGroup(180, 25, 360, 455);
        View3D->end();
        Fl_Group::current()->resizable(View3D);

        {
          Fl_Group* o = MinSizeTools = new Fl_Group(180, 25, 190, 455);
          o->end();
        }
        o->resizable(MinSizeTools);
        o->end();
        Fl_Group::current()->resizable(o);
      }
      o->end();
      Fl_Group::current()->resizable(o);
    }
    {
      Fl_Menu_Bar* o = MainMenu = new Fl_Menu_Bar(0, 0, 540, 25);
      o->menu(menu_MainMenu);
    }
    o->end();
  }

  mainWindow->label("BurrTools - unknown");
  activatePiece(0);
}

