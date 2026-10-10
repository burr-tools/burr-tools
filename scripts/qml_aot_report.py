#!/usr/bin/env python3
"""How much of burrtools-qt's QML qmlcachegen cannot compile ahead of time.

qmlcachegen compiles each QML file's bindings and functions to C++ where it
knows the types; what it cannot (an untyped `property var`, a lookup on an
unknown type, a call to an untyped JavaScript function) stays as byte code
for the JavaScript engine, slower to run. With --verbose it names each such
place as a "[compiler]" warning.

This runs qmlcachegen --verbose on every QML file of the module, the way the
build does, counts those warnings per file, and compares the total with the
baseline in the repository: more than the baseline fails (the QML lost
types), fewer asks for the baseline to be lowered (--update). The counts
depend on the Qt version, so the baseline records the Qt it was taken with;
another Qt only reports.

usage: qml_aot_report.py --qrc QRC --sources DIR [--import DIR] --baseline JSON
                         [--qmlcachegen EXE] [--update]
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

COMPILER = re.compile(r'\[compiler\]\s*$')


def find_qmlcachegen():
    """the qmlcachegen of the Qt on the PATH: a Qt host tool, in its
    libexec (or, on Windows and MSYS2, its bin) directory"""
    for q in ('qtpaths6', 'qtpaths', 'qmake6', 'qmake'):
        exe = shutil.which(q)
        if not exe:
            continue
        query = [exe, '--query'] if 'qtpaths' in q else [exe, '-query']
        try:
            out = subprocess.run(query, capture_output=True, text=True, check=True).stdout
        except (OSError, subprocess.CalledProcessError):
            continue
        paths = dict(line.split(':', 1) for line in out.splitlines() if ':' in line)
        for key in ('QT_HOST_LIBEXECS', 'QT_HOST_BINS', 'QT_INSTALL_LIBEXECS', 'QT_INSTALL_BINS'):
            for name in ('qmlcachegen', 'qmlcachegen.exe'):
                cand = os.path.join(paths.get(key, ''), name)
                if paths.get(key) and os.path.isfile(cand):
                    return cand
    return shutil.which('qmlcachegen')


def qt_minor(qmlcachegen):
    """'6.11' for a qmlcachegen 6.11.x"""
    r = subprocess.run([qmlcachegen, '--version'], capture_output=True, text=True)
    m = re.search(r'(\d+)\.(\d+)\.\d+', r.stdout + r.stderr)
    return f'{m.group(1)}.{m.group(2)}' if m else 'unknown'


def count(qmlcachegen, qrc, imports, qml, out):
    cmd = [qmlcachegen, '--verbose', '-o', out, '--resource', qrc]
    for i in imports:
        cmd += ['-I', i]
    r = subprocess.run(cmd + [qml], capture_output=True, text=True)
    return sum(1 for line in (r.stdout + r.stderr).splitlines() if COMPILER.search(line))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--qrc', required=True, help="the module's generated _qml.qrc")
    ap.add_argument('--sources', required=True, help='the directory of the QML files')
    ap.add_argument('--import', dest='imports', action='append', default=[],
                    help='an import path, as the build passes qmlcachegen (repeatable)')
    ap.add_argument('--baseline', required=True, help='the JSON file of counts to compare with')
    ap.add_argument('--qmlcachegen')
    ap.add_argument('--update', action='store_true', help='write the counts as the new baseline')
    a = ap.parse_args()

    qmlcachegen = a.qmlcachegen or find_qmlcachegen()
    if not qmlcachegen:
        print('qml_aot_report: no qmlcachegen found (put Qt on the PATH)', file=sys.stderr)
        return 2

    files = sorted(f for f in os.listdir(a.sources) if f.endswith('.qml'))
    counts = {}
    with tempfile.TemporaryDirectory() as tmp:
        out = os.path.join(tmp, 'out.cpp')
        for f in files:
            counts[f] = count(qmlcachegen, a.qrc, a.imports, os.path.join(a.sources, f), out)
    total = sum(counts.values())
    qt = qt_minor(qmlcachegen)

    print(f'Qt {qt}: bindings and functions left to the JavaScript engine, per file:')
    for f, n in sorted(counts.items(), key=lambda kv: (-kv[1], kv[0])):
        if n:
            print(f'  {n:5d}  {f}')
    print(f'  {total:5d}  total')

    if a.update:
        with open(a.baseline, 'w', encoding='utf-8', newline='\n') as fh:
            json.dump({'qt': qt, 'total': total, 'files': counts}, fh, indent=2, sort_keys=True)
            fh.write('\n')
        print(f'baseline written: {a.baseline}')
        return 0

    try:
        with open(a.baseline, encoding='utf-8') as fh:
            base = json.load(fh)
    except OSError:
        print(f'qml_aot_report: no baseline at {a.baseline}; run with --update', file=sys.stderr)
        return 1
    if base.get('qt') != qt:
        print(f'\nthe baseline is for Qt {base.get("qt")}, this is Qt {qt}: reported, not compared')
        return 0
    worse = {f: (n, base['files'].get(f, 0)) for f, n in counts.items() if n > base['files'].get(f, 0)}
    if total > base['total']:
        print(f'\nFAIL: {total} uncompiled, the baseline allows {base["total"]}. Grown:', file=sys.stderr)
        for f, (n, b) in sorted(worse.items()):
            print(f'  {f}: {b} -> {n}', file=sys.stderr)
        print('Type what was added (no `property var`, typed function parameters and returns), '
              'or, if it cannot be helped, update the baseline (just qml-aot --update).', file=sys.stderr)
        return 1
    if total < base['total']:
        print(f'\n{base["total"] - total} fewer than the baseline ({base["total"]}): '
              'lower it with `just qml-aot --update`.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
