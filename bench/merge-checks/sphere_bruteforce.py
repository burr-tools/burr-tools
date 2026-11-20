#!/usr/bin/env python3
"""Count the assemblies of sphere puzzles (grid type 2) by brute force, independently of BurrTools.

Written for the straight-piece check of design/2026-11-19-ng-merge-plan.md (section 5.8, step C4):
BurrTools turns sphere pieces by a fixed list of 120 rotations that is not a group, and for a
straight piece in a result with a non-cubic symmetry its duplicate check may give up. This script
shares no code or tables with BurrTools:

* Balls sit on the face-centred cubic packing, the integer points with an even coordinate sum
  (BurrTools' coordinates). Two balls touch at distance sqrt(2).
* The rotations of a shape are computed from the shape itself, in exact rational arithmetic: take two
  non-parallel difference vectors a, b of the shape, list every packing vector a', b' with |a'| = |a|,
  |b'| = |b| and a'.b' = a.b, take the rotation with a -> a', b -> b', a x b -> a' x b', and keep it
  if every difference vector of the shape lands on a packing vector. A straight shape (balls on one
  line) has infinitely many such rotations but finitely many images: one per image direction.
* Placements: every image of every piece, translated wherever it lies inside the result.
* Assemblies: exact covers of the result's cells with the problem's pieces (fixed counts), as sets
  of (shape, cells).
* Duplicates: the result's full symmetry group (every rotation, computed the same way, that maps the
  result onto itself) and its mirrors (rotations composed with the point reflection). Two assemblies
  are the same when such a map takes one onto the other; under a mirror a chiral piece is replaced by
  its mirror partner shape in the problem, and a mirror that needs a partner the problem lacks (or
  breaks a piece count) relates nothing. This is BurrTools' default duplicate rule (plan 5.4).

Usage:
    python3 -I bench/merge-checks/sphere_bruteforce.py FILE.xmpuzzle [...]
prints one line per problem: file, problem number, assemblies (and the raw number of covers and of
maps in the result's symmetry, mirrors included). examples/BallRoom.xmpuzzle gives 2, 1 and 3, as
burrTxt does. Results 2026-11-20: burrTxt agreed on all 82 puzzles built for the check (flat
hexagons of 7 and 19 balls and triangles of 6 to 21 balls in a (111) layer, and stacks of such
layers, cut into mostly straight pieces); test_symmetries.cpp keeps two of them.

Problems with variable result cells, piece ranges or colour constraints are refused.
"""
import gzip
from math import gcd
import itertools
import sys
import xml.etree.ElementTree as ET
from fractions import Fraction


# --- vectors -----------------------------------------------------------------------------------

def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def on_packing(v):
    return all(isinstance(c, int) or c.denominator == 1 for c in v) and int(sum(v)) % 2 == 0


def packing_vectors(norm2):
    """every packing vector v with |v|^2 = norm2"""
    r = 0
    while r * r <= norm2:
        r += 1
    out = []
    for x in range(-r, r + 1):
        for y in range(-r, r + 1):
            z2 = norm2 - x * x - y * y
            if z2 < 0:
                continue
            z = int(round(z2 ** 0.5))
            for zz in {z, -z}:
                if zz * zz == z2 and (x + y + zz) % 2 == 0:
                    out.append((x, y, zz))
    return out


# --- 3x3 rational matrices ---------------------------------------------------------------------

def mat_cols(a, b, c):
    return [[Fraction(a[i]), Fraction(b[i]), Fraction(c[i])] for i in range(3)]


def mat_inv(m):
    a, b, c = m[0]
    d, e, f = m[1]
    g, h, i = m[2]
    det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g)
    if det == 0:
        return None
    return [[(e * i - f * h) / det, (c * h - b * i) / det, (b * f - c * e) / det],
            [(f * g - d * i) / det, (a * i - c * g) / det, (c * d - a * f) / det],
            [(d * h - e * g) / det, (b * g - a * h) / det, (a * e - b * d) / det]]


def mat_mul(m, n):
    return [[sum(m[i][k] * n[k][j] for k in range(3)) for j in range(3)] for i in range(3)]


def apply(m, v):
    return tuple(m[i][0] * v[0] + m[i][1] * v[1] + m[i][2] * v[2] for i in range(3))


def as_int(v):
    return tuple(int(c) for c in v)


# --- shapes ------------------------------------------------------------------------------------

def is_straight(cells):
    cells = list(cells)
    if len(cells) <= 2:
        return True
    a = sub(cells[1], cells[0])
    return all(cross(a, sub(c, cells[0])) == (0, 0, 0) for c in cells[2:])


def rotations_onto_packing(cells):
    """the proper rotations (as exact matrices) that map every difference vector of a non-straight
    shape onto a packing vector"""
    cells = sorted(cells)
    p0 = cells[0]
    diffs = [sub(c, p0) for c in cells[1:]]
    # a: a shortest difference; b: a shortest one not parallel to it
    diffs_all = sorted({sub(c, d) for c in cells for d in cells if c != d}, key=lambda v: dot(v, v))
    a = diffs_all[0]
    b = next(v for v in diffs_all if cross(a, v) != (0, 0, 0))
    c = cross(a, b)
    inv = mat_inv(mat_cols(a, b, c))
    ab = dot(a, b)
    rots = []
    for a2 in packing_vectors(dot(a, a)):
        for b2 in packing_vectors(dot(b, b)):
            if dot(a2, b2) != ab:
                continue
            r = mat_mul(mat_cols(a2, b2, cross(a2, b2)), inv)
            if all(on_packing(apply(r, d)) for d in diffs):
                rots.append(r)
    return rots


def normalise(cells):
    """the cells moved so that their smallest point is the origin"""
    m = min(cells)
    return frozenset(sub(c, m) for c in cells)


def images(cells, mirrored=False):
    """the distinct images (normalised cell sets) of a shape under every rotation that keeps it on
    the packing; mirrored: of its mirror image"""
    cells = [(-c[0], -c[1], -c[2]) for c in cells] if mirrored else list(cells)
    if len(cells) == 1:
        return {frozenset([(0, 0, 0)])}
    if is_straight(cells):
        # the balls are p0 + k_i * s for the primitive integer step s of the line; an image puts
        # them at k_i * s' for a packing vector s' of the same length (all multiples of s' are on
        # the packing then, and the k_i * s are differences of the shape so they must be)
        p0 = min(cells)
        d = sub(max(cells), p0)
        g = gcd(gcd(abs(d[0]), abs(d[1])), abs(d[2]))
        s = (d[0] // g, d[1] // g, d[2] // g)
        ks = [dot(sub(c, p0), s) // dot(s, s) for c in cells]
        # the differences k*s must be packing vectors; with s itself off the packing (odd sum) every
        # used k is even, so step by 2s
        if (s[0] + s[1] + s[2]) % 2:
            assert all(k % 2 == 0 for k in ks)
            s = (2 * s[0], 2 * s[1], 2 * s[2])
            ks = [k // 2 for k in ks]
        out = set()
        for s2 in packing_vectors(dot(s, s)):
            out.add(normalise([(k * s2[0], k * s2[1], k * s2[2]) for k in ks]))
        return out
    p0 = min(cells)
    out = set()
    for r in rotations_onto_packing(cells):
        out.add(normalise([as_int(apply(r, sub(c, p0))) for c in cells]))
    return out


def self_maps(cells):
    """the maps (matrix, translation, mirrored) of the packing that take the shape onto itself:
    rotations, and rotations after the point reflection"""
    cells = list(cells)
    assert not is_straight(cells), "a straight result has infinitely many symmetries"
    target = frozenset(cells)
    p0 = min(cells)
    out = []
    for mirrored in (False, True):
        src = [(-c[0], -c[1], -c[2]) for c in cells] if mirrored else cells
        q0 = min(src)
        for r in rotations_onto_packing(src):
            moved = [as_int(apply(r, sub(c, q0))) for c in src]
            # the translation that puts the image onto the result
            t = sub(min(target), min(moved))
            if frozenset(add(m, t) for m in moved) == target:
                out.append((r, q0, t, mirrored))
    return out


def map_cells(m, cells):
    r, q0, t, mirrored = m
    out = []
    for c in cells:
        if mirrored:
            c = (-c[0], -c[1], -c[2])
        out.append(add(as_int(apply(r, sub(c, q0))), t))
    return frozenset(out)


def integer_map(m):
    """a map of self_maps as a fast function on cells: the matrix times its common denominator,
    divided out exactly"""
    r, q0, t, mirrored = m
    den = 1
    for row in r:
        for e in row:
            den = den * e.denominator // gcd(den, e.denominator)
    ri = [[int(e * den) for e in row] for row in r]
    sgn = -1 if mirrored else 1

    def f(cells):
        out = []
        for c in cells:
            d = (sgn * c[0] - q0[0], sgn * c[1] - q0[1], sgn * c[2] - q0[2])
            v = []
            for i in range(3):
                n = ri[i][0] * d[0] + ri[i][1] * d[1] + ri[i][2] * d[2]
                assert n % den == 0
                v.append(n // den + t[i])
            out.append(tuple(v))
        return frozenset(out)
    return f


# --- files -------------------------------------------------------------------------------------

def parse_voxel(el):
    x, y, z = int(el.get('x')), int(el.get('y')), int(el.get('z'))
    text = el.text or ''
    cells, variable = [], []
    i, pos = 0, 0
    while i < len(text):
        ch = text[i]
        i += 1
        while i < len(text) and text[i].isdigit():
            i += 1
        if ch == '#':
            cells.append((pos % x, (pos // x) % y, pos // (x * y)))
        elif ch == '+':
            variable.append(pos)
        pos += 1
    assert pos == x * y * z, 'voxel string length'
    return cells, variable


def load(path):
    with open(path, 'rb') as f:
        data = f.read()
    if data[:2] == b'\x1f\x8b':
        data = gzip.decompress(data)
    root = ET.fromstring(data)
    assert root.find('gridType').get('type') == '2', 'not a sphere puzzle'
    shapes = [parse_voxel(v) for v in root.find('shapes').findall('voxel')]
    problems = []
    for p in root.find('problems').findall('problem'):
        pieces = []
        for s in p.find('shapes').findall('shape'):
            if s.get('count') is not None:
                n = int(s.get('count'))
            else:
                n = int(s.get('min'))
                assert n == int(s.get('max')), 'piece ranges are not supported'
            pieces.append((int(s.get('id')), n))
        assert p.find('bitmap') is None or len(p.find('bitmap')) == 0, 'colour constraints are not supported'
        problems.append((int(p.find('result').get('id')), pieces))
    return shapes, problems


# --- counting ----------------------------------------------------------------------------------

def count(shapes, result_id, pieces):
    res_cells, res_var = shapes[result_id]
    assert not res_var, 'variable result cells are not supported'
    result = frozenset(res_cells)
    assert sum(len(shapes[s][0]) * n for s, n in pieces) == len(result), 'pieces do not fill the result'

    ids = [s for s, _ in pieces]
    counts = {s: n for s, n in pieces}

    # placements per shape
    placements = {}
    for s in ids:
        pl = set()
        for img in images(shapes[s][0]):
            for anchor in result:
                t = sub(anchor, min(img))
                moved = frozenset(add(c, t) for c in img)
                if moved <= result:
                    pl.add(moved)
        placements[s] = pl

    by_cell = {c: [] for c in result}
    for s in ids:
        for pl in placements[s]:
            for c in pl:
                by_cell[c].append((s, pl))

    order = sorted(result)
    found = []

    def search(covered, left, chosen):
        if len(covered) == len(result):
            found.append(frozenset(chosen))
            return
        cell = next(c for c in order if c not in covered)
        for s, pl in by_cell[cell]:
            if left[s] and not (pl & covered):
                left[s] -= 1
                chosen.append((s, pl))
                search(covered | pl, left, chosen)
                chosen.pop()
                left[s] += 1

    search(frozenset(), dict(counts), [])

    # mirror partners: a chiral shape becomes its partner under a mirror
    def same_shape(a, b):
        return normalise(b) in images(a)

    partner = {}
    for s in ids:
        mir = [(-c[0], -c[1], -c[2]) for c in shapes[s][0]]
        partner[s] = None
        for s2 in ids:
            if len(shapes[s2][0]) == len(mir) and same_shape(shapes[s2][0], mir):
                partner[s] = s2
                break

    maps = [(m[3], integer_map(m)) for m in self_maps(result)]
    for m in self_maps(result):  # the integer form agrees with the exact one
        assert integer_map(m)(result) == map_cells(m, result) == result

    want = {s: n for s, n in counts.items() if n}

    def transformed(asm, mirrored, f):
        out = []
        used = {}
        for s, pl in asm:
            s2 = s
            if mirrored:
                s2 = partner[s]
                if s2 is None:
                    return None
            used[s2] = used.get(s2, 0) + 1
            out.append((s2, f(pl)))
        if used != want:
            return None
        return frozenset(out)

    # orbits: count an assembly not seen yet, then mark its whole orbit
    found_set = set(found)
    seen = set()
    orbits = 0
    for asm in found:
        if asm in seen:
            continue
        orbits += 1
        for mirrored, f in maps:
            t = transformed(asm, mirrored, f)
            if t is not None:
                assert t in found_set, 'a symmetry of the result took a cover to a non-cover'
                seen.add(t)
    return orbits, len(found), len(maps)


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    for path in argv[1:]:
        shapes, problems = load(path)
        for i, (res, pieces) in enumerate(problems):
            n, raw, syms = count(shapes, res, pieces)
            print(f'{path} problem {i}: {n} assemblies ({raw} covers, result symmetry {syms} maps)')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
