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
#ifndef __ROTATION_MOVES_H__
#define __ROTATION_MOVES_H__

#include <vector>

class disassemblerNode_c;

/**
 * Direction codes used on disassemblerNode_c for rotation edges.
 * dir = ROTATION_DIR_BASE + axis*2 + sense  (axis 0..2, sense 0=+90 1=-90)
 */
static const unsigned int ROTATION_DIR_BASE = 100;

inline bool isRotationDirection(unsigned int dir) {
  return dir >= ROTATION_DIR_BASE;
}

/**
 * 90° rotation-move generator. Classic and Fortran each have their own
 * implementation; movementAnalysator_c only talks to this interface.
 */
class rotationMoves_c {

public:

  virtual ~rotationMoves_c(void) {}

  virtual void init_find(disassemblerNode_c * nd, const std::vector<unsigned int> & pcs) = 0;
  virtual disassemblerNode_c * find(void) = 0;
};

#endif
