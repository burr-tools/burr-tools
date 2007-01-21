/* Burr Solver
 * Copyright (C) 2003-2006  Andreas Röver
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

#include "tooltabs.h"

#include "../lib/puzzle.h"
#include "../lib/voxel.h"
#include "WindowWidgets.h"
#include "guigridtype.h"


#define LABEL_FONT_SIZE 12

void ToolTab_0::setVoxelSpace(puzzle_c * puz, unsigned int sh) {
  puzzle = puz;
  shape = sh;

  bt_assert(!puzzle || (puzzle->getGridType()->getType() == gridType_c::GT_BRICKS));

  if (puzzle && shape < puzzle->shapeNumber())
    changeSize->setXYZ(puzzle->getShape(shape)->getX(),
        puzzle->getShape(shape)->getY(),
        puzzle->getShape(shape)->getZ());
  else
    changeSize->setXYZ(0, 0, 0);
}

static void cb_ToolTab0Size_stub(Fl_Widget* o, long v) { ((ToolTab_0*)(o->parent()->parent()->parent()))->cb_size(); }
static void cb_ToolTab0Transform_stub(Fl_Widget* o, long v) { ((ToolTab_0*)(o->parent()))->cb_transform(v); }
static void cb_ToolTab0Transform2_stub(Fl_Widget* o, long v) { ((ToolTab_0*)(o->parent()->parent()->parent()))->cb_transform(v); }

ToolTab_0::ToolTab_0(int x, int y, int w, int h) : ToolTab(x, y, w, h) {

  {
    layouter_c * o = new layouter_c(0, 1, 1, 1);
    o->label("Size");
    o->pitch(5);

    layouter_c *o2 = new layouter_c(0, 0, 1, 1);

    changeSize = new ChangeSize(0, 1, 1, 1);
    changeSize->callback(cb_ToolTab0Size_stub);

    toAll = new LFl_Check_Button("Apply to All Shapes (Bricks)", 0, 0, 1, 1);
    toAll->tooltip(" If this is active, all operations (including transformations and constrains are done to all shapes ");
    toAll->clear_visible_focus();
    toAll->stretchHCenter();

    o2->end();
    o2->weight(1, 0);

    (new LFl_Box(1, 0, 1, 1))->setMinimumSize(5, 0);

    o2 = new layouter_c(2, 0, 1, 1);

    (new LFl_Box(0, 0, 1, 1))->weight(1, 1);
    (new LFl_Box(0, 8, 1, 1))->weight(1, 1);

    (new LFl_Box("Grid", 0, 1, 1, 1))->labelsize(LABEL_FONT_SIZE);

    new LFlatButton_c(0, 3, 1, 1, pm.get(Grid_Color_Minimize_xpm), pm.get(Grid_Disabled_Minimize_xpm),
        " Minimize size of grid ", cb_ToolTab0Transform2_stub, 15);
    new LFlatButton_c(0, 5, 1, 1, pm.get(Grid_Color_Center_xpm), pm.get(Grid_Disabled_Center_xpm),
        " Centre shape inside the grid ", cb_ToolTab0Transform2_stub, 25);
    new LFlatButton_c(0, 7, 1, 1, pm.get(Grid_Color_Origin_xpm), pm.get(Grid_Disabled_Origin_xpm),
        " Move shape to origin of grid ", cb_ToolTab0Transform2_stub, 24);

    (new LFl_Box("Shape", 2, 1, 1, 1))->labelsize(LABEL_FONT_SIZE);

    new LFlatButton_c(2, 3, 1, 1, pm.get(Rescale_Color_X1_xpm), pm.get(Rescale_Disabled_X1_xpm),
        " Try to minimize size of shape ", cb_ToolTab0Transform2_stub, 26);
    new LFlatButton_c(2, 5, 1, 1, pm.get(Rescale_Color_X2_xpm), pm.get(Rescale_Disabled_X2_xpm),
        " Double size of shape ", cb_ToolTab0Transform2_stub, 22);
    new LFlatButton_c(2, 7, 1, 1, pm.get(Rescale_Color_X3_xpm), pm.get(Rescale_Disabled_X3_xpm),
        " Triple size of shape ", cb_ToolTab0Transform2_stub, 23);

//    (new LFl_Box(0, 2, 1, 1))->setMinimumSize(0, 5);
    (new LFl_Box(0, 4, 1, 1))->setMinimumSize(0, 5);
    (new LFl_Box(0, 6, 1, 1))->setMinimumSize(0, 5);

    o2->end();

    o->end();
  }
  {
    Fl_Group* o = new TransformButtons(0, 1, 1, 1);
    o->callback(cb_ToolTab0Transform_stub);
    o->hide();
  }
  {
    Fl_Group* o = new layouter_c(0, 1, 1, 1);
    o->hide();

    o->label("Tools");

    // this 2nd group is not necessary, but it the callback function requires it to find
    // the right parent class
    Fl_Group* o2 = new layouter_c(0, 0, 1, 1);

    (new LFl_Box(0, 0, 1, 1))->weight(1, 1);
    (new LFl_Box(0, 8, 1, 1))->weight(1, 1);
    (new LFl_Box(8, 0, 1, 1))->weight(1, 1);
    (new LFl_Box(8, 8, 1, 1))->weight(1, 1);

    (new LFl_Box("Constrain", 3, 1, 2, 1))->labelsize(LABEL_FONT_SIZE);

    new LFlatButton_c(3, 3, 1, 1, pm.get(InOut_Color_Fixed_In_xpm), pm.get(InOut_Disabled_Fixed_In_xpm),
        " Make inside fixed ", cb_ToolTab0Transform2_stub, 16);
    new LFlatButton_c(3, 5, 1, 1, pm.get(InOut_Color_Variable_In_xpm), pm.get(InOut_Disabled_Variable_In_xpm),
        " Make inside variable ", cb_ToolTab0Transform2_stub, 18);
    new LFlatButton_c(3, 7, 1, 1, pm.get(InOut_Color_RemoveColor_In_xpm), pm.get(InOut_Disabled_RemoveColor_In_xpm),
        " Remove Colours from inside cubes ", cb_ToolTab0Transform2_stub, 20);

    new LFlatButton_c(4, 3, 1, 1, pm.get(InOut_Color_Fixed_Out_xpm), pm.get(InOut_Disabled_Fixed_Out_xpm),
        " Make outside fixed ", cb_ToolTab0Transform2_stub, 17);
    new LFlatButton_c(4, 5, 1, 1, pm.get(InOut_Color_Variable_Out_xpm), pm.get(InOut_Disabled_Variable_Out_xpm),
        " Make outside variable ", cb_ToolTab0Transform2_stub, 19);
    new LFlatButton_c(4, 7, 1, 1, pm.get(InOut_Color_RemoveColor_Out_xpm), pm.get(InOut_Disabled_RemoveColor_Out_xpm),
        " Remove Colours from outside cubes ", cb_ToolTab0Transform2_stub, 21);

    (new LFl_Box(0, 2, 1, 1))->setMinimumSize(0, 5);
    (new LFl_Box(0, 4, 1, 1))->setMinimumSize(0, 5);
    (new LFl_Box(0, 6, 1, 1))->setMinimumSize(0, 5);

    o2->end();

    o->end();
  }

  end();
}

void ToolTab_0::cb_size(void) {
  if (puzzle && shape < puzzle->shapeNumber()) {

    if (toAll->value()) {

      int dx, dy, dz;

      dx = changeSize->getX() - puzzle->getShape(shape)->getX();
      dy = changeSize->getY() - puzzle->getShape(shape)->getY();
      dz = changeSize->getZ() - puzzle->getShape(shape)->getZ();

      if (dx < 0) dx = 0;
      if (dy < 0) dy = 0;
      if (dz < 0) dz = 0;

      for (unsigned int s = 0; s < puzzle->shapeNumber(); s++) {
        int nx = puzzle->getShape(s)->getX()+dx;
        int ny = puzzle->getShape(s)->getY()+dy;
        int nz = puzzle->getShape(s)->getZ()+dz;

        if (nx < 1) nx = 1;
        if (ny < 1) ny = 1;
        if (nz < 1) nz = 1;

        puzzle->getShape(s)->resize(nx, ny, nz, 0);
      }

    }

    // we always do this, is may be that the shape is not changed in the loop above because
    // that loop and only increase the size, so smaller sizes for the selected shape must be done
    // here
    puzzle->getShape(shape)->resize(changeSize->getX(), changeSize->getY(), changeSize->getZ(), 0);

    do_callback();
  }
}

void ToolTab_0::cb_transform(long task) {
  if (puzzle && shape < puzzle->shapeNumber()) {

    int ss, se;

    if (toAll->value() && ((task == 15) || ((task >= 22) && (task <= 26)))) {
      ss = 0;
      se = puzzle->shapeNumber();
    } else {
      ss = shape;
      se = shape+1;
    }

    if (task == 26) {

      unsigned char primes[] = {2, 3, 5, 7, 11, 13, 17, 19, 0};

      // special case for minimisation

      int prime = 0;

      while (primes[prime]) {

        bool canScale = true;

        for (int s = ss; s < se; s++)
          if (!puzzle->getShape(s)->scaleDown(primes[prime], false)) {
            canScale = false;
            break;
          }

        if (canScale) {
          for (int s = ss; s < se; s++)
            puzzle->getShape(s)->scaleDown(primes[prime], true);
        } else
          prime++;
      }

      for (int s = ss; s < se; s++)
        puzzle->getShape(s)->setHotspot(0, 0, 0);

      do_callback(this, user_data());

      return;
    }

    for (int s = ss; s < se; s++) {
      voxel_c * space = puzzle->getShape(s);

      switch(task) {
        case  0: space->translate( 1, 0, 0, 0); break;
        case  1: space->translate(-1, 0, 0, 0); break;
        case  2: space->translate( 0, 1, 0, 0); break;
        case  3: space->translate( 0,-1, 0, 0); break;
        case  4: space->translate( 0, 0, 1, 0); break;
        case  5: space->translate( 0, 0,-1, 0); break;
        case  7: space->rotatex(); space->rotatex(); // fall through
        case  6: space->rotatex(); break;
        case  9: space->rotatey(); space->rotatey(); // fall through
        case  8: space->rotatey(); break;
        case 11: space->rotatez(); space->rotatez(); // fall through
        case 10: space->rotatez(); break;
        case 12: space->mirrorX(); break;
        case 13: space->mirrorY(); break;
        case 14: space->mirrorZ(); break;
        case 15: space->minimizePiece(); break;
        case 16: space->actionOnSpace(voxel_c::ACT_FIXED, true); break;
        case 17: space->actionOnSpace(voxel_c::ACT_FIXED, false); break;
        case 18: space->actionOnSpace(voxel_c::ACT_VARIABLE, true); break;
        case 19: space->actionOnSpace(voxel_c::ACT_VARIABLE, false); break;
        case 20: space->actionOnSpace(voxel_c::ACT_DECOLOR, true); break;
        case 21: space->actionOnSpace(voxel_c::ACT_DECOLOR, false); break;
        case 22: space->scale(2); break;
        case 23: space->scale(3); break;
        case 24: space->translate(- space->boundX1(), - space->boundY1(), - space->boundZ1(), 0); break;
        case 25:
                 {
		   // if the space is empty, don't do anything
		   if (space->boundX2() < space->boundX1())
		     break;

                   int fx = space->getX() - (space->boundX2()-space->boundX1()+1);
                   int fy = space->getY() - (space->boundY2()-space->boundY1()+1);
                   int fz = space->getZ() - (space->boundZ2()-space->boundZ1()+1);

                   if ((fx & 1) || (fy & 1) || (fz & 1)) {
                     space->resize(space->getX()+(fx&1), space->getY()+(fy&1), space->getZ()+(fz&1), 0);
                     fx += fx&1;
                     fy += fy&1;
                     fz += fz&1;
                   }
                   space->translate(fx/2 - space->boundX1(), fy/2 - space->boundY1(), fz/2 - space->boundZ1(), 0);
                 }
                 break;
      }
      space->setHotspot(0, 0, 0);
    }

    do_callback(this, user_data());
  }
}





void ToolTab_1::setVoxelSpace(puzzle_c * puz, unsigned int sh) {
  puzzle = puz;
  shape = sh;

  bt_assert(!puzzle || (puzzle->getGridType()->getType() == gridType_c::GT_TRIANGULAR_PRISM));

  if (puzzle && shape < puzzle->shapeNumber())
    changeSize->setXYZ(puzzle->getShape(shape)->getX(),
        puzzle->getShape(shape)->getY(),
        puzzle->getShape(shape)->getZ());
  else
    changeSize->setXYZ(0, 0, 0);
}

static void cb_ToolTab1Size_stub(Fl_Widget* o, long v) { ((ToolTab_1*)(o->parent()->parent()->parent()))->cb_size(); }
static void cb_ToolTab1Transform_stub(Fl_Widget* o, long v) { ((ToolTab_1*)(o->parent()))->cb_transform(v); }
static void cb_ToolTab1Transform2_stub(Fl_Widget* o, long v) { ((ToolTab_1*)(o->parent()->parent()->parent()))->cb_transform(v); }

ToolTab_1::ToolTab_1(int x, int y, int w, int h) : ToolTab(x, y, w, h) {

  {
    layouter_c * o = new layouter_c(0, 1, 1, 1);
    o->label("Size");
    o->pitch(5);

    layouter_c *o2 = new layouter_c(0, 0, 1, 1);

    changeSize = new ChangeSize(0, 1, 1, 1);
    changeSize->callback(cb_ToolTab1Size_stub);

    toAll = new LFl_Check_Button("Apply to All Shapes (Triangles)", 0, 0, 1, 1);
    toAll->tooltip(" If this is active, all operations (including transformations and constrains are done to all shapes ");
    toAll->clear_visible_focus();
    toAll->stretchHCenter();

    o2->end();
    o2->weight(1, 0);

    (new LFl_Box(1, 0, 1, 1))->setMinimumSize(5, 0);

    o2 = new layouter_c(2, 0, 1, 1);

    (new LFl_Box(0, 0, 1, 1))->weight(1, 1);
    (new LFl_Box(0, 8, 1, 1))->weight(1, 1);

    (new LFl_Box("Grid", 0, 1, 1, 1))->labelsize(LABEL_FONT_SIZE);

    new LFlatButton_c(0, 3, 1, 1, pm.get(Grid_Color_Minimize_xpm), pm.get(Grid_Disabled_Minimize_xpm),
        " Minimize size of grid ", cb_ToolTab1Transform2_stub, 15);
    new LFlatButton_c(0, 5, 1, 1, pm.get(Grid_Color_Center_xpm), pm.get(Grid_Disabled_Center_xpm),
        " Centre shape inside the grid ", cb_ToolTab1Transform2_stub, 25);
    new LFlatButton_c(0, 7, 1, 1, pm.get(Grid_Color_Origin_xpm), pm.get(Grid_Disabled_Origin_xpm),
        " Move shape to origin of grid ", cb_ToolTab1Transform2_stub, 24);

    (new LFl_Box("Shape", 2, 1, 1, 1))->labelsize(LABEL_FONT_SIZE);

    new LFlatButton_c(2, 3, 1, 1, pm.get(Rescale_Color_X1_xpm), pm.get(Rescale_Disabled_X1_xpm),
        " Try to minimize size of shape ", cb_ToolTab1Transform2_stub, 26);
    new LFlatButton_c(2, 5, 1, 1, pm.get(Rescale_Color_X2_xpm), pm.get(Rescale_Disabled_X2_xpm),
        " Double size of shape ", cb_ToolTab1Transform2_stub, 22);
    new LFlatButton_c(2, 7, 1, 1, pm.get(Rescale_Color_X3_xpm), pm.get(Rescale_Disabled_X3_xpm),
        " Triple size of shape ", cb_ToolTab1Transform2_stub, 23);

//    (new LFl_Box(0, 2, 1, 1))->setMinimumSize(0, 5);
    (new LFl_Box(0, 4, 1, 1))->setMinimumSize(0, 5);
    (new LFl_Box(0, 6, 1, 1))->setMinimumSize(0, 5);

    o2->end();

    o->end();
  }
  {
    Fl_Group* o = new TransformButtons(0, 1, 1, 1);
    o->callback(cb_ToolTab1Transform_stub);
    o->hide();
  }
  {
    Fl_Group* o = new layouter_c(0, 1, 1, 1);
    o->hide();

    o->label("Tools");

    // this 2nd group is not necessary, but it the callback function requires it to find
    // the right parent class
    Fl_Group* o2 = new layouter_c(0, 0, 1, 1);

    (new LFl_Box(0, 0, 1, 1))->weight(1, 1);
    (new LFl_Box(0, 8, 1, 1))->weight(1, 1);
    (new LFl_Box(8, 0, 1, 1))->weight(1, 1);
    (new LFl_Box(8, 8, 1, 1))->weight(1, 1);

    (new LFl_Box("Constrain", 3, 1, 2, 1))->labelsize(LABEL_FONT_SIZE);

    new LFlatButton_c(3, 3, 1, 1, pm.get(InOut_Color_Fixed_In_xpm), pm.get(InOut_Disabled_Fixed_In_xpm),
        " Make inside fixed ", cb_ToolTab1Transform2_stub, 16);
    new LFlatButton_c(3, 5, 1, 1, pm.get(InOut_Color_Variable_In_xpm), pm.get(InOut_Disabled_Variable_In_xpm),
        " Make inside variable ", cb_ToolTab1Transform2_stub, 18);
    new LFlatButton_c(3, 7, 1, 1, pm.get(InOut_Color_RemoveColor_In_xpm), pm.get(InOut_Disabled_RemoveColor_In_xpm),
        " Remove Colours from inside cubes ", cb_ToolTab1Transform2_stub, 20);

    new LFlatButton_c(4, 3, 1, 1, pm.get(InOut_Color_Fixed_Out_xpm), pm.get(InOut_Disabled_Fixed_Out_xpm),
        " Make outside fixed ", cb_ToolTab1Transform2_stub, 17);
    new LFlatButton_c(4, 5, 1, 1, pm.get(InOut_Color_Variable_Out_xpm), pm.get(InOut_Disabled_Variable_Out_xpm),
        " Make outside variable ", cb_ToolTab1Transform2_stub, 19);
    new LFlatButton_c(4, 7, 1, 1, pm.get(InOut_Color_RemoveColor_Out_xpm), pm.get(InOut_Disabled_RemoveColor_Out_xpm),
        " Remove Colours from outside cubes ", cb_ToolTab1Transform2_stub, 21);

    (new LFl_Box(0, 2, 1, 1))->setMinimumSize(0, 5);
    (new LFl_Box(0, 4, 1, 1))->setMinimumSize(0, 5);
    (new LFl_Box(0, 6, 1, 1))->setMinimumSize(0, 5);

    o2->end();

    o->end();
  }

  end();
}

void ToolTab_1::cb_size(void) {
  if (puzzle && shape < puzzle->shapeNumber()) {

    if (toAll->value()) {

      int dx, dy, dz;

      dx = changeSize->getX() - puzzle->getShape(shape)->getX();
      dy = changeSize->getY() - puzzle->getShape(shape)->getY();
      dz = changeSize->getZ() - puzzle->getShape(shape)->getZ();

      if (dx < 0) dx = 0;
      if (dy < 0) dy = 0;
      if (dz < 0) dz = 0;

      for (unsigned int s = 0; s < puzzle->shapeNumber(); s++) {
        int nx = puzzle->getShape(s)->getX()+dx;
        int ny = puzzle->getShape(s)->getY()+dy;
        int nz = puzzle->getShape(s)->getZ()+dz;

        if (nx < 1) nx = 1;
        if (ny < 1) ny = 1;
        if (nz < 1) nz = 1;

        puzzle->getShape(s)->resize(nx, ny, nz, 0);
      }

    }

    // we always do this, is may be that the shape is not changed in the loop above because
    // that loop and only increase the size, so smaller sizes for the selected shape must be done
    // here
    puzzle->getShape(shape)->resize(changeSize->getX(), changeSize->getY(), changeSize->getZ(), 0);

    do_callback();
  }
}

void ToolTab_1::cb_transform(long task) {
  if (puzzle && shape < puzzle->shapeNumber()) {

    int ss, se;

    if (toAll->value() && ((task == 15) || ((task >= 22) && (task <= 26)))) {
      ss = 0;
      se = puzzle->shapeNumber();
    } else {
      ss = shape;
      se = shape+1;
    }

    if (task == 26) {

      unsigned char primes[] = {2, 3, 5, 7, 11, 13, 17, 19, 0};

      // special case for minimisation

      int prime = 0;

      while (primes[prime]) {

        bool canScale = true;

        for (int s = ss; s < se; s++)
          if (!puzzle->getShape(s)->scaleDown(primes[prime], false)) {
            canScale = false;
            break;
          }

        if (canScale) {
          for (int s = ss; s < se; s++)
            puzzle->getShape(s)->scaleDown(primes[prime], true);
        } else
          prime++;
      }

      for (int s = ss; s < se; s++)
        puzzle->getShape(s)->setHotspot(0, 0, 0);

      do_callback(this, user_data());

      return;
    }

    for (int s = ss; s < se; s++) {
      voxel_c * space = puzzle->getShape(s);

      switch(task) {
        case  0: space->translate( 1, 0, 0, 0); break;
        case  1: space->translate(-1, 0, 0, 0); break;
        case  2: space->translate( 0, 1, 0, 0); break;
        case  3: space->translate( 0,-1, 0, 0); break;
        case  4: space->translate( 0, 0, 1, 0); break;
        case  5: space->translate( 0, 0,-1, 0); break;
        case  7: space->rotatex(); space->rotatex(); // fall through
        case  6: space->rotatex(); break;
        case  9: space->rotatey(); space->rotatey(); // fall through
        case  8: space->rotatey(); break;
        case 11: space->rotatez(); space->rotatez(); // fall through
        case 10: space->rotatez(); break;
        case 12: space->mirrorX(); break;
        case 13: space->mirrorY(); break;
        case 14: space->mirrorZ(); break;
        case 15: space->minimizePiece(); break;
        case 16: space->actionOnSpace(voxel_c::ACT_FIXED, true); break;
        case 17: space->actionOnSpace(voxel_c::ACT_FIXED, false); break;
        case 18: space->actionOnSpace(voxel_c::ACT_VARIABLE, true); break;
        case 19: space->actionOnSpace(voxel_c::ACT_VARIABLE, false); break;
        case 20: space->actionOnSpace(voxel_c::ACT_DECOLOR, true); break;
        case 21: space->actionOnSpace(voxel_c::ACT_DECOLOR, false); break;
        case 22: space->scale(2); break;
        case 23: space->scale(3); break;
        case 24: space->translate(- space->boundX1(), - space->boundY1(), - space->boundZ1(), 0); break;
        case 25:
                 {
		   // if the space is empty, don't do anything
		   if (space->boundX2() < space->boundX1())
		     break;

                   int fx = space->getX() - (space->boundX2()-space->boundX1()+1);
                   int fy = space->getY() - (space->boundY2()-space->boundY1()+1);
                   int fz = space->getZ() - (space->boundZ2()-space->boundZ1()+1);

                   if ((fx & 1) || (fy & 1) || (fz & 1)) {
                     space->resize(space->getX()+(fx&1), space->getY()+(fy&1), space->getZ()+(fz&1), 0);
                     fx += fx&1;
                     fy += fy&1;
                     fz += fz&1;
                   }
                   space->translate(fx/2 - space->boundX1(), fy/2 - space->boundY1(), fz/2 - space->boundZ1(), 0);
                 }
                 break;
      }
      space->setHotspot(0, 0, 0);
    }

    do_callback(this, user_data());
  }
}


void ToolTab_2::setVoxelSpace(puzzle_c * puz, unsigned int sh) {
  puzzle = puz;
  shape = sh;

  bt_assert(!puzzle || (puzzle->getGridType()->getType() == gridType_c::GT_SPHERES));

  if (puzzle && shape < puzzle->shapeNumber())
    changeSize->setXYZ(puzzle->getShape(shape)->getX(),
        puzzle->getShape(shape)->getY(),
        puzzle->getShape(shape)->getZ());
  else
    changeSize->setXYZ(0, 0, 0);
}

static void cb_ToolTab2Size_stub(Fl_Widget* o, long v) { ((ToolTab_2*)(o->parent()->parent()->parent()))->cb_size(); }
static void cb_ToolTab2Transform_stub(Fl_Widget* o, long v) { ((ToolTab_2*)(o->parent()))->cb_transform(v); }
static void cb_ToolTab2Transform2_stub(Fl_Widget* o, long v) { ((ToolTab_2*)(o->parent()->parent()->parent()))->cb_transform(v); }

ToolTab_2::ToolTab_2(int x, int y, int w, int h) : ToolTab(x, y, w, h) {

  {
    layouter_c * o = new layouter_c(0, 1, 1, 1);
    o->label("Size");
    o->pitch(5);

    layouter_c *o2 = new layouter_c(0, 0, 1, 1);

    changeSize = new ChangeSize(0, 1, 1, 1);
    changeSize->callback(cb_ToolTab2Size_stub);

    toAll = new LFl_Check_Button("Apply to All Shapes (Spheres)", 0, 0, 1, 1);
    toAll->tooltip(" If this is active, all operations (including transformations and constrains are done to all shapes ");
    toAll->clear_visible_focus();
    toAll->stretchHCenter();

    o2->end();
    o2->weight(1, 0);

    (new LFl_Box(1, 0, 1, 1))->setMinimumSize(5, 0);

    o2 = new layouter_c(2, 0, 1, 1);

    (new LFl_Box(0, 0, 1, 1))->weight(1, 1);
    (new LFl_Box(0, 8, 1, 1))->weight(1, 1);

    (new LFl_Box("Grid", 0, 1, 1, 1))->labelsize(LABEL_FONT_SIZE);

    new LFlatButton_c(0, 3, 1, 1, pm.get(Grid_Color_Minimize_xpm), pm.get(Grid_Disabled_Minimize_xpm),
        " Minimize size of grid ", cb_ToolTab2Transform2_stub, 15);
    new LFlatButton_c(0, 5, 1, 1, pm.get(Grid_Color_Center_xpm), pm.get(Grid_Disabled_Center_xpm),
        " Centre shape inside the grid ", cb_ToolTab2Transform2_stub, 25);
    new LFlatButton_c(0, 7, 1, 1, pm.get(Grid_Color_Origin_xpm), pm.get(Grid_Disabled_Origin_xpm),
        " Move shape to origin of grid ", cb_ToolTab2Transform2_stub, 24);

    (new LFl_Box("Shape", 2, 1, 1, 1))->labelsize(LABEL_FONT_SIZE);

    new LFlatButton_c(2, 3, 1, 1, pm.get(Rescale_Color_X1_xpm), pm.get(Rescale_Disabled_X1_xpm),
        " Try to minimize size of shape ", cb_ToolTab2Transform2_stub, 26);
    new LFlatButton_c(2, 5, 1, 1, pm.get(Rescale_Color_X2_xpm), pm.get(Rescale_Disabled_X2_xpm),
        " Double size of shape ", cb_ToolTab2Transform2_stub, 22);
    new LFlatButton_c(2, 7, 1, 1, pm.get(Rescale_Color_X3_xpm), pm.get(Rescale_Disabled_X3_xpm),
        " Triple size of shape ", cb_ToolTab2Transform2_stub, 23);

//    (new LFl_Box(0, 2, 1, 1))->setMinimumSize(0, 5);
    (new LFl_Box(0, 4, 1, 1))->setMinimumSize(0, 5);
    (new LFl_Box(0, 6, 1, 1))->setMinimumSize(0, 5);

    o2->end();

    o->end();
  }
  {
    Fl_Group* o = new TransformButtons(0, 1, 1, 1);
    o->callback(cb_ToolTab2Transform_stub);
    o->hide();
  }
  {
    Fl_Group* o = new layouter_c(0, 1, 1, 1);
    o->hide();

    o->label("Tools");

    // this 2nd group is not necessary, but it the callback function requires it to find
    // the right parent class
    Fl_Group* o2 = new layouter_c(0, 0, 1, 1);

    (new LFl_Box(0, 0, 1, 1))->weight(1, 1);
    (new LFl_Box(0, 8, 1, 1))->weight(1, 1);
    (new LFl_Box(8, 0, 1, 1))->weight(1, 1);
    (new LFl_Box(8, 8, 1, 1))->weight(1, 1);

    (new LFl_Box("Constrain", 3, 1, 2, 1))->labelsize(LABEL_FONT_SIZE);

    new LFlatButton_c(3, 3, 1, 1, pm.get(InOut_Color_Fixed_In_xpm), pm.get(InOut_Disabled_Fixed_In_xpm),
        " Make inside fixed ", cb_ToolTab2Transform2_stub, 16);
    new LFlatButton_c(3, 5, 1, 1, pm.get(InOut_Color_Variable_In_xpm), pm.get(InOut_Disabled_Variable_In_xpm),
        " Make inside variable ", cb_ToolTab2Transform2_stub, 18);
    new LFlatButton_c(3, 7, 1, 1, pm.get(InOut_Color_RemoveColor_In_xpm), pm.get(InOut_Disabled_RemoveColor_In_xpm),
        " Remove Colours from inside cubes ", cb_ToolTab2Transform2_stub, 20);

    new LFlatButton_c(4, 3, 1, 1, pm.get(InOut_Color_Fixed_Out_xpm), pm.get(InOut_Disabled_Fixed_Out_xpm),
        " Make outside fixed ", cb_ToolTab2Transform2_stub, 17);
    new LFlatButton_c(4, 5, 1, 1, pm.get(InOut_Color_Variable_Out_xpm), pm.get(InOut_Disabled_Variable_Out_xpm),
        " Make outside variable ", cb_ToolTab2Transform2_stub, 19);
    new LFlatButton_c(4, 7, 1, 1, pm.get(InOut_Color_RemoveColor_Out_xpm), pm.get(InOut_Disabled_RemoveColor_Out_xpm),
        " Remove Colours from outside cubes ", cb_ToolTab2Transform2_stub, 21);

    (new LFl_Box(0, 2, 1, 1))->setMinimumSize(0, 5);
    (new LFl_Box(0, 4, 1, 1))->setMinimumSize(0, 5);
    (new LFl_Box(0, 6, 1, 1))->setMinimumSize(0, 5);

    o2->end();

    o->end();
  }

  end();
}

void ToolTab_2::cb_size(void) {
  if (puzzle && shape < puzzle->shapeNumber()) {

    if (toAll->value()) {

      int dx, dy, dz;

      dx = changeSize->getX() - puzzle->getShape(shape)->getX();
      dy = changeSize->getY() - puzzle->getShape(shape)->getY();
      dz = changeSize->getZ() - puzzle->getShape(shape)->getZ();

      if (dx < 0) dx = 0;
      if (dy < 0) dy = 0;
      if (dz < 0) dz = 0;

      for (unsigned int s = 0; s < puzzle->shapeNumber(); s++) {
        int nx = puzzle->getShape(s)->getX()+dx;
        int ny = puzzle->getShape(s)->getY()+dy;
        int nz = puzzle->getShape(s)->getZ()+dz;

        if (nx < 1) nx = 1;
        if (ny < 1) ny = 1;
        if (nz < 1) nz = 1;

        puzzle->getShape(s)->resize(nx, ny, nz, 0);
      }

    }

    // we always do this, is may be that the shape is not changed in the loop above because
    // that loop and only increase the size, so smaller sizes for the selected shape must be done
    // here
    puzzle->getShape(shape)->resize(changeSize->getX(), changeSize->getY(), changeSize->getZ(), 0);

    do_callback();
  }
}

void ToolTab_2::cb_transform(long task) {
  if (puzzle && shape < puzzle->shapeNumber()) {

    int ss, se;

    if (toAll->value() && ((task == 15) || ((task >= 22) && (task <= 26)))) {
      ss = 0;
      se = puzzle->shapeNumber();
    } else {
      ss = shape;
      se = shape+1;
    }

    if (task == 26) {

      unsigned char primes[] = {2, 3, 5, 7, 11, 13, 17, 19, 0};

      // special case for minimisation

      int prime = 0;

      while (primes[prime]) {

        bool canScale = true;

        for (int s = ss; s < se; s++)
          if (!puzzle->getShape(s)->scaleDown(primes[prime], false)) {
            canScale = false;
            break;
          }

        if (canScale) {
          for (int s = ss; s < se; s++)
            puzzle->getShape(s)->scaleDown(primes[prime], true);
        } else
          prime++;
      }

      for (int s = ss; s < se; s++)
        puzzle->getShape(s)->setHotspot(0, 0, 0);

      do_callback(this, user_data());

      return;
    }

    for (int s = ss; s < se; s++) {
      voxel_c * space = puzzle->getShape(s);

      switch(task) {
        case  0: space->translate( 1, 0, 0, 0); break;
        case  1: space->translate(-1, 0, 0, 0); break;
        case  2: space->translate( 0, 1, 0, 0); break;
        case  3: space->translate( 0,-1, 0, 0); break;
        case  4: space->translate( 0, 0, 1, 0); break;
        case  5: space->translate( 0, 0,-1, 0); break;
        case  7: space->rotatex(); space->rotatex(); // fall through
        case  6: space->rotatex(); break;
        case  9: space->rotatey(); space->rotatey(); // fall through
        case  8: space->rotatey(); break;
        case 11: space->rotatez(); space->rotatez(); // fall through
        case 10: space->rotatez(); break;
        case 12: space->mirrorX(); break;
        case 13: space->mirrorY(); break;
        case 14: space->mirrorZ(); break;
        case 15: space->minimizePiece(); break;
        case 16: space->actionOnSpace(voxel_c::ACT_FIXED, true); break;
        case 17: space->actionOnSpace(voxel_c::ACT_FIXED, false); break;
        case 18: space->actionOnSpace(voxel_c::ACT_VARIABLE, true); break;
        case 19: space->actionOnSpace(voxel_c::ACT_VARIABLE, false); break;
        case 20: space->actionOnSpace(voxel_c::ACT_DECOLOR, true); break;
        case 21: space->actionOnSpace(voxel_c::ACT_DECOLOR, false); break;
        case 22: space->scale(2); break;
        case 23: space->scale(3); break;
        case 24: space->translate(- space->boundX1(), - space->boundY1(), - space->boundZ1(), 0); break;
        case 25:
                 {
		   // if the space is empty, don't do anything
		   if (space->boundX2() < space->boundX1())
		     break;

                   int fx = space->getX() - (space->boundX2()-space->boundX1()+1);
                   int fy = space->getY() - (space->boundY2()-space->boundY1()+1);
                   int fz = space->getZ() - (space->boundZ2()-space->boundZ1()+1);

                   if ((fx & 1) || (fy & 1) || (fz & 1)) {
                     space->resize(space->getX()+(fx&1), space->getY()+(fy&1), space->getZ()+(fz&1), 0);
                     fx += fx&1;
                     fy += fy&1;
                     fz += fz&1;
                   }
                   space->translate(fx/2 - space->boundX1(), fy/2 - space->boundY1(), fz/2 - space->boundZ1(), 0);
                 }
                 break;
      }
      space->setHotspot(0, 0, 0);
    }

    do_callback(this, user_data());
  }
}








static void cb_ToolTabContainer_stub(Fl_Widget* o, void*v) {
  ToolTabContainer *vv = (ToolTabContainer*)v;
  vv->do_callback(vv, vv->user_data());
}

ToolTabContainer::ToolTabContainer(int x, int y, int w, int h, const guiGridType_c * ggt) : layouter_c(x, y, w, h) {
  tt = ggt->getToolTab(0, 0, 1, 1);
  tt->callback(cb_ToolTabContainer_stub, this);
  end();
}

void ToolTabContainer::newGridType(const guiGridType_c * ggt) {

  remove(tt);
  delete tt;
  tt = ggt->getToolTab(0, 0, 1, 1);
  tt->callback(cb_ToolTabContainer_stub, this);
  add(tt);
  resize(x(), y(), w(), h());
}

