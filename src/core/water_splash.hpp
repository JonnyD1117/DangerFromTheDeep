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

// water splash (C)+(W) 2020 Thorsten Jordan

#pragma once

#include "bspline.hpp"
#include "sea_object.hpp"

#include <vector>

/// A water splash from some weapon impact etc.
class water_splash : public sea_object
{
  protected:
    double resttime;
    double lifetime;
    double risetime;
    double riseheight;
    std::unique_ptr<bspline> bradius_top;
    std::unique_ptr<bspline> bradius_bottom;
    std::unique_ptr<bspline> balpha;

    static void render_cylinder(
        double radius_bottom,
        double radius_top,
        double height,
        double alpha,
        const texture& tex,
        double u_scal    = 2.0,
        unsigned nr_segs = 16);

    [[nodiscard]] double compute_height(double t) const;

  public:
    water_splash() = default;
    water_splash(const vector3& pos, object_store<model>& model_store, double risetime = 0.4, double riseheight = 25.0);
    void simulate(double delta_time, game& gm) override;
    void display() const;
    void display_mirror_clip() const override;
    void compute_force_and_torque(vector3& F, vector3& T, game& gm) const override { } // static object, no acceleration
    static auto torpedo(const vector3& pos, object_store<model>& model_store)
    {
        return water_splash(pos, model_store, 0.4, 20.0);
    }
    static auto depth_charge(const vector3& pos, object_store<model>& model_store)
    {
        return water_splash(pos, model_store, 0.6, 30.0);
    }
    static auto gun_shell(const vector3& pos, object_store<model>& model_store)
    {
        return water_splash(pos, model_store, 0.25, 12.5);
    }
};
