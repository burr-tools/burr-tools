#ifndef __DISASSMTOMOVES_H__
#define __DISASSMTOMOVES_H__

/* this class takes a disassembly tree and generates relativ piecepositions
 * for all peaces at each step of disassembly
 */
#include <../lib/disassembly.h>

 
class DisasmToMoves {

  const disassembly_c * dis;
  float * moves;
  int piecenumber;

  int doRecursive(const separation_c * tree, int step, float weight, int cx, int cy, int cz, int remove);

public:

  DisasmToMoves(const disassembly_c * disasm, float * mv, int piecen) : dis(disasm), moves(mv), piecenumber(piecen) {}
  
  /* sets the moves for the step, if the value is not integer you
   * get a intermediate of the necessary move (for animation)
   */
  void setStep(float step);
};

#endif
