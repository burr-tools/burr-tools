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
 */
#ifndef __DISASSEMBLER_FACTORY_H__
#define __DISASSEMBLER_FACTORY_H__

#include "solvertype.h"

class disassembler_c;
class problem_c;

/** Create a take-apart engine. Caller owns the pointer.
 *  BurrTools Classic / comparison: bt_classic_solver.h
 *  SOLVER_CROWELL: crowell_solver.h
 *  SOLVER_BT2 uses Classic take-apart; assembly is assembler_bt2_c
 */
disassembler_c * createDisassembler(const problem_c & puz,
                                    bool enableRotations,
                                    solverType_e type = SOLVER_CLASSIC);

#endif
