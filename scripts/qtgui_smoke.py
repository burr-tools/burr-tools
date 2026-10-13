#!/usr/bin/env python3
"""Runs the real burrtools-qt the ways CI and the documentation do.

The Qt tests drive the controllers and the QML in their own programs; this
starts the application itself, headless (QT_QPA_PLATFORM=offscreen), on a
scratch settings file, and checks each run exits cleanly and writes what it
was asked to:

  --self-check                  the invariant check, before any window
  <puzzle> --screenshot=<png>   a puzzle opened and the window grabbed
  --gallery --screenshot=<png>  the component gallery instead of the window
  --command=<key>               a command run once the window is up
  <missing file>                a file that does not load still starts
  BURRTOOLS_RHI=<unknown>       an unknown graphics API is named and ignored
  -qmljsdebugger=<...>          qmlprofiler's argument is not a puzzle

usage: qtgui_smoke.py BURRTOOLS_QT_EXE
"""
import os
import subprocess
import sys
import tempfile

# absolute: Windows does not start a relative path written with '/'
EXE = os.path.abspath(sys.argv[1])
failures = []


def run(name, args, env=None, expect_png=None, expect_stderr=None):
    with tempfile.TemporaryDirectory() as tmp:
        e = dict(os.environ)
        e.update({
            'QT_QPA_PLATFORM': 'offscreen',
            'QSG_RHI_DISABLE_DISK_CACHE': '1',
            'BURRTOOLS_QT_SETTINGS': os.path.join(tmp, 'settings.ini'),
        })
        e.update(env or {})
        png = os.path.join(tmp, 'shot.png')
        args = [a.replace('{png}', png) for a in args]
        try:
            p = subprocess.run([EXE] + args, env=e, capture_output=True, text=True, timeout=60)
        except subprocess.TimeoutExpired:
            failures.append(f'{name}: did not exit within 60 s')
            return
        problems = []
        if p.returncode != 0:
            problems.append(f'exit code {p.returncode}')
        if expect_png and not (os.path.isfile(png) and os.path.getsize(png) > 1000):
            problems.append('no screenshot written')
        if expect_stderr and expect_stderr not in p.stderr:
            problems.append(f'stderr does not say "{expect_stderr}"')
        if problems:
            failures.append(f'{name}: {", ".join(problems)}\n--- stdout\n{p.stdout}--- stderr\n{p.stderr}')
        print(f'{"FAIL" if problems else "ok  "}  {name}')


puzzle = os.path.join('examples', 'PelikanBurr.xmpuzzle')

run('self-check', ['--self-check'])
run('a puzzle, grabbed', [puzzle, '--screenshot={png}'], expect_png=True)
run('the gallery, grabbed', ['--gallery', '--screenshot={png}'], expect_png=True)
run('a command once the window is up', [puzzle, '--command=export.stl', '--screenshot={png}'], expect_png=True)
run('a file that does not load', ['no-such-puzzle.xmpuzzle', '--screenshot={png}'], expect_png=True)
run('an unknown graphics API', ['--screenshot={png}'], env={'BURRTOOLS_RHI': 'bogus'},
    expect_png=True, expect_stderr='BURRTOOLS_RHI=bogus is not one of')
run('qmlprofiler\'s argument', ['-qmljsdebugger=port:3768', puzzle, '--screenshot={png}'], expect_png=True)

for f in failures:
    print(f, file=sys.stderr)
sys.exit(1 if failures else 0)
