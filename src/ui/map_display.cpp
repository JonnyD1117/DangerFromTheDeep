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

// user display: general map view
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#include "map_display.hpp"

#include "airplane.hpp"
#include "cfg.hpp"
#include "coastmap.hpp"
#include "convoy.hpp"
#include "font.hpp"
#include "game.hpp"
#include "game_editor.hpp"
#include "global_data.hpp"
#include "keys.hpp"
#include "primitives.hpp"
#include "ship.hpp"
#include "submarine.hpp"
#include "system_interface.hpp"
#include "texts.hpp"
#include "texture.hpp"
#include "torpedo.hpp"
#include "user_interface.hpp"
#include "xml.hpp"

#include <memory>
#include <sstream>
#include <utility>

using std::list;
using std::ostringstream;
using std::pair;
using std::string;
using std::vector;

#define MAPGRIDSIZE 1000 // meters

enum edit_panel_fg_result
{
    EPFG_CANCEL,
    EPFG_SHIPADDED,
    EPFG_CHANGEMOTION,
    EPFG_CHANGETIME,
    EPFG_ADDSELTOCV,
    EPFG_MAKENEWCV,
    EPFG_DELETECV,
    EPFG_EDITROUTECV
};

void map_display::draw_vessel_symbol(const vector2& offset, const sea_object* so, color c) const
{
    //	cout << "draw " << so << " hdg " << so->get_heading().value() << " ort "
    //<< so->get_orientation() << " orang " <<
    //		angle(so->get_orientation().rotate(0,1,0).xy()).value() << "\n";
    vector2 d = so->get_heading().direction();
    float w   = so->get_width() * mapzoom / 2;
    float l   = so->get_length() * mapzoom / 2;
    vector2 p = (so->get_pos().xy() + offset) * mapzoom;
    p.x += 512;
    p.y = 384 - p.y;

    primitive<4> vesselshape(GL_QUADS, c);
    vesselshape.vertices[3] = vector3f(p.x - d.y * w, p.y - d.x * w, 0);
    vesselshape.vertices[2] = vector3f(p.x - d.x * l, p.y + d.y * l, 0);
    vesselshape.vertices[1] = vector3f(p.x + d.y * w, p.y + d.x * w, 0);
    vesselshape.vertices[0] = vector3f(p.x + d.x * l, p.y - d.y * l, 0);
    vesselshape.render();
    primitives::line(vector2f(p.x - d.x * l, p.y + d.y * l), vector2f(p.x + d.x * l, p.y - d.y * l), c).render();
}

void map_display::draw_trail(const sea_object* so, const vector2& offset) const
{
    // fixme: clean up this mess. maybe merge with function in water.cpp
    // we draw trails in both functions.
    const auto* shp = dynamic_cast<const ship*>(so);
    if (shp != nullptr) {
        const list<ship::prev_pos>& l = shp->get_previous_positions();
        if (l.empty()) {
            return;
        }
        vector2 p = (shp->get_pos().xy() + offset) * mapzoom;
        primitives tr(GL_LINE_STRIP, l.size() + 1);
        tr.vertices[0].x = 512 + p.x;
        tr.vertices[0].y = 384 - p.y;
        tr.colors[0]     = colorf(1, 1, 1, 1);
        float la         = 1.0 / float(l.size());
        float lc         = 0;
        unsigned trc     = 1;
        for (const auto& it : l) {
            tr.colors[trc]     = colorf(1, 1, 1, 1 - lc);
            vector2 p          = (it.pos + offset) * mapzoom;
            tr.vertices[trc].x = 512 + p.x;
            tr.vertices[trc].y = 384 - p.y;
            lc += la;
            ++trc;
        }
        tr.render();
    }
}

void map_display::draw_pings(class game& gm, const vector2& offset) const
{
    // draw pings (just an experiment, you can hear pings, locate their
    // direction
    //	a bit fuzzy but not their origin or exact shape).
    const auto& pings = gm.get_pings();
    for (const auto& p : pings) {
        // vector2 r = player->get_pos ().xy () - p.pos;
        vector2 p1 = (p.pos + offset) * mapzoom;
        vector2 p2 = p1 + (p.dir + p.ping_angle).direction() * p.range * mapzoom;
        vector2 p3 = p1 + (p.dir - p.ping_angle).direction() * p.range * mapzoom;
        primitive_col<3> tri(GL_TRIANGLES);
        tri.vertices[0].x = 512 + p1.x;
        tri.vertices[0].y = 384 - p1.y;
        tri.colors[0]     = colorf(0.5, 0.5, 0.5, 1);
        tri.vertices[1].x = 512 + p2.x;
        tri.vertices[1].y = 384 - p2.y;
        tri.colors[1]     = colorf(0.5, 0.5, 0.5, 0);
        tri.vertices[2].x = 512 + p3.x;
        tri.vertices[2].y = 384 - p3.y;
        tri.colors[2]     = colorf(0.5, 0.5, 0.5, 0);
        tri.render();
    }
}

void map_display::draw_sound_contact(
    class game& /*gm*/,
    const sea_object* player,
    double max_view_dist,
    const vector2& offset) const
{
    const auto& obj = player->get_sonar_objects();
    for (const auto& it : obj) {
        vector2 ldir = it.pos - player->get_pos().xy();
        ldir         = ldir.normal() * 0.666666 * max_view_dist * mapzoom;
        vector2 pos  = (player->get_pos().xy() + offset) * mapzoom;
        colorf col;
        switch (it.type) {
            case MERCHANT:
                col = colorf(0, 0, 0);
                break;
            case WARSHIP:
                col = colorf(0, 0.5, 0);
                break;
            case ESCORT:
                col = colorf(1, 0, 0);
                break;
            case SUBMARINE:
                col = colorf(1, 0, 0.5);
                break;
            default:
                // unknown object, not used yet
                col = colorf(0, 0.5, 0.5);
                break;
        }
        primitives::line(vector2f(512 + pos.x, 384 - pos.y), vector2f(512 + pos.x + ldir.x, 384 - pos.y - ldir.y), col)
            .render();
    }
}

void map_display::draw_sound_contact(game& /*gm*/, const submarine* player, const vector2& offset) const
{
    const auto& contacts = player->get_sonarman().get_contacts();
    for (const auto& it : contacts) {
        // basic length 2km plus 10m per dB, max. 200dB or similar
        double lng   = 2000 + it.second.strength_dB * 10;
        vector2 ldir = angle(it.first).direction() * lng * mapzoom;
        vector2 pos  = (player->get_pos().xy() + offset) * mapzoom;
        colorf col;
        switch (it.second.type) {
            case MERCHANT:
                col = colorf(0, 0, 0);
                break;
            case WARSHIP:
                col = colorf(0, 0.5, 0);
                break;
            case ESCORT:
                col = colorf(1, 0, 0);
                break;
            case SUBMARINE:
                col = colorf(1, 0, 0.5);
                break;
            case NONE:
            default:
                // unknown object, not used yet
                col = colorf(0, 0.5, 0.5);
                // col = colorf(0.75,0.75,0.2);
                break;
        }
        primitives::line(vector2f(512 + pos.x, 384 - pos.y), vector2f(512 + pos.x + ldir.x, 384 - pos.y - ldir.y), col)
            .render();
    }
}

void map_display::draw_visual_contacts(class game& /*gm*/, const sea_object* player, const vector2& offset) const
{
    // draw vessel trails and symbols (since player is submerged, he is drawn
    // too)
    const auto& objs = player->get_visible_objects();

    // draw trails
    for (const auto* obj : objs) {
        draw_trail(obj, offset);
    }

    // draw vessel symbols
    for (const auto* obj : objs) {
        color c;
        if (dynamic_cast<const submarine*>(obj) != nullptr) {
            c = color(255, 255, 128);
        } else if (dynamic_cast<const torpedo*>(obj) != nullptr) {
            c = color(255, 0, 0);
        } else if (dynamic_cast<const ship*>(obj) != nullptr) {
            c = color(192, 255, 192);
        } else if (dynamic_cast<const airplane*>(obj) != nullptr) {
            c = color(0, 0, 64);
        }
        draw_vessel_symbol(offset, obj, c);
    }
}

void map_display::draw_radar_contacts(class game& /*gm*/, const sea_object* player, const vector2& offset) const
{
    const auto& objs = player->get_radar_objects();

    // draw trails
    for (const auto* obj : objs) {
        draw_trail(obj, offset);
    }

    // draw vessel symbols
    for (const auto* obj : objs) {
        color c;
        if (dynamic_cast<const submarine*>(obj) != nullptr) {
            c = color(255, 255, 128);
        } else if (dynamic_cast<const ship*>(obj) != nullptr) {
            c = color(192, 255, 192);
        }
        draw_vessel_symbol(offset, obj, c);
    }
}

void map_display::draw_square_mark(class game& /*gm*/, const vector2& mark_pos, const vector2& offset, const color& c)
    const
{
    vector2 p = (mark_pos + offset) * mapzoom;
    int x     = int(round(p.x));
    int y     = int(round(p.y));
    primitives::rectangle(vector2f(512 - 4 + x, 384 - 4 - y), vector2f(512 + 4 + x, 384 + 4 - y), c).render();
}

void map_display::draw_square_mark_special(
    class game& /*gm*/,
    const vector2& mark_pos,
    const vector2& offset,
    const color& c) const
{
    vector2 p = (mark_pos + offset) * mapzoom;
    int x     = int(round(p.x));
    int y     = int(round(p.y));
    primitives::rectangle(vector2f(512 - 8 + x, 384 - 8 - y), vector2f(512 + 8 + x, 384 + 8 - y), c).render();
    primitives::diamond(vector2f(512 + x, 384 - y), 8, c).render();
}

map_display::map_display(user_interface& ui_)
    : user_display(ui_)
    , mapzoom(0.1)
    , mapmode(0)
    , notepadsheet(texture_store().ref("notepadsheet.png"))
{
    game& gm = ui_.get_game();

    auto* ge = dynamic_cast<game_editor*>(&gm);
    if (ge != nullptr) {
        game_editor& gme = *ge;

        // create editor main panel
        edit_panel = std::make_unique<widget>(0, 0, 1024, 32, "");
        edit_panel->set_background({});
        edit_panel->add_child<widget_caller_button<map_display&, game_editor&>>(
            0, 0, 128, 32, texts::get(224), [](auto& md, auto& gme) { md.edit_add_obj(gme); }, *this, gme);
        edit_btn_del = edit_panel->add_child<widget_caller_button<map_display&, game_editor&>>(
            128, 0, 128, 32, texts::get(225), [](auto& md, auto& gme) { md.edit_del_obj(gme); }, *this, gme);
        edit_btn_chgmot = edit_panel->add_child<widget_caller_button<map_display&, game_editor&>>(
            256, 0, 128, 32, texts::get(226), [](auto& md, auto& gme) { md.edit_change_motion(gme); }, *this, gme);
        edit_btn_copy = edit_panel->add_child<widget_caller_button<map_display&, game_editor&>>(
            384, 0, 128, 32, texts::get(227), [](auto& md, auto& gme) { md.edit_copy_obj(gme); }, *this, gme);
        edit_btn_cvmenu = edit_panel->add_child<widget_caller_button<map_display&, game_editor&>>(
            512, 0, 128, 32, texts::get(228), [](auto& md, auto& gme) { md.edit_convoy_menu(gme); }, *this, gme);
        edit_panel->add_child<widget_caller_button<map_display&, game_editor&>>(
            640, 0, 128, 32, texts::get(229), [](auto& md, auto& gme) { md.edit_time(gme); }, *this, gme);
        edit_panel->add_child<widget_caller_button<map_display&, game_editor&>>(
            768, 0, 128, 32, texts::get(233), [](auto& md, auto& gme) { md.edit_description(gme); }, *this, gme);
        edit_panel->add_child<widget_caller_button<map_display&, game_editor&>>(
            896, 0, 128, 32, texts::get(230), [](auto& md, auto& gme) { md.edit_help(gme); }, *this, gme);

        // create "add ship" window

        edit_panel_add = std::make_unique<widget>(0, 32, 1024, 768 - 2 * 32, texts::get(224));
        edit_panel_add->set_background({});
        edit_shiplist = edit_panel_add->add_child<widget_list>(20, 32, 1024 - 2 * 20, 768 - 2 * 32 - 2 * 32 - 8);
        edit_panel_add->add_child<widget_caller_button<widget&>>(
            20,
            768 - 3 * 32 - 8,
            512 - 20,
            32,
            texts::get(224),
            [](auto& w) { w.close(EPFG_SHIPADDED); },
            *edit_panel_add);
        edit_panel_add->add_child<widget_caller_button<widget&>>(
            512,
            768 - 3 * 32 - 8,
            512 - 20,
            32,
            texts::get(117),
            [](auto& w) { w.close(EPFG_CANCEL); },
            *edit_panel_add);
        for (const auto& it : data_file().get_ship_list()) {
            edit_shiplist->append_entry(it);
        }

        // create "motion edit" window
        // open widget with text edits: course, speed, throttle
        edit_panel_chgmot = std::make_unique<widget>(0, 32, 1024, 768 - 2 * 32, texts::get(226));
        edit_panel_chgmot->set_background({});
        edit_heading =
            edit_panel_chgmot->add_child<widget_slider>(20, 128, 1024 - 40, 80, texts::get(1), 0, 360, 0, 15);
        edit_speed = edit_panel_chgmot->add_child<widget_slider>(
            20, 220, 1024 - 40, 80, texts::get(4), 0 /*minspeed*/, 34 /*maxspeed*/, 0, 1);
        edit_throttle = edit_panel_chgmot->add_child<widget_slider>(
            20, 330, 1024 - 40, 80, texts::get(232), 0 /*minspeed*/, 34 /*maxspeed*/, 0, 1);
        edit_panel_chgmot->add_child<widget_caller_button<widget&>>(
            20,
            768 - 3 * 32 - 8,
            512 - 20,
            32,
            texts::get(226),
            [](auto& w) { w.close(EPFG_CHANGEMOTION); },
            *edit_panel_chgmot);
        edit_panel_chgmot->add_child<widget_caller_button<widget&>>(
            512,
            768 - 3 * 32 - 8,
            512 - 20,
            32,
            texts::get(117),
            [](auto& w) { w.close(EPFG_CANCEL); },
            *edit_panel_chgmot);
        // also edit: target, country, damage status, fuel amount

        // create help window
        edit_panel_help = std::make_unique<widget>(0, 32, 1024, 768 - 2 * 32, texts::get(230));
        edit_panel_help->set_background({});
        edit_panel_help->add_child<widget_text>(
            20, 32, 1024 - 2 * 20, 768 - 2 * 32 - 2 * 32 - 8, texts::get(231), true);
        edit_panel_help->add_child<widget_caller_button<widget&>>(
            20,
            768 - 3 * 32 - 8,
            1024 - 20,
            32,
            texts::get(105),
            [](auto& w) { w.close(EPFG_CANCEL); },
            *edit_panel_help);
        // create edit time window
        edit_panel_time = std::make_unique<widget>(0, 32, 1024, 768 - 2 * 32, texts::get(229));
        edit_panel_time->set_background({});
        edit_panel_time->add_child<widget_caller_button<widget&>>(
            20,
            768 - 3 * 32 - 8,
            512 - 20,
            32,
            texts::get(229),
            [](auto& w) { w.close(EPFG_CHANGETIME); },
            *edit_panel_time);
        edit_panel_time->add_child<widget_caller_button<widget&>>(
            512,
            768 - 3 * 32 - 8,
            512 - 20,
            32,
            texts::get(117),
            [](auto& w) { w.close(EPFG_CANCEL); },
            *edit_panel_time);
        edit_timeyear =
            edit_panel_time->add_child<widget_slider>(20, 128, 1024 - 40, 80, texts::get(234), 1939, 1945, 0, 1);
        edit_timemonth =
            edit_panel_time->add_child<widget_slider>(20, 208, 1024 - 40, 80, texts::get(235), 1, 12, 0, 1);
        edit_timeday  = edit_panel_time->add_child<widget_slider>(20, 288, 1024 - 40, 80, texts::get(236), 1, 31, 0, 1);
        edit_timehour = edit_panel_time->add_child<widget_slider>(20, 368, 1024 - 40, 80, texts::get(237), 0, 23, 0, 1);
        edit_timeminute =
            edit_panel_time->add_child<widget_slider>(20, 448, 1024 - 40, 80, texts::get(238), 0, 59, 0, 5);
        edit_timesecond =
            edit_panel_time->add_child<widget_slider>(20, 528, 1024 - 40, 80, texts::get(239), 0, 59, 0, 5);

        // create convoy menu
        edit_panel_convoy = std::make_unique<widget>(0, 32, 1024, 768 - 2 * 32, texts::get(228));
        edit_panel_convoy->set_background({});
        edit_panel_convoy->add_child<widget_text>(20, 32, 256, 32, texts::get(244));
        edit_cvname =
            edit_panel_convoy->add_child<widget_edit>(256 + 20, 32, 1024 - 256 - 2 * 20, 32, "-not usable yet, fixme-");
        edit_cvspeed = edit_panel_convoy->add_child<widget_slider>(
            20, 64, 1024 - 40, 80, texts::get(245), 0 /*minspeed*/, 34 /*maxspeed*/, 0, 1);
        edit_cvlist = edit_panel_convoy->add_child<widget_list>(20, 144, 1024 - 2 * 20, 768 - 144 - 3 * 32 - 8);
        edit_panel_convoy->add_child<widget_caller_button<widget&>>(
            20 + 0 * (1024 - 40) / 5,
            768 - 3 * 32 - 8,
            (1024 - 40) / 5,
            32,
            texts::get(240),
            [](auto& w) { w.close(EPFG_ADDSELTOCV); },
            *edit_panel_convoy);
        edit_panel_convoy->add_child<widget_caller_button<widget&>>(
            20 + 1 * (1024 - 40) / 5,
            768 - 3 * 32 - 8,
            (1024 - 40) / 5,
            32,
            texts::get(241),
            [](auto& w) { w.close(EPFG_MAKENEWCV); },
            *edit_panel_convoy);
        edit_panel_convoy->add_child<widget_caller_button<widget&>>(
            20 + 2 * (1024 - 40) / 5,
            768 - 3 * 32 - 8,
            (1024 - 40) / 5,
            32,
            texts::get(242),
            [](auto& w) { w.close(EPFG_DELETECV); },
            *edit_panel_convoy);
        edit_panel_convoy->add_child<widget_caller_button<widget&>>(
            20 + 3 * (1024 - 40) / 5,
            768 - 3 * 32 - 8,
            (1024 - 40) / 5,
            32,
            texts::get(243),
            [](auto& w) { w.close(EPFG_EDITROUTECV); },
            *edit_panel_convoy);
        edit_panel_convoy->add_child<widget_caller_button<widget&>>(
            20 + 4 * (1024 - 40) / 5,
            768 - 3 * 32 - 8,
            (1024 - 40) / 5,
            32,
            texts::get(117),
            [](auto& w) { w.close(EPFG_CANCEL); },
            *edit_panel_convoy);
        // fixme: en/disable some buttons depending on wether we have a
        // selection or not

        check_edit_sel();
    }
}

void map_display::edit_add_obj(game_editor& /*gm*/)
{
    edit_panel_add->open();
    edit_panel->disable();
    edit_panel_fg = edit_panel_add;
}

void map_display::edit_del_obj(game_editor& gm)
{
    // just delete all selected objects, if they are no subs
    for (auto it : selection) {
        if (it != gm.get_player_id()) {
            gm.get_object(it).kill();
        }
    }
    selection.clear();
    check_edit_sel();
}

void map_display::edit_change_motion(game_editor& gm)
{
    if (selection.empty()) {
        return;
    }

    // compute max speed.
    int minspeed = 0;
    int maxspeed = 0;
    for (auto it : selection) {
        auto& obj     = gm.get_object(it);
        const ship* s = dynamic_cast<const ship*>(&obj);
        if (s != nullptr) {
            int sp   = int(sea_object::ms2kts(s->get_max_speed()) + 0.5);
            maxspeed = std::max(maxspeed, sp);
        }
    }

    edit_panel_chgmot->open();
    edit_speed->set_values(minspeed, maxspeed, 0, 1);
    edit_throttle->set_values(minspeed, maxspeed, 0, 1);
    edit_panel->disable();
    edit_panel_fg = edit_panel_chgmot;
}

void map_display::edit_copy_obj(game_editor& gm)
{
    // just duplicate the objects with some position offset (1km to x/y)
    std::unordered_set<sea_object_id> new_selection;
    vector3 offset(300, 100, 0);
    for (auto it : selection) {
        auto& obj = gm.get_object(it);
        ship* s   = dynamic_cast<ship*>(&obj);
        auto* su  = dynamic_cast<submarine*>(&obj);
        if ((s != nullptr) && su == nullptr) {
            xml_doc spec(data_file().get_filename(s->get_specfilename()));
            spec.load();
            ship s2(gm.get_date(), gm.get_model_store(), spec.first_child());
            s2.set_skin_layout(model::default_layout);
            // set pos and other values etc.
            vector3 pos = s->get_pos() + offset;
            s2.manipulate_position(pos);
            s2.manipulate_speed(s->get_speed());
            s2.manipulate_heading(s->get_heading());
            s2.manipulate_invulnerability(true);
            s2.set_throttle(int(s->get_throttle()));
            new_selection.insert(gm.spawn_ship(std::move(s2)).first);
        }
    }
    selection = std::move(new_selection);
    check_edit_sel();
}

void map_display::edit_convoy_menu(game_editor& gm)
{
    edit_panel_convoy->open();
    edit_panel->disable();
    edit_panel_fg = edit_panel_convoy;
    // make convoy from currently selected objects, but without sub
    if (selection.empty()) {
        // fixme: disable
    } else {
        // fixme: enable
    }
    // fill list of convoy names
    edit_cvlist->clear();
    for (const auto& [id, convoy] : gm.get_convoy_list()) {
        string nm = convoy.get_name();
        if (nm.length() == 0) {
            nm = "???";
        }
        edit_cvlist->append_entry(nm);
    }
    // fill in current cv name and speed
    //...
}

void map_display::edit_time(game_editor& /*gm*/)
{
    // open widget with text edits: date/time
    // enter date and time of day
    edit_panel_time->open();
    edit_panel->disable();
    edit_panel_fg = edit_panel_time;
}

void map_display::edit_description(game_editor& gm)
{
    // game must store mission description/briefing to make this function
    // work... fixme only store short description here? or take save file name
    // in save dialogue as description? we have no multiline-edit-widget. so we
    // can't really let the user enter long descriptions here.
}

void map_display::edit_help(game_editor& /*gm*/)
{
    edit_panel_help->open();
    edit_panel->disable();
    edit_panel_fg = edit_panel_help;
}

void map_display::check_edit_sel()
{
    if (selection.empty()) {
        edit_btn_del->disable();
        edit_btn_chgmot->disable();
        edit_btn_copy->disable();
    } else {
        edit_btn_del->enable();
        edit_btn_chgmot->enable();
        edit_btn_copy->enable();
    }
}

void map_display::display() const
{
    auto& gm           = ui.get_game();
    sea_object* player = gm.get_player();
    bool is_day_mode   = gm.is_day_mode();

    if (is_day_mode) {
        glClearColor(0.0F, 0.0F, 1.0F, 1.0F);
    } else {
        glClearColor(0.0F, 0.0F, 0.75F, 1.0F);
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    double max_view_dist = gm.get_max_view_distance();

    vector2 offset = player->get_pos().xy() + mapoffset;

    SYS().prepare_2d_drawing();

    float delta = MAPGRIDSIZE * mapzoom;
    float sx    = helper::mod(512.F, delta) - helper::mod(offset.x, double(MAPGRIDSIZE)) * mapzoom;
    float sy    = 768.0 - (helper::mod(384.0F, delta) - helper::mod(offset.y, double(MAPGRIDSIZE)) * mapzoom);
    int lx      = int(1024 / delta) + 2;
    int ly      = int(768 / delta) + 2;

    // draw grid
    if (mapzoom >= 0.01) {
        colorf col(0.5, 0.5, 1);
        for (int i = 0; i < lx; ++i) {
            primitives::line(vector2f(sx, 0), vector2f(sx, 768), col).render();
            sx += delta;
        }
        for (int i = 0; i < ly; ++i) {
            primitives::line(vector2f(0, sy), vector2f(1024, sy), col).render();
            sy -= delta;
        }
    }
    // draw map
    if (mapmode == 0) {
        glPushMatrix();
        glTranslatef(512, 384, 0);
        glScalef(mapzoom, mapzoom, 1);
        glScalef(1, -1, 1);
        glTranslatef(-offset.x, -offset.y, 0);
        glCullFace(GL_BACK);                            // we must render the map with front-faced tris
        ui.get_coastmap().draw_as_map(offset, mapzoom); //, detl); // detail should depend on zoom, fixme
        glCullFace(GL_FRONT);                           // clean up
        glPopMatrix();
    } else {
        height_generator& hg = gm.get_height_gen();
        unsigned level       = 0;
        vector2i size((1024.0 / mapzoom) / hg.get_sample_spacing(), (768.0 / mapzoom) / hg.get_sample_spacing());
        vector2i bl(
            (offset.x - (512.0 / mapzoom)) / hg.get_sample_spacing(),
            (offset.y - (384.0 / mapzoom)) / hg.get_sample_spacing());

        while ((size.x > 1024 || size.y > 768) && level < 7) {
            level++;
            size.x = size.x >> 1;
            size.y = size.y >> 1;
            bl.x   = bl.x >> 1;
            bl.y   = bl.y >> 1;
        }

        bivector<float> heights{vector2i(size)};
        std::vector<uint8_t> colors(size.x * size.y * 3);
        hg.compute_heights(level, bl, size, heights.data_ptr(), 0, 0, true);

        for (int y = 0; y < size.y; y++) {
            for (int x = 0; x < size.x; x++) {
                float& height = heights.at(x, y);

                float weight                           = std::max(0.0, (6000.0 - std::abs(height - 9000.0)) / 6000.0);
                colors[(y * size.x * 3) + (x * 3) + 0] = weight * 255;

                weight                                 = std::max(0.0, ((3000.0) - std::abs(height - 3000.0)) / 3000.0);
                colors[(y * size.x * 3) + (x * 3) + 1] = weight * 255;

                weight = std::max(0.0, ((-11000.0) - std::abs(height - 0.0)) / (-11000.0));
                colors[(y * size.x * 3) + (x * 3) + 2] = weight * 255;
            }
        }
        texture atlanticmap(colors, size.x, size.y, GL_RGB, texture::LINEAR, texture::CLAMP);
        primitives::textured_quad(vector2f(0.0, 0.0), vector2f(1024, 768), atlanticmap).render();
    }

    // draw city names
    const auto& cities = ui.get_coastmap().get_city_list();
    for (const auto& citie : cities) {
        draw_square_mark(gm, citie.first, -offset, color(255, 0, 0));
        vector2 pos = (citie.first - offset) * mapzoom;
        font_vtremington12->print(int(512 + pos.x), int(384 - pos.y), citie.second);
    }

    // draw convoy positions	fixme: should be static and fade out after some
    // time
    vector<vector2> convoy_pos = gm.convoy_positions();
    for (auto& convoy_po : convoy_pos) {
        draw_square_mark_special(gm, convoy_po, -offset, color(0, 0, 0));
    }

    // draw view range
    primitives::circle(
        vector2f(512 - mapoffset.x * mapzoom, 384 + mapoffset.y * mapzoom), max_view_dist * mapzoom, colorf(1, 0, 0))
        .render();

    auto target = gm.get_player()->get_target();

    // draw vessel symbols (or noise contacts)
    auto* sub_player = dynamic_cast<submarine*>(player);
    if ((sub_player != nullptr) && sub_player->is_submerged()) {
        // draw pings
        draw_pings(gm, -offset);

        // draw sound contacts
        // draw_sound_contact(gm, sub_player, max_view_dist, -offset);	// old
        // contacts
        draw_sound_contact(gm, sub_player, -offset);

        // draw player trails and player
        draw_trail(player, -offset);
        draw_vessel_symbol(-offset, sub_player, color(255, 255, 128));

        // Special handling for submarine player: When the submarine is
        // on periscope depth and the periscope is up the visual contact
        // must be drawn on map.
        if ((sub_player->get_depth() <= sub_player->get_periscope_depth()) && sub_player->is_scope_up()) {
            draw_visual_contacts(gm, sub_player, -offset);

            // Draw a red box around the selected target.
            if (gm.is_valid(target)) {
                draw_square_mark(gm, gm.get_object(target).get_pos().xy(), -offset, color(255, 0, 0));
            }
        }
    } else // enable drawing of all object as testing hack by commenting this,
           // fixme
    {
        draw_visual_contacts(gm, player, -offset);
        draw_radar_contacts(gm, player, -offset);

        // Draw a red box around the selected target.
        if (gm.is_valid(target)) {
            draw_square_mark(gm, gm.get_object(target).get_pos().xy(), -offset, color(255, 0, 0));
        }
    }

#if 1
    // test: draw sonar signals as circles with varying radii
    vector<pair<double, noise>> signal_strengths;
    const unsigned signal_res = 360;
    signal_strengths.resize(signal_res);
    for (unsigned i = 0; i < signal_res; ++i) {
        angle a(360.0 * i / signal_res);
        signal_strengths[i] = gm.sonar_listen_ships(sub_player, a);
    }
    // render the strengths as circles with various colors
    primitives circle(GL_LINE_LOOP, signal_res, colorf(1, 1, 1, 1));
    for (unsigned j = 0; j < noise::NR_OF_FREQUENCY_BANDS; ++j) {
        float f    = 1.0F - float(j) / noise::NR_OF_FREQUENCY_BANDS;
        circle.col = colorf(f, f, f * 0.5F);
        for (unsigned i = 0; i < signal_res; ++i) {
            angle a            = angle(360.0 * i / signal_res) + sub_player->get_heading();
            double r           = signal_strengths[i].second.frequencies[j] * 15;
            vector2 p          = (sub_player->get_pos().xy() - offset + a.direction() * r) * mapzoom;
            circle.vertices[i] = vector2f(512 + p.x, 384 - p.y).xy0();
        }
        circle.render();
    }
    // draw total signal strength
    circle.col = colorf(1.0, 0.5, 0.5);
    for (unsigned i = 0; i < signal_res; ++i) {
        angle a            = angle(360.0 * i / signal_res) + sub_player->get_heading();
        double r           = signal_strengths[i].first * 15;
        vector2 p          = (sub_player->get_pos().xy() - offset + a.direction() * r) * mapzoom;
        circle.vertices[i] = vector2f(512 + p.x, 384 - p.y).xy0();
    }
    circle.render();
//	for (int i = 0; i <= 179; ++i) {
//		printf("test[%i]=%f\n",
//		       i,
//		       compute_signal_strength_GHG(angle(45.0), 6000,
// angle(double(i))));
//	}
#endif

    // draw notepad sheet giving target distance, speed and course
    if (gm.is_valid(target)) {
        int nx = 768;
        int ny = 512;
        notepadsheet.get()->draw(nx, ny);
        ostringstream os0;
        ostringstream os1;
        ostringstream os2;
        auto& mytarget = gm.get_object(target);
        // fixme: use estimated values from target/tdc estimation here, make
        // functions for that
        os0 << texts::get(3) << ": " << unsigned(mytarget.get_pos().xy().distance(player->get_pos().xy()))
            << texts::get(206);
        os1 << texts::get(4) << ": " << unsigned(fabs(sea_object::ms2kts(mytarget.get_speed()))) << texts::get(208);
        os2 << texts::get(1) << ": " << unsigned(mytarget.get_heading().value()) << texts::get(207);
        font_vtremington12->print(nx + 16, ny + 40, os0.str(), color(0, 0, 0));
        font_vtremington12->print(nx + 16, ny + 60, os1.str(), color(0, 0, 0));
        font_vtremington12->print(nx + 16, ny + 80, os2.str(), color(0, 0, 0));
    }

    // draw world coordinates for mouse
    double mouserealmx = double(mouse_position.x - 512) / mapzoom + offset.x;
    double mouserealmy = double(384 - mouse_position.y) / mapzoom + offset.y;
    unsigned degrx;
    unsigned degry;
    unsigned minutx;
    unsigned minuty;
    bool west;
    bool south;
    sea_object::meters2degrees(mouserealmx, mouserealmy, west, degrx, minutx, south, degry, minuty);
    ostringstream rwcoordss;
    rwcoordss << degry << "/" << minuty << (south ? "S" : "N") << ", " << degrx << "/" << minutx << (west ? "W" : "E");
    font_vtremington12->print(0, 0, rwcoordss.str(), color::white(), true);

    // editor specials
    // ------------------------------------------------------------
    if (gm.is_editor()) {
        if (edit_panel_fg) {
            edit_panel_fg->draw();
        } else {
            // selection rectangle
            if (mouse_position_down.x >= 0 && mouse_position_down.y >= 0) {
                int x1 = std::min(mouse_position_down.x, mouse_position.x);
                int y1 = std::min(mouse_position_down.y, mouse_position.y);
                int x2 = std::max(mouse_position_down.x, mouse_position.x);
                int y2 = std::max(mouse_position_down.y, mouse_position.y);
                primitives::rectangle(vector2f(x1, y1), vector2f(x2, y2), colorf(1, 1, 0, 1)).render();
            }
            // selected objects
            for (auto it : selection) {
                draw_square_mark(gm, gm.get_object(it).get_pos().xy(), -offset, color(255, 0, 64));
            }
        }
        edit_panel->draw();
    }

    ui.draw_infopanel();
    SYS().unprepare_2d_drawing();
}

auto map_display::handle_key_event(const key_data& k) -> bool
{
    if (ui.get_game().is_editor()) {
        if (widget::handle_key_event(*edit_panel, k)) {
            return true;
        }
        // check if foreground window is open and event should go to it
        if (edit_panel_fg != nullptr) {
            return widget::handle_key_event(*edit_panel_fg, k);
        }
        // no panel visible. handle extra edit modes
        state_of_key_modifiers = k.mod;
    }

    // non-editor events.
    if (k.down()) {
        if (is_configured_key(key_command::ZOOM_MAP, k)) {
            if (mapzoom < 1) {
                mapzoom *= 2;
            }
            return true;
        }
        if (is_configured_key(key_command::UNZOOM_MAP, k)) {
            if (mapzoom > 1.0 / 16384) {
                mapzoom /= 2;
            }
            return true;
        }
        if (k.keycode == key_code::m) {
            mapmode++;
            if (mapmode > 1) {
                mapmode = 0;
            }
            return true;
        }
    }
    return false;
}

auto map_display::handle_mouse_button_event(const mouse_click_data& m) -> bool
{
    auto& gm           = ui.get_game();
    sea_object* player = gm.get_player();
    if (gm.is_editor()) {
        auto& ge = static_cast<game_editor&>(gm);
        if (edit_panel->is_mouse_over(m.position_2d) && widget::handle_mouse_button_event(*edit_panel, m)) {
            return true;
        }
        // check if foreground window is open and event should go to it
        if (edit_panel_fg != nullptr) {
            if (widget::handle_mouse_button_event(*edit_panel_fg, m)) {
                // we could compare edit_panel_fg to the pointers of the various
                // panels here instead of using a global enum for all possible
                // return values...
                if (edit_panel_fg->was_closed()) {
                    int retval = edit_panel_fg->get_return_value();
                    if (retval == EPFG_SHIPADDED) {
                        // add ship
                        xml_doc spec(data_file().get_filename(edit_shiplist->get_selected_entry()));
                        spec.load();
                        ship shp(gm.get_date(), gm.get_model_store(), spec.first_child());
                        shp.set_skin_layout(model::default_layout);
                        // set pos and other values etc.
                        vector2 pos = gm.get_player()->get_pos().xy() + mapoffset;
                        shp.manipulate_position(pos.xy0());
                        shp.manipulate_invulnerability(true);
                        gm.spawn_ship(std::move(shp));
                    } else if (retval == EPFG_CHANGEMOTION) {
                        for (auto it : selection) {
                            auto& obj = gm.get_object(it);
                            ship* s   = dynamic_cast<ship*>(&obj);
                            if (s != nullptr) {
                                s->set_throttle(edit_throttle->get_curr_value());
                                s->manipulate_heading(angle(edit_heading->get_curr_value()));
                                s->manipulate_speed(edit_speed->get_curr_value());
                            }
                        }
                    } else if (retval == EPFG_CHANGETIME) {
                        date d(
                            edit_timeyear->get_curr_value(),
                            edit_timemonth->get_curr_value(),
                            edit_timeday->get_curr_value(),
                            edit_timehour->get_curr_value(),
                            edit_timeminute->get_curr_value(),
                            edit_timesecond->get_curr_value());
                        double time = d.get_time();
                        ge.manipulate_time(time);
                        // construct new date to correct possible wrong date
                        // values like 30th february or so...
                        ge.manipulate_equipment_date(date(d.get_time()));
                    } else if (retval == EPFG_ADDSELTOCV) {
                        // compute center of ships
                        vector2 center;
                        unsigned nrsh = 0;
                        for (auto it : selection) {
                            auto& obj = gm.get_object(it);
                            ship* s   = dynamic_cast<ship*>(&obj);
                            if (s != nullptr) {
                                center += s->get_pos().xy();
                                ++nrsh;
                            }
                        }
                        center = center * (1.0 / nrsh);
                        // create convoy object
                        convoy cv(center, edit_cvname->get_text());
                        // add all ships to convoy with relative positions
                        nrsh = 0;
                        for (auto it : selection) {
                            auto& obj = gm.get_object(it);
                            ship* s   = dynamic_cast<ship*>(&obj);
                            if (s != nullptr) {
                                if (cv.add_ship(gm, it)) {
                                    ++nrsh;
                                }
                            }
                        }
                        // add convoy to class game, if it has ships
                        if (nrsh > 0) {
                            gm.spawn(std::move(cv));
                        }
                    }
                    edit_panel->enable();
                    edit_panel_fg = nullptr;
                }
                return true;
            }
            return false;
        }
        // no panel visible. handle extra edit modes
        if (m.down() && m.left()) {
            mouse_position_down = m.position_2d;
            return true;
        }
        if (m.up() && m.left()) {
            mouse_position = m.position_2d;
            // check for shift / ctrl
            unsigned mode = 0; // replace selection
            if (key_mod_shift(state_of_key_modifiers)) {
                mode = 1; // subtract
            }
            if (key_mod_ctrl(state_of_key_modifiers)) {
                mode = 2; // add
            }
            if (mouse_position != mouse_position_down) {
                // group select
                int x1 = std::min(mouse_position_down.x, mouse_position.x);
                int y1 = std::min(mouse_position_down.y, mouse_position.y);
                int x2 = std::max(mouse_position_down.x, mouse_position.x);
                int y2 = std::max(mouse_position_down.y, mouse_position.y);
                // fixme: later all objects
                auto objs = gm.visible_surface_objects(player);
                if (mode == 0) {
                    selection.clear();
                }
                for (auto& obj : objs) {
                    vector2 p = (obj->get_pos().xy() - (player->get_pos().xy() + mapoffset)) * mapzoom;
                    p.x += 512;
                    p.y = 384 - p.y;
                    if (p.x >= x1 && p.x <= x2 && p.y >= y1 && p.y <= y2) {
                        auto id = ge.get_id(*obj);
                        if (mode == 1) {
                            selection.erase(id);
                        } else {
                            selection.insert(id);
                        }
                    }
                }
                check_edit_sel();
            } else {
                // select nearest
                vector2 mapclick(mouse_position.x, mouse_position.y);
                // fixme: later all objects!
                auto objs           = gm.visible_surface_objects(player);
                double mapclickdist = 1e30;
                sea_object_id target;
                if (mode == 0) {
                    selection.clear();
                }
                for (auto& obj : objs) {
                    vector2 p = (obj->get_pos().xy() - (player->get_pos().xy() + mapoffset)) * mapzoom;
                    p.x += 512;
                    p.y           = 384 - p.y;
                    double clickd = mapclick.square_distance(p);
                    if (clickd < mapclickdist) {
                        auto id      = ge.get_id(*obj);
                        target       = id;
                        mapclickdist = clickd;
                    }
                }
                if (mode == 1) {
                    selection.erase(target);
                } else {
                    selection.insert(target);
                }
                check_edit_sel();
            }
            mouse_position_down = {-1, -1};
            return true;
        }
    }

    // non-editor events.
    if (m.down() && m.left()) {
        // set target. get visible objects and determine which is nearest to
        // mouse position. set target for player object
        vector2 mapclick(m.position_2d);
        auto objs           = gm.visible_surface_objects(player);
        double mapclickdist = 1e30;
        sea_object_id target;
        for (auto& obj : objs) {
            if (!obj->is_alive()) {
                continue;
            }
            vector2 p = (obj->get_pos().xy() - (player->get_pos().xy() + mapoffset)) * mapzoom;
            p.x += 512;
            p.y           = 384 - p.y;
            double clickd = mapclick.square_distance(p);
            if (clickd < mapclickdist) {
                auto id = gm.get_id(*obj); // fixme later using sensor contacts
                                           // here to select contact!
                target       = id;         // fixme: message?
                mapclickdist = clickd;
            }
        }

        player->set_target(target, gm);
        return true;
    }
    return false;
}

auto map_display::handle_mouse_motion_event(const mouse_motion_data& m) -> bool
{
    if (ui.get_game().is_editor()) {
        // handle mouse events for edit panel if that exists.
        if (edit_panel->is_mouse_over(m.position_2d) && widget::handle_mouse_motion_event(*edit_panel, m)) {
            return true;
        }
        // check if foreground window is open and event should go to it
        if (edit_panel_fg != nullptr) {
            return widget::handle_mouse_motion_event(*edit_panel_fg, m);
        }
        // no panel visible. handle extra edit modes
        mouse_position = m.position_2d;
        if (m.middle() && key_mod_ctrl(state_of_key_modifiers)) {
            // move selected objects!
            vector2 drag = vector2(m.relative_motion_2d) * (1.0 / mapzoom);
            for (auto it : selection) {
                auto& obj = ui.get_game().get_object(it);
                vector3 p = obj.get_pos();
                p.x += drag.x;
                p.y += drag.y;
                obj.manipulate_position(p);
            }
            return true;
        }
        return false;
    }

    // non-editor events.
    mouse_position = m.position_2d;
    if (m.middle() && key_mod_ctrl(state_of_key_modifiers)) {
        vector2 motion(m.relative_motion_2d);
        motion.y = -motion.y;
        mapoffset += motion * (1.0 / mapzoom);
        return true;
    }
    return false;
}

auto map_display::handle_mouse_wheel_event(const mouse_wheel_data& m) -> bool
{
    if (m.up()) {
        if (mapzoom < 1) {
            mapzoom *= 1.25;
        }
        return true;
    }
    if (m.down()) {
        if (mapzoom > 1.0 / 16384) {
            mapzoom /= 1.25;
        }
    }
    return false;
}
