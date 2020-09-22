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

#ifdef __cplusplus
extern "C" {

#else
    typedef struct voxel_c voxel_c;
    typedef struct solution_c solution_c;
    typedef struct problem_c problem_c;
    typedef struct puzzle_c puzzle_c;
    typedef unsigned char bool;
#endif

    // This is a collection of wrapper functions to enable C-compatable languages to link with the Burr Tools library.


    typedef enum {
        VX_EMPTY,   ///< This is used for empty voxels
        VX_FILLED,  ///< This value is used for filled voxels
        VX_VARIABLE ///< This value is used for voxels with variable content
    } VoxelState;

    // --------------------------------
    // Voxel (puzzle piece)
    // --------------------------------

    unsigned int getX(voxel_c *v) ;
    unsigned int getY(voxel_c *v) ;
    unsigned int getZ(voxel_c *v) ;

    unsigned int getXYZ(voxel_c *v) ;
    int getIndex(voxel_c *v, unsigned int x, unsigned int y, unsigned int z)  ;
    bool indexToXYZ(voxel_c *v, unsigned int index, unsigned int *x, unsigned int *y, unsigned int *z) ;

    bool isEmptyXYZ(voxel_c *v, unsigned int x, unsigned int y, unsigned int z)  ;
    bool isFilledXYZ(voxel_c *v, unsigned int x, unsigned int y, unsigned int z) ;
    bool isVariableXYZ(voxel_c *v, unsigned int x, unsigned int y, unsigned int z) ;
    void setStateXYZ(voxel_c *v, unsigned int x, unsigned int y, unsigned int z, int state) ;
    void setColorXYZ(voxel_c *v, unsigned int x, unsigned int y, unsigned int z, unsigned int color) ;

    bool isEmpty(voxel_c *v, unsigned int i) ;
    bool isFilled(voxel_c *v, unsigned int i) ;
    bool isVariable(voxel_c *v, unsigned int i) ;
    void setState(voxel_c *v, unsigned int i, int state)  ;
    void setColor(voxel_c *v, unsigned int i, unsigned int color)  ;

    bool validCoordinate(voxel_c *v, int x, int y, int z) ;


    // --------------------------------
    // Problem
    // --------------------------------

    void setResultId(problem_c *pr, unsigned int shape) ;
    void clearResult(problem_c *pr) ;
    bool resultValid(problem_c *pr) ;
    unsigned int getResultId(problem_c *pr) ;

    void removeShapeFromProblem(problem_c *pr, unsigned short shapeId) ;
    void setShapeMinimum(problem_c *pr, unsigned int shapeId, unsigned int count) ;
    void setShapeMaximum(problem_c *pr, unsigned int shapeId, unsigned int count) ;
    unsigned int getShapeMinimum(problem_c *pr, unsigned int shapeId) ;
    unsigned int getShapeMaximum(problem_c *pr, unsigned int shapeId) ;
    bool usesShape(problem_c *pr, unsigned int shapeId) ;
    unsigned int getNumberOfPieces(problem_c *pr) ;

    void setPartGroup(problem_c *pr, unsigned int part, unsigned short group, unsigned short count);

    unsigned int getMaxHoles(problem_c *pr) ;
    void setMaxHoles(problem_c *pr, unsigned int value)  ;
    void setMaxHolesInvalid(problem_c *pr) ;

    void removeAllSolutions(problem_c *pr) ;
    unsigned long getNumAssemblies(problem_c *pr)  ;
    unsigned long getNumSolutions(problem_c *pr)  ;
    unsigned long getUsedTime(problem_c *pr)  ;
    unsigned int getNumberOfSavedSolutions(problem_c *pr)  ;
    solution_c * getSavedSolution(problem_c *pr, unsigned int sol)  ;
    void removeSolution(problem_c *pr, unsigned int sol) ;
    void sortSolutions(problem_c *pr, int by) ;


    // --------------------------------
    // Puzzle
    // --------------------------------
    
    extern puzzle_c * alloc_empty_puzzle() ;;
    void dealloc_puzzle(puzzle_c **pz) ;

    extern unsigned int addShape(puzzle_c *pz, unsigned int sx, unsigned int sy, unsigned int sz)  ;
    unsigned int getNumberOfShapes(puzzle_c *pz) ;
    voxel_c * getShape(puzzle_c *pz, unsigned int idx)  ;
    void removeShape(puzzle_c *pz, unsigned int idx)  ;

    unsigned int addColor(puzzle_c *pz, unsigned char r, unsigned char g, unsigned char b)  ;
    void removeColor(puzzle_c *pz, unsigned int idx)  ;
    void getColor(puzzle_c *pz, unsigned int idx, unsigned char * r, unsigned char * g, unsigned char * b) ;
    unsigned int colorNumber(puzzle_c *pz) ;

    extern unsigned int addProblem(puzzle_c *pz)  ;
    unsigned int getNumberOfProblems(puzzle_c *pz) ;
    void removeProblem(puzzle_c *pz, unsigned int p)  ;
    problem_c * getProblem(puzzle_c *pz, unsigned int p)  ;

    void printPuzzleXML(puzzle_c *pz);


    // --------------------------------
    // Solver
    // --------------------------------
    
    bool solve(puzzle_c *pz);


#ifdef __cplusplus
}
#endif

