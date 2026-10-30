pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Window
import BurrTools.Ui

// The window: an ordinary window of the platform -- its own title bar,
// caption buttons, moving and resizing -- with the menu bar, workspace rail,
// the three cards of the active workspace, status bar, and the dialogs the
// controllers ask for.
//
// Wireframe (each number marks its code below):
//
//   ┌───────────────────────────────────────────────────────────┐
//   │ ① File  Edit  View  Export  Help                          │
//   ├────┬─────────────┬─────────────────────────┬──────────────┤
//   │ ②  │ ③           │ ④                       │ ⑤            │
//   │Ent │ Shapes      │ 3D view                 │ Voxel editor │
//   │Puz │ (Problems,  │ (ViewportCard)          │ (Selected    │
//   │Sol │  Solver)    │                         │  piece, ...) │
//   │    ├─────────────┴─────────────────────────┴──────────────┤
//   │    │ ⑥ status                                             │
//   └────┴──────────────────────────────────────────────────────┘
//   ⑦ until the first frame: this outline, no text
//   ⑧ the dialogs, each made when first opened
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
    // hidden behind the rail's menu button while View ▸ Show menu bar is off.
    // Made with the workspace, after the first frame (startContent).
    Component { // ①
        id: menuBarComponent
        AppMenuBar {
            visible: !App.commands.menuBarHideable || App.settings.showMenuBar
        }
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

    // Alt reaches the menus by keyboard (KeyboardCues): a tap arms them, an
    // access key opens its menu -- here in the menu bar; where View ▸ Show
    // menu bar hid it, in the rail's menu (WorkspaceRail)
    Connections {
        target: App.keyboardCues
        enabled: win.menuBar !== null && win.menuBar.visible
        function onMenuKeyPressed(index: int) {
            const bar = win.menuBar as MenuBar
            const item = index >= 0 && index < bar.count ? bar.itemAt(index) as MenuBarItem : null
            if (item)
                item.triggered()
        }
    }
    // a dialog is up: the dialogs are made when first opened (showDialog)
    function shown(dialog: Loader): bool { return dialog.status === Loader.Ready && (dialog.item as Dialog).visible }
    // the message dialogs (unsaved changes, messages, an internal error) too
    function messageShown(dialog: Loader): bool { return dialog.status === Loader.Ready && (dialog.item as MessageDialog).visible }
    // the colour chooser (Edit colour): while it is up nothing may renumber the colours
    function chooserShown(): bool { return colourDialog.status === Loader.Ready && (colourDialog.item as ColorDialog).visible }
    readonly property bool modalOpen: shown(newDialog) || shown(commentDialog) || shown(aboutDialog)
                                      || shown(settingsDialog) || shown(noticeDialog)
                                      || shown(convertDialog) || shown(importAssembliesDialog)
                                      || shown(statusDialog) || shown(stlDialog)
                                      || shown(vectorDialog) || shown(imageDialog)
                                      || shown(shortcutsDialog) || shown(updateDialog)
                                      || shown(deleteShapeDialog) || shown(removeColourDialog)
                                      || chooserShown()
                                      || messageShown(discardDialog) || messageShown(messageDialog)
                                      || messageShown(internalErrorDialog)
    function showDialog(dialog: Loader) {
        dialog.active = true
        const d = dialog.item as Dialog
        d.open()
    }
    function showFileDialog(dialog: Loader) {
        dialog.active = true
        const d = dialog.item as FileDialog
        d.open()
    }
    property bool quitApproved: false
    property int restoreVisibility: Window.Windowed

    Binding { target: App.layout; property: "windowWidth"; value: win.width }

    /* Collapse and expand keep the keyboard: when a side swaps its card for
     * its rail (or back), focus that was on the button that did it moves to
     * the button that undoes it -- the rail's expand, the card's collapse --
     * so Space toggles again, with a keyboard ring only if there was one. */
    property Item lastToggle: null          // the collapse / expand button focused last
    property bool lastToggleVisual: false
    onActiveFocusItemChanged: {
        const f = activeFocusItem
        if (f === null)
            return
        if (f.objectName.endsWith(".collapse") || f.objectName.endsWith(".rail.expand")) {
            lastToggle = f
            lastToggleVisual = (f as AbstractButton) !== null && (f as AbstractButton).visualFocus
        } else if (f.activeFocusOnTab) {
            lastToggle = null               // the keyboard moved on to something else
        }
    }
    function within(item: Item, slot: Item): bool {
        let p = item.parent
        while (p !== null && p !== slot)
            p = p.parent
        return p !== null
    }
    function keepToggleFocus(slot: Item) {
        const t = lastToggle
        const toggled = t !== null && !t.visible && within(t, slot)
        /* or the focus was elsewhere in the card the swap hid (a shortcut or
         * a menu collapsed it): it would stay on an item no one sees, and
         * Tab would start from there */
        const f = win.activeFocusItem
        const stranded = !toggled && f !== null && !f.visible && within(f, slot)
        if (!toggled && !stranded)
            return                          // not hidden by this swap, or the other side's
        const visual = toggled ? lastToggleVisual : false
        for (let i = 0; i < slot.children.length; i++) {
            const c = slot.children[i]
            if (!c.visible)
                continue
            const rail = c as CollapsedRail
            const card = c as Card
            const target = rail !== null ? rail.expandButton : card !== null ? card.collapseButton : null
            if (target !== null && target.visible) {
                target.forceActiveFocus(visual ? Qt.TabFocusReason : Qt.MouseFocusReason)
                return
            }
        }
    }

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
            enabled: !win.modalOpen && (!modelData.singleKey || App.settings.singleKeyShortcuts)
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

    // --- the first frame, then the workspace -------------------------------

    /* The window comes up with the workspace's outline only -- menu bar, rail,
     * cards and status bar in the theme's colours, no text -- and the
     * workspace is made once that first frame is on screen. Text waits for
     * the font's fallback list, which main.cpp builds on a thread of its own
     * meanwhile (0.15-0.7 s); the outline needs none, so the window shows
     * while the graphics device is still being made, and the workspace then
     * finds the fonts ready. */
    property bool contentStarted: false
    readonly property bool contentReady: workspace.status === Loader.Ready
    function startContent() {
        if (contentStarted)
            return
        // the puzzle named on the command line, so the workspace is made with it
        App.document.loadStartupFiles()
        contentStarted = true
        win.menuBar = menuBarComponent.createObject(win) as MenuBar
    }
    Connections {
        target: win
        enabled: !win.contentStarted
        function onFrameSwapped() { win.startContent() }
    }
    // a window that is never drawn (started minimised) gets its workspace all the same
    Timer {
        interval: 1000
        running: !win.contentStarted
        onTriggered: win.startContent()
    }

    component OutlineCard: Rectangle {
        color: Theme.panel
        radius: Theme.cardRadius
        border.color: Theme.line
        border.width: Theme.minimal ? 0 : 1
    }

    Item { // ⑦
        id: outline
        objectName: "shell.outline"
        anchors.fill: parent
        visible: !win.contentReady

        Rectangle {
            id: outlineMenuBar
            anchors { left: parent.left; right: parent.right; top: parent.top }
            readonly property bool shown: Qt.platform.os !== "osx"
                                          && (!App.commands.menuBarHideable || App.settings.showMenuBar)
            height: shown ? 30 : 0
            visible: shown
            color: Theme.panel
            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: 1
                color: Theme.line
            }
        }
        Rectangle {
            id: outlineRail
            anchors { left: parent.left; top: outlineMenuBar.bottom; bottom: parent.bottom }
            width: Theme.workspaceRail
            color: Theme.panel
            Rectangle {
                anchors { top: parent.top; bottom: parent.bottom; right: parent.right }
                width: 1
                color: Theme.line
            }
        }
        Rectangle {
            id: outlineStatus
            anchors { left: outlineRail.right; right: parent.right; bottom: parent.bottom }
            height: Theme.statusBar
            color: Theme.panel
            Rectangle {
                anchors { left: parent.left; right: parent.right; top: parent.top }
                height: 1
                color: Theme.line
            }
        }
        Item {
            anchors { left: outlineRail.right; right: parent.right; top: outlineMenuBar.bottom
                      bottom: outlineStatus.top; margins: Theme.bodyPadding }
            OutlineCard {
                id: outlineLeft
                anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                width: App.layout.leftWidth
            }
            OutlineCard {
                id: outlineRight
                anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
                width: App.layout.rightWidth
            }
            OutlineCard {
                // the 3D view's card is the canvas (ViewportCard)
                anchors { left: outlineLeft.right; leftMargin: Theme.cardGap; right: outlineRight.left
                          rightMargin: Theme.cardGap; top: parent.top; bottom: parent.bottom }
                color: Theme.canvas
            }
        }
    }

    // --- layout -----------------------------------------------------------

    Loader {
        id: workspace
        objectName: "shell.workspace"
        anchors.fill: parent
        active: win.contentStarted
        sourceComponent: Component {
            Item {
                WorkspaceRail { // ②
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
                        // the swap's bindings settle first, then the keyboard follows
                        readonly property bool leftRail: App.layout.leftShownAsRail
                        readonly property bool rightRail: App.layout.rightShownAsRail
                        onLeftRailChanged: Qt.callLater(win.keepToggleFocus, leftSlot)
                        onRightRailChanged: Qt.callLater(win.keepToggleFocus, rightSlot)

                        Item { // ③
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
                                list: [
                                    CollapsedShapes {
                                        anchors.fill: parent
                                        visible: body.ws === LayoutController.Entities
                                    }
                                ]
                            }
                        }

                        ViewportCard { // ④
                            id: centre
                            anchors { left: leftSlot.right; leftMargin: Theme.cardGap; right: rightSlot.left; rightMargin: Theme.cardGap
                                      top: parent.top; bottom: parent.bottom }
                        }

                        Item { // ⑤
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
                                // the drawing colour (C09), Default as the shape's
                                Rectangle {
                                    objectName: "entities.editor.colour.rail"
                                    visible: body.ws === LayoutController.Entities
                                    width: 28
                                    height: 28
                                    radius: 7
                                    color: App.colours.drawShown.valid ? App.colours.drawShown : Theme.panel2
                                    border.width: 1
                                    border.color: Theme.line2
                                    Accessible.role: Accessible.StaticText
                                    Accessible.name: qsTr("Drawing colour: %1").arg(App.colours.drawName)
                                    HoverHandler { id: railColourHover }
                                    BtToolTip { shown: railColourHover.hovered; text: qsTr("Drawing colour: %1").arg(App.colours.drawName) }
                                }
                            }
                        }
                    }

                    StatusBar { // ⑥
                        id: statusBar
                        objectName: "status"
                        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                    }
                }
            }
        }
    }

    // --- ⑧ dialogs ---------------------------------------------------------

    // each made when it is first opened (showDialog), not with the window --
    // by file name, so not even its type is loaded before then

    Loader { id: newDialog; active: false; source: "NewFileDialog.qml" }
    Loader { id: commentDialog; active: false; source: "CommentDialog.qml" }
    Loader { id: aboutDialog; active: false; source: "AboutDialog.qml" }
    Loader { id: settingsDialog; active: false; source: "SettingsDialog.qml" }
    Loader { id: convertDialog; active: false; source: "ConvertDialog.qml" }
    Loader { id: importAssembliesDialog; active: false; source: "ImportAssembliesDialog.qml" }
    Loader { id: statusDialog; active: false; source: "StatusDialog.qml" }
    Loader { id: stlDialog; active: false; source: "StlExportDialog.qml" }
    Loader { id: vectorDialog; active: false; source: "VectorExportDialog.qml" }
    Loader { id: imageDialog; active: false; source: "ImageExportDialog.qml" }
    Loader { id: shortcutsDialog; active: false; source: "KeyboardShortcutsDialog.qml" }
    Loader { id: updateDialog; active: false; source: "UpdateDialog.qml" }
    // questions before a change undo cannot fully take back
    Loader {
        id: deleteShapeDialog
        active: false
        sourceComponent: Component {
            ConfirmDialog {
                objectName: "entities.shapes.deleteDialog"
                onConfirmed: App.shapes.deleteShape(subject)
            }
        }
    }
    Loader {
        id: removeColourDialog
        active: false
        sourceComponent: Component {
            ConfirmDialog {
                objectName: "entities.editor.colour.removeDialog"
                onConfirmed: App.colours.removeColour(subject)
            }
        }
    }

    function closeConfirmations() {
        for (const l of [deleteShapeDialog, removeColourDialog])
            if (shown(l))
                (l.item as Dialog).close()
    }

    // the colour chooser for Edit colour (C09): the platform's own
    Loader {
        id: colourDialog
        active: false
        sourceComponent: Component {
            ColorDialog {
                objectName: "entities.editor.colour.chooser"
                onAccepted: App.colours.chooseColour(selectedColor)
                onRejected: App.colours.cancelChoose()
            }
        }
    }

    Loader {
        id: noticeDialog
        active: false
        sourceComponent: Component {
            BtDialog {
                id: notice
                objectName: "shell.notice.dialog"
                defaultButton: noticeOk
                width: 440
                property string body
                contentItem: Text {
                    text: notice.body
                    wrapMode: Text.WordWrap
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                }
                footer: Item {
                    implicitHeight: 60
                    BtButton {
                        anchors { right: parent.right; rightMargin: 20; verticalCenter: parent.verticalCenter }
                        id: noticeOk
                        text: qsTr("OK"); primary: true
                        onClicked: notice.close()
                    }
                }
                function show(title: string, text: string) { notice.title = title; notice.body = text; open() }
            }
        }
    }

    Loader {
        id: openDialog
        active: false
        sourceComponent: Component {
            FileDialog {
                title: qsTr("Open puzzle")
                fileMode: FileDialog.OpenFile
                nameFilters: [ qsTr("Puzzle (*.xmpuzzle)"), qsTr("All files (*)") ]
                currentFolder: App.document.folder
                onAccepted: App.document.openFile(selectedFile)
                onRejected: App.document.cancelFlow()
            }
        }
    }
    Loader {
        id: importDialog
        active: false
        sourceComponent: Component {
            FileDialog {
                title: qsTr("Import PuzzleSolver3D file")
                fileMode: FileDialog.OpenFile
                nameFilters: [ qsTr("PuzzleSolver3D (*.puz)"), qsTr("All files (*)") ]
                currentFolder: App.document.folder
                onAccepted: App.document.importFile(selectedFile)
                onRejected: App.document.cancelFlow()
            }
        }
    }
    Loader {
        id: saveDialog
        active: false
        sourceComponent: Component {
            FileDialog {
                title: qsTr("Save puzzle as")
                fileMode: FileDialog.SaveFile
                defaultSuffix: "xmpuzzle"
                nameFilters: [ qsTr("Puzzle (*.xmpuzzle)") ]
                currentFolder: App.document.folder
                onAccepted: App.document.saveAsFile(selectedFile)
                onRejected: App.document.cancelFlow()
            }
        }
    }

    Loader {
        id: discardDialog
        active: false
        sourceComponent: Component {
            MessageDialog {
                id: discard
                objectName: "shell.discard.dialog"
                title: "BurrTools"
                text: qsTr("The puzzle has unsaved changes.")
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
        }
    }

    // Messages wait in a queue and open one at a time, once the window is laid
    // out. A file named on the command line is loaded while the window is
    // still being created: a dialog opened then is centred on a window of no
    // size, at the screen's corner and mostly off it. And one file can raise
    // two (an unfinished search, then its comment), which legacy showed in turn.
    // They also wait for the workspace, so the first frames are the window's.
    property var pendingMessages: []
    function showMessage(title: string, text: string) {
        pendingMessages.push({ title: title, text: text })
        showNextMessage()
    }
    function showNextMessage() {
        if ((messageDialog.status === Loader.Ready && (messageDialog.item as MessageDialog).visible)
                || pendingMessages.length === 0 || win.Overlay.overlay.width <= 0 || !win.contentReady)
            return
        const m = pendingMessages.shift()
        messageDialog.active = true
        const d = messageDialog.item as MessageDialog
        d.title = m.title
        d.text = m.text
        d.open()
    }
    Connections {
        target: win.Overlay.overlay
        function onWidthChanged() { win.showNextMessage() }
    }
    // and, after the first frame, not before the workspace is in it
    onContentReadyChanged: showNextMessage()

    Loader {
        id: messageDialog
        active: false
        sourceComponent: Component {
            MessageDialog {
                id: message
                objectName: "shell.message"
                buttons: MessageDialog.Ok
                onVisibleChanged: if (!message.visible) Qt.callLater(win.showNextMessage)
            }
        }
    }

    Loader {
        id: internalErrorDialog
        active: false
        sourceComponent: Component {
            MessageDialog {
                objectName: "shell.internalError"
                title: qsTr("BurrTools — internal error")
                buttons: MessageDialog.Ok
                onAccepted: { win.quitApproved = true; Qt.quit() }
                onRejected: { win.quitApproved = true; Qt.quit() }
            }
        }
    }

    Connections {
        target: App.document
        function onConfirmDiscardRequested(action: string) {
            discardDialog.active = true
            const d = discardDialog.item as MessageDialog
            d.informativeText = qsTr("Save before you %1?").arg(action)
            d.open()
        }
        function onNewFileTypeRequested() { win.showDialog(newDialog) }
        // a question about a row or a colour id is void once those may have changed
        function onDocumentReplaced() { win.closeConfirmations() }
        function onHistoryApplied() { win.closeConfirmations() }
        function onOpenFileRequested() { win.showFileDialog(openDialog) }
        function onImportFileRequested() { win.showFileDialog(importDialog) }
        function onSaveAsRequested() { win.showFileDialog(saveDialog) }
        function onQuitApproved() { win.saveGeometry(); win.quitApproved = true; Qt.quit() }
        function onMessageRequested(title: string, text: string) { win.showMessage(title, text) }
    }

    Connections {
        target: App.commands
        function onAboutRequested() { win.showDialog(aboutDialog) }
        function onCommentRequested() { win.showDialog(commentDialog) }
        function onSettingsRequested() { win.showDialog(settingsDialog) }
        function onConvertRequested() { win.showDialog(convertDialog) }
        function onImportAssembliesRequested() { win.showDialog(importAssembliesDialog) }
        function onStatusRequested() { win.showDialog(statusDialog) }
        function onStlExportRequested() { win.showDialog(stlDialog) }
        function onVectorExportRequested() { win.showDialog(vectorDialog) }
        function onImageExportRequested() { win.showDialog(imageDialog) }
        function onShortcutsHelpRequested() { win.showDialog(shortcutsDialog) }
        function onFullScreenToggleRequested() {
            if (win.visibility === Window.FullScreen) {
                win.visibility = win.restoreVisibility
            } else {
                win.restoreVisibility = win.visibility
                win.showFullScreen()
            }
        }
    }

    // a delete that would lose solutions asks first (ShapesModel::requestDelete)
    Connections {
        target: App.shapes
        function onDeleteConfirmRequested(row: int, idText: string) {
            deleteShapeDialog.active = true
            const d = deleteShapeDialog.item as ConfirmDialog
            d.subject = row
            d.ask(qsTr("Delete %1?").arg(idText),
                  qsTr("A problem that uses %1 has solutions or a search in progress. Deleting %1 removes it from those problems and deletes their solutions that use it. Undo brings the shape back, but not the solutions.").arg(idText),
                  qsTr("Delete"))
        }
    }
    Connections {
        target: App.colours
        // the colours changed under the chooser: its answer would go to another
        function onChooserCancelled() {
            if (win.chooserShown())
                (colourDialog.item as ColorDialog).close()
        }
        function onChooserRequested(title: string, initial: color) {
            colourDialog.active = true
            const d = colourDialog.item as ColorDialog
            d.title = title
            d.selectedColor = initial
            d.open()
        }
        function onRemoveConfirmRequested(id: int, name: string) {
            removeColourDialog.active = true
            const d = removeColourDialog.item as ConfirmDialog
            d.subject = id
            d.ask(qsTr("Remove colour %1?").arg(name),
                  qsTr("A problem whose shapes use %1 has solutions or a search in progress. Removing %1 deletes all of those problems' solutions, and its voxels take the shape's colour. Undo brings the colour back, but not the solutions.").arg(name),
                  qsTr("Remove"))
        }
    }

    // Help > Check for updates and the check at launch (UpdateController)
    Connections {
        target: App.updates
        function onOffer(heading: string, notes: string, url: string, canSkip: bool) {
            updateDialog.active = true
            const d = updateDialog.item as UpdateDialog
            d.show(heading, notes, url, canSkip)
        }
        function onMessage(title: string, text: string) { win.showMessage(title, text) }
    }

    Connections {
        target: App
        function onInternalError(message: string) {
            internalErrorDialog.active = true
            const d = internalErrorDialog.item as MessageDialog
            d.text = message
            d.open()
        }
    }
}
