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

#include <stdio.h>
#include <string.h>

#include "lib_interface.h"

const unsigned long numShapes = 6;

// Based on the CubeInCage example puzzle...
const char* const shapes[] = {"__________#______#______#______#______#____________________#______+___#######___+______#____________________#_____+++__#+++++#__+++_____#___________#####__#+#+#__#+++#_##+++##_#+++#__#+#+#__#####___________#_____+++__#+++++#__+++_____#____________________#______+___#######___+______#____________________#______#______#______#______#__________",
    "######___##___##___##___##__#######",
    "######__###___##___##___##__#######",
    "_##__####________#",
    "______________#_____##__________________________________###___________________________________#_____#___________________________________________________________________________________________________________________",
    "_________________________________________________###_____#_____#_____________________#_#_________#_#_________________________________###________________________________________________________________________________",
};

int sizes[][3] =
    { {7,7,7}
    , {5,7,1}
    , {5,7,1}
    , {3,3,2}
    , {6,6,6}
    , {6,6,6}
    };

// Example C-code for creating puzzles..
int main()
{
    printf("Allocating new puzzle\n");
    puzzle_c *puzzle = alloc_empty_puzzle();

    printf("Allocating new problem\n");
    unsigned int probIdx = addProblem(puzzle);
    problem_c *problem = getProblem(puzzle, probIdx);

    // Load the shapes...
    printf("Loading %lu shapes\n",numShapes);
    for (unsigned long sId=0; sId < numShapes; sId++ ) {
        printf("Allocating shape %lu\n", sId);
        unsigned int shapeIdx = addShape(puzzle, sizes[sId][0], sizes[sId][1], sizes[sId][2]); // Add an empty shape of the given size
        voxel_c *shape = getShape(puzzle, shapeIdx);

        if ( sId == 0 ) { 
            // Set the target shape for the problem's result
            setResultId(problem, 0);
        } else {
            printf("   ... setting size\n");
            setShapeMinimum(problem, shapeIdx, 1);
            setShapeMaximum(problem, shapeIdx, 1);
        }

        unsigned int idx = 0;
        unsigned long sLen = strlen(shapes[sId]);
        printf("   ... reading %lu voxels\n", sLen);
        for (unsigned int pos = 0; pos < sLen; pos++) {
            switch (shapes[sId][pos]) {
                case '#': setState(shape, idx++, VX_FILLED); break;
                case '+': setState(shape, idx++, VX_VARIABLE); break;
                case '_': setState(shape, idx++, VX_EMPTY); break;
            }
        }
    }

    // Shape ID 2 has 2 pieces
    setShapeMinimum(problem, 2, 2);
    setShapeMaximum(problem, 2, 2);

    // Group parts 1 & 2 (including both instances of piece 2)
    setPartGroup(problem, /* partId, not shapeId */ 0, 1, 1);
    setPartGroup(problem, /* partId, not shapeId */ 1, 1, 2);

    printf("--------------------\n\n");
    printPuzzleXML(puzzle);
    printf("--------------------\n\n");

    printf("Solving...\n");
    solve(puzzle);

    printf("--------------------\n\n");
    printPuzzleXML(puzzle);
    printf("--------------------\n\n");
    printf("Done!\n\n");
}
