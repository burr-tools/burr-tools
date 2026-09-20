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
      double projectedRemainingSeconds = -1; // negative when unknown
    };

    /* Below this assembly fraction the projected total assembly count is too
     * unstable to weight the phases, and the model reports assembly only.
     */
    static constexpr float trustThreshold = 0.02f;

    static Output evaluate(const Input & in);
};

#endif
