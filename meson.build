project('burrtools', 'cpp', 'c')

lua_src = [
  'src/lua/lapi.c',
  'src/lua/lauxlib.c',
  'src/lua/lcode.c',
  'src/lua/ldebug.c',
  'src/lua/ldo.c',
  'src/lua/ldump.c',
  'src/lua/lfunc.c',
  'src/lua/lgc.c',
  'src/lua/llex.c',
  'src/lua/lmem.c',
  'src/lua/lobject.c',
  'src/lua/lopcodes.c',
  'src/lua/lparser.c',
  'src/lua/lstate.c',
  'src/lua/lstring.c',
  'src/lua/ltable.c',
  'src/lua/ltm.c',
  'src/lua/lundump.c',
  'src/lua/lvm.c',
  'src/lua/lzio.c',
  'src/lua/luaclass.cpp',
]

tools_src = [
  'src/tools/fileexists.cpp',
  'src/tools/gzstream.cpp',
  'src/tools/xml.cpp',
]

halfedge_src = [
  'src/halfedge/face.cpp',
  'src/halfedge/halfedge.cpp',
  'src/halfedge/modifiers.cpp',
  'src/halfedge/polyhedron.cpp',
  'src/halfedge/vector3.cpp',
  'src/halfedge/vertex.cpp',
  'src/halfedge/volume.cpp',
]

libburr_src = [
  'src/lib/assembler.cpp',
  'src/lib/assembler_0.cpp',
  'src/lib/assembler_1.cpp',
  'src/lib/assembly.cpp',
  'src/lib/bt_assert.cpp',
  'src/lib/burrgrower.cpp',
  'src/lib/converter.cpp',
  'src/lib/disassembler_0.cpp',
  'src/lib/disassembler_a.cpp',
  'src/lib/disassemblerhashes.cpp',
  'src/lib/disassemblernode.cpp',
  'src/lib/disassembly.cpp',
  'src/lib/disasmtomoves.cpp',
  'src/lib/gridtype.cpp',
  'src/lib/grouping.cpp',
  'src/lib/millable.cpp',
  'src/lib/movementanalysator.cpp',
  'src/lib/movementcache.cpp',
  'src/lib/movementcache_0.cpp',
  'src/lib/movementcache_1.cpp',
  'src/lib/print.cpp',
  'src/lib/ps3dloader.cpp',
  'src/lib/problem.cpp',
  'src/lib/puzzle.cpp',
  'src/lib/solution.cpp',
  'src/lib/solvethread.cpp',
  'src/lib/stl.cpp',
  'src/lib/stl_0.cpp',
  'src/lib/stl_2.cpp',
  'src/lib/symmetries_0.cpp',
  'src/lib/symmetries_1.cpp',
  'src/lib/symmetries_2.cpp',
  'src/lib/thread.cpp',
  'src/lib/voxel.cpp',
  'src/lib/voxel_0.cpp',
  'src/lib/voxel_1.cpp',
  'src/lib/voxel_2.cpp',
  'src/lib/voxel_2_mesh.cpp',
  'src/lib/voxel_3.cpp',
  'src/lib/voxel_4.cpp',
  'src/lib/voxeltable.cpp',
]

flu_src = [
  'src/flu/Flu_Button.cpp',
  'src/flu/Flu_Combo_Box.cpp',
  'src/flu/Flu_Combo_List.cpp',
  'src/flu/Flu_Combo_Tree.cpp',
  'src/flu/flu_file_chooser_pixmaps.cpp',
  'src/flu/Flu_Label.cpp',
  'src/flu/flu_pixmaps.cpp',
  'src/flu/Flu_Return_Button.cpp',
  'src/flu/Flu_Separator.cpp',
  'src/flu/FluSimpleString.cpp',
  'src/flu/Flu_Tree_Browser.cpp',
  'src/flu/Flu_Wrap_Group.cpp',
]



gui_src = [
  'src/tools/homedir.cpp',
  
  'src/gui/arcball.cpp',
  'src/gui/assertwindow.cpp',
  'src/gui/assmimportwindow.cpp',
  'src/gui/blocklistgroup.cpp',
  'src/gui/buttongroup.cpp',
  'src/gui/configuration.cpp',
  'src/gui/constraintsgroup.cpp',
  'src/gui/convertwindow.cpp',
  'src/gui/grideditor.cpp',
  'src/gui/grideditor_0.cpp',
  'src/gui/grideditor_1.cpp',
  'src/gui/grideditor_2.cpp',
  'src/gui/grideditor_3.cpp',
  'src/gui/grideditor_4.cpp',
  'src/gui/gridtypegui.cpp',
  'src/gui/groupseditor.cpp',
  'src/gui/guigridtype.cpp',
  'src/gui/image.cpp',
  'src/gui/imageexport.cpp',
  'src/gui/main.cpp',
  'src/gui/mainwindow.cpp',
  'src/gui/movementbrowser.cpp',
  'src/gui/multilinewindow.cpp',
  'src/gui/piececolor.cpp',
  'src/gui/placementbrowser.cpp',
  'src/gui/resultviewer.cpp',
  'src/gui/separator.cpp',
  'src/gui/statusline.cpp',
  'src/gui/statuswindow.cpp',
  'src/gui/stlexport.cpp',
  'src/gui/togglebutton.cpp',
  'src/gui/tooltabs.cpp',
  'src/gui/vectorexportwindow.cpp',
  'src/gui/view3dgroup.cpp',
  'src/gui/voxelframe.cpp',
  'src/gui/voxeleditgroup.cpp',
  'src/gui/BlockList.cpp',
  'src/gui/Fl_Table.cpp',
  'src/gui/LFl_Tile.cpp',
  'src/gui/Layouter.cpp',
  'src/gui/tr.c',
  'src/gui/gl2ps.c',
  'src/gui/Images.cpp',
]

# in case we later on really want to include the html help into the program again
# help/helpdata.cpp: $(EXTRA_DIST) help/data2c.cpp
# 	cd help; g++ data2c.cpp -O2 -o data2c `fltk-config --cxxflags --ldflags --use-images`
# 	cd help; ./data2c helpdata.cpp ../../doc/html/*.html - ../../doc/html/*.png
# 	cd help; $(RM) data2c

	
help_src = [
  'src/help/Fl_Help_Dialog.cpp',
  'src/help/Fl_Help_View.cpp',
  'src/help/helpdata.cpp'
]

boost_dep = dependency('boost', modules: ['thread'])
gl_dep = dependency('gl')
glu_dep = dependency('glu')
zlib_dep = dependency('zlib')
libpng_dep = dependency('libpng')

cxx = meson.get_compiler('cpp')
fltk_dep = cxx.find_library('fltk', required: false)
fltkgl_dep = cxx.find_library('fltk_gl', required: false)
fltkimages_dep = cxx.find_library('fltk_images', required: false)

if not fltk_dep.found()
  # fetch and build fltk from source
  cmake = import('cmake')
  fltk_proj = cmake.subproject('fltk')
  fltk_dep = fltk_proj.dependency('fltk')
  fltkgl_dep = fltk_proj.dependency('fltk_gl')
  fltkimages_dep = fltk_proj.dependency('fltk_images')
endif


executable('burrTxt',  tools_src + halfedge_src + libburr_src + ['src/burrTxt.cpp'],
 	   dependencies: [boost_dep, zlib_dep])

executable('burrTxt2',  tools_src + halfedge_src + libburr_src + ['src/burrTxt2.cpp'],
 	   dependencies: [boost_dep, zlib_dep])

executable('burrtools', lua_src + tools_src + flu_src + gui_src + halfedge_src + libburr_src,
 	   dependencies: [boost_dep, zlib_dep, gl_dep, glu_dep, fltk_dep, fltkgl_dep, libpng_dep])
