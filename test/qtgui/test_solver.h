/* The Solver workspace's test class (test_solver.cpp), run by test_qtgui.cpp's
 * main(). In a header so moc sees it.
 */
#ifndef BTTEST_TEST_SOLVER_H
#define BTTEST_TEST_SOLVER_H

#include <QObject>

/* SolverController, the busy gate and the Solver scene (C19, C20, C21) */
class TestSolver : public QObject {
  Q_OBJECT
private slots:
  void aStoredResultOpensWithItsWayOfChecking();
  void aRunFindsTheSolutionsAndMarksTheFileUnsaved();
  void theDocumentIsBusyWhileASolveRuns();
  void nothingChangesThePuzzleWhileBusy();
  void theListShowsWhatComesApartWhenTheDisassemblyIsChecked();
  void theHeadersSortAndReverse();
  void removingIsNotAnUndoStepAndKeepsTheSelection();
  void theRemovePreviewMarksExactlyItsRows();
  void analysingStoresDisassembliesInTheBackground();
  void aSolveAddsRowsInPlaceAndSparesAHiddenList();
  void aLargeFileShowsAtOnceAndItsSolutionsFollow();
  void thePlayerStepsPlaysAndSeeks();
  void theKeyMapMovesThroughSolutionsAndMoves();
  void aChipCyclesSolidWireframeHidden();
  void prepareThenStepShowsTheAssemblersState();
  void theMovementsDrawerClosesWithItsDisassembly();
  void optionsArePerProblemAndCountTheirChanges();
  void aSolutionExportsEachPlacedShapeOnce();
  void theSolversErrorsAreSaidInItsWords();
  void aGridWithoutADisassemblerRefusesTheCheck();
  void aProblemWithoutAResultDoesNotRun();
  void everySearchOptionCountsAsAChange();
  void theRestOfTheKeyMap();
  void stopPausesAndContinueFinishes();
  void escPausesOnlyWhilePlaying();
  void theEmptyListSaysWhy();
  void anEditForgetsTheAssemblersState();
  void optionsFollowTheirProblemAndAnUndoForgetsThem();
  void scrubbingSelectsAtOnceAndDrawsOnceAFrame();
  void theExplorerClosesWhenTheSelectionCannotShowIt();
  void quittingStopsTheSolverFirst();
  void anUndoThatCostsSolutionsAsksOnceASession();
};

#endif
