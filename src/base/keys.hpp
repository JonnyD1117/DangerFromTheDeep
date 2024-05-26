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

// key name definitions
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#pragma once
#include <cstddef>

/// All commands to the simulation that can be triggered by a key press
enum class key_command
{
    ZOOM_MAP,
    UNZOOM_MAP,
    SHOW_GAUGES_SCREEN,
    SHOW_PERISCOPE_SCREEN,
    SHOW_UZO_SCREEN,
    SHOW_BRIDGE_SCREEN,
    SHOW_MAP_SCREEN,
    SHOW_TORPEDO_SCREEN,
    SHOW_DAMAGE_CONTROL_SCREEN,
    SHOW_LOGBOOK_SCREEN,
    SHOW_SUCCESS_RECORDS_SCREEN,
    SHOW_FREEVIEW_SCREEN,
    RUDDER_LEFT,
    RUDDER_HARD_LEFT,
    RUDDER_RIGHT,
    RUDDER_HARD_RIGHT,
    RUDDER_UP,
    RUDDER_HARD_UP,
    RUDDER_DOWN,
    RUDDER_HARD_DOWN,
    CENTER_RUDDERS,
    THROTTLE_LISTEN,
    THROTTLE_SLOW,
    THROTTLE_HALF,
    THROTTLE_FULL,
    THROTTLE_FLANK,
    THROTTLE_STOP,
    THROTTLE_REVERSE,
    THROTTLE_REVERSEHALF,
    THROTTLE_REVERSEFULL,
    FIRE_TUBE_1,
    FIRE_TUBE_2,
    FIRE_TUBE_3,
    FIRE_TUBE_4,
    FIRE_TUBE_5,
    FIRE_TUBE_6,
    SELECT_TARGET,
    SCOPE_UP_DOWN,
    CRASH_DIVE,
    GO_TO_SNORKEL_DEPTH,
    TOGGLE_SNORKEL,
    SET_HEADING_TO_VIEW,
    IDENTIFY_TARGET,
    GO_TO_PERISCOPE_DEPTH,
    GO_TO_SURFACE,
    FIRE_TORPEDO,
    SET_VIEW_TO_HEADING,
    TOGGLE_ZOOM_OF_VIEW,
    TURN_VIEW_LEFT,
    TURN_VIEW_LEFT_FAST,
    TURN_VIEW_RIGHT,
    TURN_VIEW_RIGHT_FAST,
    TIME_SCALE_UP,
    TIME_SCALE_DOWN,
    FIRE_DECK_GUN,
    TOGGLE_RELATIVE_BEARING,
    TOGGLE_MAN_DECK_GUN,
    SHOW_TDC_SCREEN,
    TOGGLE_POPUP,
    SHOW_TORPSETUP_SCREEN,
    SHOW_TORPEDO_CAMERA,
    TAKE_SCREENSHOT,
    SHOW_TDC2_SCREEN,
    SHOW_VALVES_SCREEN,
    number
};

struct key_name
{
    key_command nr;
    const char* name;
};

extern key_name key_names[std::size_t(key_command::number)];
