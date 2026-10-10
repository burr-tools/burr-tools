import QtQuick
import BurrTools.Ui

// C15 workspace rail: Entities, Puzzle, Solver -- and, while View ▸ Show
// menu bar is off, a menu button above them that opens every menu.
//
// Wireframe (each number marks its code below):
//
//   ┌────────┐
//   │ ① ☰    │ ─▶ ② every menu   (while the menu bar is hidden)
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
            onClicked: allMenus.visible ? allMenus.close() : allMenus.popup(menuButton, menuButton.width + 6, 0)

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
