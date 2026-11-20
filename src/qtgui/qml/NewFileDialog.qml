pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import BurrTools.Ui

// File ▸ New: the one-time voxel type choice (C14). Driven by
// App.document.newFileGridTypes (gridType_c::offeredForNewPuzzles(), with
// Brick preselected and rhombic/tetra-octa on NG's grids plus hexagonal and
// Cairo prisms). The type is fixed for the life of the file.
//
// Wireframe (each number marks its code below):
//
//   ┌ New puzzle — select the voxel type ───┐
//   │ ① ┌───────────────────────────────┐   │
//   │   │ (•) Brick                     │   │   a RadioCard per offered grid
//   │   │     Cubes. The classic burr...│   │
//   │   └───────────────────────────────┘   │
//   │     ( ) Triangular Prism              │
//   │     ( ) Spheres ...                   │
//   ├───────────────────────────────────────┤
//   │ ②                  [Cancel] [Create]  │
//   └───────────────────────────────────────┘
BtDialog {
    id: root
    objectName: "shell.newfile.dialog"
    defaultButton: okButton
    title: qsTr("New puzzle — select the voxel type")
    width: 460

    property int selectedType: 0

    onAboutToShow: selectedType = 0
    onRejected: App.document.cancelFlow()
    onAccepted: App.document.newDocument(selectedType)

    contentItem: ColumnLayout {
        spacing: 8
        Repeater { // ①
            model: App.document.newFileGridTypes
            delegate: RadioCard {
                required property var modelData
                objectName: "shell.newfile.type." + modelData.key
                Layout.fillWidth: true
                text: modelData.name
                description: modelData.desc
                selected: root.selectedType === modelData.id
                onClicked: root.selectedType = modelData.id
                onDoubleClicked: { root.selectedType = modelData.id; root.accept() }
            }
        }
    }

    footer: Item { // ②
        implicitHeight: 60
        Row {
            anchors { right: parent.right; rightMargin: 20; verticalCenter: parent.verticalCenter }
            spacing: 8
            BtButton { objectName: "shell.newfile.cancel"; text: qsTr("Cancel"); onClicked: root.reject() }
            BtButton { id: okButton; objectName: "shell.newfile.ok"; text: qsTr("Create"); primary: true; onClicked: root.accept() }
        }
    }
}
