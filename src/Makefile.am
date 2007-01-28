bin_PROGRAMS=tester burrTxt converter gui/burrGui
noinst_LIBRARIES=lib/libburr.a
noinst_PROGRAMS=lib/tabs_0/generator_0 lib/tabs_1/generator_1 lib/tabs_2/generator_2

XML_CPP_ADD=@XML_CPP_ADD@
XML_LD_ADD=@XML_LD_ADD@
GUI_CPP_ADD=@GUI_CPP_ADD@
GUI_LD_ADD=@GUI_LD_ADD@

AM_CPPFLAGS=-Wall $(XML_CPP_ADD) $(GUI_CPP_ADD)
LDADD=lib/libburr.a $(GUI_LD_ADD) $(XML_LD_ADD)
lib_tabs_0_generator_0_LDADD=
lib_tabs_1_generator_1_LDADD=
lib_tabs_2_generator_2_LDADD=

tester_SOURCES=tester.cpp
burrTxt_SOURCES=burrTxt.cpp
converter_SOURCES=converter.cpp

lib_libburr_a_SOURCES=\
    lib/assembler_0.cpp \
    lib/assembly.cpp \
    lib/assm_0_frontend_0.cpp \
    lib/assm_0_frontend_1.cpp \
    lib/assm_0_frontend_2.cpp \
    lib/assemblerthread.cpp \
    lib/bt_assert.cpp \
    lib/burrgrower.cpp \
    lib/disassembler_0.cpp \
    lib/disassembly.cpp \
    lib/disasmtomoves.cpp \
    lib/gridtype.cpp \
    lib/grouping.cpp \
    lib/movementcache.cpp \
    lib/piecegenerator.cpp \
    lib/print.cpp \
    lib/ps3dloader.cpp \
    lib/puzzle.cpp \
    lib/symmetries_0.cpp \
    lib/symmetries_1.cpp \
    lib/symmetries_2.cpp \
    lib/voxel.cpp \
    lib/voxel_0.cpp \
    lib/voxel_1.cpp \
    lib/voxel_2.cpp \
    \
    lib/tabs_0/rotmatrix.inc lib/tabs_0/symmetries.inc lib/tabs_0/tablesizes.inc \
    lib/tabs_1/rotx.inc lib/tabs_1/roty.inc lib/tabs_1/rotz.inc lib/tabs_1/symmetries.inc lib/tabs_1/tablesizes.inc \
    lib/tabs_2/rotmatrix.inc lib/tabs_2/symmetries.inc lib/tabs_2/tablesizes.inc \
    \
    lib/tabs_0/symcalc.inc lib/tabs_0/transformmini.inc lib/tabs_0/transmult.inc lib/tabs_0/unifiedsym.inc \
    lib/tabs_1/symcalc.inc lib/tabs_1/transformmini.inc lib/tabs_1/transmult.inc lib/tabs_1/unifiedsym.inc \
    lib/tabs_2/symcalc.inc lib/tabs_2/transformmini.inc lib/tabs_2/transmult.inc lib/tabs_2/unifiedsym.inc

BUILT_SOURCES=\
    lib/tabs_0/symcalc.inc lib/tabs_0/transformmini.inc lib/tabs_0/transmult.inc lib/tabs_0/unifiedsym.inc \
    lib/tabs_1/symcalc.inc lib/tabs_1/transformmini.inc lib/tabs_1/transmult.inc lib/tabs_1/unifiedsym.inc \
    lib/tabs_2/symcalc.inc lib/tabs_2/transformmini.inc lib/tabs_2/transmult.inc lib/tabs_2/unifiedsym.inc
    
pkginclude_HEADERS=\
    lib/assembler.h \
    lib/assembler_0.h \
    lib/assembly.h \
    lib/assm_0_frontend_0.h \
    lib/assm_0_frontend_1.h \
    lib/assm_0_frontend_2.h \
    lib/assemblerthread.h \
    lib/bitfield.h \
    lib/bt_assert.h \
    lib/burrgrower.h \
    lib/disassembler.h \
    lib/disassembler_0.h \
    lib/disassembly.h \
    lib/disasmtomoves.h \
    lib/gridtype.h \
    lib/grouping.h \
    lib/movementcache.h \
    lib/piecegenerator.h \
    lib/print.h \
    lib/ps3dloader.h\
    lib/puzzle.h \
    lib/symmetries.h \
    lib/symmetries_0.h \
    lib/symmetries_1.h \
    lib/symmetries_2.h \
    lib/types.h \
    lib/voxel.h \
    lib/voxel_0.h \
    lib/voxel_1.h \
    lib/voxel_2.h

lib/tabs_0/symcalc.inc lib/tabs_0/transformmini.inc lib/tabs_0/transmult.inc lib/tabs_0/unifiedsym.inc : lib/tabs_0/generator_0
	cd lib/tabs_0; ./generator_0

lib/tabs_1/symcalc.inc lib/tabs_1/transformmini.inc lib/tabs_1/transmult.inc lib/tabs_1/unifiedsym.inc : lib/tabs_1/generator_1
	cd lib/tabs_1; ./generator_1

lib/tabs_2/symcalc.inc lib/tabs_2/transformmini.inc lib/tabs_2/transmult.inc lib/tabs_2/unifiedsym.inc : lib/tabs_2/generator_2
	cd lib/tabs_2; ./generator_2


lib_tabs_0_generator_0_SOURCES=\
    lib/tabs_0/generator_0.cpp \
    lib/tabs_0/rotx.inc \
    lib/tabs_0/roty.inc \
    lib/tabs_0/rotz.inc \
    lib/tabs_0/symmetries.inc \
    lib/tabs_0/tablesizes.inc

lib_tabs_1_generator_1_SOURCES=\
    lib/tabs_1/generator_1.cpp \
    lib/tabs_1/rotx.inc \
    lib/tabs_1/roty.inc \
    lib/tabs_1/rotz.inc \
    lib/tabs_1/symmetries.inc \
    lib/tabs_1/tablesizes.inc

lib_tabs_2_generator_2_SOURCES=\
    lib/tabs_2/generator_2.cpp \
    lib/tabs_2/rotmatrix.inc \
    lib/bitfield.h \
    lib/tabs_2/symmetries.inc \
    lib/tabs_2/tablesizes.inc


gui_burrGui_SOURCES=\
     gui/arcball.cpp \
     gui/configuration.cpp \
     gui/grideditor.cpp \
     gui/grideditor_0.cpp \
     gui/grideditor_1.cpp \
     gui/grideditor_2.cpp \
     gui/gridtypegui.cpp \
     gui/groupseditor.cpp \
     gui/guigridtype.cpp \
     gui/image.cpp \
     gui/imageexport.cpp \
     gui/main.cpp \
     gui/mainwindow.cpp \
     gui/piececolor.cpp \
     gui/placementbrowser.cpp \
     gui/statuswindow.cpp \
     gui/tooltabs.cpp \
     gui/voxeldrawer.cpp \
     gui/voxeldrawer_0.cpp \
     gui/voxeldrawer_1.cpp \
     gui/voxeldrawer_2.cpp \
     \
     gui/BlockList.cpp \
     gui/WindowWidgets.cpp \
     gui/gzstream.cpp \
     gui/Fl_Table.cpp \
     gui/LFl_Tile.cpp \
     gui/Layouter.cpp \
     gui/tr.c \
     gui/Images.cpp \
     \
     gui/arcball.h \
     gui/configuration.h \
     gui/grideditor.h \
     gui/grideditor_0.h \
     gui/grideditor_1.h \
     gui/grideditor_2.h \
     gui/gridtypegui.h \
     gui/groupseditor.h \
     gui/guigridtype.h \
     gui/image.h \
     gui/imageexport.h \
     gui/mainwindow.h \
     gui/piececolor.h \
     gui/placementbrowser.h \
     gui/statuswindow.h \
     gui/tooltabs.h \
     gui/voxeldrawer.h \
     gui/voxeldrawer_0.h \
     gui/voxeldrawer_1.h \
     gui/voxeldrawer_2.h \
     \
     gui/BlockList.h \
     gui/WindowWidgets.h \
     gui/gzstream.h \
     gui/Fl_Table.h \
     gui/LFl_Tile.h \
     gui/Layouter.h \
     gui/tr.h \
     gui/Images.h \
     \
     gui/images/TB_Color_Brush.xpm gui/images/TB_Color_Eraser.xpm \
     gui/images/TB_Color_Mouse_Drag.xpm gui/images/TB_Color_Columns_X.xpm \
     gui/images/TB_Color_Columns_Y.xpm gui/images/TB_Color_Columns_Z.xpm \
     gui/images/TB_Color_Mouse_Rubber_Band.xpm gui/images/TB_Color_Pen_Fixed.xpm \
     gui/images/TB_Color_Pen_Variable.xpm gui/images/TB_Color_Symmetrical_X.xpm \
     gui/images/TB_Color_Symmetrical_Y.xpm gui/images/TB_Color_Symmetrical_Z.xpm \
     gui/images/TB_Disabled_Brush.xpm gui/images/TB_Disabled_Columns_X.xpm \
     gui/images/TB_Disabled_Columns_Y.xpm gui/images/TB_Disabled_Columns_Z.xpm \
     gui/images/TB_Disabled_Eraser.xpm gui/images/TB_Disabled_Mouse_Drag.xpm \
     gui/images/TB_Disabled_Mouse_Rubber_Band.xpm gui/images/TB_Disabled_Pen_Fixed.xpm \
     gui/images/TB_Disabled_Pen_Variable.xpm gui/images/TB_Disabled_Symmetrical_X.xpm \
     gui/images/TB_Disabled_Symmetrical_Y.xpm gui/images/TB_Disabled_Symmetrical_Z.xpm \
     gui/images/TB_Monochrome_Brush.xpm gui/images/TB_Monochrome_Columns_X.xpm \
     gui/images/TB_Monochrome_Columns_Y.xpm gui/images/TB_Monochrome_Columns_Z.xpm \
     gui/images/TB_Monochrome_Eraser.xpm gui/images/TB_Monochrome_Mouse_Drag.xpm \
     gui/images/TB_Monochrome_Mouse_Rubber_Band.xpm gui/images/TB_Monochrome_Pen_Fixed.xpm \
     gui/images/TB_Monochrome_Pen_Variable.xpm gui/images/TB_Monochrome_Symmetrical_X.xpm \
     gui/images/TB_Monochrome_Symmetrical_Y.xpm gui/images/TB_Monochrome_Symmetrical_Z.xpm \
     gui/images/Transform_Color_Flip_X.xpm gui/images/Transform_Color_Flip_Y.xpm gui/images/Transform_Color_Flip_Z.xpm \
     gui/images/Transform_Color_Nudge_X_Left.xpm gui/images/Transform_Color_Nudge_X_Right.xpm \
     gui/images/Transform_Color_Nudge_Y_Left.xpm gui/images/Transform_Color_Nudge_Y_Right.xpm \
     gui/images/Transform_Color_Nudge_Z_Left.xpm gui/images/Transform_Color_Nudge_Z_Right.xpm \
     gui/images/Transform_Color_Rotate_X_Left.xpm gui/images/Transform_Color_Rotate_X_Right.xpm \
     gui/images/Transform_Color_Rotate_Y_Left.xpm gui/images/Transform_Color_Rotate_Y_Right.xpm \
     gui/images/Transform_Color_Rotate_Z_Left.xpm gui/images/Transform_Color_Rotate_Z_Right.xpm \
     gui/images/Transform_Disabled_Flip_X.xpm gui/images/Transform_Disabled_Flip_Y.xpm \
     gui/images/Transform_Disabled_Flip_Z.xpm gui/images/Transform_Disabled_Nudge_X_Left.xpm \
     gui/images/Transform_Disabled_Nudge_X_Right.xpm gui/images/Transform_Disabled_Nudge_Y_Left.xpm \
     gui/images/Transform_Disabled_Nudge_Y_Right.xpm gui/images/Transform_Disabled_Nudge_Z_Left.xpm \
     gui/images/Transform_Disabled_Nudge_Z_Right.xpm gui/images/Transform_Disabled_Rotate_X_Left.xpm \
     gui/images/Transform_Disabled_Rotate_X_Right.xpm gui/images/Transform_Disabled_Rotate_Y_Left.xpm \
     gui/images/Transform_Disabled_Rotate_Y_Right.xpm gui/images/Transform_Disabled_Rotate_Z_Left.xpm \
     gui/images/Transform_Disabled_Rotate_Z_Right.xpm gui/images/Transform_Monochrome_Flip_X.xpm \
     gui/images/Transform_Monochrome_Flip_Y.xpm gui/images/Transform_Monochrome_Flip_Z.xpm \
     gui/images/Transform_Monochrome_Nudge_X_Left.xpm gui/images/Transform_Monochrome_Nudge_X_Right.xpm \
     gui/images/Transform_Monochrome_Nudge_Y_Left.xpm gui/images/Transform_Monochrome_Nudge_Y_Right.xpm \
     gui/images/Transform_Monochrome_Nudge_Z_Left.xpm gui/images/Transform_Monochrome_Nudge_Z_Right.xpm \
     gui/images/Transform_Monochrome_Rotate_X_Left.xpm gui/images/Transform_Monochrome_Rotate_X_Right.xpm \
     gui/images/Transform_Monochrome_Rotate_Y_Left.xpm gui/images/Transform_Monochrome_Rotate_Y_Right.xpm \
     gui/images/Transform_Monochrome_Rotate_Z_Left.xpm gui/images/Transform_Monochrome_Rotate_Z_Right.xpm \
     gui/images/InOut_Color_Fixed_In.xpm gui/images/InOut_Color_Fixed_Out.xpm \
     gui/images/InOut_Color_RemoveColor_In.xpm gui/images/InOut_Color_RemoveColor_Out.xpm \
     gui/images/InOut_Color_Variable_In.xpm gui/images/InOut_Color_Variable_Out.xpm \
     gui/images/InOut_Disabled_Fixed_In.xpm gui/images/InOut_Disabled_Fixed_Out.xpm \
     gui/images/InOut_Disabled_RemoveColor_In.xpm gui/images/InOut_Disabled_RemoveColor_Out.xpm \
     gui/images/InOut_Disabled_Variable_In.xpm gui/images/InOut_Disabled_Variable_Out.xpm \
     gui/images/InOut_Monochrome_Fixed_In.xpm gui/images/InOut_Monochrome_Fixed_Out.xpm \
     gui/images/InOut_Monochrome_RemoveColor_In.xpm gui/images/InOut_Monochrome_RemoveColor_Out.xpm \
     gui/images/InOut_Monochrome_Variable_In.xpm gui/images/InOut_Monochrome_Variable_Out.xpm \
     gui/images/Grid_Color_Center.xpm gui/images/Grid_Color_Minimize.xpm gui/images/Grid_Color_Origin.xpm \
     gui/images/Grid_Disabled_Center.xpm gui/images/Grid_Disabled_Minimize.xpm gui/images/Grid_Disabled_Origin.xpm \
     gui/images/Grid_Monochrome_Center.xpm gui/images/Grid_Monochrome_Minimize.xpm gui/images/Grid_Monochrome_Origin.xpm \
     gui/images/Rescale_Color_X1.xpm gui/images/Rescale_Color_X2.xpm gui/images/Rescale_Color_X3.xpm \
     gui/images/Rescale_Disabled_X1.xpm gui/images/Rescale_Disabled_X2.xpm gui/images/Rescale_Disabled_X3.xpm \
     gui/images/Rescale_Monochrome_X1.xpm gui/images/Rescale_Monochrome_X2.xpm gui/images/Rescale_Monochrome_X3.xpm



