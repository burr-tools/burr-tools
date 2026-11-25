import QtQuick
import BurrTools.Ui

// PR-08 Chip: a shape's chip -- its id (S#) on its colour, both by position
// -- and the two badges that go with it: the count in a card header, and a
// shape's weight (Wn).
//
//   Shape:  h26, min 34 wide (Header: 28), radius 6, the shape's colour; the
//           text black or white as btui::prefersWhiteText says (the model's
//           chipTextColor), not the spec's always-white with a shadow
//   Count:  h20, muted, a pill
//   Weight: h18, panel2 on a 1 dp line, radius 5, muted text
//
// Read-only: a chip names something, its row or button does the work, so
// the chip's text is not read out on its own (Accessible.ignored).
Rectangle {
    id: root

    enum Kind { Shape, Header, Count, Weight }
    property int kind: Chip.Shape
    property string text
    property color fill: Theme.muted            // Shape, Header: the shape's colour
    property color textColor: Theme.panel       // Shape, Header: the model's chipTextColor

    readonly property bool isShape: kind === Chip.Shape || kind === Chip.Header
    readonly property real minWidth: kind === Chip.Shape ? 34 : kind === Chip.Header ? 28 : kind === Chip.Count ? 24 : 0

    implicitHeight: isShape ? 26 : kind === Chip.Count ? 20 : 18
    implicitWidth: Math.max(minWidth, label.implicitWidth + (kind === Chip.Weight ? 10 : 12))
    radius: isShape ? Theme.radiusChip : kind === Chip.Count ? height / 2 : 5
    color: isShape ? fill : kind === Chip.Count ? Theme.muted : Theme.panel2
    border.width: kind === Chip.Weight ? 1 : 0
    border.color: Theme.line

    Text {
        id: label
        objectName: "text"
        Accessible.ignored: true
        anchors.centerIn: parent
        width: Math.min(implicitWidth, root.width - 4)    // a narrow chip (the rail's) elides
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        text: root.text
        color: root.isShape ? root.textColor : root.kind === Chip.Count ? Theme.panel : Theme.muted
        font.pixelSize: Theme.fontMicro
        font.weight: root.kind === Chip.Count ? Font.DemiBold : Font.Bold
    }
}
