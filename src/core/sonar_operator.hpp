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

// Sonar operator simulation
// subsim (C) + (W). See LICENSE

#include "sonar.hpp"

#include <map>

#pragma once

class sonar_operator
{
  public:
    // defined here because global sonar_contact already exists, but has a
    // different form (should be replaced later?) weird...
    struct contact
    {
        double strength_dB;
        shipclass type;
        contact(double s = 1e-10, shipclass t = NONE)
            : strength_dB(s)
            , type(t)
        {
        }
    };

  protected:
    enum states
    {
        initial,
        find_growing_signal,
        find_max_peak_coarse,
        find_max_peak_fine,
        track_signal
    };
    states state{initial};
    angle current_angle;
    double current_signal_strength{-1};
    // store angle and contact, per contact strength (dB) and ship type
    // angle is absolute nautical angle, to make contacts invariant of sub's
    // heading. fixme: good idea, but a contact is reported many times then
    // while the sub turns, fixme!
    std::map<double, contact> contacts;
    bool active; // disabled, when user does the work

    static const double turn_speed_fast; //= 6.0;	// degrees per second.
    static const double turn_speed_slow; //= 2.0;	// degrees per second.
    static const double simulation_step; //= 0.1;	// in seconds

    double last_simulation_step_time{0};

    angle find_peak_lower_limit, find_peak_upper_limit;
    unsigned find_peak_try;

    bool keeps_in_find_peak_limit(angle add);
    void advance_angle_and_erase_old_contacts(double addang, angle sub_heading);
    void add_contact(angle absang, const contact& ct);

  public:
    sonar_operator();
    virtual ~sonar_operator() = default;
    // only make a simulation step each n seconds, n ca. 0.1 or so, simulate
    // human reaction on events.
    virtual void simulate(class game& gm, double delta_t);
    [[nodiscard]] const std::map<double, contact>& get_contacts() const { return contacts; }
    void load(const class xml_elem& parent);
    static void save(class xml_elem& parent);
};
