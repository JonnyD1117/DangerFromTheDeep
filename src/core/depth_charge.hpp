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

// depth charges
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#pragma once

#include "sea_object.hpp"

// fixme: these values depend on depth charge type.
#define DEPTH_CHARGE_SINK_SPEED  4   // m/sec
#define DEADLY_DC_RADIUS_SURFACE 120 // meters
#define DEADLY_DC_RADIUS_200M    80
#define DAMAGE_DC_RADIUS_SURFACE 480 // meters, fixme realistic values?
#define DAMAGE_DC_RADIUS_200M    320

///\brief Represents a depth charge with simulation of it.
/** At the moment there are no specialisations for various types of depth
 * charges */
class depth_charge : public sea_object
{
  protected:
    double explosion_depth;

  public:
    /// Needed to store in map, don't use
    depth_charge(object_store<model>& model_store);                                        // for loading
    depth_charge(double expl_depth, const vector3& pos, object_store<model>& model_store); // for creation

    void load(const xml_elem& parent) override;
    void save(xml_elem& parent) const override;

    void simulate(double delta_time, game& gm) override;
    void compute_force_and_torque(vector3& F, vector3& T, game& gm) const override;
};
