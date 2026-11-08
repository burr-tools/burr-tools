import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// PR-27 Radio card: radio dot, bold title, muted description; selected =
// accent border, 1 dp accent ring, accentSoft fill.
AbstractButton {
    id: root
    property string description
    property bool selected: false

    checkable: false
    implicitHeight: content.implicitHeight + 18
    implicitWidth: 360
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.RadioButton
    Accessible.name: text
    Accessible.checked: selected

    background: Rectangle {
        radius: 10
        color: root.selected ? Theme.accentSoft : (root.hovered ? Theme.panel2 : Theme.panel)
        border.width: root.selected || root.visualFocus ? 2 : 1
        border.color: root.selected || root.visualFocus ? Theme.accent : Theme.line
    }

    contentItem: Item {
        Row {
            id: content
            anchors { left: parent.left; leftMargin: 10; right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter }
            spacing: 10
            Rectangle {
                width: 16; height: 16; radius: 8
                anchors.top: parent.top
                anchors.topMargin: 1
                color: "transparent"
                border.width: 2
                border.color: root.selected ? Theme.accent : Theme.line2
                Rectangle {
                    visible: root.selected
                    anchors.centerIn: parent
                    width: 8; height: 8; radius: 4
                    color: Theme.accent
                }
            }
            Column {
                width: content.width - 26
                spacing: 2
                Text {
                    text: root.text
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    font.weight: Font.DemiBold
                }
                Text {
                    width: parent.width
                    visible: text.length > 0
                    text: root.description
                    color: Theme.muted
                    font.pixelSize: Theme.fontSecondary
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
