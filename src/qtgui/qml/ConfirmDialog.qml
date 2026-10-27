import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// A question before a change undo cannot fully take back: deleting a shape
// or removing a colour that solutions depend on (C01, C09; product decision
// 2026-10-27). Cancel is the default; the change is the danger button. Its
// buttons are named after the dialog: <objectName>.cancel, .confirm.
//
// Wireframe (each number marks its code below):
//
//   ┌ Delete S3? ───────────────────────────────┐
//   │ ① A problem that uses S3 has solutions    │
//   │   or a search in progress. ...            │
//   ├───────────────────────────────────────────┤
//   │ ②                     [Cancel] [Delete]   │
//   └───────────────────────────────────────────┘
BtDialog {
    id: root
    property string text
    property string confirmText: qsTr("Delete")
    property int subject: -1           // what it asks about: a shape's row, a colour's id
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
                onClicked: {
                    root.close()
                    root.confirmed()
                }
            }
        }
    }
}
