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
#ifndef __DISASSEMBLER_CROWELL_H__
#define __DISASSEMBLER_CROWELL_H__

#include "disassembler_a.h"
#include "crowell_solver.h"
#include "solvertype.h"

/**
 * Take-apart BFS using Fortran-style rotation generation and a per-node
 * successor cap (CheckAvailMoves Nkeep = 100). Classic disassembler_0_c
 * is unchanged.
 *
 * Feature status and how to port remaining Fortran behaviour: crowell_solver.h
 */
class disassembler_crowell_c : public disassembler_a_c {

private:

  static const int MAX_SUCCESSORS = 100;

  separation_c * disassemble_rec(const std::vector<unsigned int> & pieces, disassemblerNode_c * start);

public:

  disassembler_crowell_c(const problem_c & puz, bool enableRotations = false)
    : disassembler_a_c(puz, enableRotations, SOLVER_CROWELL) { }
  ~disassembler_crowell_c() { }

private:

  disassembler_crowell_c(const disassembler_crowell_c&);
  void operator=(const disassembler_crowell_c&);
};

#endif
