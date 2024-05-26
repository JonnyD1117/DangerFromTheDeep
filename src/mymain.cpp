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

// Danger from the Deep (C)+(W) Thorsten Jordan. SEE LICENSE
//
// system dependent main with command line translation to std::string
//
// WIN32: use WinMain, divide cmd line to list of strings
// UNIX: C-like main, make strings from char** array
// MacOSX: ? fixed with objective C code ?
//

#include "faulthandler.hpp"
#include "log.hpp"
#include "system_interface.hpp"

#include <exception>
#include <fstream>
#include <iostream>
#include <list>
#include <string>
#include <unistd.h>
using namespace std;

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

int mymain(std::vector<std::string>& args);

int call_mymain(std::vector<std::string>& args)
{
    log_info("***** Log file started *****");
    int result = 0;
#ifdef WIN32
    result = mymain(args);
#else
    try {
        result = mymain(args);
    }
    catch (std::exception& e) {
        log_warning("Caught exception: " << e.what());
        print_stack_trace();
        result = -1;
    }
    catch (...) {
        log_warning("Caught unknown exception");
        print_stack_trace();
        result = -2;
    }
#endif

    std::string log_file =
#ifdef WIN32
        "./debug.log";
#else
        // fixme: use global /var/games instead
        std::string(getenv("HOME")) + "/.dangerdeep/debug.log";
#endif
    log::instance().write(std::cerr, log::level::SYSINFO);
    unlink(log_file.c_str());
    std::ofstream f(log_file.c_str());
    log::instance().write(f, log::level::SYSINFO);
    return result;
}

#ifdef WIN32

int WinMain(HINSTANCE, HINSTANCE, LPSTR cmdline, int)
{
    std::string mycmdline(cmdline);
    std::vector<std::string> args;
    // parse mycmdline
    while (mycmdline.length() > 0) {
        std::string::size_type st = mycmdline.find(" ");
        args.push_back(mycmdline.substr(0, st));
        if (st == std::string::npos)
            break;
        mycmdline = mycmdline.substr(st + 1);
    }
    return call_mymain(args);
}

#else // UNIX

int main(int argc, char** argv)
{
    std::vector<std::string> args;
    // parse argc, argv, do not store program name
    for (int i = 1; i < argc; ++i) {
        args.push_back(std::string(argv[i]));
    }
    return call_mymain(args);
}

#endif
