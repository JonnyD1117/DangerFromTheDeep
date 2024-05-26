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
//  A 2d area (C)+(W) 2020 Thorsten Jordan
//

#pragma once

#include "vector2.hpp"

class area
{
  public:
    vector2i offset; ///< Begin of area
    vector2i size;   ///< Size of area (non-negative!)

    /// Construct empty area
    area() = default;

    /// Construct by offset/size values
    area(vector2i offset_, vector2i size_)
        : offset(offset_)
        , size(size_)
    {
        if (size.x < 0 || size.y < 0) {
            throw std::invalid_argument("area size must not be negative");
        }
    }

    /// Construct by separate offset/size values
    area(int x, int y, int w, int h)
        : offset(x, y)
        , size(w, h)
    {
        if (size.x < 0 || size.y < 0) {
            throw std::invalid_argument("area size must not be negative");
        }
    }

    /// Check if coordinate is inside area
    bool is_inside(vector2i p) const
    {
        return (p.x >= offset.x && p.x < offset.x + int(size.x) && p.y >= offset.y && p.y < offset.y + int(size.y));
    }

    vector2i get_limit() const { return offset + vector2i(size); }
    bool empty() const { return (size.x * size.y == 0); }

    area operator*(unsigned factor) const { return area(offset * int(factor), size * factor); }

    area grow(unsigned n) const { return area(offset.x - int(n), offset.y - int(n), size.x + n * 2, size.y + n * 2); }

    area half_scale() const
    {
        // we need to mask out last bit before dividing by 2, it makes a
        // difference for negative values, in theory it should not.
        vector2i upper_limit = ((get_limit() + vector2i(1, 1)) & ~1) / 2;
        vector2i half_offset = (offset & ~1) / 2;
        return area(half_offset, upper_limit - half_offset);
    }
};

inline std::ostream& operator<<(std::ostream& os, const area& ar)
{
    os << "off_x=" << ar.offset.x << "; off_y=" << ar.offset.y << "; size_x=" << ar.size.x << "; size_y=" << ar.size.y;
    return os;
}
