import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// A form checkbox (or, with `radio`, a radio button): 16 dp box with the
// accent tick, label beside it. A checkbox toggles `selected` itself; a radio
// leaves it to the owner's binding, so its group stays exclusive.
AbstractButton {
    id: root
    property bool selected: false
    property bool radio: false

    implicitHeight: 26
    implicitWidth: box.width + 8 + label.implicitWidth
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.role: radio ? Accessible.RadioButton : Accessible.CheckBox
    Accessible.checked: selected
    Accessible.name: text

    onClicked: if (!radio) selected = !selected

    contentItem: Item {
        Rectangle {
            id: box
            anchors { left: parent.left; verticalCenter: parent.verticalCenter }
            width: 16; height: 16
            radius: root.radio ? 8 : 4
            color: root.selected && !root.radio ? Theme.accent : Theme.panel
            border.width: root.selected && !root.radio ? 0 : 2
            border.color: root.selected ? Theme.accent : (root.hovered ? Theme.muted : Theme.line2)
            Icon {
                visible: root.selected && !root.radio
                anchors.centerIn: parent
                name: "check"
                size: 12
                color: "white"
            }
            Rectangle {
                visible: root.selected && root.radio
                anchors.centerIn: parent
                width: 8; height: 8; radius: 4
                color: Theme.accent
            }
            Rectangle {
                anchors { fill: parent; margins: -3 }
                visible: root.visualFocus
                radius: box.radius + 3
                color: "transparent"
                border.width: 2
                border.color: Theme.accent
            }
        }
        Text {
            id: label
            anchors { left: box.right; leftMargin: 8; verticalCenter: parent.verticalCenter }
            text: root.text
            color: Theme.text
            font.pixelSize: Theme.fontBody
        }
    }
    opacity: enabled ? 1 : 0.4
}
