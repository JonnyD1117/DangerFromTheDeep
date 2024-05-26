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

// Object to display soldbook
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#include "sub_soldbuch_display.hpp"

#include "font.hpp"
#include "game.hpp"
#include "global_data.hpp"
#include "image.hpp"
#include "submarine_interface.hpp"
#include "system_interface.hpp"
#include "texts.hpp"
#include "texture.hpp"

#include <fstream>
#include <memory>
#include <sstream>
#include <utility>

namespace
{
enum element_type
{
    et_photo   = 0,
    et_overlay = 1,
    et_stamp   = 2
};
}

sub_soldbuch_display::sub_soldbuch_display(user_interface& ui_)
    : user_display(ui_, "sub_soldbuch")
{
    const auto& gm = ui.get_game();
    const auto& pi = gm.get_player_info();
    element_for_id(et_photo).set_phase(pi.photo - 1 /* first foto starts at 1*/);
    element_for_id(et_stamp).set_phase(gm.get_date().get_value(date::year) - 1939);
}

void sub_soldbuch_display::display() const
{
    draw_elements();

    // just for simplier handling of coords
    std::stringstream ss;

    // draw background
    SYS().prepare_2d_drawing();

    // specify the primary ovberlay's coords
    auto offset = element_for_id(et_overlay).get_position();
    offset.y -= 6; // fixme needed

    const auto& gm = ui.get_game();
    const auto& pi = gm.get_player_info();

    // soldbuch nr
    auto& myfont = *font_jphsl;
    myfont.print(offset.x + 140, offset.y + 45, pi.soldbuch_nr, color(20, 20, 30));
    // rank
    myfont.print(offset.x + 30, offset.y + 79, texts::get(700), color(20, 20, 30));
    // paygroup
    ss << "A" << 2;
    myfont.print(offset.x + 230, offset.y + 81, ss.str(), color(20, 20, 30));

    // career
    if (pi.career.size() > 1) {
        ss.str("");
        ss << "A" << 2 + 1;
        myfont.print(offset.x + 25, offset.y + 140, *pi.career.begin(), color(20, 20, 30));
        myfont.print(offset.x + 100, offset.y + 140, texts::get(700 + 1), color(20, 20, 30));
        myfont.print(offset.x + 270, offset.y + 140, ss.str(), color(20, 20, 30));
        if (pi.career.size() >= 2) {
            unsigned i = 1;
            auto it    = pi.career.begin();
            for (it++; it != pi.career.end(); ++it) {
                ss.str("");
                ss << "A" << (2 + 1 + i);
                myfont.print(offset.x + 25, offset.y + 150 + (i * 20), *it, color(20, 20, 30));
                myfont.print(offset.x + 100, offset.y + 150 + (i * 20), texts::get(700 + 1 + i), color(20, 20, 30));
                myfont.print(offset.x + 270, offset.y + 150 + (i * 20), ss.str(), color(20, 20, 30));
                i++;
            }
        }
    }
    // player name
    myfont.print(offset.x + 20, offset.y + 270, pi.name, color(20, 20, 30));
    // bloodgroup
    myfont.print(offset.x + 70, offset.y + 340, pi.bloodgroup, color(20, 20, 30));
    // gasmask_size
    myfont.print(offset.x + 90, offset.y + 364, pi.gasmask_size, color(20, 20, 30));
    // marineroll
    myfont.print(offset.x + 125, offset.y + 389, pi.marine_roll, color(20, 20, 30));
    // marinegroup
    ss.str("");
    ss << pi.flotilla;
    std::string flotname = texts::get(164);
    flotname.replace(flotname.find('#'), 1, ss.str());
    myfont.print(offset.x + 95, offset.y + 438, flotname, color(20, 20, 30));
    // identification
    myfont.print(offset.x + 125, offset.y + 313, pi.name + "/" + pi.soldbuch_nr, color(20, 20, 30));

    SYS().unprepare_2d_drawing();
}
