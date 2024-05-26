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

// Object to display the damage status of a submarine.
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#pragma once

#include "submarine.hpp"
#include "user_display.hpp"
#include "vector3.hpp"

/// A display to show and control sub's damage
class sub_damage_display : public user_display
{
  public:
    sub_damage_display(class user_interface& ui_);
    void display() const override;
    bool handle_mouse_motion_event(const mouse_motion_data&) override;

  protected:
    vector2i mouse_position; // last mouse position, needed for popup display
    std::shared_ptr<texture> notepadsheet;
};
