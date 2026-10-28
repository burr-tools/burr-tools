/* The voxel editor's test classes (test_editor.cpp), run by test_qtgui.cpp's
 * main(). In a header so moc sees them.
 */
#ifndef BTTEST_TEST_EDITOR_H
#define BTTEST_TEST_EDITOR_H

#include <QObject>

/* EditorController (C07, C08, C02), the edit cursors and the 2D grid item */
class TestEditor : public QObject {
  Q_OBJECT
private slots:
  void aStrokeIsOneUndoStep();
  void anUndoDuringADragEndsIt();
  void theRightButtonErasesWhateverTheToolAndModifiers();
  void theQuickToolIsCapturedAtThePress();
  void theToolInEffectFollowsTheModifiersHeldOverTheGrid();
  void mirrorAndSpanExpandEveryPlacement();
  void theHoverPreviewIsWhatAClickWouldDo();
  void paintUsesTheDrawingColourAndLeavesEmptyCells();
  void theCursorReadoutNamesThePlanesAxes();
  void gridSizeStepsAndIsBlockedBeforeCuttingVoxels();
  void applyToAllResizesEveryShape();
  void onlyBricksHaveTheXzAndYzPlanes();
  void layerDotsFollowTheVoxels();
  void editsThatChangeNoGeometryKeepTheMesh();
  void theCursorsAreComposedPerToolColourAndTheme();
  void theGridHitsItsCellsAndPaintsTheirVoxels();
  void theGridPaintsAStrokeWithoutGaps();
  void a32By32GridRedrawsInTime();
  void theViewIsReadyInTimeAfterAStrokeSegment();
  void theGridByKeyboard();
};

/* Editing in the 3D view (C06, P5): Edit mode and the quick tools, the
 * target a click picks (a face, the active layer's slab, the origin), the
 * ghost and the status line while hovering, and the focused view's keys. */
class TestEdit3d : public QObject {
  Q_OBJECT
private slots:
  void aClickEditsWithAQuickToolOrInEditMode();
  void quickClicksAreAStepEach();
  void dragsAndOtherButtonsNeverEdit();
  void theSlabIsA2dGridInTheView();
  void anEmptyShapeWithoutTheSlabStartsAtTheOrigin();
  void aFaceWithNothingBeyondIsOutlinedNotEdited();
  void hoveringShowsTheGhostAndNamesTheTarget();
  void theGhostTakesEachToolsLook();
  void theFocusedViewsKeysTurnPanAndZoom();
  void singleKeyShortcutsCanBeTurnedOff();
};

#endif
