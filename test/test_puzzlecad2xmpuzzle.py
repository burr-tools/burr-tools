#!/usr/bin/env python3
"""Tests for scripts/puzzlecad2xmpuzzle.py.

Usage: test_puzzlecad2xmpuzzle.py <source root> <path to burrTxt>

The expected pieces are worked out by hand from test/data/puzzlecad/glued.scad
(see the comments there); they are compared up to rotation, not mirroring.
"""
import importlib.util
import os
import subprocess
import sys
import tempfile
import unittest

ROOT, BURRTXT = sys.argv[1], sys.argv[2]
spec = importlib.util.spec_from_file_location("p2x", os.path.join(ROOT, "scripts", "puzzlecad2xmpuzzle.py"))
p2x = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p2x)

SCAD = os.path.join(ROOT, "test", "data", "puzzlecad", "glued.scad")

# the female part turned half about y onto the male one's top face
PIECE_1 = {(0, 0, 0), (1, 0, 0), (1, 0, 1), (0, 0, 1), (-1, 0, 1), (1, 1, 1)}
# the second group turned a quarter about z onto the first one's end
PIECE_2 = {(0, 0, 0), (1, 0, 0), (2, 0, 0), (2, 0, 1)}
# across plates
PIECE_3 = {(0, 0, 0), (0, 1, 0), (0, 0, 1), (-1, 0, 1)}
# the only pairing of the D connectors without overlap
PIECE_4 = {(0, 0, 0), (1, 0, 0), (0, 0, 1), (-1, 0, 1), (1, 0, 1), (1, 0, 2)}


def mirrored(cells):
    return {(-x, y, z) for x, y, z in cells}


class Puzzlecad2XmpuzzleTest(unittest.TestCase):

    def test_glued_pieces(self):
        pieces, unpaired, ambiguous, settings = p2x.decode(SCAD)
        self.assertEqual(unpaired, [])
        self.assertFalse(ambiguous)
        self.assertEqual(sorted(p2x.canonical(p) for p in pieces),
                         sorted(p2x.canonical(p) for p in (PIECE_1, PIECE_2, PIECE_3, PIECE_4)))
        self.assertEqual(settings, {"scale": 10, "inset": 0.1, "bevel": 0.5})

    def test_expectations_are_chiral(self):
        # otherwise a part glued the mirrored way would pass
        for piece in (PIECE_1, PIECE_3):
            self.assertNotEqual(p2x.canonical(piece), p2x.canonical(mirrored(piece)))

    def test_options(self):
        cells, connectors = p2x.parse_piece(["x{label_text=A. B,label_scale=0.4}x{connect={fz-y+,mz+y+},clabel={C,D}}"])
        self.assertEqual(cells, {(0, 0, 0), (1, 0, 0)})
        self.assertEqual([(c["kind"], c["label"], c["cell"], c["normal"]) for c in connectors],
                         [("f", "C", (1, 0, 0), (0, 0, -1)), ("m", "D", (1, 0, 0), (0, 0, 1))])

    def test_unpaired_connector_is_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "lonely.scad")
            with open(path, "w") as f:
                f.write('burr_plate([["xx{connect=mz+y+,clabel=Q}"], ["x"]]);\n')
            pieces, unpaired, _, _ = p2x.decode(path)
            self.assertEqual(len(pieces), 2)
            self.assertEqual([(c["kind"], c["label"]) for _, c in unpaired], [("m", "Q")])

    def test_written_puzzle_loads(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = os.path.join(tmp, "glued.xmpuzzle")
            run = subprocess.run([sys.executable, os.path.join(ROOT, "scripts", "puzzlecad2xmpuzzle.py"), SCAD,
                                  "--cube", "3", "-o", out], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stderr)
            self.assertIn("-m 0.0200,0,0.0354", run.stdout)
            shapes, ids, result = p2x.read_burrtools(out)
            self.assertEqual(sorted(p2x.canonical(shapes[i][0]) for i in ids),
                             sorted(p2x.canonical(p) for p in (PIECE_1, PIECE_2, PIECE_3, PIECE_4)))
            # 20 cubes in a 3x3x3 box: every cell optional
            self.assertEqual((len(shapes[result][0]), len(shapes[result][1])), (0, 27))
            solved = subprocess.run([BURRTXT, "-q", out], capture_output=True, text=True, timeout=120)
            self.assertEqual(solved.returncode, 0, solved.stdout + solved.stderr)
            self.assertIn("assemblies", solved.stdout)


if __name__ == "__main__":
    unittest.main(argv=sys.argv[:1], verbosity=2)
