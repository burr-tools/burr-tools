project('burrtools', 'cpp', 'c')

lua_src = [
  'lua/lapi.c',
  'lua/lauxlib.c',
  'lua/lcode.c',
  'lua/ldebug.c',
  'lua/ldo.c',
  'lua/ldump.c',
  'lua/lfunc.c',
  'lua/lgc.c',
  'lua/llex.c',
  'lua/lmem.c',
  'lua/lobject.c',
  'lua/lopcodes.c',
  'lua/lparser.c',
  'lua/lstate.c',
  'lua/lstring.c',
  'lua/ltable.c',
  'lua/ltm.c',
  'lua/lundump.c',
  'lua/lvm.c',
  'lua/lzio.c',
  'lua/luaclass.cpp',
]

tools_src = [
  'tools/fileexists.cpp',
  'tools/gzstream.cpp',
  'tools/xml.cpp',
]

halfedge_src = [
  'halfedge/face.cpp',
  'halfedge/halfedge.cpp',
  'halfedge/modifiers.cpp',
  'halfedge/polyhedron.cpp',
  'halfedge/vector3.cpp',
  'halfedge/vertex.cpp',
  'halfedge/volume.cpp',
]

libburr_src = [
  'lib/assembler.cpp',
  'lib/assembler_0.cpp',
  'lib/assembler_1.cpp',
  'lib/assembly.cpp',
  'lib/bt_assert.cpp',
  'lib/burrgrower.cpp',
  'lib/converter.cpp',
  'lib/disassembler_0.cpp',
  'lib/disassembler_a.cpp',
  'lib/disassemblerhashes.cpp',
  'lib/disassemblernode.cpp',
  'lib/disassembly.cpp',
  'lib/disasmtomoves.cpp',
  'lib/gridtype.cpp',
  'lib/grouping.cpp',
  'lib/millable.cpp',
  'lib/movementanalysator.cpp',
  'lib/movementcache.cpp',
  'lib/movementcache_0.cpp',
  'lib/movementcache_1.cpp',
  'lib/print.cpp',
  'lib/ps3dloader.cpp',
  'lib/problem.cpp',
  'lib/puzzle.cpp',
  'lib/solution.cpp',
  'lib/solvethread.cpp',
  'lib/stl.cpp',
  'lib/stl_0.cpp',
  'lib/stl_2.cpp',
  'lib/symmetries_0.cpp',
  'lib/symmetries_1.cpp',
  'lib/symmetries_2.cpp',
  'lib/thread.cpp',
  'lib/voxel.cpp',
  'lib/voxel_0.cpp',
  'lib/voxel_1.cpp',
  'lib/voxel_2.cpp',
  'lib/voxel_2_mesh.cpp',
  'lib/voxel_3.cpp',
  'lib/voxel_4.cpp',
  'lib/voxeltable.cpp',
]

flu_src = [
  'flu/Flu_Button.cpp',
  'flu/Flu_Combo_Box.cpp',
  'flu/Flu_Combo_List.cpp',
  'flu/Flu_Combo_Tree.cpp',
  'flu/Flu_File_Chooser.cpp',
  'flu/flu_file_chooser_pixmaps.cpp',
  'flu/Flu_Label.cpp',
  'flu/flu_pixmaps.cpp',
  'flu/Flu_Return_Button.cpp',
  'flu/Flu_Separator.cpp',
  'flu/FluSimpleString.cpp',
  'flu/Flu_Tree_Browser.cpp',
  'flu/Flu_Wrap_Group.cpp',
]



gui_src = [
  'tools/homedir.cpp',
  
  'gui/arcball.cpp',
  'gui/assertwindow.cpp',
  'gui/assmimportwindow.cpp',
  'gui/blocklistgroup.cpp',
  'gui/buttongroup.cpp',
  'gui/configuration.cpp',
  'gui/constraintsgroup.cpp',
  'gui/convertwindow.cpp',
  'gui/grideditor.cpp',
  'gui/grideditor_0.cpp',
  'gui/grideditor_1.cpp',
  'gui/grideditor_2.cpp',
  'gui/grideditor_3.cpp',
  'gui/grideditor_4.cpp',
  'gui/gridtypegui.cpp',
  'gui/groupseditor.cpp',
  'gui/guigridtype.cpp',
  'gui/image.cpp',
  'gui/imageexport.cpp',
  'gui/main.cpp',
  'gui/mainwindow.cpp',
  'gui/movementbrowser.cpp',
  'gui/multilinewindow.cpp',
  'gui/piececolor.cpp',
  'gui/placementbrowser.cpp',
  'gui/resultviewer.cpp',
  'gui/separator.cpp',
  'gui/statusline.cpp',
  'gui/statuswindow.cpp',
  'gui/stlexport.cpp',
  'gui/togglebutton.cpp',
  'gui/tooltabs.cpp',
  'gui/vectorexportwindow.cpp',
  'gui/view3dgroup.cpp',
  'gui/voxelframe.cpp',
  'gui/voxeleditgroup.cpp',
  'gui/BlockList.cpp',
  'gui/Fl_Table.cpp',
  'gui/LFl_Tile.cpp',
  'gui/Layouter.cpp',
  'gui/tr.c',
  'gui/gl2ps.c',
  'gui/Images.cpp',
]

# help/helpdata.cpp: $(EXTRA_DIST) help/data2c.cpp
# 	cd help; g++ data2c.cpp -O2 -o data2c `fltk-config --cxxflags --ldflags --use-images`
# 	cd help; ./data2c helpdata.cpp ../../doc/html/*.html - ../../doc/html/*.png
# 	cd help; $(RM) data2c


# gen_src = custom_target('gen-output',
#                         input : ['somefile1.c', 'file2.c'],
#                         output : ['help/helpdata.cpp'],
#                         command : [data2c, '@INPUT@',
#                                    '--c-out', '@OUTPUT0@',
#                                    '--h-out', '@OUTPUT1@'])	
	
help_src = [
  'help/Fl_Help_Dialog.cpp',
  'help/Fl_Help_View.cpp',
  'help/flstring.cpp',
  'help/helpdata.cpp'
]

boost_dep = dependency('boost', modules: ['thread'])
gl_dep = dependency('gl')
glu_dep = dependency('glu')
zlib_dep = dependency('zlib')
libpng_dep = dependency('libpng')

cxx = meson.get_compiler('cpp')
fltk_dep = cxx.find_library('fltk')
fltkgl_dep = cxx.find_library('fltk_gl')

fltkimages_dep = cxx.find_library('fltk_images')

data2c = executable('data2c', 'help/data2c.cpp', dependencies: [fltk_dep, fltkimages_dep])



executable('burrTxt',  tools_src + halfedge_src + libburr_src + ['burrTxt.cpp'],
 	   dependencies: [boost_dep, zlib_dep])

executable('burrTxt2',  tools_src + halfedge_src + libburr_src + ['burrTxt2.cpp'],
 	   dependencies: [boost_dep, zlib_dep])

executable('burrtools', lua_src + tools_src + flu_src + gui_src + help_src + halfedge_src + libburr_src,
 	   dependencies: [boost_dep, zlib_dep, gl_dep, glu_dep, fltk_dep, fltkgl_dep, libpng_dep])
