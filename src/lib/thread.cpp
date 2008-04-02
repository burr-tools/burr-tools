/* Burr Solver
 * Copyright (C) 2003-2008  Andreas Röver
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

#include <signal.h>

thread_c::~thread_c(void) {
  kill();
}

#ifdef WIN32
unsigned long __stdcall start_thread(void * dat)
#else
void* start_thread(void * dat)
#endif
{

  thread_c * th = (thread_c*)dat;

  th->running = true;
  th->run();
  th->running = false;

  return 0;
}

bool thread_c::start() {

#ifdef WIN32
  DWORD ii;
  id = CreateThread(NULL, 0, start_thread, this, 0, &ii);
  return id != NULL;
#else
  return pthread_create(&id, 0, start_thread, this) == 0;
#endif
}

void thread_c::kill() {

#ifdef WIN32
  TerminateThread(id, 0);
#else
  if (pthread_kill(id, SIGTERM) == 0) {

    pthread_join(id, 0);

    running = false;
  }
#endif
}

