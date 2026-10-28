/* The voxel types' test class (test_voxeltypes.cpp), run by test_qtgui.cpp's
 * main(). In a header so moc sees it.
 */
#ifndef BTTEST_TEST_VOXELTYPES_H
#define BTTEST_TEST_VOXELTYPES_H

#include <QObject>

/* Editing every voxel type end to end (C14 with C06, C08): the 2D grid in
 * each plane the type has, by pointer and by keyboard, and clicks in the
 * 3D view -- the slab, a voxel's face, Erase -- each one undo step. The
 * rules underneath are uicore's, tested per type there; these drive them
 * through the controllers and the grid item, as the window does. */
class TestVoxelTypes : public QObject {
  Q_OBJECT
private slots:
  void paintsEveryPlaneByPointerAndKeys_data();
  void paintsEveryPlaneByPointerAndKeys();
  void theHoveredCellIsOutlined_data();
  void theHoveredCellIsOutlined();
  void editsIn3dOnTheSlabAndOnAFace_data();
  void editsIn3dOnTheSlabAndOnAFace();
};

#endif
