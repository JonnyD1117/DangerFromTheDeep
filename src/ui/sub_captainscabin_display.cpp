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

// user display: submarine's torpedo room
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#include "sub_captainscabin_display.hpp"

#include "font.hpp"
#include "game.hpp"
#include "global_data.hpp"
#include "submarine_interface.hpp"
#include "system_interface.hpp"
#include "texts.hpp"

namespace
{
enum element_type
{
    et_begin       = 0,
    et_soldbuch    = 0,
    et_logbook     = 1,
    et_torpedoes   = 2,
    et_recogmanual = 3,
    et_successes   = 4,
    et_end         = 5
};

void do_action(submarine_interface& s, element_type t)
{
    switch (t) {
        case et_soldbuch:
            s.goto_soldbuch();
            break;
        case et_logbook:
            s.goto_logbook();
            break;
        case et_torpedoes:
            s.goto_torpedomanagement();
            break;
        case et_recogmanual:
            s.goto_recogmanual();
            break;
        case et_successes:
            s.goto_successes();
            break;
        default:
            THROW(error, "invalid element type");
    }
}

auto get_description(element_type t) -> int
{
    switch (t) {
        case et_soldbuch:
            return 274;
        case et_logbook:
            return 255;
        case et_torpedoes:
            return 253;
        case et_recogmanual:
            return 273;
        case et_successes:
            return 272;
        default:
            THROW(error, "invalid element type");
    }
}

auto get_color(element_type t) -> color
{
    switch (t) {
        case et_soldbuch:
            return {224, 224, 224};
        case et_logbook:
            return {224, 224, 255};
        case et_torpedoes:
            return {224, 255, 224};
        case et_recogmanual:
            return {255, 224, 224};
        case et_successes:
            return {255, 224, 224};
        default:
            THROW(error, "invalid element type");
    }
}
} // namespace

sub_captainscabin_display::sub_captainscabin_display(user_interface& ui_)
    : user_display(ui_, "sub_captainscabin")
{
}

void sub_captainscabin_display::display() const
{
    draw_elements();

    SYS().prepare_2d_drawing();
    auto& myfont = *font_vtremington12;
    for (int i = et_begin; i != et_end; ++i) {
        if (element_for_id(i).is_mouse_over(mouse_position)) {
            myfont.print_hc(
                mouse_position.x,
                mouse_position.y - myfont.get_height(),
                texts::get(get_description(element_type(i))),
                get_color(element_type(i)),
                true);
            break;
        }
    }
    SYS().unprepare_2d_drawing();
}

auto sub_captainscabin_display::handle_mouse_button_event(const mouse_click_data& m) -> bool
{
    if (m.down()) {
        // just memorize
        mouse_position = m.position_2d;
    } else if (m.up()) {
        mouse_position = m.position_2d;
        if (m.left()) {
            for (int i = et_begin; i != et_end; ++i) {
                if (element_for_id(i).is_mouse_over(mouse_position)) {
                    do_action(static_cast<submarine_interface&>(ui), element_type(i));
                    return true;
                }
            }
        }
    }
    return false;
}

auto sub_captainscabin_display::handle_mouse_motion_event(const mouse_motion_data& m) -> bool
{
    mouse_position = m.position_2d;
    return false;
}
