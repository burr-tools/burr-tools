import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// PR-01 IconButton: a monochrome icon, muted -> text on hover, accent when on.
AbstractButton {
    id: root
    property string iconName
    property string tip
    property bool on: false
    property bool toggle: false     // `on` is a state (a mode, a menu open): checked to assistive tech
    property bool dangerHover: false
    property real iconSize: 18

    implicitWidth: 28
    implicitHeight: 28
    focusPolicy: Qt.StrongFocus    // a click too: Tab goes on from what was last used
    hoverEnabled: true

    Accessible.name: tip
    Accessible.checkable: toggle
    Accessible.checked: toggle && on
    // the button's tooltip (PR-14), for tests
    readonly property alias toolTip: tipPopup
    // on keyboard focus too: the tip holds the shortcut and, in Minimal, the caption
    BtToolTip { id: tipPopup; shown: root.hovered || root.visualFocus; text: root.tip }

    background: Rectangle {
        radius: Theme.radiusChip
        color: root.on ? Theme.accentSoft : (root.hovered ? Theme.panel2 : "transparent")
        border.width: root.visualFocus ? 2 : 0
        border.color: Theme.accent
    }
    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            name: root.iconName
            size: root.iconSize
            color: root.on ? Theme.accent
                 : (root.hovered ? (root.dangerHover ? Theme.danger : Theme.text) : Theme.muted)
        }
    }
    opacity: enabled ? 1 : 0.4
}
