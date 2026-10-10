import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// PR-21 Workspace rail button: icon over caption; selected = accentSoft fill,
// accent icon and caption, and a 3 dp indicator bar at the left edge of the
// rail, so selection never relies on colour alone.
AbstractButton {
    id: root
    property string iconName
    property string tip
    property bool selected: false

    implicitWidth: Theme.minimal ? 36 : 52
    implicitHeight: Theme.minimal ? 64 : 50
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: text
    // the button's tooltip (PR-14), for tests
    readonly property alias toolTip: tipPopup
    BtToolTip { id: tipPopup; shown: root.hovered; text: root.tip }

    background: Rectangle {
        radius: 10
        color: root.selected ? Theme.accentSoft : (root.hovered ? Theme.panel2 : "transparent")
        border.width: root.visualFocus ? 2 : 0
        border.color: Theme.accent
        Rectangle {
            visible: root.selected
            width: 3
            radius: 1.5
            color: Theme.accent
            x: -((Theme.workspaceRail - root.width) / 2) + 1
            anchors { top: parent.top; bottom: parent.bottom; topMargin: 13; bottomMargin: 13 }
        }
    }
    contentItem: Item {
        Column {
            anchors.centerIn: parent
            spacing: 2
            Icon {
                anchors.horizontalCenter: parent.horizontalCenter
                name: root.iconName
                size: 22
                color: root.selected ? Theme.accent : (root.hovered ? Theme.text : Theme.muted)
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.text
                visible: !Theme.minimal
                color: root.selected ? Theme.accent : (root.hovered ? Theme.text : Theme.muted)
                font.pixelSize: Theme.fontCaption
                font.weight: Font.DemiBold
            }
        }
    }
}
