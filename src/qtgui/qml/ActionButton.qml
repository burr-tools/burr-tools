import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// PR-03 ActionButton: a glyph beside a title and a hint (Fit & scale,
// Repair). Standard: glyph 30, title 12.5/600, hint 11 muted on up to two
// lines, at least Theme.actionButtonMin high, as wide as it is laid out.
// Minimal (density.md section 1): Theme.actionButton high, glyph 24 and the
// short label inline, as wide as they are; the hint goes to the tooltip.
//
// `available` false greys it out and the hint says why; it still takes a
// press -- the inspector then says the reason in the status bar -- and a
// pointer over it still shows the tooltip, which a disabled item would not.
AbstractButton {
    id: root
    property string glyph
    property string title
    property string shortTitle: title
    property string hint
    property bool available: true

    readonly property bool compact: Theme.minimal
    implicitHeight: compact ? Theme.actionButton
                            : Math.max(Theme.actionButtonMin, content.implicitHeight + topPadding + bottomPadding)
    implicitWidth: compact ? content.implicitWidth + leftPadding + rightPadding : 160
    leftPadding: compact ? 4 : 8
    rightPadding: compact ? 9 : 8
    topPadding: compact ? 2 : 3
    bottomPadding: compact ? 2 : 3
    /* one of a group that is a single Tab stop (RovingFocus): Tab stops on
     * the group's chosen button alone, the arrows move between them */
    property RovingFocus roving: null
    focusPolicy: roving === null || roving.stop === root ? Qt.StrongFocus : Qt.ClickFocus
    Component.onCompleted: if (roving !== null) roving.join(root)
    Component.onDestruction: if (roving !== null) roving.leave(root)
    onActiveFocusChanged: if (activeFocus && roving !== null) roving.current = root
    Keys.onPressed: (event) => {
        if (roving !== null && (event.modifiers & ~Qt.KeypadModifier) === Qt.NoModifier
                && roving.navigate(root, event.key))
            event.accepted = true
    }
    hoverEnabled: true

    Accessible.role: Accessible.Button
    Accessible.name: title
    Accessible.description: hint
    BtToolTip {
        shown: root.hovered || root.visualFocus
        // Standard shows the hint already; a title cut short is named in full
        text: root.compact ? root.title + " — " + root.hint : (titleText.truncated ? root.title : "")
    }

    background: Rectangle {
        radius: Theme.radiusControl
        color: root.available && (root.hovered || root.down) ? Theme.accentSoft : Theme.panel2
        border.width: root.visualFocus ? 2 : 1
        border.color: root.visualFocus || (root.available && root.hovered) ? Theme.accent : Theme.line
    }
    contentItem: Row {
        id: content
        spacing: root.compact ? 6 : 10
        Glyph {
            anchors.verticalCenter: parent.verticalCenter
            name: root.glyph
            size: root.compact ? 24 : 30
        }
        Column {
            anchors.verticalCenter: parent.verticalCenter
            // Minimal: as wide as the label (undefined: the implicit width)
            width: root.compact ? undefined : root.availableWidth - 30 - content.spacing
            Text {
                id: titleText
                width: root.compact ? undefined : parent.width
                text: root.compact ? root.shortTitle : root.title
                color: Theme.text
                font.pixelSize: root.compact ? Theme.fontSecondary : Theme.fontActionTitle
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Text {
                visible: !root.compact && text.length > 0
                width: parent.width
                text: root.hint
                color: Theme.muted
                font.pixelSize: Theme.fontMicro
                lineHeight: 1.1
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
        }
    }
    opacity: enabled && available ? 1 : 0.4
}
