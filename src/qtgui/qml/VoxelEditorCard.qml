import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// C07 Voxel editor card, first cut: header with the read-only voxel type chip
// (C14), Focus 2D and collapse. The tools, grid and colour footer arrive with
// phases P3 and P4.
//
// Wireframe (each number marks its code below):
//
//   ┌───────────────────────────────────────────┐
//   │ Voxel editor  ①(▪ Brick)  ②[⤢]        [◨] │   title, collapse: Card
//   ├───────────────────────────────────────────┤
//   │                                           │
//   │      ③ The voxel editor (tools, ...)      │
//   │         arrives in a later phase.         │
//   │                                           │
//   └───────────────────────────────────────────┘
Card {
    id: root
    objectName: "entities.editor"
    title: qsTr("Voxel editor")
    collapseIcon: "panelR"
    collapseObjectName: "entities.editor.collapse"
    onCollapseClicked: App.layout.rightCollapsed = true

    headerExtras: [
        Rectangle { // ①
            objectName: "entities.editor.voxelType"
            height: 22
            width: typeRow.implicitWidth + 16
            radius: 11
            color: Theme.panel2
            border.color: Theme.line
            Row {
                id: typeRow
                anchors.centerIn: parent
                spacing: 4
                // a padlock, drawn: an emoji would need a fallback font,
                // which Qt finds by reading every installed one (App)
                Item {
                    width: 8
                    height: 10
                    anchors.verticalCenter: parent.verticalCenter
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 6
                        height: 7
                        radius: 3
                        color: "transparent"
                        border.color: Theme.muted
                        border.width: 1.5
                    }
                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: 8
                        height: 6
                        radius: 1.5
                        color: Theme.muted
                    }
                }
                Text {
                    text: App.document.gridTypeName
                    color: Theme.text
                    font.pixelSize: Theme.fontMicro
                    font.weight: Font.DemiBold
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            HoverHandler { id: typeHover }
            BtToolTip {
                shown: typeHover.hovered
                text: qsTr("Voxel type: %1 — fixed when the file is created (File › New); it cannot be changed afterwards").arg(App.document.gridTypeName)
            }
        },
        Item { width: 1; height: 1 },
        IconButton { // ②
            objectName: "entities.editor.focus2d"
            iconName: App.layout.focus === LayoutController.Focus2d ? "unfocus" : "focus"
            on: App.layout.focus === LayoutController.Focus2d
            tip: qsTr("Focus 2D editor (Esc to exit)")
            onClicked: App.layout.toggleFocus2d()
        }
    ]

    Text { // ③
        anchors.centerIn: parent
        width: parent.width - 40
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: qsTr("The voxel editor (tools, mirror and span, layers, 2D grid, grid size and drawing colour) arrives in a later phase.")
        color: Theme.muted
        font.pixelSize: Theme.fontSecondary
    }
}
