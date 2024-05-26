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

// user display: submarine's gauges
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#include "sub_gauges_display.hpp"

#include "game.hpp"
#include "submarine.hpp"
#include "user_interface.hpp"

namespace
{
enum element_type
{
    et_compass            = 1,
    et_bow_depth_rudder   = 2,
    et_stern_depth_rudder = 3,
    et_depth              = 4,
    et_knots              = 5,
    et_main_rudder        = 6,
    et_machine_telegraph  = 7,
    et_battery            = 8,
    et_compressor         = 9,
    et_diesel             = 10
};

auto get_gauges_type(user_interface& ui) -> const submarine::gauges_type
{
    return static_cast<submarine*>(ui.get_game().get_player())->get_gauges_type();
}
auto get_type(user_interface& ui) -> const char*
{
    auto gt = get_gauges_type(ui);
    if (gt == submarine::gauges_type::VII) {
        return "sub_gauges_VII";
    }
    return "sub_gauges_II";
}

auto throttle_to_value(submarine::throttle_status ts) -> int
{
    switch (ts) {
        case submarine::reversefull:
            return 0;
        case submarine::reversehalf:
            return 1;
        case submarine::reverse:
            return 2;
        case submarine::aheadlisten:
            return 10;
        case submarine::aheadslow:
            return 11;
        case submarine::aheadhalf:
            return 12;
        case submarine::aheadfull:
            return 13;
        case submarine::aheadflank:
            return 14;
        case submarine::stop:
            return 7;
        default:
            return 6; // some otherwise unused value
    }
}
} // namespace

sub_gauges_display::sub_gauges_display(user_interface& ui_)
    : user_display(ui_, get_type(ui_))
{
}

void sub_gauges_display::display() const
{
    auto* player = dynamic_cast<submarine*>(ui.get_game().get_player());
    element_for_id(et_compass).set_value(360.0 - player->get_heading().value());
    element_for_id(et_bow_depth_rudder).set_value(player->get_bow_rudder());
    element_for_id(et_stern_depth_rudder).set_value(player->get_stern_rudder());
    element_for_id(et_depth).set_value(player->get_depth());
    element_for_id(et_knots).set_value(helper::ms2kts(player->get_speed()));
    element_for_id(et_main_rudder).set_value(player->get_rudder_pos());
    element_for_id(et_machine_telegraph).set_value(double(throttle_to_value(player->get_throttle())) + 0.5);
    draw_elements();
}

auto sub_gauges_display::handle_mouse_button_event(const mouse_click_data& m) -> bool
{
    if (m.down()) {
        // fixme: actions are executed, but no messages are sent...
        auto* sub = dynamic_cast<submarine*>(ui.get_game().get_player());
        // if mouse is over control c, compute angle a, set matching command
        if (element_for_id(et_compass).is_mouse_over(m.position_2d)) {
            const auto compass_angle = element_for_id(et_compass).set_value(m.position_2d);
            if (compass_angle.has_value()) {
                sub->head_to_course(compass_angle.value());
            }
        } else if (element_for_id(et_depth).is_mouse_over(m.position_2d)) {
            const auto depth = element_for_id(et_depth).set_value_uint(m.position_2d);
            if (depth.has_value()) {
                sub->dive_to_depth(depth.value(), ui.get_game());
            }
        } else if (element_for_id(et_bow_depth_rudder).is_mouse_over(m.position_2d)) {
            auto angle_for_rudder = element_for_id(et_bow_depth_rudder).set_value(m.position_2d);
            if (angle_for_rudder.has_value()) {
                sub->set_bow_depth_rudder(
                    -std::clamp(angle_for_rudder.value() / sub->get_bow_rudder_max_angle(), -1.0, 1.0));
            }
        } else if (element_for_id(et_stern_depth_rudder).is_mouse_over(m.position_2d)) {
            auto angle_for_rudder = element_for_id(et_stern_depth_rudder).set_value(m.position_2d);
            if (angle_for_rudder.has_value()) {
                sub->set_stern_depth_rudder(
                    -std::clamp(angle_for_rudder.value() / sub->get_stern_rudder_max_angle(), -1.0, 1.0));
            }
        } else if (element_for_id(et_machine_telegraph).is_mouse_over(m.position_2d)) {
            // 270° in 15 steps, 45°-315°, so 18° per step.
            auto opt = element_for_id(et_machine_telegraph).set_value_uint(m.position_2d).value_or(15U);
            switch (opt) {
                case 0:
                    sub->set_throttle(ship::reversefull);
                    break;
                case 1:
                    sub->set_throttle(ship::reversehalf);
                    break;
                case 2:
                    sub->set_throttle(ship::reverse);
                    break;
                case 7:
                    sub->set_throttle(ship::stop);
                    break;
                case 10:
                    sub->set_throttle(ship::aheadlisten);
                    break;
                case 11:
                    sub->set_throttle(ship::aheadslow);
                    break;
                case 12:
                    sub->set_throttle(ship::aheadhalf);
                    break;
                case 13:
                    sub->set_throttle(ship::aheadfull);
                    break;
                case 14:
                    sub->set_throttle(ship::aheadflank);
                    break;
                case 3: // reverse small ?
                case 4: // loading (battery)
                case 5: // both machines 10 rpm less (?)
                case 6: // use electric engines
                case 8: // attention
                case 9: // diesel engines
                default:
                    break;
            }
        }
        return true;
    }
    return false;
}
