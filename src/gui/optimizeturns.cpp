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

#include <cmath>
#include <stdio.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define GL_SILENCE_DEPRECATION 1
#include <FL/Fl.H>
#pragma GCC diagnostic pop

static void cb_Poll_stub(void * v) { static_cast<optimizeTurnsWindow_c*>(v)->cb_Poll(); }
static void cb_Use_stub(Fl_Widget * /*o*/, void * v) { static_cast<optimizeTurnsWindow_c*>(v)->cb_Use(); }
static void cb_Keep_stub(Fl_Widget * /*o*/, void * v) { static_cast<optimizeTurnsWindow_c*>(v)->cb_Keep(); }
static void cb_Cancel_stub(Fl_Widget * /*o*/, void * v) { static_cast<optimizeTurnsWindow_c*>(v)->cb_Cancel(); }

optimizeTurnsWindow_c::optimizeTurnsWindow_c(problem_c * pr, const solution_c * solution, const rotationSettings_t & s) :
  LFl_Double_Window(false), problem(pr), assembly(solution->getAssembly()), settings(s),
  copy(std::make_unique<separation_c>(solution->getDisassembly())), turns(solution->getDisassembly()->sumRotations())
{
  label("Optimize Turns");

  text = new LFl_Box(0, 0, 3, 1);
  text->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
  text->setMinimumSize(420, 70);

  progress = new LFl_Progress(0, 1, 3, 1);
  progress->minimum(0);
  progress->maximum(turns ? float(turns) : 1.0f);
  progress->selection_color(FL_BLUE);

  (new LFl_Box(0, 2))->setMinimumSize(0, 5);

  btnUse = new LFl_Button("Use measured play", 0, 3);
  btnUse->callback(cb_Use_stub, this);
  btnUse->tooltip(" Make the play this disassembly was proven to work at the problem's play, "
                  "and re-plan its turns for it. The other solutions become unknown, as after "
                  "any change of the problem ");
  btnKeep = new LFl_Button("Keep problem's play", 1, 3);
  btnKeep->callback(cb_Keep_stub, this);
  btnKeep->tooltip(" Keep the problem's play; the disassembly keeps the measured play and the "
                   "turn paths with the most room ");
  btnCancel = new LFl_Button("Cancel", 2, 3);
  btnCancel->callback(cb_Cancel_stub, this);
  /* the labels change, the layout does not: room for the longest */
  for (LFl_Button * b : { btnUse, btnKeep, btnCancel }) {
    b->setMinimumSize(150, 25);
    b->weight(1, 0);
  }
  btnUse->deactivate();
  btnKeep->deactivate();

  callback(cb_Cancel_stub, this);   // closing the window cancels

  char t[200];
  snprintf(t, sizeof(t), "Measuring the play each of the %u turns needs (a bisection of planner runs per turn; "
           "this can take a while)...", turns);
  text->copy_label(t);

  set_modal();
  start(true);
}

optimizeTurnsWindow_c::~optimizeTurnsWindow_c(void)
{
  stop = true;
  join();
  Fl::remove_timeout(cb_Poll_stub, this);
}

/* the worker owns `copy` until it finishes; the GUI thread only reads the
 * atomics until then (cb_Poll) */
void optimizeTurnsWindow_c::start(bool measure)
{
  done = 0;
  finished = false;
  progress->value(0);
  worker = std::thread([this, measure] {
    optimizeControl_t control;
    control.stop = &stop;
    control.done = &done;
    control.measure = measure;
    outcome = optimizeTurns(copy.get(), assembly, *problem, settings, control);
    finished = true;
  });
  Fl::add_timeout(0.1, cb_Poll_stub, this);
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

  if (phase == MEASURING) {
    phase = MEASURED;
    showMeasured();
  } else if (outcome.failed) {
    /* a turn got no path proven at the new play (the planner's grid can
     * miss a path the measuring found): back to the problem's play, and
     * the disassembly as measured at it */
    if (usedModel)
      problem->setRotationModel(*usedModel);
    else
      problem->clearRotationModel();
    phase = FINISHED;
    result = std::move(measuredCopy);
    char t[200];
    snprintf(t, sizeof(t), "A turn could not be re-planned for play %.4f. The problem keeps play %.4f, "
             "and the disassembly the paths measured at it.", measured.upper, rotationModelOf(*problem).play);
    text->copy_label(t);
    btnCancel->label("Close");
    btnCancel->activate();
    redraw();
  } else {
    /* re-planned at the new play: done */
    phase = FINISHED;
    result = std::move(copy);
    hide();
  }
}

void optimizeTurnsWindow_c::showMeasured(void)
{
  const rotationGeometry::model_t model = rotationModelOf(*problem);
  char t[400];

  if (outcome.failed || !outcome.rotations) {
    snprintf(t, sizeof(t), "A turn needs more play than the planner tries, so no play could be proven. "
             "The disassembly stays as it is.");
    text->copy_label(t);
    btnCancel->label("Close");
    return;
  }

  measured = { outcome.upper, outcome.lower, model.chamfer };
  snprintf(t, sizeof(t), "This disassembly works at play %.4f (bevel %.4f); no motion the planner tries "
           "works below %.4f. The problem's play is %.4f.",
           outcome.upper, model.chamfer, outcome.lower, model.play);
  text->copy_label(t);

  char use[60];
  snprintf(use, sizeof(use), "Use play %.4f", outcome.upper);
  btnUse->copy_label(use);
  char keep[60];
  snprintf(keep, sizeof(keep), "Keep play %.4f", model.play);
  btnKeep->copy_label(keep);
  btnKeep->activate();
  /* nothing to gain when the problem already has that play */
  if (std::abs(outcome.upper - model.play) >= 5e-5)
    btnUse->activate();
  progress->value(progress->maximum());
  redraw();
}

void optimizeTurnsWindow_c::cb_Use(void)
{
  if (phase != MEASURED)
    return;
  rotationGeometry::model_t model = rotationModelOf(*problem);
  usedModel = problem->getRotationModel();
  model.play = outcome.upper;
  if (!problem_c::validRotationModel(model))
    return;
  measuredCopy = std::make_unique<separation_c>(copy.get());
  problem->setRotationModel(model);
  modelChanged = true;

  phase = REPLANNING;
  btnUse->deactivate();
  btnKeep->deactivate();
  btnCancel->deactivate();   // the new play is set: the re-plan has to finish
  char t[200];
  snprintf(t, sizeof(t), "Re-planning the %u turns for play %.4f...", turns, model.play);
  text->copy_label(t);
  redraw();
  start(false);
}

void optimizeTurnsWindow_c::cb_Keep(void)
{
  if (phase != MEASURED)
    return;
  phase = FINISHED;
  result = std::move(copy);
  hide();
}

void optimizeTurnsWindow_c::cb_Cancel(void)
{
  /* measuring is dropped; once the new play is set, the re-plan has to
   * finish, or the disassembly would keep paths unproven at that play */
  if (phase == REPLANNING)
    return;
  stop = true;
  join();
  Fl::remove_timeout(cb_Poll_stub, this);
  hide();
}
