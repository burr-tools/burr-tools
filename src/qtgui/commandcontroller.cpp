/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include "commandcontroller.h"
#include "app.h"
#include "documentcontroller.h"
#include "editorcontroller.h"
#include "layoutcontroller.h"
#include "settingscontroller.h"
#include "shapesmodel.h"
#include "stlexportcontroller.h"
#include "viewportcontroller.h"

#include "../uicore/shapelist.h"

#include "../lib/bt_assert.h"
#include "../lib/puzzle.h"

#include <QDesktopServices>
#include <QKeySequence>
#include <QUrl>
#include <QVariantMap>

using btui::Command;

namespace {

  // the user guide legacy opens (src/gui/platform.cpp)
  const char * const userGuideUrl = "https://burrtools.sourceforge.net/gui-doc/toc.html";

  QString str(std::string_view s) {
    return QString::fromUtf8(s.data(), qsizetype(s.size()));
  }

  btui::Menu menuForKey(const QString & key) {
    for (const auto & m : btui::menuBar())
      if (str(m.key) == key)
        return m.menu;
    return btui::Menu::None;
  }
}

CommandController::CommandController(SettingsController * settings, DocumentController * doc, LayoutController * layout,
                                     ViewportController * viewport, EditorController * editor, ShapesModel * shapes,
                                     QObject * parent) :
  QObject(parent), m_settings(settings), m_doc(doc), m_layout(layout), m_viewport(viewport), m_editor(editor),
  m_shapes(shapes)
{
  auto bump = [this] { m_revision++; emit revisionChanged(); };
  connect(m_settings, &SettingsController::changed, this, bump);
  connect(m_doc, &DocumentController::stateChanged, this, bump);
  connect(m_doc, &DocumentController::documentReplaced, this, bump);
  connect(m_layout, &LayoutController::changed, this, bump);
  connect(m_shapes, &ShapesModel::selectedChanged, this, bump);
  connect(m_shapes, &ShapesModel::countChanged, this, bump);
}

void CommandController::setBlocked(bool b) {
  if (b == m_blocked)
    return;
  m_blocked = b;
  m_revision++;
  emit revisionChanged();
}

bool CommandController::menuItemsOwnShortcuts(void) {
#ifdef Q_OS_MACOS
  return true;
#else
  return false;
#endif
}

bool CommandController::menuBarHideable(void) {
  return !menuItemsOwnShortcuts();
}

// macOS: Command (Qt's "Ctrl") is Variable and Option (Qt's "Alt") erases
QString CommandController::variableKeyName(void) {
  return btui::currentPlatform() == btui::Platform::Mac ? QStringLiteral("Command") : QStringLiteral("Alt");
}

QString CommandController::eraseKeyName(void) {
  return btui::currentPlatform() == btui::Platform::Mac ? QStringLiteral("Option") : QStringLiteral("Ctrl");
}

std::span<const std::string_view> CommandController::platformShortcuts(const btui::CommandInfo & c) {
  return btui::shortcutsOn(c, btui::currentPlatform());
}

QString CommandController::nativeShortcutText(const btui::CommandInfo & c) {
  QStringList shown;
  for (auto s : platformShortcuts(c).first(std::min<size_t>(2, platformShortcuts(c).size())))
    shown << QKeySequence(str(s), QKeySequence::PortableText).toString(QKeySequence::NativeText);
  return shown.join(QStringLiteral(" / "));
}

QString CommandController::shortcutText(const QString & key) const {
  const btui::CommandInfo * c = btui::findCommand(key.toStdString());
  return c ? nativeShortcutText(*c) : QString();
}

/* Access keys are a Windows and Linux convention; macOS menus have none. */
QString CommandController::platformLabel(std::string_view label) {
#ifdef Q_OS_MACOS
  return QString::fromStdString(btui::plainLabel(label));
#else
  return str(label);
#endif
}

QString CommandController::platformLabel(const btui::CommandInfo & c) {
  return platformLabel(btui::labelOn(c, btui::currentPlatform()));
}

namespace {
  // the focused 3D view's steps (WCAG 2.1.1)
  constexpr float kOrbitStepDeg = 15.0f;
  constexpr float kPanStepDp = 40.0f;
}

QVariantList CommandController::menuBar(void) const {
  QVariantList out;
  for (const auto & m : btui::menuBar()) {
    QVariantMap e;
    e.insert(QStringLiteral("key"), str(m.key));
    e.insert(QStringLiteral("label"), platformLabel(m.label));
    out.append(e);
  }
  return out;
}

QVariantList CommandController::menuItems(const QString & menuKey) const {
  QVariantList out;
  const btui::Menu menu = menuForKey(menuKey);
  if (menu == btui::Menu::None)
    return out;
  for (const btui::MenuEntry & entry : btui::menuEntries(menu, btui::currentPlatform())) {
    const btui::CommandInfo & c = *entry.command;
    auto list = platformShortcuts(c);
    QVariantMap e;
    e.insert(QStringLiteral("key"), str(c.key));
    e.insert(QStringLiteral("label"), platformLabel(c));
    // on macOS the item owns its first shortcut (a key equivalent); the
    // others, and elsewhere all, are bound by shortcuts()
    e.insert(QStringLiteral("shortcut"), menuItemsOwnShortcuts() && !list.empty() ? str(list.front()) : QString());
    e.insert(QStringLiteral("shortcutText"), nativeShortcutText(c));
    e.insert(QStringLiteral("checkable"), isCheckable(str(c.key)));
    e.insert(QStringLiteral("separatorBefore"), entry.separatorBefore);
    out.append(e);
  }
  return out;
}

QVariantList CommandController::shortcuts(void) const {
  QVariantList out;
  for (const auto & c : btui::commandTable()) {
    if (c.scope != btui::Scope::Window)
      continue;
    auto list = platformShortcuts(c);
    // on macOS a menu command's first sequence belongs to its menu item
    if (menuItemsOwnShortcuts() && btui::menuOn(c, btui::currentPlatform()) != btui::Menu::None && !list.empty())
      list = list.subspan(1);
    if (list.empty())
      continue;
    QStringList seqs;
    bool single = false;
    for (auto s : list) {
      seqs.append(str(s));
      single = single || btui::isSingleCharacter(s);
    }
    QVariantMap e;
    e.insert(QStringLiteral("key"), str(c.key));
    e.insert(QStringLiteral("sequences"), seqs);
    // Settings ▸ General ▸ Single-key shortcuts turns these off (WCAG 2.1.4)
    e.insert(QStringLiteral("singleKey"), single);
    out.append(e);
  }
  return out;
}

QString CommandController::keyInScope(const QString & scope, int key, int modifiers) const {
  const btui::Scope sc = scope == QLatin1String("shapesList") ? btui::Scope::ShapesList
                       : scope == QLatin1String("viewport")   ? btui::Scope::Viewport
                                                              : btui::Scope::Window;
  if (sc == btui::Scope::Window)
    return {};
  // The keypad flag is no part of a shortcut (macOS reports the arrow keys
  // with it), and the main Enter key and the keypad's are one: Qt calls the
  // first Return, the table writes Enter (Return on macOS, for its glyph).
  const auto normal = [](QKeyCombination k) {
    return QKeyCombination(k.keyboardModifiers() & ~Qt::KeypadModifier,
                           k.key() == Qt::Key_Return ? Qt::Key_Enter : k.key());
  };
  const QKeyCombination pressed = normal(QKeyCombination(Qt::KeyboardModifiers(modifiers), Qt::Key(key)));
  for (const auto & c : btui::commandTable()) {
    if (c.scope != sc)
      continue;
    for (auto s : platformShortcuts(c)) {
      const QKeySequence seq(str(s), QKeySequence::PortableText);
      if (seq.count() == 1 && normal(seq[0]) == pressed)
        return str(c.key);
    }
  }
  return {};
}

bool CommandController::isEnabled(const QString & key) const {
  const btui::CommandInfo * c = btui::findCommand(key.toStdString());
  if (!c)
    return false;
  const int shape = m_shapes->selected();
  const bool inEntities = m_layout->workspace() == LayoutController::Entities;
  switch (c->id) {
    case Command::Undo: return m_doc->canUndo();
    case Command::Redo: return m_doc->canRedo();
    // legacy updateInterface(): nothing to export without a shape, and STL
    // only from a grid that has an exporter
    case Command::ExportImage: return m_doc->session().puzzle().getNumberOfShapes() > 0;
    case Command::ExportStl: return StlExportController::available(m_doc);
    // Entities-only keys do nothing in Puzzle and Solver (input map scoping rule)
    case Command::LayerUp:
    case Command::LayerDown:
    case Command::Edit3d:
    case Command::ToolFixed:
    case Command::ToolVariable:
    case Command::ToolErase:
    case Command::ToolPaint:
      return inEntities;
    // the Shapes list's keys act on the selected shape
    case Command::ShapeRename:
    case Command::ShapeDuplicate:
      return inEntities && shape >= 0;
    case Command::ShapeDelete:
      return inEntities && btui::canDeleteShape(m_doc->session().puzzle(), shape);
    case Command::ShapeMoveEarlier:
      return inEntities && shape > 0;
    case Command::ShapeMoveLater:
      return inEntities && shape >= 0 && shape < m_shapes->count() - 1;
    case Command::New: case Command::Open: case Command::Import: case Command::Close:
    case Command::Save: case Command::SaveAs: case Command::Convert:
    case Command::ImportAssemblies: case Command::Settings: case Command::Quit:
    case Command::EditComment:
    case Command::ToggleMenuBar: case Command::Status: case Command::FullScreen:
    case Command::Focus3d: case Command::WorkspaceEntities: case Command::WorkspacePuzzle:
    case Command::WorkspaceSolver: case Command::ToggleLeftCard: case Command::ToggleRightCard:
    case Command::ExportVector:
    case Command::HelpGuide: case Command::HelpShortcuts: case Command::CheckForUpdates:
    case Command::About:
    case Command::ViewOrbit: case Command::ViewPan: case Command::ViewHome: case Command::ViewFit:
    case Command::ViewOrbitLeft: case Command::ViewOrbitRight: case Command::ViewOrbitUp: case Command::ViewOrbitDown:
    case Command::ViewPanLeft: case Command::ViewPanRight: case Command::ViewPanUp: case Command::ViewPanDown:
    case Command::ViewZoomIn: case Command::ViewZoomOut:
    case Command::ViewTurnLeft: case Command::ViewTurnRight: case Command::ViewTurnUp: case Command::ViewTurnDown:
    case Command::ViewRollLeft: case Command::ViewRollRight:
    case Command::ViewFront: case Command::ViewBack: case Command::ViewLeft: case Command::ViewRight:
    case Command::ViewTop: case Command::ViewBottom:
      return true;
  }
  return true;
}

bool CommandController::isCheckable(const QString & key) const {
  return key == QLatin1String("view.menuBar");
}

bool CommandController::isChecked(const QString & key) const {
  if (key == QLatin1String("view.menuBar"))
    return m_settings->showMenuBar();
  return false;
}

QStringList CommandController::keyCaps(std::string_view portable) {
  const QString native = QKeySequence(str(portable), QKeySequence::PortableText).toString(QKeySequence::NativeText);
#ifdef Q_OS_MACOS
  return { native };
#else
  // "Ctrl++" ends in the plus key itself
  QStringList caps;
  QString cur;
  for (qsizetype i = 0; i < native.size(); i++) {
    if (native[i] == QLatin1Char('+') && !cur.isEmpty()) {
      caps << cur;
      cur.clear();
    } else {
      cur += native[i];
    }
  }
  if (!cur.isEmpty())
    caps << cur;
  return caps;
#endif
}

namespace {

  /* a modifier's key cap: a modifier alone is no key sequence, so it is
   * read from one with a key, and the key dropped */
  QString modifierCap(const char * modifier) {
    const QStringList caps = CommandController::keyCaps(std::string(modifier) + "+A");
#ifdef Q_OS_MACOS
    return caps.value(0).chopped(1);
#else
    return caps.value(0);
#endif
  }

  QVariantMap part(const QString & text, bool mouse = false) {
    return { { QStringLiteral("text"), text }, { QStringLiteral("mouse"), mouse } };
  }

  /* one alternative: key caps, then optionally a mouse gesture */
  QVariantList alt(const QStringList & caps, const QString & mouse = QString()) {
    QVariantList out;
    for (const QString & c : caps)
      out << part(c);
    if (!mouse.isEmpty())
      out << part(mouse, true);
    return out;
  }

  QVariantMap row(const QString & action, const QVariantList & keys) {
    return { { QStringLiteral("action"), action }, { QStringLiteral("keys"), keys } };
  }

  QVariantMap group(const QString & title, const QString & sub, const QVariantList & rows) {
    return { { QStringLiteral("title"), title }, { QStringLiteral("sub"), sub }, { QStringLiteral("rows"), rows } };
  }

  /* every sequence of the commands, each an alternative */
  QVariantList keysOf(std::initializer_list<btui::Command> cmds) {
    QVariantList out;
    for (btui::Command c : cmds)
      for (auto s : CommandController::platformShortcuts(btui::commandInfo(c)))
        out << QVariant(alt(CommandController::keyCaps(s)));
    return out;
  }
}

QVariantList CommandController::shortcutHelp(void) const {
  using btui::Command;
  const QString shift = keyCaps("Shift").value(0);
  // the quick tools' keys (btui::quickTool)
  const bool mac = btui::currentPlatform() == btui::Platform::Mac;
  // macOS: Command (Qt's "Ctrl") and Option (Qt's "Alt")
  const QString variableKey = modifierCap(mac ? "Ctrl" : "Alt");
  const QString eraseKey = modifierCap(mac ? "Alt" : "Ctrl");

  QVariantList everywhere{
    row(tr("Switch to Entities / Puzzle / Solver"), keysOf({ Command::WorkspaceEntities, Command::WorkspacePuzzle, Command::WorkspaceSolver })),
    row(tr("Keyboard shortcuts (this list)"), keysOf({ Command::HelpShortcuts })),
    row(tr("Settings"), keysOf({ Command::Settings })),
    row(tr("Collapse or expand the left / right sidebar"), keysOf({ Command::ToggleLeftCard, Command::ToggleRightCard })),
    row(tr("Focus the 3D view (toggle)"), keysOf({ Command::Focus3d })),
    row(tr("Full screen (toggle)"), keysOf({ Command::FullScreen })),
    row(tr("Step back: close dialog → close menu → exit focus mode"), { QVariant(alt({ QStringLiteral("Esc") })) }),
    row(tr("Move keyboard focus between controls"), { QVariant(alt({ QStringLiteral("Tab") })), QVariant(alt({ shift, QStringLiteral("Tab") })) }),
    row(tr("Activate the focused button, switch or chip"), { QVariant(alt({ QStringLiteral("Enter") })), QVariant(alt({ QStringLiteral("Space") })) }),
  };
  /* the one-letter and number keys stop with Settings ▸ General ▸
   * Single-key shortcuts off (WCAG 2.1.4): the list says so */
  const QString singleOff = m_settings->singleKeyShortcuts() ? QString() : tr(" — off (Settings › General)");

  QVariantList view3d{
    row(tr("Orbit (Orbit mode) or pan (Pan mode)"), { QVariant(alt({}, tr("Left-drag"))) }),
    row(tr("The other of orbit / pan"), { QVariant(alt({ shift }, tr("Left-drag"))) }),
    row(tr("Pan"), { QVariant(alt({}, tr("Middle-drag"))) }),
    row(tr("Zoom"), { QVariant(alt({}, tr("Wheel"))) }),
    row(tr("Orbit mode / Pan mode") + singleOff, keysOf({ Command::ViewOrbit, Command::ViewPan })),
    row(tr("Home view — default orientation, framed"), keysOf({ Command::ViewHome })),
    row(tr("Fit the shape to the view, orientation kept") + singleOff, keysOf({ Command::ViewFit })),
    row(tr("Turn to that view"), { QVariant(alt({}, tr("Click view-cube face, edge or corner"))) }),
    row(tr("Upright view of that face"), { QVariant(alt({}, tr("Double-click a view-cube face"))) }),
  };

  // with the 3D view focused (Tab to it): the mouse's gestures and the cube's views
  QVariantList view3dFocused{
    row(tr("Orbit"), keysOf({ Command::ViewOrbitLeft, Command::ViewOrbitRight, Command::ViewOrbitUp, Command::ViewOrbitDown })),
    row(tr("Pan"), keysOf({ Command::ViewPanLeft, Command::ViewPanRight, Command::ViewPanUp, Command::ViewPanDown })),
    row(tr("Zoom in / out"), keysOf({ Command::ViewZoomIn, Command::ViewZoomOut })),
    row(tr("Turn 90° (the view cube's arrows)"),
        keysOf({ Command::ViewTurnLeft, Command::ViewTurnRight, Command::ViewTurnUp, Command::ViewTurnDown })),
    row(tr("Roll 90° counter-clockwise / clockwise"), keysOf({ Command::ViewRollLeft, Command::ViewRollRight })),
    row(tr("View from the front / back / left / right / top / bottom"),
        keysOf({ Command::ViewFront, Command::ViewBack, Command::ViewLeft, Command::ViewRight, Command::ViewTop, Command::ViewBottom })),
  };

  QVariantList entities{
    row(tr("3D edit mode: a click in the 3D view applies the tool (toggle)") + singleOff, keysOf({ Command::Edit3d })),
    row(tr("Tools: Fixed, Variable, Erase, Paint") + singleOff,
        keysOf({ Command::ToolFixed, Command::ToolVariable, Command::ToolErase, Command::ToolPaint })),
    row(tr("Next / previous layer"), keysOf({ Command::LayerUp, Command::LayerDown })),
    row(tr("Quick tool while held: Fixed / Variable / Erase"),
        { QVariant(alt({ shift }, tr("Click"))), QVariant(alt({ variableKey }, tr("Click"))),
          QVariant(alt({ eraseKey }, tr("Click"))) }),
    row(tr("Erase, whatever the tool"), { QVariant(alt({}, tr("Right-click or right-drag"))) }),
  };

  // with the list focused (C01 Keyboard)
  QVariantList shapesList{
    row(tr("Select the previous / next shape"), { QVariant(alt(keyCaps("Up"))), QVariant(alt(keyCaps("Down"))) }),
    row(tr("Move the shape earlier / later (renumbers)"), keysOf({ Command::ShapeMoveEarlier, Command::ShapeMoveLater })),
    row(tr("Rename: Enter commits, Esc cancels"), keysOf({ Command::ShapeRename }) << QVariant(alt({}, tr("Double-click")))),
    row(tr("Duplicate, as the last shape"), keysOf({ Command::ShapeDuplicate })),
    row(tr("Delete"), keysOf({ Command::ShapeDelete })),
    row(tr("Reorder"), { QVariant(alt({}, tr("Drag a row"))) }),
    row(tr("The shape's menu"), { QVariant(alt({}, tr("Right-click"))) }),
  };

  QVariantList dialogs{
    row(tr("Close the dialog or menu"), { QVariant(alt({ QStringLiteral("Esc") })) }),
    row(tr("Commit a number field"), { QVariant(alt({ QStringLiteral("Enter") })) }),
    row(tr("Toggle the focused switch"), { QVariant(alt({ QStringLiteral("Space") })), QVariant(alt({ QStringLiteral("Enter") })) }),
  };

  return {
    group(tr("Everywhere"), QString(), everywhere),
    group(tr("3D view"), tr("every workspace"), view3d),
    group(tr("3D view, focused"), tr("Tab to the 3D view"), view3dFocused),
    group(tr("Entities"), tr("voxel editor"), entities),
    group(tr("Shapes list"), tr("Entities, with the list focused"), shapesList),
    group(tr("Dialogs and menus"), QString(), dialogs),
  };
}

void CommandController::trigger(const QString & key) {
  const btui::CommandInfo * c = btui::findCommand(key.toStdString());
  if (!c || !isEnabled(key))
    return;
  try {
    run(c->id);
  } catch (const std::exception & e) {
    if (App * app = App::instance())
      app->handleInternalError(e);
  }
}

void CommandController::run(Command c) {
  switch (c) {
    case Command::New:              m_doc->requestNew(); break;
    case Command::Open:             m_doc->requestOpen(); break;
    case Command::Import:           m_doc->requestImport(); break;
    case Command::Save:             m_doc->save(); break;
    case Command::SaveAs:           m_doc->requestSaveAs(); break;
    case Command::Quit:             m_doc->requestQuit(); break;
    // the only window closing ends the program, as legacy's macOS Close did
    case Command::Close:            m_doc->requestQuit(); break;
    case Command::Undo:             m_doc->undo(); break;
    case Command::Redo:             m_doc->redo(); break;

    case Command::Convert:          emit convertRequested(); break;
    case Command::ImportAssemblies: emit importAssembliesRequested(); break;
    case Command::Status:           emit statusRequested(); break;
    case Command::ToggleMenuBar:    m_settings->setShowMenuBar(!m_settings->showMenuBar()); break;
    case Command::ExportStl:        emit stlExportRequested(); break;
    case Command::ExportVector:     emit vectorExportRequested(); break;

    case Command::ExportImage:      emit imageExportRequested(); break;

    case Command::EditComment:      emit commentRequested(); break;
    case Command::HelpGuide:        QDesktopServices::openUrl(QUrl(QString::fromLatin1(userGuideUrl))); break;
    case Command::HelpShortcuts:    emit shortcutsHelpRequested(); break;
    case Command::CheckForUpdates:  emit checkForUpdatesRequested(); break;
    case Command::About:            emit aboutRequested(); break;
    case Command::Settings:         emit settingsRequested(); break;
    case Command::FullScreen:       emit fullScreenToggleRequested(); break;
    case Command::Focus3d:          m_layout->toggleFocus3d(); break;
    case Command::WorkspaceEntities: m_layout->setWorkspace(LayoutController::Entities); break;
    case Command::WorkspacePuzzle:  m_layout->setWorkspace(LayoutController::Puzzle); break;
    case Command::WorkspaceSolver:  m_layout->setWorkspace(LayoutController::Solver); break;
    case Command::ToggleLeftCard:   m_layout->toggleLeft(); break;
    case Command::ToggleRightCard:  m_layout->toggleRight(); break;
    case Command::ViewOrbit:        m_viewport->setNavMode(QStringLiteral("orbit")); emit toolbarModeUsed(); break;
    case Command::ViewPan:          m_viewport->setNavMode(QStringLiteral("pan")); emit toolbarModeUsed(); break;
    case Command::ViewHome:         m_viewport->home(); break;
    case Command::ViewFit:          m_viewport->fit(); emit toolbarModeUsed(); break;
    case Command::LayerUp:          m_editor->layerStep(+1); break;
    case Command::LayerDown:        m_editor->layerStep(-1); break;
    case Command::ToolFixed:        m_editor->setTool(EditorController::Fixed); break;
    case Command::ToolVariable:     m_editor->setTool(EditorController::Variable); break;
    case Command::ToolErase:        m_editor->setTool(EditorController::Erase); break;
    case Command::ToolPaint:        m_editor->setTool(EditorController::Paint); break;
    case Command::ShapeRename:      m_shapes->requestRename(m_shapes->selected()); break;
    case Command::ShapeDuplicate:   m_shapes->duplicateShape(m_shapes->selected()); break;
    case Command::ShapeDelete:      m_shapes->requestDelete(m_shapes->selected()); break;
    case Command::ShapeMoveEarlier: m_shapes->moveShape(m_shapes->selected(), m_shapes->selected() - 1); break;
    case Command::ShapeMoveLater:   m_shapes->moveShape(m_shapes->selected(), m_shapes->selected() + 1); break;
    case Command::Edit3d:           m_editor->toggleEditMode(); emit toolbarModeUsed(); break;

    // the focused 3D view: 15 degrees, 40 dp, a wheel notch; the cube's turns and faces
    case Command::ViewOrbitLeft:    m_viewport->orbitStep(-kOrbitStepDeg, 0); break;
    case Command::ViewOrbitRight:   m_viewport->orbitStep(kOrbitStepDeg, 0); break;
    case Command::ViewOrbitUp:      m_viewport->orbitStep(0, kOrbitStepDeg); break;
    case Command::ViewOrbitDown:    m_viewport->orbitStep(0, -kOrbitStepDeg); break;
    case Command::ViewPanLeft:      m_viewport->panStep(-kPanStepDp, 0); break;
    case Command::ViewPanRight:     m_viewport->panStep(kPanStepDp, 0); break;
    case Command::ViewPanUp:        m_viewport->panStep(0, -kPanStepDp); break;
    case Command::ViewPanDown:      m_viewport->panStep(0, kPanStepDp); break;
    case Command::ViewZoomIn:       m_viewport->zoomStep(+1); break;
    case Command::ViewZoomOut:      m_viewport->zoomStep(-1); break;
    case Command::ViewTurnLeft:     m_viewport->turn(btui::ViewCube::Arrow::Left); break;
    case Command::ViewTurnRight:    m_viewport->turn(btui::ViewCube::Arrow::Right); break;
    case Command::ViewTurnUp:       m_viewport->turn(btui::ViewCube::Arrow::Up); break;
    case Command::ViewTurnDown:     m_viewport->turn(btui::ViewCube::Arrow::Down); break;
    case Command::ViewRollLeft:     m_viewport->roll(+1); break;
    case Command::ViewRollRight:    m_viewport->roll(-1); break;
    // the view cube's faces, by its labels: world +Y is up, +Z toward the viewer at the front
    case Command::ViewFront:        m_viewport->viewFrom({ 0, 0, 1 }); break;
    case Command::ViewBack:         m_viewport->viewFrom({ 0, 0, -1 }); break;
    case Command::ViewLeft:         m_viewport->viewFrom({ -1, 0, 0 }); break;
    case Command::ViewRight:        m_viewport->viewFrom({ 1, 0, 0 }); break;
    case Command::ViewTop:          m_viewport->viewFrom({ 0, 1, 0 }); break;
    case Command::ViewBottom:       m_viewport->viewFrom({ 0, -1, 0 }); break;
  }
}
