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
#ifndef BT_TEST_FDCOMPAT_H
#define BT_TEST_FDCOMPAT_H

/* The descriptor calls the stdin tests use, under the names the Windows CRT
 * gives them (_dup, _close, ...), as src/tools/gzstream.cpp does: MinGW also
 * has the bare POSIX names, MSVC only deprecated ones. */

#include <cstddef>
#include <fcntl.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace btfd {

#ifdef _WIN32
  inline int dup(int fd) { return _dup(fd); }
  inline int dup2(int from, int to) { return _dup2(from, to); }
  inline int close(int fd) { return _close(fd); }
  inline int openRead(const char * file) { return _open(file, _O_RDONLY | _O_BINARY); }
  inline long write(int fd, const char * data, size_t n) {
    return _write(fd, data, static_cast<unsigned int>(n));
  }
#else
  inline int dup(int fd) { return ::dup(fd); }
  inline int dup2(int from, int to) { return ::dup2(from, to); }
  inline int close(int fd) { return ::close(fd); }
  inline int openRead(const char * file) { return ::open(file, O_RDONLY); }
  inline long write(int fd, const char * data, size_t n) {
    return static_cast<long>(::write(fd, data, n));
  }
#endif
}

#endif
