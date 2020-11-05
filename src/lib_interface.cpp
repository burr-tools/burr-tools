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

#include <iostream>
#include <streambuf>

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#include "lib/voxel.h"
#include "lib/voxel_0.h"
#include "lib/solution.h"
#include "lib/problem.h"
#include "lib/puzzle.h"
#include "lib/solvethread.h"
#include "tools/xml.h"

#include "lib_interface.h"

// This is a collection of wrapper functions to enable C-compatable languages to link with the Burr Tools library.
extern "C"
{

    // --------------------------------
    // Voxel (puzzle piece)
    // --------------------------------

    unsigned int getX(voxel_c *v) { return v->getX(); }
    unsigned int getY(voxel_c *v) { return v->getY(); }
    unsigned int getZ(voxel_c *v) { return v->getZ(); }

    unsigned int getXYZ(voxel_c *v) { return v->getXYZ(); }
    int getIndex(voxel_c *v, unsigned int x, unsigned int y, unsigned int z)  { return v->getIndex(x,y,z); }
    bool indexToXYZ(voxel_c *v, unsigned int index, unsigned int *x, unsigned int *y, unsigned int *z) { return v->indexToXYZ(index,x,y,z); }

    bool isEmptyXYZ(voxel_c *v, unsigned int x, unsigned int y, unsigned int z)  { return v->isEmpty(x,y,z); }
    bool isFilledXYZ(voxel_c *v, unsigned int x, unsigned int y, unsigned int z) { return v->isFilled(x,y,z); }
    bool isVariableXYZ(voxel_c *v, unsigned int x, unsigned int y, unsigned int z) { return v->isVariable(x,y,z); }
    void setStateXYZ(voxel_c *v, unsigned int x, unsigned int y, unsigned int z, int state) { v->setState(x,y,z,state); }
    void setColorXYZ(voxel_c *v, unsigned int x, unsigned int y, unsigned int z, unsigned int color) { v->setColor(x,y,z,color); }

    bool isEmpty(voxel_c *v, unsigned int i) { return v->isEmpty(i); }
    bool isFilled(voxel_c *v, unsigned int i) { return v->isFilled(i); }
    bool isVariable(voxel_c *v, unsigned int i) { return v->isVariable(i); }
    void setState(voxel_c *v, unsigned int i, int state)  { v->setState(i,state); }
    void setColor(voxel_c *v, unsigned int i, unsigned int color)  { v->setColor(i,color); }

    bool validCoordinate(voxel_c *v, int x, int y, int z) { return v->validCoordinate(x,y,z); }


    // --------------------------------
    // Problem
    // --------------------------------

    void setResultId(problem_c *pr, unsigned int shape) { pr->setResultId(shape); }
    void clearResult(problem_c *pr) { pr->clearResult(); }
    bool resultValid(problem_c *pr) { return pr->resultValid(); }
    unsigned int getResultId(problem_c *pr) { return pr->getResultId(); }

    void removeShapeFromProblem(problem_c *pr, unsigned short shapeId) { pr->removeShape(shapeId); }
    void setShapeMinimum(problem_c *pr, unsigned int shapeId, unsigned int count) { pr->setShapeMinimum(shapeId,count); }
    void setShapeMaximum(problem_c *pr, unsigned int shapeId, unsigned int count) { pr->setShapeMaximum(shapeId,count); }
    unsigned int getShapeMinimum(problem_c *pr, unsigned int shapeId) { return pr->getShapeMinimum(shapeId); }
    unsigned int getShapeMaximum(problem_c *pr, unsigned int shapeId) { return pr->getShapeMaximum(shapeId); }
    bool usesShape(problem_c *pr, unsigned int shapeId) { return pr->usesShape(shapeId); }
    unsigned int getNumberOfPieces(problem_c *pr) { return pr->getNumberOfPieces(); }

    void setPartGroup(problem_c *pr, unsigned int part, unsigned short group, unsigned short count)   { pr->setPartGroup(part,group,count); }

    unsigned int getMaxHoles(problem_c *pr) { return pr->getMaxHoles(); }
    void setMaxHoles(problem_c *pr, unsigned int value)  { pr->setMaxHoles(value); }
    void setMaxHolesInvalid(problem_c *pr) { pr->setMaxHolesInvalid(); }

    void removeAllSolutions(problem_c *pr) { pr->removeAllSolutions(); }
    unsigned long getNumAssemblies(problem_c *pr)  { return pr->getNumAssemblies(); }
    unsigned long getNumSolutions(problem_c *pr)  { return pr->getNumSolutions(); }
    unsigned long getUsedTime(problem_c *pr)  { return pr->getUsedTime(); }
    unsigned int getNumberOfSavedSolutions(problem_c *pr)  { return pr->getNumberOfSavedSolutions(); }
    solution_c * getSavedSolution(problem_c *pr, unsigned int sol)  { return pr->getSavedSolution(sol); }
    void removeSolution(problem_c *pr, unsigned int sol) { return pr->removeSolution(sol); }
    void sortSolutions(problem_c *pr, int by) { return pr->sortSolutions(by); }


    // --------------------------------
    // Puzzle
    // --------------------------------
    
    puzzle_c * alloc_empty_puzzle() { return new puzzle_c(new gridType_c(gridType_c::GT_BRICKS)); };
    void dealloc_puzzle(puzzle_c **pz) { delete *pz; *pz = 0; }

    unsigned int addShape(puzzle_c *pz, unsigned int sx, unsigned int sy, unsigned int sz)  { return pz->addShape(sx,sy,sz); }
    unsigned int getNumberOfShapes(puzzle_c *pz) { return pz->getNumberOfShapes(); }
    voxel_c * getShape(puzzle_c *pz, unsigned int idx)  { return pz->getShape(idx); }
    void removeShape(puzzle_c *pz, unsigned int idx)  { pz->removeShape(idx); }

    unsigned int addColor(puzzle_c *pz, unsigned char r, unsigned char g, unsigned char b)  { return pz->addColor(r,g,b); }
    void removeColor(puzzle_c *pz, unsigned int idx)  { pz->removeColor(idx); }
    void getColor(puzzle_c *pz, unsigned int idx, unsigned char * r, unsigned char * g, unsigned char * b) { pz->getColor(idx,r,g,b); }
    unsigned int colorNumber(puzzle_c *pz) { return pz->colorNumber(); }

    unsigned int addProblem(puzzle_c *pz)  { return pz->addProblem(); }
    unsigned int getNumberOfProblems(puzzle_c *pz) { return pz->getNumberOfProblems(); }
    void removeProblem(puzzle_c *pz, unsigned int p)  { pz->removeProblem(p); }
    problem_c * getProblem(puzzle_c *pz, unsigned int p)  { return pz->getProblem(p); }

    void printPuzzleXML(puzzle_c *pz) {
        xmlWriter_c xml(std::cout);
        pz->save(xml);
    }

    // --------------------------------
    // Solver
    // --------------------------------
    
    bool solve(puzzle_c *p)
    {
        int par = solveThread_c::PAR_REDUCE;
        bool restart = false;
        int filenumber = 0;
        int firstProblem = 0;
        int lastProblem = p->getNumberOfProblems();

        restart = true;
        par |= solveThread_c::PAR_DISASSM;

        for (unsigned int i = 0; i < p->getNumberOfShapes(); i++)
            p->getShape(i)->initHotspot();


        for (int pr = firstProblem ; pr < lastProblem ; pr++) {

            if (restart)
                p->getProblem(pr)->removeAllSolutions();

            solveThread_c assmThread(*p->getProblem(pr), par);

            // Initialize for new solution attempt
            assmThread.start(false);

            if (assmThread.currentAction() == solveThread_c::ACT_ERROR) {
                std::cout << "Exception in Solver\n";
                std::cout << " file      : " << assmThread.getAssertException().file;
                std::cout << " function  : " << assmThread.getAssertException().function;
                std::cout << " line      : " << assmThread.getAssertException().line;
                std::cout << " expression: " << assmThread.getAssertException().expr;
                return false;
            }
        }

        return true;
    }
};
