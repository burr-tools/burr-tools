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
#ifndef __ASSERT_H__
#define __ASSERT_H__

#include <vector>
#include <string.h>

#include <exception>

class assert_log_c {

  std::vector<const char *> list;

public:

  void addLine(const char * line) {
    list.push_back(strdup(line));
  }

  unsigned int lines(void) const { return list.size(); }
  const char * line(unsigned int l) const { return list[l]; }

};

class assert_exception : public std::exception {

public:

  const char * expr;
  const char * file;
  const char * function;
  unsigned int line;

  assert_exception(const char * e, const char * f, unsigned int l, const char * fkt) : expr(e), file(f), function(fkt), line(l) {}

  assert_exception(void) : expr(0), file(0), function(0), line(0) {}

};

extern assert_log_c * assert_log;

void bt_assert_init(void);

void bt_te(const char * expr, const char * file, unsigned int line, const char * funktion);

#ifdef NDEBUG

#define bt_assert(expr)
#define bt_assert2(expr) expr
#define bt_assert_line(line)

#else

#if defined(WIN32) || defined(EMSCRIPTEN)
#define __STRING(s) #s
#endif

#ifdef BT_ASSERT_NO_FUNC
#define bt_assert(expr)  if (!(expr)) throw assert_exception(__STRING(expr), __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define bt_assert2(expr)  if (!(expr)) throw assert_exception(__STRING(expr), __FILE__, __LINE__, __PRETTY_FUNCTION__)
#else
#define bt_assert(expr)  if (!(expr)) bt_te(__STRING(expr), __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define bt_assert2(expr)  if (!(expr)) bt_te(__STRING(expr), __FILE__, __LINE__, __PRETTY_FUNCTION__)
#endif
#define bt_assert_line(line) assert_log->addLine(line)

#endif

#endif
