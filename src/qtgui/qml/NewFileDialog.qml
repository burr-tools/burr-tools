pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import BurrTools.Ui

// File ▸ New: the one-time voxel type choice (C14). The five official types
// in the legacy "Select space grid" order, Brick preselected. The type is
// fixed for the life of the file.
//
// Wireframe (each number marks its code below):
//
//   ┌ New puzzle — select the voxel type ───┐
//   │ ① ┌───────────────────────────────┐   │
//   │   │ (•) Brick                     │   │   a RadioCard per type
//   │   │     Cubes. The classic burr...│   │
//   │   └───────────────────────────────┘   │
//   │     ( ) Prism                         │
//   │     ( ) Spheres ...                   │
//   ├───────────────────────────────────────┤
//   │ ②                  [Cancel] [Create]  │
//   └───────────────────────────────────────┘
BtDialog {
    id: root
    objectName: "shell.newfile.dialog"
    title: qsTr("New puzzle — select the voxel type")
    width: 460

    property int selectedType: 0

    readonly property list<var> types: [
        { id: 0, key: "brick",   desc: qsTr("Cubes. The classic burr and polycube grid.") },
        { id: 1, key: "prism",   desc: qsTr("Triangular prisms stacked along Z.") },
        { id: 2, key: "spheres", desc: qsTr("Closely packed spheres; each touches twelve neighbours.") },
        { id: 3, key: "rhombic", desc: qsTr("Cut cubes that build rhombic dodecahedra.") },
        { id: 4, key: "tetoct",  desc: qsTr("Tetrahedra and octahedra, a cut cube grid.") }
    ]

    onAboutToShow: selectedType = 0
    onRejected: App.document.cancelFlow()
    onAccepted: App.document.newDocument(selectedType)

    contentItem: ColumnLayout {
        spacing: 8
        Repeater { // ①
            model: root.types
            delegate: RadioCard {
                required property var modelData
                objectName: "shell.newfile.type." + modelData.key
                Layout.fillWidth: true
                text: App.document.gridTypeDisplayName(modelData.id)
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
            BtButton { objectName: "shell.newfile.ok"; text: qsTr("Create"); primary: true; onClicked: root.accept() }
        }
    }
}
