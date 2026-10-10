import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// One menu of the command table ("file", "edit", ...): its items, dividers,
// check marks and shortcuts, enabled as the commands are. The menu bar and
// the rail's menu button both build theirs from it.
AppMenu {
    id: root
    property string menuKey
    objectName: "shell.menu." + menuKey

    Component {
        id: actionComponent
        Action {
            property string key
            property string shortcutText
            onTriggered: {
                App.keyboardCues.reset()
                App.commands.trigger(key)
            }
        }
    }
    Component {
        id: separatorComponent
        MenuSeparator {
            topPadding: 3
            bottomPadding: 3
            contentItem: Rectangle { implicitHeight: 1; color: Theme.line }
        }
    }

    Component.onCompleted: {
        for (const it of App.commands.menuItems(menuKey)) {
            if (it.separatorBefore && count > 0)
                addItem(separatorComponent.createObject(contentItem))
            const key = it.key
            const action = actionComponent.createObject(root, {
                key: key, text: it.label, shortcut: it.shortcut, shortcutText: it.shortcutText,
                checkable: it.checkable
            })
            // App.commands.blocked: a dialog is open (Main.qml)
            action.enabled = Qt.binding(() => !App.commands.blocked && App.commands.revision >= 0
                                              && App.commands.isEnabled(key))
            if (it.checkable)
                action.checked = Qt.binding(() => App.commands.revision >= 0 && App.commands.isChecked(key))
            addAction(action)
            itemAt(count - 1).objectName = "shell.menuitem." + key
        }
    }
}
