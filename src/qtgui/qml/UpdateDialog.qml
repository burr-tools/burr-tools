import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import BurrTools.Ui

// Help ▸ Check for updates, or the check at launch (UpdateController): a
// newer release, its notes as GitHub has them (plain text, as legacy's
// updateWindow_c shows them), and what to do -- open its page, be reminded
// later, or, after a launch check only, skip this version. A build whose
// version cannot be compared gets the releases page instead, no notes.
//
// Wireframe (each number marks its code below):
//
//   ┌ BurrTools 0.8.1 is available ─────────────────────────────┐
//   │ ① BurrTools 0.8.1 is available. You have 0.8.0.           │
//   │ ② ┌───────────────────────────────────────────────────┐   │
//   │   │ release notes, scrolling                          │   │
//   │   │                                                   │   │
//   │   └───────────────────────────────────────────────────┘   │
//   ├───────────────────────────────────────────────────────────┤
//   │ ③ [Skip this version]   [Remind me later] [Open page…]    │
//   └───────────────────────────────────────────────────────────┘
BtDialog {
    id: root
    objectName: "help.update.dialog"
    defaultButton: openButton
    title: qsTr("Check for updates")
    width: Math.min(620, (parent ? parent.width : 700) - 32)

    property string heading
    property string notes
    property string url
    property bool canSkip: false

    function show(heading: string, notes: string, url: string, canSkip: bool) {
        root.heading = heading
        root.notes = notes
        root.url = url
        root.canSkip = canSkip
        open()
    }

    contentItem: ColumnLayout {
        spacing: 12
        Text { // ①
            objectName: "help.update.heading"
            Layout.fillWidth: true
            text: root.heading
            wrapMode: Text.WordWrap
            color: Theme.text
            font.pixelSize: Theme.fontBody
            font.weight: Font.DemiBold
        }
        ScrollView { // ②
            visible: root.notes.length > 0
            Layout.fillWidth: true
            Layout.preferredHeight: 260
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            TextArea {
                objectName: "help.update.notes"
                text: root.notes
                readOnly: true
                wrapMode: TextArea.Wrap
                textFormat: TextEdit.PlainText
                color: Theme.text
                font.pixelSize: Theme.fontSecondary
                selectByMouse: true
                Accessible.name: qsTr("Release notes")
                background: Rectangle {
                    color: Theme.panel2
                    radius: Theme.radiusControl
                    border.color: Theme.line
                }
            }
        }
    }

    footer: Item { // ③
        implicitHeight: 60
        BtButton {
            objectName: "help.update.skip"
            visible: root.canSkip
            anchors { left: parent.left; leftMargin: 20; verticalCenter: parent.verticalCenter }
            text: qsTr("Skip this version")
            onClicked: {
                App.updates.skipOffered()
                root.close()
            }
        }
        Row {
            anchors { right: parent.right; rightMargin: 20; verticalCenter: parent.verticalCenter }
            spacing: 8
            BtButton {
                objectName: "help.update.later"
                text: root.canSkip || root.notes.length > 0 ? qsTr("Remind me later") : qsTr("Cancel")
                onClicked: root.close()
            }
            BtButton {
                id: openButton
                objectName: "help.update.open"
                text: root.notes.length > 0 || root.canSkip ? qsTr("Open release page…") : qsTr("Open releases page…")
                primary: true
                onClicked: {
                    App.updates.openPage(root.url)
                    root.close()
                }
            }
        }
    }
}
