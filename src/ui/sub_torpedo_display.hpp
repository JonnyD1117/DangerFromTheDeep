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

#pragma once

#include "log.hpp"
#include "object_store.hpp"
#include "submarine.hpp"
#include "user_display.hpp"
#include "vector2.hpp"

/// A display to manage torpedo storage and transfer for submarines
class sub_torpedo_display : public user_display
{
  public:
    sub_torpedo_display(class user_interface& ui_);
    void display() const override;
    bool handle_mouse_button_event(const mouse_click_data&) override;
    bool handle_mouse_motion_event(const mouse_motion_data&) override;
    bool handle_mouse_wheel_event(const mouse_wheel_data&) override;

  protected:
    // source tube nr for manual torpedo transfer, used for drag & drop drawing
    unsigned torptranssrc;

    class desc_text
    {
        std::vector<std::string> txtlines;
        desc_text() = delete;

      public:
        desc_text(const std::string& filename);
        // give startline and number of lines to fetch (nr=0: all).
        [[nodiscard]] std::string str(unsigned startline = 0, unsigned nrlines = 0) const;
        [[nodiscard]] unsigned nr_of_lines() const { return txtlines.size(); }
    };

    mutable object_store<desc_text> desc_texts;

    void draw_torpedo(class game& gm, bool usebow, const vector2i& pos, const submarine::stored_torpedo& st) const;

    vector2i mouse_position;
    mouse_button_state mouse_buttons;
    bool left_mouse_button_pressed{false};

    mutable unsigned torp_desc_line;

    std::shared_ptr<texture> notepadsheet;

    static std::vector<vector2i> get_tubecoords(class submarine* sub);

    unsigned get_tube_below_mouse(const std::vector<vector2i>& tubecoords) const;
};
