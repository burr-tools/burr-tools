#include "progressmodel.h"

#include <cmath>

double progressModel_c::projectAssemblyCost(double sessionCostSeconds,
                                            float fraction, float baseFraction) {

  if (sessionCostSeconds <= 0) return 0.0;

  float a = fraction;
  if (a < 0) a = 0;
  if (a > 1) a = 1;

  float base = baseFraction;
  if (base < 0) base = 0;
  if (base > 1) base = 1;

  if (base <= 0) return sessionCostSeconds;

  const double gained = static_cast<double>(a) - static_cast<double>(base);
  if (gained <= 0) return 0.0;

  return sessionCostSeconds * static_cast<double>(a) / gained;
}

progressModel_c::Output progressModel_c::evaluate(const Input & in) {

  Output out;

  float a = in.assemblyFraction;
  if (a < 0) a = 0;
  if (a > 1) a = 1;

  const double asmCost = in.assemblyCostSeconds;
  const double disCost = in.disassemblyCostSeconds;

  const bool canBlend = (a >= trustThreshold)
                     && (in.assembliesFound > 0)
                     && (in.disassembled > 0)
                     && (asmCost > 0)
                     && (disCost > 0);

  if (!canBlend) {
    out.fraction = a;

    /* Assembly alone cannot report completion while disassembly work is
     * still outstanding: a == 1 does not mean nothing remains to do.
     */
    if (in.assembliesFound > in.disassembled && out.fraction >= 1.0f)
      out.fraction = std::nextafter(1.0f, 0.0f);

    out.projectedRemainingSeconds =
        (a > 0 && asmCost > 0) ? (asmCost / a - asmCost) : -1.0;
    return out;
  }

  /* Projected total assemblies, hence the fraction of disassembly done. */
  const double nHat = static_cast<double>(in.assembliesFound) / a;
  double d = static_cast<double>(in.disassembled) / nHat;
  if (d > 1.0) d = 1.0;

  const double spent     = asmCost + disCost;
  const double projected = asmCost / a + disCost / d;

  out.fraction = static_cast<float>(spent / projected);
  if (out.fraction > 1.0f) out.fraction = 1.0f;

  /* worker-seconds, because both costs are -- NOT wall seconds; a caller that
   * shows this in a time-remaining field has to convert. See the declaration
   * of Output::projectedRemainingSeconds.
   */
  out.projectedRemainingSeconds = projected - spent;

  return out;
}
