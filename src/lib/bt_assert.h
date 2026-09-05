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

/* this module contains an assert that throws an exception with information about
 * the assert, this exception can be caught at the end of the program and
 * the information displayed
 */
#ifndef BT_ASSERT_H
#define BT_ASSERT_H

#include <cstring>
#include <exception>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

class assert_log_c {
  std::vector<std::string> list;

public:
  void addLine(std::string_view line) {
    list.emplace_back(line);
  }

  unsigned int lines() const { return static_cast<unsigned int>(list.size()); }
  const char * line(unsigned int l) const { return list[l].c_str(); }
};

class assert_exception : public std::exception {
public:
  const char * expr{nullptr};
  const char * file{nullptr};
  const char * function{nullptr};
  unsigned int line{0};
  unsigned int column{0};

  assert_exception(const char * e, std::source_location loc = std::source_location::current())
    : expr(e), file(loc.file_name()), function(loc.function_name()), line(loc.line()), column(loc.column()) {}

  assert_exception(const char * e, const char * f, unsigned int l, const char * fkt)
    : expr(e), file(f), function(fkt), line(l), column(0) {}

  assert_exception() = default;

  const char * what() const noexcept override {
    return expr ? expr : "assertion failure";
  }
};

extern assert_log_c * assert_log;

void bt_assert_init();

[[noreturn]] void bt_te(const char * expr, std::source_location loc = std::source_location::current());
[[noreturn]] void bt_te(const char * expr, const char * file, unsigned int line, const char * function);

#ifdef NDEBUG

#define bt_assert(...) ((void)0)
#define bt_assert2(...) ((void)(__VA_ARGS__))
#define bt_assert_line(line) ((void)0)

#else

#define bt_assert(...) ((!(__VA_ARGS__)) ? ::bt_te(#__VA_ARGS__) : (void)0)
#define bt_assert2(...) ((!(__VA_ARGS__)) ? ::bt_te(#__VA_ARGS__) : (void)0)
#define bt_assert_line(line) (::assert_log ? ::assert_log->addLine(line) : (void)0)

#endif

#endif
