/* The test programs' operator new and delete, counting for AllocCount
 * (alloccount.h). The whole family is replaced -- plain, array, nothrow and
 * sized -- so every allocation and its release go through malloc and free
 * alike (AddressSanitizer checks that they pair up). The aligned forms keep
 * the library's own pair; they are not counted. */
#include "alloccount.h"

#include <cstdlib>
#include <new>

namespace {
  // per thread, so a count sees only its own thread's work
  thread_local int t_counting = 0;
  thread_local std::size_t t_bytes = 0;
  thread_local std::size_t t_calls = 0;

  void * allocate(std::size_t n) noexcept {
    if (t_counting) {
      t_bytes += n;
      t_calls++;
    }
    return std::malloc(n ? n : 1);
  }
}

void * operator new(std::size_t n) {
  if (void * p = allocate(n))
    return p;
  throw std::bad_alloc();
}

void * operator new[](std::size_t n) {
  if (void * p = allocate(n))
    return p;
  throw std::bad_alloc();
}

void * operator new(std::size_t n, const std::nothrow_t &) noexcept {
  return allocate(n);
}

void * operator new[](std::size_t n, const std::nothrow_t &) noexcept {
  return allocate(n);
}

void operator delete(void * p) noexcept { std::free(p); }
void operator delete[](void * p) noexcept { std::free(p); }
void operator delete(void * p, std::size_t) noexcept { std::free(p); }
void operator delete[](void * p, std::size_t) noexcept { std::free(p); }
void operator delete(void * p, const std::nothrow_t &) noexcept { std::free(p); }
void operator delete[](void * p, const std::nothrow_t &) noexcept { std::free(p); }

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

  /* an allocation the optimiser cannot leave out: a new-expression may be
   * served without calling operator new at all (Clang and GCC do so), a
   * call of the function itself may not; its result escapes through a
   * volatile all the same */
  void * volatile g_escape = nullptr;

  std::size_t probeAllocation(std::size_t n) {
    const AllocCount count;
    g_escape = ::operator new(n);
    ::operator delete(g_escape);
    return count.bytes();
  }
}
