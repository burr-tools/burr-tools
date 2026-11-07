pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// The table's Group column: which groups a piece is in, as a box reading
// "G1, G2" that opens a list of every group with a tick for each, and + New.
// A piece can be in several groups, so the list stays open while they are
// ticked; Esc or a press elsewhere closes it. A tick takes all the piece's
// copies; the card's steppers set a part of them.
//
// Wireframe (each number marks its code below):
//
//   ① [ G1, G2     ⌄ ]        closed: the groups, elided to the box
//   ┌────────────────┐
//   │② [✓] Group 1   │        open: every group of the problem
//   │  [ ] Group 2   │
//   │③ + New         │
//   └────────────────┘
AbstractButton {
    id: root
    property int shape: -1
    property string idText
    property string groupText            // "G1, G2"; empty: none

    /* A group exists while it has members, so unticking the last one ends it.
     * Its row stays until the list closes, to be ticked again. */
    property list<int> unticked: []
    readonly property list<int> shownGroups: {
        const ids = []
        for (let i = 0; App.puzzle.groups.revision >= 0 && i < App.puzzle.groups.count; i++)
            ids.push(App.puzzle.groups.value(i, "group"))
        for (let i = 0; i < unticked.length; i++)
            if (ids.indexOf(unticked[i]) < 0)
                ids.push(unticked[i])
        return ids.sort((a, b) => a - b)
    }

    readonly property alias list: popup
    readonly property alias newRow: newButton

    implicitWidth: 66
    implicitHeight: 24
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    leftPadding: 6
    rightPadding: 18
    Accessible.role: Accessible.ComboBox
    Accessible.name: qsTr("%1 groups").arg(idText)
    Accessible.description: groupText.length > 0 ? groupText : qsTr("No group")
    onClicked: popup.opened ? popup.close() : popup.open()
    Keys.onDownPressed: popup.open()

    background: Rectangle { // ①
        radius: Theme.radiusChip
        color: root.hovered ? Theme.panel2 : Theme.panel
        border.width: root.visualFocus ? 2 : 1
        border.color: root.visualFocus ? Theme.accent : Theme.line2
    }
    contentItem: Text {
        objectName: root.objectName + ".text"
        text: root.groupText.length > 0 ? root.groupText : qsTr("–")
        Accessible.ignored: true
        color: Theme.text
        font.pixelSize: Theme.fontSecondary
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }
    Icon {
        anchors { right: parent.right; rightMargin: 4; verticalCenter: parent.verticalCenter }
        name: "chevron-down"
        size: 12
        color: Theme.muted
    }

    // the warning that the change costs solutions comes on top
    Connections {
        target: App.document
        function onSolutionsWarningRequested() { popup.close() }
    }

    Popup {
        id: popup
        objectName: root.objectName + ".list"
        // a press on the box is its own: closing here, the click would open it again
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        y: root.height + 2
        width: Math.max(root.width, 150)
        padding: 4
        focus: true
        margins: 6
        background: Rectangle {
            color: Theme.panel
            border.color: Theme.line2
            radius: Theme.radiusControl
        }
        onOpened: {
            const first = rows.count > 0 ? rows.itemAt(0) : newButton
            first.forceActiveFocus(App.keyboardCues.navigating ? Qt.TabFocusReason : Qt.MouseFocusReason)
        }
        onClosed: {
            root.unticked = []
            root.forceActiveFocus(App.keyboardCues.navigating ? Qt.TabFocusReason : Qt.MouseFocusReason)
        }

        contentItem: Column {
            spacing: 0
            // ↑ and ↓ as Tab, within the list
            Keys.onUpPressed: (popup.contentItem.Window.activeFocusItem as Item).nextItemInFocusChain(false).forceActiveFocus(Qt.TabFocusReason)
            Keys.onDownPressed: (popup.contentItem.Window.activeFocusItem as Item).nextItemInFocusChain(true).forceActiveFocus(Qt.TabFocusReason)

            Repeater {
                id: rows
                model: root.shownGroups
                delegate: CheckRow { // ②
                    id: row
                    required property int modelData
                    objectName: root.objectName + ".group." + modelData
                    width: parent ? parent.width : implicitWidth
                    implicitHeight: 26
                    text: qsTr("Group %1").arg(modelData)
                    // through the revision: the call alone would not follow an edit
                    selected: App.puzzle.groups.revision >= 0 && App.puzzle.pieceGroupCount(root.shape, modelData) > 0
                    onClicked: {
                        if (selected)
                            root.unticked = root.unticked.concat([ modelData ])
                        App.puzzle.setGroupCount(root.shape, modelData, selected ? 0 : App.puzzle.copiesOf(root.shape))
                    }
                }
            }
            AbstractButton { // ③
                id: newButton
                objectName: root.objectName + ".new"
                width: parent.width
                implicitHeight: 26
                hoverEnabled: true
                focusPolicy: Qt.StrongFocus
                text: qsTr("+ New")
                Accessible.name: qsTr("New group")
                onClicked: App.puzzle.newGroup(root.shape)
                background: Rectangle {
                    radius: Theme.radiusChip
                    color: newButton.hovered ? Theme.panel2 : "transparent"
                    border.width: newButton.visualFocus ? 2 : 0
                    border.color: Theme.accent
                }
                contentItem: Text {
                    leftPadding: 6
                    text: newButton.text
                    Accessible.ignored: true
                    color: Theme.text
                    font.pixelSize: Theme.fontSecondary
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
