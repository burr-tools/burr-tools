/* this is going to contain tools to create all pieces that
 * fullfill some kind of property, e.g. all hexominos, or so
 */

#ifndef __PIECEGENERATOR_H__
#define __PIECEGENERATOR_H__

#include "voxel.h"

#include <vector>

class pieceGenerator_c {

  std::vector<voxel_c *> pieces;

  void recGen(voxel_c * p);

public:

  // this generates all possible piece variations of the
  // given piece varying the VARIANT voxels inside the
  // piece. The pieces are normal burr pieces for
  // the time being
  pieceGenerator_c(const voxel_c * piece);

  unsigned long pieceNum(void) const { return pieces.size(); }

  const voxel_c * getPiece(unsigned long num) {
    return pieces[num];
  }

};

#endif
