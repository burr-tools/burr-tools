#ifndef __ASSEMBLYCALLBACK_H__
#define __ASSEMBLYCALLBACK_H__

#include "../lib/disassembly.h"
#include "../lib/assm_0_frontend_0.h"
#include "../lib/disassembler_3.h"

//class solutions_c;
struct solution;

/* this class will handle the solving of the puzzle. It will start the background thread
 * pause it, continue the work, save and load it
 */
class assemblerThread : public assembler_cb {

  int _piecenumber;
  bool doReduce;
  int assemblies;
  int action;
  int _solutionAction;

  assm_0_frontend_0_c assembler;

  std::vector<solution> sols;

public:

  enum {
    SOL_COUNT_ASM,
    SOL_SAVE_ASM,
    SOL_DISASM
  };

  // start the thread
  assemblerThread(const puzzle_c * puz, int solAction, bool reduce);

  // stop and exit
  ~assemblerThread(void);

  // the callbacl
  bool assembly(voxel_c * assm);

  enum {
    ACT_PREPARATION,
    ACT_REDUCE,
    ACT_ASSEMBLING,
    ACT_DISASSEMBLING,
    ACT_PAUSING,
    ACT_FINISHED
  };

  int currentAction(void) { return action; }

  int getAssemblies(void) { return assemblies; }

  void start(void);
  void stop(void);

  bool stopped(void);

  float getFinished(void) { return assembler.getFinished(); }
  unsigned long getIterations(void) { return assembler.getIterations(); }

  unsigned long number(void);
  const voxel_c * getAssm(unsigned long num);
  const disassembly_c * getDisasm(unsigned long num);

  const char * errors(void) { return assembler.errors(); }

#ifdef WIN32
friend unsigned long __stdcall start_th(void * c);
#else
friend void* start_th(void * c);
#endif

};

#endif
