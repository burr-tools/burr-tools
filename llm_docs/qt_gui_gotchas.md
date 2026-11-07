# Qt GUI: gotchas found the hard way

- An `id` on a `Popup`'s `contentItem` (any name) broke its layout (rows
  102 px wide, clicks missing) in `DisplayMenu.qml`; reach the rows through
  `root.contentItem.children` instead.
- `x as Popup` is null for `QtQuick.Controls.Basic` popups (a separate QML
  type); cast to `Dialog` or the concrete type.
- A JavaScript line starting with `(` continues the previous statement
  (automatic semicolon insertion); assign to a `const` first.
- Menus open as their own windows (`Popup.Window`): a click in the main
  window does not close them by itself (`KeyboardCues::windowPressed`), and
  the application's activation, not the window's, tells when focus left.
- `forceActiveFocus` in a test window can leave focus nowhere; Tab works.
- A delegate that redeclares a property of its base type
  (`required property string title` on an `AccordionSection`) shadows it, and
  the base draws nothing: write `required title`. A property named like an
  `Item` one (`top`) does not load at all, and only a test run says why.
- A binding that calls a function reading a model does not follow the model:
  read something that notifies, in the expression (`rules.revision >= 0 && ...`).
  A `const` that only reads it can be compiled away.
- A `Dropdown` click writes `currentIndex` from JavaScript, which ends a binding
  on it: an owner that binds it binds it again in `onActivated`
  (`Qt.binding`). A test of a dialog clicks its buttons: calling the controller's
  answer directly hid a confirm that closed before it answered.
- `required used` on a delegate marks a base type's own property required; a
  `Popup`'s rows need a frame to be laid out before a test clicks them.
- Collapsing is per workspace: a test that collapses a card in Puzzle resets
  it there, before leaving for Entities.
- The Qt Quick message dialogs are in-window and modal on Windows and Linux:
  count them in `modalOpen`, and dismiss them in tests.
- A C++ enum whose values do not count up from 0 reads as `undefined` in
  ahead-of-time compiled QML; hand QML the numbers as a map
  (`InspectorController::tasks`).
- A `HoverHandler` on a parent sees no hover over the 3D view (a
  `QQuickRhiItem` that takes hover itself): put a strip item with a
  non-blocking `HoverHandler` over the part to watch (`ViewportCard`'s
  reveal zone).
- A `ListView` whose height depends on its `count` resizes inside a model
  reset and lays out rows the reset just took ("DelegateModel::cancel"
  warnings): size it from outside, `cacheBuffer: 0`.
- Positioners (`Row`, `Column`, `Flow`) lay their children out on the next
  polish: a test reads their geometry after `waitForRendering` or
  `tryVerify`, not at once.
- Windows draws the title bar dark or light from
  `QStyleHints::setColorScheme`; ask for the theme's scheme before the
  first window (`Theme`'s constructor), not only when it changes.
