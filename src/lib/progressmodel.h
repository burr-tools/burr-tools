#ifndef __PROGRESS_MODEL_H__
#define __PROGRESS_MODEL_H__

/* Blends assembly and disassembly progress into a single fraction.
 *
 * Both phases are weighted by their measured cost in seconds, so the result
 * is proportional to elapsed work rather than to a count of items. That is
 * what makes `elapsed/fraction - elapsed` a usable estimate of the time
 * remaining; a fraction derived from counting completed tasks is not
 * proportional to time and produces estimates that are wrong by an order of
 * magnitude (see design/2026-09-19-solve-progress-reporting.md).
 *
 * Deliberately free of threads, puzzles and GUI so the arithmetic can be
 * tested directly.
 */
class progressModel_c {

  public:

    struct Input {
      float assemblyFraction = 0;            // a, in [0,1]
      unsigned long assembliesFound = 0;
      unsigned long disassembled = 0;
      double assemblyCostSeconds = 0;
      double disassemblyCostSeconds = 0;
    };

    struct Output {
      float fraction = 0;                    // in [0,1]

      /* WORKER-seconds, not wall seconds. Both input costs are worker-seconds
       * -- the pool sums the wall time of its completed tasks across all of
       * its workers, and solveThread_c multiplies the assembler's elapsed wall
       * time by its thread count -- so this projection, being their difference
       * scaled by the same units, is in the same currency.
       *
       * The GUI's "estimated time remaining" field is wall seconds. Showing
       * this value there without dividing by the number of workers actually
       * accruing cost would over-report by roughly that factor (N while the
       * phases overlap, and see the note in solveThread_c::getProgress() for
       * why the factor is not even constant across the disassembly tail).
       * Nothing reads this field today; it is published for a caller that
       * knows how to convert.
       *
       * Negative when unknown.
       */
      double projectedRemainingSeconds = -1;
    };

    /* Below this assembly fraction the projected total assembly count is too
     * unstable to weight the phases, and the model reports assembly only.
     */
    static constexpr float trustThreshold = 0.02f;

    static Output evaluate(const Input & in);

    /* Input::assemblyCostSeconds has to cover the whole of assemblyFraction:
     * the model projects the total assembly cost as cost/fraction, so a cost
     * that covers only part of the fraction under-projects by that ratio.
     *
     * A resumed solve cannot measure the whole of it. The assembler carries
     * its search state across the pause, so the fraction picks up where it
     * left off, but the seconds that bought the earlier part were spent by a
     * previous solve thread and are recorded nowhere -- not in a form this
     * model can use, at least: the wall time the problem records includes
     * preparation and disassembly, and says nothing about how many threads
     * were running.
     *
     * So the missing part is inferred from the only rate the current run can
     * actually measure. Getting from baseFraction to fraction cost
     * sessionCostSeconds; at that rate the whole of fraction costs
     *
     *     sessionCostSeconds * fraction / (fraction - baseFraction)
     *
     * For a fresh solve baseFraction is 0 and this is exactly
     * sessionCostSeconds. It is well conditioned as fraction approaches
     * baseFraction: the assembly term then dominates both the numerator and
     * the denominator of the blend, so evaluate() tends to report `fraction`
     * itself -- the right answer when no cost has been measured yet.
     *
     * Returns 0 when there is no basis at all (a resume that has not advanced
     * yet), which evaluate() reads as "cannot blend" and reports the assembly
     * fraction alone.
     */
    static double projectAssemblyCost(double sessionCostSeconds,
                                      float fraction, float baseFraction);
};

#endif
