/* this module contains printing routines for the classes,
 * they print function is not part of the classes bacause they are not
 * required there
 * they are in fact quite interface specific
 */

#include "voxel.h"
#include "puzzle.h"
#include "disassembly.h"

void print(const assemblyVoxel_c * v);
void print(const pieceVoxel_c * v);
void print(const voxel_c * v);
void print(const puzzle_c * p);
void print(const state_c * s, const assemblyVoxel_c *start, const separation_c * s, unsigned int piecenumber);
void print(const separation_c * s, const assemblyVoxel_c * start);
