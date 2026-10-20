// Test input for scripts/puzzlecad2xmpuzzle.py: pieces printed in parts that
// are glued at connectors, in the forms puzzlecad files use.

include <puzzlecad.scad>

$burr_scale = 10;
$burr_inset = 0.1;
$burr_bevel = 0.5;

// Piece 1: a female part glued onto a male one; the glued piece is chiral,
// so a wrong turn of the female part shows.
// Piece 2: one plate entry with two separate groups of cubes, glued to each
// other by label B.
module plate1() {
    burr_plate([
        ["xx{connect=mz+y+,clabel=A}"],
        ["x{connect=fz+y+,clabel=A}xx|x"],
        ["xxx{connect=mz+x-,clabel=B}|...|x{connect=fz-y+,clabel=B}.."]
    ]);
}

// Piece 3: its parts are on two plates, glued by label C (a chiral piece).
// Piece 4: label D occurs twice per kind, and only one of the two pairings
// keeps the parts from overlapping.
module plate2() {
    burr_plate([
        ["x{connect=fz+y+,clabel=C}x"],
        ["x{connect=mz+y+,clabel=D}x{connect=mz+y+,clabel=D}"],
        ["x", "x{connect=fz+y+,clabel=D}"],
        ["x{connect=fz+y+,clabel=D}x"]
    ]);
}

module plate3() {
    burr_plate([
        ["x{connect=mz+y+,clabel=C}|x"]
    ]);
}
