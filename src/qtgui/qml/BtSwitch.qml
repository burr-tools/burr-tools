import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// PR-07 Switch: 32x18 track, 14 dp knob; on = accent track, knob right.
AbstractButton {
    id: root
    checkable: true
    implicitWidth: 32
    implicitHeight: 18
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.CheckBox
    Accessible.checked: checked

    background: Rectangle {
        radius: 9
        color: root.checked ? Theme.accent : Theme.line2
        Behavior on color { ColorAnimation { duration: 150 } }
        Rectangle {
            width: 14; height: 14; radius: 7
            y: 2
            x: root.checked ? parent.width - width - 2 : 2
            color: "white"
            Behavior on x { NumberAnimation { duration: 150 } }
        }
        Rectangle {
            anchors { fill: parent; margins: -3 }
            visible: root.visualFocus
            radius: 12
            color: "transparent"
            border.width: 2
            border.color: Theme.accent
        }
    }
    contentItem: Item {}
    opacity: enabled ? 1 : 0.4
}
