pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import BurrTools.Ui

// C01 Shapes card: the puzzle's shapes, S1 ... SN by position, and what can
// be done to them -- new, rename, duplicate, weight, delete, reorder -- on
// the row (hover or selected), in its context menu, and by the list's keys
// (the command table's ShapesList scope). Every change is one undo step
// (App.shapes); a reorder or delete renumbers the shapes after it.
//
// Wireframe (each number marks its code below):
//
//   ┌──────────────────────────────────────────┐
//   │ Shapes ①(8)                 ②[+ New] [◧] │   title, count, New, collapse
//   ├──────────────────────────────────────────┤
//   │ ③┌──────────────────────────────────────┐│
//   │ ④│⑤⠿ ⑥[S1] ⑦Result ⑨W2  ⑩✎ ⧉ W− W+ 🗑  ││   hovered or selected: actions
//   │  └──────────────────────────────────────┘│
//   │    ⠿  [S2]  ⑧[Cage_______]              │   renaming: a field for the label
//   │    ...                                   │
//   │ ⑪ No shapes -- click + New (when empty)  │
//   └──────────────────────────────────────────┘
//   ⑫ the row's context menu (right-click, Menu key, Shift+F10)
//
//   Dragging a row: it follows the pointer, and the rows between its place
//   and the pointer slide over by one to open the gap it drops into.
Card {
    id: root
    objectName: "entities.shapes"
    title: qsTr("Shapes")
    collapseIcon: "panelL"
    collapseObjectName: "entities.shapes.collapse"
    onCollapseClicked: App.layout.leftCollapsed = true

    readonly property AppMenu contextMenu: shapeMenu      // for tests: a popup is no child item
    readonly property string weightTip: qsTr("in the Solver, heavier shapes stay in place and lighter ones move")
    // the keys the rows' tooltips name, worked out once for every row
    readonly property string renameKeys: App.commands.shortcutText("shapes.rename")
    readonly property string duplicateKeys: App.commands.shortcutText("shapes.duplicate")
    readonly property string deleteKeys: App.commands.shortcutText("shapes.delete")

    headerExtras: [
        Chip { // ①
            objectName: "entities.shapes.count"
            kind: Chip.Count
            text: App.shapes.count
            Accessible.role: Accessible.StaticText
            Accessible.name: qsTr("%1 shapes").arg(App.shapes.count)
        }
    ]
    headerActions: [
        BtButton { // ②
            id: newButton
            objectName: "entities.shapes.new"
            primary: true
            small: true
            text: qsTr("+ New")
            Accessible.name: qsTr("New shape")
            Accessible.description: newTip.text
            onClicked: App.shapes.addShape()
            BtToolTip {
                id: newTip
                shown: newButton.hovered || newButton.visualFocus
                text: qsTr("New shape: empty, as big as the selected one, added at the end")
            }
        }
    ]

    // W− and W+: text buttons, 30 x 24 (C01)
    component WeightButton: AbstractButton {
        id: wb
        property string tip
        implicitWidth: 30
        implicitHeight: 24
        focusPolicy: Qt.NoFocus          // the list is the one Tab stop; its menu has these
        hoverEnabled: true
        property string spoken           // the name read out: "Increase weight", not "W+"
        Accessible.name: spoken
        Accessible.description: tip
        BtToolTip { shown: wb.hovered; text: wb.tip }
        background: Rectangle {
            radius: Theme.radiusChip
            color: wb.hovered ? Theme.panel2 : "transparent"
        }
        contentItem: Text {
            text: wb.text
            Accessible.ignored: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: wb.hovered ? Theme.text : Theme.muted
            font.pixelSize: Theme.fontMicro
            font.weight: Font.Bold
        }
        opacity: enabled ? 1 : 0.4
    }

    // a row's icon actions: 24 x 24, no Tab stop (the list is one)
    component RowIconButton: IconButton {
        implicitWidth: 24
        implicitHeight: 24
        iconSize: 16
        focusPolicy: Qt.NoFocus
    }

    Connections {
        target: App.shapes
        function onRenameRequested(row: int) { list.startRename(row) }
        // the menu names one row: when the rows change under it, it goes
        function onCountChanged() { shapeMenu.close() }
    }
    Connections {
        target: App.document
        function onHistoryApplied() { shapeMenu.close() }
        function onDocumentReplaced() { shapeMenu.close() }
    }

    ListView { // ③
        id: list
        objectName: "entities.shapes.list"
        anchors { fill: parent; margins: 6 }
        clip: true
        spacing: 2
        model: App.shapes
        currentIndex: App.shapes.selected
        activeFocusOnTab: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        Accessible.role: Accessible.List
        Accessible.name: qsTr("Shapes")

        readonly property real pitch: Theme.listRow + spacing
        property int editingRow: -1      // the row whose label is being edited
        property int dragFrom: -1        // the row being dragged
        property int dropAt: -1          // where it would go: before this row (count: at the end)
        property bool dragCancelled: false
        // the position the dragged row takes if dropped now, -1 for none
        readonly property int dropTo: dropAt < 0 || dragFrom < 0 ? -1 : (dropAt > dragFrom ? dropAt - 1 : dropAt)
        property bool settling: false    // a drop is being applied: rows jump, not slide

        // --- rename ------------------------------------------------------
        function startRename(row: int) {
            if (row < 0 || row >= count)
                return
            App.shapes.select(row)
            positionViewAtIndex(row, ListView.Contain)
            editingRow = row
        }
        function finishRename(refocus: bool) {
            editingRow = -1
            if (refocus)
                forceActiveFocus()
        }

        // --- drag to reorder (y in the list's content coordinates) ------
        function beginDrag(row: int) {
            if (editingRow >= 0)
                finishRename(false)          // a rename open commits first (its field loses focus)
            dragFrom = row
            dropAt = -1
            dragCancelled = false
            App.shapes.select(row)
            forceActiveFocus()               // so Esc reaches the list
        }
        function dragTo(y: real) {
            if (dragCancelled)
                return
            // before row k while over its upper half, after it over the lower
            dropAt = Math.max(0, Math.min(count, Math.floor((y - Theme.listRow / 2) / pitch) + 1))
        }
        function endDrag() {
            const from = dragFrom
            const to = dragCancelled ? -1 : dropTo
            // The rows already stand where the move puts them: their offsets go
            // in the same step as the move renumbers them, so nothing slides
            // back and forth on the drop.
            settling = true
            dragFrom = -1
            dropAt = -1
            if (from >= 0 && to >= 0 && to !== from)
                App.shapes.moveShape(from, to)
            settling = false
        }
        function cancelDrag() {
            dragCancelled = true
            dropAt = -1
        }

        // --- the context menu ---------------------------------------------
        function openMenu(row: int, at: Item, x: real, y: real) {
            if (row < 0 || row >= count)
                return
            App.shapes.select(row)
            shapeMenu.row = row
            shapeMenu.weight = App.shapes.weightOf(row)
            shapeMenu.idText = App.shapes.idTextOf(row)
            shapeMenu.popup(at, x, y)
        }
        function openMenuForSelection() {
            const row = App.shapes.selected
            const at = itemAtIndex(row)
            if (at !== null)
                openMenu(row, at, 40, at.height / 2)
        }

        // --- keys: the command table's, while the list has focus ---------
        Keys.onShortcutOverride: (event) => {
            if (list.editingRow >= 0)
                return
            if ((event.key === Qt.Key_Escape && list.dragFrom >= 0)
                    || App.commands.keyInScope("shapesList", event.key, event.modifiers) !== "")
                event.accepted = true        // the list's, not the window's shortcut
        }
        Keys.onPressed: (event) => {
            if (list.editingRow >= 0)
                return
            if (event.key === Qt.Key_Escape && list.dragFrom >= 0) {
                list.cancelDrag()
                event.accepted = true
            } else if (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier))) {
                list.openMenuForSelection()
                event.accepted = true
            } else if ((event.key === Qt.Key_Up || event.key === Qt.Key_Down)
                       && (event.modifiers & ~Qt.KeypadModifier) === Qt.NoModifier) {
                // here, not in onUp/DownPressed, which Keys calls before this
                // handler -- they would take Alt+arrows too
                App.shapes.selectBy(event.key === Qt.Key_Up ? -1 : 1)
                event.accepted = true
            } else {
                const key = App.commands.keyInScope("shapesList", event.key, event.modifiers)
                if (key !== "") {
                    App.commands.trigger(key)
                    event.accepted = true
                }
            }
        }

        delegate: Rectangle { // ④
            id: row
            required property int index
            required property string idText
            required property string label
            required property color chipColor
            required property color chipTextColor
            required property int weight
            readonly property bool selected: index === App.shapes.selected
            readonly property bool editing: list.editingRow === index
            readonly property bool dragged: list.dragFrom === index
            readonly property bool showActions: (hover.hovered || selected) && !editing && list.dragFrom < 0
            // while another row is dragged: one row up or down, to open the gap it would drop into
            readonly property real shift: {
                const from = list.dragFrom, to = list.dropTo
                if (from < 0 || to < 0 || dragged)
                    return 0
                if (from < to && index > from && index <= to)
                    return -list.pitch
                if (to < from && index >= to && index < from)
                    return list.pitch
                return 0
            }

            objectName: "entities.shapes.row." + index
            width: ListView.view.width
            height: Theme.listRow
            z: dragged ? 2 : 0
            opacity: dragged ? 0.6 : 1
            transform: Translate {
                // the dragged row follows the pointer; the others slide aside
                y: row.dragged ? rowDrag.activeTranslation.y : row.shift
                Behavior on y {
                    enabled: !row.dragged && !list.settling
                    NumberAnimation { duration: Theme.durReorder; easing.type: Easing.OutCubic }
                }
            }
            radius: Theme.radiusControl
            color: selected ? Theme.accentSoft : (hover.hovered ? Theme.panel2 : "transparent")
            // with the keyboard in the list, the selected row is ringed in the accent
            border.width: selected ? (list.activeFocus ? 2 : 1) : 0
            border.color: list.activeFocus ? Theme.accent : Theme.accentEdge
            Accessible.role: Accessible.ListItem
            Accessible.name: weight !== 1 ? qsTr("%1 %2, weight %3").arg(idText).arg(label).arg(weight) : idText + " " + label
            Accessible.selectable: true
            Accessible.selected: selected
            Accessible.onPressAction: App.shapes.select(row.index)

            HoverHandler { id: hover }
            TapHandler {
                id: tap
                objectName: "entities.shapes.row." + row.index + ".tap"
                // a double-click's second tap would take the focus from the rename field it opens
                onTapped: if (tap.tapCount === 1) { App.shapes.select(row.index); list.forceActiveFocus() }
                onDoubleTapped: list.startRename(row.index)
            }
            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: (point) => { list.forceActiveFocus(); list.openMenu(row.index, row, point.position.x, point.position.y) }
            }
            DragHandler {
                id: rowDrag
                target: null
                xAxis.enabled: false
                enabled: !row.editing && App.shapes.count > 1
                onActiveChanged: active ? list.beginDrag(row.index) : list.endDrag()
                onCentroidChanged: if (active) list.dragTo(row.mapToItem(list.contentItem, 0, centroid.position.y).y)
                onCanceled: list.cancelDrag()
            }

            RowLayout {
                anchors { fill: parent; leftMargin: 6; rightMargin: 6 }
                spacing: 8
                Icon { // ⑤
                    objectName: "entities.shapes.row." + row.index + ".grip"
                    name: "grip"
                    size: 14
                    color: hover.hovered ? Theme.muted : Theme.line2
                    HoverHandler { cursorShape: Qt.OpenHandCursor }
                }
                Chip { // ⑥
                    objectName: "entities.shapes.row." + row.index + ".chip"
                    text: row.idText
                    fill: row.chipColor
                    textColor: row.chipTextColor
                }
                Text { // ⑦
                    id: labelText
                    objectName: "entities.shapes.row." + row.index + ".label"
                    Accessible.ignored: true
                    visible: !row.editing
                    Layout.fillWidth: true
                    // the label shrinks first, but keeps about six characters
                    Layout.minimumWidth: Math.min(implicitWidth, Theme.fontBody * 3.5)
                    text: row.label
                    // the selected row's fill is accentSoft: muted is too faint on it
                    color: row.selected ? Theme.text : Theme.muted
                    font.pixelSize: Theme.fontBody
                    elide: Text.ElideRight
                    HoverHandler { id: labelHover }
                    BtToolTip { shown: labelHover.hovered && labelText.truncated; text: row.label }
                }
                Loader { // ⑧
                    active: row.editing
                    visible: active
                    Layout.fillWidth: true
                    sourceComponent: TextField {
                        id: field
                        objectName: "entities.shapes.row." + row.index + ".labelField"
                        property bool done: false
                        text: row.label
                        placeholderText: qsTr("Label")
                        maximumLength: 100
                        selectByMouse: true
                        color: Theme.text
                        placeholderTextColor: Theme.muted
                        selectionColor: Theme.accent
                        selectedTextColor: "white"
                        font.pixelSize: Theme.fontBody
                        leftPadding: 6
                        rightPadding: 6
                        topPadding: 3
                        bottomPadding: 3
                        Accessible.name: qsTr("Label of %1").arg(row.idText)
                        background: Rectangle {
                            color: Theme.panel
                            radius: Theme.radiusChip
                            border.width: 2
                            border.color: Theme.accent
                        }
                        function commit(refocus: bool) {
                            if (field.done)
                                return
                            field.done = true
                            App.shapes.renameShape(row.index, field.text)
                            list.finishRename(refocus)
                        }
                        Component.onCompleted: { forceActiveFocus(); selectAll() }
                        onAccepted: commit(true)
                        // Esc cancels here, before the window's Esc ladder
                        Keys.onShortcutOverride: (event) => { if (event.key === Qt.Key_Escape) event.accepted = true }
                        Keys.onEscapePressed: (event) => {
                            field.done = true
                            list.finishRename(true)
                            event.accepted = true
                        }
                        onActiveFocusChanged: if (!activeFocus) commit(false)
                    }
                }
                Chip { // ⑨
                    objectName: "entities.shapes.row." + row.index + ".weightBadge"
                    kind: Chip.Weight
                    visible: row.weight !== 1
                    text: "W" + row.weight
                }
                // only the hovered or selected row has its actions: made for that row alone
                Loader { // ⑩
                    active: row.showActions
                    visible: active
                    sourceComponent: Row {
                        spacing: 0
                        RowIconButton {
                            objectName: "entities.shapes.row." + row.index + ".rename"
                            iconName: "edit"
                            tip: qsTr("Rename (%1, or double-click)").arg(root.renameKeys)
                            onClicked: list.startRename(row.index)
                        }
                        RowIconButton {
                            objectName: "entities.shapes.row." + row.index + ".duplicate"
                            iconName: "copy"
                            tip: qsTr("Duplicate, as the last shape (%1)").arg(root.duplicateKeys)
                            onClicked: App.shapes.duplicateShape(row.index)
                        }
                        WeightButton {
                            objectName: "entities.shapes.row." + row.index + ".weightDown"
                            text: qsTr("W−")
                            spoken: qsTr("Decrease weight")
                            tip: qsTr("Weight −1 — %1").arg(root.weightTip)
                            enabled: row.weight > 0
                            onClicked: App.shapes.changeWeight(row.index, -1)
                        }
                        WeightButton {
                            objectName: "entities.shapes.row." + row.index + ".weightUp"
                            text: qsTr("W+")
                            spoken: qsTr("Increase weight")
                            tip: qsTr("Weight +1 — %1").arg(root.weightTip)
                            onClicked: App.shapes.changeWeight(row.index, +1)
                        }
                        RowIconButton {
                            objectName: "entities.shapes.row." + row.index + ".delete"
                            iconName: "trash"
                            dangerHover: true
                            enabled: App.shapes.canDelete
                            tip: App.shapes.canDelete ? qsTr("Delete (%1)").arg(root.deleteKeys)
                                                      : qsTr("The only shape cannot be deleted")
                            onClicked: App.shapes.requestDelete(row.index)
                        }
                    }
                }
            }
        }
        Text { // ⑪
            objectName: "entities.shapes.empty"
            visible: list.count === 0
            anchors.centerIn: parent
            text: qsTr("No shapes — click + New")
            color: Theme.muted
            font.pixelSize: Theme.fontSecondary
        }
    }

    AppMenu { // ⑫
        id: shapeMenu
        objectName: "entities.shapes.contextMenu"
        property int row: -1
        property int weight: 1           // the row's, as the menu opened
        property string idText
        property bool renameOnClose: false
        // the rename starts once the menu has closed and given the focus back
        onClosed: if (renameOnClose) { renameOnClose = false; list.startRename(row) }

        MenuItem {
            objectName: "entities.shapes.contextMenu.header"
            enabled: false
            Accessible.name: qsTr("Shape %1").arg(shapeMenu.idText)
            opacity: 1
            implicitHeight: 26
            leftPadding: 10
            background: null
            contentItem: Text {
                text: qsTr("Shape %1").arg(shapeMenu.idText)
                font.capitalization: Font.AllUppercase
                color: Theme.muted
                font.pixelSize: Theme.fontMicro
                font.weight: Font.DemiBold
                font.letterSpacing: 0.5
                verticalAlignment: Text.AlignVCenter
            }
        }
        AppMenuItem {
            objectName: "entities.shapes.contextMenu.rename"
            text: qsTr("Rename…")
            shortcutText: root.renameKeys
            onTriggered: shapeMenu.renameOnClose = true
        }
        AppMenuItem {
            objectName: "entities.shapes.contextMenu.duplicate"
            text: qsTr("Duplicate")
            shortcutText: root.duplicateKeys
            onTriggered: App.shapes.duplicateShape(shapeMenu.row)
        }
        AppMenuSeparator {}
        AppMenuItem {
            objectName: "entities.shapes.contextMenu.moveEarlier"
            text: qsTr("Move earlier")
            shortcutText: App.commands.shortcutText("shapes.moveEarlier")
            // the commands act on the selection, which the menu's row is (openMenu)
            enabled: App.commands.revision >= 0 && App.commands.isEnabled("shapes.moveEarlier")
            onTriggered: App.commands.trigger("shapes.moveEarlier")
        }
        AppMenuItem {
            objectName: "entities.shapes.contextMenu.moveLater"
            text: qsTr("Move later")
            shortcutText: App.commands.shortcutText("shapes.moveLater")
            enabled: App.commands.revision >= 0 && App.commands.isEnabled("shapes.moveLater")
            onTriggered: App.commands.trigger("shapes.moveLater")
        }
        AppMenuSeparator {}
        AppMenuItem {
            objectName: "entities.shapes.contextMenu.weightUp"
            text: qsTr("Weight +1")
            onTriggered: App.shapes.changeWeight(shapeMenu.row, +1)
        }
        AppMenuItem {
            objectName: "entities.shapes.contextMenu.weightDown"
            text: qsTr("Weight −1")
            enabled: shapeMenu.weight > 0
            onTriggered: App.shapes.changeWeight(shapeMenu.row, -1)
        }
        AppMenuSeparator {}
        AppMenuItem {
            objectName: "entities.shapes.contextMenu.delete"
            text: qsTr("Delete")
            danger: true
            shortcutText: root.deleteKeys
            enabled: App.shapes.canDelete
            onTriggered: App.shapes.requestDelete(shapeMenu.row)
        }
    }
}
