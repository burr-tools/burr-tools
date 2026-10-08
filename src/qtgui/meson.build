# burrtools-qt: the redesigned GUI. See the root meson.build for how it is
# enabled; this file is only read when Qt was found.

qt6 = import('qt6')

# The C++ types QML sees. They are moc'ed and registered as part of the QML
# module built in qml/meson.build.
qtgui_moc_headers = files(
  'app.h',
  'commandcontroller.h',
  'documentcontroller.h',
  'imageexportcontroller.h',
  'keyboardcues.h',
  'layoutcontroller.h',
  'popupwindowstyle.h',
  'scenecontroller.h',
  'settingscontroller.h',
  'shapesmodel.h',
  'statuscontroller.h',
  'stlexportcontroller.h',
  'theme.h',
  'toolscontroller.h',
  'viewcubeitem.h',
  'viewportcontroller.h',
  'voxelviewport.h',
)

# a static Qt built without zstd cannot read resources rcc compressed with it
qt_rcc_args = qt_static ? ['--no-zstd'] : []

subdir('qml')

# The 3D view's shaders: compiled by Qt's qsb for every graphics API QRhi
# may use (SPIR-V for Vulkan, GLSL, HLSL for Direct3D, MSL for Metal), then
# embedded as byte arrays -- meson will not hand rcc a .qrc that lists files
# made at build time.
qt_bins = []
qmake = find_program('qmake6', 'qmake', required: false)
if qmake.found()
  foreach q : ['QT_HOST_BINS', 'QT_HOST_LIBEXECS']
    r = run_command(qmake, '-query', q, check: false)
    if r.returncode() == 0
      qt_bins += r.stdout().strip()
    endif
  endforeach
endif
qsb = find_program('qsb', 'qsb6', dirs: qt_bins)
python3 = find_program('python3', 'python')

qtgui_shaders = []
foreach s : ['mesh.vert', 'mesh.frag', 'line.vert', 'line.frag', 'composite.vert', 'composite.frag']
  qtgui_shaders += custom_target(s + '.qsb',
    input: 'shaders' / s,
    output: s + '.qsb',
    command: [qsb, '--glsl', '100es,120,150', '--hlsl', '50', '--msl', '12', '-o', '@OUTPUT@', '@INPUT@'])
endforeach
qtgui_shader_src = custom_target('shaders_embed',
  input: qtgui_shaders,
  output: 'shaders_embed.cpp',
  command: [python3, files('shaders/embed_shaders.py'), '@OUTPUT@', '@INPUT@'])

qtgui_resources = qt6.compile_resources(name: 'burrtools_resources',
  extra_args: qt_rcc_args,
  sources: 'resources/resources.qrc')

# The menus' Windows 11 frame (popupwindowstyle.cpp) asks the desktop window
# manager for it.
qtgui_platform_deps = []
if host_machine.system() == 'windows'
  qtgui_platform_deps += cxx.find_library('dwmapi')
endif

# Everything except main(), so test_qtgui can link the same code. It is
# linked with link_whole: the QML module's type registration and compiled
# QML live in static initialisers that nothing references by name, and a
# plain static link would drop them.
libburr_qtgui = static_library('burr_qtgui', [
    'app.cpp',
    'commandcontroller.cpp',
    'documentcontroller.cpp',
    'iconprovider.cpp',
    'imageexportcontroller.cpp',
    'keyboardcues.cpp',
    'layoutcontroller.cpp',
    'offscreenrenderer.cpp',
    'pipelinecache.cpp',
    'popupwindowstyle.cpp',
    'scenecontroller.cpp',
    'scenerenderer.cpp',
    'selfcheck.cpp',
    'settingscontroller.cpp',
    'shapesmodel.cpp',
    'statuscontroller.cpp',
    'stlexportcontroller.cpp',
    'theme.cpp',
    'toolscontroller.cpp',
    'viewcubeitem.cpp',
    'viewportcontroller.cpp',
    'voxelviewport.cpp',
    qtgui_shader_src,
    qtgui_qmlmod,
    qtgui_resources,
  ],
  include_directories: [inc, include_directories('.')],
  cpp_args: ['-DBURRTOOLS_VERSION="' + meson.project_version() + '"'],
  link_with: [libburr_uicore, libburr_guicommon, libburr_core],
  dependencies: [qt6_dep, thread_dep, zlib_dep, manifold_dep, qtgui_platform_deps],
)

# A static Qt (-Dqt_static=true): the plugins have to be linked and named
# in the program itself; scripts/qt_static_link.py works out which, from
# the static Qt's .prl files and qmlimportscanner. Everything is linked
# statically (-static), so the program needs nothing beside it.
qtgui_static_sources = []
qtgui_static_link_args = []
if qt_static
  qmake = find_program('qmake6', 'qmake')
  qt_static_script = files('../../scripts/qt_static_link.py')
  qt_static_args = ['--qmake', qmake.full_path(), '--qml-root', meson.current_source_dir() / 'qml']
  qtgui_static_link_args = run_command(python3, qt_static_script, qt_static_args, '--link-args',
                                       check: true).stdout().strip().split('
') + ['-static']
  qtgui_static_sources += configure_file(output: 'qt_static_imports.cpp',
    command: [python3, qt_static_script, qt_static_args, '--cpp', '@OUTPUT@'])
endif

# macOS names an unbundled program after its file; Info.plist names it BurrTools
qtgui_plist_args = []
qtgui_plist = []
if host_machine.system() == 'darwin'
  qtgui_plist = files('Info.plist')
  qtgui_plist_args = ['-Wl,-sectcreate,__TEXT,__info_plist,' + meson.current_source_dir() / 'Info.plist']
endif

# -Dqml_debug=true: qmlprofiler and the QML debugger may attach
# (just profile-qml, the Qt profile workflow); never in a release
qtgui_main_args = get_option('qml_debug') ? ['-DQT_QML_DEBUG'] : []

burrtools_qt_exe = executable('burrtools-qt', 'main.cpp', qtgui_static_sources,
  include_directories: inc,
  cpp_args: qtgui_main_args,
  link_whole: libburr_qtgui,
  link_args: qtgui_static_link_args + qt_link_args + qtgui_plist_args,
  link_depends: qtgui_plist,
  build_rpath: qt_build_rpath,
  dependencies: [qt6_dep, thread_dep, zlib_dep, manifold_dep, qtgui_platform_deps],
  win_subsystem: 'windows',
)
