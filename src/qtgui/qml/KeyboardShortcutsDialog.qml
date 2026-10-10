import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import BurrTools.Ui

// C22 Help ▸ Keyboard shortcuts (F1): the shortcuts grouped by where they
// act, searchable by action, key or group. The list comes from the command
// table (CommandController::shortcutHelp), so its keys are the ones that
// really work, written the way this platform writes them. The groups of the
// later phases (drawing tools, shapes list, Puzzle, Solver) join when those
// features do.
//
// Wireframe (each number marks its code below):
//
//   ┌ Keyboard shortcuts ───────────────────────────────────┐
//   │ ① Grouped by where they act. ...   ② [ Search      ] │
//   │ ┌───────────────────────────────────────────────────┐ │
//   │ │ ③ FILE                                            │ │   a group
//   │ │ ④ New puzzle                           [Ctrl][N]  │ │   a row
//   │ │   Open…                                [Ctrl][O]  │ │
//   │ │   VIEW ...                                        │ │
//   │ │ ⑤ No shortcuts match “…”   (when none)            │ │
//   │ └───────────────────────────────────────────────────┘ │
//   ├───────────────────────────────────────────────────────┤
//   │ ⑥                                          [Close]    │
//   └───────────────────────────────────────────────────────┘
BtDialog {
    id: root
    objectName: "help.shortcuts.dialog"
    title: qsTr("Keyboard shortcuts")
    width: Math.min(720, (parent ? parent.width : 800) - 32)
    height: Math.min(560, (parent ? parent.height : 700) - 32)

    property var groups: []
    property string query: ""

    onAboutToShow: {
        groups = App.commands.shortcutHelp()
        search.text = ""
    }
    onOpened: search.forceActiveFocus()

    function keyText(keys: var): string {
        return keys.map(a => a.map(p => p.text).join("+")).join(" / ")
    }

    function matches(group: var, row: var): bool {
        const q = root.query.trim().toLowerCase()
        if (q.length === 0)
            return true
        return row.action.toLowerCase().indexOf(q) >= 0
            || keyText(row.keys).toLowerCase().indexOf(q) >= 0
            || group.title.toLowerCase().indexOf(q) >= 0
            || (group.sub || "").toLowerCase().indexOf(q) >= 0
    }

    contentItem: ColumnLayout {
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Text { // ①
                Layout.fillWidth: true
                text: qsTr("Grouped by where they act. Keys act when focus is not in a text field; menu shortcuts (File, Edit, …) are shown in the menus.")
                color: Theme.muted
                font.pixelSize: Theme.fontSecondary
                wrapMode: Text.WordWrap
            }
            SearchField { // ②
                id: search
                objectName: "help.shortcuts.search"
                implicitWidth: 220
                placeholderText: qsTr("Search shortcuts")
                onTextChanged: root.query = text
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            // nothing to scroll sideways; an idle horizontal bar would still take the
            // clicks in a 10 px strip along the bottom
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            Column {
                width: parent.width
                spacing: 16

                Repeater { // ③
                    model: root.groups
                    delegate: Column {
                        id: grp
                        required property var modelData
                        readonly property var shown: modelData.rows.filter(r => root.matches(modelData, r))
                        width: parent.width
                        spacing: 0
                        visible: shown.length > 0

                        Row {
                            spacing: 8
                            bottomPadding: 4
                            Text {
                                text: grp.modelData.title.toUpperCase()
                                color: Theme.muted
                                font.pixelSize: Theme.fontMicro
                                font.weight: Font.DemiBold
                                font.letterSpacing: 0.5
                            }
                            Text {
                                visible: (grp.modelData.sub || "").length > 0
                                text: grp.modelData.sub || ""
                                color: Theme.muted
                                font.pixelSize: Theme.fontMicro
                            }
                        }
                        Repeater {
                            model: grp.shown
                            delegate: Item { // ④
                                id: shortcutRow
                                required property var modelData
                                width: grp.width
                                implicitHeight: Math.max(action.implicitHeight, keys.implicitHeight) + 14
                                Text {
                                    id: action
                                    anchors { left: parent.left; right: keys.left; rightMargin: 16; verticalCenter: parent.verticalCenter }
                                    text: shortcutRow.modelData.action
                                    color: Theme.text
                                    font.pixelSize: Theme.fontBody
                                    wrapMode: Text.WordWrap
                                }
                                ShortcutKeys {
                                    id: keys
                                    anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                                    keys: shortcutRow.modelData.keys
                                }
                                Rectangle {
                                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                                    height: 1
                                    color: Theme.line
                                }
                            }
                        }
                    }
                }

                Text { // ⑤
                    objectName: "help.shortcuts.empty"
                    visible: root.groups.every(g => g.rows.filter(r => root.matches(g, r)).length === 0)
                    text: qsTr("No shortcuts match “%1”").arg(root.query)
                    color: Theme.muted
                    font.pixelSize: Theme.fontBody
                }
            }
        }
    }

    footer: Item { // ⑥
        implicitHeight: 56
        BtButton {
            objectName: "help.shortcuts.close"
            anchors { right: parent.right; rightMargin: 20; verticalCenter: parent.verticalCenter }
            text: qsTr("Close")
            primary: true
            onClicked: root.close()
        }
    }
}
