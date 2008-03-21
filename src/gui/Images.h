/* Burr Solver
 * Copyright (C) 2003-2008  Andreas Röver
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
#ifndef __IMAGES_H__
#define __IMAGES_H__

#include <vector>

#include <FL/Fl_Pixmap.H>

extern const char * TB_Color_Brush_xpm[];
extern const char * TB_Color_Columns_X_xpm[];
extern const char * TB_Color_Columns_Y_xpm[];
extern const char * TB_Color_Columns_Z_xpm[];
extern const char * TB_Color_Eraser_xpm[];
extern const char * TB_Color_Mouse_Drag_xpm[];
extern const char * TB_Color_Mouse_Rubber_Band_xpm[];
extern const char * TB_Color_Pen_Fixed_xpm[];
extern const char * TB_Color_Pen_Variable_xpm[];
extern const char * TB_Color_Symmetrical_X_xpm[];
extern const char * TB_Color_Symmetrical_Y_xpm[];
extern const char * TB_Color_Symmetrical_Z_xpm[];
extern const char * TB_Disabled_Brush_xpm[];
extern const char * TB_Disabled_Columns_X_xpm[];
extern const char * TB_Disabled_Columns_Y_xpm[];
extern const char * TB_Disabled_Columns_Z_xpm[];
extern const char * TB_Disabled_Eraser_xpm[];
extern const char * TB_Disabled_Mouse_Drag_xpm[];
extern const char * TB_Disabled_Mouse_Rubber_Band_xpm[];
extern const char * TB_Disabled_Pen_Fixed_xpm[];
extern const char * TB_Disabled_Pen_Variable_xpm[];
extern const char * TB_Disabled_Symmetrical_X_xpm[];
extern const char * TB_Disabled_Symmetrical_Y_xpm[];
extern const char * TB_Disabled_Symmetrical_Z_xpm[];
extern const char * TB_Monochrome_Brush_xpm[];
extern const char * TB_Monochrome_Columns_X_xpm[];
extern const char * TB_Monochrome_Columns_Y_xpm[];
extern const char * TB_Monochrome_Columns_Z_xpm[];
extern const char * TB_Monochrome_Eraser_xpm[];
extern const char * TB_Monochrome_Mouse_Drag_xpm[];
extern const char * TB_Monochrome_Mouse_Rubber_Band_xpm[];
extern const char * TB_Monochrome_Pen_Fixed_xpm[];
extern const char * TB_Monochrome_Pen_Variable_xpm[];
extern const char * TB_Monochrome_Symmetrical_X_xpm[];
extern const char * TB_Monochrome_Symmetrical_Y_xpm[];
extern const char * TB_Monochrome_Symmetrical_Z_xpm[];

extern const char * Transform_Color_Flip_X_xpm[];
extern const char * Transform_Color_Flip_Y_xpm[];
extern const char * Transform_Color_Flip_Z_xpm[];
extern const char * Transform_Color_Nudge_X_Left_xpm[];
extern const char * Transform_Color_Nudge_X_Right_xpm[];
extern const char * Transform_Color_Nudge_Y_Left_xpm[];
extern const char * Transform_Color_Nudge_Y_Right_xpm[];
extern const char * Transform_Color_Nudge_Z_Left_xpm[];
extern const char * Transform_Color_Nudge_Z_Right_xpm[];
extern const char * Transform_Color_Rotate_X_Left_xpm[];
extern const char * Transform_Color_Rotate_X_Right_xpm[];
extern const char * Transform_Color_Rotate_Y_Left_xpm[];
extern const char * Transform_Color_Rotate_Y_Right_xpm[];
extern const char * Transform_Color_Rotate_Z_Left_xpm[];
extern const char * Transform_Color_Rotate_Z_Right_xpm[];
extern const char * Transform_Disabled_Flip_X_xpm[];
extern const char * Transform_Disabled_Flip_Y_xpm[];
extern const char * Transform_Disabled_Flip_Z_xpm[];
extern const char * Transform_Disabled_Nudge_X_Left_xpm[];
extern const char * Transform_Disabled_Nudge_X_Right_xpm[];
extern const char * Transform_Disabled_Nudge_Y_Left_xpm[];
extern const char * Transform_Disabled_Nudge_Y_Right_xpm[];
extern const char * Transform_Disabled_Nudge_Z_Left_xpm[];
extern const char * Transform_Disabled_Nudge_Z_Right_xpm[];
extern const char * Transform_Disabled_Rotate_X_Left_xpm[];
extern const char * Transform_Disabled_Rotate_X_Right_xpm[];
extern const char * Transform_Disabled_Rotate_Y_Left_xpm[];
extern const char * Transform_Disabled_Rotate_Y_Right_xpm[];
extern const char * Transform_Disabled_Rotate_Z_Left_xpm[];
extern const char * Transform_Disabled_Rotate_Z_Right_xpm[];
extern const char * Transform_Monochrome_Flip_X_xpm[];
extern const char * Transform_Monochrome_Flip_Y_xpm[];
extern const char * Transform_Monochrome_Flip_Z_xpm[];
extern const char * Transform_Monochrome_Nudge_X_Left_xpm[];
extern const char * Transform_Monochrome_Nudge_X_Right_xpm[];
extern const char * Transform_Monochrome_Nudge_Y_Left_xpm[];
extern const char * Transform_Monochrome_Nudge_Y_Right_xpm[];
extern const char * Transform_Monochrome_Nudge_Z_Left_xpm[];
extern const char * Transform_Monochrome_Nudge_Z_Right_xpm[];
extern const char * Transform_Monochrome_Rotate_X_Left_xpm[];
extern const char * Transform_Monochrome_Rotate_X_Right_xpm[];
extern const char * Transform_Monochrome_Rotate_Y_Left_xpm[];
extern const char * Transform_Monochrome_Rotate_Y_Right_xpm[];
extern const char * Transform_Monochrome_Rotate_Z_Left_xpm[];
extern const char * Transform_Monochrome_Rotate_Z_Right_xpm[];

extern const char * InOut_Color_Fixed_In_xpm[];
extern const char * InOut_Color_Fixed_Out_xpm[];
extern const char * InOut_Color_RemoveColor_In_xpm[];
extern const char * InOut_Color_RemoveColor_Out_xpm[];
extern const char * InOut_Color_Variable_In_xpm[];
extern const char * InOut_Color_Variable_Out_xpm[];
extern const char * InOut_Disabled_Fixed_In_xpm[];
extern const char * InOut_Disabled_Fixed_Out_xpm[];
extern const char * InOut_Disabled_RemoveColor_In_xpm[];
extern const char * InOut_Disabled_RemoveColor_Out_xpm[];
extern const char * InOut_Disabled_Variable_In_xpm[];
extern const char * InOut_Disabled_Variable_Out_xpm[];
extern const char * InOut_Monochrome_Fixed_In_xpm[];
extern const char * InOut_Monochrome_Fixed_Out_xpm[];
extern const char * InOut_Monochrome_RemoveColor_In_xpm[];
extern const char * InOut_Monochrome_RemoveColor_Out_xpm[];
extern const char * InOut_Monochrome_Variable_In_xpm[];
extern const char * InOut_Monochrome_Variable_Out_xpm[];

extern const char * Grid_Color_Center_xpm[];
extern const char * Grid_Color_Minimize_xpm[];
extern const char * Grid_Color_Origin_xpm[];
extern const char * Grid_Disabled_Center_xpm[];
extern const char * Grid_Disabled_Minimize_xpm[];
extern const char * Grid_Disabled_Origin_xpm[];
extern const char * Grid_Monochrome_Center_xpm[];
extern const char * Grid_Monochrome_Minimize_xpm[];
extern const char * Grid_Monochrome_Origin_xpm[];

extern const char * Rescale_Color_X1_xpm[];
extern const char * Rescale_Color_X2_xpm[];
extern const char * Rescale_Color_X3_xpm[];
extern const char * Rescale_Disabled_X1_xpm[];
extern const char * Rescale_Disabled_X2_xpm[];
extern const char * Rescale_Disabled_X3_xpm[];
extern const char * Rescale_Monochrome_X1_xpm[];
extern const char * Rescale_Monochrome_X2_xpm[];
extern const char * Rescale_Monochrome_X3_xpm[];

extern const char * ViewModeNormal_xpm[];
extern const char * ViewModeColor_xpm[];
extern const char * ViewMode3D_xpm[];
extern const char * ViewMode3DL_xpm[];

/* just a little cache to make sure the pixmaps are freed after usage
 *
 * usage is simple, put one instance of this class into your widget that
 * uses pixmaps and use this the function inside to create the pixmap
 *
 * once the class is freed, all images are, too
 *
 */
class pixmapList_c {

  private:

    std::vector<Fl_Pixmap*> list;

  public:

    pixmapList_c(void) {}

    ~pixmapList_c(void);

    Fl_Pixmap * get(const char * data[]);
};

#endif
