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

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef __OPTIMIZE_TURNS_H__
#define __OPTIMIZE_TURNS_H__

#include "Layouter.h"

#include "../lib/rotationtables.h"
#include "../lib/solution.h"

#include <atomic>
#include <memory>
#include <optional>
#include <thread>

class problem_c;
class separation_c;
class LFl_Progress;

/* Optimize Turns for one solution (burrTxt -P in the GUI): measures the
 * play its disassembly needs at the puzzle's bevel, and the bevel it needs
 * at the puzzle's play (optimizeTurns, on a worker thread, with progress
 * and Cancel), and reports both next to the puzzle's play and bevel and
 * the margin. It only reports: the puzzle's pieces stay as they are.
 *
 * The window works on a copy of the disassembly and does not touch the
 * solution: the main window shows the original while it runs. Keep hands
 * the copy back (takeDisassembly(): its turns on the paths with the most
 * room, which the animation then shows) with what was measured (play()),
 * for the caller to store with the solution.
 */
class optimizeTurnsWindow_c : public LFl_Double_Window {

  public:

    optimizeTurnsWindow_c(problem_c * problem, const solution_c * solution, const rotationSettings_t & settings);
    ~optimizeTurnsWindow_c(void);

    /* the optimized disassembly, once one is ready to be kept; nullptr
     * after Cancel or when a turn could not be measured */
    std::unique_ptr<separation_c> takeDisassembly(void) { return std::move(result); }
    /* what was measured, valid with a disassembly */
    const solutionPlay_t & play(void) const { return measured; }

    void cb_Poll(void);
    void cb_Keep(void);
    void cb_Cancel(void);

  private:

    problem_c * problem;
    const assembly_c * assembly;
    rotationSettings_t settings;
    std::unique_ptr<separation_c> copy, result;
    unsigned int turns;

    std::thread worker;
    std::atomic<bool> stop{ false };
    std::atomic<bool> finished{ false };
    std::atomic<unsigned int> done{ 0 };
    turnPlay_t outcome;
    bool measuredOk = false;

    solutionPlay_t measured;

    LFl_Box * text;
    LFl_Progress * progress;
    LFl_Button * btnKeep, * btnCancel;

    void join(void);
    void showMeasured(void);
};

#endif
