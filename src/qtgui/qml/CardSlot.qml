import QtQuick
import BurrTools.Ui

// A side column of the shell: it animates between a card and its rail, and
// the card inside does not. The card takes `cardWidth`, the wider of where
// the column is going and where it has been, so it is laid out once and the
// moving edge uncovers or covers it, instead of every frame reflowing what
// it holds. Set `target`; lay the card out against the column's fixed edge.
Item {
    id: root
    property real target: 0
    property real cardWidth: target
    readonly property bool animating: slide.running

    width: target
    clip: slide.running
    Behavior on width {
        NumberAnimation { id: slide; duration: Theme.durCollapse; easing.type: Easing.InOutQuad }
    }

    // wider at once; narrower only when the column has arrived -- which it
    // has already if it was sent back before it moved
    function settle(): void {
        if (target > cardWidth || Math.abs(width - target) < 0.5)
            cardWidth = target
    }
    onTargetChanged: settle()
    onWidthChanged: settle()
}
