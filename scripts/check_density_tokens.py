#!/usr/bin/env python3
"""Is every density token of burrtools-qt read by the GUI?

foundations/density.md section 1 is the authority on the GUI's geometry in
its two densities, Standard and Minimal; its numbers are the "density"
tokens of src/qtgui/resources/design-tokens.json. A component that writes
its sizes as literals -- or takes them from its own, older section of the
spec -- ignores the density without any test noticing. This check makes a
token nobody reads an error:

  * a token is read when a Theme property returns it (theme.h,
    `dens("key")`) and that property is used -- `Theme.prop` in the QML or
    `Theme::instance()->prop()` in the C++ -- or when it is one of the layout's metrics
    (uicore/layoutmodel), whose numbers must then equal the tokens';
  * a token not read yet must be listed in PENDING below with the phase that
    builds its component; a listed token that is read fails too, so the
    list cannot go stale;
  * both densities must have the same tokens (ONE_DENSITY excepted), and a
    Theme property must name a token that exists.

usage: check_density_tokens.py [--root DIR]
"""
import argparse
import json
import os
import re
import sys

# tokens whose components are not built yet, and the phase that builds them
PENDING = {
    'accordionHeader': 'P6 Inspector (Accordion, C03-C05)',
    'transform': 'P6 Inspector (transform matrix, C03)',
    'actionButtonMin': 'P6 Inspector (ActionButton, C04-C05)',
    'actionButton': 'P6 Inspector (ActionButton, C04-C05)',
    'puzzleTile': 'P10 Puzzle (C16)',
    'solverListRow': 'P11 Solver (C20)',
    'player': 'P11 Solver (disassembly player)',
    'pieceDisplay': 'P11 Solver (piece display)',
    'toolbarAutoHide': "P12a Minimal's auto-hiding 3D toolbar (density.md section 3)",
}

# tokens one density has and the other has not, and why
ONE_DENSITY = {
    'actionButtonMin': "Standard's action buttons have a minimum height, Minimal's a fixed one",
    'actionButton': "Standard's action buttons have a minimum height, Minimal's a fixed one",
    'toolbarAutoHide': 'only Minimal hides the 3D toolbar',
}


def read(path):
    with open(path, encoding='utf-8') as f:
        return f.read()


def sources(folder, suffixes):
    for dirpath, _, files in os.walk(folder):
        for name in files:
            if name.endswith(suffixes):
                yield os.path.join(dirpath, name)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--root', default=os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
    args = ap.parse_args()
    root = os.path.abspath(args.root)
    qtgui = os.path.join(root, 'src', 'qtgui')
    errors = []

    density = json.loads(read(os.path.join(qtgui, 'resources', 'design-tokens.json')))['density']
    tokens = {d: density[d] for d in ('standard', 'minimal')}

    # the same tokens in both densities
    for d, other in (('standard', 'minimal'), ('minimal', 'standard')):
        for key in sorted(set(tokens[d]) - set(tokens[other]) - set(ONE_DENSITY)):
            errors.append(f'{key}: in {d} but not in {other}')
    keys = set(tokens['standard']) | set(tokens['minimal'])

    # Theme's properties and the token each returns
    theme_h = read(os.path.join(qtgui, 'theme.h'))
    props = {}                                   # property -> token key (its first part)
    for prop, path in re.findall(r'qreal\s+(\w+)\(void\)\s+const\s*\{\s*return\s+dens\("([\w.]+)"\)', theme_h):
        key = path.split('.')[0]
        props[prop] = key
        if key not in keys:
            errors.append(f'Theme.{prop} reads "{path}", which design-tokens.json does not have')

    qml = '\n'.join(read(p) for p in sources(os.path.join(qtgui, 'qml'), ('.qml',)))
    cpp = '\n'.join(read(p) for p in sources(qtgui, ('.cpp', '.h'))
                    if os.path.basename(p) not in ('theme.cpp', 'theme.h'))
    used = set()
    for prop, key in props.items():
        # C++ through the instance (Theme::instance()->prop(), m_theme->prop()),
        # not any prop() -- event->button() is no use of the token
        if re.search(r'\bTheme\.' + prop + r'\b', qml) or \
           re.search(r'(?:Theme::instance\(\)|\b\w*[tT]heme\w*)\s*->\s*' + prop + r'\(\)', cpp):
            used.add(key)

    # the layout's metrics: a copy of the tokens in uicore, which must match
    layout_h = read(os.path.join(root, 'src', 'uicore', 'layoutmodel.h'))
    layout_cpp = read(os.path.join(root, 'src', 'uicore', 'layoutmodel.cpp'))
    struct = re.search(r'struct DensityMetrics\s*\{(.*?)\};', layout_h, re.S)
    fields = re.findall(r'\b(\w+)\s*[,;]', re.sub(r'\bdouble\b', '', struct.group(1))) if struct else []
    for d in ('standard', 'minimal'):
        m = re.search(r'DensityMetrics\s+' + d + r'\s*=\s*\{([^}]*)\}', layout_cpp)
        if not m or not fields:
            errors.append(f'layoutmodel: no {d} DensityMetrics to compare')
            continue
        values = [float(v) for v in m.group(1).split(',')]
        for field, value in zip(fields, values):
            if field in tokens[d]:
                used.add(field)
                if float(tokens[d][field]) != value:
                    errors.append(f'layoutmodel {d}.{field} is {value:g}, the token {tokens[d][field]}')

    for key in sorted(keys):
        if key in used and key in PENDING:
            errors.append(f'{key}: read now -- remove it from PENDING in {os.path.basename(__file__)}')
        elif key not in used and key not in PENDING:
            errors.append(f'{key}: no component reads it -- read it through Theme (theme.h), '
                          f'or list it in PENDING with the phase that builds its component')

    pending = sorted(k for k in PENDING if k in keys and k not in used)
    print(f'{len(used)} density tokens read, {len(pending)} pending: {", ".join(pending)}')
    for e in errors:
        print('error: ' + e, file=sys.stderr)
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())
