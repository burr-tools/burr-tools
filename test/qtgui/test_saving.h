#ifndef TEST_SAVING_H
#define TEST_SAVING_H

#include <QObject>

/* What the Entities workspace edits survives a save: each case edits
 * through the App's controllers (the 2D grid, the 3D view, the inspector,
 * the shapes list, the colours), saves, opens the file in a fresh App and
 * compares the whole puzzle with what was on screen before the save. */
class TestSaving : public QObject {
  Q_OBJECT

private slots:
  void edits2dInEveryPlaneAreSaved_data();
  void edits2dInEveryPlaneAreSaved();
  void edits3dAreSaved_data();
  void edits3dAreSaved();
  void transformsGridSizeAndScaleAreSaved_data();
  void transformsGridSizeAndScaleAreSaved();
  void eachShapeKeepsItsOwnEdits();
  void problemsFollowTheirShapesThroughASave();
  void undoneEditsAreNotSavedAndASaveIsClean();
};

#endif
