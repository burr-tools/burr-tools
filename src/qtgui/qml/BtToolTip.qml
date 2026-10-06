import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// PR-14 Tooltip, styled per design-tokens §7: panel fill, 1 dp line2
// border, radius 6, padding 4x8, 12 dp text wrapping at 280 dp. It appears
// after the 500 ms hover delay and only while Settings ▸ Show tooltips is
// on -- the setting is checked here, so no host can forget it.
//
// Put one inside the control it explains:
//     BtToolTip { shown: root.hovered; text: root.tip }
ToolTip {
    id: root
    property bool shown: false
    readonly property real maxWidth: 280

    visible: shown && text.length > 0 && App.settings.tooltips
    delay: Theme.tooltipDelay
    width: Math.min(implicitWidth, maxWidth)
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
