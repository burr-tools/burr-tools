import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// PR-21 Workspace rail button: icon over caption (icon only in Minimal);
// selected = accentSoft fill reaching the rail's right edge, accent icon and
// caption, and a 3 dp indicator bar at that edge, so selection never relies
// on colour alone.
AbstractButton {
    id: root
    property string iconName
    property string tip
    property bool selected: false
    // a workspace: selected, it joins the rail's edge as a tab with the bar;
    // false for the menu button, which is only highlighted while its menu is open
    property bool tab: true

    implicitWidth: Theme.minimal ? 36 : 52
    implicitHeight: Theme.minimal ? 40 : 50
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus    // a click too: Tab goes on from what was last used
    // a workspace is a tab of the window; the menu button is a plain button
    Accessible.role: tab ? Accessible.PageTab : Accessible.Button
    Accessible.selected: tab && selected
    Accessible.name: text
    Accessible.description: tip
    // the button's tooltip (PR-14), for tests
    readonly property alias toolTip: tipPopup
    // on keyboard focus too: the tip holds the shortcut and, in Minimal, the caption
    BtToolTip { id: tipPopup; shown: root.hovered || root.visualFocus; text: root.tip }

    // the gap between the button and the rail's edge, each side
    readonly property real railGap: (Theme.workspaceRail - width) / 2

    background: Item {
        // selected: one fill (accentSoft is translucent, so no overlaps)
        // running on to the rail's edge, square there, where the indicator
        // bar is -- a tab joined to the workspace it shows. A rounded
        // rectangle wider than its clip loses its right corners to it.
        Item {
            visible: root.selected && root.tab
            width: parent.width + root.railGap - 1      // up to the rail's 1 dp edge line
            height: parent.height
            clip: true
            Rectangle {
                width: parent.width + radius
                height: parent.height
                radius: 10
                color: Theme.accentSoft
            }
            Rectangle {
                anchors { top: parent.top; bottom: parent.bottom; right: parent.right }
                width: 3
                color: Theme.accent
            }
        }
        Rectangle {
            anchors.fill: parent
            radius: 10
            color: root.selected && !root.tab ? Theme.accentSoft : (!root.selected && root.hovered ? Theme.panel2 : "transparent")
            border.width: root.visualFocus ? 2 : 0
            border.color: Theme.accent
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
