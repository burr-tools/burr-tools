import QtQuick
import BurrTools.Ui

// The DA column's mark (C20): what is known of a solution's disassembly, as
// a shape, not a word -- stored (a tick), level only (a half disc), cannot
// be disassembled (a cross), not checked (a dot). Its tooltip and the row's
// accessible name say it in words. `da` is btui::DaState.
Item {
    id: root
    property int da: 3
    readonly property list<string> words: [ qsTr("Disassembly stored"), qsTr("Level only"),
                                            qsTr("Cannot be disassembled"), qsTr("Not checked") ]
    implicitWidth: 30
    implicitHeight: 26
    Accessible.ignored: true

    Icon {
        anchors.centerIn: parent
        visible: root.da === 0 || root.da === 2
        name: root.da === 0 ? "check" : "close"
        size: 14
        color: root.da === 0 ? Theme.ok : Theme.danger
    }
    // level only: a ring, its left half filled
    Rectangle {
        id: ring
        anchors.centerIn: parent
        visible: root.da === 1
        width: 12
        height: 12
        radius: 6
        color: "transparent"
        border.width: 1.5
        border.color: Theme.accent
        Item {
            anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
            width: parent.width / 2
            clip: true
            Rectangle { width: ring.width; height: ring.height; radius: 6; color: Theme.accent }
        }
    }
    Rectangle {
        anchors.centerIn: parent
        visible: root.da === 3
        width: 4
        height: 4
        radius: 2
        color: Theme.muted
    }
    HoverHandler { id: hover }
    BtToolTip { shown: hover.hovered; text: root.words[Math.max(0, Math.min(3, root.da))] }
}
