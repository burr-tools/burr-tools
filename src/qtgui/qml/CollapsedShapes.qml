pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// The Shapes card collapsed to its rail (C11, C01 rail): the shapes still
// listed, each as its chip -- colour and id -- so one can be picked without
// opening the card, and + for a new shape at the bottom. The selected one is
// highlighted as the card's row is; the shape's name is the tooltip. The
// list is one Tab stop, ↑ and ↓ select, as in the card.
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
//   │ ③[+] │   new shape
//   └──────┘
Item {
    id: root

    ListView { // ①
        id: list
        objectName: "entities.shapes.rail.list"
        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: newButton.top; bottomMargin: 6 }
        clip: true
        spacing: 4
        model: App.shapes
        currentIndex: App.shapes.selected      // keeps the selected one in view
        activeFocusOnTab: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 4 }
        Accessible.role: Accessible.List
        Accessible.name: qsTr("Shapes")
        Keys.onUpPressed: App.shapes.selectBy(-1)
        Keys.onDownPressed: App.shapes.selectBy(1)

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

            // the card's selected row: accentSoft with a faint accent edge,
            // ringed in the accent while the keyboard is in the list
            Rectangle {
                anchors { fill: parent; leftMargin: 3; rightMargin: 3 }
                radius: Theme.radiusControl
                color: row.selected ? Theme.accentSoft : (hover.hovered ? Theme.panel2 : "transparent")
                border.width: row.selected ? (list.activeFocus ? 2 : 1) : 0
                border.color: list.activeFocus ? Theme.accent : Theme.accentEdge
            }
            Chip {
                objectName: "entities.shapes.rail.chip." + row.index
                anchors.centerIn: parent
                width: parent.width - 14
                height: 24
                text: row.idText
                fill: row.chipColor
                textColor: row.chipTextColor
            }

            HoverHandler { id: hover }
            TapHandler { onTapped: { App.shapes.select(row.index); list.forceActiveFocus() } }
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

    IconButton { // ③
        id: newButton
        objectName: "entities.shapes.rail.new"
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom }
        iconName: "plus"
        tip: qsTr("New shape")
        onClicked: App.shapes.addShape()
    }
}
