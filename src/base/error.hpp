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

// Danger from the Deep, standard error/exception
// (C)+(W) by Thorsten Jordan. See LICENSE

#pragma once

#include <stdexcept>
#include <string>

///\brief Base exception class for any runtime error.
class error : public std::runtime_error
{
  public:
    static std::string throw_location(const char* file, unsigned line);
    error(const std::string& location, const std::string& message);
};

/// error with a file context
class file_context_error : public error
{
  public:
    file_context_error(const std::string& location, const std::string& message, const std::string& filename);
};

/// error reading a file
class file_read_error : public error
{
  public:
    file_read_error(const std::string& location, const std::string& filename);
};

// only for backwards compatibility of old code!
class sdl_error : public error
{
  public:
    sdl_error(const std::string& location, const std::string& filename)
        : error(location, filename)
    {
    }
};

// Throw with description where exactly the error was thrown
#ifdef THROW
#undef THROW
#endif
#define THROW(exceptionclass, ...) throw exceptionclass(error::throw_location(__FILE__, __LINE__), ##__VA_ARGS__)
