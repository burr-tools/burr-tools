import QtQuick
import BurrTools.Ui

// C10 status bar: status text (or a flash message) and the cursor readout.
// Text only -- the legacy view toggles moved to the Display menu.
//
// Wireframe (each number marks its code below):
//
//   ┌─────────────────────────────────────────────────────────────┐
//   │ ① Shape S1 has 512 voxels (296 fixed, 216 variable)  ② x y z │
//   └─────────────────────────────────────────────────────────────┘
Rectangle {
    id: root
    height: Theme.statusBar
    color: Theme.panel

    Rectangle {
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 1
        color: Theme.line
    }

    Text { // ①
        objectName: "status.text"
        anchors { left: parent.left; leftMargin: 14; right: cursor.left; rightMargin: 14; verticalCenter: parent.verticalCenter }
        // the emphasised parts (shape id, counts) in text colour, the rest muted
        textFormat: Text.StyledText
        text: App.status.text.replace(/<b>/g, "<b><font color='" + Theme.text + "'>").replace(/<\/b>/g, "</font></b>")
        color: Theme.muted
        font.pixelSize: Theme.fontSecondary
        elide: Text.ElideRight
    }
    Text { // ②
        id: cursor
        objectName: "status.cursor"
        anchors { right: parent.right; rightMargin: 14; verticalCenter: parent.verticalCenter }
        text: App.status.cursorText
        color: Theme.muted
        font.pixelSize: Theme.fontSecondary
    }
}
