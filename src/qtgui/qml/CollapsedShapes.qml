import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// The Shapes card collapsed to its rail (C11): the shapes still listed, each
// as its chip -- colour and id -- so one can be picked without opening the
// card. The selected one is highlighted as the card's row is; the shape's name
// is the tooltip.
//
// Wireframe (each number marks its code below):
//
//   ┌──────┐
//   │①     │   scrolls when the shapes do not fit
//   │┌────┐│
//   ││②S1 ││   selected: the card's row highlight
//   │└────┘│
//   │ [S2] │
//   │ [S3] │
//   │ ...  │
//   └──────┘
ListView { // ①
    id: root
    objectName: "entities.shapes.rail.list"
    clip: true
    spacing: 4
    model: App.shapes
    currentIndex: App.shapes.selected      // keeps the selected one in view
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 4 }
    Accessible.role: Accessible.List
    Accessible.name: qsTr("Shapes")

    delegate: Item { // ②
        id: row
        required property int index
        required property string idText
        required property string label
        required property color chipColor
        required property color chipTextColor
        readonly property bool selected: index === App.shapes.selected

        objectName: "entities.shapes.rail.row." + index
        width: ListView.view.width
        height: 32

        // the card's selected row: accentSoft with a faint accent edge
        Rectangle {
            anchors { fill: parent; leftMargin: 3; rightMargin: 3 }
            radius: Theme.radiusControl
            color: row.selected ? Theme.accentSoft : (hover.hovered ? Theme.panel2 : "transparent")
            border.width: row.selected ? 1 : 0
            border.color: Theme.accentEdge
        }
        Rectangle {
            objectName: "entities.shapes.rail.chip." + row.index
            anchors.centerIn: parent
            width: parent.width - 14
            height: 24
            radius: Theme.radiusChip
            color: row.chipColor
            Text {
                anchors.centerIn: parent
                width: parent.width - 4
                horizontalAlignment: Text.AlignHCenter
                text: row.idText
                Accessible.ignored: true
                color: row.chipTextColor
                font.pixelSize: Theme.fontMicro
                font.weight: Font.Bold
                elide: Text.ElideRight
            }
        }

        HoverHandler { id: hover }
        TapHandler { onTapped: App.shapes.select(row.index) }
        BtToolTip {
            shown: hover.hovered
            text: row.label.length > 0 ? row.idText + "  " + row.label : row.idText
        }
        Accessible.role: Accessible.ListItem
        Accessible.name: row.label.length > 0 ? row.idText + " " + row.label : row.idText
        Accessible.selectable: true
        Accessible.selected: row.selected
        Accessible.onPressAction: App.shapes.select(row.index)
    }
}
