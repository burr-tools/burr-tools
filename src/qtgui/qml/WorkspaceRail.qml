import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// C15 workspace rail: Entities, Puzzle, Solver -- and, while View ▸ Show
// menu bar is off, a menu button above them that opens every menu.
//
// Wireframe (each number marks its code below):
//
//   ┌────────┐
//   │ ① ☰    │ ─▶ ② every menu   (while the menu bar is hidden; Alt opens it too)
//   │  Menu  │
//   │ ────── │
//   │ ③ ⬡    │
//   │Entities│
//   │ ④ ✣    │
//   │ Puzzle │
//   │ ⑤ ▷    │
//   │ Solver │
//   │        │
//   └────────┘
Rectangle {
    id: root
    objectName: "app-rail"
    width: Theme.workspaceRail
    color: Theme.panel

    Rectangle {
        anchors { top: parent.top; bottom: parent.bottom; right: parent.right }
        width: 1
        color: Theme.line
    }

    Column {
        anchors { top: parent.top; topMargin: 10; horizontalCenter: parent.horizontalCenter }
        spacing: 6

        RailButton { // ①
            id: menuButton
            objectName: "rail-menu"
            visible: App.commands.menuBarHideable && !App.settings.showMenuBar
            text: qsTr("Menu")
            iconName: "menu"
            tip: qsTr("All menus")
            selected: allMenus.visible
            tab: false
            onClicked: allMenus.visible ? closeMenus() : openMenus()

            // Alt reaches these menus by keyboard while the menu bar is hidden
            // (KeyboardCues): a tap opens this menu, a menu's access key its
            // submenu, Esc or another tap closes it
            function openMenus() {
                if (!allMenus.visible)
                    allMenus.popup(menuButton, menuButton.width + 6, 0)
            }
            // a menu's submenu, opening this menu first if it is closed: once
            // its window is up (PopupWindowStyle.whenExposed says why)
            function openSubmenu(index: int) {
                openMenus()
                const item = allMenus.itemAt(index) as MenuItem
                if (item)
                    App.popupStyle.whenExposed(item, () => {
                        if (allMenus.opened)
                            item.triggered()
                    })
            }
            // its submenus, then itself: they are windows of their own, and a
            // submenu would outlive its parent
            function closeMenus() {
                for (let i = 0; i < allMenus.count; i++) {
                    const m = allMenus.menuAt(i)
                    if (m)
                        m.close()
                }
                allMenus.close()
            }
            // leaving the application (another program, a click elsewhere on
            // the desktop) puts them away, as a menu bar's menus go. The
            // application's state, not the window's: a menu in a window of its
            // own may take the activation from the main window
            Connections {
                target: Qt.application
                function onStateChanged() {
                    if (Qt.application.state !== Qt.ApplicationActive)
                        menuButton.closeMenus()
                }
            }
            Connections {
                target: App.keyboardCues
                enabled: menuButton.visible
                function onMenuArmedChanged() {
                    if (!App.keyboardCues.menuArmed)
                        return
                    // a tap while they are open closes them, as on Windows
                    if (allMenus.visible) {
                        menuButton.closeMenus()
                        App.keyboardCues.reset()
                    } else {
                        menuButton.openMenus()
                    }
                }
                function onMenuKeyPressed(index: int) { menuButton.openSubmenu(index) }
                function onMenusDismissed() { menuButton.closeMenus() }
                // a click in the window outside them (on the button itself, the
                // button's own click toggles them)
                function onWindowPressed() {
                    if (allMenus.visible && !menuButton.hovered)
                        menuButton.closeMenus()
                }
            }

            AppMenu { // ②
                id: allMenus
                objectName: "rail-menu.popup"
                Instantiator {
                    model: App.commands.menuBar()
                    delegate: CommandMenu {
                        required property var modelData
                        menuKey: modelData.key
                        title: modelData.label
                        objectName: "rail-menu." + modelData.key
                    }
                    onObjectAdded: (index, object) => allMenus.insertMenu(index, object)
                    onObjectRemoved: (index, object) => allMenus.removeMenu(object)
                }
            }
        }
        Rectangle {
            visible: menuButton.visible
            width: menuButton.width - 12
            height: 1
            anchors.horizontalCenter: parent.horizontalCenter
            color: Theme.line
        }

        RailButton { // ③
            objectName: "rail-entities"
            text: qsTr("Entities")
            iconName: "rail-entities"
            tip: qsTr("Entities — create and edit pieces")
            selected: App.layout.workspace === LayoutController.Entities
            onClicked: App.layout.workspace = LayoutController.Entities
        }
        RailButton { // ④
            objectName: "rail-puzzle"
            text: qsTr("Puzzle")
            iconName: "rail-puzzle"
            tip: qsTr("Puzzle — choose the result, the pieces and the rules")
            selected: App.layout.workspace === LayoutController.Puzzle
            onClicked: App.layout.workspace = LayoutController.Puzzle
        }
        RailButton { // ⑤
            objectName: "rail-solver"
            text: qsTr("Solver")
            iconName: "rail-solver"
            tip: qsTr("Solver — run and browse solutions")
            selected: App.layout.workspace === LayoutController.Solver
            onClicked: App.layout.workspace = LayoutController.Solver
        }
    }
}
