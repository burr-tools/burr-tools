import QtQuick
import BurrTools.Ui

// PR-15 KeyBadge: a key cap for hints and the shortcuts list.
Rectangle {
    property alias text: label.text
    implicitWidth: label.implicitWidth + 8
    implicitHeight: label.implicitHeight + 3
    radius: 4
    color: Theme.panel2
    border.color: Theme.line2
    Text {
        id: label
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -0.5
        color: Theme.text
        font.family: Theme.monoFamily
        font.pixelSize: Theme.fontMicro
    }
    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 1
        color: Theme.line2
    }
}
