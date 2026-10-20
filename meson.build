project('burrtools', 'cpp', 'c',
	version: run_command('sh', '-c', 
	                     'git describe --tags --always --dirty 2>/dev/null || echo 0.8.0-unknown',
	                     check: false).stdout().strip(),
	
	default_options : [
	  'c_std=c11',
	  'build.c_std=c11',
	  'cpp_std=c++20',
	  'build.cpp_std=c++20',
	  'optimization=2',
	  'debug=true',
	  'werror=false',
	  # -Wall -Wextra for our own code.
	  'warning_level=2',
	  # ...but not for FLTK. cmake.subproject() does not hand the build off to
	  # CMake; it translates the CMake project into meson targets, which are then
	  # compiled by meson under our options. Without this override, FLTK's own
	  # sources build at -Wextra and bury our diagnostics under ~1200 lines of
	  # third-party noise. Catch2 is clean at level 2 and needs no override.
	  'fltk:warning_level=0',
	]
       )

# Define WIN32 for Windows builds (FLTK expects this)
if host_machine.system() == 'windows'
  add_project_arguments('-DWIN32', language: ['c', 'cpp'])
endif

# Set macOS deployment target: the oldest macOS the legacy binaries (burrtools,
# burrTxt) and the libraries run on, 11.0 (Big Sur) by default. burrtools-qt
# targets a later one, qt_macos_deployment_target (below, beside Qt).
#
# These are global rather than project arguments deliberately: meson subprojects
# (catch2, libpng on macOS/Windows, and zlib on Windows) do not inherit add_project_arguments, so
# with those their objects get built for the host's macOS version and linked into
# a binary declaring an older minimum. The CMake subproject (fltk) inherits
# neither and is handled separately, where it is configured below.
#
macos_deployment_target = get_option('macos_deployment_target')

if host_machine.system() == 'darwin'
  add_global_arguments('-mmacosx-version-min=' + macos_deployment_target, language: ['c', 'cpp', 'objcpp'])
  add_global_link_arguments('-mmacosx-version-min=' + macos_deployment_target, language: ['c', 'cpp', 'objcpp'])
endif

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
  'src/lib/simd_exact_cover.cpp',
  'src/lib/simd_huang_cover.cpp',
  'src/lib/assembly.cpp',
  'src/lib/bt_assert.cpp',
  'src/lib/burrgrower.cpp',
  'src/lib/converter.cpp',
  'src/lib/disassembler_0.cpp',
  'src/lib/disassembler_a.cpp',
  'src/lib/disassemblerhashes.cpp',
  'src/lib/disassemblernode.cpp',
  'src/lib/disassemblerpool.cpp',
  'src/lib/disassembly.cpp',
  'src/lib/cubemesh.cpp',
  'src/lib/cubepoly.cpp',
  'src/lib/minkmesh.cpp',
  'src/lib/disasmtomoves.cpp',
  'src/lib/gridtype.cpp',
  'src/lib/grouping.cpp',
  'src/lib/millable.cpp',
  'src/lib/movementanalysator.cpp',
  'src/lib/movementcache.cpp',
  'src/lib/movementcache_0.cpp',
  'src/lib/movementcache_1.cpp',
  'src/lib/print.cpp',
  'src/lib/progressmodel.cpp',
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
  'src/lib/voxel.cpp',
  'src/lib/voxel_0.cpp',
  'src/lib/voxel_1.cpp',
  'src/lib/voxel_2.cpp',
  'src/lib/voxel_2_mesh.cpp',
  'src/lib/voxel_3.cpp',
  'src/lib/voxel_4.cpp',
  'src/lib/voxeltable.cpp',
]

gui_src = [
  'src/tools/homedir.cpp',
  
  'src/gui/arcball.cpp',
  'src/gui/assertwindow.cpp',
  'src/gui/assmimportwindow.cpp',
  'src/gui/bulkrangewindow.cpp',
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
  'src/gui/mainmenu.cpp',
  'src/gui/mainwindow.cpp',
  'src/gui/movementbrowser.cpp',
  'src/gui/multilinewindow.cpp',
  'src/gui/piececolor.cpp',
  'src/gui/placementbrowser.cpp',
  'src/gui/platform.cpp',
  'src/gui/resultviewer.cpp',
  'src/gui/separator.cpp',
  'src/gui/solveprogresscache.cpp',
  'src/gui/statusline.cpp',
  'src/gui/statuswindow.cpp',
  'src/gui/filechooser.cpp',
  'src/gui/stlexport.cpp',
  'src/gui/stlexportsolution.cpp',
  'src/gui/togglebutton.cpp',
  'src/gui/tooltabs.cpp',
  'src/gui/vectorexportwindow.cpp',
  'src/gui/view3dgroup.cpp',
  'src/gui/viewcube.cpp',
  'src/gui/voxelframe.cpp',
  'src/gui/voxeleditgroup.cpp',
  'src/gui/BlockList.cpp',
  'src/gui/Fl_Table.cpp',
  'src/gui/LFl_Tile.cpp',
  'src/gui/Layouter.cpp',
  'src/gui/Images.cpp',
  # the undo history, shared with the Qt GUI (burr_uicore)
  'src/uicore/puzzlehistory.cpp',
  # the piece colours, which src/gui/piececolor.cpp wraps
  'src/uicore/palette.cpp',
]

# Vendored third-party C sources, built separately so that their warnings can be
# relaxed without relaxing them for our own code. gl2ps.c in particular warns
# under -Wimplicit-const-int-float-conversion at a spot the upstream author
# already marked with a FIXME; patching vendored source would only be undone by
# the next import.
#   tr.c    - Brian Paul's tile rendering library
#   gl2ps.c - GL2PS, (C) 1999-2009 C. Geuzaine
vendored_gui_src = [
  'src/gui/tr.c',
  'src/gui/gl2ps.c',
]

cc = meson.get_compiler('c')
cxx = meson.get_compiler('cpp')

# macOS specific dependencies
if host_machine.system() == 'darwin'
  cocoa_dep = dependency('appleframeworks', modules : ['Cocoa', 'ScreenCaptureKit', 'UniformTypeIdentifiers'])
else
  cocoa_dep = dependency('', required: false)
endif

# Header-only; marked as a system include so its internals never trip
# -Werror in the release jobs.
json_dep = dependency('nlohmann_json', include_type: 'system')

# The update check's HTTPS backend; see src/tools/httpget.h.
if host_machine.system() == 'darwin'
  add_languages('objcpp', native: false)
  add_project_arguments('-std=c++20', language: 'objcpp')
  httpget_src = ['src/tools/httpget_mac.mm']
  httpget_dep = dependency('appleframeworks', modules: ['Foundation'])
elif host_machine.system() == 'windows'
  httpget_src = ['src/tools/httpget_win.cpp']
  httpget_dep = cxx.find_library('winhttp')
else
  # Needs no libcurl headers and links no libcurl: the constants come from
  # curl_abi.h and the library is dlopened at runtime. Deliberately not
  # conditional on libcurl being installed -- making the backend depend on
  # the build host's headers would mean developers without them silently
  # compile a different (stub) backend and never build this file at all.
  # Where libcurl is missing at runtime, httpGet() reports Unsupported.
  httpget_src = ['src/tools/httpget_curl.cpp']
  httpget_dep = dependency('dl')
endif
# the same as files, for subdirectories (burrtools-qt's update check)
httpget_files = files(httpget_src)
updatecheck_files = files('src/tools/updatecheck.cpp')

if host_machine.system() == 'windows'
  wm = import('windows')
endif

thread_dep = dependency('threads')
if host_machine.system() == 'darwin'
  # On macOS, use native OpenGL framework, not Mesa
  gl_dep = dependency('appleframeworks', modules : ['OpenGL'])
  glu_dep = gl_dep  # OpenGL framework includes GLU on macOS
else
  gl_dep = dependency('gl')
endif

if host_machine.system() == 'windows'
  glu_dep = cxx.find_library('glu32')
  # glu_dep = dependency('', required: false)
  zlib_dep = subproject('zlib', default_options: ['default_library=static']).get_variable('zlib_dep')
  libpng_dep = subproject('libpng', default_options: ['default_library=static']).get_variable('libpng_dep')
elif host_machine.system() == 'darwin'
  # On macOS, glu_dep is already set to gl_dep above (OpenGL framework includes GLU)
  zlib_dep = dependency('zlib', fallback : ['zlib', 'zlib_dep'])
  libpng_dep = subproject('libpng', default_options: ['default_library=static']).get_variable('libpng_dep')
else
  zlib_dep = dependency('zlib', fallback : ['zlib', 'zlib_dep'])
  glu_dep = dependency('glu')
  libpng_dep = dependency('libpng')
endif



gdiplus_dep = dependency('', required: false)

fltk_dep = cxx.find_library('fltk', required: false)
fltkgl_dep = cxx.find_library('fltk_gl', required: false)
fltkimages_dep = cxx.find_library('fltk_images', required: false)
fltkpng_dep = dependency('', required: false)
fltkz_dep = dependency('', required: false)

if not fltk_dep.found()
  # fetch and build fltk from source
  cmake = import('cmake')
  
  # Configure CMake to skip test programs that require fluid
  cmake_opts = cmake.subproject_options()
  cmake_opts.set_override_option('werror', 'false')
  cmake_opts.add_cmake_defines({
    'FLTK_BUILD_TEST': 'OFF',
    'FLTK_BUILD_EXAMPLES': 'OFF'
  })
  
  # FLTK is a CMake subproject and so inherits neither the global nor the project
  # arguments that carry the deployment target. Without this, libfltk.a objects
  # are built for the host's macOS version and linking them into a binary that
  # declares an older minimum produces "built for newer macOS version" warnings -
  # and a binary that claims a compatibility it does not have.
  if host_machine.system() == 'darwin'
    cmake_opts.add_cmake_defines({
      'CMAKE_OSX_DEPLOYMENT_TARGET': macos_deployment_target
    })
  endif

  if host_machine.system() == 'windows'
    gdiplus_dep = cxx.find_library('gdiplus')

    # For Windows cross-compilation, disable optional dependencies that might
    # pull in system headers
    cmake_opts.add_cmake_defines({
      'FLTK_USE_SYSTEM_LIBJPEG': 'OFF',
      'FLTK_USE_SYSTEM_LIBPNG': 'OFF',
      'FLTK_USE_SYSTEM_ZLIB': 'OFF',
      'FLTK_OPTION_CAIRO': 'OFF',
      'CMAKE_FIND_ROOT_PATH_MODE_INCLUDE': 'ONLY',
      'CMAKE_FIND_ROOT_PATH_MODE_LIBRARY': 'ONLY'
    })
  endif
  
  fltk_proj = cmake.subproject('fltk', options: cmake_opts)
  fltk_dep = fltk_proj.dependency('fltk')
  fltkgl_dep = fltk_proj.dependency('fltk_gl')
  fltkimages_dep = fltk_proj.dependency('fltk_images')

  # On Windows, FLTK builds bundled PNG and zlib libraries when we disable system libraries
  # These need to be linked separately
  if host_machine.system() == 'windows'
    fltkpng_dep = fltk_proj.dependency('fltk_png')
    fltkz_dep = fltk_proj.dependency('fltk_z')
  else
    fltkpng_dep = dependency('', required: false)
    fltkz_dep = dependency('', required: false)
  endif
endif


# The legacy Windows binaries are linked fully static so they run without
# shipping any DLLs. The Qt GUI (src/qtgui) does not get these: it links Qt's
# shared libraries, and a static C++ runtime in its executable would put two
# copies of libstdc++ in one process.
legacy_link_args = []

if host_machine.system() == 'windows'
  legacy_link_args += [
    '-static'
    ,'-static-libgcc'
    ,'-static-libstdc++'
    ,'-lws2_32'
    ,'-lcomctl32'
  ]
endif

# Manifold (https://github.com/elalish/manifold): the boolean / Minkowski
# engine behind the chamfered STL export of the non-cube grids. Built from
# the wrap as a static library, without its 2D cross-section part (which
# would pull in Clipper2), its tests or its parallel backend.
cmake_mf = import('cmake')
manifold_opts = cmake_mf.subproject_options()
manifold_opts.add_cmake_defines({
  'MANIFOLD_CROSS_SECTION': 'OFF',
  'MANIFOLD_TEST': 'OFF',
  'MANIFOLD_PAR': 'OFF',
  'MANIFOLD_PYBIND': 'OFF',
  'MANIFOLD_DEBUG': 'OFF',
  'BUILD_SHARED_LIBS': 'OFF',
  'CMAKE_BUILD_TYPE': 'Release',
})
manifold_opts.set_override_option('warning_level', '0')
manifold_opts.set_override_option('werror', 'false')
manifold_proj = cmake_mf.subproject('manifold', options: manifold_opts)
manifold_dep = manifold_proj.dependency('manifold')

inc = include_directories('src')

# The version git describe produced at configure time, for the About box and
# the update check. Like the project version itself, it refreshes on
# reconfigure, not on every commit.
version_conf = configuration_data()
version_conf.set_quoted('BURRTOOLS_VERSION', meson.project_version())
version_h = configure_file(output: 'version.h', configuration: version_conf)

libburr_core = static_library('burr_core',
  tools_src + halfedge_src + libburr_src,
  include_directories: inc,
  dependencies: [thread_dep, zlib_dep, manifold_dep],
)

burrtxt_exe = executable('burrTxt', ['src/burrTxt.cpp'],
  include_directories: inc,
  link_with: libburr_core,
  link_args: legacy_link_args,
  dependencies: [thread_dep, zlib_dep, manifold_dep])

burrtxt2_exe = executable('burrTxt2', ['src/burrTxt2.cpp'],
  include_directories: inc,
  link_with: libburr_core,
  link_args: legacy_link_args,
  dependencies: [thread_dep, zlib_dep, manifold_dep])

# get_supported_arguments so these stay a no-op on compilers without the flag
vendored_gui_lib = static_library('vendored_gui', vendored_gui_src,
          override_options: ['werror=false'],
          c_args: cc.get_supported_arguments(
            # clang
            '-Wno-implicit-const-int-float-conversion',
            '-Wno-unused-parameter',
            # gcc; -Wimplicit-fallthrough is the only one of these that
            # warning_level=2 introduces, the other is raised at -Wall too
            '-Wno-implicit-fallthrough',
            '-Wno-maybe-uninitialized',
          ),
          dependencies: [zlib_dep, gl_dep, glu_dep, libpng_dep],
	  )

vendored_lua_lib = static_library('vendored_lua', lua_src,
          override_options: ['werror=false'],
	  )

executable('burrtools', gui_src + httpget_src + ['src/tools/updatecheck.cpp', 'src/gui/updatechecker.cpp', 'src/gui/updatewindow.cpp', version_h],
          include_directories: inc,
          link_with: [libburr_core, vendored_gui_lib, vendored_lua_lib],
          dependencies: [manifold_dep, thread_dep, zlib_dep, gl_dep, glu_dep, gdiplus_dep, fltk_dep, fltkgl_dep, fltkimages_dep, fltkpng_dep, fltkz_dep, libpng_dep, cocoa_dep, json_dep, httpget_dep],
          link_args: legacy_link_args,
          win_subsystem: 'windows',
	  )

# The GUI-independent parts of the user interface: camera and view-cube maths,
# mesh building and picking for the 3D view, the layout model, the command
# table, the document session and its undo history, and the settings store.
# Nothing in here may include a toolkit header -- that is what lets
# test_burrtools cover it without Qt or FLTK, and what keeps a later toolkit
# change cheap.
#
# The undo history (puzzlehistory) and the piece colours (palette) are
# shared: the legacy GUI compiles them into burrtools from here too.
libburr_uicore = static_library('burr_uicore', [
    'src/uicore/assemblyscene.cpp',
    'src/uicore/camera.cpp',
    'src/uicore/commands.cpp',
    'src/uicore/depthsort.cpp',
    'src/uicore/documentsession.cpp',
    'src/uicore/imagepages.cpp',
    'src/uicore/layoutmodel.cpp',
    'src/uicore/palette.cpp',
    'src/uicore/puzzlehistory.cpp',
    'src/uicore/puzzletools.cpp',
    'src/uicore/scenemesh.cpp',
    'src/uicore/settingsstore.cpp',
    'src/uicore/vecmath.cpp',
    'src/uicore/vectorexport.cpp',
    'src/uicore/viewcube.cpp',
    'src/uicore/windowplacement.cpp',
  ],
  include_directories: inc,
  link_with: libburr_core,
  dependencies: [thread_dep, zlib_dep],
)

uicore_test_src = [
  'test/test_ui_assemblyscene.cpp',
  'test/test_ui_camera.cpp',
  'test/test_ui_commands.cpp',
  'test/test_ui_depthsort.cpp',
  'test/test_ui_documentsession.cpp',
  'test/test_ui_layoutmodel.cpp',
  'test/test_ui_palette.cpp',
  'test/test_ui_puzzletools.cpp',
  'test/test_ui_scenemesh.cpp',
  'test/test_ui_settingsstore.cpp',
  'test/test_ui_vectorexport.cpp',
  'test/test_ui_viewcube.cpp',
  'test/test_ui_windowplacement.cpp',
  'test/alloccount.cpp',
  'test/bench_ui.cpp',
]

# On macOS always the bundled Catch2, like libpng above: a system copy (e.g.
# Homebrew's bottle) is prebuilt for the host's macOS version, and linking
# it into a binary declaring macOS 11 makes the linker warn once per object
# file, which --werror (the release build, CI's ship jobs) turns into a
# failed link. Built here, it gets the deployment target like everything else.
if host_machine.system() == 'darwin'
  catch2_dep = subproject('catch2', default_options: ['tests=false']).get_variable('catch2_with_main_dep')
else
  catch2_dep = dependency('catch2-with-main', fallback: ['catch2', 'catch2_with_main_dep'], default_options: ['tests=false'])
endif

test_burrtools = executable('test_burrtools',
  ['test/test_solver.cpp', 'test/test_cubemesh.cpp', 'test/test_manifold_smoke.cpp', 'test/test_minkmesh.cpp', 'test/test_bitfield.cpp', 'test/test_voxel.cpp', 'test/test_voxel_connect.cpp', 'test/test_symmetries.cpp', 'test/test_grouping.cpp', 'test/test_xml.cpp', 'test/test_roundtrip.cpp', 'test/test_disassembly.cpp', 'test/test_puzzle_model.cpp', 'test/test_voxeltable.cpp', 'test/test_problem_config.cpp', 'test/test_disassembly_save.cpp', 'test/test_grid_invariants.cpp', 'test/test_vector3.cpp', 'test/test_halfedge.cpp', 'test/test_modifiers.cpp', 'test/test_stl.cpp', 'test/test_spheremesh.cpp', 'test/test_assembler_config.cpp', 'test/test_simd_exact_cover.cpp', 'test/test_disassembler_pool.cpp', 'test/test_progressmodel.cpp', 'test/test_solveprogresscache.cpp', 'test/test_thread_protocol.cpp', 'src/gui/solveprogresscache.cpp',
    'test/test_puzzle_history.cpp',
    'test/test_updatecheck.cpp', 'src/tools/updatecheck.cpp',
    'test/test_curl_abi.cpp', 'test/test_envvars.cpp'] + uicore_test_src,
  include_directories: inc,
  link_with: [libburr_core, libburr_uicore],
  link_args: legacy_link_args,
  dependencies: [thread_dep, zlib_dep, catch2_dep, manifold_dep, json_dep],
)

# The suite is split so the everyday loop stays quick. Everything except the
# Minkowski random-shapes stress case runs in well under two seconds; that one
# case accounts for the large majority of the total runtime on its own, so it
# is given its own suite rather than being paid for on every run. CI runs both.
#
# The stress case is seeded (std::mt19937(12345)) and so is deterministic, but
# its MINK_STRESS environment variable scales how many shapes it builds, and CI
# runners are slower than a development machine. Meson's default 30s timeout is
# too tight for that; give the slow suite room rather than have it fail as a
# timeout when it is merely busy.
#
# The same reasoning now applies to the FAST suite, for a different reason.
# It runs in about two seconds natively, but the Windows job runs the
# cross-built binary under wine, where everything is far slower. The phase-2
# coverage work grew the suite from roughly 190 cases to 380, and that pushed
# the wine run to 30.01s against meson's default 30s timeout -- so the job
# failed as a TIMEOUT while reporting "All tests passed (22918 assertions in
# 378 test cases)" immediately above it.
#
# An explicit timeout rather than a faster suite: the cases are not slow, the
# emulation is. 300s leaves room for a loaded runner without being so large
# that a genuine hang would sit there for the full ten minutes the slow suite
# is allowed.
test('fast', test_burrtools,
  args: ['~[stress]'],
  suite: 'fast',
  timeout: 300,
  workdir: meson.project_source_root())

test('stress', test_burrtools,
  args: ['[stress]'],
  suite: 'slow',
  timeout: 600,
  workdir: meson.project_source_root())

python_opt = get_option('python')
if not python_opt.disabled()
  if meson.is_cross_build() and python_opt.auto()
    message('Skipping Python wrapper during cross-compilation with python=auto')
  else
    py = import('python').find_installation('python3', required: python_opt)
    if py.found()
    pybind11_dep = dependency('pybind11', fallback: ['pybind11', 'pybind11_dep'], required: python_opt)
    if pybind11_dep.found()
      python_src = [
        'src/python/py_shape.cpp',
        'src/python/py_solution_iterator.cpp',
        'src/python/py_problem.cpp',
        'src/python/py_puzzle.cpp',
        'src/python/module.cpp',
      ]
      py.extension_module('burrtools',
        python_src,
        include_directories: inc,
        link_with: libburr_core,
        link_args: legacy_link_args,
        dependencies: [pybind11_dep, thread_dep, zlib_dep],
      )

      # in the fast suite: it runs in a fraction of a second, and leaving it
      # out would quietly shrink what `just test` covers
      test('python_tests',
        py,
        args: ['-m', 'unittest', 'discover', '-s', meson.project_source_root() / 'test/python'],
        env: ['PYTHONPATH=' + meson.current_build_dir()],
        suite: 'fast',
        workdir: meson.project_source_root(),
      )
    endif
  endif
  endif
endif


# drives the burrTxt2 binary itself, for the behaviour that lives in main():
# which runs it refuses to start, and that every run terminates and says why.
#
# native builds only. meson runs a cross-built test binary through the cross
# file's exe_wrapper, but this test's program is the host python3 and the
# burrTxt2 it drives is just an argument, so nothing wraps it and the runner
# cannot exec a Windows .exe. The behaviour under test is platform independent
# and the native jobs cover it.
if not meson.is_cross_build()
  test('burrtxt2_cli_smoke',
    find_program('python3'),
    args: [meson.project_source_root() / 'test/cli_smoke.py', burrtxt2_exe, meson.project_source_root()],
    suite: 'fast',
    timeout: 300,
    workdir: meson.project_source_root(),
  )
  test('burrtxt_cli_smoke',
    find_program('python3'),
    args: [meson.project_source_root() / 'test/cli_smoke_burrtxt.py', burrtxt_exe, meson.project_source_root()],
    suite: 'fast',
    timeout: 300,
    workdir: meson.project_source_root(),
  )
endif

# burrtools-qt: the redesigned GUI (design-spec/, the Qt 6 Quick profile).
# It is a second, independent binary next to the FLTK burrtools; nothing in
# the legacy GUI depends on it. 'auto' builds it whenever a new enough Qt is
# installed and silently skips it otherwise, so a checkout without Qt still
# builds and tests everything else exactly as before.
#
# Qt >= 6.8: QQuickRhiItem (the 3D view's host) arrived in 6.7, and 6.8 is the
# LTS the implementation guide names as the floor. private_headers is needed
# for QRhi, which Qt ships in its semi-private include directory.
#
# qt.qml_module() needs meson >= 1.7. The project as a whole does not demand
# that -- CI installs an older meson for the legacy jobs -- so an older meson
# simply skips this GUI (or refuses, when it was asked for explicitly).
qt_gui_opt = get_option('qt_gui')
if meson.version().version_compare('<1.7')
  if qt_gui_opt.enabled()
    error('qt_gui needs meson >= 1.7, this is ' + meson.version())
  endif
  qt_gui_opt = qt_gui_opt.disable_auto_if(true)
endif

# qt_static: a static Qt, found through its qmake (point meson at it with a
# native file: [binaries] qmake = '.../qt6-static/bin/qmake6'), linked into
# one standalone burrtools-qt (src/qtgui/meson.build adds the plugins a
# static program must name). The Qt tests stay with the shared build.
qt_static = get_option('qt_static')
qt6_dep = dependency('qt6',
  modules: ['Core', 'Gui', 'Qml', 'Quick', 'QuickControls2', 'Svg'],
  private_headers: true,
  version: '>=6.8',
  required: qt_gui_opt,
  method: qt_static ? 'qmake' : 'auto',
  static: qt_static,
)

# The official Qt frameworks for macOS name themselves @rpath/..., so the Qt
# executables carry Qt's library directory as a build rpath: they then run
# from the build tree, and macdeployqt finds the frameworks to bundle.
# (Homebrew's Qt uses absolute paths and needs none; elsewhere meson or the
# environment already covers it.)
qt_build_rpath = ''
if qt6_dep.found() and host_machine.system() == 'darwin' and not qt_static
  qt_qmake = find_program('qmake6', 'qmake', required: false)
  if qt_qmake.found()
    qt_build_rpath = run_command(qt_qmake, '-query', 'QT_INSTALL_LIBS', check: false).stdout().strip()
  endif
endif

# The Qt GUI and its tests target qt_macos_deployment_target (14.0, above
# official Qt 6.11's macOS 13), compiled and linked; the legacy binaries keep
# macos_deployment_target. Homebrew's Qt bottles target the macOS they were
# built on, and linking one built for a newer macOS than the target makes ld
# warn (Xcode 26's ld does), and --werror fail: the Qt targets then target the
# newest macOS any of Qt's frameworks needs.
qt_macos_args = []
if host_machine.system() == 'darwin'
  qt_macos_target = get_option('qt_macos_deployment_target')
  if qt_macos_target.version_compare('<' + macos_deployment_target)
    qt_macos_target = macos_deployment_target
  endif
  if qt_build_rpath != ''
    qt_macos_min = run_command('sh', '-c',
      'd=$1; shift; for m; do otool -l "$d/Qt$m.framework/Qt$m"; done 2>/dev/null | sed -n "s/^ *minos //p" | sort -t. -k1,1n -k2,2n | tail -n 1',
      'sh', qt_build_rpath, 'Core', 'Gui', 'Qml', 'Quick', 'QuickControls2', 'Svg',
      check: false).stdout().strip()
    if qt_macos_min != '' and qt_macos_min.version_compare('>' + qt_macos_target)
      message('Qt is built for macOS @0@, so the Qt targets target it (qt_macos_deployment_target is @1@)'.format(qt_macos_min, qt_macos_target))
      qt_macos_target = qt_macos_min
    endif
  endif
  qt_macos_args = ['-mmacosx-version-min=' + qt_macos_target]
endif

# Homebrew's Qt: include/Qt<M> links to each framework's Headers, so meson
# takes the include/ layout and adds include/Qt<M>/<version>/Qt<M> for the
# private headers (and QRhi's) -- which do not exist: they are only in
# Qt<M>.framework/Headers/<version>/Qt<M>. Where that is the layout, add the
# framework's directories. The official Qt is found as frameworks and gets
# them from meson already.
qt_private_args = []
if host_machine.system() == 'darwin' and qt_build_rpath != ''
  fs = import('fs')
  qt_headers = run_command(qt_qmake, '-query', 'QT_INSTALL_HEADERS', check: false).stdout().strip()
  foreach m : ['Core', 'Gui', 'Qml', 'Quick']
    qt_module = 'Qt' + m
    qt_fw_private = qt_build_rpath / (qt_module + '.framework') / 'Headers' / qt6_dep.version()
    if (qt_headers != '' and fs.is_dir(qt_headers / qt_module)
        and not fs.is_dir(qt_headers / qt_module / qt6_dep.version() / qt_module)
        and fs.is_dir(qt_fw_private / qt_module))
      qt_private_args += ['-I' + qt_fw_private, '-I' + qt_fw_private / qt_module]
    endif
  endforeach
  if qt_private_args.length() > 0
    message('Qt has its private headers only in its frameworks (as Homebrew does): adding those')
  endif
endif

# what every Qt target compiles with
qt_cpp_args = qt_macos_args + qt_private_args

if qt6_dep.found()
  subdir('src/qtgui')

  # The program itself, defined here so that it lands in the build folder's
  # top level, beside burrtools and burrTxt; its parts come from
  # src/qtgui/meson.build.

  # macOS names an unbundled program after its file; Info.plist names it BurrTools
  qtgui_plist_args = []
  qtgui_plist = []
  if host_machine.system() == 'darwin'
    qtgui_plist = files('src/qtgui/Info.plist')
    qtgui_plist_args = ['-Wl,-sectcreate,__TEXT,__info_plist,' + meson.project_source_root() / 'src/qtgui/Info.plist']
  endif

  # -Dqml_debug=true: qmlprofiler and the QML debugger may attach
  # (just profile-qml, the Qt profile workflow); never in a release
  qtgui_main_args = get_option('qml_debug') ? ['-DQT_QML_DEBUG'] : []

  burrtools_qt_exe = executable('burrtools-qt', 'src/qtgui/main.cpp', qtgui_static_sources,
    include_directories: inc,
    cpp_args: qtgui_main_args + qt_cpp_args,
    link_whole: libburr_qtgui,
    link_args: qtgui_static_link_args + qt_macos_args + qtgui_plist_args,
    link_depends: qtgui_plist,
    build_rpath: qt_build_rpath,
    dependencies: [qt6_dep, thread_dep, zlib_dep, manifold_dep, qtgui_platform_deps, json_dep, httpget_dep],
    win_subsystem: 'windows',
  )

  if not qt_static
    subdir('test/qtgui')
  endif
endif
