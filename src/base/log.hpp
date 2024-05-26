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

//
//  A logging implementation
//

#pragma once

#include "singleton.hpp"

#include <sstream>
#include <thread>

#ifdef DEBUG
#define log_template(x, y)                                                                                             \
    do {                                                                                                               \
        std::ostringstream oss;                                                                                        \
        oss << __FILE__ << ":" << __LINE__ << " " << x;                                                                \
        log::instance().append(log::y, oss.str());                                                                     \
    } while (0)
#define log_debug(x) log_template(x, level::DEBUGGING)
#define log_info(x)  log_template(x, level::INFO)
// use this only internally for special events
#define log_sysinfo(x) log_template(x, level::SYSINFO)
#define log_warning(x) log_template(x, level::WARNING)
#else
#define log_debug(x)                                                                                                   \
    do {                                                                                                               \
    } while (0)
#define log_info(x)                                                                                                    \
    do {                                                                                                               \
    } while (0)
#define log_sysinfo(x)                                                                                                 \
    do {                                                                                                               \
    } while (0)
#define log_warning(x)                                                                                                 \
    do {                                                                                                               \
    } while (0)
#endif

/// manager class for a global threadsafe log
class log : public singleton<class log>
{
    friend class singleton<log>;

  public:
    /// level of log message, in descending importance.
    enum class level
    {
        WARNING,
        INFO,
        SYSINFO,
        DEBUGGING,
        NR_LEVELS
    };

    /// wether log output should go to console as well
    static bool copy_output_to_console;

    /// write the log to a stream, with optional filtering of importance,
    /// threadsafe
    void write(std::ostream& out, log::level limit_level = log::level::NR_LEVELS) const;

    /// append a message to the log, threadsafe
    void append(log::level l, const std::string& msg);

    /// get the last N lines in one string with return characters after each
    /// line, threadsafe
    [[nodiscard]] std::string get_last_n_lines(unsigned n) const;

    /// report a new thread - call from its context, use 8 characters for name
    /// always for nice logs
    void new_thread(const char* name);

    /// report end of a thread - call from its context
    void end_thread();

  protected:
    log();
    class log_internal* mylogint{nullptr};
    [[nodiscard]] const char* get_thread_name() const;
    [[nodiscard]] const char* get_thread_name(std::thread::id) const;
    friend struct log_msg;
};
