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

// An nautical angle (C)+(W) 2003 Thorsten Jordan

#pragma once

#include "angle.hpp"
#include "helper.hpp"

#include <vector>

/// A 1D table with wrap around interpolation indexed by an angle
template<typename T>
class angular_table
{
  public:
    angular_table(std::vector<T>&& values_ = std::vector<T>())
        : values(std::move(values_))
    {
    }
    T get(angle a) const
    {
        const auto exact_index = a.value() * values.size() / 360.0;
        const auto i0          = std::size_t(std::floor(exact_index));
        const auto i1          = (i0 + 1) == values.size() ? 0 : i0 + 1;
        return helper::interpolate(values[i0], values[i1], helper::frac(exact_index));
    }

  protected:
    std::vector<T> values;
};
