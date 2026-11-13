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
#include "optimizeturns.h"

#include "../lib/disassembly.h"
#include "../lib/problem.h"
#include "../lib/puzzle.h"

#include <stdio.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define GL_SILENCE_DEPRECATION 1
#include <FL/Fl.H>
#pragma GCC diagnostic pop

static void cb_Poll_stub(void * v) { static_cast<optimizeTurnsWindow_c*>(v)->cb_Poll(); }
static void cb_Keep_stub(Fl_Widget * /*o*/, void * v) { static_cast<optimizeTurnsWindow_c*>(v)->cb_Keep(); }
static void cb_Cancel_stub(Fl_Widget * /*o*/, void * v) { static_cast<optimizeTurnsWindow_c*>(v)->cb_Cancel(); }

optimizeTurnsWindow_c::optimizeTurnsWindow_c(problem_c * pr, const solution_c * solution, const rotationSettings_t & s) :
  LFl_Double_Window(false), problem(pr), assembly(solution->getAssembly()), settings(s),
  copy(std::make_unique<separation_c>(solution->getDisassembly())), turns(solution->getDisassembly()->sumRotations())
{
  label("Optimize Turns");

  text = new LFl_Box(0, 0, 2, 1);
  text->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
  text->setMinimumSize(460, 130);

  progress = new LFl_Progress(0, 1, 2, 1);
  progress->minimum(0);
  progress->maximum(turns ? float(turns) : 1.0f);
  progress->selection_color(FL_BLUE);

  (new LFl_Box(0, 2))->setMinimumSize(0, 5);

  btnKeep = new LFl_Button("Keep", 0, 3);
  btnKeep->callback(cb_Keep_stub, this);
  btnKeep->tooltip(" Store what was measured with the solution, and give its turns the paths "
                   "with the most room. The puzzle's play and bevel stay as they are ");
  btnCancel = new LFl_Button("Cancel", 1, 3);
  btnCancel->callback(cb_Cancel_stub, this);
  for (LFl_Button * b : { btnKeep, btnCancel }) {
    b->setMinimumSize(150, 25);
    b->weight(1, 0);
  }
  btnKeep->deactivate();

  callback(cb_Cancel_stub, this);   // closing the window cancels

  char t[200];
  snprintf(t, sizeof(t), "Measuring the play and the bevel each of the %u turns needs (bisections of planner "
           "runs per turn; this can take a while)...", turns);
  text->copy_label(t);

  set_modal();

  /* the worker owns `copy` until it finishes; the GUI thread only reads
   * the atomics until then (cb_Poll) */
  worker = std::thread([this] {
    optimizeControl_t control;
    control.stop = &stop;
    control.done = &done;
    outcome = optimizeTurns(copy.get(), assembly, *problem, settings, control);
    finished = true;
  });
  Fl::add_timeout(0.1, cb_Poll_stub, this);
}

optimizeTurnsWindow_c::~optimizeTurnsWindow_c(void)
{
  stop = true;
  join();
  Fl::remove_timeout(cb_Poll_stub, this);
}

void optimizeTurnsWindow_c::join(void)
{
  if (worker.joinable())
    worker.join();
}

void optimizeTurnsWindow_c::cb_Poll(void)
{
  progress->value(float(done.load()));
  if (!finished) {
    Fl::repeat_timeout(0.1, cb_Poll_stub, this);
    return;
  }
  join();

  if (outcome.stopped) {
    hide();
    return;
  }
  showMeasured();
}

void optimizeTurnsWindow_c::showMeasured(void)
{
  const rotationGeometry::model_t model = rotationModelOf(*problem);
  char t[600];

  btnCancel->label("Close");
  if (outcome.failed || !outcome.rotations) {
    snprintf(t, sizeof(t), "A turn needs more play than the planner tries, so no play could be proven. "
             "The disassembly stays as it is.");
    text->copy_label(t);
    redraw();
    return;
  }

  /* the margin: how much the puzzle's value exceeds what is needed;
   * below 0 a turn may not work as printed */
  measured = { outcome.upper, outcome.lower, model.chamfer };
  char bevel[300];
  if (outcome.bevelFailed)
    snprintf(bevel, sizeof(bevel), "Bevel: at play %.4f no bevel makes every turn; the puzzle's bevel is %.4f.",
             model.play, model.chamfer);
  else {
    measured.hasBevel = true;
    measured.bevelUpper = outcome.bevelUpper;
    measured.bevelLower = outcome.bevelLower;
    measured.bevelPlay = model.play;
    snprintf(bevel, sizeof(bevel), "Bevel: at play %.4f it needs %.4f, not below %.4f. The puzzle's bevel "
             "is %.4f, margin %+.4f.", model.play, outcome.bevelUpper, outcome.bevelLower, model.chamfer,
             model.chamfer - outcome.bevelUpper);
  }
  snprintf(t, sizeof(t), "Play: at bevel %.4f this disassembly needs %.4f; no motion the planner tries "
           "works below %.4f. The puzzle's play is %.4f, margin %+.4f.\n\n%s",
           model.chamfer, outcome.upper, outcome.lower, model.play, model.play - outcome.upper, bevel);
  text->copy_label(t);

  measuredOk = true;
  btnKeep->activate();
  progress->value(progress->maximum());
  redraw();
}

void optimizeTurnsWindow_c::cb_Keep(void)
{
  if (!measuredOk)
    return;
  result = std::move(copy);
  hide();
}

void optimizeTurnsWindow_c::cb_Cancel(void)
{
  stop = true;
  join();
  Fl::remove_timeout(cb_Poll_stub, this);
  hide();
}
