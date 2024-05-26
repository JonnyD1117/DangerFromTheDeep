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

// physical units
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#pragma once

#include "angle.hpp"
#include "matrix3.hpp"
#include "quaternion.hpp"
#include "vector3.hpp"

// Physical values, use C++ types to define valid calculations
/// Physical unit names
enum class physical_unit
{
    time_point,       // (s)
    duration,         // s
    mass,             // kg
    acceleration,     // m/s^2
    velocity,         // acceleration*time, m/s
    force,            // force*acceleration, kg*m/s^2 = N
    momentum,         // force*time or torque*time = mass*velocity, kg*m/s = N*s, also
                      // called impulse
    torque,           // force*distance, N*m
    angular_velocity, // angle/s
    distance,         // meters
    area              // meters^2
};

/// Later have some class that checks value is between 0 and 1. Maybe add class
/// for that, no physical unit.
using factor = double;

/// Base class for physical unit
template<typename T, physical_unit P>
struct physical_value
{
    /// The value itself, can be number or vector
    T value{T{}};
    /// Default constructor
    physical_value() = default;
    /// Explicit construction, use with care
    physical_value(T v)
        : value(v)
    {
    }
    /// Rotate 3d values
    [[nodiscard]] auto rotate(const quaternion& q) const { return physical_value<T, P>(q.rotate(value)); }

    /// Cross product for 3d values
    [[nodiscard]] auto cross(const vector3& v) const { return physical_value<T, P>(value.cross(v)); }

    /// Add values
    auto operator+=(const physical_value<T, P>& other) { value += other.value; }
    /// Add values
    auto operator+(const physical_value<T, P>& other) const { return physical_value<T, P>(value + other.value); }
    /// Compare
    auto operator<(const physical_value<T, P>& other) const { return value < other.value; }
    /// Compare (here we could make use of the spaceship operator)
    auto operator<=(const physical_value<T, P>& other) const { return value <= other.value; }
};
/// transform 3d values (also rotation)
template<physical_unit P>
auto operator*(const matrix3& m, const physical_value<vector3, P>& v)
{
    return physical_value<vector3, P>(m * v.value);
}

/// Duration in seconds
using duration = physical_value<double, physical_unit::duration>;
duration& operator-=(duration& a, const duration& b)
{
    a.value -= b.value;
    return a;
}
bool operator>(duration& a, const duration& b)
{
    return a.value > b.value;
}
duration operator*(const duration& a, factor t)
{
    return duration(a.value * t);
}

/// Time stamp in seconds
using time_point = physical_value<double, physical_unit::time_point>;
auto duration_between(time_point t0, time_point t1)
{
    return duration(t1.value - t0.value);
}

/// Velocity in meters per second
using velocity1d = physical_value<double, physical_unit::velocity>;
using velocity3d = physical_value<vector3, physical_unit::velocity>;
template<typename T>
auto operator*(const physical_value<T, physical_unit::velocity>& v, duration d)
{
    return v.value * d.value;
}

/// Angular velocity
using angular_velocity = physical_value<double, physical_unit::angular_velocity>;
angle operator*(angular_velocity av, duration d)
{
    return angle(av.value * d.value);
}

/// Force in newtons
using force1d = physical_value<double, physical_unit::force>;
using force3d = physical_value<vector3, physical_unit::force>;
template<typename T>
auto operator*(const physical_value<T, physical_unit::force>& v, duration d)
{
    return physical_value<T, physical_unit::momentum>(v.value * d.value);
}

/// Acceleration in meters per second squared
using acceleration1d = physical_value<double, physical_unit::acceleration>;
using acceleration3d = physical_value<vector3, physical_unit::acceleration>;
template<typename T>
auto operator*(const physical_value<T, physical_unit::acceleration>& v, duration d)
{
    return physical_value<T, physical_unit::velocity>(v.value * d.value);
}

/// Torque in newtons times meters
using torque1d = physical_value<double, physical_unit::torque>;
using torque3d = physical_value<vector3, physical_unit::torque>;
template<typename T>
auto operator*(const physical_value<T, physical_unit::torque>& t, duration d)
{
    // returns angular momentum!
    return physical_value<T, physical_unit::momentum>(t.value * d.value);
}

/// Mass in kilograms
using mass1d = physical_value<double, physical_unit::mass>;
/// Get force value in newtons
template<typename T>
auto operator*(const physical_value<T, physical_unit::acceleration>& a, mass1d m)
{
    return physical_value<T, physical_unit::force>(a.value * m.value);
}
/// Get linear momentum
template<typename T>
auto operator*(const physical_value<T, physical_unit::velocity>& v, mass1d m)
{
    return physical_value<T, physical_unit::momentum>(v.value * m.value);
}
/// Get velocity from momentum
template<typename T>
auto operator/(const physical_value<T, physical_unit::momentum>& v, mass1d m)
{
    return physical_value<T, physical_unit::velocity>(v.value * (1.0 / m.value));
}

/// Momentum in kilograms times meters per second
using momentum1d = physical_value<double, physical_unit::momentum>;
using momentum3d = physical_value<vector3, physical_unit::momentum>;

/// Force caused by mass and gravity
force3d gravity_force(mass1d m)
{
    return force3d(vector3(axis::neg_z) * m.value * constant::GRAVITY);
}

/// Distance in meters
using distance = physical_value<double, physical_unit::distance>;
distance get_distance(const vector3& a, const vector3& b)
{
    return distance((a - b).length());
}
velocity1d operator/(distance d, duration t)
{
    return velocity1d(d.value / t.value);
}

/// 2d area
using area2d = physical_value<double, physical_unit::area>;

/// Later have some class that checks value is between 0 and 1. Maybe add class
/// for that, no physical unit.
using factor = double;
