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

// user display: submarine's bg hearing device
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#include "sub_bg_display.hpp"

namespace
{
enum element_type
{
    et_none     = -1,
    et_pointer  = 0,
    et_turnknob = 1
};
}

sub_bg_display::sub_bg_display(user_interface& ui_)
    : user_display(ui_, "sub_bg")
{
}

auto sub_bg_display::handle_mouse_button_event(const mouse_click_data& m) -> bool
{
    which_element_is_turned = et_none;
    if (m.down()) {
        // check if mouse is over turn knobs
        if (element_for_id(et_turnknob).is_mouse_over(m.position_2d)) {
            which_element_is_turned = et_turnknob;
            return true;
        }
    } else if (m.up()) {
        return true;
    }
    return false;
}

auto sub_bg_display::handle_mouse_motion_event(const mouse_motion_data& m) -> bool
{
    if (m.left()) {
        if (which_element_is_turned != et_none) {
            auto& elem = element_for_id(which_element_is_turned);
            elem.set_value(angle(elem.get_value() + m.relative_motion.x * 100.0).value());
            return true;
        }
    }
    return false;
}

void sub_bg_display::display() const
{
    // set angle in elements and draw them
    element_for_id(et_pointer).set_value(element_for_id(et_turnknob).get_value());
    draw_elements();
}
