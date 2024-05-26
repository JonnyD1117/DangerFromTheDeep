/*
  Danger from the Deep - Open source submarine simulation
  Copyright (C) 2003-2020  Thorsten Jordan, Luis Barrancos and others.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

// multithreading primitives: thread
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#include "thread.hpp"

#include "error.hpp"
#include "log.hpp"

#include <utility>

/// Constructor
thread::thread(const char* name, std::function<void()> code)
    : myname(name)
    , mycode(code)
{
    std::unique_lock<std::mutex> ml(state_mutex);
    thread_id = std::thread(&thread::run, this);
    // we could wait with timeout, but how long? init could take any time...
    start_cond.wait(ml);
}

/// main thread run method, catches all exceptions
void thread::run()
{
    try {
        log::instance().new_thread(myname);
        {
            std::unique_lock<std::mutex> ml(state_mutex);
            mystate = state::running;
            start_cond.notify_all();
        }
        mycode();
        log::instance().end_thread();
        // normal execution finished
        std::unique_lock<std::mutex> ml(state_mutex);
        mystate = state::finished;
    }
    catch (std::exception& e) {
        // thread execution failed
        std::unique_lock<std::mutex> ml(state_mutex);
        error_message = e.what();
        mystate       = state::error;
        return;
    }
    catch (...) {
        // thread execution failed
        std::unique_lock<std::mutex> ml(state_mutex);
        error_message = "UNKNOWN";
        mystate       = state::error;
        return;
    }
}

/// Request abort of thread
void thread::request_abort()
{
    abort_request = true;
}

/// Send abort request and join thread if it had started, then delete object
thread::~thread()
{
    try {
        // request if thread runs, in that case send abort request
        if (get_state() == state::running) {
            request_abort();
        }
        // Wait for end of thread by joining, if needed
        if (get_state() != thread::state::none) {
            thread_id.join();
        }
        // Thread finished, so no more locking needed
        if (mystate != thread::state::finished) {
            log::instance().append(
                log::level::WARNING, std::string("thread ") + myname + " aborted with error: " + error_message);
        }
    }
    catch (...) {
        log::instance().append(log::level::WARNING, std::string("Uncaught exception ending thread") + myname);
    }
}

/// Get state safely with locking
auto thread::get_state() -> state
{
    std::unique_lock<std::mutex> ml(state_mutex);
    return mystate;
}

/// let caller sleep
void thread::sleep(unsigned ms)
{
    std::this_thread::sleep_for(std::chrono::microseconds(ms));
}

/// request if thread runs
auto thread::is_running() -> bool
{
    // only reading is normally safe, but not for multi-core architectures.
    return get_state() == state::running;
}
