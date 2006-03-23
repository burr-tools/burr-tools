#include "grideditor.h"

#include "../lib/puzzle.h"
#include "../lib/voxel.h"

void gridEditor_c::setZ(unsigned int z) {

  // clamp the value to valid values
  if (z >= puzzle->getShape(piecenumber)->getZ()) z = puzzle->getShape(piecenumber)->getZ()-1;

  if (z != currentZ) {

    mZ = currentZ = z;
    redraw();
    callbackReason = RS_MOUSEMOVE;
    do_callback();
  }

}

void gridEditor_c::setPuzzle(puzzle_c * p, unsigned int piecenum) {

  puzzle = p;
  piecenumber = piecenum;

  // check if the current z value is in valid regions
  if ((piecenum < puzzle->shapeNumber()) && (puzzle->getShape(piecenum)->getZ() <= currentZ))
    currentZ = puzzle->getShape(piecenum)->getZ() - 1;

  if (currentZ < 0) currentZ = 0;

  redraw();
}

void gridEditor_c::clearPuzzle() {
  piecenumber = puzzle->shapeNumber();
  redraw();
}

