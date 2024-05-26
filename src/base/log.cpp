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

#include "log.hpp"

#include "error.hpp"

#include <chrono>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

struct log_msg
{
    log::level lvl;
    std::thread::id tid;
    uint32_t time;
    std::string msg;

    log_msg(log::level l, std::string m)
        : lvl(l)
        , tid(std::this_thread::get_id())
        , time(
              std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                  .count())
        , msg(std::move(m))
    {
    }

    [[nodiscard]] auto pretty_print() const -> std::string
    {
        std::ostringstream oss;
        switch (lvl) {
            case log::level::WARNING:
                oss << "\033[1;31m";
                break;
            case log::level::INFO:
                oss << "\033[1;34m";
                break;
            case log::level::SYSINFO:
                oss << "\033[1;33m";
                break;
            case log::level::DEBUGGING:
                oss << "\033[1;32m";
                break;
            default:
                oss << "\033[0m";
        }
        oss << "[" << log::instance().get_thread_name(tid) << "] <" << std::dec << time << "> " << msg << "\033[0m";
        return oss.str();
    }

    [[nodiscard]] auto pretty_print_console() const -> std::string
    {
        std::ostringstream oss;
        switch (lvl) {
            case log::level::WARNING:
                oss << "$ff8080";
                break;
            case log::level::INFO:
                oss << "$c0c0ff";
                break;
            case log::level::SYSINFO:
                oss << "$ffff00";
                break;
            case log::level::DEBUGGING:
                oss << "$b0ffb0";
                break;
            default:
                oss << "$c0c0c0";
        }
        oss << "[" << log::instance().get_thread_name(tid) << "] <" << std::dec << time << "> " << msg;
        return oss.str();
    }
};

class log_internal
{
  public:
    std::mutex mtx;
    std::vector<log_msg> loglines;
    std::unordered_map<std::thread::id, const char*> threadnames;
    log_internal() = default;
};

log::log()
{
    mylogint                                          = new log_internal();
    mylogint->threadnames[std::this_thread::get_id()] = "__main__";
}

bool log::copy_output_to_console = false;

void log::append(log::level l, const std::string& msg)
{
    std::unique_lock<std::mutex> ml(mylogint->mtx);
    mylogint->loglines.emplace_back(l, msg);
    if (copy_output_to_console) {
        std::cout << mylogint->loglines.back().pretty_print() << std::endl;
    }
}

void log::write(std::ostream& out, log::level limit_level) const
{
    // process log_msg and make ANSI colored text lines of it
    std::unique_lock<std::mutex> ml(mylogint->mtx);
    for (const auto& logmsg : mylogint->loglines) {
        if (logmsg.lvl <= limit_level) {
            out << logmsg.pretty_print() << std::endl;
        }
    }
}

auto log::get_last_n_lines(unsigned n) const -> std::string
{
    std::string result;
    std::unique_lock<std::mutex> ml(mylogint->mtx);
    auto l = unsigned(mylogint->loglines.size());
    if (n > l) {
        for (unsigned k = 0; k < n - l; ++k) {
            result += "\n";
        }
        n = l;
    }
    auto it = mylogint->loglines.end();
    for (; n > 0; --n) {
        --it;
    }
    for (; it != mylogint->loglines.end(); ++it) {
        result += it->pretty_print_console() + "\n";
    }
    return result;
}

void log::new_thread(const char* name)
{
    {
        std::unique_lock<std::mutex> ml(mylogint->mtx);
        mylogint->threadnames[std::this_thread::get_id()] = name;
    }
    log_sysinfo("---------- < NEW > THREAD ----------");
}

void log::end_thread()
{
    log_sysinfo("---------- > END < THREAD ----------");
    std::unique_lock<std::mutex> ml(mylogint->mtx);
    /* Do not remove entry so it can be written to log file after the thread has
     * died (and message is still in buffer). It should never get very big... */
    //	mylogint->threadnames.erase(std::this_thread::get_id());
}

auto log::get_thread_name() const -> const char*
{
    return get_thread_name(std::this_thread::get_id());
}

auto log::get_thread_name(std::thread::id tid) const -> const char*
{
    auto it = mylogint->threadnames.find(tid);
    if (it == mylogint->threadnames.end()) {
        THROW(error, "no thread name registered for thread! BUG!");
    }
    return it->second;
}
