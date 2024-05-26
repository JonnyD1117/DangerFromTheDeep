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

#include "constant.hpp"
#include "helper.hpp"
#include "vector3.hpp"

#include <cmath>
#include <iostream>

///\brief This class represents clockwise angles. It is used for rotations or
/// nautical angles
/** note that mathematical angles go counter clockwise and nautical angles
    clockwise. conversion from/to mathematical angles ignores this because
    it is used for conversion of angle differences.
    so an a.rad() does not compute the corresponding mathematical angle
    for an nautical angle a but transform 0...360 degrees to 0...2pi instead. */
class angle
{
  protected:
    double val{0.0};
    static double clamped(double d) { return helper::mod(d, 360.0); }

  public:
    angle() { }
    angle(double d)
        : val(d)
    {
    }

    /// Compute angle from direction in horizontal 2d plane
    angle(const vector2& v) { val = (v == vector2(0.0, 0.0)) ? 0.0 : 90.0 - atan2(v.y, v.x) * 180.0 / constant::PI; }

    /// Compute azimuth (angle in horizontal plane) from 3d direction
    static angle azimuth(const vector3& direction) { return angle(direction.xy()); }

    /// Compute elevation angle from direction
    static angle elevation(const vector3& direction) { return {::asin(direction.z) * 180 / constant::PI}; }

    /// Compute direction from azimuth and elevation
    static vector3 direction_from_azimuth_and_elevation(angle azimuth, angle elevation)
    {
        return vector3(azimuth.direction() * elevation.cos(), elevation.sin()).normal();
    }

    [[nodiscard]] double value() const { return clamped(val); }
    [[nodiscard]] unsigned ui_value() const { return unsigned(clamped(helper::round(val))); }

    [[nodiscard]] unsigned ui_abs_value180() const { return unsigned(fabs(helper::round(value_pm180()))); }

    [[nodiscard]] double rad() const { return value() * constant::PI / 180.0; }
    [[nodiscard]] double value_pm180() const
    {
        double d = clamped(val);
        return d <= 180.0 ? d : d - 360.0;
    }

    angle operator+(const angle& other) const { return {val + other.val}; }
    angle operator-(const angle& other) const { return {val - other.val}; }
    angle operator-() const { return {-val}; }
    angle operator*(double t) const { return {val * t}; }

    /// returns true if the turn from "this" to "a" is shorter when done
    /// clockwise
    [[nodiscard]] bool is_clockwise_nearer(const angle& a) const { return clamped(a.val - val) <= 180.0; }

    static angle from_rad(double d) { return {d * 180.0 / constant::PI}; }
    static angle from_math(double d) { return {(constant::PI / 2 - d) * 180.0 / constant::PI}; }

    angle& operator+=(const angle& other)
    {
        val += other.val;
        return *this;
    }

    angle& operator-=(const angle& other)
    {
        val -= other.val;
        return *this;
    }

    [[nodiscard]] double diff(const angle& other) const
    {
        double d = clamped(other.val - val);
        if (d > 180.0)
            d = 360.0 - d;
        return d;
    }

    [[nodiscard]] double diff_in_direction(bool ccw, const angle& other) const
    {
        return ccw ? clamped(val - other.val) : clamped(other.val - val);
    }

    [[nodiscard]] double sin() const { return ::sin(rad()); }
    [[nodiscard]] double cos() const { return ::cos(rad()); }
    [[nodiscard]] double tan() const { return ::tan(rad()); }

    [[nodiscard]] vector2 direction() const
    {
        double r = rad();
        return {::sin(r), ::cos(r)};
    }

    bool operator==(const angle& b) const { return value() == b.value(); }
    bool operator!=(const angle& b) const { return value() != b.value(); }

    /// Compute azimuth (angle in horizontal plane) of direction

    /// Compute elevation angle of direction
};

inline std::ostream& operator<<(std::ostream& os, const angle& a)
{
    os << a.value();
    return os;
}
