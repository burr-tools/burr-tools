#!/usr/bin/env python3
"""Convert puzzlecad burr pieces (OpenSCAD) into a BurrTools puzzle.

puzzlecad (https://github.com/aaron-siegel/puzzlecad) describes burr pieces
as strings in burr_plate([...]) calls: every piece is a list of layers
(bottom to top), "|" starts the next row, "x" is a cube, "." is empty, and
"{...}" after a cell holds its options. Pieces that would print with
overhangs are split into parts joined by connectors:
"{connect=mz+y+,clabel=A}" puts a male connector on the cell's z+ face,
"{connect=fz-y+,clabel=A}" a female one on its z- face; the second direction
orients the connector. Gluing a female part to a male one turns the female
face against the male face, orientations aligned.

The labels are only printed letters, puzzlecad does not pair connectors
itself, and one label can occur several times. So this script pairs male and
female connectors of the same label across the whole file such that every
glued piece is consistent and has no overlapping cubes, as many as possible;
connectors that pair with nothing are reported. A plate entry that consists
of several separate groups of cubes is split into them first, since such
groups are separate parts (and may be glued to each other).

The .scad file does not describe the target shape. Give it with --result
(the result shape of an existing BurrTools file) or --cube N (an N x N x N
box, every cell optional when the pieces do not fill it); without either the
file holds just the pieces, and the target is added in BurrTools.

Examples:
  puzzlecad2xmpuzzle.py HypnoTIC.scad --cube 4 -o HypnoTIC.xmpuzzle
  puzzlecad2xmpuzzle.py HypnoTIC.scad --result HypnoTIC_ref.xmpuzzle -o HypnoTIC.xmpuzzle
  puzzlecad2xmpuzzle.py HypnoTIC.scad --check HypnoTIC_ref.xmpuzzle

The physical model of the printed pieces ($burr_scale, $burr_inset,
$burr_bevel) is printed as burrTxt's -m option for rotation moves.
"""
import argparse
import gzip
import itertools
import json
import math
import re
import sys
import xml.etree.ElementTree as ET
from xml.sax.saxutils import escape

AXES = {"x": (1, 0, 0), "y": (0, 1, 0), "z": (0, 0, 1)}
IDENTITY = (((1, 0, 0), (0, 1, 0), (0, 0, 1)), (0, 0, 0))


# --- parsing ---------------------------------------------------------------

def direction(text):
    """'z+' -> (0, 0, 1)"""
    if len(text) != 2 or text[0] not in AXES or text[1] not in "+-":
        raise ValueError(f"bad direction {text!r}")
    return tuple(c if text[1] == "+" else -c for c in AXES[text[0]])


def split_top(text, sep=","):
    """split at separators outside braces"""
    parts, cur, depth = [], "", 0
    for ch in text:
        if ch == sep and depth == 0:
            parts.append(cur)
            cur = ""
            continue
        depth += (ch == "{") - (ch == "}")
        cur += ch
    parts.append(cur)
    return parts


def parse_options(text, i):
    """text[i] is '{': (options dict, index after the matching '}')"""
    depth, j = 0, i
    while True:
        depth += (text[j] == "{") - (text[j] == "}")
        if depth == 0:
            break
        j += 1
    options = {}
    for part in split_top(text[i + 1:j]):
        if "=" in part:
            key, value = part.split("=", 1)
            value = value.strip()
            options[key.strip()] = [v.strip() for v in value[1:-1].split(",")] if value.startswith("{") else value
    return options, j + 1


def parse_piece(layers):
    """layer strings -> (cells, connectors); rows run along +y, layers along +z"""
    cells, connectors = set(), []
    for z, layer in enumerate(layers):
        x = y = i = 0
        last = None
        while i < len(layer):
            ch = layer[i]
            if ch == "|":
                x, y, i = 0, y + 1, i + 1
            elif ch == "{":
                options, i = parse_options(layer, i)
                if "connect" in options:
                    kinds = options["connect"] if isinstance(options["connect"], list) else [options["connect"]]
                    labels = options.get("clabel", [None] * len(kinds))
                    labels = labels if isinstance(labels, list) else [labels]
                    for spec, label in zip(kinds, labels):
                        if spec[0] not in "mf" or len(spec) != 5:
                            raise ValueError(f"unsupported connector {spec!r}")
                        connectors.append({"cell": last, "kind": spec[0], "normal": direction(spec[1:3]),
                                           "orient": direction(spec[3:5]), "label": label or ""})
            elif ch in "x.":
                last = (x, y, z)
                if ch == "x":
                    cells.add(last)
                x, i = x + 1, i + 1
            else:
                raise ValueError(f"unsupported cell {ch!r} in {layer!r}")
    return cells, connectors


def components(cells, connectors):
    """split into face-connected parts, each with its connectors"""
    left, out = set(cells), []
    while left:
        stack = [left.pop()]
        part = set(stack)
        while stack:
            x, y, z = stack.pop()
            for d in ((1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1)):
                n = (x + d[0], y + d[1], z + d[2])
                if n in left:
                    left.remove(n)
                    part.add(n)
                    stack.append(n)
        out.append((part, [c for c in connectors if c["cell"] in part]))
    return out


def plates(text):
    """(module name, list of pieces as layer lists) for every burr_plate call"""
    text = re.sub(r"//[^\n]*", "", text)
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    found = []
    for m in re.finditer(r"burr_plate\s*\(", text):
        module = re.findall(r"module\s+(\w+)", text[:m.start()])
        i = text.index("[", m.end())
        depth, j, in_string = 0, i, False
        while True:
            ch = text[j]
            if ch == '"':
                in_string = not in_string
            elif not in_string:
                depth += (ch == "[") - (ch == "]")
                if depth == 0:
                    break
            j += 1
        found.append((module[-1] if module else "", json.loads(re.sub(r",\s*\]", "]", text[i:j + 1]))))
    return found


def print_settings(text):
    """$burr_scale, $burr_inset, $burr_bevel as set in the file (puzzlecad defaults otherwise)"""
    values = {"scale": 11.15, "inset": 0.07, "bevel": 0.5}
    for key in values:
        m = re.search(r"\$burr_" + key + r"\s*=\s*([\d.]+)\s*;", text)
        if m:
            values[key] = float(m.group(1))
        m = re.search(r"\$burr_" + key + r"\s*=\s*\$burr_" + key + r"\s*\*\s*([\d.]+)\s*;", text)
        if m:
            values[key] *= float(m.group(1))
    return values


# --- gluing ----------------------------------------------------------------

def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def apply(rot, v):
    return tuple(sum(rot[r][c] * v[c] for c in range(3)) for r in range(3))


def compose(t1, t2):
    """t1 after t2"""
    r1, d1 = t1
    r2, d2 = t2
    rot = tuple(tuple(sum(r1[r][k] * r2[k][c] for k in range(3)) for c in range(3)) for r in range(3))
    return rot, tuple(a + b for a, b in zip(apply(r1, d2), d1))


def inverse(t):
    rot, d = t
    rt = tuple(tuple(rot[c][r] for c in range(3)) for r in range(3))
    return rt, tuple(-c for c in apply(rt, d))


def glue(male, female):
    """the motion that puts the female connector's part onto the male one's:
    female face against the male face, orientations aligned"""
    src = (female["normal"], female["orient"], cross(female["normal"], female["orient"]))
    out = tuple(-c for c in male["normal"])
    dst = (out, male["orient"], cross(out, male["orient"]))
    rot = tuple(tuple(sum(dst[k][r] * src[k][c] for k in range(3)) for c in range(3)) for r in range(3))
    target = tuple(a + b for a, b in zip(male["cell"], male["normal"]))
    return rot, tuple(a - b for a, b in zip(target, apply(rot, female["cell"])))


def place(parts, edges):
    """glue the parts along the edges; the pieces, or None when inconsistent"""
    where, root = {}, {}
    for start in range(len(parts)):
        if start in where:
            continue
        where[start], root[start] = IDENTITY, start
        stack = [start]
        while stack:
            p = stack.pop()
            for pm, pf, t in edges:
                if pm == p:
                    q, tq = pf, compose(where[p], t)
                elif pf == p:
                    q, tq = pm, compose(where[p], inverse(t))
                else:
                    continue
                if q in where:
                    if where[q] != tq:
                        return None
                else:
                    where[q], root[q] = tq, start
                    stack.append(q)
    pieces = {}
    for p, (cells, _) in enumerate(parts):
        rot, d = where[p]
        moved = {tuple(a + b for a, b in zip(apply(rot, c), d)) for c in cells}
        piece = pieces.setdefault(root[p], set())
        if piece & moved:
            return None
        piece |= moved
    return [pieces[k] for k in sorted(pieces)]


def matchings(males, females):
    """injective partial maps males -> females, most pairs first"""
    for k in range(min(len(males), len(females)), -1, -1):
        for mi in itertools.combinations(range(len(males)), k):
            for fi in itertools.permutations(range(len(females)), k):
                yield list(zip(mi, fi))


def join(parts):
    """(pieces, unpaired connectors, ambiguous)"""
    groups = []
    for label in sorted({c["label"] for _, cs in parts for c in cs}):
        males = [(p, c) for p, (_, cs) in enumerate(parts) for c in cs if c["kind"] == "m" and c["label"] == label]
        females = [(p, c) for p, (_, cs) in enumerate(parts) for c in cs if c["kind"] == "f" and c["label"] == label]
        options = []
        for match in matchings(males, females):
            if any(males[a][0] == females[b][0] for a, b in match):
                continue
            paired_m, paired_f = {a for a, _ in match}, {b for _, b in match}
            options.append(([(males[a][0], females[b][0], glue(males[a][1], females[b][1])) for a, b in match],
                            [males[a] for a in range(len(males)) if a not in paired_m] +
                            [females[b] for b in range(len(females)) if b not in paired_f]))
        groups.append(options)

    best, best_pairs, ambiguous = None, -1, False
    for combo in itertools.product(*groups):
        edges = [e for es, _ in combo for e in es]
        if len(edges) < best_pairs:
            continue
        pieces = place(parts, edges)
        if pieces is None:
            continue
        key = sorted(canonical(p) for p in pieces)
        if len(edges) > best_pairs:
            best, best_pairs, ambiguous = (pieces, [u for _, us in combo for u in us], key), len(edges), False
        elif key != best[2]:
            ambiguous = True
    if best is None:
        raise ValueError("the connectors do not glue the parts together consistently")
    return best[0], best[1], ambiguous


# --- shapes ----------------------------------------------------------------

def normalise(cells):
    low = [min(c[i] for c in cells) for i in range(3)]
    return frozenset(tuple(c[i] - low[i] for i in range(3)) for c in cells)


def rotations():
    out = []
    for perm in itertools.permutations(range(3)):
        for signs in itertools.product((1, -1), repeat=3):
            m = [[signs[r] if perm[r] == c else 0 for c in range(3)] for r in range(3)]
            det = (m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
                   + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]))
            if det == 1:
                out.append(m)
    return out


ROTATIONS = rotations()


def canonical(cells):
    """the same for all rotations of a shape"""
    return min(tuple(sorted(normalise({apply(m, c) for c in cells}))) for m in ROTATIONS)


def read_burrtools(path):
    """(shapes as (filled, variable) cell sets, piece shape ids, result id) of problem 0"""
    with open(path, "rb") as f:
        data = f.read()
    if data[:2] == b"\x1f\x8b":
        data = gzip.decompress(data)
    root = ET.fromstring(data)
    if root.find("gridType").get("type") != "0":
        raise ValueError(f"{path}: only cube grids are supported")
    shapes = []
    for v in root.iter("voxel"):
        nx, ny = int(v.get("x")), int(v.get("y"))
        filled, variable = set(), set()
        idx = 0
        for m in re.finditer(r"([#+_])\d*", v.text or ""):
            cell = (idx % nx, (idx // nx) % ny, idx // (nx * ny))
            if m.group(1) == "#":
                filled.add(cell)
            elif m.group(1) == "+":
                variable.add(cell)
            idx += 1
        shapes.append((filled, variable))
    problem = root.find("problems/problem")
    pieces = []
    for s in problem.find("shapes"):
        pieces += [int(s.get("id"))] * int(s.get("count", s.get("max", "1")))
    return shapes, pieces, int(problem.find("result").get("id"))


def voxel_xml(filled, variable=frozenset(), name=None):
    cells = set(filled) | set(variable)
    size = [max(c[i] for c in cells) + 1 for i in range(3)]
    text = "".join("#" if (x, y, z) in filled else "+" if (x, y, z) in variable else "_"
                   for z in range(size[2]) for y in range(size[1]) for x in range(size[0]))
    attr = f' name="{escape(name)}"' if name else ""
    return f'   <voxel x="{size[0]}" y="{size[1]}" z="{size[2]}" type="0"{attr}>{text}</voxel>\n'


def write_puzzle(path, pieces, result, comment):
    """pieces: cell sets; identical shapes become one shape with a count"""
    kinds = []
    for p in pieces:
        key = canonical(p)
        for k in kinds:
            if k[0] == key:
                k[2] += 1
                break
        else:
            kinds.append([key, p, 1])
    out = '<?xml version="1.0"?>\n<puzzle version="2">\n <gridType type="0"/>\n <colors/>\n <shapes>\n'
    for i, (_, p, _) in enumerate(kinds):
        out += voxel_xml(p, name=f"P{i + 1}")
    if result:
        out += voxel_xml(result[0], result[1], "result")
    out += "  </shapes>\n  <problems>\n"
    if result:
        out += "   <problem>\n    <shapes>\n"
        for i, (_, _, n) in enumerate(kinds):
            out += f'     <shape id="{i}" count="{n}"/>\n'
        out += f'    </shapes>\n    <result id="{len(kinds)}"/>\n    <bitmap/>\n   </problem>\n'
    out += f"  </problems>\n  <comment>{escape(comment)}</comment>\n </puzzle>\n"
    with open(path, "w") as f:
        f.write(out)


# --- main ------------------------------------------------------------------

def decode(path):
    """(pieces, unpaired connectors, ambiguous, print settings)"""
    with open(path) as f:
        text = f.read()
    parts = []
    for _, plate in plates(text):
        for piece in plate:
            parts += components(*parse_piece(piece))
    if not parts:
        raise ValueError(f"{path}: no burr_plate pieces found")
    pieces, unpaired, ambiguous = join(parts)
    return [normalise(p) for p in pieces], unpaired, ambiguous, print_settings(text)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("scad", help="puzzlecad .scad file")
    ap.add_argument("-o", "--output", help="BurrTools file to write")
    target = ap.add_mutually_exclusive_group()
    target.add_argument("--result", metavar="FILE", help="take the result shape from this BurrTools file")
    target.add_argument("--cube", type=int, metavar="N", help="result is an N x N x N box")
    ap.add_argument("--check", metavar="FILE", help="compare the pieces with this BurrTools file (up to rotation)")
    args = ap.parse_args()

    pieces, unpaired, ambiguous, settings = decode(args.scad)
    print(f"{len(pieces)} pieces: {', '.join(str(len(p)) for p in sorted(pieces, key=len, reverse=True))} cubes")
    for _, c in unpaired:
        print(f"warning: connector {c['kind']}{c['label']} at {c['cell']} pairs with nothing", file=sys.stderr)
    if ambiguous:
        print("warning: the connectors can be paired in several ways that give different pieces", file=sys.stderr)

    play = 2 * settings["inset"] / settings["scale"]
    chamfer = settings["bevel"] / (math.sqrt(2) * settings["scale"])
    model = f"{play:.4f},0,{chamfer:.4f}"
    print(f"printed geometry for rotation moves: burrTxt -R -m {model}")

    status = 0
    if args.check:
        shapes, ids, _ = read_burrtools(args.check)
        same = sorted(canonical(p) for p in pieces) == sorted(canonical(shapes[i][0]) for i in ids)
        print(f"pieces {'match' if same else 'DIFFER from'} {args.check}")
        status = 0 if same else 1

    if args.output:
        result = None
        if args.result:
            shapes, _, rid = read_burrtools(args.result)
            result = shapes[rid]
        elif args.cube:
            box = {(x, y, z) for x in range(args.cube) for y in range(args.cube) for z in range(args.cube)}
            volume = sum(len(p) for p in pieces)
            result = (box, set()) if volume == len(box) else (set(), box)
            if volume > len(box):
                print(f"warning: the pieces have {volume} cubes, more than the box", file=sys.stderr)
        write_puzzle(args.output, pieces, result,
                     f"converted from {args.scad.split('/')[-1]} by puzzlecad2xmpuzzle.py; "
                     f"printed geometry for rotation moves: -m {model}")
        print(f"wrote {args.output}" + ("" if result else " (pieces only, add the result shape in BurrTools)"))
    return status


if __name__ == "__main__":
    sys.exit(main())
