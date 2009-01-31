/* Burr Solver
 * Copyright (C) 2003-2009  Andreas Röver
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
#include "Images.h"

#define static

#include "images/TB_Color_Brush.xpm"
#include "images/TB_Color_Columns_X.xpm"
#include "images/TB_Color_Columns_Y.xpm"
#include "images/TB_Color_Columns_Z.xpm"
#include "images/TB_Color_Eraser.xpm"
#include "images/TB_Color_Mouse_Drag.xpm"
#include "images/TB_Color_Mouse_Rubber_Band.xpm"
#include "images/TB_Color_Pen_Fixed.xpm"
#include "images/TB_Color_Pen_Variable.xpm"
#include "images/TB_Color_Symmetrical_X.xpm"
#include "images/TB_Color_Symmetrical_Y.xpm"
#include "images/TB_Color_Symmetrical_Z.xpm"
#include "images/TB_Disabled_Brush.xpm"
#include "images/TB_Disabled_Columns_X.xpm"
#include "images/TB_Disabled_Columns_Y.xpm"
#include "images/TB_Disabled_Columns_Z.xpm"
#include "images/TB_Disabled_Eraser.xpm"
#include "images/TB_Disabled_Mouse_Drag.xpm"
#include "images/TB_Disabled_Mouse_Rubber_Band.xpm"
#include "images/TB_Disabled_Pen_Fixed.xpm"
#include "images/TB_Disabled_Pen_Variable.xpm"
#include "images/TB_Disabled_Symmetrical_X.xpm"
#include "images/TB_Disabled_Symmetrical_Y.xpm"
#include "images/TB_Disabled_Symmetrical_Z.xpm"
#include "images/TB_Monochrome_Brush.xpm"
#include "images/TB_Monochrome_Columns_X.xpm"
#include "images/TB_Monochrome_Columns_Y.xpm"
#include "images/TB_Monochrome_Columns_Z.xpm"
#include "images/TB_Monochrome_Eraser.xpm"
#include "images/TB_Monochrome_Mouse_Drag.xpm"
#include "images/TB_Monochrome_Mouse_Rubber_Band.xpm"
#include "images/TB_Monochrome_Pen_Fixed.xpm"
#include "images/TB_Monochrome_Pen_Variable.xpm"
#include "images/TB_Monochrome_Symmetrical_X.xpm"
#include "images/TB_Monochrome_Symmetrical_Y.xpm"
#include "images/TB_Monochrome_Symmetrical_Z.xpm"

#include "images/Transform_Color_Flip_X.xpm"
#include "images/Transform_Color_Flip_Y.xpm"
#include "images/Transform_Color_Flip_Z.xpm"
#include "images/Transform_Color_Nudge_X_Left.xpm"
#include "images/Transform_Color_Nudge_X_Right.xpm"
#include "images/Transform_Color_Nudge_Y_Left.xpm"
#include "images/Transform_Color_Nudge_Y_Right.xpm"
#include "images/Transform_Color_Nudge_Z_Left.xpm"
#include "images/Transform_Color_Nudge_Z_Right.xpm"
#include "images/Transform_Color_Rotate_X_Left.xpm"
#include "images/Transform_Color_Rotate_X_Right.xpm"
#include "images/Transform_Color_Rotate_Y_Left.xpm"
#include "images/Transform_Color_Rotate_Y_Right.xpm"
#include "images/Transform_Color_Rotate_Z_Left.xpm"
#include "images/Transform_Color_Rotate_Z_Right.xpm"
#include "images/Transform_Disabled_Flip_X.xpm"
#include "images/Transform_Disabled_Flip_Y.xpm"
#include "images/Transform_Disabled_Flip_Z.xpm"
#include "images/Transform_Disabled_Nudge_X_Left.xpm"
#include "images/Transform_Disabled_Nudge_X_Right.xpm"
#include "images/Transform_Disabled_Nudge_Y_Left.xpm"
#include "images/Transform_Disabled_Nudge_Y_Right.xpm"
#include "images/Transform_Disabled_Nudge_Z_Left.xpm"
#include "images/Transform_Disabled_Nudge_Z_Right.xpm"
#include "images/Transform_Disabled_Rotate_X_Left.xpm"
#include "images/Transform_Disabled_Rotate_X_Right.xpm"
#include "images/Transform_Disabled_Rotate_Y_Left.xpm"
#include "images/Transform_Disabled_Rotate_Y_Right.xpm"
#include "images/Transform_Disabled_Rotate_Z_Left.xpm"
#include "images/Transform_Disabled_Rotate_Z_Right.xpm"
#include "images/Transform_Monochrome_Flip_X.xpm"
#include "images/Transform_Monochrome_Flip_Y.xpm"
#include "images/Transform_Monochrome_Flip_Z.xpm"
#include "images/Transform_Monochrome_Nudge_X_Left.xpm"
#include "images/Transform_Monochrome_Nudge_X_Right.xpm"
#include "images/Transform_Monochrome_Nudge_Y_Left.xpm"
#include "images/Transform_Monochrome_Nudge_Y_Right.xpm"
#include "images/Transform_Monochrome_Nudge_Z_Left.xpm"
#include "images/Transform_Monochrome_Nudge_Z_Right.xpm"
#include "images/Transform_Monochrome_Rotate_X_Left.xpm"
#include "images/Transform_Monochrome_Rotate_X_Right.xpm"
#include "images/Transform_Monochrome_Rotate_Y_Left.xpm"
#include "images/Transform_Monochrome_Rotate_Y_Right.xpm"
#include "images/Transform_Monochrome_Rotate_Z_Left.xpm"
#include "images/Transform_Monochrome_Rotate_Z_Right.xpm"

#include "images/InOut_Color_Fixed_In.xpm"
#include "images/InOut_Color_Fixed_Out.xpm"
#include "images/InOut_Color_RemoveColor_In.xpm"
#include "images/InOut_Color_RemoveColor_Out.xpm"
#include "images/InOut_Color_Variable_In.xpm"
#include "images/InOut_Color_Variable_Out.xpm"
#include "images/InOut_Disabled_Fixed_In.xpm"
#include "images/InOut_Disabled_Fixed_Out.xpm"
#include "images/InOut_Disabled_RemoveColor_In.xpm"
#include "images/InOut_Disabled_RemoveColor_Out.xpm"
#include "images/InOut_Disabled_Variable_In.xpm"
#include "images/InOut_Disabled_Variable_Out.xpm"
#include "images/InOut_Monochrome_Fixed_In.xpm"
#include "images/InOut_Monochrome_Fixed_Out.xpm"
#include "images/InOut_Monochrome_RemoveColor_In.xpm"
#include "images/InOut_Monochrome_RemoveColor_Out.xpm"
#include "images/InOut_Monochrome_Variable_In.xpm"
#include "images/InOut_Monochrome_Variable_Out.xpm"

#include "images/Grid_Color_Center.xpm"
#include "images/Grid_Color_Minimize.xpm"
#include "images/Grid_Color_Origin.xpm"
#include "images/Grid_Disabled_Center.xpm"
#include "images/Grid_Disabled_Minimize.xpm"
#include "images/Grid_Disabled_Origin.xpm"
#include "images/Grid_Monochrome_Center.xpm"
#include "images/Grid_Monochrome_Minimize.xpm"
#include "images/Grid_Monochrome_Origin.xpm"

#include "images/Rescale_Color_X1.xpm"
#include "images/Rescale_Color_X2.xpm"
#include "images/Rescale_Color_X3.xpm"
#include "images/Rescale_Disabled_X1.xpm"
#include "images/Rescale_Disabled_X2.xpm"
#include "images/Rescale_Disabled_X3.xpm"
#include "images/Rescale_Monochrome_X1.xpm"
#include "images/Rescale_Monochrome_X2.xpm"
#include "images/Rescale_Monochrome_X3.xpm"

#include "images/ViewModeNormal.xpm"
#include "images/ViewModeColor.xpm"
#include "images/ViewMode3D.xpm"
#include "images/ViewMode3DL.xpm"

pixmapList_c::~pixmapList_c(void) {
  for (unsigned int i = 0; i < list.size(); i++)
    delete list[i];
}

Fl_Pixmap * pixmapList_c::get(const char * data[]) {
  list.push_back(new Fl_Pixmap(data));
  return list[list.size()-1];
}

