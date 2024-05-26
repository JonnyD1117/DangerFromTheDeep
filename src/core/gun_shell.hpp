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

// gun shells
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#pragma once

#include "sea_object.hpp"
class ship;

/// Represents a gun shell with simulation of it.
class gun_shell : public sea_object
{
  public:
    gun_shell(object_store<model>& model_store); // for loading
    gun_shell(
        const vector3& pos,
        angle direction,
        angle elevation,
        double initial_velocity,
        double damage,
        double caliber_,
        object_store<model>& model_store); // for creation

    void load(const xml_elem& parent) override;
    void save(xml_elem& parent) const override;
    [[nodiscard]] auto get_caliber() const { return caliber; }

    void simulate(double delta_time, game& gm) override;
    virtual void display() const;
    [[nodiscard]] float surface_visibility(const vector2& watcher) const override;
    // acceleration is only gravity and already handled by sea_object
    [[nodiscard]] virtual double damage() const { return damage_amount; }

  protected:
    vector3 oldpos; // position at last iteration (for collision detection)
    double damage_amount{0};
    /// gun/shell caliber
    double caliber{0};

    void check_collision(game& gm);
    void check_collision_precise(game& gm, const ship& s, const vector3& oldrelpos, const vector3& newrelpos);
    void check_collision_voxel(game& gm, const ship& s, const vector3f& oldrelpos, const vector3f& newrelpos);
};
