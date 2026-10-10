pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import BurrTools.Ui

// Status (legacy statusWindow_c "Shape Information"), restyled with its
// content kept (OQ-34). One row per shape: select box, shape chip, voxel
// counts, the earlier shape it is identical to (as mirror, as shape,
// complete with colours), connectivity, holes, the brick-only tool checks
// and the symmetry. As in legacy, a cell that flags a problem -- not
// connected, has holes, unknown symmetry -- is filled with the shape's
// colour, and an Identical cell shows the other shape's chip.
//
// The table fills in as it is computed; Cancel stops that and keeps the rows
// so far, and the Select / Remove buttons then act on those rows only.
//
// Wireframe (each number marks its code below):
//
//   ┌ Shape information ─────────────────────────────────────────────────────┐
//   │ ①     Shape │   Units    │ Identical  │Connectivity│ Holes │Tools│     │
//   │             │Nor Var Tot │Mir Shp Cmp │Fac Edg Cor │2D  3D │N  M │ Sym │
//   │ ② [ ] [S1]  │296 216 512 │            │ X   X   X  │       │X  X │ ... │
//   │   [ ] [S2]  │ ...        │    [S1]    │            │   X   │     │     │
//   │ ③ This puzzle has no shapes.   (when none)                             │
//   │ ④ Calculating status information…  [━━━━━━      ]  [Cancel]            │
//   ├────────────────────────────────────────────────────────────────────────┤
//   │ ⑤ [Select holes] [Select identical shapes] [... complete] [... mirror] │
//   │ ⑥                                          [Remove selected] [Close]   │
//   └────────────────────────────────────────────────────────────────────────┘
//   Tools: bricks only
BtDialog {
    id: root
    objectName: "tools.status.dialog"
    title: qsTr("Shape information")
    width: Math.min(table.tableWidth + 42, (parent ? parent.width : 1200) - 32)
    height: Math.min(640, (parent ? parent.height : 800) - 32)
    closePolicy: Popup.CloseOnEscape

    readonly property ShapeStatusModel model: App.tools.shapeStatus

    onAboutToShow: model.start()
    onClosed: model.clear()

    readonly property int colW: 56
    readonly property int selectW: 34
    readonly property int shapeW: 150

    // a header group: title over its sub-columns
    readonly property var groups: {
        var g = [
            { title: qsTr("Units"), cols: [qsTr("Normal"), qsTr("Variable"), qsTr("Total")] },
            { title: qsTr("Identical"), cols: [qsTr("Mirror"), qsTr("Shape"), qsTr("Complete")] },
            { title: qsTr("Connectivity"), cols: [qsTr("Face"), qsTr("Edge"), qsTr("Corner")] },
            { title: qsTr("Holes"), cols: [qsTr("2D"), qsTr("3D")] }
        ]
        if (model.bricks)
            g.push({ title: qsTr("Tools"), cols: [qsTr("Notch"), qsTr("Mill")] })
        g.push({ title: "", cols: [qsTr("Sym")] })
        return g
    }

    component Cell: Rectangle {
        property string text
        property color ink: Theme.text
        property bool bold: false
        width: root.colW
        height: parent ? parent.height : 28
        color: "transparent"
        Text {
            anchors.centerIn: parent
            text: parent.text
            color: parent.ink
            font.pixelSize: Theme.fontSecondary
            font.weight: parent.bold ? Font.DemiBold : Font.Normal
        }
    }

    // fills with a shape's chip colour when `shape` >= 0
    component ChipCell: Cell {
        property int shape: -1
        color: shape >= 0 ? root.model.chipColor(shape) : "transparent"
        ink: shape >= 0 ? root.model.chipTextColor(shape) : Theme.text
        bold: shape >= 0
        radius: 4
        border.width: shape >= 0 ? 2 : 0
        border.color: Theme.panel
    }

    contentItem: ColumnLayout {
        spacing: 10

        ListView {
            id: table
            objectName: "tools.status.table"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.model
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.AutoFlickIfNeeded
            contentWidth: tableWidth
            headerPositioning: ListView.OverlayHeader
            ScrollBar.vertical: ScrollBar {}
            ScrollBar.horizontal: ScrollBar {}

            readonly property int columnCount: {
                var n = 0
                for (var i = 0; i < root.groups.length; i++)
                    n += root.groups[i].cols.length
                return n
            }
            readonly property int tableWidth: root.selectW + root.shapeW + columnCount * root.colW

            header: Rectangle { // ①
                z: 2
                width: table.tableWidth
                height: 46
                color: Theme.panel
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line2 }
                Row {
                    anchors.fill: parent
                    Item { width: root.selectW; height: parent.height }
                    Text {
                        width: root.shapeW
                        height: parent.height
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 6
                        text: qsTr("Shape")
                        color: Theme.muted
                        font.pixelSize: Theme.fontMicro
                        font.weight: Font.Bold
                    }
                    Repeater {
                        model: root.groups
                        delegate: Column {
                            required property var modelData
                            width: modelData.cols.length * root.colW
                            Text {
                                width: parent.width
                                height: 22
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignBottom
                                text: parent.modelData.title
                                color: Theme.text
                                font.pixelSize: Theme.fontMicro
                                font.weight: Font.Bold
                            }
                            Row {
                                Repeater {
                                    model: parent.parent.modelData.cols
                                    delegate: Text {
                                        required property string modelData
                                        width: root.colW
                                        height: 22
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                        text: modelData
                                        color: Theme.muted
                                        font.pixelSize: Theme.fontMicro
                                    }
                                }
                            }
                        }
                    }
                }
            }

            delegate: Rectangle { // ②
                id: row
                required property int index
                required property string idText
                required property int fixed
                required property int variable
                required property int total
                required property int identicalMirror
                required property int identicalShape
                required property int identicalComplete
                required property bool face
                required property bool edge
                required property bool corner
                required property bool holes2d
                required property bool holes3d
                required property bool notchable
                required property bool millable
                required property string symmetry
                required property bool rowSelected
                objectName: "tools.status.row." + index

                width: table.tableWidth
                height: 30
                color: rowSelected ? Theme.accentSoft : (index % 2 ? Theme.panel2 : Theme.panel)

                Row {
                    anchors.fill: parent
                    Item {
                        width: root.selectW
                        height: parent.height
                        BtCheckBox {
                            objectName: "tools.status.select." + row.index
                            anchors.centerIn: parent
                            implicitWidth: 16
                            selected: row.rowSelected
                            Accessible.name: qsTr("Select %1").arg(row.idText)
                            onClicked: root.model.setSelected(row.index, !row.rowSelected)
                        }
                    }
                    Item {
                        width: root.shapeW
                        height: parent.height
                        Rectangle {
                            anchors { left: parent.left; leftMargin: 4; verticalCenter: parent.verticalCenter }
                            height: 22
                            width: Math.min(chipLabel.implicitWidth + 14, parent.width - 8)
                            radius: Theme.radiusChip
                            color: root.model.chipColor(row.index)
                            Text {
                                id: chipLabel
                                anchors { fill: parent; leftMargin: 7; rightMargin: 7 }
                                verticalAlignment: Text.AlignVCenter
                                text: row.idText
                                elide: Text.ElideRight
                                color: root.model.chipTextColor(row.index)
                                font.pixelSize: Theme.fontSecondary
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                    Cell { text: row.fixed }
                    Cell { text: row.variable }
                    Cell { text: row.total }
                    ChipCell { shape: row.identicalMirror - 1; text: row.identicalMirror > 0 ? row.identicalMirror : "" }
                    ChipCell { shape: row.identicalShape - 1; text: row.identicalShape > 0 ? row.identicalShape : "" }
                    ChipCell { shape: row.identicalComplete - 1; text: row.identicalComplete > 0 ? row.identicalComplete : "" }
                    ChipCell { shape: row.face ? -1 : row.index; text: row.face ? "X" : "" }
                    ChipCell { shape: row.edge ? -1 : row.index; text: row.edge ? "X" : "" }
                    ChipCell { shape: row.corner ? -1 : row.index; text: row.corner ? "X" : "" }
                    ChipCell { shape: row.holes2d ? row.index : -1; text: row.holes2d ? "X" : "" }
                    ChipCell { shape: row.holes3d ? row.index : -1; text: row.holes3d ? "X" : "" }
                    Cell { visible: root.model.bricks; text: row.notchable ? "X" : "" }
                    Cell { visible: root.model.bricks; text: row.millable ? "X" : "" }
                    ChipCell { shape: row.symmetry === "---" ? row.index : -1; text: row.symmetry }
                }
            }

            Text { // ③
                anchors.centerIn: parent
                visible: table.count === 0 && !root.model.busy
                text: qsTr("This puzzle has no shapes.")
                color: Theme.muted
                font.pixelSize: Theme.fontBody
            }
        }

        // legacy's separate "Calculating Status information" progress window
        RowLayout { // ④
            visible: root.model.busy
            Layout.fillWidth: true
            spacing: 10
            Text {
                text: qsTr("Calculating status information…")
                color: Theme.muted
                font.pixelSize: Theme.fontSecondary
            }
            Rectangle {
                Layout.fillWidth: true
                height: 6
                radius: 3
                color: Theme.panel2
                Rectangle {
                    objectName: "tools.status.progress"
                    width: parent.width * root.model.progress
                    height: parent.height
                    radius: 3
                    color: Theme.accent
                }
            }
            BtButton {
                objectName: "tools.status.cancel"
                small: true
                text: qsTr("Cancel")
                onClicked: root.model.cancel()
            }
        }
    }

    footer: Item {
        implicitHeight: Math.max(selectButtons.implicitHeight, closeButtons.height) + 28
        Flow { // ⑤
            id: selectButtons
            anchors { left: parent.left; leftMargin: 20; right: closeButtons.left; rightMargin: 24; verticalCenter: parent.verticalCenter }
            spacing: 8
            enabled: !root.model.busy && table.count > 0
            BtButton { objectName: "tools.status.selectHoles"; text: qsTr("Select holes"); onClicked: root.model.selectHoles() }
            BtButton { objectName: "tools.status.selectShape"; text: qsTr("Select identical shapes"); onClicked: root.model.selectIdentical("shape") }
            BtButton { objectName: "tools.status.selectComplete"; text: qsTr("Select identical complete"); onClicked: root.model.selectIdentical("complete") }
            BtButton { objectName: "tools.status.selectMirror"; text: qsTr("Select identical mirror"); onClicked: root.model.selectIdentical("mirror") }
        }
        Row { // ⑥
            id: closeButtons
            anchors { right: parent.right; rightMargin: 20; verticalCenter: parent.verticalCenter }
            spacing: 8
            BtButton {
                objectName: "tools.status.remove"
                danger: true
                enabled: !root.model.busy && root.model.selectedCount > 0
                text: root.model.selectedCount > 0 ? qsTr("Remove selected (%1)").arg(root.model.selectedCount) : qsTr("Remove selected")
                onClicked: {
                    const n = root.model.removeSelected()
                    App.status.flash(n === 1 ? qsTr("Removed 1 shape") : qsTr("Removed %1 shapes").arg(n))
                }
            }
            BtButton { objectName: "tools.status.close"; text: qsTr("Close"); primary: true; onClicked: root.close() }
        }
    }
}
