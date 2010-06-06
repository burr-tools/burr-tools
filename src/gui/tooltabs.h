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

// the class that contains the tool tab
class ToolTab : public LFl_Tabs {

public:

  ToolTab(int x, int y, int w, int h) : LFl_Tabs(x, y, w, h) {}

  virtual void setVoxelSpace(puzzle_c * puz, unsigned int sh) = 0;
  bool operationToAll(void) { return toAll->value() != 0; }

protected:

  LFl_Check_Button * toAll;
};

// the class that contains the tool tab
class ToolTab_0 : public ToolTab {

  ChangeSize * changeSize;
  puzzle_c * puzzle;
  unsigned int shape;
  pixmapList_c pm;

public:

  ToolTab_0(int x, int y, int w, int h);

  void setVoxelSpace(puzzle_c * puz, unsigned int sh);

  void cb_size(void);
  void cb_transform(long task);
};

// the class that contains the tool tab
class ToolTab_1 : public ToolTab {

  ChangeSize * changeSize;
  puzzle_c * puzzle;
  unsigned int shape;
  pixmapList_c pm;

public:

  ToolTab_1(int x, int y, int w, int h);

  void setVoxelSpace(puzzle_c * puz, unsigned int sh);

  void cb_size(void);
  void cb_transform(long task);
};

// the class that contains the tool tab
class ToolTab_2 : public ToolTab {

  ChangeSize * changeSize;
  puzzle_c * puzzle;
  unsigned int shape;
  pixmapList_c pm;

public:

  ToolTab_2(int x, int y, int w, int h);

  void setVoxelSpace(puzzle_c * puz, unsigned int sh);

  void cb_size(void);
  void cb_transform(long task);
};


class ToolTab_3 : public ToolTab {

  ChangeSize * changeSize;
  puzzle_c * puzzle;
  unsigned int shape;
  pixmapList_c pm;

public:

  ToolTab_3(int x, int y, int w, int h);

  void setVoxelSpace(puzzle_c * puz, unsigned int sh);

  void cb_size(void);
  void cb_transform(long task);
};

class ToolTab_4 : public ToolTab {

  ChangeSize * changeSize;
  puzzle_c * puzzle;
  unsigned int shape;
  pixmapList_c pm;

public:

  ToolTab_4(int x, int y, int w, int h);

  void setVoxelSpace(puzzle_c * puz, unsigned int sh);

  void cb_size(void);
  void cb_transform(long task);
};

class ToolTabContainer : public layouter_c {

  ToolTab * tt;

  public:

  ToolTabContainer(int x, int y, int w, int h, const guiGridType_c * ggt);

  void setVoxelSpace(puzzle_c * puz, unsigned int sh) { if (tt) tt->setVoxelSpace(puz, sh); }
  bool operationToAll(void) { if (tt) return tt->operationToAll(); else return false; }

  void newGridType(const guiGridType_c * ggt);
};

#endif
