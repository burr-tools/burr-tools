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

#include "thread.h"

thread_c::~thread_c(void) {
  stop();
#ifndef NO_THREADING
  t.join();
#endif
}

void thread_c::start_thread(void)
{
  running = true;
  run();
  running = false;
}

bool thread_c::start() {

  running = true;
  bool result = false;

#ifdef NO_THREADING
  start_thread();
  result = true;
#else
  t = std::thread([this](){ this->start_thread();});
  result = t.get_id() != std::this_thread::get_id();

  if (!result)
  {
    running = false;
  }
#endif

  return result;
}

