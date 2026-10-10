import QtQuick
import BurrTools.Ui

// A side card whose content arrives in a later phase (Puzzle, Solver).
Card {
    id: root
    property string note

    Text {
        anchors.centerIn: parent
        width: parent.width - 40
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: root.note
        color: Theme.muted
        font.pixelSize: Theme.fontSecondary
    }
}
