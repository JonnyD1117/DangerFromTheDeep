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

// file helper functions
// (C)+(W) Thorsten Jordan. SEE LICENSE

#include "filehelper.hpp"

#include "error.hpp"

#include <fstream>
#include <vector>

static const char* PATHSEPARATOR = "/"; // just use it on all systems.

#ifdef WIN32

inline std::wstring convertUTF8toUTF16(const std::string& s)
{
    int needed = MultiByteToWideChar(CP_ACP, 0, s.c_str(), int(s.length() + 1), 0, 0);
    std::wstring result(needed, wchar_t(0));
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), int(s.length() + 1), &result[0], needed);
    return std::move(result);
}
inline std::string convertUTF16toUTF8(const std::wstring& s)
{
    int needed = WideCharToMultiByte(CP_ACP, 0, s.c_str(), int(s.length() + 1), 0, 0, 0, 0);
    std::string result(needed, char(0));
    WideCharToMultiByte(CP_ACP, 0, s.c_str(), int(s.length() + 1), &result[0], needed, 0, 0);
    return std::move(result);
}

directory::directory(const std::string& filename)
    : temporary_used(true)
{
    if (!is_directory(filename)) {
        THROW(error, std::string("no directory: ") + filename);
    }
#ifdef UNICODE
    dir = FindFirstFile(convertUTF8toUTF16(filename + "*.*").c_str(), &Win32_FileFind_Temporary);
#else
    dir = FindFirstFile((filename + "*.*").c_str(), &Win32_FileFind_Temporary);
#endif
    if (!dir)
        THROW(error, std::string("Can't open directory ") + filename);
}

std::string directory::read()
{
    if (temporary_used) {
        temporary_used = false;
#ifdef UNICODE
        return convertUTF16toUTF8(std::wstring(Win32_FileFind_Temporary.cFileName));
#else
        return std::string(Win32_FileFind_Temporary.cFileName);
#endif
    } else {
        BOOL b = FindNextFile(dir, &Win32_FileFind_Temporary);
        if (b == TRUE)
#ifdef UNICODE
            return convertUTF16toUTF8(std::wstring(Win32_FileFind_Temporary.cFileName));
#else
            return std::string(Win32_FileFind_Temporary.cFileName);
#endif
        else
            return std::string();
    }
}

directory::~directory()
{
    FindClose(dir);
}

bool make_dir(const std::string& dirname)
{
#ifdef UNICODE
    return (CreateDirectory(convertUTF8toUTF16(dirname).c_str(), nullptr) == TRUE);
#else
    return (CreateDirectory(dirname.c_str(), nullptr) == TRUE);
#endif
}

std::string get_current_directory()
{
    unsigned sz = 256;
#ifdef UNICODE
    std::wstring s(sz, wchar_t(0));
#else
    std::string s(sz, char(0));
#endif
    DWORD dw = 0;
    while (dw == 0) {
        dw = GetCurrentDirectory(sz, &s[0]);
        if (dw == 0) {
            sz += sz;
#ifdef UNICODE
            s = std::wstring(sz, wchar_t(0));
#else
            s = std::string(sz, char(0));
#endif
        }
    }
#ifdef UNICODE
    return convertUTF16toUTF8(s) + PATHSEPARATOR;
#else
    return s + PATHSEPARATOR;
#endif
}

bool is_directory(const std::string& filename)
{
#ifdef UNICODE
    int err = GetFileAttributes(convertUTF8toUTF16(filename).c_str());
#else
    int err = GetFileAttributes(filename.c_str());
#endif
    if (err == -1)
        return false;
    return ((err & FILE_ATTRIBUTE_DIRECTORY) != 0);
}

#else /* Win32 */

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

directory::directory(const std::string& filename)
    : dir(nullptr)
{
    if (!is_directory(filename)) {
        THROW(error, std::string("no directory: ") + filename);
    }
    dir = opendir(filename.c_str());
    if (dir == nullptr) {
        THROW(error, std::string("Can't open directory ") + filename);
    }
}

auto directory::read() -> std::string
{
    struct dirent* dir_entry = readdir(dir);
    if (dir_entry != nullptr) {
        return std::string(dir_entry->d_name);
    }
    return {};
}

directory::~directory()
{
    closedir(dir);
}

auto make_dir(const std::string& dirname) -> bool
{
    int err = mkdir(dirname.c_str(), 0755);
    return (err != -1);
}

auto get_current_directory() -> std::string
{
    unsigned sz = 256;
    std::vector<char> s(sz);
    char* c = nullptr;

    while (c == nullptr) {
        c = getcwd(s.data(), sz - 1);
        if (c == nullptr) {
            sz += sz;
            s.resize(sz);
        }
    }
    return std::string(s.data()) + PATHSEPARATOR;
}

auto is_directory(const std::string& filename) -> bool
{
    struct stat fileinfo;
    int err = stat(filename.c_str(), &fileinfo);

    if (err != 0) {
        return false;
    }
    if (S_ISDIR(fileinfo.st_mode)) {
        return true;
    }
    return false;
}

#endif /* Win32 */

auto is_file(const std::string& filename) -> bool
{
    // Check if valid filename (can be file or directory)
    {
        std::ifstream ifs(filename);
        if (!ifs.good()) {
            return false;
        }
    }

    // sort out directories
    return !is_directory(filename);
}

void directory::walk(const std::string& path, std::function<void(const std::string&)> func)
{
    if (path.empty()) {
        THROW(error, "can't walk over directory \"\"");
    }

    if (!is_directory(path)) {
        // just call the function for the filename and exit
        func(path);
        return;
    }

    std::function<void(const std::string&)> handle_directory;

    handle_directory = [&](const std::string& current_path) {
        directory d(current_path);
        for (std::string curr_file = d.read(); !curr_file.empty(); curr_file = d.read()) {
            if (curr_file != "." && curr_file != "..") {
                auto combined_filename = current_path + curr_file;
                if (is_file(combined_filename)) {
                    func(combined_filename);
                } else {
                    handle_directory(combined_filename + PATHSEPARATOR);
                }
            }
        }
    };

    handle_directory(path.substr(path.length() - 1, 1) == PATHSEPARATOR ? path : path + PATHSEPARATOR);
}
