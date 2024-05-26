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

// user display: submarine's ghg (Gruppenhorchgeraet) hearing device
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#include "sub_ghg_display.hpp"

#include "global_data.hpp"

namespace
{
enum element_type
{
    et_none           = -1,
    et_direction_ptr  = 0,
    et_direction_knob = 1,
    et_volume_dial    = 2,
    et_volume_knob    = 3,
};
}

sub_ghg_display::sub_ghg_display(user_interface& ui_)
    : user_display(ui_, "sub_ghg")
{
}

auto sub_ghg_display::handle_mouse_button_event(const mouse_click_data& m) -> bool
{
    which_element_is_turned = et_none;
    if (m.down()) {
        // check if mouse is over turn knobs
        if (element_for_id(et_direction_knob).is_mouse_over(m.position_2d)) {
            which_element_is_turned = et_direction_knob;
            return true;
        }
        if (element_for_id(et_volume_knob).is_mouse_over(m.position_2d, 128)) {
            which_element_is_turned = et_volume_knob;
            return true;
        }
    } else if (m.up()) {
        return true;
    }
    return false;
}

auto sub_ghg_display::handle_mouse_motion_event(const mouse_motion_data& m) -> bool
{
    // fixme here and for sub_bg just use the elements directly, like with kdb!
    if (m.left()) {
        if (which_element_is_turned != et_none) {
            auto& elem = element_for_id(which_element_is_turned);
            elem.set_value(angle(elem.get_value() + m.relative_motion.x * 100.0).value());
            return true;
        }
    }
    return false;
}

void sub_ghg_display::display() const
{
    element_for_id(et_direction_ptr).set_value(element_for_id(et_direction_knob).get_value());
    element_for_id(et_volume_dial).set_value(element_for_id(et_volume_knob).get_value());
    draw_elements();

    // get hearing device angle from submarine, if it has one

    // test hack: test signal strengths
    // 	angle sonar_ang = angle(turnknobang[TK_DIRECTION]*0.5) +
    // player->get_heading(); 	vector<double> noise_strengths =
    // gm.sonar_listen_ships(player, sonar_ang); 	printf("noise strengths,
    // global ang=%f, L=%f M=%f H=%f U=%f\n", 	       sonar_ang.value(),
    // noise_strengths[0], noise_strengths[1], noise_strengths[2],
    // noise_strengths[3]);
}
