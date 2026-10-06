import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// A 3D-toolbar button (C06): icon over a caption, 48x44; compact 34x34
// icon-only (Focus 2D, Minimal), where the caption becomes the tooltip.
AbstractButton {
    id: root
    property string iconName
    property string glyphName        // a coloured glyph instead of a UI icon (the Edit button)
    property string caption
    property string tip
    property bool on: false
    property bool compact: false
    property string badge            // the Edit button's tool number

    implicitWidth: compact ? 34 : Theme.toolbarButtonWidth
    implicitHeight: compact ? 34 : Theme.toolbarButtonHeight
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: caption

    // the button's tooltip (PR-14), for tests
    readonly property alias toolTip: tipPopup
    BtToolTip {
        id: tipPopup
        shown: root.hovered && (root.tip.length > 0 || root.compact)
        text: root.tip.length > 0 ? root.tip : root.caption
    }

    background: Rectangle {
        radius: 8
        color: root.on ? Theme.accentSoft : (root.hovered ? Theme.panel2 : "transparent")
        border.width: root.visualFocus ? 2 : 0
        border.color: Theme.accent
    }

    contentItem: Item {
        Column {
            anchors.centerIn: parent
            spacing: 1
            Item {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 22
                height: 22
                Icon {
                    visible: root.glyphName.length === 0
                    anchors.centerIn: parent
                    name: root.iconName
                    size: 20
                    color: root.on ? Theme.accent : (root.hovered ? Theme.text : Theme.muted)
                }
                Glyph {
                    visible: root.glyphName.length > 0
                    anchors.centerIn: parent
                    name: root.glyphName
                    size: 22
                }
            }
            Text {
                visible: !root.compact
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.caption
                color: root.on ? Theme.accent : (root.hovered ? Theme.text : Theme.muted)
                font.pixelSize: Theme.fontCaption
                font.weight: Font.DemiBold
            }
        }
        Text {
            visible: root.badge.length > 0
            anchors { top: parent.top; right: parent.right; topMargin: 2; rightMargin: 4 }
            text: root.badge
            color: root.on ? Theme.accent : Theme.muted
            font.pixelSize: Theme.fontKeyBadge
            font.weight: Font.Bold
        }
    }
    opacity: enabled ? 1 : 0.4
}
