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
//  A 3d box (C)+(W) 2020 Thorsten Jordan
//

#pragma once

#include "vector3.hpp"

#include <vector>

/// a 3d-box
template<typename D>
class box_t
{
  public:
    vector3t<D> minpos, maxpos; ///< Minimum and maximum values
    bool is_empty{true};        ///< Is box empty and invalid?

    /// Construct invalid empty box
    box_t() = default;
    /// Extend box
    void extend(const vector3t<D>& p)
    {
        if (is_empty) {
            minpos = maxpos = p;
        } else {
            minpos = minpos.min(p);
            maxpos = maxpos.max(p);
        }
    }
    /// Construct from bound of values
    box_t(const std::vector<vector3t<D>>& values)
        : is_empty(values.empty())
    {
        for (const auto& p : values) {
            extend(p);
        }
    }
    /// Return size of box
    [[nodiscard]] vector3t<D> size() const { return maxpos - minpos; }

    /// Return center of box
    [[nodiscard]] vector3t<D> center() const { return (maxpos + minpos) / D(2); }

    /// Check if coordinate is inside box
    [[nodiscard]] bool is_inside(const vector3t<D>& p) const
    {
        return !is_empty && p.x >= minpos.x && p.y >= minpos.y && p.z >= minpos.z && p.x <= maxpos.x && p.y <= maxpos.y
               && p.z <= maxpos.z;
    }

    /// Compute box from bound of two other boxes
    box_t(const box_t& a, const box_t& b)
        : is_empty(false)
    {
        if (a.is_empty)
            *this = b;
        else if (b.is_empty)
            *this = a;
        else {
            minpos = a.minpos.min(b.minpos);
            maxpos = a.maxpos.max(b.maxpos);
        }
    }

    /// Extend box with other box
    void extend(const box_t& other)
    {
        if (is_empty)
            *this = other;
        else if (!other.is_empty) {
            minpos = minpos.min(other.minpos);
            maxpos = maxpos.max(other.maxpos);
        }
    }

    /// Create intersection with other box
    void intersect(const box_t& other)
    {
        is_empty |= other.is_empty;
        if (!is_empty) {
            minpos = minpos.max(other.minpos);
            maxpos = maxpos.min(other.maxpos);
            if (maxpos.x <= minpos.x || maxpos.y <= minpos.y || maxpos.z <= minpos.z) {
                is_empty = true;
            }
        }
    }

    /// Get translated version
    [[nodiscard]] box_t<D> translated(const vector3t<D>& v) const
    {
        box_t copy(*this);
        if (!copy.is_empty) {
            copy.minpos += v;
            copy.maxpos += v;
        }
        return copy;
    }
};

using box  = box_t<double>;
using boxf = box_t<float>;
