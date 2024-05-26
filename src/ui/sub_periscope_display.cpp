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

// user display: submarine's periscope
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#include "sub_periscope_display.hpp"

#include "cfg.hpp"
#include "game.hpp"
#include "global_data.hpp"
#include "keys.hpp"
#include "primitives.hpp"
#include "submarine.hpp"
#include "submarine_interface.hpp"
#include "system_interface.hpp"

namespace
{
enum element_type
{
    et_direction = 0,
    et_hours     = 1,
    et_minutes   = 2
};

}

void sub_periscope_display::pre_display() const
{
    glClear(GL_DEPTH_BUFFER_BIT);
}

auto sub_periscope_display::get_projection_data(class game& gm) const -> freeview_display::projection_data
{
    projection_data pd;
    pd.x = 453 * SYS().get_res_x() / 1024;
    pd.y = (768 - 424 - 193) * SYS().get_res_x() / 1024;
    pd.w = 424 * SYS().get_res_x() / 1024;
    pd.h = 424 * SYS().get_res_x() / 1024;
    // with normal fov of 70 degrees, this is 1.5 / 6.0 magnification
    pd.fov_x      = zoomed ? 13.31 : 50.05; // fixme: historic values?
    pd.near_z     = 1.0;
    pd.far_z      = gm.get_max_view_distance();
    pd.fullscreen = false;
    return pd;
}

auto sub_periscope_display::get_viewpos(class game& gm) const -> vector3
{
    const auto* sub = dynamic_cast<const submarine*>(gm.get_player());
    return sub->get_pos() + add_pos + vector3(0, 0, 6) * sub->get_scope_raise_level();
}

void sub_periscope_display::set_modelview_matrix(game& gm, const vector3& /*viewpos*/) const
{
    glLoadIdentity();

    // set up rotation (player's view direction) - we have no elevation for the
    // periscope. so set standard elevation of 90°
    glRotated(-90.0F, 1, 0, 0);

    // if we're aboard the player's vessel move the world instead of the ship
    if (aboard) {
        // This should be a negative angle, but nautical view dir is clockwise,
        // OpenGL uses ccw values, so this is a double negation
        glRotated(ui.get_relative_bearing().value(), 0, 0, 1);
        gm.get_player()->get_orientation().conj().rotmat4().multiply_gl();
    } else {
        // This should be a negative angle, but nautical view dir is clockwise,
        // OpenGL uses ccw values, so this is a double negation
        glRotated(ui.get_absolute_bearing().value(), 0, 0, 1);
    }

    // set up modelview matrix as if player is at position (0, 0, 0), so do NOT
    // set a translational part. This is done to avoid rounding errors caused by
    // large x/y values (modelview matrix seems to store floats, but coordinates
    // are in real meters, so float is not precise enough).
}

void sub_periscope_display::post_display() const
{
    auto& gm = ui.get_game();
    if (use_hqsfx) {
        // here we render scope view as blurred, watery image
        viewtex->set_gl_texture();
        projection_data pd = get_projection_data(gm);
        // copy visible part of viewport to texture
        // fixme: w/h must be powers of 2. here we have 424. could work for
        // newer cards though (non-power-2-tex)
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, pd.x, pd.y, pd.w, pd.h, 0);
        // now render texture as 2d image combined with blur texture.
        glPushMatrix();
        glLoadIdentity();
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        // bind shader...
        glsl_blurview->use();
        glsl_blurview->set_gl_texture(*viewtex, loc_tex_view, 0);
        glsl_blurview->set_gl_texture(*blurtex, loc_tex_blur, 1);
        double blur_y_off = myfrac(gm.get_time() / 10.0);
        glsl_blurview->set_uniform(loc_blur_texc_offset, vector3(blur_y_off, 0, 0));
        primitives::textured_quad(vector2f(-1, -1), vector2f(2, 2), *viewtex).render_plain();
        // unbind shader
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }

    if (gm.is_valid(gm.get_player()->get_target())) {
        projection_data pd = get_projection_data(gm);
        ui.show_target(pd.x, pd.y, pd.w, pd.h, get_viewpos(gm));
    }

    element_for_id(et_direction).set_value(ui.get_relative_bearing().value());
    const auto tm = gm.get_time();
    element_for_id(et_hours).set_value(helper::mod(tm, 86400.0 / 2) / 3600.0);
    element_for_id(et_minutes).set_value(helper::mod(tm, 3600.0) / 60.0);
    draw_elements();
}

sub_periscope_display::sub_periscope_display(user_interface& ui_)
    : freeview_display(ui_, "sub_periscope")
{
    add_pos               = vector3(0, 0, 8); // fixme, depends on sub
    aboard                = true;
    withunderwaterweapons = false; // they can be seen when scope is partly
                                   // below water surface, fixme
    drawbridge = false;

    use_hqsfx     = cfg::instance().getb("use_hqsfx");
    viewtex       = std::make_unique<texture>(512, 512, GL_RGB, texture::LINEAR, texture::CLAMP);
    glsl_blurview = std::make_unique<glsl_shader_setup>(
        get_shader_dir() + "blurview.vshader", get_shader_dir() + "blurview.fshader");
    glsl_blurview->use();
    loc_blur_texc_offset = glsl_blurview->get_uniform_location("blur_texc_offset");
    loc_tex_view         = glsl_blurview->get_uniform_location("tex_view");
    loc_tex_blur         = glsl_blurview->get_uniform_location("tex_blur");
    /* Note 2007/05/08:
       we can have a better Blur texture if we generate it at runtime.
       What makes up the blur texture?
       Water running down over the lens of the periscope.
       So use one texture, no UV scrolling, just (re)generate it every frame.
       Make an empty (black) texture, draw a set of drops on it (GL_POINTS
       of a certain size with a texture that is like a blurmap for a
       water drop).
       Simulate each drop (moving down by gravity, adding more drops when
       periscope collides with water surface).
       Render the drops to the texture (with textured GL_POINTS).
       Use the texture to render the final effect.
       This can be done quick & cheap, but the current scrolling texture
       is also ok.
    */
    blurtex = std::make_unique<texture>(get_texture_dir() + "blurtest.png", texture::LINEAR, texture::REPEAT);
}

auto sub_periscope_display::handle_key_event(const key_data& k) -> bool
{
    if (k.down()) {
        if (is_configured_key(key_command::TOGGLE_ZOOM_OF_VIEW, k)) {
            zoomed = !zoomed;
            return true;
        }
        if (k.is_keypad_number()) {
            // filter away keys NP_1...NP_9 to avoid moving viewer like in
            // freeview mode
            return true;
        }
    }
    return freeview_display::handle_key_event(k);
}

auto sub_periscope_display::handle_mouse_motion_event(const mouse_motion_data& m) -> bool
{
    if (m.left() && m.relative_motion_2d.y != 0) {
        // remove y motion, replace by scope raise/lower code
        auto& gm = ui.get_game();
        auto* s  = dynamic_cast<submarine*>(gm.get_player());
        s->scope_to_level(s->get_scope_raise_level() - m.relative_motion.y / 100.0F);
        auto m2{m};
        m2.relative_motion.y    = 0;
        m2.relative_motion_2d.y = 0;
        return freeview_display::handle_mouse_motion_event(m2);
    }
    return freeview_display::handle_mouse_motion_event(m);
}

auto sub_periscope_display::handle_mouse_wheel_event(const mouse_wheel_data& m) -> bool
{
    if (m.up()) {
        zoomed = true;
        return true;
    }
    if (m.down()) {
        zoomed = false;
        return true;
    }
    return freeview_display::handle_mouse_wheel_event(m);
}

void sub_periscope_display::display() const
{
    // with new compassbar lower 32 pixel of 3d view are not visible... maybe
    // shrink 3d view? fixme
    // fixme: add specials for underwater rendering here... or in freeview
    // class!
    freeview_display::display();
}

auto sub_periscope_display::get_popup_allow_mask() const -> unsigned
{
    return (1 << submarine_interface::popup_mode_control) | (1 << submarine_interface::popup_mode_tdc)
           | (1 << submarine_interface::popup_mode_ecard) | (1 << submarine_interface::popup_mode_recogmanual);
}
