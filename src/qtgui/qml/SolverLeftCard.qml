pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import BurrTools.Ui

// C19 Solver left card: set up and run the search. Nothing is edited here;
// the rules (which button a state allows, what the options ask of the
// solver) are btui::solver's, the state SolverController's (App.solver).
//
// Wireframe (each number marks its code below):
//
//   ┌──────────────────────────────────────────┐
//   │ Solver                               [◧] │
//   ├──────────────────────────────────────────┤
//   │ PROBLEM                                  │
//   │ ①[P1] Burr 6                          ⌄  │   a popover lists the problems
//   │      Result S7 · 6 piece shapes          │
//   ├──────────────────────────────────────────┤
//   │ WHAT TO CHECK                            │
//   │ ②(•) Assemblies                          │   radio cards
//   │      Find every way the pieces fit ...   │
//   │  ( ) Assemblies and disassembly          │
//   │      Also check that each assembly ...   │
//   │ ③   Don't keep the disassemblies   [  ] │   with the disassembly only
//   ├──────────────────────────────────────────┤
//   │ ④⌄ Search options             · 2 changed│   collapsed by default
//   │   Just count                       [  ]  │
//   │   Keep mirror solutions            [  ]  │
//   │   Keep rotated solutions           [  ]  │
//   │   Thorough rotation check          [  ]  │
//   │   Sort found solutions by  [Moves ⌄]     │
//   │   Keep every              [−  1  +]      │
//   │   Limit                   [− 100 +]      │
//   ├──────────────────────────────────────────┤
//   │ RUN                                      │
//   │ ⑤[Prepare][ Start  ][Continue][ Stop ]   │
//   │ ⑥[██████████░░░░░░░░░░░░░░░]  42.0 %     │
//   │ ⑦Activity    assembling                  │
//   │  Assemblies  12   Solutions  —           │
//   │  Time used   3 seconds  Time left  ...   │
//   ├──────────────────────────────────────────┤
//   │ STEP-BY-STEP EXPLORER — ADVANCED         │
//   │ ⑧[Placements][Movements][  Step  ]       │
//   │   Placements browses the assembly ...    │
//   └──────────────────────────────────────────┘
Card {
    id: root
    objectName: "solver.left"
    title: qsTr("Solver")
    collapseIcon: "panelL"
    collapseObjectName: "solver.left.collapse"
    onCollapseClicked: App.layout.leftCollapsed = true

    // density.md §1: Solver sections 9 x 12 (Minimal 7 x 10)
    readonly property real pad: Theme.solverSectionH
    readonly property real vpad: Theme.solverSectionV

    // a section's label: 11/600 uppercase muted (C19)
    component Label: Text {
        color: Theme.muted
        font.pixelSize: Theme.fontMicro
        font.weight: Font.DemiBold
        font.capitalization: Font.AllUppercase
        textFormat: Text.PlainText
    }
    component Help: Text {
        visible: !Theme.minimal
        wrapMode: Text.WordWrap
        color: Theme.muted
        font.pixelSize: Theme.fontMicro
        textFormat: Text.PlainText
    }
    component Divider: Rectangle {
        width: parent ? parent.width : 0
        height: 1
        color: Theme.line
    }
    // a switch row of the options: title, help, the switch at the right
    component OptionRow: Item {
        id: optionRow
        property string title
        property string help
        property alias checked: toggle.checked
        signal toggled(bool on)
        width: parent ? parent.width : 0
        implicitHeight: Math.max(texts.implicitHeight, toggle.implicitHeight) + 8
        Column {
            id: texts
            anchors { left: parent.left; right: toggle.left; rightMargin: 10; verticalCenter: parent.verticalCenter }
            spacing: 1
            Text {
                width: parent.width
                text: optionRow.title
                color: Theme.text
                font.pixelSize: Theme.fontBody
                textFormat: Text.PlainText
                elide: Text.ElideRight
            }
            Help {
                width: parent.width
                text: optionRow.help
            }
        }
        BtSwitch {
            id: toggle
            objectName: optionRow.objectName + ".switch"
            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
            Accessible.name: optionRow.title
            Accessible.description: optionRow.help
            onClicked: optionRow.toggled(checked)
            BtToolTip { shown: Theme.minimal && (toggle.hovered || toggle.visualFocus); text: optionRow.help }
        }
    }
    // a cell of the status grid
    component Stat: Row {
        id: stat
        property string name
        property string value
        spacing: 8
        Accessible.role: Accessible.StaticText
        Accessible.name: qsTr("%1: %2").arg(name).arg(value.length > 0 ? value : qsTr("none"))
        Text {
            width: 76
            text: stat.name
            color: Theme.muted
            font.pixelSize: Theme.fontSecondary
            textFormat: Text.PlainText
            Accessible.ignored: true
        }
        Text {
            text: stat.value.length > 0 ? stat.value : "—"
            color: Theme.text
            font.pixelSize: Theme.fontSecondary
            font.weight: Font.DemiBold
            textFormat: Text.PlainText
            Accessible.ignored: true
        }
    }

    ScrollToFocus { view: flick; follow: true }

    Flickable {
        id: flick
        objectName: "solver.left.sections"
        anchors.fill: parent
        contentHeight: column.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Column {
            id: column
            width: flick.width

            // --- Problem -------------------------------------------------------
            Column {
                width: parent.width
                leftPadding: root.pad
                rightPadding: root.pad
                topPadding: root.vpad
                bottomPadding: root.vpad
                spacing: 6
                Label { text: qsTr("Problem") }
                AbstractButton { // ①
                    id: problemButton
                    objectName: "solver.problem"
                    width: parent.width - 2 * root.pad
                    implicitHeight: problemRow.implicitHeight + 16
                    hoverEnabled: true
                    focusPolicy: Qt.StrongFocus
                    enabled: App.puzzle.problems.count > 0
                    Accessible.role: Accessible.ComboBox
                    Accessible.name: qsTr("Problem to solve")
                    Accessible.description: qsTr("%1 %2, %3").arg(App.solver.problemIdText).arg(App.solver.problemLabel).arg(App.solver.problemDetail)
                    onClicked: problemPopup.open()
                    Keys.onDownPressed: App.solver.selectProblem(App.solver.problem + 1)
                    Keys.onUpPressed: App.solver.selectProblem(App.solver.problem - 1)
                    background: Rectangle {
                        radius: 10
                        color: problemButton.hovered ? Theme.line : Theme.panel2
                        border.width: problemButton.visualFocus ? 2 : 1
                        border.color: problemButton.visualFocus ? Theme.accent : Theme.line
                    }
                    contentItem: Item {
                        RowLayout {
                            id: problemRow
                            anchors { left: parent.left; right: parent.right; leftMargin: 10; rightMargin: 10; verticalCenter: parent.verticalCenter }
                            spacing: 10
                            Rectangle {
                                Layout.preferredWidth: Math.max(30, pchip.implicitWidth + 12)
                                Layout.preferredHeight: 24
                                radius: Theme.radiusChip
                                color: Theme.accent
                                visible: App.solver.problem >= 0
                                Text {
                                    id: pchip
                                    anchors.centerIn: parent
                                    text: App.solver.problemIdText
                                    color: Theme.onAccent
                                    font.pixelSize: Theme.fontSecondary
                                    font.weight: Font.Bold
                                    textFormat: Text.PlainText
                                }
                            }
                            Column {
                                Layout.fillWidth: true
                                spacing: 1
                                Text {
                                    width: parent.width
                                    text: App.solver.problemLabel.length > 0 ? App.solver.problemLabel : qsTr("Problem %1").arg(App.solver.problem + 1)
                                    visible: App.solver.problem >= 0
                                    color: Theme.text
                                    font.pixelSize: Theme.fontBody
                                    font.weight: Font.Bold
                                    elide: Text.ElideRight
                                    textFormat: Text.PlainText
                                }
                                Text {
                                    width: parent.width
                                    text: App.solver.problemDetail
                                    color: Theme.muted
                                    font.pixelSize: Theme.fontSecondary
                                    elide: Text.ElideRight
                                    textFormat: Text.PlainText
                                }
                            }
                            Icon { name: "chevD"; size: 14 }
                        }
                    }

                    Popup {
                        id: problemPopup
                        objectName: "solver.problem.list"
                        y: problemButton.height + 4
                        width: problemButton.width
                        padding: 4
                        focus: true
                        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
                        background: Rectangle {
                            color: Theme.panel
                            border.color: Theme.line2
                            radius: Theme.radiusControl
                        }
                        onOpened: {
                            const row = problemRows.itemAt(Math.max(0, App.solver.problem))
                            if (row !== null)
                                row.forceActiveFocus(App.keyboardCues.navigating ? Qt.TabFocusReason : Qt.MouseFocusReason)
                        }
                        onClosed: problemButton.forceActiveFocus(App.keyboardCues.navigating ? Qt.TabFocusReason : Qt.MouseFocusReason)
                        contentItem: Column {
                            Keys.onUpPressed: (problemPopup.contentItem.Window.activeFocusItem as Item).nextItemInFocusChain(false).forceActiveFocus(Qt.TabFocusReason)
                            Keys.onDownPressed: (problemPopup.contentItem.Window.activeFocusItem as Item).nextItemInFocusChain(true).forceActiveFocus(Qt.TabFocusReason)
                            Text {
                                leftPadding: 8
                                topPadding: 4
                                bottomPadding: 4
                                text: qsTr("Solve problem")
                                color: Theme.muted
                                font.pixelSize: Theme.fontMicro
                                font.weight: Font.DemiBold
                                font.capitalization: Font.AllUppercase
                                Accessible.ignored: true
                            }
                            Repeater {
                                id: problemRows
                                model: App.puzzle.problems
                                delegate: CheckRow {
                                    required property int index
                                    required property string idText
                                    required property string label
                                    objectName: "solver.problem.list." + index
                                    width: problemPopup.availableWidth
                                    implicitHeight: 28
                                    radio: true
                                    text: label.length > 0 ? qsTr("%1  %2").arg(idText).arg(label) : idText
                                    selected: index === App.solver.problem
                                    onClicked: {
                                        App.solver.selectProblem(index)
                                        problemPopup.close()
                                    }
                                }
                            }
                        }
                    }
                }
            }
            Divider {}

            // --- What to check --------------------------------------------------------
            Column {
                width: parent.width
                leftPadding: root.pad
                rightPadding: root.pad
                topPadding: root.vpad
                bottomPadding: root.vpad
                spacing: 6
                Label { text: qsTr("What to check") }
                // one group: the selected card is the Tab stop, ↑ ↓ choose
                RadioCard { // ②
                    id: asmCard
                    objectName: "solver.mode.asm"
                    width: parent.width - 2 * root.pad
                    text: qsTr("Assemblies")
                    description: Theme.minimal ? "" : qsTr("Find every way the pieces fit the result.")
                    Accessible.description: qsTr("Find every way the pieces fit the result.")
                    selected: !App.solver.disassemble
                    focusPolicy: selected || !disCard.enabled ? Qt.StrongFocus : Qt.ClickFocus
                    onClicked: App.solver.disassemble = false
                    Keys.onDownPressed: if (disCard.enabled) { App.solver.disassemble = true; disCard.forceActiveFocus(Qt.TabFocusReason) }
                }
                RadioCard {
                    id: disCard
                    objectName: "solver.mode.dis"
                    width: parent.width - 2 * root.pad
                    enabled: App.solver.canDisassemble
                    opacity: enabled ? 1 : 0.5
                    text: qsTr("Assemblies and disassembly")
                    readonly property string help: App.solver.canDisassemble
                        ? qsTr("Also check that each assembly can be taken apart, moving pieces along the voxel axes.")
                        : qsTr("This voxel type has no disassembler.")
                    description: Theme.minimal ? "" : help
                    Accessible.description: help
                    selected: App.solver.disassemble
                    focusPolicy: selected ? Qt.StrongFocus : Qt.ClickFocus
                    onClicked: App.solver.disassemble = true
                    Keys.onUpPressed: { App.solver.disassemble = false; asmCard.forceActiveFocus(Qt.TabFocusReason) }
                }
                OptionRow { // ③
                    objectName: "solver.drop"
                    visible: App.solver.disassemble
                    width: parent.width - 2 * root.pad
                    title: qsTr("Don't keep the disassemblies")
                    help: qsTr("Check disassemblies but keep only their level, not the moves. Recompute them later with “Analyse”.")
                    checked: App.solver.dropDisassemblies
                    onToggled: (on) => App.solver.dropDisassemblies = on
                }
            }
            Divider {}

            // --- Search options -----------------------------------------------------------
            AccordionSection { // ④
                id: options
                objectName: "solver.options"
                width: parent.width
                title: qsTr("Search options")
                summary: App.solver.changedOptions > 0 && !open ? qsTr("· %1 changed").arg(App.solver.changedOptions) : ""
                open: App.solver.optionsOpen
                onToggled: App.solver.optionsOpen = !App.solver.optionsOpen

                Column {
                    width: parent.width
                    spacing: 2
                    OptionRow {
                        objectName: "solver.options.justCount"
                        title: qsTr("Just count")
                        help: qsTr("Only count solutions; they are dropped as soon as they are found.")
                        checked: App.solver.justCount
                        onToggled: (on) => App.solver.justCount = on
                    }
                    OptionRow {
                        objectName: "solver.options.keepMirror"
                        title: qsTr("Keep mirror solutions")
                        help: qsTr("Do not remove solutions that are mirror images of others (useful with piece ranges).")
                        checked: App.solver.keepMirror
                        onToggled: (on) => App.solver.keepMirror = on
                    }
                    OptionRow {
                        objectName: "solver.options.keepRotated"
                        title: qsTr("Keep rotated solutions")
                        help: qsTr("Do not remove solutions that are only rotations of others.")
                        checked: App.solver.keepRotated
                        onToggled: (on) => App.solver.keepRotated = on
                    }
                    OptionRow {
                        objectName: "solver.options.thorough"
                        title: qsTr("Thorough rotation check")
                        help: qsTr("More thorough, slower check for duplicate (rotated) solutions.")
                        checked: App.solver.thoroughRotation
                        onToggled: (on) => App.solver.thoroughRotation = on
                    }
                    Column {
                        width: parent.width
                        spacing: 4
                        topPadding: 4
                        Text {
                            text: qsTr("Sort found solutions by")
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                            textFormat: Text.PlainText
                        }
                        Dropdown {
                            id: sortBy
                            objectName: "solver.options.sort"
                            width: parent.width
                            readonly property list<string> labels: [
                                qsTr("Search order (original id)"), qsTr("Number of pieces in the assembly"),
                                qsTr("Level"), qsTr("Moves for complete disassembly")
                            ]
                            // Level and Moves need the disassembly check (AC-C19-03)
                            model: App.solver.disassemble ? labels
                                   : [ labels[0], labels[1], qsTr("Level (needs disassembly)"), qsTr("Moves (needs disassembly)") ]
                            currentIndex: App.solver.searchSortAvailable(App.solver.searchSort) ? App.solver.searchSort : 0
                            readonly property string help: App.solver.disassemble ? qsTr("The order the solver keeps them in")
                                : App.solver.canDisassemble ? qsTr("The order the solver keeps them in — level and moves need the disassembly check")
                                : qsTr("The order the solver keeps them in — this voxel type has no disassembler, so no level or moves")
                            Accessible.name: qsTr("Sort found solutions by")
                            Accessible.description: help
                            BtToolTip { shown: Theme.minimal && (sortBy.hovered || sortBy.visualFocus); text: sortBy.help }
                            function rebind(): void {
                                currentIndex = Qt.binding(() => App.solver.searchSortAvailable(App.solver.searchSort) ? App.solver.searchSort : 0)
                            }
                            // a new list of choices may reset the index: the choice stays the solver's
                            onModelChanged: rebind()
                            onActivated: (i) => {
                                if (App.solver.searchSortAvailable(i))
                                    App.solver.searchSort = i
                                rebind()
                            }
                        }
                        Help {
                            width: parent.width
                            text: sortBy.help
                        }
                    }
                    RowLayout {
                        width: parent.width
                        spacing: 10
                        Column {
                            Layout.fillWidth: true
                            Text { text: qsTr("Keep every"); color: Theme.text; font.pixelSize: Theme.fontBody; textFormat: Text.PlainText }
                            Help { width: parent.width; text: qsTr("Keep only every n-th solution found; 1 keeps them all") }
                        }
                        Stepper {
                            objectName: "solver.options.keepEvery"
                            Layout.preferredWidth: 112
                            from: 1
                            to: App.solver.maxCount
                            value: App.solver.keepEvery
                            tip: qsTr("Keep only every n-th solution found")
                            Accessible.name: qsTr("Keep every")
                            onCommitted: (v) => App.solver.keepEvery = v
                        }
                    }
                    RowLayout {
                        width: parent.width
                        spacing: 10
                        Column {
                            Layout.fillWidth: true
                            Text { text: qsTr("Limit"); color: Theme.text; font.pixelSize: Theme.fontBody; textFormat: Text.PlainText }
                            Help { width: parent.width; text: qsTr("Keep at most this many solutions") }
                        }
                        Stepper {
                            objectName: "solver.options.limit"
                            Layout.preferredWidth: 112
                            from: 1
                            to: App.solver.maxCount
                            value: App.solver.limit
                            tip: qsTr("Keep at most this many solutions")
                            Accessible.name: qsTr("Limit")
                            onCommitted: (v) => App.solver.limit = v
                        }
                    }
                }
            }

            // --- Run ------------------------------------------------------------------------
            Column {
                width: parent.width
                leftPadding: root.pad
                rightPadding: root.pad
                topPadding: root.vpad
                bottomPadding: root.vpad
                spacing: 8
                Label { text: qsTr("Run") }
                // 1 : 1.4 : 1 : 1 (C19); in two rows where the labels would not fit
                GridLayout { // ⑤
                    id: runRow
                    objectName: "solver.run"
                    width: parent.width - 2 * root.pad
                    columnSpacing: 6
                    rowSpacing: 6
                    readonly property real minButton: 64
                    columns: (width - 3 * columnSpacing) / 4.4 >= minButton ? 4 : 2
                    readonly property real unit: columns === 4 ? (width - 3 * columnSpacing) / 4.4 : (width - columnSpacing) / 2

                    // a Run button that greys out under the keyboard hands it on: to Stop
                    // while a search runs, else to Continue, Start or Prepare
                    function passFocus(): void {
                        const reason = App.keyboardCues.navigating ? Qt.TabFocusReason : Qt.OtherFocusReason
                        if (stopButton.enabled)
                            stopButton.forceActiveFocus(reason)
                        else if (contButton.enabled)
                            contButton.forceActiveFocus(reason)
                        else if (startButton.enabled)
                            startButton.forceActiveFocus(reason)
                        else if (prepButton.enabled)
                            prepButton.forceActiveFocus(reason)
                    }
                    BtButton {
                        id: prepButton
                        objectName: "solver.run.prep"
                        Layout.preferredWidth: runRow.unit
                        Layout.preferredHeight: Theme.button
                        small: true
                        text: qsTr("Prepare")
                        enabled: App.solver.canPrepare
                        Accessible.description: qsTr("Build the search structures without solving")
                        BtToolTip { shown: prepButton.hovered || prepButton.visualFocus; text: qsTr("Build the search structures without solving") }
                        onClicked: App.solver.prepare()
                        // disabling takes the focus away: the keyboard goes on to the next
                        onActiveFocusChanged: if (!activeFocus && !enabled) runRow.passFocus()
                    }
                    BtButton {
                        id: startButton
                        objectName: "solver.run.start"
                        Layout.preferredWidth: runRow.columns === 4 ? runRow.unit * 1.4 : runRow.unit
                        Layout.preferredHeight: Theme.button
                        primary: true
                        text: qsTr("Start")
                        enabled: App.solver.canStart
                        Accessible.description: qsTr("Run the search from the start")
                        BtToolTip { shown: startButton.hovered || startButton.visualFocus; text: qsTr("Run the search from the start") }
                        onClicked: App.solver.start()
                        // disabling takes the focus away: the keyboard goes on to the next
                        onActiveFocusChanged: if (!activeFocus && !enabled) runRow.passFocus()
                    }
                    BtButton {
                        id: contButton
                        objectName: "solver.run.cont"
                        Layout.preferredWidth: runRow.unit
                        Layout.preferredHeight: Theme.button
                        small: true
                        text: qsTr("Continue")
                        enabled: App.solver.canContinue
                        Accessible.description: qsTr("Resume a stopped search")
                        BtToolTip { shown: contButton.hovered || contButton.visualFocus; text: qsTr("Resume a stopped search") }
                        onClicked: App.solver.resume()
                        // disabling takes the focus away: the keyboard goes on to the next
                        onActiveFocusChanged: if (!activeFocus && !enabled) runRow.passFocus()
                    }
                    BtButton {
                        id: stopButton
                        objectName: "solver.run.stop"
                        Layout.preferredWidth: runRow.unit
                        Layout.preferredHeight: Theme.button
                        small: true
                        text: qsTr("Stop")
                        enabled: App.solver.canStop
                        Accessible.description: qsTr("Pause the search")
                        BtToolTip { shown: stopButton.hovered || stopButton.visualFocus; text: qsTr("Pause the search") }
                        onClicked: App.solver.stop()
                        // disabling takes the focus away: the keyboard goes on to the next
                        onActiveFocusChanged: if (!activeFocus && !enabled) runRow.passFocus()
                    }
                }
                RowLayout { // ⑥
                    width: parent.width - 2 * root.pad
                    spacing: 10
                    ProgressBar {
                        id: bar
                        objectName: "solver.bar"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 8
                        from: 0
                        to: 1
                        value: App.solver.progress
                        Accessible.name: qsTr("Search progress")
                        Accessible.description: App.solver.progressText
                        background: Rectangle {
                            radius: 4
                            color: Theme.panel2
                            border.width: 1
                            border.color: Theme.controlLine
                        }
                        contentItem: Item {
                            Rectangle {
                                width: bar.visualPosition * parent.width
                                height: parent.height
                                radius: 4
                                color: Theme.accent
                            }
                        }
                    }
                    Text {
                        objectName: "solver.pct"
                        Layout.preferredWidth: 52
                        horizontalAlignment: Text.AlignRight
                        text: App.solver.progressText
                        color: Theme.text
                        font.pixelSize: Theme.fontSecondary
                        font.weight: Font.DemiBold
                        textFormat: Text.PlainText
                        Accessible.ignored: true
                    }
                }
                Grid { // ⑦
                    objectName: "solver.stat"
                    columns: 1
                    rowSpacing: 3
                    Stat { objectName: "solver.stat.activity"; name: qsTr("Activity"); value: App.solver.activityText }
                    Stat { objectName: "solver.stat.assemblies"; name: qsTr("Assemblies"); value: App.solver.assembliesText }
                    Stat { objectName: "solver.stat.solutions"; name: qsTr("Solutions"); value: App.solver.solutionsText }
                    Stat { objectName: "solver.stat.used"; name: qsTr("Time used"); value: App.solver.timeUsedText }
                    Stat { objectName: "solver.stat.left"; name: qsTr("Time left"); value: App.solver.timeLeftText }
                }
            }
            Divider {}

            // --- Step-by-step explorer ----------------------------------------------------------
            Column {
                width: parent.width
                leftPadding: root.pad
                rightPadding: root.pad
                topPadding: root.vpad
                bottomPadding: root.vpad
                spacing: 6
                Label { text: qsTr("Step-by-step explorer — advanced") }
                RowLayout { // ⑧
                    objectName: "solver.explorer"
                    width: parent.width - 2 * root.pad
                    spacing: 6
                    BtButton {
                        id: placementsButton
                        objectName: "solver.explorer.placements"
                        Layout.fillWidth: true
                        small: true
                        text: qsTr("Placements")
                        // not in this version: the explorer drawer comes later (README, deferred)
                        enabled: false
                        Accessible.role: Accessible.Button
                        Accessible.description: qsTr("Not in this version")
                    }
                    BtButton {
                        id: movementsButton
                        objectName: "solver.explorer.movements"
                        Layout.fillWidth: true
                        small: true
                        text: qsTr("Movements")
                        enabled: false
                        Accessible.description: qsTr("Not in this version")
                    }
                    BtButton {
                        id: stepButton
                        objectName: "solver.explorer.step"
                        Layout.fillWidth: true
                        small: true
                        text: qsTr("Step")
                        enabled: App.solver.canStep
                        Accessible.description: qsTr("Advance the assembler one step")
                        BtToolTip { shown: stepButton.hovered || stepButton.visualFocus; text: qsTr("Advance the assembler one step") }
                        onClicked: App.solver.step()
                    }
                }
                Help {
                    width: parent.width - 2 * root.pad
                    text: qsTr("Step advances the assembler one step. Browsing the placements and the movements comes in a later version.")
                }
            }
        }
    }
}
