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

#include "generic_rudder.hpp"

/// Simulate the rudder movement
void generic_rudder::simulate(duration delta_time)
{
    const auto max_turn_dist           = max_turn_speed * delta_time;
    const auto rudder_angle_difference = (to_angle - current_angle).value_pm180();
    if (std::abs(rudder_angle_difference) <= max_turn_dist) {
        // if rudder_angle_difference is 0, nothing happens.
        current_angle = to_angle;
    } else if (rudder_angle_difference < 0.0) {
        current_angle -= max_turn_dist;
    } else {
        current_angle += max_turn_dist;
    }
}

/// Load data from XML
void generic_rudder::load(const xml_elem& parent)
{
    current_angle = parent.attrf("angle");
    to_angle      = parent.attrf("to_angle");
}

/// Save data to XML
void ship::generic_rudder::save(xml_elem& parent) const
{
    parent.set_attr(current_angle.value_pm180(), "angle");
    parent.set_attr(to_angle.value_pm180(), "to_angle");
}

force3d generic_rudder::compute_force(
    const velocity3d& parent_local_velocity,
    force1d forward_force,
    double medium_density = 1000.0) const
{
    // rudders are placed in forward movement direction
    const auto local_forward_velocity = parent_local_velocity.value.at(axis::y);
    // this correct by physical units, but how is the physical explanation?!
    const auto myforce = (area * medium_density * s * s + forward_force.value) * deflect_factor();
    return force3d(vector3(turn_axis) * myforce);
}

/// Compute factor for deflection
double generic_rudder::deflect_factor() const
{
    return -current_angle.sin();
}

/// Compute factor for bypassed medium
double generic_rudder::bypass_factor() const
{
    return current_angle.cos();
}
