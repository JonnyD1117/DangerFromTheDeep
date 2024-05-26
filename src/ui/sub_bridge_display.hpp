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

// user display: submarine's bridge
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#pragma once

#include "freeview_display.hpp"

class sub_bridge_display : public freeview_display
{
  public:
    sub_bridge_display(class user_interface& ui_);

    // overload for glasses key handling ('y')
    bool handle_key_event(const key_data&) override;
    bool handle_mouse_wheel_event(const mouse_wheel_data&) override;

  protected:
    void pre_display() const override;
    projection_data get_projection_data(class game& gm) const override;
    void post_display() const override;
};
