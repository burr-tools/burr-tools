import QtQuick
import QtTest
import BurrTools.Ui

// C12 Settings and C22 Keyboard shortcuts, through the real Main.qml.
TestCase {
    id: tc
    name: "Settings"
    when: windowShown

    Component { id: mainComponent; Main {} }

    property var win

    function init() {
        // see tst_shell.qml for why this one warning is allowed
        failOnWarning(/^(?!No QRhi found for window).*/)
        App.settings.restoreAllDefaults()
        win = createTemporaryObject(mainComponent, tc)
        verify(win !== null, "Main.qml failed to load")
        win.width = 1400
        win.height = 900
        waitForRendering(win.contentItem)
    }

    function cleanup() {
        for (const name of ["settings.dialog", "help.shortcuts.dialog"]) {
            const d = findChild(win, name)
            if (d && d.visible)
                d.close()
        }
        App.settings.restoreAllDefaults()
    }

    function openSettings() {
        App.commands.trigger("settings")
        const d = findChild(win, "settings.dialog")
        verify(d !== null)
        tryVerify(() => d.opened, 2000)
        waitForRendering(d.contentItem)
        return d
    }

    function inside(dialog, name) {
        const o = findChild(dialog.contentItem, name) || findChild(dialog.header, name)
                  || findChild(dialog.footer, name)
        verify(o !== null, "no item " + name)
        return o
    }

    function test_everyLegacySettingIsOnItsPage() {
        // AC-C12-01: each once, on the spec's page
        const d = openSettings()
        const pages = {
            general: ["density", "theme", "undoDepth", "tooltips"],
            view3d: ["viewCube", "reverseScroll", "rotationMethod", "voxelStyle", "lighting", "fadePieces"],
            performance: ["workerThreads", "displayLists"]
        }
        for (const page in pages) {
            mouseClick(inside(d, "settings.nav." + page))
            compare(d.page, page)
            waitForRendering(d.contentItem)
            for (const key of pages[page])
                verify(inside(d, "settings.row." + key).visible, key + " on " + page)
        }
    }

    function test_controlsApplyAtOnce() {
        const d = openSettings()
        mouseClick(inside(d, "settings.nav.view3d"))
        waitForRendering(d.contentItem)
        const fade = inside(d, "settings." + "fadePieces")
        verify(App.settings.fadePieces)
        mouseClick(fade)
        verify(!App.settings.fadePieces)

        mouseClick(inside(d, "settings.nav.general"))
        waitForRendering(d.contentItem)
        const undo = inside(d, "settings.undoDepth")
        compare(undo.currentText, "25")
        compare(undo.count, 5)                      // 25 50 100 200 500 only
        undo.currentIndex = 2
        undo.activated(2)
        compare(App.settings.undoDepth, 100)

        mouseClick(inside(d, "settings.nav.performance"))
        waitForRendering(d.contentItem)
        const threads = inside(d, "settings.workerThreads")
        compare(threads.to, App.settings.maxWorkerThreads)
        threads.moved(1)
        compare(App.settings.workerThreads, 1)
    }

    function test_voxelStyleIsASegmentedChoice() {
        // the redesign's flat voxels by default; legacy's look behind Classic
        const d = openSettings()
        mouseClick(inside(d, "settings.nav.view3d"))
        waitForRendering(d.contentItem)
        compare(App.settings.voxelStyle, "flat")
        mouseClick(inside(d, "settings.voxelStyle.legacy"))
        compare(App.settings.voxelStyle, "legacy")
        mouseClick(inside(d, "settings.voxelStyle.flat"))
        compare(App.settings.voxelStyle, "flat")
    }

    function test_resetSectionTouchesOnlyItsPage() {
        // AC-C12-06
        App.settings.lighting = false
        App.settings.tooltips = false
        const d = openSettings()
        mouseClick(inside(d, "settings.nav.view3d"))
        waitForRendering(d.contentItem)
        mouseClick(inside(d, "settings.reset.view3d"))
        verify(App.settings.lighting)
        verify(!App.settings.tooltips)
        mouseClick(inside(d, "settings.restoreAll"))
        verify(App.settings.tooltips)
    }

    function test_searchFiltersAcrossPagesAndFindsShortcuts() {
        // T-C12-1, AC-C22-02
        const d = openSettings()
        const search = inside(d, "settings.search")
        search.text = "zoom"
        waitForRendering(d.contentItem)
        verify(inside(d, "settings.row.reverseScroll").visible)
        compare(findChild(d.contentItem, "settings.row.lighting"), null)
        search.text = "undo"
        waitForRendering(d.contentItem)
        verify(inside(d, "settings.row.undoDepth").visible)
        search.text = "nothing like this"
        waitForRendering(d.contentItem)
        verify(inside(d, "settings.noResults").visible)
        search.text = "settings"                    // a shortcut row: Ctrl+, Settings
        waitForRendering(d.contentItem)
        verify(d.shortcutMatches.length > 0)
        search.text = ""
        waitForRendering(d.contentItem)
        verify(inside(d, "settings.row.density").visible)
    }

    function test_closeButtonAndDone() {
        let d = openSettings()
        mouseClick(inside(d, "settings.close"))
        tryVerify(() => !d.visible, 2000)
        d = openSettings()
        mouseClick(inside(d, "settings.done"))
        tryVerify(() => !d.visible, 2000)
    }

    function test_keyboardShortcutsWindow() {
        // AC-C22-01/02/03
        App.commands.trigger("help.shortcuts")
        const d = findChild(win, "help.shortcuts.dialog")
        tryVerify(() => d.opened, 2000)
        verify(d.groups.length >= 3)
        compare(d.groups[0].title, "Everywhere")
        const settingsRow = d.groups[0].rows.find(r => r.action === "Settings")
        verify(settingsRow !== undefined)
        // the key caps come from the command table
        compare(settingsRow.keys[0].map(p => p.text).join("+"),
                Qt.platform.os === "osx" ? "⌘," : "Ctrl+,")
        // mouse inputs are marked for the dashed style
        verify(d.groups[1].rows[0].keys[0][0].mouse)

        const search = findChild(d.contentItem, "help.shortcuts.search")
        search.text = "zzzz"
        verify(findChild(d.contentItem, "help.shortcuts.empty").visible)
        search.text = ""
        mouseClick(findChild(d.footer, "help.shortcuts.close"))
        tryVerify(() => !d.visible, 2000)
    }

    // keys reach the focused window; Main is a window of its own
    function activate() {
        win.requestActivate()
        tryVerify(() => win.active, 3000)
    }

    function test_searchHasTheFocusAndEscClearsItFirst() {
        // C12: focus in search on open; Esc from the search field clears it,
        // then closes the dialog
        activate()
        const d = openSettings()
        const search = inside(d, "settings.search")
        tryVerify(() => search.activeFocus, 2000)
        keyClick(Qt.Key_U)
        keyClick(Qt.Key_N)
        compare(search.text, "un")
        verify(d.searching)
        keyClick(Qt.Key_Escape)
        compare(search.text, "")
        verify(d.visible)                       // the first Esc only cleared
        keyClick(Qt.Key_Escape)
        tryVerify(() => !d.visible, 2000)
    }

    function test_theLastPageIsRemembered() {
        let d = openSettings()
        mouseClick(inside(d, "settings.nav.performance"))
        compare(d.page, "performance")
        d.close()
        tryVerify(() => !d.visible, 2000)
        d = openSettings()
        compare(d.page, "performance")
        verify(inside(d, "settings.row.workerThreads").visible)
        d.page = "general"
    }

    function test_theDropdownStepsWithTheArrowKeys() {
        // PR-19: Up and Down change the value when the dropdown has focus
        activate()
        const d = openSettings()
        const undo = inside(d, "settings.undoDepth")
        undo.forceActiveFocus()
        tryVerify(() => undo.activeFocus, 2000)
        compare(App.settings.undoDepth, 25)
        keyClick(Qt.Key_Down)
        compare(App.settings.undoDepth, 50)
        keyClick(Qt.Key_Down)
        compare(App.settings.undoDepth, 100)
        keyClick(Qt.Key_Up)
        compare(App.settings.undoDepth, 50)
    }

    function test_theSliderStepsWithTheArrowKeys() {
        if (App.settings.maxWorkerThreads < 2)
            skip("one core: nothing to step")
        activate()
        App.settings.workerThreads = 1
        const d = openSettings()
        mouseClick(inside(d, "settings.nav.performance"))
        waitForRendering(d.contentItem)
        const slider = inside(d, "settings.workerThreads.slider")
        slider.forceActiveFocus()
        tryVerify(() => slider.activeFocus, 2000)
        keyClick(Qt.Key_Right)
        compare(App.settings.workerThreads, 2)
        keyClick(Qt.Key_Left)
        compare(App.settings.workerThreads, 1)
        d.page = "general"
    }

    function test_tabGoesSearchNavRowsFooter() {
        // C12: Tab order is search -> nav -> rows (top to bottom) -> footer;
        // the close button is reached with Esc, not Tab
        const d = openSettings()
        const search = inside(d, "settings.search")
        const order = []
        let it = search
        for (let i = 0; i < 60; i++) {
            it = it.nextItemInFocusChain(true)
            if (!it || it === search)
                break
            if (it.objectName.length > 0 && it.visible)
                order.push(it.objectName)
        }
        const pos = (name) => order.indexOf(name)
        verify(pos("settings.nav.general") >= 0, JSON.stringify(order))
        verify(pos("settings.nav.general") < pos("settings.nav.view3d"))
        verify(pos("settings.nav.performance") < pos("settings.density"))
        verify(pos("settings.density") < pos("settings.theme"))
        verify(pos("settings.theme") < pos("settings.undoDepth"))
        verify(pos("settings.undoDepth") < pos("settings.tooltips"))
        verify(pos("settings.tooltips") < pos("settings.restoreAll"))
        verify(pos("settings.restoreAll") < pos("settings.done"))
        compare(pos("settings.close"), -1)
    }
}
