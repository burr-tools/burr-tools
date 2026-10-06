import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// Edit comment (legacy multiLineWindow_c "Change the comment for the current
// puzzle"), restyled. OK writes the comment; Cancel or Esc discards edits.
BtDialog {
    id: root
    objectName: "shell.comment.dialog"
    title: qsTr("Edit comment")
    width: 560
    height: 420

    onAboutToShow: editor.text = App.document.comment
    onAccepted: App.document.setComment(editor.text)

    contentItem: Column {
        spacing: 8
        Text {
            text: qsTr("Change the comment for the current puzzle")
            color: Theme.muted
            font.pixelSize: Theme.fontSecondary
        }
        ScrollView {
            width: parent.width
            height: parent.height - 30
            TextArea {
                id: editor
                objectName: "shell.comment.text"
                wrapMode: TextArea.Wrap
                color: Theme.text
                font.pixelSize: Theme.fontBody
                selectByMouse: true
                background: Rectangle {
                    color: Theme.panel
                    radius: Theme.radiusControl
                    border.color: editor.activeFocus ? Theme.accent : Theme.line2
                }
            }
        }
    }

    footer: Item {
        implicitHeight: 60
        Row {
            anchors { right: parent.right; rightMargin: 20; verticalCenter: parent.verticalCenter }
            spacing: 8
            BtButton { objectName: "shell.comment.cancel"; text: qsTr("Cancel"); onClicked: root.reject() }
            BtButton { objectName: "shell.comment.ok"; text: qsTr("OK"); primary: true; onClicked: root.accept() }
        }
    }
}
