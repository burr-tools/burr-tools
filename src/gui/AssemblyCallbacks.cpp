#include "AssemblyCallbacks.h"

#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif


/* this structure contains all information for one solution
 * if there is no disassembly requested by the user, the disasm field will be 0
 */
typedef struct solution {
  const disassembly_c * disassembly;
  const voxel_c * assembly;
} solution;


#ifdef WIN32
unsigned long __stdcall start_th(void * c)
#else
void* start_th(void * c)
#endif
{
  assemblerThread * p = (assemblerThread*)c;

  if (p->doReduce) {
    p->action = assemblerThread::ACT_REDUCE;
    p->assembler.reduce();
    p->doReduce = false;
  }

  p->action = assemblerThread::ACT_ASSEMBLING;

  p->assembler.assemble(p);

  p->action = assemblerThread::ACT_FINISHED;

  return 0;
}

assemblerThread::assemblerThread(const puzzle_c * puzzle, int solAction, bool reduce) :
assembler(puzzle),
doReduce(reduce),
assemblies(0),
action(ACT_PREPARATION),
_solutionAction(solAction),
_piecenumber(puzzle->getPieces())
{ }

assemblerThread::~assemblerThread(void) {

  assembler.stop();

  struct timespec req, rem;

  while (!assembler.stopped()) {
    req.tv_sec = 0;
    req.tv_nsec = 100000;
    nanosleep (&req, &rem);
  }

  std::vector<solution>::iterator i = sols.begin();
  
  while (i != sols.end()) {
    if (i->disassembly) {
      delete i->disassembly;
      i->assembly = 0;
    } else
      delete i->assembly;
    i++;
  }
  sols.clear();
}

bool assemblerThread::assembly(voxel_c * as) {

  assemblies++;

  switch(_solutionAction) {
  case SOL_SAVE_ASM:
    {
      solution s;
  
      s.assembly = new voxel_c(as);
      s.disassembly = 0;
  
      sols.push_back(s);
    }
    break;

  case SOL_DISASM:
    {
      solution s;

      disassembler_3_c d(as, _piecenumber);
  
      action = ACT_DISASSEMBLING;
      s.disassembly = d.disassemble();
      action = ACT_ASSEMBLING;
  
      if (s.disassembly) {
        s.assembly = s.disassembly->getStart();
        sols.push_back(s);
      }
    }
    break;
  }

  return true;
}

void assemblerThread::stop(void) {
  assembler.stop();

  struct timespec req, rem;

  while (!assembler.stopped()) {
    req.tv_sec = 0;
    req.tv_nsec = 100000;
    nanosleep (&req, &rem);
  }

  action = ACT_PAUSING;
}

void assemblerThread::start(void) {

#ifdef WIN32
  CreateThread(NULL,0, start_th, this,0,0 );
#else
  pthread_t th; pthread_create(&th, 0, start_th, this);
#endif

}

bool assemblerThread::stopped(void) { return assembler.stopped(); }

unsigned long assemblerThread::number(void) { return sols.size(); }
const voxel_c * assemblerThread::getAssm(unsigned long num) { return sols[num].assembly; }
const disassembly_c * assemblerThread::getDisasm(unsigned long num) { return sols[num].disassembly; }

