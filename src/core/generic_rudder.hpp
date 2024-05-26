/*
Danger from the Deep - Open source submarine simulation
Copyright (C) 2003-2016  Thorsten Jordan, Luis Barrancos and others.

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

// generic rudder
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#ifndef GENERIC_RUDDER_H
#define GENERIC_RUDDER_H

#include "rigid_body.hpp"

/// Status/position of the rudder
enum class rudder_status
{
    full_left  = -2,
    left       = -1,
    midships   = 0,
    right      = 1,
    full_right = 2
};

/// A rudder for ships (or airplane's control surface)
struct generic_rudder
{
    // read from spec file, run-time constants
    vector3 pos;                     ///< 3d pos of rudder relative to parent (local!)
    axis turn_axis;                  ///< the axis that the rudder turns around, could be a
                                     ///< template parameter.
    angle max_angle;                 ///< max. angle of rudder (+-)
    area2d area;                     ///< area of rudder in m^2
    angular_velocity max_turn_speed; ///< max turn speed in angles/sec

    angle current_angle; ///< current rudder angle in degrees
    angle to_angle;      ///< angle that rudder should move to

    generic_rudder(const vector3& p, axis a, angle ma, area2d ar, angular_velocity mts)
        : pos(p)
        , turn_axis(a)
        , max_angle(ma)
        , area(ar)
        , max_turn_speed(mts)
    {
    }
    void simulate(duration delta_time);
    void load(const xml_elem& parent);
    void save(xml_elem& parent) const;
    void set_to(double part) { to_angle = max_angle * part; } ///< -1 ... 1
    void set_to(rudder_status rs);
    void midships() { to_angle = 0.0; }
    double deflect_factor() const; ///< sin(angle)
    double bypass_factor() const;  ///< cos(angle)
    /** Compute force caused by rudder.
     * @param parent_local_velocity velocity of parent object
     * @param forward_force additional force directly applied to rudder by
     * screws nearby
     * @param medium_density medium density in kg/m^3, i.e. 1000 for water
     */
    force3d
    compute_force(const velocity3d& parent_local_velocity, force1d forward_force, double medium_density = 1000.0) const;
};

#endif
