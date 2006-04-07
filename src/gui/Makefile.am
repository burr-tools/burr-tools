bin_PROGRAMS=burrGui

GUI_CPP_ADD=@GUI_CPP_ADD@
GUI_LD_ADD=@GUI_LD_ADD@
burrGui_LDADD=-L../lib -lburr

burrGui_CPPFLAGS=$(GUI_CPP_ADD) $(XML_CPP_ADD) -Wall
burrGui_LDFLAGS=$(GUI_LD_ADD) $(XML_LD_ADD)
burrGui_SOURCES=VoxelView.cpp VoxelView.h \
		voxeldrawer.cpp voxeldrawer.h \
		voxeldrawer_0.cpp voxeldrawer_0.h \
		voxeldrawer_1.cpp voxeldrawer_1.h \
                ArcBall.cpp ArcBall.h \
		statuswindow.cpp statuswindow.h \
		guigridtype.cpp guigridtype.h \
		gridtypegui.cpp gridtypegui.h \
                BlockList.cpp BlockList.h \
                MainWindow.cpp MainWindow.h \
                WindowWidgets.cpp WindowWidgets.h \
		grideditor.cpp grideditor.h \
                grideditor_0.cpp grideditor_0.h \
                grideditor_1.cpp grideditor_1.h \
                DisasmToMoves.cpp DisasmToMoves.h \
                pieceColor.cpp pieceColor.h \
                gzstream.cpp gzstream.h \
                Fl_Table.cpp Fl_Table.h \
                GroupsEditor.cpp GroupsEditor.h \
                PlacementBrowser.cpp PlacementBrowser.h \
                configuration.cpp configuration.h \
		Layouter.cpp Layouter.h \
		ImageExport.cpp ImageExport.h \
		Image.cpp Image.h \
		tr.c tr.h \
		Images.cpp Images.h \
                main.cpp \
                images/TB_Color_Brush.xpm images/TB_Color_Eraser.xpm \
		images/TB_Color_Mouse_Drag.xpm images/TB_Color_Columns_X.xpm \
		images/TB_Color_Columns_Y.xpm images/TB_Color_Columns_Z.xpm \
		images/TB_Color_Mouse_Rubber_Band.xpm images/TB_Color_Pen_Fixed.xpm \
		images/TB_Color_Pen_Variable.xpm images/TB_Color_Symmetrical_X.xpm \
		images/TB_Color_Symmetrical_Y.xpm images/TB_Color_Symmetrical_Z.xpm \
		images/TB_Disabled_Brush.xpm images/TB_Disabled_Columns_X.xpm \
		images/TB_Disabled_Columns_Y.xpm images/TB_Disabled_Columns_Z.xpm \
		images/TB_Disabled_Eraser.xpm images/TB_Disabled_Mouse_Drag.xpm \
		images/TB_Disabled_Mouse_Rubber_Band.xpm images/TB_Disabled_Pen_Fixed.xpm \
		images/TB_Disabled_Pen_Variable.xpm images/TB_Disabled_Symmetrical_X.xpm \
		images/TB_Disabled_Symmetrical_Y.xpm images/TB_Disabled_Symmetrical_Z.xpm \
		images/TB_Monochrome_Brush.xpm images/TB_Monochrome_Columns_X.xpm \
		images/TB_Monochrome_Columns_Y.xpm images/TB_Monochrome_Columns_Z.xpm \
		images/TB_Monochrome_Eraser.xpm images/TB_Monochrome_Mouse_Drag.xpm \
		images/TB_Monochrome_Mouse_Rubber_Band.xpm images/TB_Monochrome_Pen_Fixed.xpm \
		images/TB_Monochrome_Pen_Variable.xpm images/TB_Monochrome_Symmetrical_X.xpm \
		images/TB_Monochrome_Symmetrical_Y.xpm images/TB_Monochrome_Symmetrical_Z.xpm \
		images/Transform_Color_Flip_X.xpm images/Transform_Color_Flip_Y.xpm images/Transform_Color_Flip_Z.xpm \
		images/Transform_Color_Nudge_X_Left.xpm images/Transform_Color_Nudge_X_Right.xpm \
		images/Transform_Color_Nudge_Y_Left.xpm images/Transform_Color_Nudge_Y_Right.xpm \
		images/Transform_Color_Nudge_Z_Left.xpm images/Transform_Color_Nudge_Z_Right.xpm \
		images/Transform_Color_Rotate_X_Left.xpm images/Transform_Color_Rotate_X_Right.xpm \
		images/Transform_Color_Rotate_Y_Left.xpm images/Transform_Color_Rotate_Y_Right.xpm \
		images/Transform_Color_Rotate_Z_Left.xpm images/Transform_Color_Rotate_Z_Right.xpm \
		images/Transform_Disabled_Flip_X.xpm images/Transform_Disabled_Flip_Y.xpm \
		images/Transform_Disabled_Flip_Z.xpm images/Transform_Disabled_Nudge_X_Left.xpm \
		images/Transform_Disabled_Nudge_X_Right.xpm images/Transform_Disabled_Nudge_Y_Left.xpm \
		images/Transform_Disabled_Nudge_Y_Right.xpm images/Transform_Disabled_Nudge_Z_Left.xpm \
		images/Transform_Disabled_Nudge_Z_Right.xpm images/Transform_Disabled_Rotate_X_Left.xpm \
		images/Transform_Disabled_Rotate_X_Right.xpm images/Transform_Disabled_Rotate_Y_Left.xpm \
		images/Transform_Disabled_Rotate_Y_Right.xpm images/Transform_Disabled_Rotate_Z_Left.xpm \
		images/Transform_Disabled_Rotate_Z_Right.xpm images/Transform_Monochrome_Flip_X.xpm \
		images/Transform_Monochrome_Flip_Y.xpm images/Transform_Monochrome_Flip_Z.xpm \
		images/Transform_Monochrome_Nudge_X_Left.xpm images/Transform_Monochrome_Nudge_X_Right.xpm \
		images/Transform_Monochrome_Nudge_Y_Left.xpm images/Transform_Monochrome_Nudge_Y_Right.xpm \
		images/Transform_Monochrome_Nudge_Z_Left.xpm images/Transform_Monochrome_Nudge_Z_Right.xpm \
		images/Transform_Monochrome_Rotate_X_Left.xpm images/Transform_Monochrome_Rotate_X_Right.xpm \
		images/Transform_Monochrome_Rotate_Y_Left.xpm images/Transform_Monochrome_Rotate_Y_Right.xpm \
		images/Transform_Monochrome_Rotate_Z_Left.xpm images/Transform_Monochrome_Rotate_Z_Right.xpm \
		images/InOut_Color_Fixed_In.xpm images/InOut_Color_Fixed_Out.xpm \
		images/InOut_Color_RemoveColor_In.xpm images/InOut_Color_RemoveColor_Out.xpm \
		images/InOut_Color_Variable_In.xpm images/InOut_Color_Variable_Out.xpm \
		images/InOut_Disabled_Fixed_In.xpm images/InOut_Disabled_Fixed_Out.xpm \
		images/InOut_Disabled_RemoveColor_In.xpm images/InOut_Disabled_RemoveColor_Out.xpm \
		images/InOut_Disabled_Variable_In.xpm images/InOut_Disabled_Variable_Out.xpm \
		images/InOut_Monochrome_Fixed_In.xpm images/InOut_Monochrome_Fixed_Out.xpm \
		images/InOut_Monochrome_RemoveColor_In.xpm images/InOut_Monochrome_RemoveColor_Out.xpm \
		images/InOut_Monochrome_Variable_In.xpm images/InOut_Monochrome_Variable_Out.xpm \
		images/Grid_Color_Center.xpm images/Grid_Color_Minimize.xpm images/Grid_Color_Origin.xpm \
		images/Grid_Disabled_Center.xpm images/Grid_Disabled_Minimize.xpm images/Grid_Disabled_Origin.xpm \
		images/Grid_Monochrome_Center.xpm images/Grid_Monochrome_Minimize.xpm images/Grid_Monochrome_Origin.xpm \
		images/Rescale_Color_X1.xpm images/Rescale_Color_X2.xpm images/Rescale_Color_X3.xpm \
		images/Rescale_Disabled_X1.xpm images/Rescale_Disabled_X2.xpm images/Rescale_Disabled_X3.xpm \
 	        images/Rescale_Monochrome_X1.xpm images/Rescale_Monochrome_X2.xpm images/Rescale_Monochrome_X3.xpm


