/* The rule for tests that draw through a real graphics API (offscreen QRhi).
 *
 * Where no backend can be made they are skipped -- a developer machine
 * without one still runs everything else. CI sets BURRTOOLS_REQUIRE_GPU_TESTS
 * where a backend is known to exist (Direct3D WARP on Windows, Mesa's
 * software Vulkan on Linux), and there a missing backend fails instead, so a
 * broken setup cannot pass by skipping every render.
 */
#ifndef BTTEST_TEST_GPU_H
#define BTTEST_TEST_GPU_H

#include <QTest>
#include <QtGlobal>

inline bool gpuTestsRequired(void) {
  return qEnvironmentVariableIsSet("BURRTOOLS_REQUIRE_GPU_TESTS");
}

#define GPU_OR_SKIP(available)                                                              \
  do {                                                                                      \
    if (!(available)) {                                                                     \
      if (gpuTestsRequired())                                                               \
        QFAIL("no offscreen graphics backend, and BURRTOOLS_REQUIRE_GPU_TESTS is set");     \
      QSKIP("no offscreen graphics backend");                                               \
    }                                                                                       \
  } while (0)

#endif
