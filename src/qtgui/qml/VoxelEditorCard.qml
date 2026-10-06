import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// C07 Voxel editor card, first cut: header with the read-only voxel type chip
// (C14), Focus 2D and collapse. The tools, grid and colour footer arrive with
// phases P3 and P4.
Card {
    id: root
    objectName: "entities.editor"
    title: qsTr("Voxel editor")
    collapseIcon: "panelR"
    collapseObjectName: "entities.editor.collapse"
    onCollapseClicked: App.layout.rightCollapsed = true

    headerExtras: [
        Rectangle {
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
                Text { text: "🔒"; font.pixelSize: 10; color: Theme.muted; anchors.verticalCenter: parent.verticalCenter }
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
                text: qsTr("Voxel type: %1 — fixed when the file is created (File ▸ New); it cannot be changed afterwards").arg(App.document.gridTypeName)
            }
        },
        Item { width: 1; height: 1 },
        IconButton {
            objectName: "entities.editor.focus2d"
            iconName: App.layout.focus === LayoutController.Focus2d ? "unfocus" : "focus"
            on: App.layout.focus === LayoutController.Focus2d
            tip: qsTr("Focus 2D editor (Esc to exit)")
            onClicked: App.layout.toggleFocus2d()
        }
    ]

    Text {
        anchors.centerIn: parent
        width: parent.width - 40
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: qsTr("The voxel editor (tools, mirror and span, layers, 2D grid, grid size and drawing colour) arrives in a later phase.")
        color: Theme.muted
        font.pixelSize: Theme.fontSecondary
    }
}
