import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// C06 Display menu: Show, Projection, Voxel colour. It stays open while
// options are toggled; every option persists. The layer slab and dimming
// rows need the voxel editor on screen and are disabled without it.
//
// Wireframe (each number marks its code below):
//
//   ┌─────────────────────────────┐
//   │ ① SHOW                      │   Entities only
//   │   ✓ Axes                    │
//   │   ✓ Grid boundary           │
//   │     Active layer slab       │   these two need the
//   │     Dim other layers        │   Voxel editor shown
//   ├─────────────────────────────┤
//   │ ② PROJECTION                │
//   │   ✓ Perspective             │
//   │     Orthographic            │
//   ├─────────────────────────────┤
//   │ ③ VOXEL COLOUR              │
//   │   ✓ Piece colour            │
//   │     Voxel colour            │
//   └─────────────────────────────┘
Popup {
    id: root
    objectName: "entities.viewport.display.menu"
    padding: 6
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

    // only Entities has the Show items (C18, C21)
    property bool showItems: App.layout.workspace === LayoutController.Entities

    onAboutToShow: if (parent && parent.ApplicationWindow.window) parent.ApplicationWindow.window.openPopups++
    onAboutToHide: if (parent && parent.ApplicationWindow.window) parent.ApplicationWindow.window.openPopups--

    background: Rectangle {
        color: Theme.panel
        border.color: Theme.line2
        radius: Theme.radiusPopup
    }

    component Section: Text {
        leftPadding: 10
        topPadding: 6
        bottomPadding: 4
        color: Theme.muted
        font.pixelSize: Theme.fontMicro
        font.weight: Font.DemiBold
        font.letterSpacing: 0.5
    }
    component Divider: Rectangle {
        width: parent ? parent.width : 0
        height: 1
        color: Theme.line
    }

    contentItem: Column {
        spacing: 0
        width: 240

        Section { visible: root.showItems; text: qsTr("SHOW") } // ①
        CheckRow {
            objectName: "entities.viewport.display.axes"
            visible: root.showItems
            width: parent.width
            text: qsTr("Axes")
            selected: App.viewport.displayAxes
            onClicked: App.viewport.displayAxes = !App.viewport.displayAxes
        }
        CheckRow {
            objectName: "entities.viewport.display.bounds"
            visible: root.showItems
            width: parent.width
            text: qsTr("Grid boundary")
            selected: App.viewport.displayBounds
            onClicked: App.viewport.displayBounds = !App.viewport.displayBounds
        }
        CheckRow {
            objectName: "entities.viewport.display.layer"
            visible: root.showItems
            width: parent.width
            text: qsTr("Active layer slab")
            selected: App.viewport.displayLayerSlab
            enabled: App.viewport.editorVisible
            hint: enabled ? "" : qsTr("needs Voxel editor")
            onClicked: App.viewport.displayLayerSlab = !App.viewport.displayLayerSlab
        }
        CheckRow {
            objectName: "entities.viewport.display.dim"
            visible: root.showItems
            width: parent.width
            text: qsTr("Dim other layers")
            selected: App.viewport.displayDimOtherLayers
            enabled: App.viewport.editorVisible
            hint: enabled ? "" : qsTr("needs Voxel editor")
            onClicked: App.viewport.displayDimOtherLayers = !App.viewport.displayDimOtherLayers
        }
        Divider { visible: root.showItems }

        Section { text: qsTr("PROJECTION") } // ②
        CheckRow {
            objectName: "entities.viewport.display.persp"
            width: parent.width
            text: qsTr("Perspective")
            selected: App.viewport.projection === "perspective"
            onClicked: App.viewport.projection = "perspective"
        }
        CheckRow {
            objectName: "entities.viewport.display.ortho"
            width: parent.width
            text: qsTr("Orthographic")
            selected: App.viewport.projection === "orthographic"
            onClicked: App.viewport.projection = "orthographic"
        }
        Divider {}

        Section { text: qsTr("VOXEL COLOUR") } // ③
        CheckRow {
            objectName: "entities.viewport.display.colourPiece"
            width: parent.width
            text: qsTr("Piece colour")
            selected: App.viewport.colourView === "piece"
            onClicked: App.viewport.colourView = "piece"
        }
        CheckRow {
            objectName: "entities.viewport.display.colourVoxel"
            width: parent.width
            text: qsTr("Voxel colour")
            selected: App.viewport.colourView === "voxel"
            onClicked: App.viewport.colourView = "voxel"
        }
    }
}
