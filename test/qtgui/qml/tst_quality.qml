import QtQuick
import QtQuick.Controls
import QtTest
import BurrTools.Ui

// Readability and accessibility of the shell, in the light and the dark
// theme: colour contrast (WCAG AA), menus and dialogs that fit their text,
// rendered colours that match the theme, and the roles and names assistive
// tools read. Structural checks rather than reference images, so fonts that
// differ between platforms do not matter.
TestCase {
    id: tc
    name: "Quality"
    when: windowShown

    Component { id: mainComponent; Main {} }

    property var win

    function init() {
        // see tst_shell.qml for why these two warnings are allowed
        failOnWarning(/^(?!No QRhi found for window|This plugin does not support grabbing the keyboard).*/)
        win = createTemporaryObject(mainComponent, tc)
        verify(win !== null)
        win.width = 1400
        win.height = 900
        App.document.openFile(Qt.resolvedUrl("../../../examples/PelikanBurr.xmpuzzle"))
        waitForRendering(win.contentItem)
        tryVerify(() => win.contentReady, 5000)       // made after the first frame
    }

    function cleanup() {
        for (const name of ["settings.dialog", "help.shortcuts.dialog", "shell.about.dialog"]) {
            const d = findChild(win, name)
            if (d && d.visible)
                d.close()
        }
        App.settings.theme = "system"
        App.settings.tooltips = true
        App.viewport.navMode = "orbit"
    }

    // --- WCAG contrast ---------------------------------------------------------

    function channel(c) {
        return c <= 0.03928 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4)
    }
    function luminance(c) {
        return 0.2126 * channel(c.r) + 0.7152 * channel(c.g) + 0.0722 * channel(c.b)
    }
    function contrast(a, b) {
        const la = luminance(a), lb = luminance(b)
        return (Math.max(la, lb) + 0.05) / (Math.min(la, lb) + 0.05)
    }

    function test_textContrastMeetsWcagAA_data() {
        return [ { tag: "light", theme: "light" }, { tag: "dark", theme: "dark" } ]
    }
    function test_textContrastMeetsWcagAA(data) {
        App.settings.theme = data.theme
        // body text, secondary (muted) text, on the card and the raised fill
        for (const fill of ["panel", "panel2"]) {
            verify(contrast(Theme.text, Theme[fill]) >= 4.5, "text on " + fill)
            verify(contrast(Theme.muted, Theme[fill]) >= 4.5,
                   "muted on " + fill + ": " + contrast(Theme.muted, Theme[fill]).toFixed(2))
        }
        // primary buttons: white text on the accent
        if (data.theme === "dark")
            expectFail("dark", "the dark accent #5b9bff gives white button text 2.77:1 -- a design-token decision is pending")
        verify(contrast(Qt.color("white"), Theme.accent) >= 4.5,
               "white on accent: " + contrast(Qt.color("white"), Theme.accent).toFixed(2))
    }

    // --- menus fit their text ----------------------------------------------------

    function test_menuLabelsNeverRunIntoTheirShortcuts_data() {
        return [ { tag: "light", theme: "light" }, { tag: "dark", theme: "dark" } ]
    }
    function test_menuLabelsNeverRunIntoTheirShortcuts(data) {
        if (Qt.platform.os === "osx")
            skip("the system draws the menus")
        App.settings.theme = data.theme
        const bar = win.menuBar
        for (let m = 0; m < bar.count; m++) {
            const menu = bar.menuAt(m)
            menu.open()
            tryVerify(() => menu.opened, 2000)
            waitForRendering(menu.contentItem)
            for (let i = 0; i < menu.count; i++) {
                const item = menu.itemAt(i)
                if (!item.objectName.startsWith("shell.menuitem."))
                    continue
                const label = findChild(item, "label"), keys = findChild(item, "keys")
                verify(label !== null && keys !== null)
                verify(item.width <= menu.width, item.objectName + " is wider than its menu")
                if (keys.text.length > 0)
                    verify(label.x + label.width + 8 <= keys.x,
                           item.objectName + ": label ends at " + (label.x + label.width) + ", keys start at " + keys.x)
                else
                    verify(label.x + label.width <= item.contentItem.width + 1, item.objectName + " is clipped")
            }
            menu.close()
            tryVerify(() => !menu.visible, 2000)
        }
    }

    // --- dialogs hold their text ---------------------------------------------------

    function texts(item, out) {
        for (const child of item.children) {
            if (!child.visible)
                continue
            if (child.text !== undefined && typeof child.text === "string" && child.text.length > 0
                    && child.width > 0 && child.hasOwnProperty("wrapMode"))
                out.push(child)
            texts(child, out)
        }
        return out
    }

    function test_dialogTextStaysInsideTheDialog_data() {
        return [
            { tag: "settings light", dialog: "settings.dialog", command: "settings", theme: "light" },
            { tag: "settings dark", dialog: "settings.dialog", command: "settings", theme: "dark" },
            { tag: "shortcuts light", dialog: "help.shortcuts.dialog", command: "help.shortcuts", theme: "light" },
            { tag: "shortcuts dark", dialog: "help.shortcuts.dialog", command: "help.shortcuts", theme: "dark" },
        ]
    }
    function test_dialogTextStaysInsideTheDialog(data) {
        App.settings.theme = data.theme
        App.commands.trigger(data.command)
        const d = findChild(win, data.dialog)
        tryVerify(() => d.opened, 2000)
        waitForRendering(d.contentItem)
        const root = d.contentItem
        const found = texts(root, [])
        verify(found.length > 5)
        for (const t of found) {
            const p = t.mapToItem(root, 0, 0)
            verify(p.x >= -1 && p.x + t.width <= root.width + 1,
                   "\"" + t.text.substring(0, 30) + "\" spans " + p.x + ".." + (p.x + t.width) + " of " + root.width)
        }
    }

    function test_dialogBackgroundIsTheThemesPanel_data() {
        return [ { tag: "light", theme: "light" }, { tag: "dark", theme: "dark" } ]
    }
    function test_dialogBackgroundIsTheThemesPanel(data) {
        App.settings.theme = data.theme
        App.commands.trigger("help.shortcuts")
        const d = findChild(win, "help.shortcuts.dialog")
        tryVerify(() => d.opened, 2000)
        waitForRendering(d.contentItem)
        const img = grabImage(d.background)
        const px = img.pixel(Math.floor(img.width / 2), img.height - 8)
        verify(Qt.colorEqual(px, Theme.panel), "background " + px + ", panel " + Theme.panel)
    }

    // --- accessibility --------------------------------------------------------------

    function test_menuItemsAnnounceTheirLabelsAndState() {
        if (Qt.platform.os === "osx")
            skip("the system menu bar announces itself")
        const file = win.menuBar.menuAt(0)
        let save = null
        for (let i = 0; i < file.count; i++)
            if (file.itemAt(i).objectName === "shell.menuitem.file.save")
                save = file.itemAt(i)
        verify(save !== null)
        compare(save.Accessible.role, Accessible.MenuItem)
        compare(save.Accessible.name, "Save")             // no "&" marker read out

        if (App.commands.menuBarHideable) {
            const view = win.menuBar.menuAt(2)
            let toggle = null
            for (let i = 0; i < view.count; i++)
                if (view.itemAt(i).objectName === "shell.menuitem.view.menuBar")
                    toggle = view.itemAt(i)
            verify(toggle !== null)
            verify(toggle.checkable)
            verify(toggle.checked)
            compare(toggle.Accessible.name, "Show menu bar")
        }
    }

    function test_railAndToolbarButtonsHaveNames() {
        const rail = ["rail-entities", "rail-puzzle", "rail-solver"]
        for (const name of rail) {
            const b = findChild(win.contentItem, name)
            verify(b !== null)
            verify(b.Accessible.name.length > 0, name)
        }
        if (App.commands.menuBarHideable) {
            App.settings.showMenuBar = false
            const menu = findChild(win.contentItem, "rail-menu")
            tryVerify(() => menu.visible, 1000)
            compare(menu.Accessible.name, "Menu")
            App.settings.showMenuBar = true
        }
    }

    function test_settingsControlsAreNamedByTheirRows() {
        App.commands.trigger("settings")
        const d = findChild(win, "settings.dialog")
        tryVerify(() => d.opened, 2000)
        waitForRendering(d.contentItem)
        const find = (n) => findChild(d.contentItem, n) || findChild(d.header, n)

        const nav = find("settings.nav.general")
        compare(nav.Accessible.role, Accessible.PageTab)
        compare(nav.Accessible.name, "General")

        const tips = find("settings.tooltips")
        compare(tips.Accessible.role, Accessible.CheckBox)
        compare(tips.Accessible.name, "Show tooltips")
        compare(tips.Accessible.checked, App.settings.tooltips)

        compare(find("settings.undoDepth").Accessible.name, "Undo history depth")

        const theme = find("settings.theme")
        compare(theme.Accessible.name, "Theme")
        const system = find("settings.theme." + App.settings.theme)
        compare(system.Accessible.role, Accessible.RadioButton)
        verify(system.Accessible.checked)

        compare(find("settings.done").Accessible.name, "Done")
    }

    // --- the accessibility review's fixes ----------------------------------------

    function activate() {
        // the file's own messages first (its comment, an unfinished search):
        // modal, they would take the keys
        for (let i = 0; i < 5; i++) {
            wait(50)
            const m = findChild(win, "shell.message")
            if (m === null || !m.visible)
                break
            m.close()
        }
        win.requestActivate()
        tryVerify(() => win.active, 3000)
    }

    function test_shapeRowsAreNamedSelectableListItems() {
        const list = findChild(win.contentItem, "entities.shapes.list")
        compare(list.Accessible.role, Accessible.List)
        compare(list.Accessible.name, "Shapes")
        App.shapes.select(1)
        const row = findChild(list.contentItem, "entities.shapes.row.1")
        tryVerify(() => row !== null)
        compare(row.Accessible.role, Accessible.ListItem)
        compare(row.Accessible.name, "S2 Cage, weight 2")            // the weight badge, in words
        verify(row.Accessible.selected)
        verify(!findChild(list.contentItem, "entities.shapes.row.0").Accessible.selected)
        compare(findChild(win.contentItem, "entities.shapes.count").Accessible.name,
                App.shapes.count + " shapes")
    }

    function test_theViewTagTextReadsOnEveryShapeColour() {
        for (let i = 0; i < App.shapes.count; i++) {
            App.shapes.select(i)
            verify(contrast(App.viewport.shapeColor, App.viewport.shapeTextColor) >= 4.5,
                   "S" + (i + 1) + ": " + contrast(App.viewport.shapeColor, App.viewport.shapeTextColor).toFixed(2))
        }
    }

    function test_settingsDialogIsNamed() {
        App.commands.trigger("settings")
        const d = findChild(win, "settings.dialog")
        tryVerify(() => d.opened, 2000)
        compare(d.title, "Settings")             // a Dialog's accessible name
    }

    function test_statusBarSpeaksPlainText() {
        const bar = findChild(win.contentItem, "status")
        compare(bar.Accessible.role, Accessible.StatusBar)
        verify(bar.Accessible.name.indexOf("<") < 0, bar.Accessible.name)
        verify(bar.Accessible.name.indexOf("S1") >= 0, bar.Accessible.name)
        App.status.flash("Exported a.stl")
        compare(bar.Accessible.name, "Exported a.stl")
        compare(App.status.lastMessage, "Exported a.stl")       // kept after the flash goes
    }

    function test_displayMenuTakesTheKeyboard() {
        activate()
        const button = findChild(win.contentItem, "entities.viewport.toolbar.display")
        mouseClick(button)
        const menu = findChild(button, "entities.viewport.display.menu")
        tryVerify(() => menu.opened, 1000)
        const axes = findChild(menu.contentItem, "entities.viewport.display.axes")
        const bounds = findChild(menu.contentItem, "entities.viewport.display.bounds")
        tryVerify(() => axes.activeFocus, 1000)                  // the first row, at once
        keyClick(Qt.Key_Down)
        verify(bounds.activeFocus)
        keyClick(Qt.Key_Up)
        verify(axes.activeFocus)
        compare(findChild(menu.contentItem, "entities.viewport.display.persp").Accessible.role,
                Accessible.RadioButton)                         // one of a set
        compare(axes.Accessible.role, Accessible.CheckBox)
        keyClick(Qt.Key_Escape)                                 // the menu has the focus, so Esc closes it
        tryVerify(() => !menu.visible, 1000)
    }

    function test_toolbarStatesAndTipsReachTheKeyboard() {
        activate()
        const orbit = findChild(win.contentItem, "entities.viewport.toolbar.orbit")
        App.viewport.navMode = "orbit"
        verify(orbit.Accessible.checkable)
        verify(orbit.Accessible.checked)
        verify(orbit.Accessible.description.indexOf("(O)") >= 0)
        App.viewport.navMode = "pan"
        verify(!orbit.Accessible.checked)
        App.viewport.navMode = "orbit"
        // the tooltip shows on keyboard focus (Tab), not only under the mouse
        App.settings.tooltips = true
        for (let i = 0; i < 40 && !orbit.activeFocus; i++)
            keyClick(Qt.Key_Tab)
        verify(orbit.activeFocus, "Tab reaches the Orbit button")
        verify(orbit.visualFocus)
        tryVerify(() => orbit.toolTip.visible, 2000)
        const rail = findChild(win.contentItem, "rail-entities")
        compare(rail.Accessible.role, Accessible.PageTab)
        verify(rail.Accessible.selected)
    }

    function test_enterPressesTheDialogsDefaultButton() {
        activate()
        App.commands.trigger("about")
        const d = findChild(win, "shell.about.dialog")
        tryVerify(() => d.opened, 2000)
        tryVerify(() => findChild(d.footer, "shell.about.close").activeFocus, 1000)   // focus starts there
        keyClick(Qt.Key_Return)
        tryVerify(() => !d.visible, 2000)
    }
}
