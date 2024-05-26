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

// Danger from the Deep, helper functions for stack trace or SIGSEGV handling
// (C)+(W) by Thorsten Jordan. See LICENSE

#pragma once

/*
Win32 and MacOsX do not suppport backtracking
*/

#if (defined(__APPLE__) && defined(__MACH__)) || defined MINGW32

#include <stdio.h>

inline void print_stack_trace()
{
    printf("Stack backtracing not supported on Win32 and MacOSX systems.\n");
}

void install_segfault_handler()
{
    printf("SIGSEGV catching not supported on Win32 and MacOSX systems.\n");
}

#elif defined WIN32

#include "dbghelp.hpp"
#include "log.hpp"

#include <fstream>
#include <shlobj.h>
#include <windows.h>

typedef BOOL(WINAPI* MINIDUMPWRITEDUMP)(
    HANDLE hprocess,
    DWORD pid,
    HANDLE hfile,
    MINIDUMP_TYPE dumptype,
    CONST PMINIDUMP_EXCEPTION_INFORMATION exceptionparam,
    CONST PMINIDUMP_USER_STREAM_INFORMATION userstreamparam,
    CONST PMINIDUMP_CALLBACK_INFORMATION callbackparam);
inline void print_stack_trace()
{
    printf("Stack backtracing not supported on Win32 and MacOSX systems.\n");
}

static LONG WINAPI DangerdeepCrashDump(struct _EXCEPTION_POINTERS* pexceptioninfo)
{
    std::ofstream f("log.txt");
    log::instance().write(f, log::LOG_SYSINFO);

    MINIDUMPWRITEDUMP m_dump;
    static HANDLE m_hfile;
    static HMODULE m_hdll;
#ifdef UNICODE
    wchar_t path[MAX_PATH + 1];
    wchar_t file[MAX_PATH + 1];
#else
    char path[MAX_PATH + 1];
    char file[MAX_PATH + 1];
#endif
    BOOL ok;
    SYSTEMTIME sysTime = {0};
    GetSystemTime(&sysTime);

    SHGetFolderPath(nullptr, CSIDL_APPDATA, nullptr, 0, path);

#ifdef UNICODE
    _snwprintf_s(
        file,
        MAX_PATH,
        TEXT("\\dangerdeep-%04u-%02u-%02u_%02u-%02u-%02u.dmp"),
        sysTime.wYear,
        sysTime.wMonth,
        sysTime.wDay,
        sysTime.wHour,
        sysTime.wMinute,
        sysTime.wSecond);
    std::wstring foo = path;
#else
    _snprintf_s(
        file,
        MAX_PATH,
        TEXT("\\dangerdeep-%04u-%02u-%02u_%02u-%02u-%02u.dmp"),
        sysTime.wYear,
        sysTime.wMonth,
        sysTime.wDay,
        sysTime.wHour,
        sysTime.wMinute,
        sysTime.wSecond);
    std::string foo = path;
#endif
    foo              = foo + file;
    //	foo = foo + "\\dangerdeep.dmp";

    _MINIDUMP_EXCEPTION_INFORMATION exinfo;
    exinfo.ThreadId          = ::GetCurrentThreadId();
    exinfo.ExceptionPointers = pexceptioninfo;
    exinfo.ClientPointers    = FALSE;

    m_hfile =
        CreateFile(foo.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);

    m_hdll = LoadLibrary(TEXT("DBGHELP.DLL"));

    m_dump = (MINIDUMPWRITEDUMP)::GetProcAddress(m_hdll, "MiniDumpWriteDump");

    ok  = m_dump(GetCurrentProcess(), GetCurrentProcessId(), m_hfile, MiniDumpNormal, &exinfo, nullptr, nullptr);

#ifdef UNICODE
    foo = std::wstring(TEXT("Please send the following file to the developers: ")) + foo;
#else
    foo             = std::string(TEXT("Please send the following file to the developers: ")) + foo;
#endif

    if (ok) {
        MessageBox(nullptr, foo.c_str(), TEXT("Core dumped"), MB_OK | MB_TASKMODAL | MB_ICONERROR);
        return EXCEPTION_EXECUTE_HANDLER;
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

void install_segfault_handler()
{
    SetUnhandledExceptionFilter(DangerdeepCrashDump);
}

#else // non-WIN32-MacOSX

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cxxabi.h> // Needed for __cxa_demangle
#include <execinfo.h>
#include <list>
#include <sstream>
#include <string>
#include <unistd.h>

// Note: use --export-dynamic as linker option or you won't get function names
// here.

inline void print_stack_trace()
{
    void* array[16];
    int size = backtrace(array, 16);

    if (size < 0) {
        fprintf(stderr, "Backtrace could not be created!\n");
        return;
    }

    char** strings = backtrace_symbols(array, size);
    if (!strings) {
        fprintf(stderr, "Could not get Backtrace symbols!\n");
        return;
    }

    fprintf(stderr, "Stack trace: (%u frames)\n", size);
    std::string addrs;
    std::list<std::string> lines;

    for (int i = 0; i < size; ++i) {
        std::string addr;
        std::string s(strings[i]);
        std::string::size_type p1 = s.rfind('[');
        std::string::size_type p2 = s.rfind(']');

        if ((p1 != std::string::npos) && (p2 != std::string::npos)) {
            addr = s.substr(p1 + 1, p2 - p1 - 1);
            addrs += addr + " ";
        }

        p1 = s.rfind("_Z");
        p2 = s.rfind('+');

        if (p2 == std::string::npos)
            p2 = s.rfind(')');

        std::string func;

        if (p1 != std::string::npos) {
            func       = s.substr(p1, p2 - p1);
            int status = 0;

            char* c = abi::__cxa_demangle(func.c_str(), nullptr, nullptr, &status);

            if (c)
                func = c;
            else
                func = "???";
        } else {
            p1 = s.rfind('(');

            if (p1 != std::string::npos) {
                func = s.substr(p1 + 1, p2 - p1 - 1);
            } else {
                p2   = s.rfind('[');
                func = s.substr(0, p2 - 1);
            }
        }
        lines.push_back(addr + " in " + func);
        if (func == "main")
            break;
    }
    free(strings);

    // determine lines from addresses
    std::ostringstream oss;
    oss << "addr2line -e /proc/" << getpid() << "/exe -s " << addrs;
    FILE* f = popen(oss.str().c_str(), "r");

    if (f) {
        for (auto& line : lines) {
            char tmp[128];
            if (nullptr == fgets(tmp, 127, f))
                break;
            fprintf(stderr, "%s at %s", line.c_str(), tmp);
        }
        pclose(f);
    } else {
        for (auto& line : lines) {
            fprintf(stderr, "%s\n", line.c_str());
        }
    }
}

void sigsegv_handler(int)
{
    fprintf(stderr, "SIGSEGV caught!\n");
    print_stack_trace();
    fprintf(stderr, "Aborting program.\n");
    abort();
}

void install_segfault_handler()
{
    signal(SIGSEGV, sigsegv_handler);
}

#endif // WIN32 || MacOSX
