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

// user display: torpedo tracking camera
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#pragma once

#include "freeview_display.hpp"

class torpedo_camera_display : public freeview_display
{
    void pre_display() const override;
    projection_data get_projection_data(class game& gm) const override;
    void post_display() const override;
    vector3 get_viewpos(class game& gm) const override;

    mutable const class torpedo* trackobj;

  public:
    torpedo_camera_display(class user_interface& ui_);

    unsigned get_popup_allow_mask() const override;

    void enter(bool is_day) override;
    void leave() override;

    void set_tracker(const class torpedo* t) { trackobj = t; }
};
