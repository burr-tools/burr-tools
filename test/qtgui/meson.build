# Tests of the redesigned GUI. Read only when Qt was found (root meson.build).
#
#   test_qtgui      Qt Test: the controllers, theme and icon provider, no window
#   test_qtgui_qml  Qt Quick Test: the QML shell, by the spec's object ids,
#                   and the component gallery against its reference images
#                   (snapshots/<os>/), once per device-pixel ratio
#
# Both run headless (QT_QPA_PLATFORM=offscreen) from the project root, so
# example files resolve, and are part of the fast suite: `just test` covers
# the GUI whenever it is built.

qt6_test_dep = dependency('qt6',
  modules: ['Core', 'Gui', 'Qml', 'Quick', 'QuickControls2', 'Svg', 'Test', 'QuickTest'],
  private_headers: true,
  version: '>=6.8',
  required: false,
)

if qt6_test_dep.found()

  # headless; and never Qt Quick's automatic pipeline cache, which would
  # write to the user's cache folder
  qt_test_env = ['QT_QPA_PLATFORM=offscreen', 'QSG_RHI_DISABLE_DISK_CACHE=1']

  # QtTest and QtQuickTest include Qt's QtCore umbrella header, which on its
  # own trips GCC 16's -Wsfinae-incomplete (Qt 6.11). That is Qt's header,
  # not ours -- silence exactly that warning, and only for these two
  # executables, so the --werror release build is not failed by it.
  qt_test_args = cxx.get_supported_arguments('-Wno-sfinae-incomplete')

  test_qtgui = executable('test_qtgui',
    ['test_qtgui.cpp', 'test_viewport.cpp',
     qt6.compile_moc(sources: 'test_qtgui.cpp', headers: 'test_viewport.h', dependencies: qt6_test_dep)],
    include_directories: [inc, include_directories('../../src/qtgui')],
    link_whole: libburr_qtgui,
    cpp_args: qt_test_args,
    build_rpath: qt_build_rpath,
    dependencies: [qt6_test_dep, thread_dep, zlib_dep, manifold_dep],
  )

  test('qtgui', test_qtgui,
    suite: 'fast',
    env: qt_test_env,
    timeout: 120,
    workdir: meson.project_source_root())

  test_qtgui_qml = executable('test_qtgui_qml',
    ['test_qtgui_qml.cpp', 'snapshots.cpp',
     qt6.compile_moc(sources: 'test_qtgui_qml.cpp', headers: 'snapshots.h', dependencies: qt6_test_dep)],
    include_directories: [inc, include_directories('../../src/qtgui')],
    link_whole: libburr_qtgui,
    cpp_args: qt_test_args,
    build_rpath: qt_build_rpath,
    dependencies: [qt6_test_dep, thread_dep, zlib_dep, manifold_dep],
  )

  # where the reference images are, and where grabs that did not match go
  qt_snapshot_env = qt_test_env + [
    'BURRTOOLS_SNAPSHOT_DIR=' + meson.current_source_dir() / 'snapshots',
    'BURRTOOLS_SNAPSHOT_OUT=' + meson.current_build_dir() / 'snapshots',
  ]

  test('qtgui_qml', test_qtgui_qml,
    args: ['-input', meson.current_source_dir() / 'qml'],
    suite: 'fast',
    env: qt_snapshot_env,
    timeout: 180,
    workdir: meson.project_source_root())

  # the gallery again at the spec's other dp ratios (P0: 1.0 / 1.5 / 2.0)
  foreach ratio : [['150', '1.5'], ['200', '2']]
    test('qtgui_gallery_' + ratio[0], test_qtgui_qml,
      args: ['-input', meson.current_source_dir() / 'qml' / 'tst_gallery.qml'],
      suite: 'fast',
      env: qt_snapshot_env + ['QT_SCALE_FACTOR=' + ratio[1]],
      timeout: 180,
      workdir: meson.project_source_root())
  endforeach

else
  message('Qt Test / Qt Quick Test not found: the Qt GUI tests are skipped')
endif
