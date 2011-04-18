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
  kill();
}

void thread_c::start_thread(void)
{
  running = true;
  run();
  running = false;
}

bool thread_c::start() {

  running = true;
  thread = boost::thread(&thread_c::start_thread, this);

  bool result = thread.get_id() != boost::thread::id();

  if (!result)
  {
    running = false;
  }

  return result;
}

void thread_c::kill() {

  stop();
  thread.join();
}

