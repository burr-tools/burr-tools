import QtQuick
import BurrTools.Ui

// A side card whose content arrives in a later phase (the Solver).
Card {
    id: root
    property string note
    collapseObjectName: objectName + ".collapse"     // "solver.left.collapse", ...

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
