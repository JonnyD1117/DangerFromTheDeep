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

// user display: general map view
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#pragma once

#include "bivector.hpp"
#include "color.hpp"
#include "sea_object_id.hpp"
#include "user_display.hpp"
#include "vector2.hpp"
#include "widget.hpp"

#include <unordered_set>

class game;
class game_editor;
class sea_object;
class submarine;

class map_display : public user_display
{
  protected:
    // map
    float mapzoom;           // factor pixel/meter
    vector2 mapoffset;       // additional offset used for display, relative to player
                             // (meters)
    vector2i mouse_position; // last mouse position
    int mapmode;

    void draw_vessel_symbol(const vector2& offset, const sea_object* so, color c) const;
    void draw_trail(const sea_object* so, const vector2& offset) const;
    void draw_pings(game& gm, const vector2& offset) const;
    void draw_sound_contact(game& gm, const sea_object* player, double max_view_dist, const vector2& offset) const;
    void draw_sound_contact(game& gm, const submarine* player, const vector2& offset) const;
    void draw_visual_contacts(game& gm, const sea_object* player, const vector2& offset) const;
    void draw_radar_contacts(game& gm, const sea_object* player, const vector2& offset) const;
    void draw_square_mark(game& gm, const vector2& mark_pos, const vector2& offset, const color& c) const;
    void draw_square_mark_special(game& gm, const vector2& mark_pos, const vector2& offset, const color& c) const;

    // only used in editor mode
    // fixme: this should be part of the user interface, so that the editor
    // panel is available also in other screens...
    std::unique_ptr<widget> edit_panel;
    std::unique_ptr<widget> edit_panel_add;
    std::unique_ptr<widget> edit_panel_chgmot;
    std::unique_ptr<widget> edit_panel_time;
    std::unique_ptr<widget> edit_panel_descr;
    std::unique_ptr<widget> edit_panel_help;
    std::unique_ptr<widget> edit_panel_convoy;
    widget::ptr edit_btn_del;
    widget::ptr edit_btn_chgmot;
    widget::ptr edit_btn_copy;
    widget::ptr edit_btn_cvmenu;
    widget::ptr edit_panel_fg;
    widget_list::ptr edit_shiplist;
    widget_slider::ptr edit_heading;
    widget_slider::ptr edit_speed;
    widget_slider::ptr edit_throttle;
    widget_slider::ptr edit_timeyear;
    widget_slider::ptr edit_timemonth;
    widget_slider::ptr edit_timeday;
    widget_slider::ptr edit_timehour;
    widget_slider::ptr edit_timeminute;
    widget_slider::ptr edit_timesecond;
    widget_edit::ptr edit_cvname;
    widget_slider::ptr edit_cvspeed;
    widget_list::ptr edit_cvlist;
    vector2i mouse_position_down{-1, -1}; // position of mouse when button was pressed
    std::unordered_set<sea_object_id> selection;
    key_mod state_of_key_modifiers;
    std::shared_ptr<texture> notepadsheet;

    // editor methods
    void edit_add_obj(game_editor& gm);
    void edit_del_obj(game_editor& gm);
    void edit_change_motion(game_editor& gm);
    void edit_copy_obj(game_editor& gm);
    void edit_convoy_menu(game_editor& gm);
    void edit_time(game_editor& gm);
    void edit_description(game_editor& gm);
    void edit_help(game_editor& gm);

    // check if we have a selection and enables/disables buttons
    void check_edit_sel();

  private:
    map_display() = delete;

  public:
    map_display(class user_interface& ui_);

    void display() const override;
    bool handle_key_event(const key_data&) override;
    bool handle_mouse_button_event(const mouse_click_data&) override;
    bool handle_mouse_motion_event(const mouse_motion_data&) override;
    bool handle_mouse_wheel_event(const mouse_wheel_data&) override;
};
