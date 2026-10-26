import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// Delete a shape whose problems have solutions (C01, product decision
// 2026-10-27): undo brings the shape back, not the solutions, so the delete
// asks first -- only then (btui::deleteLosesSolutions). Cancel is the
// default; Delete is the danger button.
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
    objectName: "entities.shapes.deleteDialog"
    property int row: -1
    property string idText
    title: qsTr("Delete %1?").arg(idText)
    defaultButton: cancelButton
    width: 460

    function ask(r: int, id: string) {
        row = r
        idText = id
        open()
    }

    contentItem: Text { // ①
        objectName: "entities.shapes.deleteDialog.text"
        wrapMode: Text.WordWrap
        color: Theme.text
        font.pixelSize: Theme.fontBody
        lineHeight: 1.35
        text: qsTr("A problem that uses %1 has solutions or a search in progress. Deleting %1 removes it from those problems and deletes their solutions that use it. Undo brings the shape back, but not the solutions.").arg(root.idText)
    }

    footer: Item { // ②
        implicitHeight: 60
        Row {
            anchors { right: parent.right; rightMargin: 20; verticalCenter: parent.verticalCenter }
            spacing: 8
            BtButton {
                id: cancelButton
                objectName: "entities.shapes.deleteDialog.cancel"
                text: qsTr("Cancel")
                onClicked: root.close()
            }
            BtButton {
                objectName: "entities.shapes.deleteDialog.delete"
                text: qsTr("Delete")
                danger: true
                onClicked: {
                    root.close()
                    App.shapes.deleteShape(root.row)
                }
            }
        }
    }
}
