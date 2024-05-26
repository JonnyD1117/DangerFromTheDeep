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

// user display: submarine's tdc
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#include "sub_tdc2_display.hpp"

#include "game.hpp"
#include "log.hpp"
#include "submarine.hpp"
#include "submarine_interface.hpp"
#include "user_interface.hpp"

namespace
{
enum element_type
{
    et_tube1                       = 0,
    et_tube2                       = 1,
    et_tube3                       = 2,
    et_tube4                       = 3,
    et_tube5                       = 4,
    et_tube6                       = 5,
    et_firebutton                  = 6,
    et_mode                        = 7,
    et_leadangle_main              = 8,
    et_leadangle_refinement        = 9,
    et_brightness                  = 10,
    et_targetcourse_main           = 11,
    et_targetcourse_refinement     = 12,
    et_targetcourse_main_mkr       = 13,
    et_targetcourse_refinement_mkr = 14,
    et_targetdist_ptr              = 15,
    et_targetdist_mkr              = 16
};
}

sub_tdc2_display::sub_tdc2_display(user_interface& ui_)
    : user_display(ui_, "sub_tdc2")
{
}

auto sub_tdc2_display::handle_mouse_button_event(const mouse_click_data& m) -> bool
{
    auto& gm  = ui.get_game();
    auto* sub = dynamic_cast<submarine*>(gm.get_player());
    auto& si  = dynamic_cast<submarine_interface&>(ui);
    tdc& TDC  = sub->get_tdc();

    if (m.down() && m.left()) {
        // check if mouse is over tube indicators
        const auto nrtubes = sub->get_nr_of_bow_tubes() + sub->get_nr_of_stern_tubes();
        for (unsigned i = 0; i < nrtubes; ++i) {
            if (element_for_id(et_tube1 + i).is_mouse_over(m.position_2d)) {
                si.select_tube(i);
                log_debug("Torpedo tube selected: #" << i + 1);
                tubeselected_time = gm.get_time();
            }
        }

        // fire button
        if (element_for_id(et_firebutton).is_mouse_over(m.position_2d)) {
            si.fire_tube(sub, si.get_selected_tube());
        }

        // auto mode
        else if (element_for_id(et_mode).is_mouse_over(m.position_2d)) {
            TDC.set_auto_mode(!TDC.auto_mode_enabled());
        }
    }
    return true;
}

void sub_tdc2_display::display() const
{
    auto& gm               = ui.get_game();
    auto* player           = dynamic_cast<submarine*>(gm.get_player());
    const tdc& TDC         = player->get_tdc();
    unsigned selected_tube = dynamic_cast<const submarine_interface&>(ui).get_selected_tube();
    // draw tubes if ready
    const double blink_duration        = 3.0;
    const double blink_period_duration = 0.25;
    for (unsigned i = 0; i < 6; ++i) {
        bool visible = false;
        if (player->is_tube_ready(i)) {
            if (selected_tube != i || gm.get_time() > tubeselected_time + blink_duration
                || ((unsigned(floor((gm.get_time() - tubeselected_time) / blink_period_duration)) & 1) != 0U)) {
                visible = true;
            }
        }
        element_for_id(et_tube1 + i).set_visible(visible);
    }
    element_for_id(et_firebutton).set_visible(player->is_tube_ready(selected_tube) && TDC.solution_valid());
    // automatic fire solution on / off switch
    element_for_id(et_mode).set_phase(TDC.auto_mode_enabled() ? 0 : 1);
    // draw gyro pointers
    const auto leadangle = TDC.get_lead_angle();
    element_for_id(et_leadangle_main).set_value(leadangle.value()); // fixme order?!
    element_for_id(et_leadangle_refinement)
        .set_value(helper::mod(leadangle.value(), 10.0) * 36.0); // fixme 1 degree in 1/10th or 10 degrees?
    // target values (influenced by quality!)
    const auto tgtcourse = TDC.get_target_course().value();
    element_for_id(et_targetcourse_main).set_value(tgtcourse);
    element_for_id(et_targetcourse_refinement).set_value(helper::mod(tgtcourse, 10.0) * 36.0);
    // fixme later integrate them
    // element_for_id(et_targetcourse_main_mkr).set_value(tgtcourse);
    // element_for_id(et_targetcourse_refinement_mkr).set_value(helper::mod(tgtcourse,
    // 10.0) * 36.0);
    // target range
    element_for_id(et_targetdist_ptr).set_value(TDC.get_target_distance());
    // compute non-linear dial value fixme wtf - we must offer that in
    // user_display!!!
    // tgtrange = sqrt(12.61855670103 * tgtrange - 3685.567010309);
    element_for_id(et_brightness).set_value(45.0); // fixme show some sensible value

    draw_elements();
}
