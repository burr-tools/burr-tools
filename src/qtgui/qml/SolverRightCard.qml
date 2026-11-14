pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import BurrTools.Ui

// C20 Solver right card: browse, play and manage the results. Everything the
// user does with a solution happens here; the 3D view only shows it. The
// state is SolverController's (App.solver), the rules btui::solver's.
//
// Wireframe (each number marks its code below):
//
//   ┌──────────────────────────────────────────┐
//   │ Solutions (5)            ①[List|Slider][◨]│
//   ├──────────────────────────────────────────┤
//   │ ②#   LEVEL       MOVES ▼     PCS   DA    │   the headers sort, again reverses
//   │ ③ 3  8.2.1       [███ 11]     6    ✓     │   rows; the selected one ringed
//   │   1  5.1.1       [██  7 ]     6    ✓     │
//   │ ④5 solutions · ↑↓ solution · ←→ move ...  │
//   │ ⑤ Solution 3  of 5                        │   (Slider view instead of ②–④)
//   │   [‹]  ──────●────────  [›]               │
//   │   [#3][Level][Moves][Pieces][DA][3/5]     │
//   │ ⑥ No results yet  Press Start ...         │   (no rows: what to do)
//   ├──────────────────────────────────────────┤
//   │ ⑦DISASSEMBLY PLAYER         Move 4.2 / 11 │   fixed height
//   │  [⏮][◀][▶][▶|][⏭]               [1× ⌄]  │   or a message + Analyse
//   │  ───────●──────────────────────────────  │
//   │ ⑧[Number|Level|Moves ▼|Pieces]           │   Slider view only
//   │ ⑨[Manage solutions ⌄]  [Export to STL]   │
//   ├──────────────────────────────────────────┤
//   │ ⑩Piece display                [Show all] │
//   │  [■ S1] [▫ S2] [□ S3.1] [■ S3.2] ...     │   fixed height, scrolls
//   └──────────────────────────────────────────┘
Card {
    id: root
    objectName: "solver.right"
    title: qsTr("Solutions")
    collapseIcon: "panelR"
    collapseObjectName: "solver.right.collapse"
    onCollapseClicked: App.layout.rightCollapsed = true

    // Manage ▸ Remove: Main asks before (PR-32), then App.solver.removeSolutions
    signal removeRequested(int kind)
    signal exportRequested()

    readonly property bool listView: App.solver.view === "list"
    // density.md §1: Solver sections 9 x 12, Minimal 7 x 10
    readonly property real pad: Theme.solverSectionH
    readonly property real vpad: Theme.solverSectionV
    readonly property list<string> daWords: [ qsTr("stored"), qsTr("level only"), qsTr("cannot be disassembled"), qsTr("not checked") ]
    // the Disassembly tile's shorter words (C20)
    readonly property list<string> daTileWords: [ qsTr("stored"), qsTr("level only"), qsTr("none exists"), qsTr("not checked") ]
    readonly property list<string> keyWords: [ qsTr("Number"), qsTr("Level"), qsTr("Moves"), qsTr("Pieces") ]
    readonly property string keysHelp: qsTr("↑↓ solution · ←→ move · Space play")
    readonly property real flexWidth: list.width - Theme.solverColNumber - Theme.solverColMoves - Theme.solverColPieces - Theme.solverColDa

    // the list, the slider and the seekbar share one key map (overview/04 §8)
    function solverKey(context: string, event: KeyEvent): void {
        if ((event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace) && context === "list") {
            if (App.solver.canManage)
                root.removeRequested(0)
            event.accepted = true
            return
        }
        event.accepted = App.solver.key(context, event.key, (event.modifiers & Qt.ShiftModifier) !== 0)
        // a selection the keys made is brought into view; one the solver moved is not
        if (event.accepted && context === "list" && App.solver.selectedRow >= 0)
            list.positionViewAtIndex(App.solver.selectedRow, ListView.Contain)
    }

    // the map's keys are this control's while it has the focus, before any
    // window shortcut (the layer keys, Space) takes them; Esc only while it
    // has something to pause, else the window's Esc ladder has it
    function claimsKey(event: KeyEvent): bool {
        if ((event.modifiers & ~(Qt.ShiftModifier | Qt.KeypadModifier)) !== 0)
            return false
        switch (event.key) {
        case Qt.Key_Space: case Qt.Key_Return: case Qt.Key_Enter: case Qt.Key_Left: case Qt.Key_Right:
        case Qt.Key_Up: case Qt.Key_Down: case Qt.Key_Home: case Qt.Key_End: case Qt.Key_PageUp:
        case Qt.Key_PageDown: case Qt.Key_Delete: case Qt.Key_Backspace:
            return true
        case Qt.Key_Escape:
            return App.solver.playing
        default:
            return false
        }
    }

    // a control that hides or disables under the keyboard hands it to the list
    function focusList(): void {
        if (list.visible)
            list.forceActiveFocus(App.keyboardCues.navigating ? Qt.TabFocusReason : Qt.OtherFocusReason)
    }

    component SliderHandle: Rectangle {
        property Slider slider
        property real size: 16
        x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
        y: slider.topPadding + slider.availableHeight / 2 - height / 2
        width: size
        height: size
        radius: size / 2
        color: Theme.panel
        border.width: 2
        border.color: Theme.accent
        // the keyboard's ring, outside the handle
        Rectangle {
            anchors { fill: parent; margins: -4 }
            visible: parent.slider.visualFocus
            radius: width / 2
            color: "transparent"
            border.width: 2
            border.color: Theme.accent
        }
    }

    // a tile of the Slider view (C20): name over value, the value's tooltip when cut
    component Tile: Rectangle {
        id: tile
        property string name
        property string value
        implicitHeight: Theme.listRow + 10
        radius: Theme.radiusControl
        color: Theme.panel2
        Accessible.role: Accessible.StaticText
        Accessible.name: qsTr("%1: %2").arg(name).arg(value)
        Column {
            anchors { fill: parent; margins: 6 }
            Text {
                text: tile.name
                color: Theme.muted
                font.pixelSize: Theme.fontMicro
                textFormat: Text.PlainText
                Accessible.ignored: true
            }
            Text {
                id: tileValue
                width: parent.width
                text: tile.value
                color: Theme.text
                font.pixelSize: Theme.fontSecondary
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                textFormat: Text.PlainText
                Accessible.ignored: true
            }
        }
        HoverHandler { id: tileHover }
        BtToolTip { shown: tileHover.hovered && tileValue.truncated; text: tile.value }
    }

    // a Manage row that is not a Remove item: no preview, by hover or by keys
    component ManageRow: CheckRow {
        width: parent ? parent.width : implicitWidth
        implicitHeight: Theme.listRow - 4
        onHoveredChanged: if (hovered) App.solver.previewRemove = -1
        onActiveFocusChanged: if (activeFocus) App.solver.previewRemove = -1
    }

    headerExtras: [
        Chip {
            objectName: "solver.count"
            kind: Chip.Count
            text: String(App.solver.shownCount)
            anchors.verticalCenter: parent ? parent.verticalCenter : undefined
        }
    ]
    headerActions: [
        Segmented { // ①
            objectName: "solver.view"
            options: [ { value: "list", label: qsTr("List") }, { value: "slider", label: qsTr("Slider") } ]
            value: App.solver.view
            Accessible.name: qsTr("Show the solutions as")
            Accessible.description: root.listView ? qsTr("All solutions at a glance") : qsTr("One solution at a time with a slider")
            onActivated: (v) => App.solver.view = v
        }
    ]

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // --- the selection area -------------------------------------------------------
        Item {
            id: area
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 150

            // the list's sticky header (PR-28): one Tab stop, ← → between the columns
            FocusScope { // ②
                id: listHeader
                objectName: "solver.list.header"
                visible: root.listView && App.solver.shownCount > 0
                anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: root.pad; rightMargin: root.pad }
                height: Theme.solverListRow
                activeFocusOnTab: true
                property int current: App.solver.sortKey
                Accessible.role: Accessible.Grouping
                Accessible.name: qsTr("Sort the solutions")
                function usable(k: int): bool { return k === 0 || k === 3 || App.solver.levelSortable }
                function move(by: int): void {
                    for (let k = current + by; k >= 0 && k <= 3; k += by)
                        if (usable(k)) {
                            current = k
                            return
                        }
                }
                Keys.onLeftPressed: move(-1)
                Keys.onRightPressed: move(1)
                Keys.onSpacePressed: App.solver.clickSort(current)
                Keys.onReturnPressed: App.solver.clickSort(current)
                Keys.onEnterPressed: App.solver.clickSort(current)

                Row {
                    anchors.fill: parent
                    Repeater {
                        model: 4
                        delegate: AbstractButton {
                            id: head
                            required property int index
                            readonly property bool active: App.solver.sortKey === index
                            readonly property list<string> labels: [ qsTr("#"), qsTr("Level"), qsTr("Moves"), qsTr("Pcs") ]
                            readonly property list<real> widths: [ Theme.solverColNumber, root.flexWidth, Theme.solverColMoves, Theme.solverColPieces ]
                            objectName: "solver.list.sort." + index
                            width: widths[index]
                            height: listHeader.height
                            hoverEnabled: true
                            focusPolicy: Qt.NoFocus
                            // Level and Moves wait for some disassembly information
                            enabled: listHeader.usable(index)
                            Accessible.role: Accessible.ColumnHeader
                            Accessible.name: qsTr("Sort by %1").arg(root.keyWords[index])
                            Accessible.description: active ? (App.solver.sortDescending ? qsTr("descending") : qsTr("ascending")) : ""
                            onClicked: {
                                listHeader.current = index
                                listHeader.forceActiveFocus(Qt.MouseFocusReason)
                                App.solver.clickSort(index)
                            }
                            background: Rectangle {
                                readonly property bool ring: listHeader.activeFocus && App.keyboardCues.navigating
                                                             && listHeader.current === head.index
                                visible: head.hovered || ring
                                color: head.hovered ? Theme.panel2 : Theme.panel
                                radius: Theme.radiusChip
                                border.width: ring ? 2 : 0
                                border.color: Theme.accent
                            }
                            contentItem: Text {
                                leftPadding: 6
                                text: head.active ? qsTr("%1 %2").arg(head.labels[head.index]).arg(App.solver.sortDescending ? "▼" : "▲")
                                                  : head.labels[head.index]
                                color: head.active ? Theme.accent : Theme.muted
                                opacity: head.enabled ? 1 : 0.5
                                font.pixelSize: Theme.fontMicro
                                font.weight: Font.DemiBold
                                font.capitalization: Font.AllUppercase
                                verticalAlignment: Text.AlignVCenter
                                textFormat: Text.PlainText
                                elide: Text.ElideRight
                            }
                        }
                    }
                    Text {
                        width: Theme.solverColDa
                        height: listHeader.height
                        text: qsTr("DA")
                        leftPadding: 6
                        color: Theme.muted
                        font.pixelSize: Theme.fontMicro
                        font.weight: Font.DemiBold
                        verticalAlignment: Text.AlignVCenter
                        Accessible.ignored: true
                    }
                }
            }

            ListView { // ③
                id: list
                objectName: "solver.list"
                visible: root.listView && App.solver.shownCount > 0
                anchors { left: parent.left; right: parent.right; top: listHeader.bottom
                          bottom: keysHint.visible ? keysHint.top : parent.bottom; bottomMargin: keysHint.visible ? 4 : root.vpad
                          leftMargin: root.pad; rightMargin: root.pad }
                clip: true
                model: App.solver.solutions
                currentIndex: App.solver.selectedRow
                highlightMoveDuration: 0
                boundsBehavior: Flickable.StopAtBounds
                activeFocusOnTab: true
                keyNavigationEnabled: false
                // the rows keep no state of their own: thousands of them scroll by reusing
                reuseItems: true
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                Accessible.role: Accessible.List
                Accessible.name: qsTr("Solutions")
                // the key hint, which Minimal hides (AC-DEN-03)
                Accessible.description: root.keysHelp
                Keys.onShortcutOverride: (event) => event.accepted = root.claimsKey(event)
                Keys.onPressed: (event) => root.solverKey("list", event)

                HoverHandler { id: listHover }
                BtToolTip { shown: Theme.minimal && (listHover.hovered || (list.activeFocus && App.keyboardCues.navigating)); text: root.keysHelp }
                // the keyboard's ring round the whole list, besides the selected row's
                Rectangle {
                    anchors.fill: parent
                    z: 2
                    visible: list.activeFocus && App.keyboardCues.navigating
                    radius: Theme.radiusControl
                    color: "transparent"
                    border.width: 2
                    border.color: Theme.accent
                }

                delegate: Item {
                    id: row
                    required property int index
                    required property int number
                    required property string level
                    required property int moves
                    required property double movesFraction
                    required property int pieces
                    required property int da
                    required property bool willDelete
                    readonly property bool selected: index === App.solver.selectedRow
                    objectName: "solver.list.row." + index
                    width: ListView.view.width
                    height: Theme.solverListRow
                    Accessible.role: Accessible.ListItem
                    Accessible.name: qsTr("Assembly %1, level %2, %3 moves, %4 pieces, disassembly %5")
                                       .arg(number).arg(level.length > 0 ? level : qsTr("unknown"))
                                       .arg(moves >= 0 ? moves : qsTr("unknown")).arg(pieces).arg(root.daWords[da])
                    Accessible.selectable: true
                    Accessible.selected: selected
                    Accessible.onPressAction: App.solver.selectRow(index)
                    // a fill only where there is one to draw (hover, selection, the delete preview)
                    Rectangle {
                        anchors.fill: parent
                        visible: row.willDelete || row.selected || rowHover.hovered
                        radius: Theme.radiusControl
                        color: row.willDelete ? Theme.dangerSoft : row.selected ? Theme.accentSoft : Theme.panel2
                        border.width: row.selected || row.willDelete ? (row.selected && list.activeFocus && App.keyboardCues.navigating ? 2 : 1) : 0
                        border.color: row.willDelete ? Theme.danger : Theme.accent
                    }
                    HoverHandler { id: rowHover }
                    TapHandler {
                        onTapped: {
                            list.forceActiveFocus(Qt.MouseFocusReason)
                            App.solver.selectRow(row.index)
                        }
                        onDoubleTapped: {
                            App.solver.selectRow(row.index)
                            if (App.solver.playerState === SolverController.Playable && !App.solver.playing)
                                App.solver.togglePlay()
                        }
                    }
                    Row {
                        anchors.fill: parent
                        Text {
                            width: Theme.solverColNumber
                            height: parent.height
                            leftPadding: 6
                            text: row.number
                            font.pixelSize: Theme.fontSecondary
                            font.weight: Font.Bold
                            font.strikeout: row.willDelete
                            color: Theme.text
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                            Accessible.ignored: true
                        }
                        Text {
                            id: levelText
                            width: root.flexWidth
                            height: parent.height
                            leftPadding: 6
                            text: row.level.length > 0 ? row.level : "—"
                            font.family: Theme.monoFamily
                            font.pixelSize: Theme.fontSecondary
                            font.strikeout: row.willDelete
                            color: Theme.text
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                            textFormat: Text.PlainText
                            Accessible.ignored: true
                            HoverHandler { id: levelHover }
                            // made only while it is wanted: a tooltip per row is a popup per row
                            Loader {
                                active: levelHover.hovered && levelText.truncated
                                sourceComponent: BtToolTip { shown: true; text: row.level }
                            }
                        }
                        Item {
                            width: Theme.solverColMoves
                            height: parent.height
                            Rectangle {
                                anchors { left: parent.left; leftMargin: 4; verticalCenter: parent.verticalCenter }
                                width: (parent.width - 8) * row.movesFraction
                                height: parent.height - 8
                                radius: Theme.radiusChip
                                color: Theme.accentSoft
                                visible: row.moves > 0
                            }
                            Text {
                                anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
                                text: row.moves >= 0 ? row.moves : "—"
                                font.pixelSize: Theme.fontSecondary
                                font.strikeout: row.willDelete
                                color: Theme.text
                                Accessible.ignored: true
                            }
                        }
                        Text {
                            width: Theme.solverColPieces
                            height: parent.height
                            leftPadding: 6
                            text: row.pieces
                            font.pixelSize: Theme.fontSecondary
                            font.strikeout: row.willDelete
                            color: Theme.text
                            verticalAlignment: Text.AlignVCenter
                            Accessible.ignored: true
                        }
                        DaMark {
                            width: Theme.solverColDa
                            height: parent.height
                            da: row.da
                        }
                    }
                }
            }

            Text { // ④
                id: keysHint
                objectName: "solver.list.footer"
                visible: root.listView && App.solver.shownCount > 0 && !Theme.minimal
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: root.pad; rightMargin: root.pad
                          bottomMargin: root.vpad }
                text: (App.solver.shownCount === 1 ? qsTr("1 solution") : qsTr("%1 solutions").arg(App.solver.shownCount))
                      + qsTr(" · ") + root.keysHelp
                color: Theme.muted
                font.pixelSize: Theme.fontMicro
                elide: Text.ElideRight
                textFormat: Text.PlainText
                // said by the list (its description) instead
                Accessible.ignored: true
            }

            Column { // ⑤
                id: sliderView
                objectName: "solver.sel"
                visible: !root.listView && App.solver.shownCount > 0
                anchors { left: parent.left; right: parent.right; top: parent.top
                          leftMargin: root.pad; rightMargin: root.pad; topMargin: root.vpad }
                spacing: 8
                Row {
                    spacing: 6
                    Text {
                        id: sliderTitle
                        text: qsTr("Solution %1").arg(App.solver.selectedRow + 1)
                        color: Theme.text
                        font.pixelSize: Theme.fontActionTitle
                        font.weight: Font.Bold
                    }
                    Text {
                        anchors.baseline: sliderTitle.baseline
                        text: App.solver.disassemble ? qsTr("of %1").arg(App.solver.shownCount)
                                                     : qsTr("of %1 assemblies").arg(App.solver.shownCount)
                        color: Theme.muted
                        font.pixelSize: Theme.fontSecondary
                    }
                }
                RowLayout {
                    width: parent.width
                    spacing: 6
                    IconButton {
                        objectName: "solver.slider.prev"
                        iconName: "transport-step-back"
                        tip: qsTr("Previous solution")
                        // a Tab stop, but a click leaves the keyboard where it was (sticky focus)
                        focusPolicy: Qt.TabFocus
                        enabled: App.solver.selectedRow > 0
                        onClicked: App.solver.selectRow(App.solver.selectedRow - 1)
                    }
                    Slider {
                        id: solutionSlider
                        objectName: "solver.slider"
                        Layout.fillWidth: true
                        from: 1
                        to: Math.max(1, App.solver.shownCount)
                        stepSize: 1
                        snapMode: Slider.SnapAlways
                        live: true
                        value: App.solver.selectedRow + 1
                        Accessible.name: qsTr("Solution")
                        Accessible.description: root.keysHelp
                        // scrubbing: the title and tiles at once, the scene once a
                        // frame, the slider stays (AC-C20-02)
                        onMoved: App.solver.scrubTo(Math.round(value) - 1)
                        Keys.priority: Keys.BeforeItem
                        Keys.onShortcutOverride: (event) => event.accepted = root.claimsKey(event)
                        Keys.onPressed: (event) => root.solverKey("slider", event)
                        background: Rectangle {
                            x: solutionSlider.leftPadding
                            y: solutionSlider.topPadding + solutionSlider.availableHeight / 2 - height / 2
                            width: solutionSlider.availableWidth
                            height: 4
                            radius: 2
                            color: Theme.line2
                        }
                        handle: SliderHandle { slider: solutionSlider }
                    }
                    IconButton {
                        objectName: "solver.slider.next"
                        iconName: "transport-step-forward"
                        tip: qsTr("Next solution")
                        focusPolicy: Qt.TabFocus
                        enabled: App.solver.selectedRow < App.solver.shownCount - 1
                        onClicked: App.solver.selectRow(App.solver.selectedRow + 1)
                    }
                }
                // six tiles made once: scrubbing changes their words, not the items
                Grid {
                    objectName: "solver.meta"
                    id: tiles
                    width: parent.width
                    columns: 3
                    spacing: 6
                    readonly property real tileWidth: (width - 2 * spacing) / 3
                    Tile { width: tiles.tileWidth; name: qsTr("Assembly"); value: "#" + App.solver.selectedNumber }
                    Tile { width: tiles.tileWidth; name: qsTr("Level"); value: App.solver.selectedLevel }
                    Tile { width: tiles.tileWidth; name: qsTr("Moves"); value: App.solver.selectedMoves }
                    Tile { width: tiles.tileWidth; name: qsTr("Pieces"); value: String(App.solver.selectedPieces) }
                    Tile { width: tiles.tileWidth; name: qsTr("Disassembly"); value: root.daTileWords[App.solver.selectedDa] }
                    Tile {
                        width: tiles.tileWidth
                        name: qsTr("Position")
                        value: qsTr("%1 / %2").arg(App.solver.selectedRow + 1).arg(App.solver.shownCount)
                    }
                }
            }

            Column { // ⑥
                objectName: "solver.empty"
                visible: App.solver.shownCount === 0
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: root.pad }
                spacing: 6
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: App.solver.emptyTitle
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    font.weight: Font.Bold
                    wrapMode: Text.WordWrap
                    textFormat: Text.PlainText
                }
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: App.solver.emptyText
                    color: Theme.muted
                    font.pixelSize: Theme.fontSecondary
                    wrapMode: Text.WordWrap
                    textFormat: Text.PlainText
                }
                BtButton {
                    objectName: "solver.empty.showAssemblies"
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: App.solver.emptyState === SolverController.NoneDisassemblable && App.solver.disassemble
                    small: true
                    text: qsTr("Show the assemblies")
                    onClicked: App.solver.showAssemblies()
                    // it goes with what it did: the keyboard moves on to the list
                    onActiveFocusChanged: if (!activeFocus && !visible) Qt.callLater(root.focusList)
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.line }

        // --- the disassembly player (PR-29): one height in every state ------------------
        Item { // ⑦
            objectName: "solver.player"
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.player
            Layout.minimumHeight: Theme.player
            Layout.maximumHeight: Theme.player
            readonly property bool playable: App.solver.playerState === SolverController.Playable

            Column {
                anchors { fill: parent; leftMargin: root.pad; rightMargin: root.pad; topMargin: root.vpad; bottomMargin: root.vpad }
                spacing: Theme.minimal ? 4 : 8
                visible: parent.playable
                RowLayout {
                    width: parent.width
                    Text {
                        // Minimal keeps the counter, not the title (density.md §1)
                        Layout.fillWidth: true
                        visible: !Theme.minimal
                        text: qsTr("Disassembly player")
                        color: Theme.muted
                        font.pixelSize: Theme.fontMicro
                        font.weight: Font.DemiBold
                        font.capitalization: Font.AllUppercase
                    }
                    Item { Layout.fillWidth: true; visible: Theme.minimal }
                    Text {
                        objectName: "solver.player.move"
                        text: App.solver.moveText
                        color: Theme.text
                        font.pixelSize: Theme.fontSecondary
                        font.weight: Font.DemiBold
                        textFormat: Text.PlainText
                    }
                }
                RowLayout {
                    width: parent.width
                    spacing: 4
                    // Tab stops, but a click leaves the keyboard on the list (sticky focus, C20)
                    IconButton { objectName: "solver.player.start"; iconName: "transport-skip-start"; tip: qsTr("Assembled (Shift+←)"); focusPolicy: Qt.TabFocus; onClicked: App.solver.transport("start") }
                    IconButton { objectName: "solver.player.back"; iconName: "transport-step-back"; tip: qsTr("One move back (←)"); focusPolicy: Qt.TabFocus; onClicked: App.solver.transport("back") }
                    BtButton {
                        id: playButton
                        objectName: "solver.player.play"
                        primary: true
                        small: true
                        focusPolicy: Qt.TabFocus
                        Layout.preferredWidth: Theme.button + 12
                        text: ""
                        readonly property string tip: App.solver.playing ? qsTr("Pause (Space)") : qsTr("Play (Space)")
                        Accessible.name: App.solver.playing ? qsTr("Pause") : qsTr("Play")
                        Accessible.description: tip
                        onClicked: App.solver.togglePlay()
                        BtToolTip { shown: playButton.hovered || playButton.visualFocus; text: playButton.tip }
                        Icon {
                            anchors.centerIn: parent
                            name: App.solver.playing ? "transport-pause" : "transport-play"
                            size: 16
                            color: Theme.onAccent
                        }
                    }
                    IconButton { objectName: "solver.player.fwd"; iconName: "transport-step-forward"; tip: qsTr("One move forward (→)"); focusPolicy: Qt.TabFocus; onClicked: App.solver.transport("fwd") }
                    IconButton { objectName: "solver.player.end"; iconName: "transport-skip-end"; tip: qsTr("Fully disassembled (Shift+→)"); focusPolicy: Qt.TabFocus; onClicked: App.solver.transport("end") }
                    Item { Layout.fillWidth: true }
                    Dropdown {
                        objectName: "solver.player.speed"
                        Layout.preferredWidth: 76
                        implicitWidth: 76
                        readonly property list<real> speeds: [ 0.5, 1, 2, 4, 8 ]
                        model: [ "0.5×", "1×", "2×", "4×", "8×" ]
                        currentIndex: speeds.indexOf(App.solver.speed)
                        Accessible.name: qsTr("Playback speed")
                        onActivated: (i) => {
                            App.solver.speed = speeds[i]
                            currentIndex = Qt.binding(() => speeds.indexOf(App.solver.speed))
                        }
                    }
                }
                Slider {
                    id: seekbar
                    objectName: "solver.player.seek"
                    width: parent.width
                    from: 0
                    to: Math.max(0.05, App.solver.totalMoves)
                    stepSize: 0.05
                    value: App.solver.position
                    live: true
                    Accessible.name: qsTr("Move")
                    Accessible.description: App.solver.moveText
                    onMoved: App.solver.seek(value)
                    Keys.priority: Keys.BeforeItem
                    Keys.onShortcutOverride: (event) => event.accepted = root.claimsKey(event)
                    Keys.onPressed: (event) => root.solverKey("seekbar", event)
                    background: Rectangle {
                        x: seekbar.leftPadding
                        y: seekbar.topPadding + seekbar.availableHeight / 2 - height / 2
                        width: seekbar.availableWidth
                        height: 4
                        radius: 2
                        color: Theme.line2
                        Rectangle {
                            width: seekbar.visualPosition * parent.width
                            height: parent.height
                            radius: 2
                            color: Theme.accent
                        }
                    }
                    handle: SliderHandle { slider: seekbar; size: 14 }
                }
            }

            // the alternate states, at the same height
            Column {
                objectName: "solver.player.message"
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: root.pad }
                visible: !parent.playable
                spacing: 6
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: Theme.muted
                    font.pixelSize: Theme.fontSecondary
                    textFormat: Text.PlainText
                    text: {
                        switch (App.solver.playerState) {
                        case SolverController.NotChecked: return qsTr("Disassembly was not checked for this assembly.")
                        case SolverController.LevelOnly: return qsTr("Only the level was kept for this solution — the moves were not stored.")
                        case SolverController.NoneStored: return qsTr("No disassembly stored.")
                        case SolverController.CannotDisassemble: return qsTr("This assembly cannot be taken apart.")
                        case SolverController.NoDisassembler: return qsTr("This voxel type has no disassembler: there are no moves to play.")
                        default: return qsTr("Nothing to play yet — run the solver to find assemblies.")
                        }
                    }
                }
                BtButton {
                    objectName: "solver.player.analyse"
                    anchors.horizontalCenter: parent.horizontalCenter
                    // only where a disassembly could exist (AC-C20-04)
                    visible: App.solver.playerState === SolverController.NotChecked
                             || App.solver.playerState === SolverController.LevelOnly
                             || App.solver.playerState === SolverController.NoneStored
                    enabled: App.solver.canAnalyse
                    small: true
                    text: qsTr("Analyse disassembly")
                    onClicked: App.solver.analyse("current")
                    onActiveFocusChanged: if (!activeFocus && !visible) Qt.callLater(root.focusList)
                }
            }
        }

        // --- the Sort-by segment (Slider view; the same state as the headers) -----------
        Segmented { // ⑧
            objectName: "solver.sort"
            visible: !root.listView && App.solver.shownCount > 0
            Layout.leftMargin: root.pad
            Layout.bottomMargin: 6
            Accessible.name: qsTr("Sort by")
            Accessible.description: qsTr("Space or Enter on the option on reverses it")
            value: App.solver.sortKey
            options: [0, 1, 2, 3].map((k) => ({
                value: k,
                label: App.solver.sortKey === k ? qsTr("%1 %2").arg(root.keyWords[k]).arg(App.solver.sortDescending ? "▼" : "▲")
                                                : root.keyWords[k]
            }))
            onActivated: (v) => App.solver.clickSort(v)
        }

        // --- Manage and Export ---------------------------------------------------------------
        RowLayout { // ⑨
            objectName: "solver.act"
            Layout.fillWidth: true
            Layout.leftMargin: root.pad
            Layout.rightMargin: root.pad
            Layout.bottomMargin: root.vpad
            spacing: 6
            BtButton {
                id: manageButton
                objectName: "solver.act.manage"
                Layout.fillWidth: true
                small: true
                text: App.solver.analysing ? App.solver.analysingText : qsTr("Manage solutions")
                enabled: App.solver.canManage
                Accessible.name: qsTr("Manage solutions")
                onClicked: manageMenu.open()
                rightPadding: 26
                Icon {
                    anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                    visible: !App.solver.analysing
                    name: "chevD"
                    size: 12
                }

                Popup {
                    id: manageMenu
                    objectName: "solver.manage"
                    y: -height - 4
                    // its longest item and its legacy abbreviation side by side
                    width: 390
                    padding: 4
                    focus: true
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
                    background: Rectangle {
                        color: Theme.panel
                        border.color: Theme.line2
                        radius: Theme.radiusPopup
                    }
                    onClosed: {
                        App.solver.previewRemove = -1
                        manageButton.forceActiveFocus(App.keyboardCues.navigating ? Qt.TabFocusReason : Qt.MouseFocusReason)
                    }
                    onOpened: {
                        const first = manageItems.itemAt(0)
                        if (first !== null)
                            first.forceActiveFocus(App.keyboardCues.navigating ? Qt.TabFocusReason : Qt.MouseFocusReason)
                    }
                    contentItem: Column {
                        Keys.onUpPressed: (manageMenu.contentItem.Window.activeFocusItem as Item).nextItemInFocusChain(false).forceActiveFocus(Qt.TabFocusReason)
                        Keys.onDownPressed: (manageMenu.contentItem.Window.activeFocusItem as Item).nextItemInFocusChain(true).forceActiveFocus(Qt.TabFocusReason)
                        Text {
                            leftPadding: 8; topPadding: 4; bottomPadding: 2
                            text: qsTr("Remove assemblies")
                            color: Theme.muted
                            font.pixelSize: Theme.fontMicro
                            font.weight: Font.DemiBold
                            font.capitalization: Font.AllUppercase
                            Accessible.ignored: true
                        }
                        Repeater {
                            id: manageItems
                            // btui::RemoveKind: At, Before, After, WithoutDisassembly, All
                            model: [ { kind: 0, label: qsTr("This solution only"), hint: "At" },
                                     { kind: 1, label: qsTr("All before this one"), hint: "Before" },
                                     { kind: 2, label: qsTr("All after this one"), hint: "After" },
                                     { kind: 3, label: qsTr("All without a disassembly"), hint: "w/o DA" },
                                     { kind: 4, label: qsTr("All solutions"), hint: "All" } ]
                            delegate: CheckRow {
                                id: removeItem
                                required property var modelData
                                readonly property int kind: modelData.kind
                                // through what notifies: a method call alone is read once
                                readonly property int count: App.solver.solutions.revision >= 0 && App.solver.selectedRow >= -1
                                                             && App.solver.savedCount >= 0 ? App.solver.removeCount(kind) : 0
                                objectName: "solver.manage.remove." + kind
                                width: manageMenu.availableWidth
                                implicitHeight: Theme.listRow - 4
                                text: modelData.label
                                hint: modelData.hint
                                enabled: count > 0
                                Accessible.description: count === 1 ? qsTr("Removes 1 solution") : qsTr("Removes %1 solutions").arg(count)
                                // the rows it would take are tinted and struck through
                                onHoveredChanged: {
                                    if (hovered)
                                        App.solver.previewRemove = kind
                                    else if (App.solver.previewRemove === kind)
                                        App.solver.previewRemove = -1
                                }
                                onActiveFocusChanged: if (activeFocus) App.solver.previewRemove = kind
                                onClicked: {
                                    const k = kind
                                    manageMenu.close()
                                    root.removeRequested(k)
                                }
                            }
                        }
                        Rectangle { width: parent.width; height: 1; color: Theme.line }
                        Text {
                            leftPadding: 8; topPadding: 4; bottomPadding: 2
                            text: qsTr("Disassemblies")
                            color: Theme.muted
                            font.pixelSize: Theme.fontMicro
                            font.weight: Font.DemiBold
                            font.capitalization: Font.AllUppercase
                            Accessible.ignored: true
                        }
                        ManageRow {
                            objectName: "solver.manage.deleteCurrent"
                            text: qsTr("Delete the disassembly of this solution")
                            hint: "D DA"
                            enabled: App.solver.selectedDa === 0
                            Accessible.description: qsTr("Keeps its level")
                            onClicked: { manageMenu.close(); App.solver.deleteDisassembly(false) }
                        }
                        ManageRow {
                            objectName: "solver.manage.deleteAll"
                            text: qsTr("Delete all disassemblies")
                            hint: "D A DA"
                            Accessible.description: qsTr("Keeps the assemblies and their levels")
                            onClicked: { manageMenu.close(); App.solver.deleteDisassembly(true) }
                        }
                        ManageRow {
                            objectName: "solver.manage.analyseCurrent"
                            text: qsTr("Analyse this assembly's disassembly")
                            hint: "A DA"
                            enabled: App.solver.canAnalyse && App.solver.selectedRow >= 0
                            onClicked: { manageMenu.close(); App.solver.analyse("current") }
                        }
                        ManageRow {
                            objectName: "solver.manage.analyseAll"
                            text: qsTr("Analyse the disassembly of every assembly")
                            hint: "A A DA"
                            enabled: App.solver.canAnalyse
                            onClicked: { manageMenu.close(); App.solver.analyse("all") }
                        }
                        ManageRow {
                            objectName: "solver.manage.analyseMissing"
                            text: qsTr("Analyse only the missing disassemblies")
                            hint: "A M DA"
                            enabled: App.solver.canAnalyse
                            onClicked: { manageMenu.close(); App.solver.analyse("missing") }
                        }
                    }
                }
            }
            BtButton {
                objectName: "solver.act.stl"
                small: true
                text: qsTr("Export to STL")
                enabled: App.solver.savedCount > 0 && App.solver.selectedRow >= 0 && !App.solver.solving
                onClicked: root.exportRequested()
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.line }

        // --- the piece display (PR-30): its height fixed, it scrolls ----------------------------
        Item { // ⑩
            objectName: "solver.pieces"
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.cardHeader + Theme.pieceDisplay
            Layout.minimumHeight: Theme.cardHeader + Theme.pieceDisplay
            Layout.maximumHeight: Theme.cardHeader + Theme.pieceDisplay

            Item {
                id: piecesHeader
                anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: root.pad; rightMargin: root.pad }
                height: Theme.cardHeader
                Text {
                    anchors { left: parent.left; verticalCenter: parent.verticalCenter }
                    text: qsTr("Piece display")
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    font.weight: Font.DemiBold
                }
            }
            Flickable {
                id: chipsFlick
                anchors { left: parent.left; right: parent.right; top: piecesHeader.bottom; leftMargin: root.pad; rightMargin: root.pad }
                height: Theme.pieceDisplay
                contentHeight: chips.implicitHeight + 6
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                Flow {
                    id: chips
                    width: chipsFlick.width
                    spacing: 6
                    topPadding: 3            // room for the focus ring
                    leftPadding: 3
                    Repeater {
                        id: chipList
                        model: App.solver.pieces
                        delegate: PieceChipButton {
                            required property int index
                            required property string pid
                            required property color chipColor
                            required property string tip
                            required pieceState
                            objectName: "solver.pieces.chip." + pid
                            text: pid
                            colour: chipColor
                            toolTipText: tip
                            onClicked: App.solver.cyclePiece(index)
                            Keys.onLeftPressed: { const it = chipList.itemAt(index - 1); if (it !== null) it.forceActiveFocus(Qt.TabFocusReason) }
                            Keys.onRightPressed: { const it = chipList.itemAt(index + 1); if (it !== null) it.forceActiveFocus(Qt.TabFocusReason) }
                        }
                    }
                }
            }
            // a chip the keys reach scrolls into view
            ScrollToFocus { view: chipsFlick; follow: true }
            // after the chips in the Tab order (§9), in the header on screen
            BtButton {
                objectName: "solver.showAll"
                anchors { right: piecesHeader.right; verticalCenter: piecesHeader.verticalCenter }
                small: true
                text: qsTr("Show all")
                enabled: App.solver.pieces.count > 0
                Accessible.description: qsTr("Every piece solid again")
                onClicked: App.solver.showAllPieces()
            }
        }
    }
}
