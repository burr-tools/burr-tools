import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// PR-02 GlyphButton: a glyph over a caption, an optional key badge at the
// top right. `on` is the selected state: of a radio group (the tool strip,
// the planes) or of a toggle (Mirror, Span: `toggle`). The caption can take
// an axis colour (Mirror and Span's X Y Z). `iconOnly` (Minimal density)
// hides the caption and the badge; the tooltip names the button then.
// `horizontal` puts the caption beside the glyph (Minimal's Scale buttons).
// `available` false greys it out but leaves it pressable and its tooltip
// shown -- the tooltip says why (the inspector's buttons).
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
    property bool horizontal: false
    property bool available: true

    implicitWidth: 62
    implicitHeight: Theme.toolStrip
    /* one of a group that is a single Tab stop (RovingFocus): Tab stops on
     * the group's chosen button alone, the arrows move between them. The
     * focused one keeps its stop: Qt refuses to take it from the focus item,
     * which `stop` can name another for a moment while a press runs. */
    property RovingFocus roving: null
    focusPolicy: roving === null || roving.stop === root || activeFocus ? Qt.StrongFocus : Qt.ClickFocus
    Component.onCompleted: if (roving !== null) roving.join(root)
    Component.onDestruction: if (roving !== null) roving.leave(root)
    onActiveFocusChanged: if (activeFocus && roving !== null) roving.current = root
    Keys.onPressed: (event) => {
        if (roving !== null && (event.modifiers & ~Qt.KeypadModifier) === Qt.NoModifier
                && roving.navigate(root, event.key))
            event.accepted = true
    }
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
        border.color: root.visualFocus || root.ring || root.on ? Theme.accent
                    : (root.hovered && root.available ? Theme.line2 : Theme.line)
    }
    contentItem: Item {
        Grid {
            anchors.centerIn: parent
            columns: root.horizontal ? 2 : 1
            rowSpacing: 2
            columnSpacing: 6
            horizontalItemAlignment: Grid.AlignHCenter
            verticalItemAlignment: Grid.AlignVCenter
            Glyph {
                name: root.glyph
                size: root.glyphSize
            }
            Text {
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
    opacity: enabled && available ? 1 : 0.4
}
