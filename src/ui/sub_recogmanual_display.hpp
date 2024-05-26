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

// Object to display the ship recognition manual
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#pragma once

#include "datadirs.hpp"
#include "user_display.hpp"
#include "widget.hpp"

class sub_recogmanual_display : public user_display
{
  protected:
    class widget_button_next : public widget_button
    {
      protected:
        int direction;
        int& page;

      public:
        widget_button_next(
            int x,
            int y,
            int w,
            int h,
            int dir,
            int& att_page,
            const std::string& text_,
            const std::string& bg_image_);
        void draw() const override;
        void on_release() override;
    };

    int page;
    std::vector<std::unique_ptr<image>> silhouettes;
    std::vector<std::string> classes;
    std::vector<std::string> lengths;
    std::vector<std::string> displacements;
    std::vector<std::string> weapons;
    std::vector<std::string> countries;
    widget_button_next btn_left;
    widget_button_next btn_right;

  public:
    sub_recogmanual_display(class user_interface& ui_);

    void display() const override;
    bool handle_mouse_button_event(const mouse_click_data&) override;
    bool handle_mouse_motion_event(const mouse_motion_data&) override;
    bool handle_mouse_wheel_event(const mouse_wheel_data&) override;

    void enter(bool is_day) override;
    void leave() override;
};
