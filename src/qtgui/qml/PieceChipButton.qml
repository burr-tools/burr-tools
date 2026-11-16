import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// PR-30 Piece-display chip (C20): a piece of the shown assembly, its state
// told by the chip's look and not by words -- solid: a filled swatch;
// wireframe: a dotted border in the piece's colour and a hollow dotted
// swatch; hidden: the chip at 42 % opacity, in grey, with a hollow dashed
// swatch (PR-30). The keyboard's ring is drawn at full strength outside it,
// and the name stays in the tooltip and the accessible name. A click, Space or
// Enter cycles solid -> wireframe -> hidden. Qt Quick has no dashed border
// without Qt Quick Shapes (not in the build): the dots and dashes are drawn
// on a Canvas, again when the colour, the size or the theme changes.
AbstractButton {
    id: root
    property color colour: Theme.muted
    property int pieceState: 0           // btui::PieceState: 0 solid, 1 wireframe, 2 hidden
    property string toolTipText

    readonly property bool wire: pieceState === 1
    readonly property bool hidden: pieceState === 2

    implicitHeight: Theme.pieceChip
    implicitWidth: label.implicitWidth + 16 + 6 + 20
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.description: toolTipText
    // its keys before a window shortcut takes them (Space, the arrows)
    Keys.onShortcutOverride: (event) => event.accepted = event.modifiers === Qt.NoModifier
                             && (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                                 || event.key === Qt.Key_Left || event.key === Qt.Key_Right)
    Keys.onSpacePressed: clicked()
    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()

    function repaint(): void {
        edge.requestPaint()
        hollow.requestPaint()
    }
    onColourChanged: repaint()
    onPieceStateChanged: repaint()
    onHeightChanged: repaint()
    Connections {
        target: Theme
        function onChanged() { root.repaint() }
    }

    BtToolTip { shown: root.hovered || root.visualFocus; text: root.toolTipText }

    background: Item {
        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusControl
            color: root.hovered ? Theme.panel2 : Theme.panel
            border.width: root.wire ? 0 : 1.5
            border.color: root.hidden ? Theme.muted : Theme.line2
            opacity: root.hidden ? 0.42 : 1
        }
        // the dotted edge in the piece's colour (wireframe)
        Canvas {
            id: edge
            anchors.fill: parent
            visible: root.wire
            onPaint: {
                const g = getContext("2d")
                g.reset()
                if (!root.wire)
                    return
                g.strokeStyle = root.colour
                g.lineWidth = 1.5
                g.setLineDash([ 1.5, 2.5 ])
                const r = Theme.radiusControl
                g.beginPath()
                g.roundedRect(0.75, 0.75, width - 1.5, height - 1.5, r, r)
                g.stroke()
            }
        }
        // the keyboard's ring: outside, at full strength, whatever the state
        Rectangle {
            anchors { fill: parent; margins: -3 }
            visible: root.visualFocus
            radius: Theme.radiusControl + 3
            color: "transparent"
            border.width: 2
            border.color: Theme.accent
        }
    }

    contentItem: Item {
        opacity: root.hidden ? 0.42 : 1
        // the swatch: filled, or hollow and dotted / dashed
        Rectangle {
            id: swatch
            anchors { left: parent.left; leftMargin: 4; verticalCenter: parent.verticalCenter }
            width: 16
            height: 16
            radius: 4
            color: root.pieceState === 0 ? root.colour : "transparent"
            Canvas {
                id: hollow
                anchors.fill: parent
                visible: root.pieceState !== 0
                onPaint: {
                    const g = getContext("2d")
                    g.reset()
                    if (root.pieceState === 0)
                        return
                    g.strokeStyle = root.hidden ? Theme.muted : root.colour
                    g.lineWidth = 1.5
                    g.setLineDash(root.hidden ? [ 3, 2 ] : [ 1.5, 2 ])
                    g.beginPath()
                    g.roundedRect(0.75, 0.75, width - 1.5, height - 1.5, 4, 4)
                    g.stroke()
                }
            }
        }
        Text {
            id: label
            anchors { left: swatch.right; leftMargin: 6; verticalCenter: parent.verticalCenter }
            text: root.text
            color: root.hidden ? Theme.muted : Theme.text
            font.pixelSize: Theme.fontSecondary
            font.weight: Font.DemiBold
            textFormat: Text.PlainText
        }
    }
}
