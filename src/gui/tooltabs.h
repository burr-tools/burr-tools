/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
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
#ifndef __TOOL_TABS__
#define __TOOL_TABS__

#include "Images.h"
#include "Layouter.h"

class ChangeSize;
class puzzle_c;
class guiGridType_c;
class voxel_c;

/* Filled in by applyTask() so previewTransform() can describe what the task
 * geometrically does, without previewTransform() needing to know the per-tab
 * task-id -> operation mapping itself. */
struct TaskPreviewInfo {
  int transformIdx = -1;       // voxel_c::transform() index, if this task calls transform()
  bool hasTranslate = false;   // true if this task is a literal cardinal/diagonal nudge
  int dx = 0, dy = 0, dz = 0;  // nudge delta in grid units, valid only when hasTranslate
};

// the class that contains the tool tab
class ToolTab : public LFl_Tabs {

public:

  ToolTab(int x, int y, int w, int h) : LFl_Tabs(x, y, w, h) {}

  virtual void setVoxelSpace(puzzle_c * puz, unsigned int sh) = 0;
  bool operationToAll(void) { return toAll->value() != 0; }
  void previewTransform(long task, bool on);

protected:

  LFl_Check_Button * toAll = nullptr;
  puzzle_c * puzzle = nullptr;
  unsigned int shape = 0;

  /* Applies the given task to space, and if info is not null, describes what the
   * task did geometrically (see TaskPreviewInfo); used by previewTransform() to
   * draw a rotation/mirror/nudge hint. */
  virtual void applyTask(voxel_c * space, long task, TaskPreviewInfo * info = nullptr) = 0;
};

// the class that contains the tool tab
class ToolTab_0 : public ToolTab {

  ChangeSize * changeSize = nullptr;
  pixmapList_c pm;

public:

  ToolTab_0(int x, int y, int w, int h);

  void setVoxelSpace(puzzle_c * puz, unsigned int sh);

  void cb_size(void);
  void cb_transform(long task);
  void applyTask(voxel_c * space, long task, TaskPreviewInfo * info = nullptr);
};

// the class that contains the tool tab
class ToolTab_1 : public ToolTab {

  ChangeSize * changeSize = nullptr;
  pixmapList_c pm;

public:

  ToolTab_1(int x, int y, int w, int h);

  void setVoxelSpace(puzzle_c * puz, unsigned int sh);

  void cb_size(void);
  void cb_transform(long task);
  void applyTask(voxel_c * space, long task, TaskPreviewInfo * info = nullptr);
};

// the class that contains the tool tab
class ToolTab_2 : public ToolTab {

  ChangeSize * changeSize = nullptr;
  pixmapList_c pm;

public:

  ToolTab_2(int x, int y, int w, int h);

  void setVoxelSpace(puzzle_c * puz, unsigned int sh);

  void cb_size(void);
  void cb_transform(long task);
  void applyTask(voxel_c * space, long task, TaskPreviewInfo * info = nullptr);
};


class ToolTab_3 : public ToolTab {

  ChangeSize * changeSize = nullptr;
  pixmapList_c pm;

public:

  ToolTab_3(int x, int y, int w, int h);

  void setVoxelSpace(puzzle_c * puz, unsigned int sh);

  void cb_size(void);
  void cb_transform(long task);
  void applyTask(voxel_c * space, long task, TaskPreviewInfo * info = nullptr);
};

class ToolTab_4 : public ToolTab {

  ChangeSize * changeSize = nullptr;
  pixmapList_c pm;

public:

  ToolTab_4(int x, int y, int w, int h);

  void setVoxelSpace(puzzle_c * puz, unsigned int sh);

  void cb_size(void);
  void cb_transform(long task);
  void applyTask(voxel_c * space, long task, TaskPreviewInfo * info = nullptr);
};

/* kind/axis/angleDeg describe, in the shape's own local coordinate frame, what
 * kind of change the preview represents, so the 3D view can draw a matching hint:
 *   0 = none        (no hint drawn)
 *   1 = rotation     - axis/angleDeg valid, draws a sweeping arc + arrowhead
 *   2 = mirror       - axis is the mirror plane's normal, draws a double-headed arrow
 *   3 = translation  - axis is the (non-unit) move direction, draws a single arrow
 */
typedef void (*TransformPreviewCb)(void * user, voxel_c * preview, unsigned int shapeNum,
                                    int kind, float axisX, float axisY, float axisZ, float angleDeg);

class ToolTabContainer : public layouter_c {

  ToolTab * tt = nullptr;
  TransformPreviewCb previewHandler = nullptr;
  void * previewUser = nullptr;
  unsigned int delayedClearShape = 0;
  static void previewClearTimeout(void * v);

  public:

  ToolTabContainer(int x, int y, int w, int h, const guiGridType_c * ggt);
  ~ToolTabContainer(void);

  void setVoxelSpace(puzzle_c * puz, unsigned int sh) { if (tt) tt->setVoxelSpace(puz, sh); }
  bool operationToAll(void) { if (tt) return tt->operationToAll(); else return false; }
  void setPreviewHandler(TransformPreviewCb cb, void * user) {
    previewHandler = cb;
    previewUser = user;
  }
  void emitPreview(voxel_c * preview, unsigned int shapeNum, int kind = 0,
                    float axisX = 0, float axisY = 0, float axisZ = 1, float angleDeg = 0);

  void newGridType(const guiGridType_c * ggt);
};

#endif
