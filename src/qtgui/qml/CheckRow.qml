import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// PR-16: a menu row with a 14 dp check column; a radio row draws the same
// check. Rows stay in their menu when clicked (Display menu keep-open rule).
AbstractButton {
    id: root
    property bool selected: false
    property string hint            // shown muted at the right, e.g. why the row is disabled

    implicitHeight: 30
    implicitWidth: 230
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.role: Accessible.CheckBox
    Accessible.checked: selected
    Accessible.name: text

    background: Rectangle {
        radius: Theme.radiusChip
        color: root.hovered && root.enabled ? Theme.panel2 : "transparent"
        border.width: root.visualFocus ? 2 : 0
        border.color: Theme.accent
    }
    contentItem: Item {
        Icon {
            id: tick
            visible: root.selected
            anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
            name: "check"
            size: 14
            color: Theme.accent
        }
        Text {
            anchors { left: parent.left; leftMargin: 30; verticalCenter: parent.verticalCenter }
            text: root.text
            color: Theme.text
            font.pixelSize: Theme.fontBody
        }
        Text {
            visible: root.hint.length > 0
            anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
            text: root.hint
            color: Theme.muted
            font.pixelSize: Theme.fontMicro
        }
    }
    opacity: enabled ? 1 : 0.4
}
