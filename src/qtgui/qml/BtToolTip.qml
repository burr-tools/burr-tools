import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// PR-14 Tooltip, styled per design-tokens §7: panel fill, 1 dp line2
// border, radius 6, padding 4x8, 12 dp text wrapping at 280 dp. It appears
// after the 500 ms hover delay and only while Settings ▸ Show tooltips is
// on -- the setting is checked here, so no host can forget it.
//
// Put one inside the control it explains:
//     BtToolTip { shown: root.hovered || root.visualFocus; text: root.tip }
// -- on keyboard focus as well as hover, so the keyboard reaches what it says.
ToolTip {
    id: root
    property bool shown: false
    readonly property real maxWidth: 280

    visible: shown && text.length > 0 && App.settings.tooltips
    delay: Theme.tooltipDelay
    width: Math.min(implicitWidth, maxWidth)
    /* centred over its control by the width it has: the Basic style's
     * centres by implicitWidth, which for a text that wraps is the whole of
     * it on one line -- the tooltip landed far to the left */
    x: parent ? (parent.width - width) / 2 : 0
    y: -height - 3
    topPadding: 4
    bottomPadding: 4
    leftPadding: 8
    rightPadding: 8
    font.pixelSize: Theme.fontSecondary

    contentItem: Text {
        objectName: "tooltip.text"
        text: root.text
        font: root.font
        color: Theme.text
        wrapMode: Text.Wrap
    }
    background: Rectangle {
        color: Theme.panel
        border.width: 1
        border.color: Theme.line2
        radius: Theme.radiusChip
    }
}
