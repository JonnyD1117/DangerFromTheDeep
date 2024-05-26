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

#include "rigid_body.hpp"

#include "xml.hpp"

/*
Physical rigid body simulation.

fixme compute_force_and_torque: replace by function that computes positions and
forces applied there? torque can be computed internally then. that way we don't
need to take the orientation out of here... only offer function that gets a list
of forces applied in world space

fixme: medium drag causing sideways movement has to be implemented correctly in
sea_object!

Forces are applied to a body leading to a linear force and a torque.
The values are integrated over time to compute linear and angular momentum
and finally position and orientation.

Force comes from screws/rudders etc. and also from the medium pushing on the
body. This explains why moving objects can do curves even if no more force
applied: The surrounding medium causes drag that slows the body down but because
of the shape of the object it can also generate linear momentum that is not in
move direction causing the body to change direction of movement. For example
ships or airplanes move forward, and when turning change their orientation. They
sill move forward but the medium will bounce off their side causing a push to
the side, so the drag will not only slow them down but also make them move
sideways. This leads to a linear movement that is following the changed
orientation. This explains how bodies can change their direction only with
internal force/energy.
For ships we simulate voxels that generate lift values causing the ship to
change orientation with water height values.

compute force at bow/stern/mid/left side/right side or some other important
points. like where the trim tanks of subs are.

the torque is computed as

M = (r1 - r0) x F1, where F1 is the force acting at point r1, where r0 is center
of gravity do r1-r0 is the vector from the object's center to the point where F1
is acting. M is a vector definining axis of rotation (direction of M) and
strength of rotation (length of M). To compute total torque, sum all M over i:
M_total = Sum_i (r_i - r0) x F_i

To compute translational forces, just sum up all forces F: F_total = Sum_i F_i.

Problem: orientation is stored as quaternion, not as three angles.
torque changes angular velocity over time, and that changes orientation over
time. The axes of torque or angular velocity don't need to be identical! That is
the problem. Given torque, angular velocity and orientation as quaternions q_t,
q_v and q_r we have q_v' = q_v * (q_t * delta_t) q_r' = q_r * (q_v * delta_t)
But computing q * z, where q is a quaternion for rotation (unit quaternion) and
z is a number is not that easy. q represents a rotation around an axis x and and
angle w. q * z would then be the rotation around the same axis, but with angle
w*z. A rotation quaternion for axis x and angle w is given as: (cos(w/2),
sin(w/2)*x) Thus q*z would be (cos(w*z/2), sin(w*z/2)*) how to compute that from
q? we would need an acos function call, which is very costly... One could also
see cos(w/2),sin(w/2) as complex number. It has an absolute value of 1. So
multiplying the angle with n would be the same as taking the n'th power of that
number. But this computation would also be very costly - it is the same problem
as we have here. Alternativly: Represent angular velocity and torque around
three fix axes. Store axis and angle for each part, like x/y/z-axis and the
three angles. Changing angular velocity by torque is easy then, orientation
could still be stored as quaternion.

Computing orientation forced by the waves:
Compute buoyancy on points around the ship. Draught gives weight of displaced
water, the difference of the ships weight (or the weight of that part for which
the buoyancy is computed) gives a force, that is applied to the ship. Force =
mass * acceleration. Acceleration is g, mass is difference between displaced
water and the ship's part.

*/

/// default force if only gravity applied, no torque
auto rigid_body::compute_default_gravity_force() const -> force3d
{
    // force is in world space!
    /* general formulas:
       Total force acting on a body is just the sum of all forces acting on it.
       Total torque is the sum over all forces with index i, with summands
       (p_i - x) cross F_i.
       F_i is the force.
       p_i is the point in 3-space where F_i acts.
       x is the gravitational center of the body.
       Hence: total torque = sum xr_i cross F_i  (xr : relative position).
       Torque is a vector, that has a direction and a length.
       The direction is the axis around that the torque/force acts,
       and the length is proportional to the amount of the torque.
       In our current model the length would be proportional to the turn
       acceleration.
    */
    return gravity_force(mass);
}

/// set mass and inertia tensor - use only this method to set it!
void rigid_body::set_mass_and_inertia_tensor(mass1d mass_, const matrix3& inertia_tensor_)
{
    mass               = mass_;
    inertia_tensor     = inertia_tensor_;
    inertia_tensor_inv = inertia_tensor.inverse();
}

/// compute some auxiliary values for simulation
void rigid_body::compute_helper_values()
{
    velocity       = linear_momentum / mass;
    local_velocity = velocity.rotate(orientation.conj());

    heading = angle(orientation.rotate(0.0, 1.0, 0.0).xy());

    // w is _old_ spin vector, but we need the new one...
    // does it make a large difference?
    // |w| is revolutions per time, thus 2*Pi/second for |w|=1.
    // we have to multiply it by 360/(2*Pi) to get angles per second.
    // hmmm, w2.length is speed, but sign of it depends on direction of w!!!!
    // if the ship turns right (clockwise), turn_velocity should be positive?
    // in that case, w is pointing downwards.

    // unit of |w| is revolutions per time, that is 2*Pi/second.
    // Note! here w is local. Get global w by rotating it with
    // orientation.rotate(w)
    velocity3d w = (inertia_tensor_inv * angular_momentum.rotate(orientation.conj())) / mass;

    // turn velocity around z-axis is projection of w to z-axis, that is
    // simply w.z. Transform to angles per second. same for x/y.
    auto av = w.value * (180.0 / constant::PI);

    turn_velocity  = angular_velocity(av.z); // could also be named yaw_velocity.
    pitch_velocity = angular_velocity(av.x);
    roll_velocity  = angular_velocity(av.y);
}

void rigid_body::load(const xml_elem& parent)
{
    parent.child("position").get_attr(position);
    parent.child("orientation").get_attr(orientation);
    parent.child("linear_momentum").get_attr(linear_momentum.value);
    parent.child("angular_momentum").get_attr(angular_momentum.value);

    compute_helper_values();
}

void rigid_body::save(xml_elem& parent) const
{
    // specfilename is requested and stored by game or callers of this function
    parent.add_child("position").set_attr(position);
    parent.add_child("orientation").set_attr(orientation);
    parent.add_child("linear_momentum").set_attr(linear_momentum.value);
    parent.add_child("angular_momentum").set_attr(angular_momentum.value);
}

/// simulate next state of rigid body with time delta and force/torque applied
void rigid_body::simulate(duration delta_time, const force3d& force, const torque3d& torque)
{
    // compute new position by integrating linear_momentum
    // M^-1 * P = v, linear_momentum is in world space!
    const velocity3d world_space_velocity = linear_momentum / mass;
    position += world_space_velocity * delta_time;

    // compute new linear_momentum by integrating force
    linear_momentum += force * delta_time;

    // compute new orientation by integrating angular momentum
    // L = I * w = R * I_k * R^T * w =>
    // w = I^-1 * L = R * I_k^-1 * R^-1 * L
    // so we can compute w from I_k^-1 and L.
    // with some math we have:
    // w = R * (I_k^-1 * (R^-1 * L))
    //                   ^^^^^^^^^^
    //                      vector
    //         ^^^^^^^^^^^^^^^^^^^^
    //               vector
    // Thus we don't need the matrix R but can use
    // the orientation quaternion directly for rotation.
    // This is much more efficient.
    // with w we can update orientation.
    // w codes the axis/angle, which needs to get multiplied by delta_t,
    // so we can just compute w' = w * delta_t and then compute a
    // rotation quaternion from w' and set new orientation as
    // produkt of old orientation and w'.
    const velocity3d w = (inertia_tensor_inv * angular_momentum.rotate(orientation.conj())).rotate(orientation) / mass;

    const vector3 w2 = w * delta_time;

    // unit of |w| is revolutions per time, that is 2*Pi/second.
    const double w2l = w2.length();

    if (w2l > 1e-8) {
        // avoid too small numbers
        const quaternion q = quaternion::rot_rad(w2l, w2 * (1.0 / w2l));

        // multiply orientation with q: combined rotation.
        orientation = q * orientation;

        // we should renormalize orientation regularly, to avoid that
        // orientation isn't a valid rotation after many changes.
        if (std::abs(orientation.square_length() - 1.0) > 1e-8) {
            orientation.normalize();
        }
    }

    // compute new angular momentum by integrating torque (both in world space)
    angular_momentum += torque * delta_time;

    // update helper variables
    compute_helper_values();
}

/// Simulate applying local forces, give position and force in a list
void rigid_body::simulate(duration delta_time, std::initializer_list<std::pair<vector3, vector3>> local_forces)
{
    vector3 local_force;
    vector3 local_torque;

    for (const auto& data : local_forces) {
        // just sum of all forces for total force
        local_force += data.second;
        // relative position cross force
        local_torque += data.first.cross(data.second);
    }

    simulate(delta_time, orientation.rotate(local_force), orientation.rotate(local_torque));
}

/// compute linear velocity of the rigid body at a given global position
auto rigid_body::compute_linear_velocity(const vector3& position_global) const -> velocity3d
{
    // result is v(t) + w(t) x r(t)  (linear velocity + omega cross relative
    // vector)
    const velocity3d w = (inertia_tensor_inv * angular_momentum.rotate(orientation.conj())).rotate(orientation) / mass;

    return velocity + w.cross(position_global - position);
}

/// handle collision response - return value is velocity/mass
auto rigid_body::compute_collision_response_value(const vector3& collision_pos, const vector3& N) const -> double
{
    // WTF is N and what is this formula about? some force acting locally gives
    // force/torque. why not apply collision force?!
    const vector3 relative_position = collision_pos - position;

    return (N
            * orientation.rotate(inertia_tensor_inv * orientation.conj().rotate(relative_position.cross(N)))
                  .cross(relative_position))
           / mass.value;
}

/// apply impulse caused by a collision to the rigid body
void rigid_body::apply_collision_impulse(const vector3& collision_pos, const momentum3d& J)
{
    const vector3 relative_position = collision_pos - position;
    linear_momentum += J;
    angular_momentum += J.cross(-relative_position); // r.cross(J) = -J.cross(r)
    compute_helper_values();
}

void rigid_body::manipulate_position(const vector3& newpos)
{
    position = newpos;
    compute_helper_values();
}

void rigid_body::manipulate_speed(velocity1d localforwardspeed)
{
    local_velocity.value.y = localforwardspeed.value;
    linear_momentum        = local_velocity.rotate(orientation) * mass;
    compute_helper_values();
}

void rigid_body::manipulate_heading(angle hdg)
{
    orientation     = quaternion::rot(-hdg.value(), 0, 0, 1);
    linear_momentum = local_velocity.rotate(orientation) * mass;
    compute_helper_values();
}
