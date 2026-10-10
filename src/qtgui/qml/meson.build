# The QML module BurrTools.Ui: the QML files of the redesigned GUI plus the
# C++ types they use. It is declared here, next to the QML files, so they sit
# at the module root -- loadFromModule("BurrTools.Ui", "Main") finds Main.qml
# under its own name, not under a "qml/" prefix.

qtgui_qml_files = files(
  'AboutDialog.qml',
  'AppMenu.qml',
  'AppMenuBar.qml',
  'AppMenuItem.qml',
  'BtButton.qml',
  'BtCheckBox.qml',
  'BtDialog.qml',
  'BtSlider.qml',
  'BtSwitch.qml',
  'BtToolTip.qml',
  'Card.qml',
  'CheckRow.qml',
  'CollapsedRail.qml',
  'CommandMenu.qml',
  'CommentDialog.qml',
  'ConvertDialog.qml',
  'DecimalField.qml',
  'DisplayMenu.qml',
  'Dropdown.qml',
  'Gallery.qml',
  'Glyph.qml',
  'Icon.qml',
  'IconButton.qml',
  'ImageExportDialog.qml',
  'ImportAssembliesDialog.qml',
  'KeyBadge.qml',
  'KeyboardShortcutsDialog.qml',
  'Main.qml',
  'NewFileDialog.qml',
  'NumberField.qml',
  'PlaceholderCard.qml',
  'ProblemList.qml',
  'RadioCard.qml',
  'RailButton.qml',
  'SearchField.qml',
  'Segmented.qml',
  'SettingsDialog.qml',
  'ShapesCard.qml',
  'ShortcutKeys.qml',
  'StatusBar.qml',
  'StatusDialog.qml',
  'StlExportDialog.qml',
  'ToolbarButton.qml',
  'VectorExportDialog.qml',
  'ViewportCard.qml',
  'VoxelEditorCard.qml',
  'WorkspaceRail.qml',
)

# qmlcachegen runs below, not in qml_module (cachegen: false): it has to see
# the module's own C++ types, which it finds only through an import path
qtgui_qmlmod = qt6.qml_module('BurrTools.Ui',
  version: '1.0',
  qml_sources: qtgui_qml_files,
  cachegen: false,
  moc_headers: qtgui_moc_headers,
  include_directories: [inc, include_directories('..')],
  imports: ['QtQuick', 'QtQuick.Controls', 'QtQuick.Layouts', 'QtQuick.Dialogs'],
  rcc_extra_arguments: qt_rcc_args,
  dependencies: qt6_dep,
)

# The QML compiled ahead of time (qmlcachegen), as qml_module would, but
# with the module importable: meson writes its qmldir and .qmltypes flat
# here, and an `import BurrTools.Ui` cannot find them, so qmlcachegen would
# compile without knowing App, Theme or the controllers and leave every
# binding that uses them to the JavaScript engine. The module's qmldir and
# .qmltypes are laid out in import/BurrTools/Ui (scripts/qml_import_tree.py,
# the qmldir pointing at the QML sources) once the type registrar has
# written the .qmltypes, and qmlcachegen runs with -I import. The tree
# changes with the C++ types only: a QML edit compiles that file again.
#
# qml_module's results, cachegen off: the C++ headers' moc, their JSON, the
# type registrar (registrations .cpp, .qmltypes), the resources.
qtgui_qml_registrar = qtgui_qmlmod[-2]
assert(qtgui_qml_registrar.to_list()[1].full_path().endswith('.qmltypes'),
       'qt.qml_module returned its targets in another order: find the type registrar again')
qmlcachegen = find_program('qmlcachegen', dirs: qt_bins)
qtgui_qml_qrc = meson.current_build_dir() / 'BurrTools_Ui_qml.qrc'
qtgui_qml_import_root = meson.current_build_dir() / 'import'
qtgui_qml_import = custom_target('BurrTools_Ui_import',
  input: qtgui_qml_registrar[1],
  output: 'BurrTools_Ui_import.stamp',
  command: [python3, files('../../../scripts/qml_import_tree.py'),
            '--root', qtgui_qml_import_root, '--module', 'BurrTools.Ui',
            '--qmldir', meson.current_build_dir() / 'BurrTools_Ui_qmldir',
            '--qmltypes', '@INPUT@', '--sources', meson.current_source_dir(),
            '--stamp', '@OUTPUT@'])

qtgui_qmlcache = []
qtgui_qml_resource_paths = []
foreach q : qtgui_qml_files
  name = fs.name(q)
  qtgui_qmlcache += custom_target('BurrTools_Ui_' + fs.stem(name) + '.cpp',
    input: q,
    output: 'BurrTools_Ui_' + fs.stem(name) + '.cpp',
    depends: qtgui_qml_import,
    command: [qmlcachegen, '-o', '@OUTPUT@', '-I', qtgui_qml_import_root,
              '--resource', qtgui_qml_qrc, '@INPUT@'])
  qtgui_qml_resource_paths += '/qt/qml/BurrTools/Ui/' + name
endforeach
qtgui_qmlcache += custom_target('BurrTools_Ui_qmlcache_loader.cpp',
  output: 'BurrTools_Ui_qmlcache_loader.cpp',
  depend_files: qtgui_qml_files,
  command: [qmlcachegen, '-o', '@OUTPUT@', '--resource-name', 'qmlcache_BurrTools_Ui',
            '--resource=' + qtgui_qml_qrc, qtgui_qml_resource_paths])
