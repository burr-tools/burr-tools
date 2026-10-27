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
};

#endif
