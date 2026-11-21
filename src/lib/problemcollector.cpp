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
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02110-1307, USA.
 */
#include "problemcollector.h"

#include "assembly.h"
#include "disassembly.h"
#include "problem.h"
#include "solution.h"

#include <vector>

problemCollector_c::problemCollector_c(problem_c & problem, problemCollectorOptions_t options)
  : problem_(problem), options_(options)
{}

void problemCollector_c::resetDropMultiplicator(void) {
  dropMultiplicator_ = 1;
  syncedAssemblies_ = 0;
  syncedSolutions_ = 0;

  unsigned int a;
  if (!options_.countOnly && !options_.disassemble) {
    a = problem_.numAssembliesKnown() ? problem_.getNumAssemblies() : 0;
  } else {
    a = problem_.numSolutionsKnown() ? problem_.getNumSolutions() : 0;
  }

  if (options_.solutionLimit > 0 && options_.solutionDrop > 0) {
    while (a + options_.solutionDrop > 2 * options_.solutionLimit * options_.solutionDrop) {
      dropMultiplicator_ *= 2;
      a = (a + 1) / 2;
    }
  }
}

void problemCollector_c::consume(solverResult_t && r) {
  if (options_.disassemble)
    consumeDisassembled(std::move(r));
  else
    consumeAssemblyOnly(std::move(r));
}

void problemCollector_c::consumeAssemblyOnly(solverResult_t && r) {
  if (!options_.countOnly) {
    if (problem_.getNumAssemblies() % (options_.solutionDrop * dropMultiplicator_) == 0)
      problem_.addSolution(r.assembly.release());
  }

  problem_.incNumAssemblies();

  if (options_.solutionLimit && (problem_.getNumberOfSavedSolutions() > options_.solutionLimit)) {
    unsigned int idx = problem_.getNumAssemblies() - 1;
    idx = (idx % (options_.solutionLimit * options_.solutionDrop * dropMultiplicator_)) /
          (options_.solutionDrop * dropMultiplicator_);

    if (idx == options_.solutionLimit - 1)
      dropMultiplicator_ *= 2;

    problem_.removeSolution(idx + 1);
  }

  int ls = liveSort_.load(std::memory_order_relaxed);
  if (ls >= 0 && problem_.getNumberOfSavedSolutions() >= 2)
    problem_.sortSolutions(ls);
}

void problemCollector_c::consumeDisassembled(solverResult_t && r) {
  std::unique_ptr<assembly_c> a = std::move(r.assembly);
  std::unique_ptr<separation_c> s = std::move(r.separation);

  // When the assembly has only 1 piece it is always a solution with no move
  // list. It is stored even when only counting, as solveThread_c always did.
  if (a->placementCount() <= 1) {
    problem_.addSolution(a.release());
    problem_.incNumSolutions();
    problem_.incNumAssemblies();
    return;
  }

  if (!s) {
    problem_.incNumAssemblies();
    return;
  }

  if (!options_.countOnly) {
    bool ins = false;
    {
      std::unique_lock<std::recursive_mutex> solGuard = problem_.lockSolutions();

      switch (options_.sortMethod) {
        case problemCollectorOptions_t::SRT_COMPLETE_MOVES: {
          unsigned int lev = s->sumMoves();

          for (unsigned int i = 0; i < problem_.getNumberOfSavedSolutions(); i++) {
            const disassembly_c * s2 = problem_.getSavedSolution(i)->getDisassembly();

            if (s2 && s2->sumMoves() > lev) {
              if (options_.dropDisassemblies)
                problem_.addSolution(a.release(), new separationInfo_c(s.get()), i);
              else
                problem_.addSolution(a.release(), s.release(), i);
              ins = true;
              break;
            }
          }

          if (!ins) {
            if (options_.dropDisassemblies)
              problem_.addSolution(a.release(), new separationInfo_c(s.get()));
            else
              problem_.addSolution(a.release(), s.release());
          }

          if (options_.solutionLimit && (problem_.getNumberOfSavedSolutions() > options_.solutionLimit))
            problem_.removeSolution(0);
          break;
        }

        case problemCollectorOptions_t::SRT_LEVEL: {
          for (unsigned int i = 0; i < problem_.getNumberOfSavedSolutions(); i++) {
            const disassembly_c * s2 = problem_.getSavedSolution(i)->getDisassemblyInfo();

            if (s2 && (s2->compare(s.get()) > 0)) {
              if (options_.dropDisassemblies)
                problem_.addSolution(a.release(), new separationInfo_c(s.get()), i);
              else
                problem_.addSolution(a.release(), s.release(), i);
              ins = true;
              break;
            }
          }

          if (!ins) {
            if (options_.dropDisassemblies)
              problem_.addSolution(a.release(), new separationInfo_c(s.get()));
            else
              problem_.addSolution(a.release(), s.release());
          }

          if (options_.solutionLimit && (problem_.getNumberOfSavedSolutions() > options_.solutionLimit))
            problem_.removeSolution(0);
          break;
        }

        case problemCollectorOptions_t::SRT_UNSORT:
          if (problem_.getNumSolutions() % (options_.solutionDrop * dropMultiplicator_) == 0) {
            if (options_.dropDisassemblies)
              problem_.addSolution(a.release(), new separationInfo_c(s.get()));
            else
              problem_.addSolution(a.release(), s.release());
          }
          break;
      }
    }

    problem_.incNumSolutions();
  } else {
    problem_.incNumSolutions();
  }

  problem_.incNumAssemblies();

  if (options_.solutionLimit && (problem_.getNumberOfSavedSolutions() > options_.solutionLimit)) {
    unsigned int idx = problem_.getNumSolutions() - 1;

    idx = (idx % (options_.solutionLimit * options_.solutionDrop * dropMultiplicator_)) /
          (options_.solutionDrop * dropMultiplicator_);

    if (idx == options_.solutionLimit - 1)
      dropMultiplicator_ *= 2;

    problem_.removeSolution(idx + 1);
  }

  int ls = liveSort_.load(std::memory_order_relaxed);
  if (ls >= 0 && problem_.getNumberOfSavedSolutions() >= 2)
    problem_.sortSolutions(ls);
}

void problemCollector_c::syncCountOnly(uint64_t sessionAssemblies, uint64_t sessionSolutions) {
  while (syncedAssemblies_ < sessionAssemblies) {
    problem_.incNumAssemblies();
    syncedAssemblies_++;
  }
  while (syncedSolutions_ < sessionSolutions) {
    problem_.incNumSolutions();
    syncedSolutions_++;
  }
}

size_t problemCollector_c::drainFrom(solverSession_c & session, size_t maxItems) {
  std::vector<solverResult_t> batch;
  size_t n = session.drain(batch, maxItems);
  for (auto & r : batch)
    consume(std::move(r));
  return n;
}

void problemCollector_c::consumeAll(solverSession_c & session) {
  while (auto r = session.pop())
    consume(std::move(*r));
}
