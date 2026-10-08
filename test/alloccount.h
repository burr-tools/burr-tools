/* What the program's own code allocates, for the performance tests' "this
 * allocates nothing per frame" checks.
 *
 * alloccount.cpp replaces the global operator new of the test program it is
 * linked into (test_burrtools, test_qtgui). While an AllocCount lives, the
 * allocations made on its thread are counted. Only the program's own code
 * is seen: on Windows a DLL -- Qt -- keeps its own operator new, which is
 * what these checks want (our containers, not Qt's internals).
 */
#ifndef BTTEST_ALLOCCOUNT_H
#define BTTEST_ALLOCCOUNT_H

#include <cstddef>

namespace btui::test {

  class AllocCount {
  public:
    AllocCount(void);
    ~AllocCount(void);
    AllocCount(const AllocCount &) = delete;
    AllocCount & operator=(const AllocCount &) = delete;

    std::size_t bytes(void) const;     ///< allocated since this was made
    std::size_t calls(void) const;     ///< operator new calls since this was made

  private:
    std::size_t m_bytes0, m_calls0;
  };
}

#endif
