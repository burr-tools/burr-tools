/* The test programs' operator new, counting for AllocCount (alloccount.h). */
#include "alloccount.h"

#include <cstdlib>
#include <new>

namespace {
  // per thread, so a count sees only its own thread's work
  thread_local int t_counting = 0;
  thread_local std::size_t t_bytes = 0;
  thread_local std::size_t t_calls = 0;
}

void * operator new(std::size_t n) {
  if (t_counting) {
    t_bytes += n;
    t_calls++;
  }
  if (void * p = std::malloc(n ? n : 1))
    return p;
  throw std::bad_alloc();
}

void operator delete(void * p) noexcept {
  std::free(p);
}

void operator delete(void * p, std::size_t) noexcept {
  std::free(p);
}

namespace btui::test {

  AllocCount::AllocCount(void) : m_bytes0(t_bytes), m_calls0(t_calls) {
    t_counting++;
  }

  AllocCount::~AllocCount(void) {
    t_counting--;
  }

  std::size_t AllocCount::bytes(void) const {
    return t_bytes - m_bytes0;
  }

  std::size_t AllocCount::calls(void) const {
    return t_calls - m_calls0;
  }
}
