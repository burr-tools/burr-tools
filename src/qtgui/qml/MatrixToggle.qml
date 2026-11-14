import QtQuick
import BurrTools.Ui

// PR-26 Matrix toggle: a cell of the colour rules, 30 x 24 as a layer chip,
// radius 6, 1 dp line2; ticked: the accent fill with a white check, so a state is a mark and
// not a hue. The matrix around it is one Tab stop and moves `cursor`.
Rectangle {
    id: root
    property bool on: false
    property bool cursor: false      // the keyboard's cell
    property bool hovered: hover.hovered
    // the pointer has the highlight: false while the keys hold it, so the hover mark
    // does not show beside the keyboard's (as the voxel editor's 2D grid)
    property bool pointerShown: true

    signal toggled()

    implicitWidth: 30
    implicitHeight: 24
    radius: Theme.radiusChip
    color: on ? Theme.accent : Theme.panel
    border.width: cursor ? 2 : 1
    border.color: cursor ? Theme.text : hover.hovered && pointerShown ? Theme.accent : on ? Theme.accent : Theme.controlLine

    HoverHandler { id: hover }
    TapHandler { onTapped: root.toggled() }

    Icon {
        anchors.centerIn: parent
        visible: root.on
        name: "check"
        size: 14
        color: Theme.onAccent
    }
}
