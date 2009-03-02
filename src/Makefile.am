bin_PROGRAMS=burrTxt burrTxt2 gui/burrGui
noinst_LIBRARIES=lib/libburr.a flu/libflu.a help/libhelp.a lua/liblua.a
noinst_PROGRAMS=help/helpviewer

XML_CPP_ADD=@XML_CPP_ADD@
XML_LD_ADD=@XML_LD_ADD@
GUI_CPP_ADD=@GUI_CPP_ADD@
GUI_LD_ADD=@GUI_LD_ADD@
PTHREAD_CPP_ADD=@PTHREAD_CPP_ADD@
PTHREAD_LD_ADD=@PTHREAD_LD_ADD@

AM_CPPFLAGS=-Wall -W $(XML_CPP_ADD) $(GUI_CPP_ADD) $(PTHREAD_CPP_ADD) @LARGEFILE@
LDADD= $(GUI_LD_ADD) $(XML_LD_ADD) $(PTHREAD_LD_ADD)

lua_liblua_a_SOURCES =
lua_liblua_a_SOURCES += lua/lapi.c lua/lapi.h
lua_liblua_a_SOURCES += lua/lauxlib.c lua/lauxlib.h
lua_liblua_a_SOURCES += lua/lcode.c lua/lcode.h
lua_liblua_a_SOURCES += lua/ldebug.c lua/ldebug.h
lua_liblua_a_SOURCES += lua/ldo.c lua/ldo.h
lua_liblua_a_SOURCES += lua/ldump.c
lua_liblua_a_SOURCES += lua/lfunc.c lua/lfunc.h
lua_liblua_a_SOURCES += lua/lgc.c lua/lgc.h
lua_liblua_a_SOURCES += lua/llex.c lua/llex.h
lua_liblua_a_SOURCES += lua/lmem.c lua/lmem.h
lua_liblua_a_SOURCES += lua/lobject.c lua/lobject.h
lua_liblua_a_SOURCES += lua/lopcodes.c lua/lopcodes.h
lua_liblua_a_SOURCES += lua/lparser.c lua/lparser.h
lua_liblua_a_SOURCES += lua/lstate.c lua/lstate.h
lua_liblua_a_SOURCES += lua/lstring.c lua/lstring.h
lua_liblua_a_SOURCES += lua/ltable.c lua/ltable.h
lua_liblua_a_SOURCES += lua/ltm.c lua/ltm.h
lua_liblua_a_SOURCES += lua/lundump.c lua/lundump.h
lua_liblua_a_SOURCES += lua/lvm.c lua/lvm.h
lua_liblua_a_SOURCES += lua/lzio.c lua/lzio.h
lua_liblua_a_SOURCES += lua/luaclass.cpp lua/luaclass.h
lua_liblua_a_SOURCES += lua/llimits.h
lua_liblua_a_SOURCES += lua/luaconf.h
lua_liblua_a_SOURCES += lua/lua.h

help_helpviewer_CPPFLAGS=-DBT_EXTERNAL $(GUI_HELP_CPP_ADD) @LARGEFILE@
help_helpviewer_LDADD=flu/libflu.a $(GUI_LD_ADD) $(GUI_HELP_LD_ADD)
help_helpviewer_SOURCES =
help_helpviewer_SOURCES += help/helpviewer.cpp
help_helpviewer_SOURCES += help/Fl_Help_Dialog.cpp help/Fl_Help_Dialog.h
help_helpviewer_SOURCES += help/Fl_Help_View.cpp help/Fl_Help_View.h
help_helpviewer_SOURCES += help/flstring.cpp help/flstring.h

burrTxt_LDADD = lib/libburr.a lua/liblua.a $(LDADD)
burrTxt_SOURCES=burrTxt.cpp

burrTxt2_LDADD = lib/libburr.a lua/liblua.a $(LDADD)
burrTxt2_SOURCES=burrTxt2.cpp

lib_libburr_a_SOURCES =
lib_libburr_a_SOURCES += lib/assembler.cpp lib/assembler.h
lib_libburr_a_SOURCES += lib/assembler_0.cpp lib/assembler_0.h
lib_libburr_a_SOURCES += lib/assembler_1.cpp lib/assembler_1.h
lib_libburr_a_SOURCES += lib/assembly.cpp lib/assembly.h
lib_libburr_a_SOURCES += lib/bitfield.h
lib_libburr_a_SOURCES += lib/bt_assert.cpp lib/bt_assert.h
lib_libburr_a_SOURCES += lib/burrgrower.cpp lib/burrgrower.h
lib_libburr_a_SOURCES += lib/converter.cpp lib/converter.h
lib_libburr_a_SOURCES += lib/disassembler.h
lib_libburr_a_SOURCES += lib/disassembler_0.cpp lib/disassembler_0.h
lib_libburr_a_SOURCES += lib/disassembler_a.cpp lib/disassembler_a.h
lib_libburr_a_SOURCES += lib/disassemblerhashes.cpp lib/disassemblerhashes.h
lib_libburr_a_SOURCES += lib/disassemblernode.cpp lib/disassemblernode.h
lib_libburr_a_SOURCES += lib/disassembly.cpp lib/disassembly.h
lib_libburr_a_SOURCES += lib/disasmtomoves.cpp lib/disasmtomoves.h
lib_libburr_a_SOURCES += lib/gridtype.cpp lib/gridtype.h
lib_libburr_a_SOURCES += lib/grouping.cpp lib/grouping.h
lib_libburr_a_SOURCES += lib/gzstream.cpp lib/gzstream.h
lib_libburr_a_SOURCES += lib/millable.cpp lib/millable.h
lib_libburr_a_SOURCES += lib/movementanalysator.cpp lib/movementanalysator.h
lib_libburr_a_SOURCES += lib/movementcache.cpp lib/movementcache.h
lib_libburr_a_SOURCES += lib/movementcache_0.cpp lib/movementcache_0.h
lib_libburr_a_SOURCES += lib/movementcache_1.cpp lib/movementcache_1.h
lib_libburr_a_SOURCES += lib/print.cpp lib/print.h
lib_libburr_a_SOURCES += lib/ps3dloader.cpp lib/ps3dloader.h
lib_libburr_a_SOURCES += lib/problem.cpp lib/problem.h
lib_libburr_a_SOURCES += lib/puzzle.cpp lib/puzzle.h
lib_libburr_a_SOURCES += lib/solvethread.cpp lib/solvethread.h
lib_libburr_a_SOURCES += lib/stl.cpp lib/stl.h
lib_libburr_a_SOURCES += lib/stl_0.cpp lib/stl_0.h
lib_libburr_a_SOURCES += lib/stl_2.cpp lib/stl_2.h
lib_libburr_a_SOURCES += lib/symmetries.h
lib_libburr_a_SOURCES += lib/symmetries_0.cpp lib/symmetries_0.h
lib_libburr_a_SOURCES += lib/symmetries_1.cpp lib/symmetries_1.h
lib_libburr_a_SOURCES += lib/symmetries_2.cpp lib/symmetries_2.h
lib_libburr_a_SOURCES += lib/thread.cpp lib/thread.h
lib_libburr_a_SOURCES += lib/types.h
lib_libburr_a_SOURCES += lib/voxel.cpp lib/voxel.h
lib_libburr_a_SOURCES += lib/voxel_0.cpp lib/voxel_0.h
lib_libburr_a_SOURCES += lib/voxel_1.cpp lib/voxel_1.h
lib_libburr_a_SOURCES += lib/voxel_2.cpp lib/voxel_2.h
lib_libburr_a_SOURCES += lib/voxel_3.cpp lib/voxel_3.h
lib_libburr_a_SOURCES += lib/voxel_4.cpp lib/voxel_4.h
lib_libburr_a_SOURCES += lib/xml.cpp lib/xml.h

lib_libburr_a_SOURCES += lib/tabs_0/rotmatrix.inc lib/tabs_0/symmetries.inc lib/tabs_0/tablesizes.inc
lib_libburr_a_SOURCES += lib/tabs_1/rotmatrix.inc lib/tabs_1/symmetries.inc lib/tabs_1/tablesizes.inc
lib_libburr_a_SOURCES += lib/tabs_2/rotmatrix.inc lib/tabs_2/symmetries.inc lib/tabs_2/tablesizes.inc
lib_libburr_a_SOURCES += lib/tabs_0/symcalc.inc lib/tabs_0/transformmini.inc lib/tabs_0/transmult.inc lib/tabs_0/unifiedsym.inc
lib_libburr_a_SOURCES += lib/tabs_1/symcalc.inc lib/tabs_1/transformmini.inc lib/tabs_1/transmult.inc lib/tabs_1/unifiedsym.inc
lib_libburr_a_SOURCES += lib/tabs_2/symcalc.inc lib/tabs_2/transformmini.inc lib/tabs_2/transmult.inc lib/tabs_2/unifiedsym.inc


EXTRA_DIST=\
    help/data2c.cpp

help_libhelp_a_SOURCES=\
    help/Fl_Help_Dialog.h \
    help/Fl_Help_Dialog.cpp \
    help/Fl_Help_View.h \
    help/Fl_Help_View.cpp \
    help/flstring.cpp \
    help/flstring.h \
    help/helpdata.h

nodist_help_libhelp_a_SOURCES=\
    help/helpdata.cpp

BUILT_SOURCES=\
    help/helpdata.cpp

help/helpdata.cpp: $(EXTRA_DIST) help/data2c.cpp
	cd help; g++ data2c.cpp -O2 -o data2c `fltk-config --cxxflags --ldflags --use-images`
	cd help; ./data2c helpdata.cpp ../../doc/*.html - ../../doc/*.png
	cd help; $(RM) data2c

flu_libflu_a_SOURCES=\
    flu/Flu_Button.cpp \
    flu/Flu_Button.h \
    flu/Flu_Combo_Box.cpp \
    flu/Flu_Combo_Box.h \
    flu/Flu_Combo_List.cpp \
    flu/Flu_Combo_List.h \
    flu/Flu_Combo_Tree.cpp \
    flu/Flu_Combo_Tree.h \
    flu/Flu_Enumerations.h \
    flu/flu_export.h \
    flu/Flu_File_Chooser.cpp \
    flu/Flu_File_Chooser.h \
    flu/flu_file_chooser_pixmaps.cpp \
    flu/flu_file_chooser_pixmaps.h \
    flu/Flu_Label.h \
    flu/Flu_Label.cpp \
    flu/flu_pixmaps.cpp \
    flu/flu_pixmaps.h \
    flu/Flu_Return_Button.cpp \
    flu/Flu_Return_Button.h \
    flu/Flu_Separator.cpp \
    flu/Flu_Separator.h \
    flu/FluSimpleString.cpp \
    flu/FluSimpleString.h \
    flu/Flu_Tree_Browser.cpp \
    flu/Flu_Tree_Browser.h \
    flu/FluVectorClass.h \
    flu/Flu_Wrap_Group.cpp \
    flu/Flu_Wrap_Group.h

gui_burrGui_LDADD = lib/libburr.a help/libhelp.a flu/libflu.a lua/liblua.a $(LDADD)
gui_burrGui_SOURCES =
gui_burrGui_SOURCES += gui/arcball.cpp gui/arcball.h
gui_burrGui_SOURCES += gui/assertwindow.cpp gui/assertwindow.h
gui_burrGui_SOURCES += gui/assmimportwindow.cpp gui/assmimportwindow.h
gui_burrGui_SOURCES += gui/blocklistgroup.cpp gui/blocklistgroup.h
gui_burrGui_SOURCES += gui/buttongroup.cpp gui/buttongroup.h
gui_burrGui_SOURCES += gui/configuration.cpp gui/configuration.h
gui_burrGui_SOURCES += gui/constraintsgroup.cpp gui/constraintsgroup.h
gui_burrGui_SOURCES += gui/convertwindow.cpp gui/convertwindow.h
gui_burrGui_SOURCES += gui/grideditor.cpp gui/grideditor.h
gui_burrGui_SOURCES += gui/grideditor_0.cpp gui/grideditor_0.h
gui_burrGui_SOURCES += gui/grideditor_1.cpp gui/grideditor_1.h
gui_burrGui_SOURCES += gui/grideditor_2.cpp gui/grideditor_2.h
gui_burrGui_SOURCES += gui/grideditor_3.cpp gui/grideditor_3.h
gui_burrGui_SOURCES += gui/grideditor_4.cpp gui/grideditor_4.h
gui_burrGui_SOURCES += gui/gridtypegui.cpp gui/gridtypegui.h
gui_burrGui_SOURCES += gui/groupseditor.cpp gui/groupseditor.h
gui_burrGui_SOURCES += gui/guigridtype.cpp gui/guigridtype.h
gui_burrGui_SOURCES += gui/image.cpp gui/image.h
gui_burrGui_SOURCES += gui/imageexport.cpp gui/imageexport.h
gui_burrGui_SOURCES += gui/main.cpp
gui_burrGui_SOURCES += gui/mainwindow.cpp gui/mainwindow.h
gui_burrGui_SOURCES += gui/movementbrowser.cpp gui/movementbrowser.h
gui_burrGui_SOURCES += gui/multilinewindow.cpp gui/multilinewindow.h
gui_burrGui_SOURCES += gui/piececolor.cpp gui/piececolor.h
gui_burrGui_SOURCES += gui/placementbrowser.cpp gui/placementbrowser.h
gui_burrGui_SOURCES += gui/resultviewer.cpp gui/resultviewer.h
gui_burrGui_SOURCES += gui/separator.cpp gui/separator.h
gui_burrGui_SOURCES += gui/statusline.cpp gui/statusline.h
gui_burrGui_SOURCES += gui/statuswindow.cpp gui/statuswindow.h
gui_burrGui_SOURCES += gui/stlexport.cpp gui/stlexport.h
gui_burrGui_SOURCES += gui/togglebutton.cpp gui/togglebutton.h
gui_burrGui_SOURCES += gui/tooltabs.cpp gui/tooltabs.h
gui_burrGui_SOURCES += gui/vectorexportwindow.cpp gui/vectorexportwindow.h
gui_burrGui_SOURCES += gui/view3dgroup.cpp gui/view3dgroup.h
gui_burrGui_SOURCES += gui/voxeldrawer.cpp gui/voxeldrawer.h
gui_burrGui_SOURCES += gui/voxeldrawer_0.cpp gui/voxeldrawer_0.h
gui_burrGui_SOURCES += gui/voxeldrawer_1.cpp gui/voxeldrawer_1.h
gui_burrGui_SOURCES += gui/voxeldrawer_2.cpp gui/voxeldrawer_2.h
gui_burrGui_SOURCES += gui/voxeldrawer_3.cpp gui/voxeldrawer_3.h
gui_burrGui_SOURCES += gui/voxeldrawer_4.cpp gui/voxeldrawer_4.h
gui_burrGui_SOURCES += gui/voxeldrawer_a.cpp gui/voxeldrawer_a.h
gui_burrGui_SOURCES += gui/voxelframe.cpp gui/voxelframe.h
gui_burrGui_SOURCES += gui/voxeleditgroup.cpp gui/voxeleditgroup.h

gui_burrGui_SOURCES += gui/BlockList.cpp gui/BlockList.h
gui_burrGui_SOURCES += gui/WindowWidgets.h
gui_burrGui_SOURCES += gui/Fl_Table.cpp gui/Fl_Table.h
gui_burrGui_SOURCES += gui/LFl_Tile.cpp gui/LFl_Tile.h
gui_burrGui_SOURCES += gui/Layouter.cpp gui/Layouter.h
gui_burrGui_SOURCES += gui/tr.c gui/tr.h
gui_burrGui_SOURCES += gui/gl2ps.c gui/gl2ps.h
gui_burrGui_SOURCES += gui/Images.cpp gui/Images.h

gui_burrGui_SOURCES += gui/images/TB_Color_Brush.xpm gui/images/TB_Color_Eraser.xpm
gui_burrGui_SOURCES += gui/images/TB_Color_Mouse_Drag.xpm gui/images/TB_Color_Columns_X.xpm
gui_burrGui_SOURCES += gui/images/TB_Color_Columns_Y.xpm gui/images/TB_Color_Columns_Z.xpm
gui_burrGui_SOURCES += gui/images/TB_Color_Mouse_Rubber_Band.xpm gui/images/TB_Color_Pen_Fixed.xpm
gui_burrGui_SOURCES += gui/images/TB_Color_Pen_Variable.xpm gui/images/TB_Color_Symmetrical_X.xpm
gui_burrGui_SOURCES += gui/images/TB_Color_Symmetrical_Y.xpm gui/images/TB_Color_Symmetrical_Z.xpm
gui_burrGui_SOURCES += gui/images/TB_Disabled_Brush.xpm gui/images/TB_Disabled_Columns_X.xpm
gui_burrGui_SOURCES += gui/images/TB_Disabled_Columns_Y.xpm gui/images/TB_Disabled_Columns_Z.xpm
gui_burrGui_SOURCES += gui/images/TB_Disabled_Eraser.xpm gui/images/TB_Disabled_Mouse_Drag.xpm
gui_burrGui_SOURCES += gui/images/TB_Disabled_Mouse_Rubber_Band.xpm gui/images/TB_Disabled_Pen_Fixed.xpm
gui_burrGui_SOURCES += gui/images/TB_Disabled_Pen_Variable.xpm gui/images/TB_Disabled_Symmetrical_X.xpm
gui_burrGui_SOURCES += gui/images/TB_Disabled_Symmetrical_Y.xpm gui/images/TB_Disabled_Symmetrical_Z.xpm
gui_burrGui_SOURCES += gui/images/TB_Monochrome_Brush.xpm gui/images/TB_Monochrome_Columns_X.xpm
gui_burrGui_SOURCES += gui/images/TB_Monochrome_Columns_Y.xpm gui/images/TB_Monochrome_Columns_Z.xpm
gui_burrGui_SOURCES += gui/images/TB_Monochrome_Eraser.xpm gui/images/TB_Monochrome_Mouse_Drag.xpm
gui_burrGui_SOURCES += gui/images/TB_Monochrome_Mouse_Rubber_Band.xpm gui/images/TB_Monochrome_Pen_Fixed.xpm
gui_burrGui_SOURCES += gui/images/TB_Monochrome_Pen_Variable.xpm gui/images/TB_Monochrome_Symmetrical_X.xpm
gui_burrGui_SOURCES += gui/images/TB_Monochrome_Symmetrical_Y.xpm gui/images/TB_Monochrome_Symmetrical_Z.xpm
gui_burrGui_SOURCES += gui/images/Transform_Color_Flip_X.xpm gui/images/Transform_Color_Flip_Y.xpm gui/images/Transform_Color_Flip_Z.xpm
gui_burrGui_SOURCES += gui/images/Transform_Color_Nudge_X_Left.xpm gui/images/Transform_Color_Nudge_X_Right.xpm
gui_burrGui_SOURCES += gui/images/Transform_Color_Nudge_Y_Left.xpm gui/images/Transform_Color_Nudge_Y_Right.xpm
gui_burrGui_SOURCES += gui/images/Transform_Color_Nudge_Z_Left.xpm gui/images/Transform_Color_Nudge_Z_Right.xpm
gui_burrGui_SOURCES += gui/images/Transform_Color_Rotate_X_Left.xpm gui/images/Transform_Color_Rotate_X_Right.xpm
gui_burrGui_SOURCES += gui/images/Transform_Color_Rotate_Y_Left.xpm gui/images/Transform_Color_Rotate_Y_Right.xpm
gui_burrGui_SOURCES += gui/images/Transform_Color_Rotate_Z_Left.xpm gui/images/Transform_Color_Rotate_Z_Right.xpm
gui_burrGui_SOURCES += gui/images/Transform_Disabled_Flip_X.xpm gui/images/Transform_Disabled_Flip_Y.xpm
gui_burrGui_SOURCES += gui/images/Transform_Disabled_Flip_Z.xpm gui/images/Transform_Disabled_Nudge_X_Left.xpm
gui_burrGui_SOURCES += gui/images/Transform_Disabled_Nudge_X_Right.xpm gui/images/Transform_Disabled_Nudge_Y_Left.xpm
gui_burrGui_SOURCES += gui/images/Transform_Disabled_Nudge_Y_Right.xpm gui/images/Transform_Disabled_Nudge_Z_Left.xpm
gui_burrGui_SOURCES += gui/images/Transform_Disabled_Nudge_Z_Right.xpm gui/images/Transform_Disabled_Rotate_X_Left.xpm
gui_burrGui_SOURCES += gui/images/Transform_Disabled_Rotate_X_Right.xpm gui/images/Transform_Disabled_Rotate_Y_Left.xpm
gui_burrGui_SOURCES += gui/images/Transform_Disabled_Rotate_Y_Right.xpm gui/images/Transform_Disabled_Rotate_Z_Left.xpm
gui_burrGui_SOURCES += gui/images/Transform_Disabled_Rotate_Z_Right.xpm gui/images/Transform_Monochrome_Flip_X.xpm
gui_burrGui_SOURCES += gui/images/Transform_Monochrome_Flip_Y.xpm gui/images/Transform_Monochrome_Flip_Z.xpm
gui_burrGui_SOURCES += gui/images/Transform_Monochrome_Nudge_X_Left.xpm gui/images/Transform_Monochrome_Nudge_X_Right.xpm
gui_burrGui_SOURCES += gui/images/Transform_Monochrome_Nudge_Y_Left.xpm gui/images/Transform_Monochrome_Nudge_Y_Right.xpm
gui_burrGui_SOURCES += gui/images/Transform_Monochrome_Nudge_Z_Left.xpm gui/images/Transform_Monochrome_Nudge_Z_Right.xpm
gui_burrGui_SOURCES += gui/images/Transform_Monochrome_Rotate_X_Left.xpm gui/images/Transform_Monochrome_Rotate_X_Right.xpm
gui_burrGui_SOURCES += gui/images/Transform_Monochrome_Rotate_Y_Left.xpm gui/images/Transform_Monochrome_Rotate_Y_Right.xpm
gui_burrGui_SOURCES += gui/images/Transform_Monochrome_Rotate_Z_Left.xpm gui/images/Transform_Monochrome_Rotate_Z_Right.xpm
gui_burrGui_SOURCES += gui/images/InOut_Color_Fixed_In.xpm gui/images/InOut_Color_Fixed_Out.xpm
gui_burrGui_SOURCES += gui/images/InOut_Color_RemoveColor_In.xpm gui/images/InOut_Color_RemoveColor_Out.xpm
gui_burrGui_SOURCES += gui/images/InOut_Color_Variable_In.xpm gui/images/InOut_Color_Variable_Out.xpm
gui_burrGui_SOURCES += gui/images/InOut_Disabled_Fixed_In.xpm gui/images/InOut_Disabled_Fixed_Out.xpm
gui_burrGui_SOURCES += gui/images/InOut_Disabled_RemoveColor_In.xpm gui/images/InOut_Disabled_RemoveColor_Out.xpm
gui_burrGui_SOURCES += gui/images/InOut_Disabled_Variable_In.xpm gui/images/InOut_Disabled_Variable_Out.xpm
gui_burrGui_SOURCES += gui/images/InOut_Monochrome_Fixed_In.xpm gui/images/InOut_Monochrome_Fixed_Out.xpm
gui_burrGui_SOURCES += gui/images/InOut_Monochrome_RemoveColor_In.xpm gui/images/InOut_Monochrome_RemoveColor_Out.xpm
gui_burrGui_SOURCES += gui/images/InOut_Monochrome_Variable_In.xpm gui/images/InOut_Monochrome_Variable_Out.xpm
gui_burrGui_SOURCES += gui/images/Grid_Color_Center.xpm gui/images/Grid_Color_Minimize.xpm gui/images/Grid_Color_Origin.xpm
gui_burrGui_SOURCES += gui/images/Grid_Disabled_Center.xpm gui/images/Grid_Disabled_Minimize.xpm gui/images/Grid_Disabled_Origin.xpm
gui_burrGui_SOURCES += gui/images/Grid_Monochrome_Center.xpm gui/images/Grid_Monochrome_Minimize.xpm gui/images/Grid_Monochrome_Origin.xpm
gui_burrGui_SOURCES += gui/images/Rescale_Color_X1.xpm gui/images/Rescale_Color_X2.xpm gui/images/Rescale_Color_X3.xpm
gui_burrGui_SOURCES += gui/images/Rescale_Disabled_X1.xpm gui/images/Rescale_Disabled_X2.xpm gui/images/Rescale_Disabled_X3.xpm
gui_burrGui_SOURCES += gui/images/Rescale_Monochrome_X1.xpm gui/images/Rescale_Monochrome_X2.xpm gui/images/Rescale_Monochrome_X3.xpm
gui_burrGui_SOURCES += gui/images/ViewMode3D.xpm gui/images/ViewMode3DL.xpm gui/images/ViewModeColor.xpm gui/images/ViewModeNormal.xpm

