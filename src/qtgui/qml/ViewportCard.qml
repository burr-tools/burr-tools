import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// C06 3D viewport card: the canvas, the floating toolbar, the Display
// menu, the view tag, the hint, the empty-shape message and the view cube
// (C13). Editing in 3D (the Edit button, cursors, ghosts) arrives with the
// editing phase; the Edit button is shown, disabled, so the toolbar already
// has its final layout.
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

    VoxelViewport {
        id: canvas
        objectName: "entities.viewport.surface"
        anchors.fill: parent
        controller: App.viewport
    }

    // --- view tag (top left) ---
    Row {
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
                color: (App.viewport.shapeColor.r * 0.3 + App.viewport.shapeColor.g * 0.59 + App.viewport.shapeColor.b * 0.11) > 0.6 ? "black" : "white"
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
    Rectangle {
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
                iconName: "orbit"
                caption: qsTr("Orbit")
                tip: qsTr("Orbit: drag to rotate the view (O)")
                compact: root.compact
                on: App.viewport.navMode === "orbit"
                onClicked: App.viewport.navMode = "orbit"
            }
            ToolbarButton {
                objectName: "entities.viewport.toolbar.pan"
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
                glyphName: "edit-tool-fixed"
                caption: qsTr("Edit")
                badge: "1"
                tip: qsTr("3D edit mode arrives with the editing phase")
                compact: root.compact
                enabled: false
            }
            Separator {}
            ToolbarButton {
                id: displayButton
                objectName: "entities.viewport.toolbar.display"
                iconName: "eye"
                caption: qsTr("Display")
                tip: qsTr("Display options")
                compact: root.compact
                on: displayMenu.visible
                onClicked: displayMenu.visible ? displayMenu.close() : displayMenu.open()

                DisplayMenu {
                    id: displayMenu
                    y: displayButton.height + 8
                    x: displayButton.width / 2 - width / 2
                }
            }
            Separator {}
            ToolbarButton {
                objectName: "entities.viewport.toolbar.focus"
                iconName: App.layout.focus === LayoutController.Focus3d ? "unfocus" : "focus"
                caption: App.layout.focus === LayoutController.Focus3d ? qsTr("Exit") : qsTr("Focus")
                tip: qsTr("Focus the 3D view (Ctrl+Shift+F, Esc to exit)")
                compact: root.compact
                on: App.layout.focus === LayoutController.Focus3d
                onClicked: App.layout.toggleFocus3d()
            }
        }
    }

    // --- view cube (top right, C13 placement) ---
    ViewCubeItem {
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
    Row {
        objectName: "entities.viewport.hint"
        visible: !root.focus2d && !Theme.minimal
        anchors { left: parent.left; bottom: parent.bottom; margins: 14 }
        spacing: 5

        component HintText: Text {
            anchors.verticalCenter: parent ? parent.verticalCenter : undefined
            color: Theme.muted
            font.pixelSize: Theme.fontSecondary
        }

        HintText { text: App.viewport.navMode === "orbit" ? qsTr("Drag orbit ·") : qsTr("Drag pan ·") }
        KeyBadge { text: "Shift" }
        HintText { text: App.viewport.navMode === "orbit" ? qsTr("/ middle-drag pan · Wheel zoom") : qsTr("-drag orbit · Middle-drag pan · Wheel zoom") }
    }

    // --- empty states ---
    Text {
        objectName: "entities.viewport.empty"
        anchors.centerIn: parent
        visible: text.length > 0
        text: !root.entities ? (App.layout.workspace === LayoutController.Puzzle
                                  ? qsTr("The Puzzle scene arrives with the Puzzle phase")
                                  : qsTr("The Solver scene arrives with the Solver phase"))
             : !App.viewport.hasShape ? qsTr("No shape — open a puzzle or add a shape")
             : App.viewport.emptyShape ? qsTr("Empty shape — draw in the Voxel editor") : ""
        color: Theme.muted
        font.pixelSize: Theme.fontSecondary
    }
}
