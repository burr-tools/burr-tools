pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// C06 3D viewport card: the canvas, the floating toolbar, the Display
// menu, the view tag, the hint, the empty-shape message and the view cube
// (C13). The canvas edits (P5): a quick tool held, or Edit mode (the Edit
// button, E) -- ViewportController picks the target, EditorController
// applies the tool. Tab reaches the canvas, which then has keys of its own
// (the command table's viewport scope) and a ring on the card's edge.
//
// Wireframe (each number marks its code below):
//
//   ┌───────────────────────────────────────────────────────┐
//   │ ②[S1] Result  ③┌Orbit│Pan│Edit│Display│Focus┐   ④┌──┐ │
//   │                └───────────┬─⑤──────────────┘    └──┘ │
//   │                    ┌───────┴──────┐              cube │
//   │                    │ Display menu │                   │
//   │                    └──────────────┘                   │
//   │        ① the 3D canvas, the whole card                │
//   │        ⑦ No shape — open a puzzle ... (when empty)    │
//   │                                                       │
//   │ ⑥ [Shift] click 1 fixed · [Alt] click 2 variable · …  │
//   │   Drag orbit · [Shift] / middle-drag pan · Wheel zoom │
//   └───────────────────────────────────────────────────────┘
Rectangle {
    id: root
    objectName: "entities.viewport"
    color: Theme.canvas
    radius: Theme.cardRadius
    border.color: Theme.line
    border.width: Theme.minimal ? 0 : 1
    clip: true

    readonly property bool entities: App.layout.workspace === LayoutController.Entities
    readonly property bool focus2d: App.layout.focus === LayoutController.Focus2d
    readonly property bool compact: focus2d || Theme.minimal

    // --- view tag (top left) ---
    Row { // ②
        objectName: "entities.viewport.tag"
        visible: root.entities && !root.focus2d && App.viewport.hasShape
        anchors { left: parent.left; top: parent.top; margins: 14 }
        spacing: 8
        Rectangle {
            width: Math.max(28, tagId.implicitWidth + 10)
            height: 22
            radius: Theme.radiusChip
            color: App.viewport.shapeColor
            Text {
                id: tagId
                anchors.centerIn: parent
                text: App.viewport.shapeId
                color: App.viewport.shapeTextColor
                font.pixelSize: Theme.fontMicro
                font.weight: Font.Bold
            }
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: App.viewport.shapeLabel
            color: Theme.muted
            font.pixelSize: Theme.fontSecondary
        }
    }

    // --- floating toolbar (top centre) ---
    Rectangle { // ③
        id: toolbar
        objectName: "entities.viewport.toolbar"
        anchors { top: parent.top; topMargin: 8; horizontalCenter: parent.horizontalCenter }
        width: buttons.implicitWidth + 6
        height: buttons.implicitHeight + 6
        radius: 10
        color: Theme.panel
        border.color: Theme.line

        component Separator: Rectangle {
            width: 1
            height: root.compact ? 20 : 28
            anchors.verticalCenter: parent ? parent.verticalCenter : undefined
            color: Theme.line
        }

        Row {
            id: buttons
            anchors.centerIn: parent
            spacing: 2

            ToolbarButton {
                objectName: "entities.viewport.toolbar.orbit"
                toggle: true
                iconName: "orbit"
                caption: qsTr("Orbit")
                tip: qsTr("Orbit: drag to rotate the view (O)")
                compact: root.compact
                on: App.viewport.navMode === "orbit"
                onClicked: App.viewport.navMode = "orbit"
            }
            ToolbarButton {
                objectName: "entities.viewport.toolbar.pan"
                toggle: true
                iconName: "pan"
                caption: qsTr("Pan")
                tip: qsTr("Pan: drag to move the view (P)")
                compact: root.compact
                on: App.viewport.navMode === "pan"
                onClicked: App.viewport.navMode = "pan"
            }
            Separator { visible: root.entities }
            ToolbarButton {
                objectName: "entities.viewport.toolbar.edit"
                visible: root.entities
                readonly property var toolNames: [qsTr("Fixed"), qsTr("Variable"), qsTr("Erase"), qsTr("Paint")]
                readonly property string toolName: toolNames[App.editor.toolInEffect]
                readonly property int toolNo: App.editor.toolInEffect + 1
                toggle: true
                glyphName: ["edit-tool-fixed", "edit-tool-variable", "edit-tool-erase",
                            "edit-tool-paint"][App.editor.toolInEffect]
                caption: qsTr("Edit")
                badge: String(toolNo)
                // C06 Edit button: on in Edit mode; override while a quick tool is held
                on: App.editor.editMode
                override: App.editor.quickHeld3d
                tip: override ? qsTr("Quick tool — %1 (%2).").arg(toolName).arg(toolNo)
                   : App.editor.editMode ? qsTr("Click edits with the active tool. Press E to leave edit mode.")
                   : qsTr("3D edit mode — %1 (%2). Press E to turn edit mode on; or hold Shift/Alt/Ctrl while clicking.").arg(toolName).arg(toolNo)
                compact: root.compact
                onClicked: App.editor.toggleEditMode()
            }
            Separator {}
            ToolbarButton {
                id: displayButton
                objectName: "entities.viewport.toolbar.display"
                toggle: true
                iconName: "eye"
                caption: qsTr("Display")
                tip: qsTr("Display options")
                compact: root.compact
                on: displayMenu.visible
                onClicked: displayMenu.visible ? displayMenu.close() : displayMenu.open()

                DisplayMenu { // ⑤
                    id: displayMenu
                    y: displayButton.height + 8
                    x: displayButton.width / 2 - width / 2
                }
            }
            Separator {}
            ToolbarButton {
                objectName: "entities.viewport.toolbar.focus"
                toggle: true
                iconName: App.layout.focus === LayoutController.Focus3d ? "unfocus" : "focus"
                caption: App.layout.focus === LayoutController.Focus3d ? qsTr("Exit") : qsTr("Focus")
                tip: qsTr("Focus the 3D view (Ctrl+Shift+F, Esc to exit)")
                compact: root.compact
                on: App.layout.focus === LayoutController.Focus3d
                onClicked: App.layout.toggleFocus3d()
            }
        }
    }

    VoxelViewport { // ①
        id: canvas
        objectName: "entities.viewport.surface"
        anchors.fill: parent
        // after the toolbar in Tab order (the spec's: toolbar, then canvas), drawn below everything
        z: -1
        controller: App.viewport
        // to assistive tech: what it shows, and the gestures the hint line (hidden
        // in Minimal and Focus 2D) explains
        Accessible.role: Accessible.Graphic
        Accessible.name: App.viewport.hasShape ? qsTr("3D view of %1 %2").arg(App.viewport.shapeId).arg(App.viewport.shapeLabel) : qsTr("3D view")
        Accessible.description: App.viewport.navMode === "orbit"
                                ? qsTr("Drag to orbit, Shift-drag or middle-drag to pan, wheel to zoom. With the view focused: arrows orbit, Shift+arrows pan, + and − zoom. O orbit mode, P pan mode, Home home view, F fit.")
                                : qsTr("Drag to pan, Shift-drag to orbit, middle-drag to pan, wheel to zoom. With the view focused: arrows orbit, Shift+arrows pan, + and − zoom. O orbit mode, P pan mode, Home home view, F fit.")
        // Tab reaches it (a click does not take the focus: C20); its keys are
        // the command table's viewport scope (Help ▸ Keyboard shortcuts)
        activeFocusOnTab: true
        Keys.onShortcutOverride: (event) => {
            if (App.commands.keyInScope("viewport", event.key, event.modifiers) !== "")
                event.accepted = true
        }
        Keys.onPressed: (event) => {
            const key = App.commands.keyInScope("viewport", event.key, event.modifiers)
            if (key !== "") {
                App.commands.trigger(key)
                event.accepted = true
            }
        }
    }

    // the keyboard's focus on the canvas: a ring on the card's edge
    Rectangle {
        objectName: "entities.viewport.focusRing"
        anchors.fill: parent
        visible: canvas.activeFocus
        color: "transparent"
        radius: root.radius
        border.width: 2
        border.color: Theme.accent
        z: 10
    }

    // --- view cube (top right, C13 placement) ---
    ViewCubeItem { // ④
        id: cube
        objectName: "entities.viewport.cube"
        controller: App.viewport
        // the design's 156 x 170 plus room for the axis labels at the left
        // and bottom (ViewCube::kPadLeft / kPadBottom)
        width: implicitWidth
        height: implicitHeight
        // the visible content 16 dp from the top and right edges; below the
        // toolbar when the card is narrower than 640 dp
        x: root.width - width - 2
        y: root.width < 640 ? 76 : -11
        visible: App.viewport.showViewCube && !root.focus2d
    }

    // --- hint (bottom left) ---
    Column { // ⑥
        objectName: "entities.viewport.hint"
        visible: !root.focus2d && !Theme.minimal
        anchors { left: parent.left; bottom: parent.bottom; margins: 14 }
        spacing: 4

        component HintText: Text {
            anchors.verticalCenter: parent ? parent.verticalCenter : undefined
            color: Theme.muted
            font.pixelSize: Theme.fontSecondary
        }

        // line 1, fixed: the quick tools (Entities)
        Row {
            objectName: "entities.viewport.hint.tools"
            visible: root.entities
            spacing: 5
            KeyBadge { text: "Shift" }
            HintText { text: qsTr("click 1 fixed ·") }
            KeyBadge { text: Qt.platform.os === "osx" ? "Option" : "Alt" }
            HintText { text: qsTr("click 2 variable ·") }
            KeyBadge { text: Qt.platform.os === "osx" ? "Control" : "Ctrl" }
            HintText { text: qsTr("click 3 erase") }
        }
        // line 2 follows the navigation mode
        Row {
            spacing: 5
            HintText { text: App.viewport.navMode === "orbit" ? qsTr("Drag orbit ·") : qsTr("Drag pan ·") }
            KeyBadge { text: "Shift" }
            HintText { text: App.viewport.navMode === "orbit" ? qsTr("/ middle-drag pan · Wheel zoom") : qsTr("-drag orbit · Middle-drag pan · Wheel zoom") }
        }
    }

    // --- empty states ---
    Text { // ⑦
        objectName: "entities.viewport.empty"
        anchors.centerIn: parent
        visible: text.length > 0
        text: !root.entities ? (App.layout.workspace === LayoutController.Puzzle
                                  ? qsTr("The Puzzle scene arrives with the Puzzle phase")
                                  : qsTr("The Solver scene arrives with the Solver phase"))
             : !App.viewport.hasShape ? qsTr("No shape — open a puzzle or add a shape")
             : !App.viewport.emptyShape ? ""
             : App.viewport.editorVisible && App.viewport.displayLayerSlab
               ? qsTr("Empty shape — draw in the Voxel editor, or Shift-click the highlighted layer here")
             : qsTr("Empty shape — Shift-click here (or press E, then click) to place the first voxel at the origin")
        color: Theme.muted
        font.pixelSize: Theme.fontSecondary
    }
}
