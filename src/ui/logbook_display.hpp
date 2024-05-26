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

// Object to create and display logbook entries.
// subsim (C)+(W) Markus Petermann and Thorsten Jordan. SEE LICENSE

#pragma once

#include "user_display.hpp"

class logbook_display : public user_display
{
  public:
    logbook_display(class user_interface& ui_);
    void display() const override;
    bool handle_key_event(const key_data&) override;
    bool handle_mouse_button_event(const mouse_click_data&) override;

  protected:
    const vector2i page_left_offset;
    const vector2i page_right_offset;
    const vector2i page_size;
    unsigned current_page{0};
    mutable unsigned nr_of_pages{1};

    void next_page();
    void previous_page();
};
