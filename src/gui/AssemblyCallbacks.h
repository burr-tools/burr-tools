/* Burr Solver
 * Copyright (C) 2003-2005  Andreas Röver
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */


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

  bool stopped(void) const { return assembler.stopped(); }

  float getFinished(void) { return assembler.getFinished(); }
  unsigned long getIterations(void) { return assembler.getIterations(); }

  unsigned long number(void) const;
  const assemblyVoxel_c * getAssm(unsigned long num) const;
  const disassembly_c * getDisasm(unsigned long num) const;

  const char * errors(void) { return assembler.errors(); }

#ifdef WIN32
friend unsigned long __stdcall start_th(void * c);
#else
friend void* start_th(void * c);
#endif

};

#endif
