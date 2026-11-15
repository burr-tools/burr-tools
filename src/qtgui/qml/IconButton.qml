pragma ComponentBehavior: Bound

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
    /* The button's tooltip (PR-14), made the first time it could show and kept:
     * a popup for every button cost a card made of many (the Puzzle tiles have
     * two each) about 0.04 ms a button. On keyboard focus too: the tip holds
     * the shortcut and, in Minimal, the caption. */
    property BtToolTip toolTip: null
    // made hidden, then armed: one made already shown skips the tooltip's delay
    property bool tipArmed: false
    Component {
        id: tipComponent
        BtToolTip { shown: root.tipArmed && (root.hovered || root.visualFocus); text: root.tip }
    }
    function wantTip(): void {
        if (root.toolTip !== null)
            return
        root.toolTip = tipComponent.createObject(root) as BtToolTip
        root.tipArmed = true
    }
    onHoveredChanged: if (root.hovered) root.wantTip()
    onVisualFocusChanged: if (root.visualFocus) root.wantTip()

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
