pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import BurrTools.Ui

// C16 Puzzle left card: the file's problems, P1 ... PN by position, and the
// selected problem's result and pieces. Every change is one undo step
// (App.puzzle), after a warning when it would cost the problem its solutions.
//
// Wireframe (each number marks its code below):
//
//   ┌──────────────────────────────────────────┐
//   │ Problems ①(2)              ②[+ New]  [◧] │   title, count, New, collapse
//   ├──────────────────────────────────────────┤
//   │ ③⠿ [P1] Small          ④✎ ⧉ 🗑           │   hovered or selected: actions
//   │  ⠿ [P2] Large                            │   renaming: a field for the label
//   ├──────────────────────────────────────────┤
//   │ ⑤[S5] Result: Target         [Change…]   │   one row per goal slot
//   │       19 voxels · can hold 19            │
//   ├──────────────────────────────────────────┤
//   │ ⑥In this puzzle (5)  ⑦[Chips|Table]  ⑧⋯ │   a short window scrolls this much,
//   │ ⑨┌────┐┌────┐┌────┐                      │   tiles, or ⑩ the table:
//   │  │S3  ││S4  ││S6  │                      │   Shape · Count · Range · Group
//   │  │− ×1+││− ×1+││−0–1+│                    │
//   │  └────┘└────┘└────┘                      │
//   ├──────────────────────────────────────────┤
//   │ ⑪All pieces (11)  drag, double-click ⌄   │
//   │ ⑫[⠿■S1 − +] [⠿■S2 Arm − ×1 +] ...        │
//   ├──────────────────────────────────────────┤
//   │ ⑬✓ Pieces can fill the result            │   the strip stays: fit, totals, holes
//   │    Result 19 voxels · Pieces n = 4–5 ...  │
//   └──────────────────────────────────────────┘
//   ⑭ the piece's menu   ⑮ the ⋯ menu   ⑯ Change…'s list   ⑰ the dragged piece
//
//   While a piece is dragged, the zone it can drop on is outlined: In this
//   puzzle from the library, All pieces from a tile.
Card {
    id: root
    objectName: "puzzle.left"
    title: qsTr("Problems")
    collapseIcon: "panelL"
    collapseObjectName: "puzzle.left.collapse"
    onCollapseClicked: App.layout.leftCollapsed = true
    onVisibleChanged: {
        if (!visible) {
            list.focus = false
            tiles.focus = false
        } else {
            seen = true
        }
    }

    // popups are no child items: for tests
    readonly property AppMenu pieceContextMenu: pieceMenu
    readonly property AppMenu moreContextMenu: moreMenu
    readonly property AppMenu resultContextMenu: resultMenu

    signal setRangeRequested()

    // a piece being dragged: its shape, and "library" or "used"
    property int dragShape: -1
    property string dragFrom: ""
    property string dragText: ""
    property point dragAt: Qt.point(0, 0)
    readonly property bool overUsed: dragFrom === "library" && contains(usedZone, dragAt)
    readonly property bool overLibrary: dragFrom === "used" && contains(libraryZone, dragAt)

    function contains(zone: Item, p: point): bool {
        const q = ghost.parent.mapToItem(zone, p.x, p.y)
        return q.x >= 0 && q.y >= 0 && q.x <= zone.width && q.y <= zone.height
    }
    function beginDrag(shape: int, from: string, text: string): void {
        dragShape = shape
        dragFrom = from
        dragText = text
    }
    function moveDrag(item: Item, x: real, y: real): void {
        dragAt = item.mapToItem(ghost.parent, x, y)
    }
    function endDrag(): void {
        const shape = dragShape
        const add = overUsed
        const remove = overLibrary
        cancelDrag()
        if (add)
            App.puzzle.stepPiece(shape, +1)
        else if (remove)
            App.puzzle.setCount(shape, 0)
    }
    function cancelDrag(): void {
        dragShape = -1
        dragFrom = ""
    }
    // an edit under a drag: the row or tile it began on may be gone, and with it the release
    function dropDrags(): void {
        cancelDrag()
        list.cancelRowDrag()
    }

    function openPieceMenu(shape: int, at: Item, x: real, y: real): void {
        App.puzzle.selectPiece(shape)
        pieceMenu.shape = shape
        pieceMenu.popup(at, x, y)
    }

    // the keys of a list of pieces: arrows go through them, in the model's order
    function movePiece(model: RowsModel, delta: int): void {
        const n = model.count
        if (n === 0)
            return
        let at = -1
        for (let i = 0; i < n; i++)
            if (model.value(i, "shape") === App.puzzle.selectedPiece)
                at = i
        const next = at < 0 ? 0 : Math.max(0, Math.min(n - 1, at + delta))
        App.puzzle.selectPiece(model.value(next, "shape"))
    }
    // the keyboard arrives in a list: something in it is selected, for the ring to show on
    function enterPieces(model: RowsModel): void {
        for (let i = 0; i < model.count; i++)
            if (model.value(i, "shape") === App.puzzle.selectedPiece)
                return
        movePiece(model, 0)
    }
    function pieceKey(event: KeyEvent, model: RowsModel, items: Repeater): void {
        if (event.key === Qt.Key_Left || event.key === Qt.Key_Up) {
            movePiece(model, -1)
            event.accepted = true
        } else if (event.key === Qt.Key_Right || event.key === Qt.Key_Down) {
            movePiece(model, 1)
            event.accepted = true
        } else if (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier))) {
            for (let i = 0; i < items.count; i++) {
                const it = items.itemAt(i)
                if (model.value(i, "shape") === App.puzzle.selectedPiece)
                    openPieceMenu(App.puzzle.selectedPiece, it, 0, it.height)
            }
            event.accepted = true
        } else {
            const key = App.commands.keyInScope("puzzlePieces", event.key, event.modifiers)
            if (key !== "") {
                App.commands.trigger(key)
                event.accepted = true
            }
        }
    }

    // the pieces as tiles, else as the table: one comparison, read by both views
    readonly property bool chipsShown: App.puzzle.piecesView !== "table"
    /* Puzzle has shown (a card made in the background has not): from then the view
     * of the pieces not chosen is made too, when App.builds gets to it, so that the
     * first switch between tiles and table finds it ready. */
    property bool seen: false
    Component.onCompleted: if (visible) seen = true

    readonly property string renameKeys: App.commands.shortcutText("problems.rename")
    readonly property string duplicateKeys: App.commands.shortcutText("problems.duplicate")
    readonly property string deleteKeys: App.commands.shortcutText("problems.delete")

    headerExtras: [
        Chip { // ①
            objectName: "puzzle.problems.count"
            kind: Chip.Count
            text: App.puzzle.problems.count
            Accessible.role: Accessible.StaticText
            Accessible.name: qsTr("%1 problems").arg(App.puzzle.problems.count)
        }
    ]
    headerActions: [
        BtButton { // ②
            id: newButton
            objectName: "puzzle.problems.new"
            primary: true
            small: true
            text: Theme.minimal ? qsTr("+") : qsTr("+ New")
            width: Theme.minimal ? height : implicitWidth
            Accessible.name: qsTr("New problem")
            Accessible.description: newTip.text
            onClicked: App.puzzle.newProblem()
            BtToolTip {
                id: newTip
                shown: newButton.hovered || newButton.visualFocus
                text: qsTr("New problem: no pieces yet, each colour on itself")
            }
        }
    ]

    // a row's icon actions: no Tab stop (the list is one)
    component RowIconButton: IconButton {
        implicitWidth: 24
        implicitHeight: 24
        iconSize: 16
        focusPolicy: Qt.NoFocus
    }

    // a section's heading, as high as the inspector's sections, a line above it
    component SectionTitle: Text {
        color: Theme.text
        font.pixelSize: Theme.fontBody
        font.weight: Font.DemiBold
        elide: Text.ElideRight
        Layout.alignment: Qt.AlignVCenter
    }
    component SectionRule: Rectangle {
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 1
        color: Theme.line
    }

    // a count in the table: what is typed goes through after a short pause, or at
    // Enter or leaving it; otherwise it shows the value at once, whoever changed it
    // (NumberEntry). A pause commits nothing below liveFrom: 0 removes the row.
    component CountField: TextField {
        id: countField
        property int value: 0
        property int from: 0
        property int liveFrom: 1
        property string name
        signal committed(int value)
        signal entryStarted()
        signal entryEnded()
        signal focusTaken()
        implicitWidth: 40
        implicitHeight: 24
        padding: 2
        horizontalAlignment: TextInput.AlignHCenter
        color: Theme.text
        selectionColor: Theme.accent
        selectedTextColor: Theme.onAccent
        selectByMouse: true
        font.pixelSize: Theme.fontSecondary
        inputMethodHints: Qt.ImhDigitsOnly
        validator: IntValidator { bottom: 0; top: App.puzzle.maxCount }
        Accessible.name: name
        background: Rectangle {
            radius: Theme.radiusChip
            color: Theme.panel
            border.width: countField.activeFocus ? 2 : 1
            border.color: countField.activeFocus ? Theme.accent : Theme.line2
        }
        Component.onCompleted: text = String(value)
        onTextEdited: entry.edited()
        onActiveFocusChanged: activeFocus ? focusTaken() : entry.finish()
        Keys.onReturnPressed: entry.finish()
        Keys.onEnterPressed: entry.finish()
        Keys.onShortcutOverride: (event) => { if (event.key === Qt.Key_Escape && entry.entering) event.accepted = true }
        Keys.onEscapePressed: (event) => {
            event.accepted = entry.entering
            entry.cancel()
        }
        NumberEntry {
            id: entry
            field: countField
            value: countField.value
            from: countField.from
            to: App.puzzle.maxCount
            liveFrom: countField.liveFrom
            onCommitted: (v) => countField.committed(v)
            onStarted: countField.entryStarted()
            onEnded: countField.entryEnded()
        }
        Connections {
            target: App.puzzle
            function onTypingEnding(keep: bool) {
                if (!entry.open)
                    return
                if (keep)
                    entry.finish()
                else
                    entry.drop()
            }
        }
    }

    Connections {
        target: App.puzzle
        function onRenameRequested(row: int) { list.startRename(row) }
    }
    Connections {
        target: App.document
        function onHistoryApplied() { pieceMenu.close(); root.dropDrags() }
        function onDocumentReplaced() { pieceMenu.close(); list.finishRename(false); root.dropDrags() }
        function onPuzzleEdited() { root.dropDrags() }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // the most a short window gets: what is above the strip scrolls there, the
        // strip stays in sight
        Flickable {
            id: body
            objectName: "puzzle.left.body"
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentHeight: bodyColumn.height
            clip: true
            interactive: false       // the lists in it drag and flick; the bar and the wheel scroll it
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            WheelHandler {
                enabled: body.contentHeight > body.height
                onWheel: (event) => {
                    body.contentY = Math.max(0, Math.min(body.contentHeight - body.height, body.contentY - event.angleDelta.y / 2))
                }
            }

            ColumnLayout {
                id: bodyColumn
                width: body.width
                spacing: 0
                readonly property real leastHeight: list.Layout.minimumHeight + 2 * list.Layout.margins
                                                    + (App.puzzle.problem >= 0 ? resultSection.Layout.preferredHeight
                                                                                 + usedZone.Layout.minimumHeight
                                                                                 + libraryZone.Layout.minimumHeight : 0)
                height: Math.max(body.height, leastHeight)

            ListView { // ③
                id: list
                objectName: "puzzle.problems.list"
                Layout.fillWidth: true
                Layout.margins: 6
                // C16: at most about 140 dp, then it scrolls; three rows at the least, so
                // that what is below does not move for a new problem or give the list up
                readonly property real leastHeight: 3 * pitch - spacing
                Layout.minimumHeight: leastHeight
                Layout.preferredHeight: Math.max(leastHeight, Math.min(140, count * pitch - spacing))
                cacheBuffer: 0
                clip: true
                spacing: 2
                model: App.puzzle.problems
                currentIndex: App.puzzle.problem
                /* The selected row in view at once, however it was selected (the arrows,
                 * the rail, + New): following currentIndex alone, the view glided at
                 * 400 px/s behind held arrows, moved under reduced motion, and at times
                 * not at all. */
                highlightMoveDuration: 0
                highlightMoveVelocity: -1
                Connections {
                    target: App.puzzle
                    // (none selected, -1: the view clamps it to the first row)
                    function onProblemChanged() { list.positionViewAtIndex(App.puzzle.problem, ListView.Contain) }
                }
                activeFocusOnTab: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                Accessible.role: Accessible.List
                Accessible.name: qsTr("Problems")

                readonly property real pitch: Theme.listRow + spacing
                property int editingRow: -1
                onCountChanged: if (editingRow >= count) editingRow = -1
                property int dragFrom: -1
                property int dropAt: -1
                property bool dragCancelled: false
                readonly property int dropTo: dropAt < 0 || dragFrom < 0 ? -1 : (dropAt > dragFrom ? dropAt - 1 : dropAt)
                property bool settling: false

                function startRename(row: int): void {
                    if (row < 0 || row >= count)
                        return
                    App.puzzle.selectProblem(row)
                    positionViewAtIndex(row, ListView.Contain)
                    editingRow = row
                }
                function finishRename(refocus: bool): void {
                    editingRow = -1
                    if (refocus)
                        forceActiveFocus()
                }
                function beginRowDrag(row: int): void {
                    App.puzzle.flushTyping()     // a rename in progress goes in
                    dragFrom = row
                    dropAt = -1
                    dragCancelled = false
                    App.puzzle.selectProblem(row)
                    forceActiveFocus()
                }
                function rowDragTo(y: real): void {
                    if (!dragCancelled)
                        dropAt = Math.max(0, Math.min(count, Math.floor((y - Theme.listRow / 2) / pitch) + 1))
                }
                function cancelRowDrag(): void {
                    dragCancelled = true
                    dragFrom = -1
                    dropAt = -1
                }
                function endRowDrag(): void {
                    const from = dragFrom
                    const to = dragCancelled ? -1 : dropTo
                    settling = true
                    dragFrom = -1
                    dropAt = -1
                    if (from >= 0 && to >= 0 && to !== from)
                        App.puzzle.moveProblem(from, to)
                    settling = false
                }

                Keys.onShortcutOverride: (event) => {
                    if (list.editingRow >= 0)
                        return
                    if ((event.key === Qt.Key_Escape && list.dragFrom >= 0)
                            || App.commands.keyInScope("problemsList", event.key, event.modifiers) !== "")
                        event.accepted = true
                }
                Keys.onPressed: (event) => {
                    if (list.editingRow >= 0)
                        return
                    if (event.key === Qt.Key_Escape && list.dragFrom >= 0) {
                        list.dragCancelled = true
                        list.dropAt = -1
                        event.accepted = true
                    } else if ((event.key === Qt.Key_Up || event.key === Qt.Key_Down)
                               && (event.modifiers & ~Qt.KeypadModifier) === Qt.NoModifier) {
                        App.puzzle.selectProblemBy(event.key === Qt.Key_Up ? -1 : 1)
                        event.accepted = true
                    } else {
                        const key = App.commands.keyInScope("problemsList", event.key, event.modifiers)
                        if (key !== "") {
                            App.commands.trigger(key)
                            event.accepted = true
                        }
                    }
                }

                delegate: Rectangle {
                    id: row
                    required property int index
                    required property string idText
                    required property string label
                    readonly property bool selected: index === App.puzzle.problem
                    readonly property bool editing: list.editingRow === index
                    readonly property bool dragged: list.dragFrom === index
                    readonly property bool showActions: (hover.hovered || selected) && !editing && list.dragFrom < 0
                    // The actions take presses only from the event after they showed:
                    // a press that arrives with the pointer (a pen, a touch, a row
                    // moving under a still pointer) hovers the row and lands at once,
                    // and must select it, not hit a button the user never saw.
                    property bool actionsArmed: false
                    onShowActionsChanged: {
                        actionsArmed = false
                        if (showActions)
                            armTimer.restart()
                    }
                    Component.onCompleted: if (showActions) armTimer.restart()
                    Timer { // the next turn of the event loop, after the press that showed them
                        id: armTimer
                        interval: 0
                        onTriggered: row.actionsArmed = row.showActions
                    }
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

                    objectName: "puzzle.problems.row." + index
                    width: ListView.view.width
                    height: Theme.listRow
                    z: dragged ? 2 : 0
                    opacity: dragged ? 0.6 : 1
                    transform: Translate {
                        y: row.dragged ? gripDrag.activeTranslation.y : row.shift
                        Behavior on y {
                            enabled: !row.dragged && !list.settling
                            NumberAnimation { duration: Theme.durReorder; easing.type: Easing.OutCubic }
                        }
                    }
                    radius: Theme.radiusControl
                    color: selected ? Theme.accentSoft : (hover.hovered ? Theme.panel2 : "transparent")
                    border.width: selected ? (list.activeFocus && App.keyboardCues.navigating ? 2 : 1) : 0
                    border.color: list.activeFocus && App.keyboardCues.navigating ? Theme.accent : Theme.accentEdge
                    Accessible.role: Accessible.ListItem
                    Accessible.name: label.length > 0 ? qsTr("%1 %2").arg(idText).arg(label) : idText
                    Accessible.selectable: true
                    Accessible.selected: selected
                    Accessible.onPressAction: App.puzzle.selectProblem(row.index)

                    HoverHandler { id: hover }
                    TapHandler {
                        id: tap
                        onTapped: if (tap.tapCount === 1) { App.puzzle.selectProblem(row.index); list.forceActiveFocus() }
                        onDoubleTapped: list.startRename(row.index)
                    }

                    RowLayout {
                        anchors { fill: parent; leftMargin: 6; rightMargin: 6 }
                        spacing: 8
                        Icon {
                            objectName: "puzzle.problems.row." + row.index + ".grip"
                            name: "grip"
                            size: 14
                            color: hover.hovered ? Theme.muted : Theme.line2
                            HoverHandler { cursorShape: Qt.OpenHandCursor }
                            BtToolTip { shown: gripHover.hovered; text: qsTr("Drag to reorder") }
                            HoverHandler { id: gripHover }
                            DragHandler {
                                id: gripDrag
                                target: null
                                xAxis.enabled: false
                                enabled: !row.editing && App.puzzle.problems.count > 1
                                onActiveChanged: active ? list.beginRowDrag(row.index) : list.endRowDrag()
                                onCentroidChanged: if (active) list.rowDragTo(row.mapToItem(list.contentItem, 0, centroid.position.y).y)
                                onCanceled: { list.dragCancelled = true; list.dropAt = -1 }
                            }
                        }
                        Chip {
                            objectName: "puzzle.problems.row." + row.index + ".chip"
                            text: row.idText
                            neutral: true
                        }
                        Text {
                            objectName: "puzzle.problems.row." + row.index + ".label"
                            Accessible.ignored: true
                            visible: !row.editing
                            Layout.fillWidth: true
                            text: row.label
                            textFormat: Text.PlainText
                            color: row.selected ? Theme.text : Theme.muted
                            font.pixelSize: Theme.fontBody
                            elide: Text.ElideRight
                        }
                        Loader {
                            active: row.editing
                            visible: active
                            Layout.fillWidth: true
                            sourceComponent: TextField {
                                id: field
                                objectName: "puzzle.problems.row." + row.index + ".labelField"
                                property bool done: false
                                text: row.label
                                placeholderText: qsTr("Label")
                                maximumLength: 100
                                selectByMouse: true
                                color: Theme.text
                                placeholderTextColor: Theme.muted
                                selectionColor: Theme.accent
                                selectedTextColor: Theme.onAccent
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
                                function commit(refocus: bool): void {
                                    if (field.done)
                                        return
                                    field.done = true
                                    App.puzzle.renameProblem(row.index, field.text)
                                    list.finishRename(refocus)
                                }
                                Connections {
                                    target: App.puzzle
                                    function onTypingEnding(keep: bool) {
                                        if (keep) {
                                            field.commit(false)
                                        } else {
                                            field.done = true
                                            list.finishRename(false)
                                        }
                                    }
                                }
                                Component.onCompleted: { forceActiveFocus(); selectAll() }
                                onAccepted: commit(true)
                                Keys.onShortcutOverride: (event) => { if (event.key === Qt.Key_Escape) event.accepted = true }
                                Keys.onEscapePressed: (event) => {
                                    field.done = true
                                    list.finishRename(true)
                                    event.accepted = true
                                }
                                onActiveFocusChanged: if (!activeFocus) commit(false)
                            }
                        }
                        // the actions' room, taken from the label only while they show
                        Item {
                            visible: row.showActions
                            Layout.preferredWidth: actions.width
                        }
                    }
                    // Every row has its actions, placed where they show; only the
                    // hovered or selected row shows them, and they take presses once
                    // armed. (Made on demand, they took the press that made them,
                    // before their row had placed them: PR 181 review, the shapes
                    // list.) They lie over the row, not in its layout, so a hidden
                    // row's label has their room.
                    Row { // ④
                        id: actions
                        objectName: "puzzle.problems.row." + row.index + ".actions"
                        anchors { right: parent.right; rightMargin: 6; verticalCenter: parent.verticalCenter }
                        visible: !row.editing        // the rename field takes the room
                        opacity: row.showActions ? 1 : 0
                        enabled: row.actionsArmed
                        spacing: 0
                        RowIconButton {
                            objectName: "puzzle.problems.row." + row.index + ".rename"
                            Accessible.ignored: !row.showActions     // hidden: not read out
                            iconName: "edit"
                            tip: qsTr("Rename (%1, or double-click)").arg(root.renameKeys)
                            onClicked: list.startRename(row.index)
                        }
                        RowIconButton {
                            objectName: "puzzle.problems.row." + row.index + ".duplicate"
                            Accessible.ignored: !row.showActions     // hidden: not read out
                            iconName: "copy"
                            tip: qsTr("Duplicate, as the last problem (%1)").arg(root.duplicateKeys)
                            onClicked: App.puzzle.duplicateProblem(row.index)
                        }
                        RowIconButton {
                            objectName: "puzzle.problems.row." + row.index + ".delete"
                            Accessible.ignored: !row.showActions     // hidden: not read out
                            iconName: "trash"
                            dangerHover: true
                            enabled: App.puzzle.canDeleteProblem
                            tip: App.puzzle.canDeleteProblem ? qsTr("Delete (%1)").arg(root.deleteKeys)
                                                             : qsTr("The only problem cannot be deleted")
                            onClicked: App.puzzle.deleteProblem(row.index)
                        }
                    }
                }
                Text {
                    objectName: "puzzle.problems.empty"
                    visible: list.count === 0
                    anchors.centerIn: parent
                    text: qsTr("No problems yet — click + New")
                    color: Theme.muted
                    font.pixelSize: Theme.fontSecondary
                }
            }

            Item { // ⑤
                id: resultSection
                objectName: "puzzle.result"
                Layout.fillWidth: true
                Layout.preferredHeight: resultColumn.implicitHeight
                visible: App.puzzle.problem >= 0
                SectionRule {}

                Column {
                    id: resultColumn
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    Repeater {
                        model: App.puzzle.goal
                        delegate: Column {
                            id: slotRow
                            required property int index
                            required property string role
                            required property string title
                            required property int shape
                            required property string idText
                            required property string name
                            required property color chipColor
                            required property color chipTextColor
                            required property string detail
                            width: parent.width

                            RowLayout {
                                objectName: "puzzle.goal." + slotRow.role
                                width: parent.width - 22
                                x: 14
                                height: Theme.listRow + 6
                                spacing: 8
                                Chip {
                                    objectName: "puzzle.goal." + slotRow.role + ".chip"
                                    visible: slotRow.shape >= 0
                                    text: slotRow.idText
                                    fill: slotRow.chipColor
                                    textColor: slotRow.chipTextColor
                                }
                                Column {
                                    Layout.fillWidth: true
                                    Text {
                                        objectName: "puzzle.goal." + slotRow.role + ".name"
                                        width: parent.width
                                        text: slotRow.name === slotRow.title ? slotRow.name
                                                                             : qsTr("%1: %2").arg(slotRow.title).arg(slotRow.name)
                                        textFormat: Text.PlainText
                                        elide: Text.ElideRight
                                        color: Theme.text
                                        font.pixelSize: Theme.fontBody
                                        font.weight: Font.DemiBold
                                    }
                                    Text {
                                        objectName: "puzzle.goal." + slotRow.role + ".detail"
                                        width: parent.width
                                        visible: !Theme.minimal
                                        text: slotRow.detail
                                        textFormat: Text.PlainText
                                        elide: Text.ElideRight
                                        color: Theme.muted
                                        font.pixelSize: Theme.fontSecondary
                                    }
                                }
                                BtButton {
                                    id: changeButton
                                    objectName: "puzzle.goal." + slotRow.role + ".change"
                                    small: true
                                    text: slotRow.shape >= 0 ? qsTr("Change…") : qsTr("Choose…")
                                    enabled: App.shapes.count > 0
                                    Accessible.description: slotRow.detail
                                    BtToolTip { shown: Theme.minimal && (changeButton.hovered || changeButton.visualFocus); text: slotRow.detail }
                                    onClicked: {
                                        resultMenu.current = slotRow.shape
                                        resultMenu.wanted = true
                                        resultMenu.popup(changeButton, 0, changeButton.height)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Item {
                id: usedZone
                objectName: "puzzle.used"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: Theme.accordionHeader + 96
                visible: App.puzzle.problem >= 0
                SectionRule {}

                RowLayout {
                    id: usedHeader
                    anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: 14; rightMargin: 8 }
                    height: Theme.accordionHeader
                    spacing: 6
                    SectionTitle { // ⑥
                        objectName: "puzzle.used.title"
                        text: qsTr("In this puzzle")
                        Layout.fillWidth: true
                        Layout.minimumWidth: implicitWidth
                    }
                    Chip {
                        objectName: "puzzle.used.count"
                        visible: !Theme.minimal       // the header never wraps or cuts its title (AC-DEN-02)
                        kind: Chip.Count
                        text: App.puzzle.pieces.count
                        Accessible.role: Accessible.StaticText
                        Accessible.name: qsTr("%1 pieces in this puzzle").arg(App.puzzle.pieces.count)
                    }
                    Segmented { // ⑦
                        objectName: "puzzle.used.view"
                        Accessible.name: qsTr("Show the pieces as")
                        options: [ { value: "chips", label: qsTr("Chips") }, { value: "table", label: qsTr("Table") } ]
                        value: App.puzzle.piecesView
                        onActivated: (v) => App.puzzle.piecesView = v
                    }
                    IconButton { // ⑧
                        id: moreButton
                        objectName: "puzzle.used.more"
                        iconName: "more-horizontal"
                        tip: qsTr("More for the pieces")
                        onClicked: moreMenu.popup(moreButton, 0, moreButton.height)
                        // into the table at its first row (the table walks its rows itself)
                        Keys.onTabPressed: (event) => { event.accepted = table.enter(false) }
                    }
                }

                FocusScope { // ⑨
                    id: tiles
                    objectName: "puzzle.used.tiles"
                    visible: root.chipsShown
                    anchors { left: parent.left; right: parent.right; top: usedHeader.bottom; bottom: parent.bottom
                              leftMargin: 6; rightMargin: 6; bottomMargin: 4 }
                    activeFocusOnTab: App.puzzle.pieces.count > 0
                    Accessible.role: Accessible.List
                    Accessible.name: qsTr("Pieces in this puzzle")

                    onActiveFocusChanged: if (activeFocus && App.keyboardCues.navigating) root.enterPieces(App.puzzle.pieces)
                    Keys.onShortcutOverride: (event) => {
                        if (App.commands.keyInScope("puzzlePieces", event.key, event.modifiers) !== "")
                            event.accepted = true
                    }
                    Keys.onPressed: (event) => root.pieceKey(event, App.puzzle.pieces, tileList)

                    Flickable {
                        id: tileView
                        anchors.fill: parent
                        contentHeight: flow.height
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                        Flow {
                            id: flow
                            width: parent.width
                            spacing: 3
                            /* a batch at a time through App.builds (a first screen at once while
                             * shown); then each piece added or taken away is one tile made or
                             * dropped, every other edit updates what exists. Declared, not
                             * written into `model` (a var): there its bindings are not compiled. */
                            BatchedRows {
                                id: tileRows
                                queue: App.builds
                                source: App.puzzle.pieces
                                active: root.chipsShown || root.seen
                                shown: flow.visible
                            }
                            Repeater {
                                id: tileList
                                model: tileRows
                                delegate: PieceTile {
                                    id: tile
                                    required property int shape
                                    required idText
                                    required label
                                    required property color chipColor
                                    required countText
                                    required groupText
                                    objectName: "puzzle.used.tile." + shape
                                    stripe: chipColor
                                    selected: shape === App.puzzle.selectedPiece
                                    focusRing: selected && tiles.activeFocus && App.keyboardCues.navigating
                                    onFocusRingChanged: if (focusRing) tileFollower.show(tile)
                                    opacity: root.dragShape === shape && root.dragFrom === "used" ? 0.5 : 1
                                    onTapped: { App.puzzle.selectPiece(shape); tiles.forceActiveFocus() }
                                    onStepped: (delta) => App.puzzle.stepPiece(shape, delta)
                                    onMenuRequested: (x, y) => root.openPieceMenu(shape, tile, x, y)
                                    onDragStarted: { App.puzzle.selectPiece(shape); root.beginDrag(shape, "used", idText) }
                                    onDragMoved: (x, y) => root.moveDrag(tile, x, y)
                                    onDragEnded: root.endDrag()
                                    onDragCancelled: root.cancelDrag()
                                }
                            }
                        }
                    }
                    ScrollToFocus { id: tileFollower; view: tileView }
                    Text {
                        objectName: "puzzle.used.empty"
                        visible: App.puzzle.pieces.count === 0
                        anchors { fill: parent; margins: 8 }
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        text: qsTr("No pieces yet. Add them from All pieces below — drag, double-click, or press +.")
                        color: Theme.muted
                        font.pixelSize: Theme.fontSecondary
                    }
                }

                ListView { // ⑩
                    id: table
                    objectName: "puzzle.used.table"
                    visible: !root.chipsShown
                    anchors { left: parent.left; right: parent.right; top: usedHeader.bottom; bottom: parent.bottom
                              leftMargin: 6; rightMargin: 6; bottomMargin: 4 }
                    clip: true
                    cacheBuffer: 0
                    spacing: 2
                    // as the tiles' (four controls a row, a group list each); the view makes the rows in sight
                    BatchedRows {
                        id: tableRows
                        queue: App.builds
                        source: App.puzzle.pieces
                        active: !root.chipsShown || root.seen
                        shown: table.visible
                    }
                    model: tableRows
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                    Accessible.role: Accessible.Table
                    Accessible.name: qsTr("Pieces in this puzzle")

                    /* Tab and Shift+Tab walk the rows in order and bring each into
                     * view. The view makes only the rows in sight, so the focus chain
                     * alone stopped at the last of them (and, once scrolled back, went
                     * through the rows in the order they were made); the rows hand the
                     * keyboard on themselves, and the table to what is around it. */
                    signal rowFocusRequested(int row, bool last)
                    function focusRow(row: int, last: bool): void {
                        table.positionViewAtIndex(row, ListView.Contain)
                        table.forceLayout()
                        table.rowFocusRequested(row, last)
                    }
                    function enter(last: bool): bool {
                        if (!table.visible || table.count === 0)
                            return false
                        table.focusRow(last ? table.count - 1 : 0, last)
                        return true
                    }

                    // the widths of Count, Range and Group; Shape takes the rest
                    readonly property real countWidth: 92
                    readonly property real rangeWidth: 40
                    readonly property real groupWidth: 66
                    readonly property real rowHeight: 28

                    header: Item {
                        width: ListView.view.width
                        height: 22
                        Row {
                            anchors { left: parent.left; right: parent.right; leftMargin: 4; rightMargin: 4 }
                            spacing: 4
                            component Heading: Text {
                                color: Theme.muted
                                font.pixelSize: Theme.fontMicro
                                font.weight: Font.DemiBold
                                height: 22
                                verticalAlignment: Text.AlignVCenter
                            }
                            Heading { objectName: "puzzle.used.table.shape"; text: qsTr("Shape"); width: parent.width - table.countWidth - table.rangeWidth - table.groupWidth - 12 }
                            Heading { objectName: "puzzle.used.table.count"; text: qsTr("Count"); width: table.countWidth }
                            Heading { objectName: "puzzle.used.table.range"; text: qsTr("Range"); width: table.rangeWidth }
                            Heading { objectName: "puzzle.used.table.group"; text: qsTr("Group"); width: table.groupWidth }
                        }
                    }

                    delegate: Rectangle {
                        id: tableRow
                        required property int index
                        required property int shape
                        required property string idText
                        required property string label
                        required property color chipColor
                        required property color chipTextColor
                        required property int minCount
                        required property int maxCount
                        required property bool rangeOn
                        required property string groupText
                        readonly property bool selected: shape === App.puzzle.selectedPiece

                        objectName: "puzzle.used.row." + shape
                        width: ListView.view.width
                        height: table.rowHeight
                        radius: Theme.radiusChip
                        color: selected ? Theme.accentSoft : "transparent"
                        Accessible.role: Accessible.Row
                        Accessible.name: label.length > 0 ? qsTr("%1 %2").arg(idText).arg(label) : idText
                        Accessible.selectable: true
                        Accessible.selected: selected
                        TapHandler { onTapped: App.puzzle.selectPiece(tableRow.shape) }

                        // a control of the row took the keyboard: the row in view (the mouse scrolls for itself)
                        function focusTaken(): void {
                            if (App.keyboardCues.navigating)
                                table.positionViewAtIndex(tableRow.index, ListView.Contain)
                        }
                        Connections {
                            target: table
                            function onRowFocusRequested(row: int, last: bool) {
                                if (row === tableRow.index)
                                    (last ? groupField : minField).forceActiveFocus(Qt.TabFocusReason)
                            }
                        }
                        /* Past the row's last control, the next row's first; before its
                         * first, the row above's last. On the controls themselves: an
                         * item that is a Tab stop moves the focus on its own and passes
                         * no Tab up to its row. */
                        function tabOut(): void {
                            if (tableRow.index < table.count - 1)
                                table.focusRow(tableRow.index + 1, false)
                            else
                                libraryHeader.forceActiveFocus(Qt.TabFocusReason)
                        }
                        function backtabOut(): void {
                            if (tableRow.index > 0)
                                table.focusRow(tableRow.index - 1, true)
                            else
                                moreButton.forceActiveFocus(Qt.TabFocusReason)
                        }

                        Row {
                            anchors { fill: parent; leftMargin: 4; rightMargin: 4 }
                            spacing: 4
                            Item {
                                width: parent.width - table.countWidth - table.rangeWidth - table.groupWidth - 12
                                height: 24
                                anchors.verticalCenter: parent.verticalCenter
                                Rectangle {
                                    objectName: "puzzle.used.row." + tableRow.shape + ".chip"
                                    // as wide as what it says, up to the column
                                    width: Math.min(parent.width, Math.max(34, chipLabel.implicitWidth + 12))
                                    height: parent.height
                                    radius: Theme.radiusChip
                                    color: tableRow.chipColor
                                    Text {
                                        id: chipLabel
                                        anchors { fill: parent; leftMargin: 6; rightMargin: 6 }
                                        verticalAlignment: Text.AlignVCenter
                                        elide: Text.ElideRight
                                        text: tableRow.label.length > 0 ? qsTr("%1 – %2").arg(tableRow.idText).arg(tableRow.label) : tableRow.idText
                                        textFormat: Text.PlainText
                                        Accessible.ignored: true
                                        color: tableRow.chipTextColor
                                        font.pixelSize: Theme.fontMicro
                                        font.weight: Font.Bold
                                    }
                                }
                            }
                            Row {
                                width: table.countWidth
                                spacing: 2
                                anchors.verticalCenter: parent.verticalCenter
                                CountField {
                                    id: minField
                                    objectName: "puzzle.used.row." + tableRow.shape + ".min"
                                    name: tableRow.rangeOn ? qsTr("%1 minimum").arg(tableRow.idText) : qsTr("%1 count").arg(tableRow.idText)
                                    value: tableRow.rangeOn ? tableRow.minCount : tableRow.maxCount
                                    liveFrom: tableRow.rangeOn ? 0 : 1
                                    onFocusTaken: { App.puzzle.selectPiece(tableRow.shape); tableRow.focusTaken() }
                                    Keys.onBacktabPressed: tableRow.backtabOut()
                                    onEntryStarted: App.puzzle.holdCounts(tableRow.shape)
                                    onEntryEnded: App.puzzle.releaseCounts()
                                    onCommitted: (v) => tableRow.rangeOn ? App.puzzle.setPieceMin(tableRow.shape, v)
                                                                         : App.puzzle.setCount(tableRow.shape, v)
                                }
                                Text {
                                    visible: tableRow.rangeOn
                                    text: qsTr("–")
                                    height: 24
                                    verticalAlignment: Text.AlignVCenter
                                    color: Theme.muted
                                    font.pixelSize: Theme.fontSecondary
                                }
                                CountField {
                                    objectName: "puzzle.used.row." + tableRow.shape + ".max"
                                    visible: tableRow.rangeOn
                                    from: 1
                                    name: qsTr("%1 maximum").arg(tableRow.idText)
                                    value: tableRow.maxCount
                                    onFocusTaken: { App.puzzle.selectPiece(tableRow.shape); tableRow.focusTaken() }
                                    onEntryStarted: App.puzzle.holdCounts(tableRow.shape)
                                    onEntryEnded: App.puzzle.releaseCounts()
                                    onCommitted: (v) => App.puzzle.setPieceMax(tableRow.shape, v)
                                }
                            }
                            Item {
                                width: table.rangeWidth
                                height: 24
                                anchors.verticalCenter: parent.verticalCenter
                                BtSwitch {
                                    id: rangeField
                                    objectName: "puzzle.used.row." + tableRow.shape + ".range"
                                    anchors.verticalCenter: parent.verticalCenter
                                    onActiveFocusChanged: if (rangeField.activeFocus) tableRow.focusTaken()
                                    // the state is the model's: a warning may keep it as it is
                                    checkable: false
                                    checked: tableRow.rangeOn
                                    Accessible.name: qsTr("%1 as a range").arg(tableRow.idText)
                                    onClicked: {
                                        App.puzzle.selectPiece(tableRow.shape)
                                        App.puzzle.setRangeOn(tableRow.shape, !tableRow.rangeOn)
                                    }
                                }
                            }
                            GroupPicker {
                                id: groupField
                                objectName: "puzzle.used.row." + tableRow.shape + ".group"
                                onActiveFocusChanged: if (groupField.activeFocus) tableRow.focusTaken()
                                Keys.onTabPressed: tableRow.tabOut()
                                width: table.groupWidth
                                height: 24
                                anchors.verticalCenter: parent.verticalCenter
                                shape: tableRow.shape
                                idText: tableRow.idText
                                groupText: tableRow.groupText
                            }
                        }
                    }
                }

                DashedFrame { // what a drop would do, drawn over the zone
                    objectName: "puzzle.used.dropZone"
                    anchors { fill: parent; margins: 2 }
                    visible: root.dragFrom === "library"
                    fill: Theme.accentSoft
                    lineWidth: 2
                }
            }

            Item {
                id: libraryZone
                objectName: "puzzle.library"
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.accordionHeader + (App.puzzle.allPiecesOpen ? Math.min(210, libraryFlow.height + 6) : 0)
                Layout.minimumHeight: Theme.accordionHeader
                visible: App.puzzle.problem >= 0
                SectionRule {}

                AbstractButton { // ⑪
                    id: libraryHeader
                    objectName: "puzzle.library.header"
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    height: Theme.accordionHeader
                    leftPadding: 14
                    rightPadding: 8
                    hoverEnabled: true
                    focusPolicy: Qt.StrongFocus
                    Accessible.name: qsTr("All pieces")
                    Accessible.description: App.puzzle.allPiecesOpen ? qsTr("Expanded. %1").arg(libraryHint.text)
                                                                     : qsTr("Collapsed. %1").arg(libraryHint.text)
                    onClicked: App.puzzle.allPiecesOpen = !App.puzzle.allPiecesOpen
                    // back into the table at its last row
                    Keys.onBacktabPressed: (event) => { event.accepted = table.enter(true) }
                    BtToolTip { shown: Theme.minimal && (libraryHeader.hovered || libraryHeader.visualFocus); text: libraryHint.text }
                    background: Rectangle {
                        color: libraryHeader.hovered ? Theme.panel2 : "transparent"
                        border.width: libraryHeader.visualFocus ? 2 : 0
                        border.color: Theme.accent
                    }
                    contentItem: RowLayout {
                        spacing: 6
                        Text {
                            objectName: "puzzle.library.title"
                            text: qsTr("All pieces")
                            Accessible.ignored: true
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                            font.weight: Font.DemiBold
                        }
                        Chip {
                            objectName: "puzzle.library.count"
                            kind: Chip.Count
                            text: App.puzzle.library.count
                        }
                        Text {
                            id: libraryHint
                            objectName: "puzzle.library.hint"
                            Layout.fillWidth: true
                            visible: !Theme.minimal
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideRight
                            text: qsTr("drag, double-click or + to add")
                            Accessible.ignored: true
                            color: Theme.muted
                            font.pixelSize: Theme.fontMicro
                        }
                        Item { Layout.fillWidth: true; visible: Theme.minimal }
                        Icon {
                            name: "chevD"
                            size: 14
                            rotation: App.puzzle.allPiecesOpen ? 0 : -90
                        }
                    }
                }

                FocusScope { // ⑫
                    id: libraryKeys
                    objectName: "puzzle.library.chips"
                    visible: App.puzzle.allPiecesOpen
                    anchors { left: parent.left; right: parent.right; top: libraryHeader.bottom; bottom: parent.bottom
                              leftMargin: 6; rightMargin: 6; bottomMargin: 3 }
                    activeFocusOnTab: App.puzzle.library.count > 0
                    Accessible.role: Accessible.List
                    Accessible.name: qsTr("All pieces")

                    onActiveFocusChanged: if (activeFocus && App.keyboardCues.navigating) root.enterPieces(App.puzzle.library)
                    Keys.onShortcutOverride: (event) => {
                        if (App.commands.keyInScope("puzzlePieces", event.key, event.modifiers) !== "")
                            event.accepted = true
                    }
                    Keys.onPressed: (event) => root.pieceKey(event, App.puzzle.library, libraryList)
                    Keys.onReturnPressed: App.puzzle.stepPiece(App.puzzle.selectedPiece, +1)
                    Keys.onEnterPressed: App.puzzle.stepPiece(App.puzzle.selectedPiece, +1)

                    Flickable {
                        id: libraryView
                        objectName: "puzzle.library.list"
                        anchors.fill: parent
                        contentHeight: libraryFlow.height
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                        Flow {
                            id: libraryFlow
                            width: parent.width
                            spacing: 3
                            // a chip for every shape: shut, the section's are made in the background
                            BatchedRows {
                                id: libraryRows
                                queue: App.builds
                                source: App.puzzle.library
                                shown: libraryFlow.visible
                            }
                            Repeater {
                                id: libraryList
                                model: libraryRows
                                delegate: LibraryChip {
                                    id: chip
                                    required property int shape
                                    required idText
                                    required label
                                    required property color chipColor
                                    required used
                                    required countText
                                    objectName: "puzzle.library.chip." + shape
                                    swatch: chipColor
                                    selected: shape === App.puzzle.selectedPiece
                                    focusRing: selected && libraryKeys.activeFocus && App.keyboardCues.navigating
                                    onFocusRingChanged: if (focusRing) libraryFollower.show(chip)
                                    onTapped: { App.puzzle.selectPiece(shape); libraryKeys.forceActiveFocus() }
                                    onAdded: { App.puzzle.selectPiece(shape); App.puzzle.stepPiece(shape, +1) }
                                    onReduced: { App.puzzle.selectPiece(shape); App.puzzle.stepPiece(shape, -1) }
                                    onMenuRequested: (x, y) => root.openPieceMenu(shape, chip, x, y)
                                    onDragStarted: root.beginDrag(shape, "library", idText)
                                    onDragMoved: (x, y) => root.moveDrag(chip, x, y)
                                    onDragEnded: root.endDrag()
                                    onDragCancelled: root.cancelDrag()
                                }
                            }
                        }
                    }
                    ScrollToFocus { id: libraryFollower; view: libraryView }
                }

                DashedFrame {
                    objectName: "puzzle.library.dropZone"
                    anchors { fill: parent; margins: 2 }
                    visible: root.dragFrom === "used"
                    fill: Theme.accentSoft
                    lineWidth: 2
                }
            }

                // with no problems the list stays at the top
                Item {
                    Layout.fillHeight: true
                    visible: App.puzzle.problem < 0
                }
            }
        }
        ScrollToFocus { view: body; follow: true }

        SummaryStrip { // ⑬
            objectName: "puzzle.summary"
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
            visible: App.puzzle.problem >= 0
            ok: App.puzzle.fits
            title: App.puzzle.summaryTitle
            detail: App.puzzle.summaryDetail

            Row {
                objectName: "puzzle.summary.holeLimit"
                readonly property real summaryExtraWidth: parent ? parent.width : 0
                visible: App.puzzle.holeLimitText.length > 0
                spacing: 6
                Text {
                    // what is left beside the button: Clear stays on the card
                    width: Math.min(implicitWidth, parent.summaryExtraWidth - holeLimitClear.width - 6)
                    text: App.puzzle.holeLimitText
                    elide: Text.ElideRight
                    color: Theme.muted
                    font.pixelSize: Theme.fontSecondary
                    height: 24
                    verticalAlignment: Text.AlignVCenter
                }
                BtButton {
                    id: holeLimitClear
                    objectName: "puzzle.summary.holeLimit.clear"
                    small: true
                    text: qsTr("Clear")
                    Accessible.name: qsTr("Clear the limit on holes")
                    onClicked: App.puzzle.clearHoleLimit()
                }
            }
        }
    }

    AppMenu { // ⑭
        id: pieceMenu
        objectName: "puzzle.pieceMenu"
        property int shape: -1

        AppMenuItem {
            objectName: "puzzle.pieceMenu.more"
            text: qsTr("One more")
            onTriggered: App.puzzle.stepPiece(pieceMenu.shape, +1)
        }
        AppMenuItem {
            objectName: "puzzle.pieceMenu.fewer"
            text: qsTr("One fewer")
            enabled: App.puzzle.pieceMax > 0
            onTriggered: App.puzzle.stepPiece(pieceMenu.shape, -1)
        }
        AppMenuItem {
            objectName: "puzzle.pieceMenu.remove"
            text: qsTr("Remove from puzzle")
            enabled: App.puzzle.pieceMax > 0
            onTriggered: App.puzzle.setCount(pieceMenu.shape, 0)
        }
        AppMenuSeparator {}
        AppMenuItem {
            objectName: "puzzle.pieceMenu.result"
            text: qsTr("Set as result")
            onTriggered: App.puzzle.setResult(pieceMenu.shape)
        }
    }

    AppMenu { // ⑮
        id: moreMenu
        objectName: "puzzle.moreMenu"
        AppMenuItem {
            objectName: "puzzle.moreMenu.addAll"
            text: qsTr("Add one of each piece")
            onTriggered: App.puzzle.addOneOfEach()
        }
        AppMenuItem {
            objectName: "puzzle.moreMenu.range"
            text: qsTr("Set range for all pieces…")
            enabled: App.puzzle.pieces.count > 0
            onTriggered: root.setRangeRequested()
        }
        AppMenuSeparator {}
        AppMenuItem {
            objectName: "puzzle.moreMenu.removeAll"
            text: qsTr("Remove all pieces")
            danger: true
            enabled: App.puzzle.pieces.count > 0
            onTriggered: App.puzzle.removeAllPieces()
        }
    }

    /* ⑯ Each shape with its picture (as the 3D view's Home shows it), code,
     * label and voxels. The current result has a selected row's look; the
     * shape with the most voxels -- the likely result -- has its count in the
     * accent, a hint only, not a row tint a selection could be mistaken for
     * (product decisions 2026-11-14 and -15). */
    AppMenu {
        id: resultMenu
        objectName: "puzzle.resultMenu"
        /* the result's shape when the menu opened, -1 for none */
        property int current: -1
        /* Its rows -- a picture, three texts and a layout each, for every shape --
         * are made the first time it opens, not with the card, and then follow
         * the shapes. */
        property bool wanted: false
        /* the largest shape's voxels; 0 with one shape, nothing to hint at */
        readonly property int mostVoxels: {
            let most = 0
            for (let i = 0; i < shapeItems.count; i++) {
                const it = shapeItems.objectAt(i)
                if (it)
                    most = Math.max(most, it.voxels)
            }
            return shapeItems.count > 1 ? most : 0
        }
        Instantiator {
            id: shapeItems
            active: resultMenu.wanted
            model: App.shapes
            delegate: AppMenuItem {
                id: shapeItem
                required property int index
                required property string idText
                required property string label
                required property color chipColor
                required property int fixedCount
                required property int variableCount
                readonly property int voxels: fixedCount + variableCount
                readonly property bool largest: resultMenu.mostVoxels > 0 && voxels === resultMenu.mostVoxels
                objectName: "puzzle.resultMenu.shape." + index
                text: label.length > 0 ? qsTr("%1 %2 · %3 voxels").arg(idText).arg(label).arg(voxels)
                                       : qsTr("%1 · %2 voxels").arg(idText).arg(voxels)
                Accessible.description: largest ? qsTr("The shape with the most voxels") : ""
                // not checkable: a click would toggle checked and break the binding
                readonly property bool current: index === resultMenu.current
                Accessible.checkable: true
                Accessible.checked: current
                implicitHeight: 36
                leftPadding: 4
                rightPadding: 8
                background: Rectangle {
                    radius: Theme.radiusChip
                    color: shapeItem.current ? Theme.accentSoft : shapeItem.highlighted ? Theme.panel2 : "transparent"
                    border.width: shapeItem.current ? 1 : 0
                    border.color: shapeItem.highlighted ? Theme.accent : Theme.accentEdge
                }
                contentItem: RowLayout {
                    spacing: 8
                    ShapePicture {
                        objectName: "puzzle.resultMenu.shape." + shapeItem.index + ".picture"
                        Layout.preferredWidth: 28
                        Layout.preferredHeight: 28
                        document: App.document
                        shape: shapeItem.index
                        colour: shapeItem.chipColor
                    }
                    Text {
                        text: shapeItem.idText
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                        font.weight: Font.DemiBold
                    }
                    Text {
                        visible: text.length > 0
                        Layout.maximumWidth: 180
                        text: shapeItem.label
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                    }
                    Item { Layout.fillWidth: true; Layout.minimumWidth: 16 }
                    Text {
                        objectName: "puzzle.resultMenu.shape." + shapeItem.index + ".voxels"
                        text: qsTr("%1 voxels").arg(shapeItem.voxels)
                        color: shapeItem.largest ? Theme.accent : Theme.muted
                        font.pixelSize: Theme.fontSecondary
                        font.weight: shapeItem.largest ? Font.DemiBold : Font.Normal
                    }
                }
                onTriggered: App.puzzle.setResult(index)
            }
            onObjectAdded: (i, item) => resultMenu.insertItem(i, item as MenuItem)
            onObjectRemoved: (i, item) => resultMenu.removeItem(item as MenuItem)
        }
    }

    Rectangle { // ⑰
        id: ghost
        objectName: "puzzle.dragGhost"
        visible: root.dragFrom !== ""
        x: root.dragAt.x + 8
        y: root.dragAt.y + 8
        z: 10
        width: ghostText.implicitWidth + 14
        height: 24
        radius: Theme.radiusChip
        color: Theme.panel
        border.width: 1
        border.color: Theme.accent
        Text {
            id: ghostText
            anchors.centerIn: parent
            text: root.dragText
            Accessible.ignored: true
            color: Theme.text
            font.pixelSize: Theme.fontSecondary
            font.weight: Font.Bold
        }
    }
}
