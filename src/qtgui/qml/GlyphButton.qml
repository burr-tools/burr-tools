import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// PR-02 GlyphButton: a glyph over a caption, an optional key badge at the
// top right. `on` is the selected state: of a radio group (the tool strip,
// the planes) or of a toggle (Mirror, Span: `toggle`). The caption can take
// an axis colour (Mirror and Span's X Y Z). `iconOnly` (Minimal density)
// hides the caption and the badge; the tooltip names the button then.
AbstractButton {
    id: root
    property string glyph
    property string caption
    property color captionColor: "transparent"     // transparent: the theme's muted / text
    property string keyBadge
    property string tip
    property bool on: false
    property bool toggle: false
    // the focus ring for a group that holds the focus for its buttons (a radio strip)
    property bool ring: false
    property real glyphSize: 30
    property bool iconOnly: false

    implicitWidth: 62
    implicitHeight: Theme.toolStrip
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true

    Accessible.role: toggle ? Accessible.CheckBox : Accessible.RadioButton
    Accessible.name: tip.length > 0 ? tip : caption
    Accessible.checkable: true
    Accessible.checked: on
    readonly property alias toolTip: tipPopup
    BtToolTip { id: tipPopup; shown: root.hovered || root.visualFocus; text: root.tip }

    background: Rectangle {
        radius: Theme.radiusControl
        color: root.on ? Theme.accentSoft : Theme.panel2
        border.width: root.visualFocus || root.ring ? 2 : 1
        border.color: root.visualFocus || root.ring || root.on ? Theme.accent : (root.hovered ? Theme.line2 : Theme.line)
    }
    contentItem: Item {
        Column {
            anchors.centerIn: parent
            spacing: 2
            Glyph {
                anchors.horizontalCenter: parent.horizontalCenter
                name: root.glyph
                size: root.glyphSize
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.caption
                visible: text.length > 0 && !root.iconOnly
                color: root.captionColor.a > 0 ? root.captionColor
                     : (root.on || root.hovered ? Theme.text : Theme.muted)
                font.pixelSize: Theme.fontCaption
                font.weight: Font.DemiBold
            }
        }
        Text {
            anchors { top: parent.top; right: parent.right; topMargin: 3; rightMargin: 6 }
            text: root.keyBadge
            visible: text.length > 0 && !root.iconOnly
            color: Theme.muted
            font.pixelSize: Theme.fontKeyBadge
            font.weight: Font.Bold
        }
    }
    opacity: enabled ? 1 : 0.4
}
