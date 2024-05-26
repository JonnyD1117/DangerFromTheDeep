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
//  A 3d cylinder (C)+(W) 2020 Thorsten Jordan
//

#pragma once

#include "sphere.hpp"
#include "vector3.hpp"

#include <utility>

/// a 3d cylinder with template coordinate types
template<class D>
class cylinder_t
{
  public:
    vector3t<D> start; ///< center of bottom segment
    vector3t<D> end;   ///< center of top segment
    D radius{D(0)};    ///< radius of sphere

    cylinder_t() = default;
    cylinder_t(const vector3t<D>& p0, const vector3t<D>& p1, const D& r)
        : start(p0)
        , end(p1)
        , radius(r)
    {
    }

    /// determine distance to cylinder
    [[nodiscard]] D distance(const vector3t<D>& a) const
    {
        // check for distance of sphere center to line
        // project onto line
        const auto delta = end - start;
        const auto t     = (a - start) * delta / delta.square_length();

        if (t < 0.0) {
            return start.distance(a);
        } else if (t > 1.0) {
            return end.distance(a);
        } else {
            // Compare distance to axis with radii of cylinder and sphere
            return (start + delta * t).distance(a);
        }
    }

    /// determine if point is inside
    [[nodiscard]] bool is_inside(const vector3t<D>& a) const
    {
        // project onto line
        const auto delta = end - start;
        const auto t     = (a - start) * delta / delta.square_length();

        if (t < 0.0 || t > 1.0) {
            // Projection onto cylinder axis outside range
            return false;
        }

        // Compare distance to axis with radius
        return (start + delta * t).square_distance(a) > radius * radius;
    }

    /// determine if intersects sphere
    [[nodiscard]] bool intersects(const sphere_t<D>& s) const
    {
        // check for distance of sphere center to line
        // project onto line
        const auto delta = end - start;
        const auto t     = (s.center - start) * delta / delta.square_length();

        if (t < 0.0) {
            // check for collision with sphere around start
            return sphere_t<D>(start, radius).intersects(s);
        } else if (t > 1.0) {
            // check for collision with sphere around end
            return sphere_t<D>(end, radius).intersects(s);
        } else {
            // Compare distance to axis with radii of cylinder and sphere
            return (start + delta * t).distance(s.center) < radius + s.radius;
        }
    }
};

using cylinder  = cylinder_t<double>;
using cylinderf = cylinder_t<float>;
