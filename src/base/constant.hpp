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

// constants
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#pragma once

#include <cmath>

namespace constant
{
//> yet no c++ standard
constexpr double PI   = 3.14159265358979323846;
constexpr double PI_2 = 1.57079632679489661923;

// a very "global" constant
constexpr double GRAVITY = 9.806;

// computed with an earth perimeter of 40030.17359km
constexpr double DEGREE_IN_METERS = 111194.9266388889;
constexpr double MINUTE_IN_METERS = 1853.248777315;
constexpr double SECOND_IN_METERS = 30.887479622;

// earth perimeter in meters
constexpr double EARTH_PERIMETER = 40030173.59;

// sqrt cannot be used in constexpr but see this
// https://baptiste-wicht.com/posts/2014/07/compile-integer-square-roots-at-compile-time-in-cpp.html

static constexpr std::size_t ct_sqrt(std::size_t res, std::size_t l, std::size_t r)
{
    if (l == r) {
        return r;
    } else {
        const auto mid = (r + l) / 2;

        if (mid * mid >= res) {
            return ct_sqrt(res, l, mid);
        } else {
            return ct_sqrt(res, mid + 1, r);
        }
    }
}

static constexpr std::size_t ct_sqrt(std::size_t res)
{
    return ct_sqrt(res, 1, res);
}

// Brutto register ton in cubic meters (100 cubic feet, 1 feet = 30.48cm)
constexpr double BRT_VOLUME = 2.8316846592;

constexpr double SEA_WATER_DENSITY = 1030.0; ///< Density of sea water in kg/m³

constexpr double WGS84_A = 6378137.0;
constexpr double WGS84_B = 6356752.314;
constexpr double WGS84_K = ct_sqrt(WGS84_A * WGS84_A - WGS84_B * WGS84_B) / WGS84_A;

// earth radius in meters (radius of a globe with same volume as the GRS 80
// ellipsoide)
constexpr double EARTH_RADIUS         = 6371000.785; // 6371km
constexpr double SUN_RADIUS           = 696e6;       // 696.000km
constexpr double MOON_RADIUS          = 1.738e6;     // 1738km
constexpr double EARTH_SUN_DISTANCE   = 149600e6;    // 149.6 million km.
constexpr double MOON_EARTH_DISTANCE  = 384.4e6;     // 384.000km
constexpr double EARTH_ROT_AXIS_ANGLE = 23.45;       // degrees.

constexpr double MOON_ORBIT_TIME_SIDEREAL = 27.3333333 * 86400.0; // sidereal month is 27 1/3 days

constexpr double MOON_ORBIT_TIME_SYNODIC = 29.5306 * 86400.0; // synodic month is 29.5306 days

// more precise values:
// 29.53058867
// new moon was on 18/11/1998 9:36:00 pm
constexpr double MOON_ORBIT_AXIS_ANGLE = 5.15;     // degrees
constexpr double EARTH_ROTATION_TIME   = 86164.09; // 23h56m4.09s, one sidereal day!

constexpr double EARTH_ORBIT_TIME = 31556926.5; // in seconds. 365 days, 5 hours, 48 minutes, 46.5 seconds

constexpr double MOON_POS_ADJUST = 300.0; // in degrees. Moon pos in its orbit
                                          // on 1.1.1939 fixme: research the value
} // namespace constant
