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

// sea object id
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#pragma once

#include <functional>

/// Define datatype for a sea object reference. 0 is invalid reference.
struct sea_object_id
{
    static constexpr unsigned invalid = 0; ///< Invalid value
    unsigned id{invalid};                  ///< The ID. But ask only class game for that!
    sea_object_id() = default;
    sea_object_id(unsigned n)
        : id(n)
    {
    }
    bool operator==(const sea_object_id& other) const { return id == other.id; }
    bool operator!=(const sea_object_id& other) const { return id != other.id; }
    bool is_valid() const { return id != invalid; }
};

/// Declare hash function so we can use sea_object_id as map-key
template<>
struct std::hash<sea_object_id>
{
    std::size_t operator()(const sea_object_id& id) const noexcept { return id.id; }
};
