#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "lib/progressmodel.h"

#include <cmath>

namespace {

/* A synthetic solve: 100 s of assembly and 100 s of disassembly over 1000
 * assemblies, sampled at a given fraction through the assembly search.
 */
progressModel_c::Input sample(float a, double disFractionOfFound) {
  progressModel_c::Input in;
  in.assemblyFraction        = a;
  in.assembliesFound         = static_cast<unsigned long>(1000.0 * a);
  in.disassembled            = static_cast<unsigned long>(1000.0 * a * disFractionOfFound);
  in.assemblyCostSeconds     = 100.0 * a;
  in.disassemblyCostSeconds  = 100.0 * a * disFractionOfFound;
  return in;
}

/* A solve whose projected total cost REVISES as it goes, which sample() above
 * deliberately does not do: there the projected total is 200 s at every point
 * of the sweep, so the blend reduces to 0.9a and a monotonicity check over it
 * cannot fail for the reason monotonicity is actually at risk.
 *
 * Here the assembly cost is quadratic in a -- later subtrees are more
 * expensive than the first ones, which is the normal shape of a DLX search --
 * so the projected assembly cost Ca/a climbs from 1 s to 100 s across the
 * sweep and the projected TOTAL climbs from ~101 s to 200 s. Disassembly is
 * unchanged from sample(): 80% of what has been found, at 0.1 s each.
 */
progressModel_c::Input revisingSample(float a) {
  progressModel_c::Input in;
  in.assemblyFraction        = a;
  in.assembliesFound         = static_cast<unsigned long>(1000.0 * a);
  in.disassembled            = static_cast<unsigned long>(1000.0 * a * 0.8);
  in.assemblyCostSeconds     = 100.0 * a * a;
  in.disassemblyCostSeconds  = 100.0 * a * 0.8;
  return in;
}

double projectedTotal(const progressModel_c::Input & in) {
  return progressModel_c::evaluate(in).projectedRemainingSeconds
       + in.assemblyCostSeconds + in.disassemblyCostSeconds;
}

}

TEST_CASE("progress model blends both phases into a time-proportional fraction",
          "[progress]") {
  /* Halfway through assembly, with 80% of found assemblies disassembled:
   * projected total cost is 100/0.5 ... no: asmCost/a = 50/0.5 = 100,
   * disCost/d = 40/0.4 = 100, so 200 s projected, 90 s spent.
   */
  auto out = progressModel_c::evaluate(sample(0.5f, 0.8));

  CHECK(out.fraction > 0.44f);
  CHECK(out.fraction < 0.46f);
  CHECK(std::fabs(out.projectedRemainingSeconds - 110.0) < 1.0);

  /* 90% through: 90 s assembly + 72 s disassembly spent of 200 s projected.
   * This is the point at which the old task-count fraction was worst — it
   * predicted 2.7 s while 30.3 s remained.
   */
  auto late = progressModel_c::evaluate(sample(0.9f, 0.8));
  CHECK(std::fabs(late.projectedRemainingSeconds - 38.0) < 1.0);
  CHECK(late.fraction > out.fraction);
}

TEST_CASE("progress model falls back to assembly-only when untrustworthy",
          "[progress]") {
  SECTION("assembly fraction below the trust threshold") {
    auto in = sample(progressModel_c::trustThreshold / 2.0f, 0.8);
    CHECK(progressModel_c::evaluate(in).fraction == in.assemblyFraction);
  }

  SECTION("nothing disassembled yet") {
    auto in = sample(0.5f, 0.0);
    CHECK(progressModel_c::evaluate(in).fraction == in.assemblyFraction);
  }
}

TEST_CASE("progress model is monotone and bounded", "[progress]") {
  float previous = 0;
  for (int i = 1; i <= 100; i++) {
    auto out = progressModel_c::evaluate(sample(i / 100.0f, 0.8));
    CHECK(out.fraction >= previous);
    CHECK(out.fraction >= 0.0f);
    CHECK(out.fraction <= 1.0f);
    previous = out.fraction;
  }

  /* Why the sweep above is not enough on its own: its projected total cost is
   * the same 200 s at every point, so the blend reduces to 0.9a and the loop
   * can only fail if a itself goes backwards. A revising projection is the
   * thing that actually puts monotonicity at risk -- the fraction is
   * spent/projected and the denominator moves too.
   */
  CHECK(projectedTotal(sample(0.1f, 0.8)) == projectedTotal(sample(1.0f, 0.8)));
}

TEST_CASE("progress model stays monotone while the projection revises",
          "[progress]") {

  /* the projection this sweep is built on really does move: the projected
   * total roughly doubles between the start and the end of it
   */
  const double early = projectedTotal(revisingSample(0.1f));
  const double late  = projectedTotal(revisingSample(1.0f));
  INFO("projected total: " << early << " s early, " << late << " s late");
  CHECK(late > early * 1.5);

  float previous = 0;
  for (int i = 1; i <= 100; i++) {
    auto out = progressModel_c::evaluate(revisingSample(i / 100.0f));
    INFO("a = " << i / 100.0f << " fraction = " << out.fraction);
    CHECK(out.fraction >= previous);
    CHECK(out.fraction >= 0.0f);
    CHECK(out.fraction <= 1.0f);
    previous = out.fraction;
  }

  /* and it is not the trivially-monotone 0.9a curve: at the halfway point the
   * two sweeps disagree by more than a rounding error
   */
  CHECK(progressModel_c::evaluate(revisingSample(0.5f)).fraction
        < progressModel_c::evaluate(sample(0.5f, 0.8)).fraction - 0.01f);
}

/* Characterises the model, and pins why solveThread_c::getProgress() carries a
 * monotone guard of its own rather than relying on this one.
 *
 * evaluate() is a cost-weighted mean of the two phase fractions, so a
 * projection that revises the DISASSEMBLY cost upward shifts weight onto the
 * phase that is further behind, and the blend falls. Here 800 assemblies
 * disassemble in 0.1 s each and the 801st takes 220 s on its own -- one
 * assembly with a huge movement tree, an ordinary thing for a puzzle to have.
 * If this ever stops holding, the guard in solvethread.cpp has become
 * unnecessary and the comment there should say so.
 */
TEST_CASE("progress model is not monotone when the disassembly projection revises",
          "[progress]") {
  progressModel_c::Input first;
  first.assemblyFraction       = 0.9f;
  first.assembliesFound        = 900;
  first.disassembled           = 800;
  first.assemblyCostSeconds    = 90.0;
  first.disassemblyCostSeconds = 80.0;

  progressModel_c::Input second;
  second.assemblyFraction       = 0.91f;
  second.assembliesFound        = 910;
  second.disassembled           = 801;   /* one more, and it took 220 s */
  second.assemblyCostSeconds    = 91.0;
  second.disassemblyCostSeconds = 300.0;

  const float a = progressModel_c::evaluate(first).fraction;
  const float b = progressModel_c::evaluate(second).fraction;
  INFO("blend went " << a << " -> " << b);
  CHECK(b < a);
}

TEST_CASE("progress model reaches 1.0 only when both phases are complete",
          "[progress]") {
  CHECK(progressModel_c::evaluate(sample(1.0f, 0.5)).fraction < 1.0f);
  CHECK(progressModel_c::evaluate(sample(1.0f, 1.0)).fraction > 0.999f);

  /* Assembly complete, disassembly not started: the fallback-to-assembly
   * path must not report completion while disassembly work is outstanding.
   */
  CHECK(progressModel_c::evaluate(sample(1.0f, 0.0)).fraction < 1.0f);

  /* A puzzle with no assemblies at all: nothing is outstanding, so
   * reporting 1.0 is correct. Built directly rather than via sample(),
   * since sample() always derives a non-zero count from a non-zero a.
   */
  progressModel_c::Input noAssemblies;
  noAssemblies.assemblyFraction = 1.0f;
  noAssemblies.assembliesFound  = 0;
  noAssemblies.disassembled     = 0;
  CHECK(progressModel_c::evaluate(noAssemblies).fraction == 1.0f);
}

/* Characterises a known limitation rather than a desired behaviour.
 *
 * solveThread_c::getProgress() clamps a rounded-up assemblyFraction to the
 * largest float below 1 while assembly is demonstrably still running, so the
 * model is never told "assembly complete" on a float rounding artefact. That
 * clamp makes the input honest, but it does not change what the caller ends up
 * reporting: with the pool drained, the blend is 1.0f before the clamp and
 * 0.99999994f after, and getProgress() caps anything above 0.999f to 0.999f.
 * Both therefore latch the bar at 99.9% for the rest of the solve.
 *
 * Pinned here so the limitation stays checkable: if a future change to
 * evaluate() makes the clamped case land meaningfully below the cap, this test
 * fails and the comment in solvethread.cpp needs revisiting.
 */
TEST_CASE("clamping a rounded-up assembly fraction does not lower a drained blend",
          "[progress]") {
  progressModel_c::Input in;
  in.assembliesFound         = 1000;
  in.disassembled            = 1000;   /* pool drained */
  in.assemblyCostSeconds     = 2264;
  in.disassemblyCostSeconds  = 500;

  in.assemblyFraction = 1.0f;
  const float unclamped = progressModel_c::evaluate(in).fraction;

  in.assemblyFraction = std::nextafter(1.0f, 0.0f);
  const float clamped = progressModel_c::evaluate(in).fraction;

  CHECK(unclamped == 1.0f);
  CHECK(clamped < 1.0f);
  CHECK(clamped == std::nextafter(1.0f, 0.0f));

  /* the point: both are still above the 0.999 cap getProgress() applies */
  CHECK(unclamped > 0.999f);
  CHECK(clamped > 0.999f);
}

/* The cost basis a resumed solve reports. A solveThread_c is built per solve
 * and destroyed on pause, so the seconds that bought the assembly fraction the
 * assembler carries across the pause are gone; only the fraction survives.
 * Feeding the model the current run's seconds against the whole of that
 * fraction under-projects the assembly phase by exactly the ratio of the two.
 */
TEST_CASE("assembly cost is projected across a resume", "[progress]") {

  SECTION("a fresh solve is charged exactly what it spent") {
    CHECK(progressModel_c::projectAssemblyCost(37.5, 0.4f, 0.0f) == 37.5);
    CHECK(progressModel_c::projectAssemblyCost(37.5, 1.0f, 0.0f) == 37.5);
  }

  SECTION("a resume is charged for the head start at the rate it can measure") {
    /* 10 worker-seconds bought 0.6 -> 0.8, a quarter of the search, so the
     * whole of 0.8 costs 40 at that rate
     */
    CHECK(progressModel_c::projectAssemblyCost(10.0, 0.8f, 0.6f)
          == Catch::Approx(40.0));
  }

  SECTION("no basis yet means no cost, not a wrong one") {
    /* a resume that has not advanced: nothing measured, so the model is told
     * nothing and reports the assembly fraction alone
     */
    CHECK(progressModel_c::projectAssemblyCost(10.0, 0.6f, 0.6f) == 0.0);
    CHECK(progressModel_c::projectAssemblyCost(10.0, 0.5f, 0.6f) == 0.0);
    CHECK(progressModel_c::projectAssemblyCost(0.0, 0.8f, 0.6f) == 0.0);
  }

  SECTION("it degrades to the assembly fraction as the measured span shrinks") {
    /* The near-degenerate case is not a blow-up. As the span approaches zero
     * the projected assembly cost dominates both sides of the blend, so the
     * reported fraction tends to the assembly fraction itself -- the right
     * answer when there is nothing else to go on.
     */
    progressModel_c::Input in;
    in.assemblyFraction       = 0.8f;
    in.assembliesFound        = 800;
    in.disassembled           = 600;
    in.disassemblyCostSeconds = 50.0;

    in.assemblyCostSeconds =
        progressModel_c::projectAssemblyCost(10.0, 0.8f, 0.79999f);
    CHECK(progressModel_c::evaluate(in).fraction == Catch::Approx(0.8f).margin(0.01));
  }

  SECTION("it restores the assembly phase's weight in the blend") {
    progressModel_c::Input in;
    in.assemblyFraction       = 0.8f;
    in.assembliesFound        = 800;
    in.disassembled           = 600;
    in.disassemblyCostSeconds = 50.0;

    /* what the phase cost this run alone: assembly is weighted 10/0.8 = 12.5 s
     * against disassembly's 50/0.6 = 83.3 s, i.e. almost ignored
     */
    in.assemblyCostSeconds = 10.0;
    const float unprojected = progressModel_c::evaluate(in).fraction;

    /* projected across the resume: 40 s of assembly, weighted 50 s against the
     * same 83.3 s, which is the share the phase actually earned
     */
    in.assemblyCostSeconds =
        progressModel_c::projectAssemblyCost(10.0, 0.8f, 0.6f);
    const float projected = progressModel_c::evaluate(in).fraction;

    INFO("unprojected " << unprojected << " projected " << projected);
    CHECK(projected > unprojected);
  }
}
