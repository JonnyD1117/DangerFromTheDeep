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

// bvtree-bvtree intersect test
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#include "OglExt.h"
#include "cfg.hpp"
#include "datadirs.hpp"
#include "log.hpp"
#include "make_mesh.hpp"
#include "model.hpp"
#include "mymain.cpp"
#include "shader.hpp"
#include "system_interface.hpp"
#include "triangle_intersection.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using std::vector;

inline double rnd()
{
    return double(rand()) / RAND_MAX;
}

int mymain(std::vector<std::string>& args)
{
    if (args.size() != 2)
        return -1;

    cfg& mycfg = cfg::instance();
    mycfg.register_option("screen_res_x", 1024);
    mycfg.register_option("screen_res_y", 768);
    mycfg.register_option("fullscreen", true);
    mycfg.register_option("debug", false);
    mycfg.register_option("sound", true);
    mycfg.register_option("use_hqsfx", true);
    mycfg.register_option("use_ani_filtering", false);
    mycfg.register_option("anisotropic_level", 1.0f);
    mycfg.register_option("use_compressed_textures", false);
    mycfg.register_option("multisampling_level", 0);
    mycfg.register_option("use_multisampling", false);
    mycfg.register_option("bloom_enabled", false);
    mycfg.register_option("hdr_enabled", false);
    mycfg.register_option("hint_multisampling", 0);
    mycfg.register_option("hint_fog", 0);
    mycfg.register_option("hint_mipmap", 0);
    mycfg.register_option("hint_texture_compression", 0);
    mycfg.register_option("vsync", false);
    mycfg.register_option("water_detail", 128);
    mycfg.register_option("wave_fft_res", 128);
    mycfg.register_option("wave_phases", 256);
    mycfg.register_option("wavetile_length", 256.0f);
    mycfg.register_option("wave_tidecycle_time", 10.24f);
    mycfg.register_option("usex86sse", true);
    mycfg.register_option("language", 0);
    mycfg.register_option("cpucores", 1);
    mycfg.register_option("terrain_texture_resolution", 0.1f);

    system_interface::parameters params;

    params.resolution   = {1024, 768};
    params.near_z       = 1.0;
    params.far_z        = 1000.0;
    params.fullscreen   = false;
    params.resolution2d = {1024, 768};

    system_interface::create_instance(params);

    std::cout << "Testing intersection of models:\n";
    std::cout << args[0] << "\n";
    std::cout << args[1] << "\n";

    std::unique_ptr<model> modelA(new model(args[0]));
    std::unique_ptr<model> modelB(new model(args[1]));

    modelA->register_layout(model::default_layout);
    modelB->register_layout(model::default_layout);

    modelA->set_layout(model::default_layout);
    modelB->set_layout(model::default_layout);

    modelA->get_base_mesh().compute_bv_tree();
    modelB->get_base_mesh().compute_bv_tree();

    // modelA->get_base_mesh().bounding_volume_tree->debug_dump();
    // modelB->get_base_mesh().bounding_volume_tree->debug_dump();

    vector3f pos(0, 0, 10);
    vector3f viewangles(0, 0, 0);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    model* curr_model        = modelA.get();
    bool move_not_rotate     = true;
    unsigned axis            = 0;
    bool intersects          = false;
    bool intersects_tri      = false;
    bool render_spheres      = false;
    unsigned splevel         = 0;
    matrix4f transformA      = matrix4f::one();
    matrix4f transformB      = matrix4f::trans(50.0, 50.0, 0.0);
    matrix4f* curr_transform = &transformA;
    bool check_tri_tri       = false;

    bool doquit = false;
    auto ic     = std::make_shared<input_event_handler_custom>();
    ic->set_handler([&](const input_event_handler::key_data& k) {
        if (k.down()) {
            switch (k.keycode) {
                case key_code::ESCAPE:
                    doquit = true;
                    break;
                case key_code::a:
                    curr_model     = modelA.get();
                    curr_transform = &transformA;
                    break;
                case key_code::b:
                    curr_model     = modelB.get();
                    curr_transform = &transformB;
                    break;
                case key_code::m:
                    move_not_rotate = true;
                    break;
                case key_code::r:
                    move_not_rotate = false;
                    break;
                case key_code::x:
                    axis = 0;
                    break;
                case key_code::y:
                    axis = 1;
                    break;
                case key_code::z:
                    axis = 2;
                    break;
                case key_code::s:
                    render_spheres = !render_spheres;
                    break;
                case key_code::_1:
                    if (splevel > 0)
                        --splevel;
                    break;
                case key_code::_2:
                    ++splevel;
                    break;
                case key_code::_3:
                    splevel = 0;
                    break;
                case key_code::t:
                    check_tri_tri  = !check_tri_tri;
                    intersects_tri = false;
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
        if (m.right()) {
            matrix4f transf;
            if (move_not_rotate) {
                vector3f t;
                (&t.x)[axis] = m.relative_motion_2d.x * 0.1f;
                transf       = matrix4f::trans(t);
            } else {
                switch (axis) {
                    case 0:
                        transf = matrix4f::rot_x(m.relative_motion_2d.x * 0.1f);
                        break;
                    case 1:
                        transf = matrix4f::rot_y(m.relative_motion_2d.x * 0.1f);
                        break;
                    case 2:
                        transf = matrix4f::rot_z(m.relative_motion_2d.x * 0.1f);
                        break;
                }
            }
            *curr_transform = transf * *curr_transform;
            matrix4f transA = transformA * modelA->get_base_mesh_transformation();

            matrix4f transB = transformB * modelB->get_base_mesh_transformation();

            model::mesh& mA = modelA->get_base_mesh();
            model::mesh& mB = modelB->get_base_mesh();

            // here we transform in world space
            bv_tree::param p0(mA.get_bv_tree(), mA.vertices, transA);
            bv_tree::param p1(mB.get_bv_tree(), mB.vertices, transB);
#if 0
			std::vector<vector3f> contact_points;
			intersects = bv_tree::collides(p0, p1, contact_points);
#else
            vector3f contact_point;
            intersects = bv_tree::closest_collision(p0, p1, contact_point);
#endif
            if (check_tri_tri) {
                matrix4f transformAtoB = transB.inverse() * transA;
                intersects_tri         = mA.intersects(mB, transformAtoB);
            }
            return true;
        } else if (m.left()) {
            viewangles.x += m.relative_motion_2d.x;
            viewangles.y += m.relative_motion_2d.y;
            return true;
        } else if (m.middle()) {
            viewangles.y += m.relative_motion_2d.x;
            viewangles.z += m.relative_motion_2d.y;
            return true;
        }
        return false;
    });
    ic->set_handler([&](const input_event_handler::mouse_wheel_data& m) {
        if (m.up()) {
            pos.z -= 1;
            return true;
        } else if (m.down()) {
            pos.z += 1;
            return true;
        }
        return false;
    });
    SYS().add_input_event_handler(ic);

    // hier laufen lassen
    while (!doquit) {
        if (intersects) {
            if (intersects_tri) {
                glClearColor(1.0, 0.2, 0.2, 0.0);
            } else {
                glClearColor(1.0, 0.8, 0.2, 0.0);
            }
        } else {
            if (intersects_tri) {
                glClearColor(0.2, 0.8, 1.0, 0.0);
            } else {
                glClearColor(0.2, 0.2, 1.0, 0.0);
            }
        }
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

        glLoadIdentity();

        glTranslated(-pos.x, -pos.y, -pos.z);
        glRotatef(viewangles.z, 0, 0, 1);
        glRotatef(viewangles.y, 0, 1, 0);
        glRotatef(viewangles.x, 1, 0, 0);
        glMatrixMode(GL_MODELVIEW);

        glPushMatrix();
        transformA.multiply_gl();
        modelA->display();
        glPopMatrix();

        glPushMatrix();
        transformB.multiply_gl();
        modelB->display();
        glPopMatrix();

        if (render_spheres) {
            std::vector<spheref> volumesA, volumesB;
            const bv_tree& b0 = modelA->get_base_mesh().get_bv_tree();
            const bv_tree& b1 = modelA->get_base_mesh().get_bv_tree();

            b0.collect_volumes_of_tree_depth(volumesA, splevel);
            b1.collect_volumes_of_tree_depth(volumesB, splevel);

            std::unique_ptr<model::material> mat0(new model::material());
            std::unique_ptr<model::material> mat1(new model::material());

            mat0->diffuse = color(255, 255, 255, 128);
            mat1->diffuse = color(128, 32, 32, 128);

            std::vector<std::unique_ptr<model::mesh>> spheresA, spheresB;

            spheresA.resize(volumesA.size());
            spheresB.resize(volumesB.size());

            unsigned k = 0;
            for (auto& it : volumesA) {
                spheresA[k] = std::unique_ptr<model::mesh>(make_mesh::sphere(it.radius, 2 * it.radius));

                spheresA[k]->transform(matrix4f::trans(it.center));
                spheresA[k]->compile();

                glPushMatrix();
                (transformA * modelA->get_base_mesh_transformation()).multiply_gl();
                spheresA[k]->mymaterial = mat0.get();
                spheresA[k]->display();
                glPopMatrix();

                ++k;
            }
            k = 0;
            for (auto& it : volumesB) {
                spheresB[k] = std::unique_ptr<model::mesh>(make_mesh::sphere(it.radius, 2 * it.radius));

                spheresB[k]->transform(matrix4f::trans(it.center));
                spheresB[k]->compile();

                glPushMatrix();
                (transformB * modelB->get_base_mesh_transformation()).multiply_gl();
                spheresB[k]->mymaterial = mat1.get();
                spheresB[k]->display();
                glPopMatrix();

                ++k;
            }
        }

        SYS().finish_frame();
    }
    ic = nullptr;

    return 0;
}
