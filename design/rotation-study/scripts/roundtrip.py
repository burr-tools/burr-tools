#!/usr/bin/env python3
"""The round trip of rotation moves (design/2026-11-11-rotation-consistent-solver.md, step 4).

Solve a puzzle at a model, measure the play (at the model's bevel) and the bevel
(at the model's play) each disassembly needs (burrTxt -P), then solve again at
the measured play and at the measured bevel: the puzzle must come apart again.
A puzzle that does not is a finding, reported with the measured values.

  roundtrip.py [--burrtxt PATH] [--timeout S] [--out DIR] jobs.txt [-- burrTxt options]

jobs.txt: one "file play,bevel" per line (a file name with spaces: the model is
the last field). The burrTxt options follow -R, so they start with its mode
(default: balanced); they are used for all three solves.
Prints one line per puzzle, "|"-separated, and a summary.
"""

import argparse
import gzip
import os
import re
import subprocess
import sys
import time


def solve(burrtxt, puzzle, model, options, timeout, write=None):
    cmd = [burrtxt, '-d', '-R', *options, '-q', '-m', model, puzzle]
    if write:
        cmd[1:1] = ['-P', '-w', write]
    start = time.time()
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout).stdout
    except subprocess.TimeoutExpired:
        return None, timeout
    m = re.findall(r'(\d+) assemblies and (\d+) solutions', out)
    return (int(m[-1][1]) if m else None), time.time() - start


def measured(path):
    """per solution: (playUpper, bevelUpper or None) from a file burrTxt -P -w wrote"""
    with gzip.open(path, 'rt') as f:
        text = f.read()
    result = []
    for tag in re.findall(r'<solution [^>]*>', text):
        up = re.search(r'playUpper="([^"]+)"', tag)
        bu = re.search(r'bevelUpper="([^"]+)"', tag)
        if up:
            result.append((float(up.group(1)), float(bu.group(1)) if bu else None))
    return result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--burrtxt', default='build-rel/burrTxt')
    ap.add_argument('--timeout', type=float, default=300)
    ap.add_argument('--out', default='/tmp/roundtrip')
    ap.add_argument('jobs')
    ap.add_argument('options', nargs='*')
    a = ap.parse_args()
    options = a.options or ['balanced']
    os.makedirs(a.out, exist_ok=True)

    rows = []
    for line in open(a.jobs):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        puzzle, model = line.rsplit(' ', 1)
        play, bevel = (float(x) for x in model.split(','))
        name = os.path.splitext(os.path.basename(puzzle))[0]
        written = os.path.join(a.out, name.replace(' ', '_') + '.xmpuzzle')
        sols, t = solve(a.burrtxt, puzzle, model, options, a.timeout, written)
        row = {'name': name, 'model': model, 'solutions': sols, 't': t}
        if sols:
            m = measured(written)
            if m:
                # every solution's disassembly works at its own upper value: the
                # puzzle must come apart there; the most demanding is the smallest
                up = min(u for u, _ in m)
                row['playUpper'] = up
                row['atPlay'], _ = solve(a.burrtxt, puzzle, f'{up!r},{bevel!r}', options, a.timeout)
                bevels = [b for _, b in m if b is not None]
                if bevels:
                    bu = min(bevels)
                    row['bevelUpper'] = bu
                    row['atBevel'], _ = solve(a.burrtxt, puzzle, f'{play!r},{bu!r}', options, a.timeout)
        rows.append(row)
        print('|'.join(f'{k}={v:.5g}' if isinstance(v, float) else f'{k}={v}' for k, v in row.items()), flush=True)

    measuredRows = [r for r in rows if 'playUpper' in r]
    failPlay = [r['name'] for r in measuredRows if not r.get('atPlay')]
    failBevel = [r['name'] for r in measuredRows if 'bevelUpper' in r and not r.get('atBevel')]
    print(f'# {len(rows)} puzzles, {len(measuredRows)} apart and measured; '
          f'not apart again at the measured play: {failPlay or "none"}; at the measured bevel: {failBevel or "none"}')
    return 1 if failPlay or failBevel else 0


if __name__ == '__main__':
    sys.exit(main())
