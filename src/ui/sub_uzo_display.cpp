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

// user display: submarine's UZO
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#include "sub_uzo_display.hpp"

#include "cfg.hpp"
#include "game.hpp"
#include "keys.hpp"
#include "submarine.hpp"
#include "submarine_interface.hpp"
#include "system_interface.hpp"

namespace
{
enum element_type
{
    et_direction = 0
};
}

void sub_uzo_display::pre_display() const
{
    glClear(GL_DEPTH_BUFFER_BIT);
}

auto sub_uzo_display::get_projection_data(class game& gm) const -> freeview_display::projection_data
{
    projection_data pd;
    pd.x = SYS().get_res_area_2d_x();
    pd.y = SYS().get_res_area_2d_y();
    pd.w = SYS().get_res_area_2d_w();
    pd.h = SYS().get_res_area_2d_h();
    // with normal fov of 70 degrees, this is 1.5 / 6.0 magnification
    pd.fov_x      = zoomed ? 13.31 : 50.05; // fixme: historic values?
    pd.near_z     = 1.0;
    pd.far_z      = gm.get_max_view_distance();
    pd.fullscreen = true;
    return pd;
}

void sub_uzo_display::set_modelview_matrix(game& gm, const vector3& /*viewpos*/) const
{
    glLoadIdentity();

    // set up rotation (player's view direction)
    // limit elevation to -20...20 degrees.
    float elev        = -ui.get_elevation().value();
    const float LIMIT = 20.0F;
    if (elev < -LIMIT - 90.0F) {
        elev = -LIMIT - 90.0F;
    }
    if (elev > +LIMIT - 90.0F) {
        elev = +LIMIT - 90.0F;
    }
    glRotated(elev, 1, 0, 0);

    // if we're aboard the player's vessel move the world instead of the ship
    if (aboard) {
        // This should be a negative angle, but nautical view dir is clockwise,
        // OpenGL uses ccw values, so this is a double negation
        glRotated(ui.get_relative_bearing().value(), 0, 0, 1);
        gm.get_player()->get_orientation().conj().rotmat4().multiply_gl();
    } else {
        // This should be a negative angle, but nautical view dir is clockwise,
        // OpenGL uses ccw values, so this is a double negation
        glRotated(ui.get_absolute_bearing().value(), 0, 0, 1);
    }

    // set up modelview matrix as if player is at position (0, 0, 0), so do NOT
    // set a translational part. This is done to avoid rounding errors caused by
    // large x/y values (modelview matrix seems to store floats, but coordinates
    // are in real meters, so float is not precise enough).
}

void sub_uzo_display::post_display() const
{
    auto& gm = ui.get_game();
    if (gm.is_valid(gm.get_player()->get_target())) {
        projection_data pd = get_projection_data(gm);
        ui.show_target(pd.x, pd.y, pd.w, pd.h, get_viewpos(gm));
    }

    element_for_id(et_direction).set_value(ui.get_relative_bearing().value());
    draw_elements();
}

sub_uzo_display::sub_uzo_display(user_interface& ui_)
    : freeview_display(ui_, "sub_uzo")
{
    auto* sub             = static_cast<submarine*>(ui_.get_game().get_player());
    add_pos               = sub->get_uzo_position();
    aboard                = true;
    withunderwaterweapons = false;
    drawbridge            = false;
}

auto sub_uzo_display::handle_key_event(const key_data& k) -> bool
{
    if (k.down()) {
        if (is_configured_key(key_command::TOGGLE_ZOOM_OF_VIEW, k)) {
            zoomed = !zoomed;
            return true;
        }
        if (k.is_keypad_number()) {
            // filter away keys NP_1...NP_9 to avoid moving viewer like in
            // freeview mode
            return true;
        }
    }
    return freeview_display::handle_key_event(k);
}

auto sub_uzo_display::handle_mouse_wheel_event(const mouse_wheel_data& m) -> bool
{
    if (m.up()) {
        zoomed = true;
        return true;
    }
    if (m.down()) {
        zoomed = false;
        return true;
    }
    return freeview_display::handle_mouse_wheel_event(m);
}

auto sub_uzo_display::get_popup_allow_mask() const -> unsigned
{
    return (1 << submarine_interface::popup_mode_ecard);
}
