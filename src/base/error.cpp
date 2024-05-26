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

#include "error.hpp"

#include <sstream>

auto error::throw_location(const char* file, unsigned line) -> std::string
{
    std::ostringstream oss;
    oss << ", in file: " << file << ", in line: " << line;
    return oss.str();
}

error::error(const std::string& location, const std::string& message)
    : std::runtime_error(std::string("DftD error at ") + location + ", Type: " + message)
{
}

file_context_error::file_context_error(
    const std::string& location,
    const std::string& message,
    const std::string& filename)
    : error(location, message + ", regarding file: " + filename)
{
}

file_read_error::file_read_error(const std::string& location, const std::string& filename)
    : error(location, std::string("failed to load: ") + filename)
{
}
