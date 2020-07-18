/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

let numShapes = 6;

// Based on the CubeInCage example puzzle...
let shapes =
    [ Array("__________#______#______#______#______#____________________#______+___#######___+______#____________________#_____+++__#+++++#__+++_____#___________#####__#+#+#__#+++#_##+++##_#+++#__#+#+#__#####___________#_____+++__#+++++#__+++_____#____________________#______+___#######___+______#____________________#______#______#______#______#__________")
    , Array("######___##___##___##___##__#######")
    , Array("######__###___##___##___##__#######")
    , Array("_##__####________#")
    , Array("______________#_____##__________________________________###___________________________________#_____#___________________________________________________________________________________________________________________")
    , Array("_________________________________________________###_____#_____#_____________________#_#_________#_#_________________________________###________________________________________________________________________________")
    ];

let sizes =
    [ [7,7,7]
    , [5,7,1]
    , [5,7,1]
    , [3,3,2]
    , [6,6,6]
    , [6,6,6]
    ];

print("Allocating new puzzle");
let puzzle = alloc_empty_puzzle();

print("Allocating new problem");
let probIdx = addProblem(puzzle);
let problem = getProblem(puzzle, probIdx);

// Load the shapes...
print("Loading \(numShapes) shapes");
for sId in 0...numShapes-1 {
    print("Allocating shape \(sId)");
    let shapeIdx = addShape(puzzle, UInt32(sizes[sId][0]), UInt32(sizes[sId][1]), UInt32(sizes[sId][2])); // Add an empty shape of the given size
    let shape = getShape(puzzle, shapeIdx);

    if ( sId == 0 ) { 
        // Set the target shape for the problem's result
        setResultId(problem, 0);
    } else {
        print("   ... setting size");
        setShapeMinimum(problem, shapeIdx, 1);
        setShapeMaximum(problem, shapeIdx, 1);
    }

    var idx = UInt32(0);
    let sLen = shapes[sId].count;
    print("   ... reading \(sLen) voxels");
    for pos in 0...sLen-1 {
        switch (shapes[sId][pos]) {
            case "#": setState(shape, idx, Int32(VX_FILLED.rawValue)); break;
            case "+": setState(shape, idx, Int32(VX_VARIABLE.rawValue)); break;
            case "_": setState(shape, idx, Int32(VX_EMPTY.rawValue)); break;
            default: break;
        }
        idx += 1;
    }
}

// Shape ID 2 has 2 pieces
setShapeMinimum(problem, 2, 2);
setShapeMaximum(problem, 2, 2);

// Group parts 1 & 2 (including both instances of piece 2)
setPartGroup(problem, /* partId, not shapeId */ 0, 1, 1);
setPartGroup(problem, /* partId, not shapeId */ 1, 1, 2);

print("--------------------\n\n");
printPuzzleXML(puzzle);
print("--------------------\n\n");

print("Solving...\n");
solve(puzzle);

print("--------------------\n\n");
printPuzzleXML(puzzle);
print("--------------------\n\n");
print("Done!\n\n");
