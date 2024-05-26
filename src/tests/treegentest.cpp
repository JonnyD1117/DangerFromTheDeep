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

// main program
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "OglExt.h"
#include "datadirs.hpp"
#include "faulthandler.hpp"
#include "font.hpp"
#include "fpsmeasure.hpp"
#include "frustum.hpp"
#include "image.hpp"
#include "log.hpp"
#include "model.hpp"
#include "mymain.cpp"
#include "primitives.hpp"
#include "shader.hpp"
#include "system_interface.hpp"
#include "texture.hpp"
#include "tree_generator.hpp"
#include "vector3.hpp"

#include <ctime>
#include <glu.h>
#include <iostream>
#include <sstream>
#include <utility>

int res_x, res_y;

void run();

texture* terraintex;

int mymain(std::vector<string>& args)
{
    // report critical errors (on Unix/Posix systems)
    install_segfault_handler();

    // randomize
    srand(time(nullptr));

    // command line argument parsing
    res_x           = 1024;
    bool fullscreen = true;

    // parse commandline
    for (auto it = args.begin(); it != args.end(); ++it) {
        if (*it == "--help") {
            cout << "*** Danger from the Deep ***\nusage:\n--help\t\tshow this\n"
                 << "--res n\t\tuse resolution n horizontal,\n\t\tn is "
                    "512,640,800,1024 (recommended) or 1280\n"
                 << "--nofullscreen\tdon't use fullscreen\n"
                 << "--debug\t\tdebug mode: no fullscreen, resolution 800\n"
                 << "--editor\trun mission editor directly\n"
                 << "--mission fn\trun mission from file fn (just the filename "
                    "in the mission directory)\n"
                 << "--nosound\tdon't use sound\n";
            return 0;
        } else if (*it == "--nofullscreen") {
            fullscreen = false;
        } else if (*it == "--debug") {
            fullscreen = false;
            res_x      = 800;
        } else if (*it == "--res") {
            auto it2 = it;
            ++it2;
            if (it2 != args.end()) {
                int r = atoi(it2->c_str());
                if (r == 512 || r == 640 || r == 800 || r == 1024 || r == 1280)
                    res_x = r;
                ++it;
            }
        }
    }

    // fixme: also allow 1280x1024, set up gl viewport for 4:3 display
    // with black borders at top/bottom (height 2*32pixels)
    res_y = res_x * 3 / 4;
    // weather conditions and earth curvature allow 30km sight at maximum.
    system_interface::parameters params;
    params.resolution     = {res_x, res_y};
    params.resolution2d   = {1024, 768};
    params.window_caption = "treegentest";
    params.fullscreen     = fullscreen;
    params.vertical_sync  = true;
    params.near_z         = 1.0;
    params.far_z          = 30000.0 + 500.0;
    system_interface::create_instance(params);

    log_info("Danger from the Deep");

    GLfloat lambient[4]  = {0.3, 0.3, 0.3, 1};
    GLfloat ldiffuse[4]  = {1, 1, 1, 1};
    GLfloat lposition[4] = {0, 0, 1, 0};
    glLightfv(GL_LIGHT0, GL_AMBIENT, lambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, ldiffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, lposition);
    glEnable(GL_LIGHT0);

    run();

    system_interface::destroy_instance();

    return 0;
}

void run()
{
    terraintex = new texture(get_texture_dir() + "terrain.jpg", texture::LINEAR_MIPMAP_LINEAR);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    SYS().gl_perspective_fovx(70, 4.0 / 3.0, 0.1, 1000);
    glMatrixMode(GL_MODELVIEW);

    glDisable(GL_LIGHTING);

    vector3 viewangles(0, 0, 0);
    vector3 pos(1.5, 1.5, 0.3);

    double tm0     = SYS().millisec();
    int mv_forward = 0, mv_upward = 0, mv_sideward = 0;

    fpsmeasure fpsm(1.0f);

    tree_generator tgn;
    std::unique_ptr<model> treemdl = tgn.generate();

    vector3f wind_movement;

    bool doquit = false;
    auto ic     = std::make_shared<input_event_handler_custom>();
    ic->set_handler([&](const input_event_handler::key_data& k) {
        if (k.down()) {
            switch (k.keycode) {
                case key_code::ESCAPE:
                    doquit = true;
                    break;
                case key_code::KP_4:
                    mv_sideward = -1;
                    break;
                case key_code::KP_6:
                    mv_sideward = 1;
                    break;
                case key_code::KP_8:
                    mv_upward = 1;
                    break;
                case key_code::KP_2:
                    mv_upward = -1;
                    break;
                case key_code::KP_1:
                    mv_forward = 1;
                    break;
                case key_code::KP_3:
                    mv_forward = -1;
                    break;
                default:
                    return false;
                    break;
            }
            return true;
        } else if (k.up()) {
            switch (k.keycode) {
                case key_code::KP_4:
                    mv_sideward = 0;
                    break;
                case key_code::KP_6:
                    mv_sideward = 0;
                    break;
                case key_code::KP_8:
                    mv_upward = 0;
                    break;
                case key_code::KP_2:
                    mv_upward = 0;
                    break;
                case key_code::KP_1:
                    mv_forward = 0;
                    break;
                case key_code::KP_3:
                    mv_forward = 0;
                    break;
                default:
                    return false;
                    break;
            }
            return true;
        }
        return false;
    });
    ic->set_handler([&](const input_event_handler::mouse_motion_data& m) {
        if (m.left()) {
            viewangles.z -= m.relative_motion.x;
            viewangles.x -= m.relative_motion.y;
            return true;
        } else if (m.right()) {
            viewangles.y += m.relative_motion.x;
            return true;
        }
        return false;
    });
    SYS().add_input_event_handler(ic);

    while (!doquit) {
        double tm1     = SYS().millisec();
        double delta_t = tm1 - tm0;
        tm0            = tm1;

        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

        // compute mvp etc. for user
        glLoadIdentity();
        // make camera look to pos. y-axis.
        glRotatef(-90, 1, 0, 0);

        glRotatef(-viewangles.x, 1, 0, 0);
        glRotatef(-viewangles.y, 0, 1, 0);
        glRotatef(-viewangles.z, 0, 0, 1);
        matrix4 mvr = matrix4::get_gl(GL_MODELVIEW_MATRIX);
        glTranslated(-pos.x, -pos.y, -pos.z);
        matrix4 mv     = matrix4::get_gl(GL_MODELVIEW_MATRIX);
        matrix4 prj    = matrix4::get_gl(GL_PROJECTION_MATRIX);
        matrix4 mvp    = prj * mv;
        matrix4 invmvr = mvr.inverse();
        matrix4 invmvp = mvp.inverse();
        vector3 wbln   = invmvp * vector3(-1, -1, -1);
        vector3 wbrn   = invmvp * vector3(+1, -1, -1);
        vector3 wtln   = invmvp * vector3(-1, +1, -1);
        vector3 wtrn   = invmvp * vector3(+1, +1, -1);
        polygon viewwindow(wbln, wbrn, wtrn, wtln);
        frustum viewfrustum(viewwindow, pos, 0.1 /* fixme: read from matrix! */);

        // set light
        vector3 ld(
            cos((SYS().millisec() % 10000) * 2 * 3.14159 / 10000),
            sin((SYS().millisec() % 10000) * 2 * 3.14159 / 10000),
            1.0);
        ld.normalize();
        GLfloat lposition[4] = {static_cast<GLfloat>(ld.x), static_cast<GLfloat>(ld.y), static_cast<GLfloat>(ld.z), 0};
        glLightfv(GL_LIGHT0, GL_POSITION, lposition);
        wind_movement.z = cos(tm1 / 2000.0f * M_PI) * 0.01;

        // render ground plane
        float tc = 600, vc = 3000;
        primitives::textured_quad(
            vector2f(-vc, -vc),
            vector2f(vc, vc),
            *terraintex,
            vector2f(-tc, -tc),
            vector2f(tc, tc),
            colorf(0.5, 0.8, 0.5, 1))
            .render();

        treemdl->display();
        auto& leafmat          = dynamic_cast<model::material_glsl&>(treemdl->get_material(1));
        glsl_shader_setup& gss = leafmat.get_shadersetup();
        gss.use();
        gss.set_uniform(gss.get_uniform_location("wind_movement"), wind_movement);

        const double movesc     = 0.25;
        vector3 forward         = -invmvr.column3(2) * movesc;
        vector3 upward          = invmvr.column3(1) * movesc;
        vector3 sideward        = invmvr.column3(0) * movesc;
        const double move_speed = 0.003;
        pos += forward * mv_forward * delta_t * move_speed + sideward * mv_sideward * delta_t * move_speed
               + upward * mv_upward * delta_t * move_speed;

        // record fps
        float fps = fpsm.account_frame();

        SYS().prepare_2d_drawing();
        std::ostringstream oss;
        oss << "FPS: " << fps << "\n(all time total " << fpsm.get_total_fps() << ")";
        font_arial->print(0, 0, oss.str());
        SYS().unprepare_2d_drawing();

        SYS().finish_frame();
    }

    delete terraintex;
}
