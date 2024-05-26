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

#pragma once

#include "error.hpp"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

/// Class for a thread running a function
class thread final
{
  public:
    /// create a thread
    thread(const char* name, std::function<void()> code);

    /// Join and end a thrad
    ~thread();

    /// abort thread (do not force, just request)
    void request_abort();

    /// Was abort requested?
    [[nodiscard]] bool abort_requested() const { return abort_request; }

    /// let this thread sleep
    ///@param ms - sleep time in milliseconds
    static void sleep(unsigned ms);

    /// request if thread runs
    bool is_running();

  private:
    /// The state a thread is in
    enum class state
    {
        none,     // before start
        running,  // normal operation
        finished, // after thread has exited (can't be restarted)
        error     // Something went wrong during execution
    };

    std::thread thread_id{};
    bool abort_request{};
    state mystate{state::none};
    std::mutex state_mutex;
    std::condition_variable start_cond;
    std::string error_message; // to pass exception texts via thread boundaries
    const char* myname{};
    std::function<void()> mycode;

    void run();
    /// Get state with locking
    state get_state();
};
