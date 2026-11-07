pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// The Puzzle left card collapsed to its rail: the problems still listed, each
// as its chip, so one can be picked without opening the card, and + for a new
// one at the bottom, as CollapsedShapes does for the shapes. The problem's
// label is the tooltip. The list is one Tab stop, ↑ and ↓ select.
//
// Wireframe (each number marks its code below):
//
//   ┌──────┐
//   │①     │   scrolls when the problems do not fit
//   │┌────┐│
//   ││②P1 ││   selected: the card's row highlight
//   │└────┘│
//   │ [P2] │
//   │ ...  │
//   │ ③[+] │   new problem
//   └──────┘
Item {
    id: root
    objectName: "puzzle.problems.rail"

    ListView { // ①
        id: list
        objectName: "puzzle.problems.rail.list"
        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: newButton.top; bottomMargin: 6 }
        clip: true
        spacing: 4
        model: App.puzzle.problems
        currentIndex: App.puzzle.problem
        activeFocusOnTab: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 4 }
        Accessible.role: Accessible.List
        Accessible.name: qsTr("Problems")
        Keys.onUpPressed: App.puzzle.selectProblemBy(-1)
        Keys.onDownPressed: App.puzzle.selectProblemBy(1)

        delegate: Item { // ②
            id: row
            required property int index
            required property string idText
            required property string label
            readonly property bool selected: index === App.puzzle.problem

            objectName: "puzzle.problems.rail.row." + index
            width: ListView.view.width
            height: 32

            Rectangle {
                anchors { fill: parent; leftMargin: 3; rightMargin: 3 }
                radius: Theme.radiusControl
                color: row.selected ? Theme.accentSoft : (hover.hovered ? Theme.panel2 : "transparent")
                border.width: row.selected ? (list.activeFocus && App.keyboardCues.navigating ? 2 : 1) : 0
                border.color: list.activeFocus && App.keyboardCues.navigating ? Theme.accent : Theme.accentEdge
            }
            Chip {
                objectName: "puzzle.problems.rail.chip." + row.index
                anchors.centerIn: parent
                width: parent.width - 14
                text: row.idText
                fill: Theme.accent
                textColor: "white"
            }

            Accessible.role: Accessible.ListItem
            Accessible.name: row.idText + " " + row.label
            Accessible.selectable: true
            Accessible.selected: row.selected
            Accessible.onPressAction: App.puzzle.selectProblem(row.index)

            HoverHandler { id: hover }
            TapHandler {
                onTapped: {
                    App.puzzle.selectProblem(row.index)
                    list.forceActiveFocus()
                }
            }
            BtToolTip {
                shown: (hover.hovered || (row.ListView.isCurrentItem && list.activeFocus && App.keyboardCues.navigating)) && row.label.length > 0
                text: row.label
            }
        }
    }

    IconButton { // ③
        id: newButton
        objectName: "puzzle.problems.rail.new"
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom }
        iconName: "plus"
        tip: qsTr("New problem")
        onClicked: App.puzzle.newProblem()
    }
}
