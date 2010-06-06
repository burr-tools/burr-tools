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
#ifndef __THREAD_H__
#define __THREAD_H__

#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

/* this class encapsulates a single thread */
class thread_c {

  private:

#ifdef WIN32
    HANDLE id;
#else
    pthread_t id;
#endif

    bool running;

  public:

    /* create the thread data structure, but don't start the thread */
    thread_c(void) : running(false) {}

    /* try to stop the thread, if that doesn't work, kill it and then
     * delete data structur
     */
    virtual ~thread_c(void);

    /* run the thread return true on success */
    bool start();

    /* inform the thread to stop running, this is dependent on the thread */
    virtual void stop() {};

    /* kill the thread */
    void kill();

    /** return true, if the thread is running */
    bool isRunning(void) { return running; }

  protected:

    /* this is the function that gets started for the thread, once this
     * function finishes, the thread will end
     */
    virtual void run(void) = 0;

#ifdef WIN32
    friend unsigned long __stdcall start_thread(void * dat);
#else
    friend void * start_thread(void * dat);
#endif

  private:

    // no copying and assigning
    thread_c(const thread_c&);
    void operator=(const thread_c&);

};

#endif
