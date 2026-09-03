/* BurrTools */
#include "disassembler_factory.h"

#include "disassembler_0.h"
#include "disassembler_crowell.h"

disassembler_c * createDisassembler(const problem_c & puz,
                                    bool enableRotations,
                                    solverType_e type) {

  if (type == SOLVER_CROWELL)
    return new disassembler_crowell_c(puz, enableRotations);

  return new disassembler_0_c(puz, enableRotations);
}
