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

// a model viewer
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define DATADIR "./data/"
#endif

#include "cfg.hpp"
#include "datadirs.hpp"
#include "filehelper.hpp"
#include "font.hpp"
#include "image.hpp"
#include "log.hpp"
#include "make_mesh.hpp"
#include "model.hpp"
#include "object_store.hpp"
#include "primitives.hpp"
#include "system_interface.hpp"
#include "texture.hpp"
#include "vector3.hpp"
#include "widget.hpp"
#include "xml.hpp"

#include <glu.h>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#define VIEWMODEL

#include "mymain.cpp"

using std::vector;

// switch features on/off:

#undef IS_INSIDE_TEST

int res_x, res_y;
font* font_arial         = nullptr;
font* font_vtremington12 = nullptr;

vector4t<GLfloat> lposition(0, 0, 0, 1);

#define LIGHT_ANG_PER_SEC 30

// Forward declaration
void view_model(const string& modelfilename, const string& datafilename);

string model_layout;

struct constraint
{
    unsigned obj;
    float min;
    float max;
    bool increasing;
    float current;
};

class model_load_dialog
{
  public:
    model_load_dialog();
    virtual ~model_load_dialog() = default;

    void get_model_list(const std::vector<std::string>& namelist);
    const std::vector<std::string>& get_models();

    void load_menu();
    void load_model(widget_list* list);

  private:
    struct model_entry
    {
        string name;
        string dir;
    };

    unique_ptr<widget::theme> theme;
    std::vector<model_entry> files;

    int selected_model;

    void message(const string& msg);
};

model_load_dialog::model_load_dialog()
{
    theme = std::make_unique<widget::theme>(
        "widgetelements_menu.png",
        "widgeticons_menu.png",
        font_vtremington12,
        color(182, 146, 137),
        color(222, 208, 195),
        color(92, 72, 68));
}

void model_load_dialog::get_model_list(const std::vector<std::string>& namelist)
{
    for (const auto& it : namelist) {
        model_entry new_model = {it, data_file().get_path(it)};
        files.push_back(new_model);
    }
}

void model_load_dialog::load_menu()
{

    widget w(0, 0, 1024, 768, "", "threesubs.jpg");
    w.set_theme(std::move(theme));

    w.add_child<widget_text>(
        10,
        10,
        800,
        80,
        "Danger from the Deep Viewmodel OpenGL Frontend.\nCopyright (C) "
        "2003-2020 Thorsten Jordan.");
    w.add_child<widget_text>(300, 100, 424, 30, "Available Models:", true);
    auto models_list = w.add_child<widget_list>(300, 150, 424, 438);

    auto it = files.begin();
    for (; it != files.end(); ++it) {
        models_list->append_entry(it->name);
    }

    auto wm = w.add_child<widget_menu>(87, 650, 400, 40, "", true);

    wm->set_entry_spacing(50);

    wm->add_entry(
        "Load",
        std::make_unique<widget_caller_button<model_load_dialog&, widget_list*>>(
            [](auto& mld, auto* ml) { mld.load_model(ml); }, *this, &*models_list.get<widget_list>()));

    wm->add_entry("Quit", std::make_unique<widget_caller_button<widget&>>([](auto& w) { w.close(0); }, w));

    widget::run(w, 0, false);
}

void model_load_dialog::load_model(widget_list* models)
{
    selected_model = models->get_selected();

    model_entry entry = files[selected_model];

    string data_filename = entry.dir + entry.name;
    string model_filename;

    try {
        xml_doc dataxml(data_filename);
        dataxml.load();
        xml_elem root  = dataxml.first_child().child("classification");
        model_filename = root.attr("modelname");

        view_model(model_filename, data_filename);
    }
    catch (...) {
        message("Unable to read one of the files:\n\n" + data_filename + "\n" + model_filename);
    }
}

void model_load_dialog::message(const string& msg)
{
    widget w(0, 0, 1024, 768, "", "threesubs.jpg");

    w.add_child<widget_text>(0, 0, 0, 0, msg, true);

    auto wm = w.add_child<widget_menu>(112, 120, 200, 40, "", true);
    wm->add_entry("OK", std::make_unique<widget_caller_button<widget&>>([](auto& w) { w.close(0); }, w));

    widget::run(w, 0, false);
}

int scalelength(int i)
{
    return (i % 50 == 0) ? 5 : ((i % 10 == 0) ? 4 : ((i % 5 == 0) ? 2 : 1));
}

void view_model(const string& modelfilename, const string& datafilename)
{
    auto* mdl = new model(/*get_model_dir() + */ modelfilename);
    mdl->register_layout(model_layout);
    mdl->set_layout(model_layout);

    mdl->write_to_dftd_model_file("test.xml");

    //	mdl->get_mesh(0).write_off_file("test.off");

#ifdef IS_INSIDE_TEST // Test hack for is_inside function
    const unsigned iizz = 6, iiyy = 10, iixx = 8;
    // fixme: maybe better use double resolution in every direction, then merge
    // the is inside (some anti aliasing to make it more accurate)
    std::vector<bool> ii(iizz * iiyy * iixx);
    unsigned total_in = 0;
    vector3f dimmin = mdl->get_min(), dimmax = mdl->get_max();
    vector3f dimlen = dimmax - dimmin;
    vector3f voxelsz(dimlen.x / iixx, dimlen.y / iiyy, dimlen.z / iizz);
    vector3f voxeloff  = dimmin + voxelsz * 0.5f;
    vector3f voxeloff2 = voxeloff + vector3f(0.5f, 0.5f, 0.5f);
    std::cout << "voxelsize: " << voxelsz << " voxeloff " << voxeloff << "\n";
    for (unsigned izz = 0; izz < iizz; ++izz) {
        std::cout << "crosssection picture " << izz + 1 << "/" << iizz << "\n";
        for (unsigned iyy = 0; iyy < iiyy; ++iyy) {
            for (unsigned ixx = 0; ixx < iixx; ++ixx) {
                bool is_in = (mdl->is_inside(vector3f(voxelsz.x * ixx, voxelsz.y * iyy, voxelsz.z * izz) + voxeloff));
                is_in =
                    is_in || (mdl->is_inside(vector3f(voxelsz.x * ixx, voxelsz.y * iyy, voxelsz.z * izz) + voxeloff2));
                total_in += is_in ? 1 : 0;
                ii[(izz * iiyy + iyy) * iixx + ixx] = is_in;
                std::cout << (is_in ? "X" : " ");
            }
            std::cout << "\n";
        }
    }
    std::cout << "Center of gravity of mesh#0: " << mdl->get_mesh(0).compute_center_of_gravity() << "\n";
    std::cout << "Inertia tensor of mesh#0:\n";
    mdl->get_mesh(0).compute_inertia_tensor().print();
    std::cout << "Inside: " << total_in << " of " << ii.size() << "\n";

    /* // test code
    unsigned t1 = SYS().millisec();
    for (int i = 0; i < 1000; ++i)
        mdl->get_mesh(0).compute_center_of_gravity();
    unsigned t2 = SYS().millisec();
    for (int i = 0; i < 1000; ++i)
        mdl->get_mesh(0).compute_inertia_tensor();
    unsigned t3 = SYS().millisec();
    std::cout << "Time A = " << t2-t1 << " Time B = " << t3-t2 << "\n";
    */

#endif

    // rendering mode

    int wireframe = 0;

    // Smoke variables

    bool smoke = false;
    vector3 smoke_pos;
    bool smoke_display = false;

    // Read xml file if supplied

    try {
        xml_doc dataxml(datafilename);
        dataxml.load();
        xml_elem smoke_elem = dataxml.first_child().child("smoke");
        // smoke_type = smoke_elem.attri("type");
        float smokex = smoke_elem.attrf("x");
        float smokey = smoke_elem.attrf("y");
        float smokez = smoke_elem.attrf("z");

        smoke_pos = vector3(smokex, smokey, smokez);
        smoke     = true;
    }
    catch (...) {
        smoke = false;
    }

    /*
        // fixme test hack
        delete mdl;
        mdl = new model("test.xml", true, false);
        mdl->write_to_dftd_model_file("test2.xml");
        delete mdl;
        mdl = new model("test2.xml", true, false);
        mdl->write_to_dftd_model_file("test3.xml");
    */

    double sc = (mdl->get_boundbox_size() * 0.5).length();
    vector3 viewangles;
    // place viewer along positive z-axis
    vector3 pos(0, 0, sc);
    bool lightmove        = true;
    bool coordinatesystem = false;

    vector<uint8_t> pixels(32 * 32 * 3, 64);
    for (unsigned i = 0; i < 32 * 32; ++i) {
        pixels[3 * i + 2] = (((i / 32) + (i % 32)) & 1) * 127 + 128;
    }
    vector<uint8_t> bumps(32 * 32);
    for (unsigned i = 0; i < 32 * 32; ++i) {
        bumps[i] = 0;
    } //(((i/32)%8)<7 && ((i%32)%8)<7)?255:0; }
    auto* dmap = new model::material::map();
    auto* bmap = new model::material::map();
    dmap->set_texture(new texture(pixels, 32, 32, GL_RGB, texture::NEAREST, texture::CLAMP));
    bmap->set_texture(new texture(bumps, 32, 32, GL_LUMINANCE, texture::LINEAR, texture::CLAMP, true));
    auto* mat     = new model::material();
    mat->specular = color::white();
    mat->colormap.reset(dmap);
    mat->normalmap.reset(bmap);
    mdl->add_material(mat);
    model::mesh* msh = make_mesh::cube(3 * sc, 3 * sc, 3 * sc, 1.0f, 1.0f, false);
    //	model::mesh* msh = make_mesh::sphere(1.5*sc, 3*sc, 16, 16, 1.0f, 1.0f,
    // false); 	model::mesh* msh = make_mesh::cylinder(1.5*sc, 3*sc,
    // 16, 1.0f, 1.0f, true, false);
    msh->mymaterial = mat;
    // mdl->add_mesh(msh);
    msh->compile();
    mdl->compile();

    unsigned time1 = SYS().millisec();
    double ang     = 0;

    model::mesh* lightsphere = make_mesh::sphere(5.0f, 5.0f, 8, 8, 1, 1, true, "sun");
    //	model::mesh* lightsphere = make_mesh::cube(5.0f, 5.0f, 5.0f, 1, 1, true,
    //"sun");
    lightsphere->mymaterial           = new model::material();
    lightsphere->mymaterial->diffuse  = color(255, 255, 128);
    lightsphere->mymaterial->specular = color(255, 255, 128);
    lightsphere->compile();

    unsigned frames     = 1;
    unsigned lastframes = 1;
    double fpstime      = SYS().millisec() / 1000.0;
    double totaltime    = SYS().millisec() / 1000.0;
    double measuretime  = 5; // seconds

    unsigned max_objects = 0;
    while (mdl->object_exists(max_objects))
        ++max_objects;

    log_info("Found " << (max_objects - 1) << " objects");

    vector<constraint> constraints;

    for (unsigned ix = 0; ix < max_objects; ++ix) {
        vector2f mm = mdl->get_object_angle_constraints(ix);
        // get_object_translation_constraints
        constraint c = {ix, mm.x, mm.y, true, 0};
        constraints.push_back(c);
    }

    float ang_delta = 0.0f;
    vector3f smoke_delta;

    bool doquit     = false;
    bool xyzpressed = false;
    auto ic         = std::make_shared<input_event_handler_custom>();
    ic->set_handler([&](const input_event_handler::key_data& k) {
        if (k.down()) {
            switch (k.keycode) {
                case key_code::ESCAPE:
                    doquit = true;
                    break;
                case key_code::KP_4:
                    pos.x -= 1.0;
                    break;
                case key_code::KP_6:
                    pos.x += 1.0;
                    break;
                case key_code::KP_8:
                    pos.y -= 1.0;
                    break;
                case key_code::KP_2:
                    pos.y += 1.0;
                    break;
                case key_code::KP_1:
                    pos.z -= 1.0;
                    break;
                case key_code::KP_3:
                    pos.z += 1.0;
                    break;
                case key_code::l:
                    lightmove = !lightmove;
                    break;
                case key_code::s: {
                    // Allow user to save smoke position
                    if (smoke && ((k.mod & key_mod::ctrl) != key_mod::none)) {
                        try {
                            xml_doc dataxml(datafilename);
                            dataxml.load();
                            xml_elem smoke_elem = dataxml.first_child().child("smoke");

                            smoke_elem.set_attr(smoke_pos.x, "x");
                            smoke_elem.set_attr(smoke_pos.y, "y");
                            smoke_elem.set_attr(smoke_pos.z, "z");

                            dataxml.save();
                        }
                        catch (...) {
                            std::cout << "unable to save smoke origin"
                                      << "\n";
                        }
                    } else {
                        pair<model::mesh*, model::mesh*> parts = mdl->get_mesh(0).split(vector3f(0, 1, 0), -1);
                        parts.first->transform(matrix4f::trans(0, 30, 50));
                        parts.second->transform(matrix4f::trans(0, -30, 50));
                        mdl->add_mesh(parts.first);
                        mdl->add_mesh(parts.second);
                    }
                } break;
                case key_code::c:
                    coordinatesystem = !coordinatesystem;
                    break;
                case key_code::p:
                    smoke_display = !smoke_display;
                    break;
                case key_code::w:
                    if (++wireframe > 1)
                        wireframe = 0;
                    break;
                case key_code::x:
                    smoke_delta.x = (k.mod & key_mod::shift) != key_mod::none ? 1.0f : 0.1f;
                    xyzpressed    = true;
                    break;
                case key_code::y:
                    smoke_delta.y = (k.mod & key_mod::shift) != key_mod::none ? 1.0f : 0.1f;
                    xyzpressed    = true;
                    break;
                case key_code::z:
                    smoke_delta.z = (k.mod & key_mod::shift) != key_mod::none ? 1.0f : 0.1f;
                    xyzpressed    = true;
                    break;
                default:
                    return false;
                    break;
            }
            return true;
        } else if (k.up()) {
            if (k.keycode == key_code::x) {
                smoke_delta.x = 0.f;
                xyzpressed    = false;
                return true;
            } else if (k.keycode == key_code::y) {
                smoke_delta.y = 0.f;
                xyzpressed    = false;
                return true;
            } else if (k.keycode == key_code::z) {
                smoke_delta.z = 0.f;
                xyzpressed    = false;
                return true;
            }
        }
        return false;
    });
    ic->set_handler([&](const input_event_handler::mouse_motion_data& m) {
        if (m.left()) {
            viewangles.y += m.relative_motion.x;
            viewangles.x += m.relative_motion.y;
            return true;
        } else if (m.right()) {
            viewangles.z += m.relative_motion.x;
            viewangles.x += m.relative_motion.y;
            return true;
        } else if (m.middle()) {
            pos.x += m.relative_motion.x;
            pos.y += m.relative_motion.y;
            return true;
        }
        return false;
    });
    ic->set_handler([&](const input_event_handler::mouse_wheel_data& m) {
        // Check if x,,y,z are pressed and if so mouse wheel moves smoke
        // position by delta. delta is either 1.0 if a shift key is pressed or
        // 0.1 otherwise.
        if (m.up()) {
            if (xyzpressed) {
                smoke_pos += smoke_delta;
            } else {
                pos.z -= 2;
            }
        } else if (m.down()) {
            if (xyzpressed) {
                smoke_pos -= smoke_delta;
            } else {
                pos.z += 2;
            }
        }
        if (smoke_pos.x < 0.00001 && smoke_pos.x > -0.00001)
            smoke_pos.x = 0;
        if (smoke_pos.y < 0.00001 && smoke_pos.y > -0.00001)
            smoke_pos.y = 0;
        if (smoke_pos.z < 0.00001 && smoke_pos.z > -0.00001)
            smoke_pos.z = 0;
        return true;
    });
    SYS().add_input_event_handler(ic);

    while (!doquit) {
        // rotate light
        unsigned time2 = SYS().millisec();
        if (lightmove && time2 > time1) {
            ang_delta = LIGHT_ANG_PER_SEC * (time2 - time1) / 1000.0;
            ang += ang_delta;
            if (ang > 360)
                ang -= 360;
            time1       = time2;
            lposition.x = 1.4 * sc * cos(3.14159 * ang / 180);
            lposition.z = 1.4 * sc * sin(3.14159 * ang / 180);
        } else {
            ang_delta = 0.0f;
        }

        // test: rotate objects
        for (auto& c : constraints) {
            if (c.max < c.current || c.min > c.current)
                c.increasing = !c.increasing;
            if (c.increasing) {
                c.current += ang_delta;
            } else {
                c.current -= ang_delta;
            }
            mdl->set_object_angle(c.obj, c.current);
        }
        //		mdl->set_object_translation(4, ang*50/360);

        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
        glColor3f(1, 1, 1);
        glBindTexture(GL_TEXTURE_2D, 0);

        glLoadIdentity();
        glTranslated(-pos.x, -pos.y, -pos.z);
        glRotatef(viewangles.z, 0, 0, 1);
        glRotatef(viewangles.y, 0, 1, 0);
        glRotatef(viewangles.x, 1, 0, 0);

        glPushMatrix();
        glTranslatef(lposition.x, lposition.y, lposition.z);
        vector4t<GLfloat> null(0, 0, 0, 1);
        glLightfv(GL_LIGHT0, GL_POSITION, &null.x);
        lightsphere->display();
        glPopMatrix();

        primitives::line(vector3f(0, 0, 0), vector3f(lposition.x, lposition.y, lposition.z), colorf(1, 1, 1)).render();

        primitives::line(vector3f(0, 0, 0), vector3f(1, 0, 0), colorf(1, 0, 0)).render();
        primitives::line(vector3f(0, 0, 0), vector3f(0, 1, 0), colorf(1, 1, 0)).render();
        primitives::line(vector3f(0, 0, 0), vector3f(0, 0, 1), colorf(0, 1, 0)).render();

        if (coordinatesystem) {
            glDisable(GL_LIGHTING);
            vector3f max = mdl->get_max();
            float h      = max.z;
            float w      = max.x;
            int c        = 0;
            primitive<244> coord1(GL_LINES, colorf(1, 0, 0, 1));
            coord1.vertices[c++] = vector3f(-30, 0, h);
            coord1.vertices[c++] = vector3f(30, 0, h);
            coord1.vertices[c++] = vector3f(-30, 0, -h);
            coord1.vertices[c++] = vector3f(30, 0, -h);
            for (int i = 0; i <= 30; ++i) {
                int l                = scalelength(i);
                coord1.vertices[c++] = vector3f(i, 0, h);
                coord1.vertices[c++] = vector3f(i, 0, h + l);
                coord1.vertices[c++] = vector3f(-i, 0, h);
                coord1.vertices[c++] = vector3f(-i, 0, h + l);
                coord1.vertices[c++] = vector3f(i, 0, -h);
                coord1.vertices[c++] = vector3f(i, 0, -h + l);
                coord1.vertices[c++] = vector3f(-i, 0, -h);
                coord1.vertices[c++] = vector3f(-i, 0, -h + l);
            }
            coord1.render();

            c = 0;
            primitive<1204> coord2(GL_LINES, colorf(0, 1, 0, 1));
            coord2.vertices[c++] = vector3f(0, -150, h);
            coord2.vertices[c++] = vector3f(0, 150, h);
            coord2.vertices[c++] = vector3f(0, -150, -h);
            coord2.vertices[c++] = vector3f(0, 150, -h);
            for (int i = 0; i <= 150; ++i) {
                int l                = scalelength(i);
                coord2.vertices[c++] = vector3f(0, i, h);
                coord2.vertices[c++] = vector3f(0, i, h + l);
                coord2.vertices[c++] = vector3f(0, -i, h);
                coord2.vertices[c++] = vector3f(0, -i, h + l);
                coord2.vertices[c++] = vector3f(0, i, -h);
                coord2.vertices[c++] = vector3f(0, i, -h + l);
                coord2.vertices[c++] = vector3f(0, -i, -h);
                coord2.vertices[c++] = vector3f(0, -i, -h + l);
            }
            coord2.render();

            c = 0;
            primitive<244> coord3(GL_LINES, colorf(1, 1, 0, 1));
            coord3.vertices[c++] = vector3f(w, 0, 30);
            coord3.vertices[c++] = vector3f(w, 0, -30);
            coord3.vertices[c++] = vector3f(-w, 0, 30);
            coord3.vertices[c++] = vector3f(-w, 0, -30);
            for (int i = 0; i <= 30; ++i) {
                int l                = scalelength(i);
                coord3.vertices[c++] = vector3f(w, 0, i);
                coord3.vertices[c++] = vector3f(w + l, 0, i);
                coord3.vertices[c++] = vector3f(w, 0, -i);
                coord3.vertices[c++] = vector3f(w + l, 0, -i);
                coord3.vertices[c++] = vector3f(-w, 0, i);
                coord3.vertices[c++] = vector3f(-w + l, 0, i);
                coord3.vertices[c++] = vector3f(-w, 0, -i);
                coord3.vertices[c++] = vector3f(-w + l, 0, -i);
            }
            coord3.render();

            glEnable(GL_LIGHTING);
        }

        msh->display();

        if (wireframe == 1) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        }

        matrix4 mvp = matrix4::get_gl(GL_PROJECTION_MATRIX) * matrix4::get_gl(GL_MODELVIEW_MATRIX);
        mdl->display();
        // test
        // mdl->display_mirror_clip();

        // display smoke origin
        if (smoke && smoke_display) {
            glDisable(GL_LIGHTING);
            glDisable(GL_DEPTH_TEST);

            primitive<6> origin(GL_LINES, colorf(1, 0, 0));
            origin.vertices[0] = vector3f(smoke_pos.x - 1, smoke_pos.y, smoke_pos.z);
            origin.vertices[1] = vector3f(smoke_pos.x + 1, smoke_pos.y, smoke_pos.z);
            origin.vertices[2] = vector3f(smoke_pos.x, smoke_pos.y - 1, smoke_pos.z);
            origin.vertices[3] = vector3f(smoke_pos.x, smoke_pos.y + 1, smoke_pos.z);
            origin.vertices[4] = vector3f(smoke_pos.x, smoke_pos.y, smoke_pos.z - 1);
            origin.vertices[5] = vector3f(smoke_pos.x, smoke_pos.y, smoke_pos.z + 1);
            origin.render();

            glEnable(GL_DEPTH_TEST);
            glEnable(GL_LIGHTING);
        }

        if (wireframe == 1) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        // draw scales if requested
        if (coordinatesystem) {
            glColor4f(1, 1, 1, 1);
        }

        SYS().prepare_2d_drawing();
        vector3f minp = mdl->get_min(), maxp = mdl->get_max();
        ostringstream os;
        os << "A simple model viewer for Danger from the Deep.\n"
           << "Press any key to exit.\n"
           << "Press left mouse button and move mouse to rotate x/y.\n"
           << "Press right mouse button and move mouse to rotate z.\n"
           << "Press 'p' to toggle smoke origin.\n"
           << "Press 'w' to toggle wireframe mode.\n"
           << "Rotation " << viewangles.x << ", " << viewangles.y << ", " << viewangles.z << "\nTranslation " << pos.x
           << ", " << pos.y << ", " << pos.z << "\nmin " << minp.x << ", " << minp.y << ", " << minp.z << "\nmax "
           << maxp.x << ", " << maxp.y << ", " << maxp.z
           << "\nbounding sphere radius=" << mdl->get_bounding_sphere_radius() << "\n";

        if (smoke) {
            os << "Smoke: " << ((smoke_display) ? "On" : "Off") << "\n";
            os << "Smoke origin " << smoke_pos.x << ", " << smoke_pos.y << ", " << smoke_pos.z << "\n";
        } else {
            os << "Smoke: Off (no info found).\n";
        }

        font_arial->print(0, 0, os.str());

        // print scale descriptions
        if (coordinatesystem) {
            matrix4 xf   = matrix4::trans(res_x / 2, res_y / 2, 0) * matrix4::diagonal(res_x / 2, -res_y / 2, 1) * mvp;
            vector3f max = mdl->get_max();
            float h      = max.z;
            float w      = max.x;

            glColor4f(1, 0, 0, 1);
            for (int i = 0; i <= 30; i += 5) {
                vector3 a(i, 0, h), b(i, 0, -h);
                vector2 c = (xf * a).xy();
                vector2 d = (xf * b).xy();
                char tmp[20];
                sprintf(tmp, "%i", i);
                // 2006-12-01 doc1972 print() needs int pixel coords, so we cast
                font_arial->print((int) c.x, (int) c.y, tmp);
                font_arial->print((int) d.x, (int) d.y, tmp);
            }
            glColor4f(0, 1, 0, 1);
            for (int i = 0; i <= 150; i += 5) {
                vector3 a(0, i, h), b(0, i, -h);
                vector2 c = (xf * a).xy();
                vector2 d = (xf * b).xy();
                char tmp[20];
                sprintf(tmp, "%i", i);
                // 2006-12-01 doc1972 print() needs int pixel coords, so we cast
                font_arial->print((int) c.x, (int) c.y, tmp);
                font_arial->print((int) d.x, (int) d.y, tmp);
            }
            glColor4f(1, 1, 0, 1);
            for (int i = 0; i <= 30; i += 5) {
                vector3 a(w, 0, i), b(-w, 0, i);
                vector2 c = (xf * a).xy();
                vector2 d = (xf * b).xy();
                char tmp[20];
                sprintf(tmp, "%i", i);
                // 2006-12-01 doc1972 print() needs int pixel coords, so we cast
                font_arial->print((int) c.x, (int) c.y, tmp);
                font_arial->print((int) d.x, (int) d.y, tmp);
            }
        }

        SYS().unprepare_2d_drawing();

        // record fps
        ++frames;
        totaltime = SYS().millisec() / 1000.0;
        if (totaltime - fpstime >= measuretime) {
            fpstime = totaltime;
            log_info("fps " << (frames - lastframes) / measuretime);
            lastframes = frames;
        }

        SYS().finish_frame();
    }

    delete msh;
    delete mdl;
}

void run_gui()
{
    model_load_dialog ml;

    // fixme: use data_file() 's lists here, no parsing of directories
    ml.get_model_list(data_file().get_ship_list());
    ml.get_model_list(data_file().get_submarine_list());
    ml.get_model_list(data_file().get_airplane_list());

    ml.load_menu();
}

auto image_store() -> object_store<image>&
{
    static object_store<image> image_store(get_image_dir());
    return image_store;
}

int mymain(std::vector<string>& args)
{
    // command line argument parsing
    res_x           = 1024;
    bool fullscreen = true;
    bool use_gui    = false;

    string modelfilename;
    string datafilename;
    model_layout = model::default_layout;

    for (auto it = args.begin(); it != args.end(); ++it) {
        if (*it == "--help") {
            cout << "DftD viewmodel, usage:\n--help\t\tshow this\n"
                 << "--res n\t\tuse resolution n horizontal,\n\t\tn is "
                    "512,640,800,1024 (recommended) or 1280\n"
                 << "--nofullscreen\tdon't use fullscreen\n"
                 << "--layout layoutname\tuse layout with specific name for "
                    "skins\n"
                 << "--gui starts viewmodel in GUI mode, with model list.\n"
                 << "MODELFILENAME\n";
            return 0;
        } else if (*it == "--nofullscreen") {
            fullscreen = false;
        } else if (*it == "--res") {
            auto it2 = it;
            ++it2;
            if (it2 != args.end()) {
                int r = atoi(it2->c_str());
                if (r == 512 || r == 640 || r == 800 || r == 1024 || r == 1280)
                    res_x = r;
                ++it;
            }
        } else if (*it == "--dataxml") {
            if (++it != args.end()) {
                datafilename = it->c_str();
            } else
                --it;
        } else if (*it == "--layout") {
            if (++it != args.end()) {
                model_layout = it->c_str();
            } else
                --it;
        } else if (*it == "--gui") {
            use_gui = true;
        } else {
            modelfilename = *it;
        }
    }

    res_y = res_x * 3 / 4;

    // parse configuration
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
    params.near_z       = 1.0;
    params.far_z        = 1000.0;
    params.resolution   = {res_x, res_y};
    params.resolution2d = {1024, 768};
    params.fullscreen   = fullscreen;
    system_interface::create_instance(params);

    log_info("A simple model viewer for DftD-.mdl files");
    log_info("copyright and written 2003 by Thorsten Jordan");

    GLfloat lambient[4]  = {0.1, 0.1, 0.09, 1};
    GLfloat ldiffuse[4]  = {1, 1, 0.9, 1};
    GLfloat lspecular[4] = {1, 1, 0.9, 1};
    glLightfv(GL_LIGHT0, GL_AMBIENT, lambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, ldiffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lspecular);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHTING);
    glEnable(GL_NORMALIZE);

    font_arial         = new font(get_font_dir() + "font_arial");
    font_vtremington12 = new font(get_font_dir() + "font_vtremington12");

    if (use_gui) {
        run_gui();
    } else {
        view_model(modelfilename, datafilename);
    }

    delete font_arial;
    delete font_vtremington12;

    return 0;
}
