pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import BurrTools.Ui

// C17 Puzzle right card: the selected piece, then what the selected problem
// holds beside its pieces. The sections come from App.puzzle.sections
// (btui::puzzleSections), which the collapsed rail lists too: a new section
// is a row there and one more Component here.
//
// Wireframe (each number marks its code below):
//
//   ┌──────────────────────────────────────────┐
//   │ Piece & rules                        [◨] │
//   ├──────────────────────────────────────────┤
//   │ ①⌄ Selected piece                     S6 │   a section each, open or shut
//   │  ②[S6] A                                 │   (a hint when none is selected)
//   │     4 voxels · 4 fixed · 0 variable      │
//   │  ③Copies, fixed count      [−   1   +]   │   or Min and Max, side by side
//   │  ④( ) Allow a range (min–max)            │
//   │  ⑤Groups  [G1] [G2]  [+ New]             │   in or out; with several copies
//   │           [G1 − 1 +]                     │   a stepper each: how many
//   ├──────────────────────────────────────────┤
//   │ ⌄ Groups                         1 group │
//   │  ⑥Group 1 [S3 ×1] [S4 ×1]             🗑 │   or "No groups in this problem"
//   ├──────────────────────────────────────────┤
//   │ ⌄ Colour rules                 4 colours │
//   │  ⑦Tick where a piece colour may sit ...  │
//   │  ⑧             RESULT COLOUR             │   the axes' names
//   │              [C1][C2][C3]            (i) │
//   │  P   [C1]    [✓ ][  ][  ]                │   ⑨ a tick: allowed
//   │  I   [C2]    [  ][✓ ][  ]                │
//   │  E   [C3]    [  ][  ][✓ ]                │
//   │  ⑩Piece C2 on result C3: not allowed     │   the cell under the pointer or keys
//   │  ⑪Default voxels fit anywhere ...        │
//   │  ⑫[Each colour on itself][Allow all][Clear]
//   └──────────────────────────────────────────┘
Card {
    id: root
    objectName: "puzzle.right"
    title: qsTr("Piece & rules")
    collapseIcon: "panelR"
    collapseObjectName: "puzzle.right.collapse"
    onCollapseClicked: App.layout.rightCollapsed = true

    // a small heading inside a section
    component Label: Text {
        color: Theme.muted
        font.pixelSize: Theme.fontMicro
        font.weight: Font.DemiBold
        font.capitalization: Font.AllUppercase
    }
    component Help: Text {
        width: parent.width
        visible: !Theme.minimal
        wrapMode: Text.WordWrap
        color: Theme.muted
        font.pixelSize: Theme.fontSecondary
    }

    function sectionHeader(key: string): Item {
        for (let i = 0; i < sectionList.count; i++) {
            const it = sectionList.itemAt(i)
            if (it !== null && it.objectName === "puzzle.section." + key)
                return it
        }
        return null
    }

    // a count of the selected piece: what is typed lands on that piece, before
    // the selection moves on
    component PieceStepper: Stepper {
        id: pieceStepper
        Connections {
            target: App.puzzle
            function onTypingEnding(keep: bool) {
                if (!pieceStepper.entering)
                    return
                if (keep)
                    pieceStepper.commitText()
                else
                    pieceStepper.dropText()
            }
        }
    }

    // the card opens at a section the rail named
    Connections {
        target: App.puzzle
        function onSectionRequested(key: string) { Qt.callLater(root.reveal, key) }
    }
    function reveal(key: string): void {
        for (let i = 0; i < sectionList.count; i++) {
            const it = sectionList.itemAt(i)
            if (it !== null && it.objectName === "puzzle.section." + key) {
                pendingReveal = it
                settleReveal.restart()
                it.forceActiveFocus(App.keyboardCues.navigating ? Qt.TabFocusReason : Qt.MouseFocusReason)
                return
            }
        }
    }
    // the card may still be opening and laying its sections out: at the end of that
    property Item pendingReveal: null
    Timer {
        id: settleReveal
        interval: Theme.durCollapse + 16
        onTriggered: {
            if (root.pendingReveal === null)
                return
            const top = root.pendingReveal.mapToItem(flick.contentItem, 0, 0).y
            flick.contentY = Math.max(0, Math.min(top, flick.contentHeight - flick.height))
            root.pendingReveal = null
        }
    }

    // Tab can reach past the card's edge
    ScrollToFocus { view: flick; follow: true }

    Flickable {
        id: flick
        objectName: "puzzle.right.sections"
        anchors.fill: parent
        contentHeight: sections.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Column {
            id: sections
            width: flick.width
            Repeater {
                id: sectionList
                model: App.puzzle.sections
                delegate: AccordionSection { // ①
                    id: section
                    required property string key
                    required title
                    required summary
                    required open
                    objectName: "puzzle.section." + key
                    width: sections.width
                    onToggled: App.puzzle.setSectionOpen(key, !open)
                    Loader {
                        width: parent.width
                        // the matrix alone is thousands of items at the top colour counts
                        active: section.open
                        sourceComponent: section.key === "piece" ? pieceBody
                                       : section.key === "groups" ? groupsBody : rulesBody
                    }
                }
            }
        }
    }

    Component {
        id: pieceBody
        Column {
            spacing: 5

            Text {
                objectName: "puzzle.piece.hint"
                width: parent.width
                visible: !App.puzzle.pieceShown
                wrapMode: Text.WordWrap
                text: qsTr("Select a piece in the lists or click it in the 3D view to set its count and groups.")
                color: Theme.muted
                font.pixelSize: Theme.fontSecondary
            }

            RowLayout { // ②
                width: parent.width
                visible: App.puzzle.pieceShown
                spacing: 8
                Chip {
                    objectName: "puzzle.piece.chip"
                    text: App.puzzle.pieceIdText
                    fill: App.puzzle.pieceColor
                    textColor: App.puzzle.pieceTextColor
                }
                Column {
                    Layout.fillWidth: true
                    Text {
                        objectName: "puzzle.piece.name"
                        width: parent.width
                        elide: Text.ElideRight
                        text: App.puzzle.pieceName
                        textFormat: Text.PlainText
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                        font.weight: Font.DemiBold
                    }
                    Text {
                        objectName: "puzzle.piece.detail"
                        width: parent.width
                        elide: Text.ElideRight
                        text: App.puzzle.pieceDetail
                        color: Theme.muted
                        font.pixelSize: Theme.fontSecondary
                    }
                }
            }

            RowLayout { // ③
                objectName: "puzzle.piece.fixed"
                width: parent.width
                visible: App.puzzle.pieceShown && !App.puzzle.pieceRange
                Text {
                    objectName: "puzzle.piece.fixed.note"
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: App.puzzle.pieceMax === 0 ? qsTr("Not in this puzzle") : qsTr("Copies, fixed count")
                    color: Theme.muted
                    font.pixelSize: Theme.fontSecondary
                }
                PieceStepper {
                    objectName: "puzzle.piece.count"
                    implicitWidth: 112
                    commitWhileTyping: true
                    liveFrom: 1
                    from: 0
                    to: App.puzzle.maxCount
                    value: App.puzzle.pieceMax
                    tip: qsTr("Copies of %1 in the puzzle").arg(App.puzzle.pieceIdText)
                    onCommitted: (v) => App.puzzle.setCount(App.puzzle.selectedPiece, v)
                }
            }

            Row {
                objectName: "puzzle.piece.range"
                visible: App.puzzle.pieceShown && App.puzzle.pieceRange
                spacing: 6
                // AC-C17-02: each label inside its box; min <= max is the controller's
                PieceStepper {
                    objectName: "puzzle.piece.min"
                    liveFrom: 0
                    commitWhileTyping: true
                    width: (root.width - 40) / 2
                    axis: qsTr("Min")
                    axisColor: Theme.muted
                    onEntryStarted: App.puzzle.holdCounts(App.puzzle.selectedPiece)
                    onEntryEnded: App.puzzle.releaseCounts()
                    from: 0
                    to: App.puzzle.maxCount
                    value: App.puzzle.pieceMin
                    tip: qsTr("Fewest copies of %1").arg(App.puzzle.pieceIdText)
                    onCommitted: (v) => App.puzzle.setPieceMin(App.puzzle.selectedPiece, v)
                }
                PieceStepper {
                    objectName: "puzzle.piece.max"
                    commitWhileTyping: true
                    liveFrom: 1
                    width: (root.width - 40) / 2
                    axis: qsTr("Max")
                    axisColor: Theme.muted
                    onEntryStarted: App.puzzle.holdCounts(App.puzzle.selectedPiece)
                    onEntryEnded: App.puzzle.releaseCounts()
                    from: Math.min(1, App.puzzle.pieceMax)       // 0 would remove the piece
                    to: App.puzzle.maxCount
                    value: App.puzzle.pieceMax
                    tip: qsTr("Most copies of %1").arg(App.puzzle.pieceIdText)
                    onCommitted: (v) => App.puzzle.setPieceMax(App.puzzle.selectedPiece, v)
                }
            }

            RowLayout { // ④
                width: parent.width
                visible: App.puzzle.pieceShown
                spacing: 8
                BtSwitch {
                    id: rangeSwitch
                    objectName: "puzzle.piece.rangeSwitch"
                    checkable: false            // the model's: a warning may keep it as it is
                    checked: App.puzzle.pieceRange
                    Accessible.name: rangeLabel.text
                    onClicked: App.puzzle.setRangeOn(App.puzzle.selectedPiece, !App.puzzle.pieceRange)
                }
                Text {
                    id: rangeLabel
                    objectName: "puzzle.piece.rangeLabel"
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("Allow a range (min–max) instead of a fixed count")
                    Accessible.ignored: true
                    color: Theme.text
                    font.pixelSize: Theme.fontSecondary
                }
            }

            Column { // ⑤
                objectName: "puzzle.piece.groups"
                width: parent.width
                visible: App.puzzle.pieceShown && App.puzzle.pieceMax > 0
                spacing: 4
                Label { objectName: "puzzle.piece.groups.title"; text: qsTr("Groups") }
                Flow {
                    width: parent.width
                    spacing: 4
                    Repeater {
                        model: App.puzzle.pieceGroups
                        delegate: Loader {
                            id: groupItem
                            required property int group
                            required property string name
                            required property int count
                            // one copy: in or out; several: how many may be in the group
                            sourceComponent: App.puzzle.pieceMax > 1 ? groupStepper : groupToggle

                            Component {
                                id: groupToggle
                                BtButton {
                                    objectName: "puzzle.piece.group." + groupItem.group
                                    small: true
                                    primary: groupItem.count > 0
                                    text: groupItem.name
                                    Accessible.role: Accessible.CheckBox
                                    Accessible.checkable: true
                                    Accessible.checked: groupItem.count > 0
                                    onClicked: App.puzzle.setGroupCount(App.puzzle.selectedPiece, groupItem.group,
                                                                        groupItem.count > 0 ? 0 : 1)
                                }
                            }
                            Component {
                                id: groupStepper
                                PieceStepper {
                                    objectName: "puzzle.piece.group." + groupItem.group
                                    implicitWidth: 104
                                    axis: groupItem.name
                                    axisColor: Theme.muted
                                    from: 0
                                    to: App.puzzle.pieceMax
                                    value: groupItem.count
                                    tip: qsTr("Copies of %1 that may be in %2").arg(App.puzzle.pieceIdText).arg(groupItem.name)
                                    onCommitted: (v) => App.puzzle.setGroupCount(App.puzzle.selectedPiece, groupItem.group, v)
                                }
                            }
                        }
                    }
                    BtButton {
                        objectName: "puzzle.piece.group.new"
                        small: true
                        text: qsTr("+ New")
                        Accessible.name: qsTr("New group")
                        onClicked: App.puzzle.newGroup(App.puzzle.selectedPiece)
                    }
                }
            }
        }
    }

    Component {
        id: groupsBody
        Column {
            spacing: 4

            Help {
                objectName: "puzzle.groups.help"
                text: qsTr("Pieces of one group need not be taken apart from each other. A piece can be in several groups.")
            }
            Text {
                objectName: "puzzle.groups.empty"
                width: parent.width
                visible: App.puzzle.groups.count === 0
                wrapMode: Text.WordWrap
                text: qsTr("No groups in this problem")
                color: Theme.muted
                font.pixelSize: Theme.fontSecondary
            }
            Repeater {
                model: App.puzzle.groups
                delegate: Rectangle { // ⑥
                    id: groupCard
                    required property int group
                    required property string name
                    required property var members
                    objectName: "puzzle.groups.card." + group
                    width: parent.width
                    height: cardRow.implicitHeight + 8
                    radius: Theme.radiusControl
                    color: Theme.panel2
                    border.color: Theme.line

                    RowLayout {
                        id: cardRow
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 4; leftMargin: 8 }
                        spacing: 6
                        Text {
                            Layout.alignment: Qt.AlignTop
                            Layout.topMargin: 5
                            objectName: "puzzle.groups.card." + groupCard.group + ".name"
                            text: groupCard.name
                            color: Theme.text
                            font.pixelSize: Theme.fontSecondary
                            font.weight: Font.DemiBold
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 3
                            Repeater {
                                model: groupCard.members
                                delegate: Chip {
                                    required property var modelData
                                    objectName: "puzzle.groups.card." + groupCard.group + ".member." + modelData.idText
                                    text: qsTr("%1 ×%2").arg(modelData.idText).arg(modelData.count)
                                    fill: modelData.chipColor
                                    textColor: modelData.chipTextColor
                                }
                            }
                        }
                        IconButton {
                            objectName: "puzzle.groups.card." + groupCard.group + ".delete"
                            Layout.alignment: Qt.AlignTop
                            implicitWidth: 24
                            implicitHeight: 24
                            iconSize: 16
                            iconName: "trash"
                            dangerHover: true
                            tip: qsTr("Delete %1").arg(groupCard.name)
                            onClicked: {
                                const header = root.sectionHeader("groups")
                                App.puzzle.deleteGroup(groupCard.group)
                                // this card is gone: the keyboard goes on from its section
                                if (App.keyboardCues.navigating && header !== null)
                                    header.forceActiveFocus(Qt.TabFocusReason)
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: rulesBody
        Column {
            id: rules
            spacing: 4

            // the cell in question: under the pointer, else the keyboard's
            property int hoverPiece: -1
            property int hoverResult: -1
            property int cursorPiece: 0     // indices into the colours, not ids
            property int cursorResult: 0
            /* As the 2D grid's (VoxelGridItem): the keys hold the highlight, the
             * pointer's hover ignored until it moves off where it rested. Qt Quick
             * repeats a hover at a still pointer after each frame, and a scroll
             * moves the cells under it; neither is a move. */
            property bool keysHold: false
            property real pointerX: -1      // the pointer's last scene position over the cells
            property real pointerY: -1
            property bool focusByPointer: false        // a click is focusing the matrix

            readonly property bool pointerShown: hoverPiece >= 0 && !keysHold

            function pointerAt(x: real, y: real): void {
                if (keysHold && x === pointerX && y === pointerY)
                    return
                keysHold = false
                pointerX = x
                pointerY = y
            }
            // the arrows, Space and Enter take the highlight from the pointer, going on from its cell
            function takeKeys(): void {
                if (!keysHold && hoverPiece >= 0) {
                    cursorPiece = hoverPiece
                    cursorResult = hoverResult
                }
                keysHold = true
            }
            readonly property int colourCount: App.puzzle.colours.count
            readonly property real swatchWidth: 30
            readonly property real cellWidth: 30
            readonly property real cellHeight: 24
            readonly property real cellGap: 3

            function ruleIndex(piece: int, result: int): int { return piece * colourCount + result }
            function colourId(i: int): int { return App.puzzle.colours.value(i, "colour") }
            function colourName(i: int): string { return App.puzzle.colours.value(i, "name") }

            readonly property bool cursorShown: matrix.activeFocus && App.keyboardCues.navigating && !pointerShown
            readonly property int shownPiece: pointerShown ? hoverPiece : cursorShown ? cursorPiece : -1
            readonly property int shownResult: pointerShown ? hoverResult : cursorShown ? cursorResult : -1
            readonly property string cellText: {
                if (shownPiece < 0 || shownResult < 0 || shownPiece >= colourCount || shownResult >= colourCount)
                    return qsTr("Move to a cell to read its rule.")
                // through the revision: value() alone would not follow a toggle
                const allowed = App.puzzle.rules.revision >= 0
                                && App.puzzle.rules.value(ruleIndex(shownPiece, shownResult), "allowed")
                const piece = colourName(shownPiece), result = colourName(shownResult)
                return allowed ? qsTr("Piece %1 on result %2: allowed").arg(piece).arg(result)
                               : qsTr("Piece %1 on result %2: not allowed").arg(piece).arg(result)
            }
            readonly property string explanation: qsTr("Each row is a colour on the piece, each column a colour on the result. A tick lets a piece voxel of the row's colour sit on a result voxel of the column's colour. Colour rules are per problem.")

            Text {
                objectName: "puzzle.rules.empty"
                width: parent.width
                visible: rules.colourCount === 0
                wrapMode: Text.WordWrap
                text: qsTr("No colours in this puzzle. Add them under Colours in Entities: colour rules then say which piece colours may sit on which result colours.")
                color: Theme.muted
                font.pixelSize: Theme.fontSecondary
            }

            Help { // ⑦
                objectName: "puzzle.rules.help"
                visible: rules.colourCount > 0 && !Theme.minimal
                text: qsTr("Tick where a piece colour may sit on a result colour.")
            }

            Item {
                id: matrixArea
                objectName: "puzzle.rules.matrix"
                visible: rules.colourCount > 0
                width: parent.width
                // a colour or two are shorter than the label along their rows
                height: columnsTitle.height + rules.cellHeight + rules.cellGap
                        + Math.max(rules.colourCount * (rules.cellHeight + rules.cellGap), rowsLabel.implicitWidth)

                Text { // ⑧
                    id: columnsTitle
                    objectName: "puzzle.rules.axis.result"
                    x: rowsTitleBox.width + rules.swatchWidth + rules.cellGap
                    width: parent.width - x
                    height: 15
                    verticalAlignment: Text.AlignVCenter
                    text: qsTr("Result colour")
                    color: Theme.muted
                    font.pixelSize: Theme.fontMicro
                    font.weight: Font.DemiBold
                    font.capitalization: Font.AllUppercase
                }
                Item {
                    id: rowsTitleBox
                    y: columnsTitle.height + rules.cellHeight + rules.cellGap
                    width: 16
                    height: Math.max(rules.colourCount * (rules.cellHeight + rules.cellGap) - rules.cellGap, rowsLabel.implicitWidth)
                    Text {
                        id: rowsLabel
                        objectName: "puzzle.rules.axis.piece"
                        anchors.centerIn: parent
                        rotation: -90
                        text: qsTr("Piece colour")
                        color: Theme.muted
                        font.pixelSize: Theme.fontMicro
                        font.weight: Font.DemiBold
                        font.capitalization: Font.AllUppercase
                    }
                }
                IconButton {
                    objectName: "puzzle.rules.info"
                    anchors.right: parent.right
                    y: columnsTitle.height - 2
                    implicitWidth: 24
                    implicitHeight: 24
                    iconSize: 14
                    iconName: "info"
                    tip: rules.explanation
                    Accessible.name: qsTr("About the colour rules")
                    Accessible.description: rules.explanation
                }
                Column { // the row headers stay while the cells scroll sideways
                    x: rowsTitleBox.width
                    y: rowsTitleBox.y
                    spacing: rules.cellGap
                    Repeater {
                        model: App.puzzle.colours
                        delegate: Rectangle {
                            id: rowHeader
                            required property int index
                            required property string name
                            required property color swatch
                            required property color swatchText
                            objectName: "puzzle.rules.piece." + name
                            width: rules.swatchWidth
                            height: rules.cellHeight
                            radius: Theme.radiusChip
                            color: swatch
                            border.width: rules.shownPiece === index ? 2 : 1
                            border.color: rules.shownPiece === index ? Theme.accent : Theme.line2
                            Text {
                                anchors.centerIn: parent
                                text: rowHeader.name
                                Accessible.ignored: true
                                color: rowHeader.swatchText
                                font.pixelSize: Theme.fontMicro
                                font.weight: Font.Bold
                            }
                        }
                    }
                }

                FocusScope {
                    id: matrix
                    objectName: "puzzle.rules.grid"
                    x: rowsTitleBox.width + rules.swatchWidth + rules.cellGap
                    y: columnsTitle.height
                    width: parent.width - x
                    height: parent.height - y
                    activeFocusOnTab: true
                    Accessible.role: Accessible.Table
                    Accessible.name: qsTr("Colour rules: rows are piece colours, columns are result colours")
                    Accessible.description: rules.cellText

                    // Tab: the keys' cell is the one shown, wherever the pointer rests; a click's is the pointer's
                    onActiveFocusChanged: rules.keysHold = matrix.activeFocus && !rules.focusByPointer

                    // the pointer over the cells: a move takes the highlight back from the keys
                    HoverHandler {
                        id: pointerOver
                        onPointChanged: rules.pointerAt(pointerOver.point.scenePosition.x, pointerOver.point.scenePosition.y)
                        onHoveredChanged: if (!pointerOver.hovered) rules.keysHold = false
                    }

                    function moveCursor(dp: int, dr: int): void {
                        rules.takeKeys()
                        const n = rules.colourCount
                        rules.cursorPiece = Math.max(0, Math.min(n - 1, rules.cursorPiece + dp))
                        rules.cursorResult = Math.max(0, Math.min(n - 1, rules.cursorResult + dr))
                        const left = rules.cursorResult * (rules.cellWidth + rules.cellGap)
                        if (left < cells.contentX)
                            cells.contentX = left
                        else if (left + rules.cellWidth > cells.contentX + cells.width)
                            cells.contentX = left + rules.cellWidth - cells.width
                        App.status.announce(rules.cellText)
                    }
                    function toggleCursor(): void {
                        rules.takeKeys()
                        App.puzzle.toggleRule(rules.colourId(rules.cursorPiece), rules.colourId(rules.cursorResult))
                        App.status.announce(rules.cellText)
                    }
                    Keys.onUpPressed: moveCursor(-1, 0)
                    Keys.onDownPressed: moveCursor(1, 0)
                    Keys.onLeftPressed: moveCursor(0, -1)
                    Keys.onRightPressed: moveCursor(0, 1)
                    Keys.onSpacePressed: toggleCursor()
                    Keys.onReturnPressed: toggleCursor()
                    Keys.onEnterPressed: toggleCursor()

                    Flickable {
                        id: cells
                        anchors.fill: parent
                        contentWidth: rules.colourCount * (rules.cellWidth + rules.cellGap) - rules.cellGap
                        contentHeight: height
                        flickableDirection: Flickable.HorizontalFlick
                        boundsBehavior: Flickable.StopAtBounds
                        clip: true
                        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }

                        Column {
                            spacing: rules.cellGap
                            Row {
                                spacing: rules.cellGap
                                Repeater {
                                    model: App.puzzle.colours
                                    delegate: Rectangle {
                                        id: columnHeader
                                        required property int index
                                        required property string name
                                        required property color swatch
                                        required property color swatchText
                                        objectName: "puzzle.rules.result." + name
                                        width: rules.cellWidth
                                        height: rules.cellHeight
                                        radius: Theme.radiusChip
                                        color: swatch
                                        border.width: rules.shownResult === index ? 2 : 1
                                        border.color: rules.shownResult === index ? Theme.accent : Theme.line2
                                        Text {
                                            anchors.centerIn: parent
                                            text: columnHeader.name
                                            Accessible.ignored: true
                                            color: columnHeader.swatchText
                                            font.pixelSize: Theme.fontMicro
                                            font.weight: Font.Bold
                                        }
                                    }
                                }
                            }
                            Grid {
                                columns: Math.max(1, rules.colourCount)
                                spacing: rules.cellGap
                                Repeater {
                                    model: App.puzzle.rules
                                    delegate: MatrixToggle { // ⑨
                                        id: cell
                                        required property int index
                                        required property int pieceColour
                                        required property int resultColour
                                        required property string pieceName
                                        required property string resultName
                                        required property bool allowed
                                        readonly property int row: Math.floor(index / rules.colourCount)
                                        readonly property int column: index % rules.colourCount
                                        objectName: "puzzle.rules.cell." + pieceName + "." + resultName
                                        width: rules.cellWidth
                                        height: rules.cellHeight
                                        on: allowed
                                        cursor: rules.cursorShown && rules.cursorPiece === cell.row && rules.cursorResult === cell.column
                                        pointerShown: !rules.keysHold
                                        Accessible.role: Accessible.CheckBox
                                        Accessible.name: qsTr("Piece %1 on result %2").arg(pieceName).arg(resultName)
                                        Accessible.checkable: true
                                        Accessible.checked: allowed
                                        Accessible.onPressAction: App.puzzle.toggleRule(pieceColour, resultColour)
                                        onHoveredChanged: {
                                            if (hovered) {
                                                rules.hoverPiece = row
                                                rules.hoverResult = column
                                            } else if (rules.hoverPiece === row && rules.hoverResult === column) {
                                                rules.hoverPiece = -1
                                                rules.hoverResult = -1
                                            }
                                        }
                                        onToggled: {
                                            // a click focuses the matrix too, so the keys go on from the clicked cell
                                            rules.cursorPiece = row
                                            rules.cursorResult = column
                                            rules.focusByPointer = true
                                            matrix.forceActiveFocus(Qt.MouseFocusReason)
                                            rules.focusByPointer = false
                                            rules.keysHold = false
                                            App.puzzle.toggleRule(pieceColour, resultColour)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Text { // ⑩
                objectName: "puzzle.rules.cellText"
                width: parent.width
                visible: rules.colourCount > 0
                text: rules.cellText
                elide: Text.ElideRight
                color: Theme.text
                font.pixelSize: Theme.fontSecondary
            }

            Help { // ⑪
                objectName: "puzzle.rules.default"
                visible: rules.colourCount > 0 && !Theme.minimal
                text: qsTr("Default voxels fit anywhere, so they have no row or column.")
            }

            Flow { // ⑫
                width: parent.width
                visible: rules.colourCount > 0
                spacing: 6
                BtButton {
                    objectName: "puzzle.rules.identity"
                    small: true
                    text: qsTr("Each colour on itself")
                    onClicked: App.puzzle.setRulePreset(0)
                }
                BtButton {
                    objectName: "puzzle.rules.all"
                    small: true
                    text: qsTr("Allow all")
                    onClicked: App.puzzle.setRulePreset(1)
                }
                BtButton {
                    objectName: "puzzle.rules.clear"
                    small: true
                    text: qsTr("Clear")
                    onClicked: App.puzzle.setRulePreset(2)
                }
            }
        }
    }
}
