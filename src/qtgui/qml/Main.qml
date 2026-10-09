import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Window
import BurrTools.Ui

// The window: an ordinary window of the platform -- its own title bar,
// caption buttons, moving and resizing -- with the menu bar, workspace rail,
// the three cards of the active workspace, status bar, and the dialogs the
// controllers ask for.
ApplicationWindow {
    id: win
    objectName: "shell.window"

    width: Math.min(1600, Screen.desktopAvailableWidth * 0.92)
    height: Math.min(1000, Screen.desktopAvailableHeight * 0.92)
    minimumWidth: 960
    minimumHeight: 640
    title: App.document.windowTitle
    color: Theme.bg
    font.pixelSize: Theme.fontBody

    // what the platform does not draw itself (menus without a native
    // implementation, tooltips) takes the theme's colours
    palette {
        window: Theme.panel
        windowText: Theme.text
        base: Theme.panel
        alternateBase: Theme.panel2
        text: Theme.text
        button: Theme.panel
        buttonText: Theme.text
        highlight: Theme.accent
        highlightedText: "white"
        toolTipBase: Theme.panel
        toolTipText: Theme.text
        mid: Theme.line2
        light: Theme.panel2
        dark: Theme.line
    }

    // macOS: the system menu bar. Windows and Linux: drawn in the theme, and
    // hidden behind the rail's menu button while View ▸ Show menu bar is off
    menuBar: AppMenuBar {
        visible: !App.commands.menuBarHideable || App.settings.showMenuBar
    }
    Binding { target: App.commands; property: "blocked"; value: win.modalOpen }

    // the window comes back where the user left it (legacy kept windowpos*)
    // if that is still on a screen; else, and on the first start, it is
    // centred on the screen's free area (App.windowPlacement)
    function place() {
        const p = App.windowPlacement(win.minimumWidth, win.minimumHeight)
        win.x = p.x
        win.y = p.y
        win.width = p.width
        win.height = p.height
        return p
    }
    Component.onCompleted: {
        if (place().maximized)
            win.showMaximized()
        else
            win.show()
    }

    function saveGeometry() {
        if (win.visibility === Window.FullScreen)
            App.settings.saveWindowGeometry(0, 0, 0, 0, win.restoreVisibility === Window.Maximized)
        else
            App.settings.saveWindowGeometry(win.x, win.y, win.width, win.height, win.visibility === Window.Maximized)
    }

    // menus count themselves in here so Esc reaches them before the layout
    property int openPopups: 0
    readonly property bool modalOpen: newDialog.visible || commentDialog.visible || aboutDialog.visible
                                      || settingsDialog.visible || noticeDialog.visible
                                      || convertDialog.visible || importAssembliesDialog.visible
                                      || statusDialog.visible || stlDialog.visible
                                      || vectorDialog.visible || imageDialog.visible
                                      || shortcutsDialog.visible
    property bool quitApproved: false
    property int restoreVisibility: Window.Windowed

    Binding { target: App.layout; property: "windowWidth"; value: win.width }

    onClosing: (close) => {
        if (!win.quitApproved) {
            close.accepted = false
            App.commands.trigger("file.quit")
        }
    }

    // --- shortcuts, all from the command table ----------------------------

    Instantiator {
        objectName: "shell.shortcuts"
        model: App.commands.shortcuts()
        delegate: Shortcut {
            required property var modelData
            sequences: modelData.sequences
            context: Qt.WindowShortcut
            enabled: !win.modalOpen
            onActivated: App.commands.trigger(modelData.key)
        }
    }

    // Esc ladder: dialogs and menus close themselves first; then the layout
    Shortcut {
        objectName: "shell.escape"
        sequence: "Esc"
        context: Qt.WindowShortcut
        enabled: !win.modalOpen && win.openPopups === 0
        onActivated: App.layout.escape()
    }

    // --- layout -----------------------------------------------------------

    WorkspaceRail {
        id: rail
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
    }

    Item {
        id: mainArea
        anchors { left: rail.right; right: parent.right; top: parent.top; bottom: parent.bottom }

        Item {
            id: body
            objectName: "workspace"
            anchors { left: parent.left; right: parent.right; top: parent.top; bottom: statusBar.top; margins: Theme.bodyPadding }

            readonly property int ws: App.layout.workspace

            Item {
                id: leftSlot
                objectName: "workspace.left"
                anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                width: App.layout.leftWidth
                Behavior on width { NumberAnimation { duration: Theme.durCollapse; easing.type: Easing.InOutQuad } }

                ShapesCard {
                    anchors.fill: parent
                    visible: body.ws === LayoutController.Entities && !App.layout.leftShownAsRail
                }
                PlaceholderCard {
                    objectName: "puzzle.left"
                    anchors.fill: parent
                    visible: body.ws === LayoutController.Puzzle && !App.layout.leftShownAsRail
                    title: qsTr("Problems")
                    collapseIcon: "panelL"
                    onCollapseClicked: App.layout.leftCollapsed = true
                    note: qsTr("Problems, result and pieces arrive with the Puzzle phase.")
                }
                PlaceholderCard {
                    objectName: "solver.left"
                    anchors.fill: parent
                    visible: body.ws === LayoutController.Solver && !App.layout.leftShownAsRail
                    title: qsTr("Solver")
                    collapseIcon: "panelL"
                    onCollapseClicked: App.layout.leftCollapsed = true
                    note: qsTr("Search set-up and the run controls arrive with the Solver phase.")
                }
                CollapsedRail {
                    objectName: "workspace.left.rail"
                    anchors.fill: parent
                    visible: App.layout.leftShownAsRail
                    expandIcon: "panelL"
                    expandObjectName: "workspace.left.rail.expand"
                    title: body.ws === LayoutController.Entities ? qsTr("Shapes")
                         : body.ws === LayoutController.Puzzle ? qsTr("Problems") : qsTr("Solver")
                    onExpandClicked: App.layout.leftCollapsed = false
                }
            }

            ViewportCard {
                id: centre
                anchors { left: leftSlot.right; leftMargin: Theme.cardGap; right: rightSlot.left; rightMargin: Theme.cardGap
                          top: parent.top; bottom: parent.bottom }
            }

            Item {
                id: rightSlot
                objectName: "workspace.right"
                anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
                width: App.layout.rightWidth
                Behavior on width { NumberAnimation { duration: Theme.durCollapse; easing.type: Easing.InOutQuad } }

                VoxelEditorCard {
                    anchors.fill: parent
                    visible: body.ws === LayoutController.Entities && !App.layout.rightShownAsRail
                }
                PlaceholderCard {
                    objectName: "puzzle.right"
                    anchors.fill: parent
                    visible: body.ws === LayoutController.Puzzle && !App.layout.rightShownAsRail
                    title: qsTr("Selected piece")
                    collapseIcon: "panelR"
                    onCollapseClicked: App.layout.rightCollapsed = true
                    note: qsTr("Counts, ranges, groups and colour rules arrive with the Puzzle phase.")
                }
                PlaceholderCard {
                    objectName: "solver.right"
                    anchors.fill: parent
                    visible: body.ws === LayoutController.Solver && !App.layout.rightShownAsRail
                    title: qsTr("Solutions")
                    collapseIcon: "panelR"
                    onCollapseClicked: App.layout.rightCollapsed = true
                    note: qsTr("The solution list and the disassembly player arrive with the Solver phase.")
                }
                CollapsedRail {
                    objectName: "workspace.right.rail"
                    anchors.fill: parent
                    visible: App.layout.rightShownAsRail
                    expandIcon: "panelR"
                    expandObjectName: "workspace.right.rail.expand"
                    title: body.ws === LayoutController.Entities ? qsTr("Voxel editor")
                         : body.ws === LayoutController.Puzzle ? qsTr("Selected piece") : qsTr("Solutions")
                    onExpandClicked: App.layout.rightCollapsed = false
                }
            }
        }

        StatusBar {
            id: statusBar
            objectName: "status"
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        }
    }

    // --- dialogs ------------------------------------------------------------

    NewFileDialog { id: newDialog }
    CommentDialog { id: commentDialog }
    AboutDialog { id: aboutDialog }
    SettingsDialog { id: settingsDialog }
    ConvertDialog { id: convertDialog }
    ImportAssembliesDialog { id: importAssembliesDialog }
    StatusDialog { id: statusDialog }
    StlExportDialog { id: stlDialog }
    VectorExportDialog { id: vectorDialog }
    ImageExportDialog { id: imageDialog }
    KeyboardShortcutsDialog { id: shortcutsDialog }

    BtDialog {
        id: noticeDialog
        objectName: "shell.notice.dialog"
        width: 440
        property string body
        contentItem: Text {
            text: noticeDialog.body
            wrapMode: Text.WordWrap
            color: Theme.text
            font.pixelSize: Theme.fontBody
        }
        footer: Item {
            implicitHeight: 60
            BtButton {
                anchors { right: parent.right; rightMargin: 20; verticalCenter: parent.verticalCenter }
                text: qsTr("OK"); primary: true
                onClicked: noticeDialog.close()
            }
        }
        function show(title: string, text: string) { noticeDialog.title = title; noticeDialog.body = text; open() }
    }

    FileDialog {
        id: openDialog
        title: qsTr("Open puzzle")
        fileMode: FileDialog.OpenFile
        nameFilters: [ qsTr("Puzzle (*.xmpuzzle)"), qsTr("All files (*)") ]
        currentFolder: App.document.folder
        onAccepted: App.document.openFile(selectedFile)
        onRejected: App.document.cancelFlow()
    }
    FileDialog {
        id: importDialog
        title: qsTr("Import PuzzleSolver3D file")
        fileMode: FileDialog.OpenFile
        nameFilters: [ qsTr("PuzzleSolver3D (*.puz)"), qsTr("All files (*)") ]
        currentFolder: App.document.folder
        onAccepted: App.document.importFile(selectedFile)
        onRejected: App.document.cancelFlow()
    }
    FileDialog {
        id: saveDialog
        title: qsTr("Save puzzle as")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "xmpuzzle"
        nameFilters: [ qsTr("Puzzle (*.xmpuzzle)") ]
        currentFolder: App.document.folder
        onAccepted: App.document.saveAsFile(selectedFile)
        onRejected: App.document.cancelFlow()
    }

    MessageDialog {
        id: discardDialog
        objectName: "shell.discard.dialog"
        title: "BurrTools"
        text: qsTr("The puzzle has unsaved changes.")
        property string action
        informativeText: qsTr("Save before you %1?").arg(action)
        buttons: MessageDialog.Save | MessageDialog.Discard | MessageDialog.Cancel
        onButtonClicked: (button, role) => {
            if (button === MessageDialog.Save)
                App.document.resolveDiscard(DocumentController.Save)
            else if (button === MessageDialog.Discard)
                App.document.resolveDiscard(DocumentController.Discard)
            else
                App.document.resolveDiscard(DocumentController.Cancel)
        }
        onRejected: App.document.resolveDiscard(DocumentController.Cancel)
    }

    // Messages wait in a queue and open one at a time, once the window is laid
    // out. A file named on the command line is loaded while the window is
    // still being created: a dialog opened then is centred on a window of no
    // size, at the screen's corner and mostly off it. And one file can raise
    // two (an unfinished search, then its comment), which legacy showed in turn.
    property var pendingMessages: []
    function showMessage(title: string, text: string) {
        pendingMessages.push({ title: title, text: text })
        showNextMessage()
    }
    function showNextMessage() {
        if (messageDialog.visible || pendingMessages.length === 0 || win.Overlay.overlay.width <= 0)
            return
        const m = pendingMessages.shift()
        messageDialog.title = m.title
        messageDialog.text = m.text
        messageDialog.open()
    }
    Connections {
        target: win.Overlay.overlay
        function onWidthChanged() { win.showNextMessage() }
    }

    MessageDialog {
        id: messageDialog
        objectName: "shell.message"
        buttons: MessageDialog.Ok
        onVisibleChanged: if (!messageDialog.visible) Qt.callLater(win.showNextMessage)
    }

    MessageDialog {
        id: internalErrorDialog
        title: qsTr("BurrTools — internal error")
        buttons: MessageDialog.Ok
        onAccepted: { win.quitApproved = true; Qt.quit() }
        onRejected: { win.quitApproved = true; Qt.quit() }
    }

    Connections {
        target: App.document
        function onConfirmDiscardRequested(action: string) { discardDialog.action = action; discardDialog.open() }
        function onNewFileTypeRequested() { newDialog.open() }
        function onOpenFileRequested() { openDialog.open() }
        function onImportFileRequested() { importDialog.open() }
        function onSaveAsRequested() { saveDialog.open() }
        function onQuitApproved() { win.saveGeometry(); win.quitApproved = true; Qt.quit() }
        function onMessageRequested(title: string, text: string) { win.showMessage(title, text) }
    }

    Connections {
        target: App.commands
        function onAboutRequested() { aboutDialog.open() }
        function onCommentRequested() { commentDialog.open() }
        function onSettingsRequested() { settingsDialog.open() }
        function onConvertRequested() { convertDialog.open() }
        function onImportAssembliesRequested() { importAssembliesDialog.open() }
        function onStatusRequested() { statusDialog.open() }
        function onStlExportRequested() { stlDialog.open() }
        function onVectorExportRequested() { vectorDialog.open() }
        function onImageExportRequested() { imageDialog.open() }
        function onShortcutsHelpRequested() { shortcutsDialog.open() }
        function onFullScreenToggleRequested() {
            if (win.visibility === Window.FullScreen) {
                win.visibility = win.restoreVisibility
            } else {
                win.restoreVisibility = win.visibility
                win.showFullScreen()
            }
        }
    }

    Connections {
        target: App
        function onInternalError(message: string) { internalErrorDialog.text = message; internalErrorDialog.open() }
    }
}
