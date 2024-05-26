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

// user display: submarine's torpedo setup
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#include "sub_torpsetup_display.hpp"

#include "game.hpp"
#include "log.hpp"
#include "submarine.hpp"
#include "submarine_interface.hpp"

namespace
{
enum element_type
{
    et_temperature       = 0,
    et_torpspeeddial     = 1,
    et_primaryrangedial  = 2,
    et_turnangledial     = 3,
    et_rundepth          = 4,
    et_secondaryrangeptr = 5,
    et_primaryrangeptr   = 6,
    et_torpspeed         = 7,
    et_firstturn         = 8,
    et_secondaryrange    = 9,
    et_preheating        = 10,
    et_primaryrangeknob  = 11,
    et_turnangleknob     = 12,
    et_rundepthknob      = 13
};
}

sub_torpsetup_display::sub_torpsetup_display(user_interface& ui_)
    : user_display(ui_, "sub_torpsetup")
{
}

auto sub_torpsetup_display::handle_mouse_button_event(const mouse_click_data& m) -> bool
{
    auto& gm                = ui.get_game();
    auto* sub               = dynamic_cast<submarine*>(gm.get_player());
    auto& tbsetup           = sub->get_torp_in_tube(dynamic_cast<submarine_interface&>(ui).get_selected_tube()).setup;
    which_element_is_turned = -1;
    if (m.down() && m.left()) {
        // check if mouse is over turn knobs
        if (element_for_id(et_primaryrangeknob).is_mouse_over(m.position_2d)) {
            which_element_is_turned = et_primaryrangeknob;
        } else if (element_for_id(et_turnangleknob).is_mouse_over(m.position_2d)) {
            which_element_is_turned = et_turnangleknob;
        } else if (element_for_id(et_rundepthknob).is_mouse_over(m.position_2d)) {
            which_element_is_turned = et_rundepthknob;
        } else if (element_for_id(et_firstturn).is_mouse_over(m.position_2d)) {
            tbsetup.initialturn_left = !tbsetup.initialturn_left;
        } else if (element_for_id(et_secondaryrange).is_mouse_over(m.position_2d)) {
            tbsetup.short_secondary_run = !tbsetup.short_secondary_run;
        } else if (element_for_id(et_preheating).is_mouse_over(m.position_2d)) {
            tbsetup.preheating = !tbsetup.preheating;
        } else if (element_for_id(et_torpspeed).is_mouse_over(m.position_2d)) {
            tbsetup.torpspeed = (tbsetup.torpspeed + 1) % 3;
        }
        return true;
    }
    return m.up() && m.left();
}

auto sub_torpsetup_display::handle_mouse_motion_event(const mouse_motion_data& m) -> bool
{
    auto& gm      = ui.get_game();
    auto* sub     = dynamic_cast<submarine*>(gm.get_player());
    auto& tbsetup = sub->get_torp_in_tube(dynamic_cast<submarine_interface&>(ui).get_selected_tube()).setup;
    if (m.left()) {
        if (which_element_is_turned != -1) {
            auto& elem = element_for_id(which_element_is_turned);
            elem.set_value(angle(elem.get_value() + m.relative_motion.x * 50.0).value()); // fixme scaling
            switch (which_element_is_turned) {
                case et_primaryrangeknob:
                    element_for_id(et_primaryrangedial).set_angle(elem.get_value());
                    tbsetup.primaryrange = element_for_id(et_primaryrangedial).get_value();
                    break;
                case et_turnangleknob:
                    // tbsetup.turnangle = elem.get_value();
                    break;
                case et_rundepthknob:
                    element_for_id(et_rundepth).set_angle(elem.get_value());
                    tbsetup.rundepth = element_for_id(et_rundepth).get_value();
                    break;
                default:
                    break;
            }
        }
    }
    return false;
}

void sub_torpsetup_display::display() const
{
    auto& gm  = ui.get_game();
    auto* sub = dynamic_cast<submarine*>(gm.get_player());

    // If we had a separate virtual method that gathers data from game and
    // stores it in the displays we wouldn't need to make value mutable.

    element_for_id(et_temperature).set_value(helper::mod(gm.get_time(), 35.0));   // a test
    element_for_id(et_torpspeeddial).set_value(helper::mod(gm.get_time(), 55.0)); // a test
    // get tube settings
    const auto& tbsetup = sub->get_torp_in_tube(dynamic_cast<submarine_interface&>(ui).get_selected_tube()).setup;
    element_for_id(et_primaryrangedial).set_value(double(tbsetup.primaryrange));
    element_for_id(et_turnangledial).set_value(tbsetup.turnangle.value()); // 0...240 degrees for LUT, 180 for FAT.
    element_for_id(et_torpspeed).set_phase(tbsetup.torpspeed);
    element_for_id(et_firstturn).set_phase(tbsetup.initialturn_left ? 0 : 1);
    element_for_id(et_secondaryrange).set_phase(tbsetup.short_secondary_run ? 0 : 1);
    element_for_id(et_preheating).set_phase(tbsetup.preheating ? 1 : 0);
    element_for_id(et_rundepth).set_value(double(tbsetup.rundepth));
    element_for_id(et_secondaryrangeptr)
        .set_value(helper::mod(gm.get_time(), 1600.0)); // fixme tbsetup.secondaryrange atm only
                                                        // 800/1600 what was realistic?
    element_for_id(et_primaryrangeptr).set_value(helper::mod(gm.get_time(), 1600.0)); // fixme tbsetup.primaryrange
    // fixme no element for the LUT angle, the angle that LUT turns to after
    // first run. Or is THAT the turn angle and LUT turns always 180 between
    // runs? FAT can do 90° or 180° turns but we can only use 180°. Primary run
    // length is 1200 or 1900m we have 800m and 1600m. The turn angle is the LUT
    // angle to turn to after initial run. Primary range can be chosen as what?
    // We have 0-1600m.
    draw_elements();
}
