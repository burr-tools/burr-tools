import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// A question before a change undo cannot fully take back: the session's
// warning that edits remove solutions (DocumentController::mayCostSolutions).
// Cancel is the default; the change is the danger button. Its buttons are
// named after the dialog: <objectName>.cancel, .confirm.
//
// Wireframe (each number marks its code below):
//
//   ┌ Changes remove stored solutions ──────────┐
//   │ ① This change removes the stored solutions│
//   │   of the problems it touches ...          │
//   ├───────────────────────────────────────────┤
//   │ ②                   [Cancel] [Continue]   │
//   └───────────────────────────────────────────┘
BtDialog {
    id: root
    property string text
    property string confirmText: qsTr("Delete")
    signal confirmed()

    defaultButton: cancelButton
    width: 460

    function ask(title: string, text: string, confirmText: string) {
        root.title = title
        root.text = text
        root.confirmText = confirmText
        open()
    }

    contentItem: Text { // ①
        objectName: root.objectName + ".text"
        wrapMode: Text.WordWrap
        color: Theme.text
        font.pixelSize: Theme.fontBody
        lineHeight: 1.35
        text: root.text
        textFormat: Text.PlainText
    }

    footer: Item { // ②
        implicitHeight: 60
        Row {
            anchors { right: parent.right; rightMargin: 20; verticalCenter: parent.verticalCenter }
            spacing: 8
            BtButton {
                id: cancelButton
                objectName: root.objectName + ".cancel"
                text: qsTr("Cancel")
                onClicked: root.close()
            }
            BtButton {
                objectName: root.objectName + ".confirm"
                text: root.confirmText
                danger: true
                // the answer first: closing is what Cancel and Esc do, and a
                // handler of closed must find it answered
                onClicked: {
                    root.confirmed()
                    root.close()
                }
            }
        }
    }
}
