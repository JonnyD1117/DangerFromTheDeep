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

// user display: free 3d view
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#pragma once

#include "angle.hpp"
#include "user_display.hpp"
#include "vector3.hpp"
class sea_object;

///\brief User display implementation for free 3D view of the game world.
class freeview_display : public user_display
{
  public:
    struct projection_data
    {
        unsigned x, y, w, h; // viewport, holds also aspect info
        double fov_x;        // angle of field of view (horizontal) in degrees
        double near_z, far_z;
        bool fullscreen;
    };

  protected:
    vector3 add_pos; // additional offset to viewpos

    bool aboard;                // is player aboard?
    bool withunderwaterweapons; // draw underwater weapons?
    bool drawbridge;            // draw bridge if aboard?

    // used only in 3d view. a bit hackish to place it here, but better than in
    // global data. later store in class submarine. use cache to store, this is
    // only a reference.
    class model* conning_tower{};

    std::shared_ptr<class texture> underwater_background;

    freeview_display();

    // display() calls these functions
    virtual void pre_display() const;
    // fixme: reflections need special viewport... depends on detail settings.
    // mabye retrieve from ui
    virtual projection_data get_projection_data(class game& gm) const;
    virtual void set_modelview_matrix(class game& gm, const vector3& viewpos) const;
    virtual void post_display() const;

    // draw all sea_objects
    virtual void draw_objects(
        class game& gm,
        const vector3& viewpos,
        const std::vector<const sea_object*>& objects,
        const colorf& light_color,
        const bool under_water,
        bool mirrorclip) const;

    // draw the whole view
    virtual void draw_view(class game& gm, const vector3& viewpos) const;

    // compute view position for this display (can be overloaded!)
    virtual vector3 get_viewpos(class game& gm) const;

  public:
    freeview_display(class user_interface& ui_, const char* display_name = nullptr /* when no elements are used */);
    ~freeview_display() override;

    void display() const override;
    bool handle_key_event(const key_data&) override;
    bool handle_mouse_motion_event(const mouse_motion_data&) override;
    bool handle_mouse_wheel_event(const mouse_wheel_data&) override;
};
