#!/usr/bin/env python3
"""An importable copy of a QML module's types, for qmlcachegen.

meson's qt.qml_module writes the module's qmldir and .qmltypes flat in the
build directory, not in a tree an `import BurrTools.Ui` can find, so
qmlcachegen compiles the module's QML without knowing its C++ types (App,
Theme, the controllers): "Failed to import BurrTools.Ui", and the bindings
that use them are left to the JavaScript engine. This lays the module out
as an import path expects --

    <root>/BurrTools/Ui/qmldir
    <root>/BurrTools/Ui/<typeinfo>.qmltypes

-- and qmlcachegen is then run with `-I <root>`. The qmldir is the module's,
its QML components pointed at their sources (relative paths) and without
its `prefer` line (the resource path, which a tool cannot read). The QML
files are not copied: then an edit to one QML file changes nothing here,
and only that file is compiled again. A file is rewritten only when its
content changed. The stamp file is the build step's output.

usage: qml_import_tree.py --root DIR --module URI --qmldir FILE
                          --qmltypes FILE --sources DIR --stamp FILE
"""
import argparse
import os
import re
import sys

COMPONENT = re.compile(r'^(\s*(?:singleton\s+|internal\s+)?[A-Z]\w*\s+(?:\d+\.\d+\s+)?)(\S+\.qml)\s*$')


def put(path, data):
    try:
        with open(path, 'rb') as f:
            if f.read() == data:
                return
    except OSError:
        pass
    with open(path, 'wb') as f:
        f.write(data)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', required=True)
    ap.add_argument('--module', required=True)
    ap.add_argument('--qmldir', required=True)
    ap.add_argument('--qmltypes', required=True)
    ap.add_argument('--sources', required=True, help='the directory of the QML files')
    ap.add_argument('--stamp', required=True)
    a = ap.parse_args()

    out = os.path.join(a.root, *a.module.split('.'))
    os.makedirs(out, exist_ok=True)
    rel = os.path.relpath(a.sources, out).replace(os.sep, '/')

    lines = []
    with open(a.qmldir, encoding='utf-8') as f:
        for line in f.read().splitlines():
            if line.startswith('prefer '):
                continue
            m = COMPONENT.match(line)
            lines.append(f'{m.group(1)}{rel}/{m.group(2)}' if m else line)
    put(os.path.join(out, 'qmldir'), ('\n'.join(lines) + '\n').encode('utf-8'))
    with open(a.qmltypes, 'rb') as f:
        put(os.path.join(out, os.path.basename(a.qmltypes)), f.read())

    with open(a.stamp, 'w', encoding='utf-8') as f:
        f.write(out + '\n')
    return 0


if __name__ == '__main__':
    sys.exit(main())
