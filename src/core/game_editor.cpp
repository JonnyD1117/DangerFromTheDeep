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

// game editor
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "airplane.hpp"
#include "convoy.hpp"
#include "datadirs.hpp"
#include "depth_charge.hpp"
#include "game_editor.hpp"
#include "gun_shell.hpp"
#include "matrix4.hpp"
#include "model.hpp"
#include "particle.hpp"
#include "quaternion.hpp"
#include "sensors.hpp"
#include "ship.hpp"
#include "submarine.hpp"
#include "system_interface.hpp"
#include "texts.hpp"
#include "torpedo.hpp"
#include "water_splash.hpp"

#include <cfloat>
#include <sstream>
#include <utility>

using std::string;
using std::vector;

// TODO(tj): move saved games layout constants elsewhere

constexpr auto SAVEVERSION = 1;
constexpr auto GAMETYPE    = 0; // fixme, 0-mission , 1-patrol etc.

/***************************************************************************/

game_editor::game_editor(const date& start_date)
{
    time           = start_date.get_time() + 86400 / 2; // 12.00 o'clock
    equipment_date = start_date;

    // standard sub type, can be changed later
    string subtype = "submarine_VIIc";

    for (unsigned i = 0; i < 1 /*nr_of_players*/; ++i) {
        xml_doc doc(data_file().get_filename(subtype));
        doc.load();
        submarine sub(equipment_date, get_model_store(), doc.first_child());
        sub.set_skin_layout(model::default_layout);
        sub.init_fill_torpedo_tubes(start_date);
        sub.manipulate_invulnerability(true);
        auto& [id, thesub] = spawn_submarine(std::move(sub));
        if (i == 0) {
            player    = &thesub;
            player_id = id;
            compute_max_view_dist();
        }
    }

    my_run_state    = running;
    last_trail_time = time - TRAIL_TIME;
}

// --------------------------------------------------------------------------------
//                        LOAD GAME (SAVEGAME OR MISSION)
// --------------------------------------------------------------------------------
game_editor::game_editor(const string& filename)
    : game(filename)
{
    // nothing special for now
}

/*
game_editor::~game_editor()
{
}
*/

// copied from class game
template<class T>
void cleanup(std::vector<T>& s)
{
    for (unsigned i = 0; i < s.size(); ++i) {
        if (s[i] && s[i]->is_defunct()) {
            s.reset(i);
        }
    }
    s.compact();
}

void game_editor::manipulate_time(double tm)
{
    time = tm;
}

void game_editor::manipulate_equipment_date(date equipdate)
{
    equipment_date = equipdate;
}
