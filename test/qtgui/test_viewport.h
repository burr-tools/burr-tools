/* The 3D-view test classes (test_viewport.cpp), run by test_qtgui.cpp's
 * main(). In a header so moc sees them.
 */
#ifndef BTTEST_TEST_VIEWPORT_H
#define BTTEST_TEST_VIEWPORT_H

#include <QObject>

class TestViewportController : public QObject {
  Q_OBJECT
private slots:
  void framesFollowTheDisplayOptions();
  void slabAndDimOnlyWithTheEditorVisible();
  void displayOptionsPersist();
  void onlyEntitiesShowsTheShape();
  void aClickWithinTheSlopDoesNotNavigate();
  void dragsOrbitAndPanByMode();
  void theWheelZoomsAndHonoursReverseScroll();
  void theCubeDrivesTheCamera();
  void layerStepsClampAndFollowThePlane();
  void theVoxelStyleSettingRebuildsTheMesh();
};

class TestViewCubePaint : public QObject {
  Q_OBJECT
private slots:
  void drawsTheCubeAndHighlightsTheHoveredRegion();
  void showsTheArrowsOnlyFaceAligned();
  void theItemHasRoomForTheAxisLabelsAndHitsThroughIt();
};

class TestRender : public QObject {
  Q_OBJECT
private slots:
  void rendersTheShapeOnTheCanvas();
  void lightingChangesTheShading();
  void darkThemeUsesTheDarkCanvas();
  void theSlabTintsItsLayer();
  void everyGridTypeRenders();
  void orthographicDiffersFromPerspective();
  void theStlPreviewDrawsTheMeshAndItsInsides();
  void theFlatStyleOutlinesEveryVoxelFace();
  void translucencyBlendsInLinearLight();
  void innerVariableVoxelsShowThroughTheOuterOnes();
  void theTargetsOutgrowTheViewInSteps();
  void theTargetsMemoryStaysInBudget();
  void framesDoNotAllocatePerTriangle();
};

/* The real 3D view item (VoxelViewport, a QQuickRhiItem) drawn by Qt Quick
 * through a QQuickRenderControl into the offscreen QRhi. */
class TestViewportItem : public QObject {
  Q_OBJECT
private slots:
  void redrawsAtTheFinalSizeAfterTheCardGrows();
  void drawsWithTheBestMultisamplingTheDeviceHas();
};

/* The pipeline caches of the main window and the offscreen renderer. */
class TestPipelineCache : public QObject {
  Q_OBJECT
private slots:
  void theFilesLiveBesideTheSettings();
  void aWindowLoadsOnlyACacheThatExists();
  void theOffscreenRendererKeepsItsCache();
  void theWindowWritesItsCacheAndTheNextStartLoadsIt();
};

#endif
