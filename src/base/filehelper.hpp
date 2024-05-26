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

#pragma once

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif /* WIN32_LEAN_AND_MEAN */
#include <windows.h>
#else
#include <dirent.h>
#endif

#include <functional>
#include <string>

/// directory reading/writing encapsulated for compatibility and ease of use,
/// fixme replace by std::filesystem???
class directory
{
    directory()                            = delete;
    directory(const directory&)            = delete;
    directory& operator=(const directory&) = delete;

  public:
    /// Open directory.
    ///@note throws exception if it is an invalid directory or if filename does
    /// note exist
    directory(const std::string& filename);

    /// Close directory
    ~directory();

    /// Read next filename from directory.
    ///@returns empty string if directory is fully read, filename else
    std::string read();

    /// Recursivly walk a direcory tree and call a function for every filename
    static void walk(const std::string& path, std::function<void(const std::string&)> func);

  private:
    // system specific part
#ifdef WIN32
    HANDLE dir;
    WIN32_FIND_DATA Win32_FileFind_Temporary; // needed because findfirst does
    bool temporary_used;                      // open and first read together
#else
    DIR* dir;
#endif
};

// file helper interface

///\brief Make new directory. Returns true on success.
bool make_dir(const std::string& dirname);

///\brief Returns absolute path of current working directory.
std::string get_current_directory();

///\brief Test if the given filename is a directory.
bool is_directory(const std::string& filename);

///\brief Test if the given filename is a file (can be read by fopen())
bool is_file(const std::string& filename);
