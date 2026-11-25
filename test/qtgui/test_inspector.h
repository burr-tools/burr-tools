/* The inspector's test class (test_inspector.cpp), run by test_qtgui.cpp's
 * main(). In a header so moc sees it.
 */
#ifndef BTTEST_TEST_INSPECTOR_H
#define BTTEST_TEST_INSPECTOR_H

#include <QObject>

/* InspectorController (C03 Fit & scale, C04 Transform, C05 Repair & surface) */
class TestInspector : public QObject {
  Q_OBJECT
private slots:
  void theSectionsRememberWhetherTheyAreOpen();
  void eachVoxelTypeHasItsButtons();
  void anOperationIsOneUndoStep();
  void aNudgeOutOfTheGridIsBlockedWithItsReason();
  void fitAndScaleFollowApplyToAll();
  void theStatesFollowTheShape();
  void repairSaysWhatItChanged();
  void aTransformButtonPreviewsItsResult();
};

#endif
