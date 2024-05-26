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
//  A 2d rectangle (C)+(W) 2020 Thorsten Jordan
//

#pragma once

#include "vector2.hpp"

#include <vector>

/// A 2D rectangle with template coordinate types
template<typename D>
class rectangle_t
{
  public:
    vector2t<D> minpos, maxpos; ///< Minimum and maximum values
    bool is_empty{true};        ///< Is rectangle empty and invalid?

    /// Construct invalid empty rectangle
    rectangle_t() = default;

    /// Construct by two vectors - note! data is not checked! Sort by yourself
    /// and make sure data is valid rectangle!
    rectangle_t(const vector2t<D>& bottomleft, const vector2t<D>& topright)
        : minpos(bottomleft)
        , maxpos(topright)
        , is_empty(false)
    {
    }

    /// Construct by two vectors - note! data is not checked! Sort by yourself
    /// and make sure data is valid rectangle!
    rectangle_t(D left, D bottom, D right, D top)
        : minpos(vector2t<D>(left, bottom))
        , maxpos(vector2t<D>(right, top))
        , is_empty(false)
    {
    }

    /// Extend rectangle
    void extend(const vector2t<D>& p)
    {
        if (is_empty) {
            minpos = maxpos = p;
        } else {
            minpos = minpos.min(p);
            maxpos = maxpos.max(p);
        }
    }

    /// Construct from bound of values
    rectangle_t(const std::vector<vector2t<D>>& values)
        : is_empty(values.size() == 0)
    {
        for (const auto& p : values) {
            extend(p);
        }
    }

    /// Return size of rectangle
    [[nodiscard]] vector2t<D> size() const { return maxpos - minpos; }

    /// Return center of rectangle
    [[nodiscard]] vector2t<D> center() const { return (maxpos + minpos) / D(2); }

    /// Check if coordinate is inside rectangle
    [[nodiscard]] bool is_inside(const vector2t<D>& p) const
    {
        return !is_empty && p.x >= minpos.x && p.y >= minpos.y && p.x <= maxpos.x && p.y <= maxpos.y;
    }

    /// Compute rectangle from bound of two other rectanglees
    rectangle_t(const rectangle_t& a, const rectangle_t& b)
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

    /// Extend rectangle with other rectangle
    void extend(const rectangle_t& other)
    {
        if (is_empty)
            *this = other;
        else if (!other.is_empty) {
            minpos = minpos.min(other.minpos);
            maxpos = maxpos.max(other.maxpos);
        }
    }

    /// Create intersection with other rectangle
    void intersect(const rectangle_t& other)
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
    [[nodiscard]] rectangle_t<D> translated(const vector2t<D>& v) const
    {
        rectangle_t copy(*this);
        if (!copy.is_empty) {
            copy.minpos += v;
            copy.maxpos += v;
        }
        return copy;
    }

    /// Get values
    [[nodiscard]] D x() const { return minpos.x; }

    /// Get values
    [[nodiscard]] D y() const { return minpos.y; }

    /// Get values
    [[nodiscard]] D w() const { return maxpos.x - minpos.x; }

    /// Get values
    [[nodiscard]] D h() const { return maxpos.y - minpos.y; }
};

typedef rectangle_t<double> rectangle;
typedef rectangle_t<float> rectanglef;
typedef rectangle_t<int> rect;
