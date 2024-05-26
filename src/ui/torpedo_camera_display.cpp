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

#include "torpedo_camera_display.hpp"

#include "game.hpp"
#include "image.hpp"
#include "submarine_interface.hpp"
#include "system_interface.hpp"
#include "texture.hpp"
#include "torpedo.hpp"

#include <utility>

void torpedo_camera_display::pre_display() const
{
    if (trackobj == nullptr) {
        return;
    }
    if (!trackobj->is_reference_ok()) {
        trackobj = nullptr;
        return;
    }
    glClear(GL_DEPTH_BUFFER_BIT);
}

auto torpedo_camera_display::get_projection_data(class game& gm) const -> freeview_display::projection_data
{
    projection_data pd;
    pd.x          = SYS().get_res_x() * 3 / 4;
    pd.y          = 0;
    pd.w          = SYS().get_res_x() / 4;
    pd.h          = SYS().get_res_y() / 4;
    pd.fov_x      = 70.0;
    pd.near_z     = 1.0;
    pd.far_z      = gm.get_max_view_distance();
    pd.fullscreen = false;
    return pd;
}

void torpedo_camera_display::post_display() const
{
    if (trackobj == nullptr) {
        return;
    }
    // nothing to do
}

auto torpedo_camera_display::get_viewpos(class game& /*gm*/) const -> vector3
{
    if (trackobj != nullptr) {
        return trackobj->get_pos() + add_pos;
    }
    return {};
}

torpedo_camera_display::torpedo_camera_display(user_interface& ui_)
    : freeview_display(ui_)
    , trackobj(nullptr)
{
    add_pos               = vector3(0, 0, 0.5); // on the back of the torpedo like riding a whale...
    aboard                = true;               // ?
    withunderwaterweapons = true;
    drawbridge            = false;
}

auto torpedo_camera_display::get_popup_allow_mask() const -> unsigned
{
    return 0;
}

void torpedo_camera_display::enter(bool is_day)
{
}

void torpedo_camera_display::leave()
{
}
