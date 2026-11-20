#!/usr/bin/env python3
"""Generate a polyrhombic-dodecahedron puzzle on the native rhombic grid (type 3).

Rhombic dodecahedra sit at cube centres (i, j, k) with i+j+k even (an fcc packing).
A dodecahedron is its cube's 24 voxels plus, from each of the 6 face neighbours,
the 4 voxels of that neighbour's pyramid towards it. Native voxel residues per
cube are 0..4 per axis; a voxel is valid when its folded residues (r > 2 -> 4 - r)
are a permutation of 0, 1, 2. The folded 0 axis is the face the voxel lies at:
residue 0 the low face, 4 the high face.

The result is the dodecahedra at the fcc sites of a box (or a ball), the pieces a
random partition of them into connected groups of SIZE (12 fcc neighbours).

    gen_rhombic_puzzle.py SEED N SIZE OUT [box | box:X,Y,Z | ball]

bench/RhombicDodecahedraBox.xmpuzzle, the rhombic benchmark of step C1 of
design/2026-11-19-ng-merge-plan.md, is

    gen_rhombic_puzzle.py 34 0 4 out.xmpuzzle box:6,4,3   (then gzip -n -9, as the examples are)
"""
import random, sys, itertools

def valid(a, b, c):
    f = sorted((r if r <= 2 else 4 - r) for r in (a, b, c))
    return f == [0, 1, 2]

CUBE = [(a, b, c) for a in range(5) for b in range(5) for c in range(5) if valid(a, b, c)]
assert len(CUBE) == 24

def rd_voxels(i, j, k):
    out = set()
    for (a, b, c) in CUBE:
        out.add((5*i + a, 5*j + b, 5*k + c))
    for axis in range(3):
        for s in (-1, 1):
            n = [i, j, k]
            n[axis] += s
            # the neighbour's voxels at its face towards us: residue 4 if it is below (s=-1), 0 if above
            want = 4 if s < 0 else 0
            for (a, b, c) in CUBE:
                r = (a, b, c)
                if r[axis] == want:
                    out.add((5*n[0] + a, 5*n[1] + b, 5*n[2] + c))
    return out

NB = [v for v in itertools.product((-1, 0, 1), repeat=3) if sorted(map(abs, v)) == [0, 1, 1]]

def voxel_xml(cells, filled_char='#'):
    xs = [c[0] for c in cells]; ys = [c[1] for c in cells]; zs = [c[2] for c in cells]
    # keep the native raster: shift by multiples of 5
    ox = (min(xs) // 5) * 5; oy = (min(ys) // 5) * 5; oz = (min(zs) // 5) * 5
    sx = ((max(xs) - ox) // 5 + 1) * 5; sy = ((max(ys) - oy) // 5 + 1) * 5; sz = ((max(zs) - oz) // 5 + 1) * 5
    s = set((x - ox, y - oy, z - oz) for x, y, z in cells)
    chars = []
    for z in range(sz):
        for y in range(sy):
            for x in range(sx):
                chars.append(filled_char if (x, y, z) in s else '_')
    return f'    <voxel x="{sx}" y="{sy}" z="{sz}" type="0">{"".join(chars)}</voxel>\n'

def partition(sites, size, rng):
    """random partition of the sites into connected pieces of `size` (retry until it works)"""
    for attempt in range(10000):
        left = set(sites)
        pieces = []
        ok = True
        while left:
            # start at the site with the fewest free neighbours
            start = min(left, key=lambda s: (sum((s[0]+d[0], s[1]+d[1], s[2]+d[2]) in left for d in NB), rng.random()))
            piece = [start]; left.remove(start)
            while len(piece) < size:
                cand = [(p[0]+d[0], p[1]+d[1], p[2]+d[2]) for p in piece for d in NB]
                cand = [c for c in cand if c in left]
                if not cand:
                    break
                c = rng.choice(cand)
                piece.append(c); left.remove(c)
            if len(piece) < size and left:
                ok = False
                break
            pieces.append(piece)
        if ok:
            return pieces
    raise SystemExit('no partition')

def main():
    seed = int(sys.argv[1]); n = int(sys.argv[2]); size = int(sys.argv[3]); out = sys.argv[4]
    shape = sys.argv[5] if len(sys.argv) > 5 else 'box'
    rng = random.Random(seed)
    if shape.startswith('box'):
        dims = [int(x) for x in shape[4:].split(',')] if ':' in shape else [n, n, n]
        sites = [(i, j, k) for i in range(dims[0]) for j in range(dims[1]) for k in range(dims[2]) if (i + j + k) % 2 == 0]
    else:
        # an octahedral-ish ball of fcc sites
        sites = [(i, j, k) for i in range(-n, n+1) for j in range(-n, n+1) for k in range(-n, n+1)
                 if (i + j + k) % 2 == 0 and abs(i) + abs(j) + abs(k) <= n]
        sites = [(i + n + 1, j + n + 1, k + n + 1) for i, j, k in sites]
    sites = [(i + 1, j + 1, k + 1) for i, j, k in sites]
    pieces = partition(sites, size, rng)
    result = set()
    for s in sites:
        result |= rd_voxels(*s)
    xml = ['<?xml version="1.0"?>\n<puzzle version="2">\n  <gridType type="3"/>\n  <colors/>\n  <shapes>\n']
    xml.append(voxel_xml(result))
    for p in pieces:
        cells = set()
        for s in p:
            cells |= rd_voxels(*s)
        xml.append(voxel_xml(cells))
    xml.append('  </shapes>\n  <problems>\n    <problem name="Fill">\n      <shapes>\n')
    for i in range(len(pieces)):
        xml.append(f'        <shape id="{i+1}" count="1"/>\n')
    xml.append('      </shapes>\n      <result id="0"/>\n      <bitmap/>\n    </problem>\n  </problems>\n')
    xml.append(f'  <comment>Polyrhombic dodecahedra: {len(sites)} rhombic dodecahedra at fcc sites, '
               f'cut into {len(pieces)} pieces of {size}. Generated by bench/gen_rhombic_puzzle.py (seed {seed}) as a benchmark for the '
               f'rhombic grid (step C1 of design/2026-11-19-ng-merge-plan.md).</comment>\n</puzzle>\n')
    open(out, 'w').write(''.join(xml))
    print(f'sites={len(sites)} pieces={len(pieces)} result voxels={len(result)}')

main()
