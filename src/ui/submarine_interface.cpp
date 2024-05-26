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

// user interface for controlling a submarine
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#include "submarine_interface.hpp"

#include "cfg.hpp"
#include "date.hpp"
#include "game.hpp"
#include "global_data.hpp"
#include "image.hpp"
#include "keys.hpp"
#include "log.hpp"
#include "logbook_display.hpp"
#include "map_display.hpp"
#include "music.hpp"
#include "particle.hpp" // for hack, fixme
#include "ships_sunk_display.hpp"
#include "sub_bg_display.hpp"
#include "sub_bridge_display.hpp"
#include "sub_captainscabin_display.hpp"
#include "sub_control_popup.hpp"
#include "sub_damage_display.hpp"
#include "sub_ecard_popup.hpp"
#include "sub_gauges_display.hpp"
#include "sub_ghg_display.hpp"
#include "sub_kdb_display.hpp"
#include "sub_periscope_display.hpp"
#include "sub_recogmanual_display.hpp"
#include "sub_recogmanual_popup.hpp"
#include "sub_soldbuch_display.hpp"
#include "sub_tdc2_display.hpp"
#include "sub_tdc_display.hpp"
#include "sub_tdc_popup.hpp"
#include "sub_torpedo_display.hpp"
#include "sub_torpsetup_display.hpp"
#include "sub_uzo_display.hpp"
#include "sub_valves_display.hpp"
#include "submarine_interface.hpp"
#include "system_interface.hpp"
#include "texts.hpp"
#include "torpedo_camera_display.hpp"
#include "widget.hpp"

#include <list>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <utility>

using namespace std;

submarine_interface::submarine_interface(game& gm)
    : user_interface(gm)
    , selected_tube(0)
    , torpedo_cam_track_nr(0)
{
    auto* player = dynamic_cast<submarine*>(gm.get_player());

    displays.resize(nr_of_displays);
    displays[display_mode_gauges]        = std::make_unique<sub_gauges_display>(*this);
    displays[display_mode_valves]        = std::make_unique<sub_valves_display>(*this);
    displays[display_mode_periscope]     = std::make_unique<sub_periscope_display>(*this);
    displays[display_mode_uzo]           = std::make_unique<sub_uzo_display>(*this);
    displays[display_mode_bridge]        = std::make_unique<sub_bridge_display>(*this);
    displays[display_mode_map]           = std::make_unique<map_display>(*this);
    displays[display_mode_torpedoroom]   = std::make_unique<sub_torpedo_display>(*this);
    displays[display_mode_damagestatus]  = std::make_unique<sub_damage_display>(*this);
    displays[display_mode_logbook]       = std::make_unique<logbook_display>(*this);
    displays[display_mode_captainscabin] = std::make_unique<sub_captainscabin_display>(*this);
    displays[display_mode_soldbuch]      = std::make_unique<sub_soldbuch_display>(*this);
    displays[display_mode_successes]     = std::make_unique<ships_sunk_display>(*this);
    displays[display_mode_recogmanual]   = std::make_unique<sub_recogmanual_display>(*this);
    switch (player->get_hearing_device_type()) {
        case submarine::hearing_device_KDB:
            displays[display_mode_sonar] = std::make_unique<sub_kdb_display>(*this);
            break;
        default:
        case submarine::hearing_device_GHG:
            displays[display_mode_sonar] = std::make_unique<sub_ghg_display>(*this);
            break;
        case submarine::hearing_device_BG:
            displays[display_mode_sonar] = std::make_unique<sub_bg_display>(*this);
            break;
    }
    displays[display_mode_freeview]  = std::make_unique<freeview_display>(*this);
    displays[display_mode_tdc]       = std::make_unique<sub_tdc_display>(*this);
    displays[display_mode_tdc2]      = std::make_unique<sub_tdc2_display>(*this);
    displays[display_mode_torpsetup] = std::make_unique<sub_torpsetup_display>(*this);

    // fixme: use texture caches here too...
    popups.resize(nr_of_popups);
    popups[popup_mode_control]     = std::make_unique<sub_control_popup>(*this);
    popups[popup_mode_tdc]         = std::make_unique<sub_tdc_popup>(*this);
    popups[popup_mode_ecard]       = std::make_unique<sub_ecard_popup>(*this);
    popups[popup_mode_recogmanual] = std::make_unique<sub_recogmanual_popup>(*this);

    // torpedo cam
    torpedo_cam_view = std::make_unique<torpedo_camera_display>(*this);

    // note: we could change the width of the menu dynamically, according to
    // longest text of the buttons.
    // 	int maxs = 0;
    // 	for (unsigned i = 247; i <= 260; ++i)
    // 		maxs =
    // std::max(widget::get_theme()->myfont->get_size(texts::get(i)).x, maxs);
    auto screen_selector_menu = std::make_unique<widget_menu>(0, 0, /*maxs + 16*/ 256, 32, texts::get(247));
    screen_selector_menu->set_entry_spacing(0);

    screen_selector_menu->add_entry(
        texts::get(248), std::make_unique<widget_caller_button<>>([this]() { goto_gauges(); }));
    screen_selector_menu->add_entry(
        texts::get(249), std::make_unique<widget_caller_button<>>([this]() { goto_periscope(); }));
    screen_selector_menu->add_entry(
        texts::get(250), std::make_unique<widget_caller_button<>>([this]() { goto_UZO(); }));
    screen_selector_menu->add_entry(
        texts::get(251), std::make_unique<widget_caller_button<>>([this]() { goto_bridge(); }));
    screen_selector_menu->add_entry(
        texts::get(252), std::make_unique<widget_caller_button<>>([this]() { goto_map(); }));
    screen_selector_menu->add_entry(
        texts::get(253), std::make_unique<widget_caller_button<>>([this]() { goto_torpedomanagement(); }));
    screen_selector_menu->add_entry(
        texts::get(254), std::make_unique<widget_caller_button<>>([this]() { goto_damagecontrol(); }));
    screen_selector_menu->add_entry(
        texts::get(271), std::make_unique<widget_caller_button<>>([this]() { goto_captainscabin(); }));
    screen_selector_menu->add_entry(
        texts::get(255), std::make_unique<widget_caller_button<>>([this]() { goto_logbook(); }));
    screen_selector_menu->add_entry(
        texts::get(274), std::make_unique<widget_caller_button<>>([this]() { goto_soldbuch(); }));
    screen_selector_menu->add_entry(
        texts::get(272), std::make_unique<widget_caller_button<>>([this]() { goto_successes(); }));
    screen_selector_menu->add_entry(
        texts::get(256), std::make_unique<widget_caller_button<>>([this]() { goto_sonar(); }));
    screen_selector_menu->add_entry(
        texts::get(257), std::make_unique<widget_caller_button<>>([this]() { goto_freeview(); }));
    screen_selector_menu->add_entry(
        texts::get(258), std::make_unique<widget_caller_button<>>([this]() { goto_TDC(); }));
    screen_selector_menu->add_entry(
        texts::get(259), std::make_unique<widget_caller_button<>>([this]() { goto_torpedosettings(); }));
    screen_selector_menu->add_entry(
        texts::get(260), std::make_unique<widget_caller_button<>>([this]() { screen_selector_visible = false; }));
    screen_selector->add_child_near_last_child(std::move(screen_selector_menu));
    screen_selector->clip_to_children_area();
    screen_selector->set_pos(vector2i(0, 0));

    // note! we could add a second menu with the most common actions here...

    // fixme: later set "current_display" to other default value, like captain's
    // cabin.

    // Important! call enter() here for current display.
    displays[current_display]->enter(gm.is_day_mode());

    add_loading_screen("submarine interface initialized");
}

submarine_interface::~submarine_interface() = default;

void submarine_interface::fire_tube(submarine* player, int nr)
{
    // fixme here was a check that we don't target ourselves - but we can't
    // request the ID from a sea_object ptr. This must be avoided elsewhere.
    if (mygame->is_valid(player->get_target())) {
        auto& mytarget = mygame->get_object(player->get_target());
        bool ok        = player->launch_torpedo(nr, mytarget.get_pos(), *mygame);
        if (ok) {
            add_message(texts::get(49));
            ostringstream oss;
            oss << texts::get(49);
            if (mygame->is_valid(player->get_target())) {
                oss << " " << texts::get(6) << ": " << mytarget.get_description(2);
            }
            mygame->add_logbook_entry(oss.str());
            play_sound_effect(SFX_TUBE_LAUNCH, player->get_pos());
        } else {
            add_message(texts::get(138));
        }

#if 0 // old code
        submarine::stored_torpedo::st_status tube_status = submarine::stored_torpedo::st_empty;

        // fixme: ask TDC!
        // depends on direction of target and wether we have bow/stern tube and wether the
        // tube is not empty!
        if (true /*player->can_torpedo_be_launched(nr, target, tube_status)*/) {
            add_message(texts::get(49));
            ostringstream oss;
            oss << texts::get(49);
            if (target)
                oss << " " << texts::get(6) << ": " << target->get_description(2);
            mygame->add_logbook_entry(oss.str());
            player->launch_torpedo(nr, target);
            play_sound_effect(se_submarine_torpedo_launch, player->get_pos());
        } else {
            string failed_to_fire_msg;

            if (-1 != nr) {
                switch (tube_status)
                {
                    case submarine::stored_torpedo::st_empty:
                        failed_to_fire_msg = texts::get(134);
                        break;
                    case submarine::stored_torpedo::st_reloading:
                        failed_to_fire_msg = texts::get(135);
                        break;
                    case submarine::stored_torpedo::st_unloading:
                        failed_to_fire_msg = texts::get(136);
                        break;
                    default:
                        // could happen when tube is loaded
                        // but gyro angle is invalid for
                        // current target
                        return;
                }
            } else {
                if (true == player->get_torpedoes().empty())
                    failed_to_fire_msg = texts::get(137);
                else
                    failed_to_fire_msg = texts::get(138);
            }

            add_message(failed_to_fire_msg);
        }
#endif
    } else {
        add_message(texts::get(80));
    }
}

auto submarine_interface::handle_key_event(const key_data& k) -> bool
{
    if (k.down()) {
        auto* player = dynamic_cast<submarine*>(mygame->get_player());

        // SCREENS
        if (is_configured_key(key_command::SHOW_VALVES_SCREEN, k)) {
            goto_valves();
        } else if (is_configured_key(key_command::SHOW_GAUGES_SCREEN, k)) {
            goto_gauges();
        } else if (is_configured_key(key_command::SHOW_PERISCOPE_SCREEN, k)) {
            goto_periscope();
        } else if (is_configured_key(key_command::SHOW_UZO_SCREEN, k)) {
            goto_UZO();
        } else if (is_configured_key(key_command::SHOW_BRIDGE_SCREEN, k)) {
            goto_bridge();
        } else if (is_configured_key(key_command::SHOW_MAP_SCREEN, k)) {
            goto_map();
        } else if (is_configured_key(key_command::SHOW_TORPEDO_SCREEN, k)) {
            goto_torpedomanagement();
        } else if (is_configured_key(key_command::SHOW_DAMAGE_CONTROL_SCREEN, k)) {
            goto_damagecontrol();
        } else if (is_configured_key(key_command::SHOW_LOGBOOK_SCREEN, k)) {
            goto_captainscabin();
        } else if (is_configured_key(key_command::SHOW_SUCCESS_RECORDS_SCREEN, k)) {
            goto_sonar();
        } else if (is_configured_key(key_command::SHOW_FREEVIEW_SCREEN, k)) {
            goto_freeview();
        } else if (is_configured_key(key_command::SHOW_TDC_SCREEN, k)) {
            goto_TDC();
        } else if (is_configured_key(key_command::SHOW_TDC2_SCREEN, k)) {
            goto_TDC2();
        } else if (is_configured_key(key_command::SHOW_TORPSETUP_SCREEN, k)) {
            goto_torpedosettings();
        } else if (is_configured_key(key_command::SHOW_TORPEDO_CAMERA, k)) {
            // show next torpedo in torpedo camera view
            torpedo_cam_track_nr++;

            // MOVEMENT
        } else if (is_configured_key(key_command::RUDDER_LEFT, k)) {
            player->set_rudder(ship::rudderleft);
            add_message(texts::get(33));
        } else if (is_configured_key(key_command::RUDDER_HARD_LEFT, k)) {
            player->set_rudder(ship::rudderfullleft);
            add_message(texts::get(35));
        } else if (is_configured_key(key_command::RUDDER_RIGHT, k)) {
            player->set_rudder(ship::rudderright);
            add_message(texts::get(34));
        } else if (is_configured_key(key_command::RUDDER_HARD_RIGHT, k)) {
            player->set_rudder(ship::rudderfullright);
            add_message(texts::get(36));
        } else if (is_configured_key(key_command::RUDDER_UP, k)) {
            player->set_planes_to(-0.5, *mygame);
            add_message(texts::get(37));
        } else if (is_configured_key(key_command::RUDDER_HARD_UP, k)) {
            player->set_planes_to(-1.0, *mygame);
            add_message(texts::get(37));
        } else if (is_configured_key(key_command::RUDDER_DOWN, k)) {
            add_message(texts::get(38));
            player->set_planes_to(0.5, *mygame);
        } else if (is_configured_key(key_command::RUDDER_HARD_DOWN, k)) {
            add_message(texts::get(38));
            player->set_planes_to(1.0, *mygame);
        } else if (is_configured_key(key_command::CENTER_RUDDERS, k)) {
            player->set_rudder(ship::ruddermidships);
            player->set_planes_to(0, *mygame);
            add_message(texts::get(42));

            // THROTTLE
        } else if (is_configured_key(key_command::THROTTLE_LISTEN, k)) {
            player->set_throttle(ship::aheadlisten);
            add_message(texts::get(139));
        } else if (is_configured_key(key_command::THROTTLE_SLOW, k)) {
            player->set_throttle(ship::aheadslow);
            add_message(texts::get(43));
        } else if (is_configured_key(key_command::THROTTLE_HALF, k)) {
            player->set_throttle(ship::aheadhalf);
            add_message(texts::get(44));
        } else if (is_configured_key(key_command::THROTTLE_FULL, k)) {
            player->set_throttle(ship::aheadfull);
            add_message(texts::get(45));
        } else if (is_configured_key(key_command::THROTTLE_FLANK, k)) {
            player->set_throttle(ship::aheadflank);
            add_message(texts::get(46));
        } else if (is_configured_key(key_command::THROTTLE_STOP, k)) {
            player->set_throttle(ship::stop);
            add_message(texts::get(47));
        } else if (is_configured_key(key_command::THROTTLE_REVERSE, k)) {
            player->set_throttle(ship::reverse);
            add_message(texts::get(48));
        } else if (is_configured_key(key_command::THROTTLE_REVERSEHALF, k)) {
            player->set_throttle(ship::reversehalf);
            add_message(texts::get(140));
        } else if (is_configured_key(key_command::THROTTLE_REVERSEFULL, k)) {
            player->set_throttle(ship::reversefull);
            add_message(texts::get(141));

            // TORPEDOES
        } else if (is_configured_key(key_command::FIRE_TORPEDO, k)) {
            fire_tube(player, -1);
        } else if (is_configured_key(key_command::FIRE_TUBE_1, k)) {
            fire_tube(player, 0);
        } else if (is_configured_key(key_command::FIRE_TUBE_2, k)) {
            fire_tube(player, 1);
        } else if (is_configured_key(key_command::FIRE_TUBE_3, k)) {
            fire_tube(player, 2);
        } else if (is_configured_key(key_command::FIRE_TUBE_4, k)) {
            fire_tube(player, 3);
        } else if (is_configured_key(key_command::FIRE_TUBE_5, k)) {
            fire_tube(player, 4);
        } else if (is_configured_key(key_command::FIRE_TUBE_6, k)) {
            fire_tube(player, 5);
        } else if (is_configured_key(key_command::SELECT_TARGET, k)) {
            auto tgt = mygame->contact_in_direction(player, get_absolute_bearing());
            // set initial tdc values, also do that when tube is switched
            player->set_target(tgt, *mygame);
            if (mygame->is_valid(tgt)) {
                add_message(texts::get(50));
                mygame->add_logbook_entry(texts::get(50));
            } else {
                add_message(texts::get(51));
            }

            // DEPTH, SNORKEL, SCOPE
        } else if (is_configured_key(key_command::SCOPE_UP_DOWN, k)) {
            if (player->is_scope_up()) {
                player->scope_down();
                add_message(texts::get(54));
            } else {
                player->scope_up();
                add_message(texts::get(55));
            }
        } else if (is_configured_key(key_command::CRASH_DIVE, k)) {
            add_message(texts::get(41));
            mygame->add_logbook_entry(texts::get(41));
            player->crash_dive(*mygame);
        } else if (is_configured_key(key_command::GO_TO_SNORKEL_DEPTH, k)) {
            if (player->has_snorkel()) {
                player->dive_to_depth(unsigned(player->get_snorkel_depth()), *mygame);
                add_message(texts::get(12));
                mygame->add_logbook_entry(texts::get(97));
            }
        } else if (is_configured_key(key_command::TOGGLE_SNORKEL, k)) {
            if (player->has_snorkel()) {
                if (player->is_snorkel_up()) {
                    player->snorkel_down();
                    // fixme: was an if, why? say "snorkel down only when it was
                    // down"
                    add_message(texts::get(96));
                    mygame->add_logbook_entry(texts::get(96));
                } else {
                    player->snorkel_up();
                    // fixme: was an if, why? say "snorkel up only when it was
                    // up"
                    add_message(texts::get(95));
                    mygame->add_logbook_entry(texts::get(95));
                }
            }
        } else if (is_configured_key(key_command::SET_HEADING_TO_VIEW, k)) {
            player->head_to_course(get_absolute_bearing());
        } else if (is_configured_key(key_command::IDENTIFY_TARGET, k)) {
            // calculate distance to target for identification detail
            if (mygame->is_valid(player->get_target())) {
                ostringstream oss;
                oss << texts::get(79) << mygame->get_object(player->get_target()).get_description(2); // fixme
                add_message(oss.str());
                mygame->add_logbook_entry(oss.str());
            } else {
                add_message(texts::get(80));
            }
        } else if (is_configured_key(key_command::GO_TO_PERISCOPE_DEPTH, k)) {
            add_message(texts::get(40));
            mygame->add_logbook_entry(texts::get(40));
            player->dive_to_depth(unsigned(player->get_periscope_depth()), *mygame);
        } else if (is_configured_key(key_command::GO_TO_SURFACE, k)) {
            player->dive_to_depth(0, *mygame);
            add_message(texts::get(39));
            mygame->add_logbook_entry(texts::get(39));

            // VIEWS
        } else if (is_configured_key(key_command::SET_VIEW_TO_HEADING, k)) {
            bearing = (bearing_is_relative) ? 0.0 : player->get_heading();
        } else if (is_configured_key(key_command::TURN_VIEW_LEFT, k)) {
            add_bearing(angle(-1));
        } else if (is_configured_key(key_command::TURN_VIEW_LEFT_FAST, k)) {
            add_bearing(angle(-10));
        } else if (is_configured_key(key_command::TURN_VIEW_RIGHT, k)) {
            add_bearing(angle(1));
        } else if (is_configured_key(key_command::TURN_VIEW_RIGHT_FAST, k)) {
            add_bearing(angle(10));

            // TIME SCALE
        } else if (is_configured_key(key_command::TIME_SCALE_UP, k)) {
            if (time_scale_up()) {
                add_message(texts::get(31));
            }
        } else if (is_configured_key(key_command::TIME_SCALE_DOWN, k)) {
            if (time_scale_down()) {
                add_message(texts::get(32));
            }

            // GUNS
        } else if (is_configured_key(key_command::FIRE_DECK_GUN, k)) {
            if (player->has_deck_gun()) {
                if (!player->is_submerged()) {
                    if (mygame->is_valid(player->get_target()) /*fixme && player->get_target() != player*/) {
                        int res =
                            player->fire_shell_at(mygame->get_object(player->get_target()).get_pos().xy(), *mygame);
                        if (ship::TARGET_OUT_OF_RANGE == res) {
                            add_message(texts::get(218));
                        } else if (ship::NO_AMMO_REMAINING == res) {
                            add_message(texts::get(219));
                        } else if (ship::RELOADING == res) {
                            add_message(texts::get(130));
                        } else if (ship::GUN_NOT_MANNED == res) {
                            add_message(texts::get(131));
                        } else if (ship::GUN_TARGET_IN_BLINDSPOT == res) {
                            add_message(texts::get(132));
                        } else if (ship::GUN_FIRED == res) {
                            add_message(texts::get(270));
                        }
                    } else {
                        add_message(texts::get(80));
                    }
                } else {
                    add_message(texts::get(27));
                }
            }
        } else if (is_configured_key(key_command::TOGGLE_MAN_DECK_GUN, k)) {
            if (player->has_deck_gun()) {
                if (!player->is_submerged()) {
                    if (key_mod_shift(k.mod)) {
                        if (player->is_gun_manned()) {
                            if (player->unman_guns()) {
                                add_message(texts::get(126));
                            }
                        } else {
                            if (player->man_guns()) {
                                add_message(texts::get(133));
                            }
                        }
                    }
                } else {
                    add_message(texts::get(27));
                }
            } else {
                add_message(texts::get(269));
            }
        } else if (is_configured_key(key_command::TAKE_SCREENSHOT, k)) {
            SYS().screenshot();
            log_info("screenshot taken.");

            // DEFAULT
        } else {
            // rest of the keys per switch (not user defineable)
            // quit, screenshot, pause etc.
            switch (k.keycode) {
                case key_code::ESCAPE:
                    request_abort();
                    break;
                    /*			case key_command::PRINT:
                                    SYS().screenshot();
                                    log_info("screenshot taken.");
                                    break;*/
                case key_code::PAUSE:
                    toggle_pause();
                    break;
#if 1 // fixme test hack
                case key_code::r:
                    mygame->spawn(
                        std::make_unique<fireworks_particle>(mygame->get_player()->get_pos() + vector3(0, 0, 5)));
                    break;
#endif
                default:
                    // let display handle the key
                    user_interface::handle_key_event(k);
            }
        }
        // we handled the key
        return true;
    }
    return user_interface::handle_key_event(k);
}

auto submarine_interface::handle_mouse_button_event(const mouse_click_data& m) -> bool
{
    // switch screen selector on if it is not visible
    if (m.down() && m.right()) {
        if (!main_menu_visible) {
            main_menu_visible = true;
            return true;
        }
    }
    return user_interface::handle_mouse_button_event(m);
}

void submarine_interface::set_time(double tm)
{
    user_interface::set_time(tm);

    // change current screen if necessary (forcibly)
    auto* player = dynamic_cast<submarine*>(mygame->get_player());
    if ((current_display == display_mode_uzo || current_display == display_mode_bridge) && player->is_submerged()) {
        set_current_display(display_mode_periscope);
    }
    if (current_display == display_mode_periscope && player->get_depth() > player->get_periscope_depth()) {
        set_current_display(display_mode_map);
    }
}

void submarine_interface::goto_gauges()
{
    set_current_display(display_mode_gauges);
}

void submarine_interface::goto_valves()
{
    log_debug("blubb");
    set_current_display(display_mode_valves);
}

void submarine_interface::goto_periscope()
{
    auto* player = dynamic_cast<submarine*>(mygame->get_player());
    if (player->get_depth() > player->get_periscope_depth()) {
        add_message(texts::get(28));
        // will later be replaced when scope can be raised in smaller steps...
        // no. height of scope and en/disabling are not the same.
    } else {
        set_current_display(display_mode_periscope);
    }
}

void submarine_interface::goto_UZO()
{
    auto* player = dynamic_cast<submarine*>(mygame->get_player());
    if (player->is_submerged()) {
        add_message(texts::get(27));
    } else {
        set_current_display(display_mode_uzo);
    }
}

void submarine_interface::goto_bridge()
{
    auto* player = dynamic_cast<submarine*>(mygame->get_player());
    if (player->is_submerged()) {
        add_message(texts::get(27));
    } else {
        set_current_display(display_mode_bridge);
    }
}

void submarine_interface::goto_map()
{
    set_current_display(display_mode_map);
}

void submarine_interface::goto_torpedomanagement()
{
    set_current_display(display_mode_torpedoroom);
}

void submarine_interface::goto_damagecontrol()
{
    set_current_display(display_mode_damagestatus);
}

void submarine_interface::goto_captainscabin()
{
    set_current_display(display_mode_captainscabin);
}

void submarine_interface::goto_logbook()
{
    set_current_display(display_mode_logbook);
}

void submarine_interface::goto_soldbuch()
{
    set_current_display(display_mode_soldbuch);
}

void submarine_interface::goto_successes()
{
    set_current_display(display_mode_successes);
}

void submarine_interface::goto_sonar()
{
    set_current_display(display_mode_sonar);
}

void submarine_interface::goto_freeview()
{
    set_current_display(display_mode_freeview);
}

void submarine_interface::goto_TDC()
{
    set_current_display(display_mode_tdc);
}

void submarine_interface::goto_TDC2()
{
    set_current_display(display_mode_tdc2);
}

void submarine_interface::goto_torpedosettings()
{
    set_current_display(display_mode_torpsetup);
}

void submarine_interface::goto_recogmanual()
{
    set_current_display(display_mode_recogmanual);
}
/*
bool submarine_interface::object_visible(sea_object* so,
    const vector2& dl, const vector2& dr) const //fixme buggy
{
    vector2 p = so->get_pos().xy();
    double rad = so->get_length()/2, s, t;	// most objects are longer than
wide...fixme s = p.x*dl.x + p.y*dl.y; t = p.y*dl.x - p.x*dl.y; if (s < -rad || t
> rad) return false; s = p.x*dr.x + p.y*dr.y; t = p.y*dr.x - p.x*dr.y; if (s <
-rad || t < -rad) return false; return true;
}
*/

void submarine_interface::display() const
{
    auto* player = dynamic_cast<submarine*>(mygame->get_player());

    // machine sound
    unsigned thr = 0;
    switch (player->get_throttle()) {
        case ship::reversefull:
            thr = 100;
            break;
        case ship::reversehalf:
            thr = 66;
            break;
        case ship::reverse:
            thr = 33;
            break;
        case ship::aheadlisten:
            thr = 20;
            break;
        case ship::aheadsonar:
            thr = 20;
            break;
        case ship::aheadslow:
            thr = 40;
            break;
        case ship::aheadhalf:
            thr = 60;
            break;
        case ship::aheadfull:
            thr = 80;
            break;
        case ship::aheadflank:
            thr = 100;
            break;
        case ship::stop:
            thr = 0;
            break;
        default:
            thr = 0;
            break; // throttled speeds not supported here...
    }
    // music::instance().play_sfx_machine(SFX_MACHINE_SUB_DIESEL, thr);

    user_interface::display();

    // panel is drawn in each display function, so the above code is all...

    if (torpedo_cam_track_nr > 0) {
        const torpedo* tt = mygame->get_torpedo_for_camera_track(torpedo_cam_track_nr - 1);
        torpedo_cam_view->set_tracker(tt);
        if (tt != nullptr) {
            /*
            glMatrixMode(GL_PROJECTION);
            glPushMatrix();
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            */
            torpedo_cam_view->display();
            /*
            glMatrixMode(GL_PROJECTION);
            glPopMatrix();
            glMatrixMode(GL_MODELVIEW);
            glPopMatrix();
            */
        } else {
            torpedo_cam_track_nr = 0;
        }
    } else {
        torpedo_cam_view->set_tracker(nullptr);
    }
}
