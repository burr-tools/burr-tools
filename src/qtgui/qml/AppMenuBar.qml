import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// The application menu bar, built from the command table. On macOS it is
// the system menu bar at the top of the screen (Qt moves Settings, About and
// Quit into the BurrTools menu as Preferences…, About BurrTools and Quit).
// Elsewhere it is drawn in the theme, its menus in windows of their own
// (AppMenu), and View ▸ Show menu bar can put it away behind the rail's menu
// button (WorkspaceRail).
MenuBar {
    id: root
    objectName: "shell.menubar"

    background: Rectangle {
        implicitHeight: 30
        color: Theme.panel
        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 1
            color: Theme.line
        }
    }

    // "&File" keeps its Alt+F mnemonic in the item's text; the label shows the
    // underline only while the keyboard drives the menus (KeyboardCues)
    function accessKeyText(label, show) {
        const i = label.search(/&[^&]/)
        const plain = label.replace(/&(.)/g, "$1")
        if (!show || i < 0)
            return plain
        return plain.substring(0, i) + "<u>" + plain.charAt(i) + "</u>" + plain.substring(i + 1)
    }

    delegate: MenuBarItem {
        id: barItem
        implicitHeight: 30
        leftPadding: 10
        rightPadding: 10
        contentItem: Text {
            text: root.accessKeyText(barItem.text, App.keyboardCues.showAccessKeys)
            textFormat: Text.StyledText
            color: Theme.text
            font.pixelSize: Theme.fontBody
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: Theme.radiusChip
            color: barItem.highlighted ? Theme.panel2 : "transparent"
        }
    }

    Component {
        id: menuComponent
        CommandMenu {}
    }

    Component.onCompleted: {
        for (const m of App.commands.menuBar())
            root.addMenu(menuComponent.createObject(root, { menuKey: m.key, title: m.label }))
    }
}
