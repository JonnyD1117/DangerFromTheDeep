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

// physical rigid body
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#pragma once

#include "units.hpp"
class xml_elem;

/// a physical rigid body with simulation
class rigid_body
{
  public:
    // Rigid body variables

    // position, [SAVE]
    vector3 position;

    // l.m./impulse ("P") P = M * v [SAVE], world space!
    momentum3d linear_momentum;

    // orientation, [SAVE]
    quaternion orientation;

    // angular momentum ("L") L = I * w = R * I_k * R^T * w [SAVE], world space!
    momentum3d angular_momentum;

    // total weight, later read from spec file (Kg)
    mass1d mass;

    // object local (I_k). [could be a reference into a model object...]
    matrix3 inertia_tensor;

    // object local (I_k), inverse of inertia tensor
    matrix3 inertia_tensor_inv;

    // Computed from rigid body variables

    // world space velocity
    velocity3d velocity;

    // angular velocity around the local z-axis (mathematical CCW)
    angular_velocity turn_velocity;

    // angular velocity around the local x-axis (mathematical CCW)
    angular_velocity pitch_velocity;

    // angular velocity around the local y-axis (mathematical CCW)
    angular_velocity roll_velocity;

    // global z-orientation is stored additionally
    angle heading;

    // recomputed every frame by simulate() method
    velocity3d local_velocity;

    [[nodiscard]] force3d compute_default_gravity_force() const;

    void set_mass_and_inertia_tensor(mass1d mass_, const matrix3& inertia_tensor = matrix3::one());

    // recomputes *_velocity, heading etc.
    void compute_helper_values();

    // Load data
    void load(const xml_elem& parent);
    // Save data
    void save(xml_elem& parent) const;

    void create_storage_definition(class data_node& parent);

    void simulate(duration delta_time, std::initializer_list<std::pair<vector3, vector3>> local_forces);

    [[nodiscard]] velocity3d compute_linear_velocity(const vector3& position_global) const;

    [[nodiscard]] double compute_collision_response_value(const vector3& collision_pos, const vector3& N) const;

    void apply_collision_impulse(const vector3& collision_pos, const momentum3d& J);

    void manipulate_position(const vector3& newpos);
    void manipulate_speed(velocity1d localforwardspeed);
    void manipulate_heading(angle hdg);

  protected:
    void simulate(duration delta_time, const force3d& force, const torque3d& torque);
};
