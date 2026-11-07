/* The Puzzle workspace's test class (test_puzzle.cpp), run by test_qtgui.cpp's
 * main(). In a header so moc sees it.
 */
#ifndef BTTEST_TEST_PUZZLE_H
#define BTTEST_TEST_PUZZLE_H

#include <QObject>

/* PuzzleController and the Puzzle scene (C16, C17, C18) */
class TestPuzzle : public QObject {
  Q_OBJECT
private slots:
  void aFileWithoutProblemsSelectsNone();
  void theListsFollowTheProblem();
  void problemsAreAddedCopiedAndDeletedAsUndoSteps();
  void aMoveKeepsTheSelectionOnItsProblem();
  void theLibraryReadsTheSelectedProblem();
  void theResultIsNoPiece();
  void countsStepAndLeaveAtZero();
  void aRangeStaysOnShowWhileItsNumbersMeet();
  void theSummaryAndStatusSayTheTotals();
  void aPieceCanBeInSeveralGroups();
  void theColourMatrixHasNoDefault();
  void theRailsListComeFromTheSectionTable();
  void aChangeThatCostsSolutionsWarnsOnceASession();
  void cancellingTheWarningChangesNothing();
  void renamingAndReorderingNeverWarn();
  void deletingAProblemWithSolutionsWarns();
  void everyVoxelEditRemovesTheSolutionsOfItsProblems();
  void inspectorGridAndClickEditsRemoveSolutions_data();
  void inspectorGridAndClickEditsRemoveSolutions();
  void aShapeNoSolvedProblemUsesNeverWarns();
  void undoKeepsTheSolutionsOfOtherProblems();
  void theHoleLimitShowsAndClears();
  void listsOnlyFollowWhileTheWorkspaceShows();
  void theSceneHasAnObjectPerCopy();
  void aClickSelectsAPieceNotTheResult();
  void framesStayPutWhileTheObjectsTurn();
  void panAndZoomMoveTheWholeLayout();
  void theSceneDrawsEachObjectInItsFrame();
  void theCameraGoesBackToTheShapeInEntities();
  void piecesAndProblemsKeysWorkOnlyInPuzzle();
  void aChangeThatChangesNothingDoesNotWarn();
  void aCancelledRangeLeavesTheSwitchOff();
  void undoAndRedoMoveTheSelection();
  void undoEndsAHeldRange();
  void theSelectionFollowsItsShape();
  void undoOfAShapeDeleteBringsTheSelectionBack();
  void aHeldStepperIsOneUndoStep();
  void aTypedNumberDoesNotLowerTheOtherEndForGood();
  void anEditWaitingForAnAnswerGoesWithTheDocument();
  void twoEditsOfOneGestureShareTheWarning();
  void aVectorExportFromPuzzleSpreadsTheObjects();
  void theTopCountStops();
  void anOddValueChangesNothing();
  void shapeColoursDefaultToMutedAndSwitchBack();
  void everyViewFollowsTheShapeColours();
  void theThemePicksItsMutedSet();
};

#endif
