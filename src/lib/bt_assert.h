/* Burr Solver
 * Copyright (C) 2003-2005  Andreas Röver
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

class assert_log_c {

  std::vector<const char *> list;

public:

  void addLine(const char * line) {
    list.push_back(strdup(line));
  }

  unsigned int lines(void) const { return list.size(); }
  const char * line(unsigned int l) const { return list[l]; }

};

class assert_exception {

public:

  const char * expr;
  const char * file;
  const char * function;
  unsigned int line;

  assert_exception(const char * e, const char * f, unsigned int l, const char * fkt) : expr(e), file(f), function(fkt), line(l) {}

};

extern assert_log_c * assert_log;

void bt_assert_init(void);

#ifdef NDEBUG

#define bt_assert(expr)
#define bt_assert_line(line)

#else

#define bt_assert(expr)  (expr) ? 0 : throw new assert_exception(__STRING(expr), __FILE__, __LINE__, __PRETTY_FUNCTION__)
#define bt_assert_line(line) assert_log->addLine(line)

#endif

#endif
