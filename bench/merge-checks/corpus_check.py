#!/usr/bin/env python3
"""Compare burrTxt's assembly counts over the whole puzzle corpus under different solver settings.

Each solver step of design/2026-11-19-ng-merge-plan.md must leave the results unchanged. A mode
runs every problem of examples/ and puzzles/BTFiles/ once per variant (a set of BURRTOOLS_ variables)
and reports problems whose variants disagree. Run from the repository root with a release binary:

    just build-release
    python3 bench/merge-checks/corpus_check.py engines
    python3 bench/merge-checks/corpus_check.py engine-choice --timeout 30 --out out/choice.txt

Modes:
  engines        the default / Dancing Cells forced / the fallback path (bit-parallel switched off
                 through its memory budget), on every problem
  symbreak       the range symmetry breaker off / on, on problems with piece ranges
  merge          one engine column per voxel / identical voxel columns merged (BURRTOOLS_MERGE_COLUMNS),
                 on every problem
  described      the native grid (burrTxt --native-grids) / the puzzle converted to its described grid
                 (burrTxt -D, NG's grids as data, step G3 of design/2026-11-19-ng-merge-plan.md), on
                 bricks, triangular prisms, rhombic and tetra-octa problems. Triangular prisms have no
                 native grid since C3 (every reader converts them), so for them both sides are NG's. burrTxt reads rhombic and
                 tetra-octa onto NG's grids by default (step C1), so only --native-grids solves them
                 natively; there NG's rhombic grid may find more assemblies (pieces half a cube
                 diagonal away), which shows as MISMATCH and has to be looked at
  described-solutions  the same with disassembly (-d), on bricks and prisms: assemblies and solutions
                 must agree (step G4)
  engine-choice-mt  the same at the default thread count (step A1.2: does Dancing Cells scale
                 better than the bit-parallel engine?)
  engine-choice  the bit-parallel engine / Dancing Cells, one thread, on problems assembler 1 takes;
                 each line also has the problem's assemblies and the shapes, pieces and ranges
                 (step 1.9: what makes Dancing Cells the faster engine)

A line per problem: OK (all variants finished and agree), TIMEOUT (some variant over the limit; the
ones that finished agree) or MISMATCH. Exit status 1 if any problem mismatches. --resume FILE skips
the problems already listed in FILE (an earlier output), for runs that were interrupted.
"""
import argparse, glob, gzip, os, re, subprocess, sys, time

MODES = {
    'engines': [('default', {}),
                ('cells', {'BURRTOOLS_BITPARALLEL': '0'}),
                ('fallback', {'BURRTOOLS_HUANG_MEM_MB': '0'})],
    'merge': [('off', {'BURRTOOLS_MERGE_COLUMNS': '0'}),
              ('on', {'BURRTOOLS_MERGE_COLUMNS': '1'})],
    'symbreak': [('off', {'BURRTOOLS_RANGE_SYMBREAK': '0'}),
                 ('on', {'BURRTOOLS_RANGE_SYMBREAK': '1'})],
    'described': [('native', {'--native-grids': ''}), ('described', {'-D': ''})],
    'described-solutions': [('native', {'--native-grids': '', '-d': ''}), ('described', {'-D': '', '-d': ''})],
    'engine-choice': [('bitparallel', {'BURRTOOLS_THREADS': '1'}),
                      ('cells', {'BURRTOOLS_BITPARALLEL': '0', 'BURRTOOLS_THREADS': '1'})],
    'engine-choice-mt': [('bitparallel', {}),
                         ('cells', {'BURRTOOLS_BITPARALLEL': '0'})],
}

def gridtype(path):
    try:
        data = gzip.open(path, 'rt', errors='replace').read()
    except OSError:
        data = open(path, errors='replace').read()
    m = re.search(r'<gridType type="(\d+)"', data)
    return m.group(1) if m else '?'

def features(text):
    """shapes, pieces (sum of the largest counts) and whether any shape has a range, from a <problem>"""
    shapes = pieces = 0
    ranges = False
    for m in re.finditer(r'<shape\b([^>]*)', text):
        a = m.group(1)
        shapes += 1
        mx = re.search(r'\bmax="(\d+)"', a) or re.search(r'\bcount="(\d+)"', a)
        mn = re.search(r'\bmin="(\d+)"', a)
        pieces += int(mx.group(1)) if mx else 1
        if mn and mx and mn.group(1) != mx.group(1):
            ranges = True
    return f'shapes={shapes} pieces={pieces} ranges={int(ranges)}'


def problems(path):
    try:
        data = gzip.open(path, 'rt', errors='replace').read()
    except OSError:
        data = open(path, errors='replace').read()
    return re.findall(r'<problem\b.*?</problem>', data, re.S)

def run(binary, path, idx, env, timeout):
    # keys starting with '-' are burrTxt options, the rest environment variables
    e = dict(os.environ)
    e.update({k: v for k, v in env.items() if not k.startswith('-')})
    opts = [k for k in env if k.startswith('-')]
    start = time.time()
    try:
        r = subprocess.run([binary, '-n'] + opts + ['-o', str(idx), path], capture_output=True, timeout=timeout, env=e)
    except subprocess.TimeoutExpired:
        return None, None, timeout
    out = (r.stdout + r.stderr).decode('utf-8', 'replace')
    count = re.search(r'(\d+) assemblies(?: and (\d+) solutions)?', out)
    assembler = re.search(r'using assembler (\d)', out)
    if not count:
        value = -1
    elif '-d' in env and count.group(2) is not None:
        value = f'{count.group(1)}/{count.group(2)}'
    else:
        value = int(count.group(1))
    return value, (assembler.group(1) if assembler else '?'), time.time() - start

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('mode', choices=sorted(MODES))
    ap.add_argument('--burrtxt', default='build-rel/burrTxt')
    ap.add_argument('--timeout', type=float, default=15)
    ap.add_argument('--resume', help='an earlier output; its problems are skipped')
    ap.add_argument('--out', help='also write the lines to this file')
    args = ap.parse_args()

    done = set()
    if args.resume:
        with open(args.resume, errors='replace') as f:
            for line in f:
                m = re.match(r'\w+\s+(.*?:\d+) ', line)
                if m:
                    done.add(m.group(1))
    out = open(args.out, 'a') if args.out else None
    def emit(line):
        print(line, flush=True)
        if out:
            out.write(line + '\n')
            out.flush()

    variants = MODES[args.mode]
    files = sorted(glob.glob('examples/*.xmpuzzle') + glob.glob('puzzles/BTFiles/**/*.xmpuzzle', recursive=True))
    checked = bad = 0
    totals = [0.0] * len(variants)
    for path in files:
        for idx, text in enumerate(problems(path)):
            name = f'{path}:{idx}'
            if name in done:
                continue
            if args.mode == 'symbreak' and not re.search(r'<shape[^>]*\bmin=', text):
                continue
            if args.mode == 'described' and gridtype(path) not in ('0', '1', '3', '4'):
                continue
            # natively only bricks and prisms disassemble; rhombic and tetra-octa
            # go to NG instead (bench/ng-crosscheck/crosscheck.sh --described)
            if args.mode == 'described-solutions' and gridtype(path) not in ('0', '1'):
                continue
            results = []
            for k, (label, env) in enumerate(variants):
                count, assembler, secs = run(args.burrtxt, path, idx, env, args.timeout)
                results.append((label, count, secs))
                if args.mode.startswith('engine-choice') and k == 0 and assembler != '1':
                    break
            else:
                counts = [c for _, c, _ in results if c is not None]
                state = 'MISMATCH' if len(set(counts)) > 1 else ('OK' if len(counts) == len(results) else 'TIMEOUT')
                checked += 1
                bad += state == 'MISMATCH'
                if state == 'OK':
                    for k, (_, _, secs) in enumerate(results):
                        totals[k] += secs
                extra = ' ' + features(text) if args.mode.startswith('engine-choice') else ''
                emit(f'{state:8} {name} ' + ' '.join(f'{label}={count} ({secs:.2f}s)' for label, count, secs in results) + extra)
    emit(f'problems={checked} mismatches={bad} ' +
         ' '.join(f'total_{label}={t:.1f}s' for (label, _), t in zip(variants, totals)))
    if out:
        out.close()
    return 1 if bad else 0

if __name__ == '__main__':
    sys.exit(main())
