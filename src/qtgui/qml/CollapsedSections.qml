pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// The Puzzle right card collapsed to its rail: its sections, from the same
// table as the card (App.puzzle.sections), their names turned; a press opens
// the card at that section. The selected piece's chip sits above its section,
// so the rail says which piece, and a section with a count shows it. One Tab
// stop: ↑ and ↓ move, Enter or Space opens.
//
// Wireframe (each number marks its code below):
//
//   ┌──────┐
//   │①     │   scrolls when the sections do not fit
//   │ ②[S5]│   the selected piece; an outline and – when there is none
//   │ ③ S  │   the section's name, turned
//   │   e  │
//   │   l  │
//   │ ④ 1  │   its count, if it has one
//   │──────│
//   │   G  │
//   │   r  │
//   │ ...  │
//   └──────┘
Item {
    id: root
    objectName: "puzzle.sections.rail"

    ListView { // ①
        id: list
        objectName: "puzzle.sections.rail.list"
        anchors.fill: parent
        clip: true
        spacing: 6
        model: App.puzzle.sections
        activeFocusOnTab: true
        boundsBehavior: Flickable.StopAtBounds
        keyNavigationEnabled: true
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 4 }
        Accessible.role: Accessible.List
        Accessible.name: qsTr("Sections of Piece & rules")

        Keys.onReturnPressed: App.puzzle.showSection(App.puzzle.sections.value(currentIndex, "key"))
        Keys.onEnterPressed: App.puzzle.showSection(App.puzzle.sections.value(currentIndex, "key"))
        Keys.onSpacePressed: App.puzzle.showSection(App.puzzle.sections.value(currentIndex, "key"))

        delegate: Item {
            id: row
            required property int index
            required property string key
            required property string title
            required property string railText
            required property string badge
            readonly property bool isPiece: key === "piece"
            readonly property bool current: ListView.isCurrentItem

            objectName: "puzzle.sections.rail.row." + key
            width: ListView.view.width
            height: column.implicitHeight + 8

            Accessible.role: Accessible.Button
            Accessible.name: railText.length > 0 ? qsTr("%1: %2").arg(title).arg(railText) : title
            Accessible.description: qsTr("Opens the sidebar at this section")
            Accessible.onPressAction: App.puzzle.showSection(row.key)

            Rectangle {
                anchors { fill: parent; leftMargin: 3; rightMargin: 3 }
                radius: Theme.radiusControl
                color: hover.hovered ? Theme.panel2 : "transparent"
                border.width: list.activeFocus && App.keyboardCues.navigating && row.current ? 2 : 0
                border.color: Theme.accent
            }
            HoverHandler { id: hover }
            TapHandler { onTapped: App.puzzle.showSection(row.key) }
            BtToolTip {
                shown: hover.hovered || (row.current && list.activeFocus && App.keyboardCues.navigating)
                text: row.railText.length > 0 ? qsTr("%1: %2").arg(row.title).arg(row.railText) : row.title
            }

            Column {
                id: column
                anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 4 }
                spacing: 6

                Rectangle { // ②
                    objectName: "puzzle.sections.rail.piece"
                    visible: row.isPiece
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: row.width - 14
                    height: 26
                    radius: Theme.radiusChip
                    color: App.puzzle.pieceShown ? App.puzzle.pieceColor : "transparent"
                    border.width: App.puzzle.pieceShown ? 0 : 1
                    border.color: Theme.line2
                    Text {
                        anchors.centerIn: parent
                        width: Math.min(implicitWidth, parent.width - 4)
                        elide: Text.ElideRight
                        text: App.puzzle.pieceShown ? App.puzzle.pieceIdText : qsTr("–")
                        Accessible.ignored: true
                        color: App.puzzle.pieceShown ? App.puzzle.pieceTextColor : Theme.muted
                        font.pixelSize: Theme.fontMicro
                        font.weight: Font.Bold
                    }
                }
                Item { // ③
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: titleText.implicitHeight
                    height: titleText.implicitWidth
                    Text {
                        id: titleText
                        objectName: "puzzle.sections.rail.title." + row.key
                        anchors.centerIn: parent
                        rotation: 90
                        text: row.title
                        Accessible.ignored: true
                        color: Theme.muted
                        font.pixelSize: Theme.fontSecondary
                        font.weight: Font.DemiBold
                    }
                }
                Chip { // ④
                    objectName: "puzzle.sections.rail.badge." + row.key
                    visible: !row.isPiece && row.badge.length > 0
                    anchors.horizontalCenter: parent.horizontalCenter
                    kind: Chip.Count
                    text: row.badge
                }
            }
        }
    }
}
