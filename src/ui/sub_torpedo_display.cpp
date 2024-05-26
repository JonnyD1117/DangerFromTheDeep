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

// user display: submarine's torpedo room
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#include "sub_torpedo_display.hpp"

#include "font.hpp"
#include "game.hpp"
#include "global_data.hpp"
#include "primitives.hpp"
#include "system_interface.hpp"
#include "texts.hpp"
//#include "torpedo.hpp"
#include "user_interface.hpp"

#include <fstream>

namespace
{
const unsigned ILLEGAL_TUBE = unsigned(-1);

enum element_type
{
    et_torpempty     = 0,
    et_torpload      = 1,
    et_torpunload    = 2,
    et_torp1fat1     = 3,
    et_torp1lut1     = 4,
    et_torp1lut2     = 5,
    et_torp1         = 6,
    et_torp1practice = 7,
    et_torp2         = 8,
    et_torp3afat2    = 9,
    et_torp3alut1    = 10,
    et_torp3alut2    = 11,
    et_torp3fat2     = 12,
    et_torp3         = 13,
    et_torp4         = 14,
    et_torp5b        = 15,
    et_torp5         = 16,
    et_torp6lut1     = 17,
    et_torp11        = 666, // missing as well as more torpedo types

    // et_submodelVIIc = 0,
    et_hours   = 18,
    et_minutes = 19,
    et_seconds = 20,

    et_subtopsideview = 21
};

auto element_by_spec(const std::string& torpname) -> element_type
{
    if (torpname == "TI") {
        return et_torp1;
    }
    if (torpname == "TI_FaTI") {
        return et_torp1fat1;
    }
    if (torpname == "TI_LuTI") {
        return et_torp1lut1;
    }
    if (torpname == "TI_LuTII") {
        return et_torp1lut2;
    }
    if (torpname == "TII") {
        return et_torp2;
    }
    if (torpname == "TIII") {
        return et_torp3;
    }
    if (torpname == "TIII_FaTII") {
        return et_torp3fat2;
    }
    if (torpname == "TIIIa_FaTII") {
        return et_torp3afat2;
    }
    if (torpname == "TIIIa_LuTI") {
        return et_torp3alut1;
    }
    if (torpname == "TIIIa_LuTII") {
        return et_torp3alut2;
    }
    if (torpname == "TIV") {
        return et_torp4;
    }
    if (torpname == "TV") {
        return et_torp5;
    }
    if (torpname == "TVb") {
        return et_torp5b;
    }
    if (torpname == "TVI_LuTI") {
        return et_torp6lut1;
    }
    if (torpname == "TXI") {
        return et_torp1practice;
    }
    THROW(error, std::string("illegal torpedo type ") + torpname);
}
} // namespace

sub_torpedo_display::desc_text::desc_text(const std::string& filename)
{
    std::ifstream ifs(filename.c_str(), std::ios::in);
    if (ifs.fail()) {
        THROW(error, std::string("couldn't open ") + filename);
    }

    // read lines.
    while (!ifs.eof()) {
        std::string s;
        getline(ifs, s);
        txtlines.push_back(s);
    }
}

auto sub_torpedo_display::desc_text::str(unsigned startline, unsigned nrlines) const -> std::string
{
    startline        = std::min(startline, unsigned(txtlines.size()));
    unsigned endline = std::min(startline + nrlines, unsigned(txtlines.size()));
    std::string result;
    for (unsigned i = startline; i < endline; ++i) {
        result += txtlines[i] + "\n";
    }
    return result;
}

void sub_torpedo_display::draw_torpedo(
    class game& /*gm*/,
    bool usebow,
    const vector2i& pos,
    const submarine::stored_torpedo& st) const
{
    if (usebow) {
        if (st.status == 0) { // empty
            element_for_id(et_torpempty).get_texture().draw(pos);
        } else if (st.status == 1) { // reloading
            element_for_id(element_by_spec(st.specfilename)).get_texture().draw(pos);
            element_for_id(et_torpload).get_texture().draw(pos);
        } else if (st.status == 2) { // unloading
            element_for_id(et_torpempty).get_texture().draw(pos);
            element_for_id(et_torpunload).get_texture().draw(pos);
        } else { // loaded
            element_for_id(element_by_spec(st.specfilename)).get_texture().draw(pos);
        }
    } else {
        if (st.status == 0) { // empty
            element_for_id(et_torpempty).get_texture().draw_hm(pos);
        } else if (st.status == 1) { // reloading
            element_for_id(element_by_spec(st.specfilename)).get_texture().draw_hm(pos);
            element_for_id(et_torpload).get_texture().draw_hm(pos);
        } else if (st.status == 2) { // unloading
            element_for_id(et_torpempty).get_texture().draw_hm(pos);
            element_for_id(et_torpunload).get_texture().draw_hm(pos);
        } else { // loaded
            element_for_id(element_by_spec(st.specfilename)).get_texture().draw_hm(pos);
        }
    }
}

auto sub_torpedo_display::get_tubecoords(submarine* sub) -> std::vector<vector2i>
{
    std::vector<vector2i> tubecoords(sub->get_torpedoes().size());
    const auto bow_tube_indices          = sub->get_bow_tube_indices();
    const auto stern_tube_indices        = sub->get_stern_tube_indices();
    const auto bow_reserve_indices       = sub->get_bow_reserve_indices();
    const auto stern_reserve_indices     = sub->get_stern_reserve_indices();
    const auto bow_deckreserve_indices   = sub->get_bow_deckreserve_indices();
    const auto stern_deckreserve_indices = sub->get_stern_deckreserve_indices();
    unsigned k                           = bow_tube_indices.second - bow_tube_indices.first;
    // Note that these coordinates should be defined as clickable areas in the
    // layout.xml dump that out to a file for all possible tubes to have the
    // data.
    for (unsigned i = bow_tube_indices.first; i < bow_tube_indices.second; ++i) {
        tubecoords[i] = vector2i(23, 188 + (i - k / 2) * 13);
    }
    for (unsigned i = bow_reserve_indices.first; i < bow_reserve_indices.second; ++i) {
        unsigned j    = i - bow_reserve_indices.first;
        tubecoords[i] = vector2i(161 + (j / k) * 138, 188 + (j % k - k / 2) * 13);
    }
    for (unsigned i = bow_deckreserve_indices.first; i < bow_deckreserve_indices.second; ++i) {
        unsigned j    = i - bow_deckreserve_indices.first;
        tubecoords[i] = vector2i(161 + (j / 2) * 138, 145 + (j % 2) * 13);
    }
    for (unsigned i = stern_tube_indices.first; i < stern_tube_indices.second; ++i) {
        unsigned j    = i - stern_tube_indices.first;
        tubecoords[i] = vector2i(823, 188 + j * 13);
    }
    for (unsigned i = stern_reserve_indices.first; i < stern_reserve_indices.second; ++i) {
        unsigned j    = i - stern_reserve_indices.first;
        tubecoords[i] = vector2i(684, 188 + j * 13);
    }
    for (unsigned i = stern_deckreserve_indices.first; i < stern_deckreserve_indices.second; ++i) {
        unsigned j    = i - stern_deckreserve_indices.first;
        tubecoords[i] = vector2i(684 - (j / 2) * 138, 145 + (j % 2) * 13);
    }
    return tubecoords;
}

auto sub_torpedo_display::get_tube_below_mouse(const std::vector<vector2i>& tubecoords) const -> unsigned
{
    for (unsigned i = 0; i < tubecoords.size(); ++i) {
        if (mouse_position.x >= tubecoords[i].x && mouse_position.x < tubecoords[i].x + 128
            && mouse_position.y >= tubecoords[i].y && mouse_position.y < tubecoords[i].y + 16) {
            return i;
        }
    }
    return ILLEGAL_TUBE;
}

sub_torpedo_display::sub_torpedo_display(user_interface& ui_)
    : user_display(ui_, "sub_torpedo")
    , torptranssrc(ILLEGAL_TUBE)
    , desc_texts(get_data_dir())
    , torp_desc_line(0)
    , notepadsheet(texture_store().ref("notepadsheet.png"))
{
    // adjust filename for one element
    const auto* pl = static_cast<const submarine*>(ui.get_game().get_player());
    element_for_id(et_subtopsideview)
        .set_filename(
            get_data_dir() + data_file().get_rel_path(pl->get_specfilename()) + pl->get_torpedomanage_img_name());
}

void sub_torpedo_display::display() const
{
    auto& gm  = ui.get_game();
    auto* sub = dynamic_cast<submarine*>(gm.get_player());

    double hours   = 0.0;
    double minutes = 0.0;
    double seconds = 0.0;
    element_for_id(et_seconds).set_value(std::floor(seconds));
    element_for_id(et_minutes).set_value(minutes);
    element_for_id(et_hours).set_value(hours);

    // draw background and sub model
    draw_elements();

    // draw background
    SYS().prepare_2d_drawing();

    // draw sub model
    // if (subtopsideview.get())//fixme later do not accept empty data
    // subtopsideview->draw(0, 0);

    // tube handling. compute coordinates for display and mouse use
    const auto& torpedoes                = sub->get_torpedoes();
    std::vector<vector2i> tubecoords     = get_tubecoords(sub);
    const auto bow_tube_indices          = sub->get_bow_tube_indices();
    const auto stern_tube_indices        = sub->get_stern_tube_indices();
    const auto bow_reserve_indices       = sub->get_bow_reserve_indices();
    const auto stern_reserve_indices     = sub->get_stern_reserve_indices();
    const auto bow_deckreserve_indices   = sub->get_bow_deckreserve_indices();
    const auto stern_deckreserve_indices = sub->get_stern_deckreserve_indices();

    // draw tubes
    for (unsigned i = bow_tube_indices.first; i < bow_tube_indices.second; ++i) {
        draw_torpedo(gm, true, tubecoords[i], torpedoes[i]);
    }
    for (unsigned i = bow_reserve_indices.first; i < bow_reserve_indices.second; ++i) {
        draw_torpedo(gm, true, tubecoords[i], torpedoes[i]);
    }
    for (unsigned i = bow_deckreserve_indices.first; i < bow_deckreserve_indices.second; ++i) {
        draw_torpedo(gm, true, tubecoords[i], torpedoes[i]);
    }
    for (unsigned i = stern_tube_indices.first; i < stern_tube_indices.second; ++i) {
        draw_torpedo(gm, false, tubecoords[i], torpedoes[i]);
    }
    for (unsigned i = stern_reserve_indices.first; i < stern_reserve_indices.second; ++i) {
        draw_torpedo(gm, false, tubecoords[i], torpedoes[i]);
    }
    for (unsigned i = stern_deckreserve_indices.first; i < stern_deckreserve_indices.second; ++i) {
        draw_torpedo(gm, false, tubecoords[i], torpedoes[i]);
    }

    // draw transfer graphics if needed
    if (torptranssrc != ILLEGAL_TUBE && torpedoes[torptranssrc].status == submarine::stored_torpedo::st_loaded) {
        const auto& elem = element_for_id(element_by_spec(torpedoes[torptranssrc].specfilename));
        const auto sz    = elem.get_size();
        elem.get_texture().draw(mouse_position - sz / 2, colorf(1, 1, 1, 0.5));
        primitives::line(
            vector2f(tubecoords[torptranssrc].x + 124 / 2, tubecoords[torptranssrc].y + 12 / 2),
            vector2f(mouse_position),
            color::white())
            .render();
    }

    // draw information about torpedo in tube if needed
    unsigned tb = get_tube_below_mouse(tubecoords);
    if (tb != ILLEGAL_TUBE) {
        // display type info.
        // fixme load all info first and just store it!
        if (torpedoes[tb].status == submarine::stored_torpedo::st_loaded) {
            const desc_text* torpdesctext{};
            auto sfn = torpedoes[tb].specfilename;
            try {
                torpdesctext =
                    desc_texts.ref(data_file().get_rel_path(sfn) + sfn + "_" + texts::get_language_code() + ".txt");
            }
            catch (error& e) {
                // try again with english text if other text(s) don't exist.
                torpdesctext = desc_texts.ref(data_file().get_rel_path(sfn) + sfn + "_en.txt");
            }
            // fixme: implement scrolling here!
            // torpedo description text, for notepad
            if (torp_desc_line > torpdesctext->nr_of_lines()) {
                torp_desc_line = torpdesctext->nr_of_lines();
            }
            font_vtremington12->print_wrapped(100, 550, 570, 0, torpdesctext->str(torp_desc_line, 10), color(0, 0, 0));
        }
        if (torpedoes[tb].status == submarine::stored_torpedo::st_reloading
            || torpedoes[tb].status == submarine::stored_torpedo::st_unloading) {
            hours   = torpedoes[tb].remaining_time / 3600;
            minutes = (torpedoes[tb].remaining_time - floor(hours) * 3600) / 60;
            seconds = torpedoes[tb].remaining_time - floor(hours) * 3600 - floor(minutes) * 60;
            // fixme do that BEFORE rendering above!!!!
        }
        if (left_mouse_button_pressed) {
            // display remaining time if possible
            // torpedo reload remaning time
            if (torpedoes[tb].status == submarine::stored_torpedo::st_reloading
                || torpedoes[tb].status == submarine::stored_torpedo::st_unloading) {
                notepadsheet.get()->draw(mouse_position.x, mouse_position.y);
                font_vtremington12->print(
                    mouse_position.x + 32,
                    mouse_position.y + 50,
                    texts::get(211) + get_time_string(torpedoes[tb].remaining_time),
                    color(32, 0, 0));
            }
        }
    }

    // draw deck gun ammo remaining
    if (sub->has_deck_gun()) {
        font_vtremington12->print(400, 85, helper::str(sub->num_shells_remaining()), color(0, 0, 0));
    }

    SYS().unprepare_2d_drawing();
}

auto sub_torpedo_display::handle_mouse_button_event(const mouse_click_data& m) -> bool
{
    // fixme:
    // increase/decrease torp_desc_line when clicking on desc text area or using
    // mouse wheel
    auto& gm              = ui.get_game();
    auto* sub             = dynamic_cast<submarine*>(gm.get_player());
    const auto& torpedoes = sub->get_torpedoes();
    if (m.down() && m.left()) {
        torptranssrc = get_tube_below_mouse(get_tubecoords(sub));
        if (torptranssrc != ILLEGAL_TUBE) {
            if (torpedoes[torptranssrc].status != submarine::stored_torpedo::st_loaded) {
                torptranssrc = ILLEGAL_TUBE;
            }
        }
        left_mouse_button_pressed = true;
        return true;
    }
    if (m.up() && m.left()) {
        unsigned torptransdst = get_tube_below_mouse(get_tubecoords(sub));
        if (torptransdst != ILLEGAL_TUBE && torptranssrc != ILLEGAL_TUBE) {
            if (torpedoes[torptransdst].status == submarine::stored_torpedo::st_empty) {
                sub->transfer_torpedo(torptranssrc, torptransdst);
            }
        }
        torptranssrc              = ILLEGAL_TUBE;
        left_mouse_button_pressed = false;
        return true;
    }
    return false;
}

auto sub_torpedo_display::handle_mouse_motion_event(const mouse_motion_data& m) -> bool
{
    mouse_position            = m.position_2d;
    left_mouse_button_pressed = m.left();
    return false;
}

auto sub_torpedo_display::handle_mouse_wheel_event(const mouse_wheel_data& m) -> bool
{
    if (m.up()) {
        if (torp_desc_line > 0) {
            --torp_desc_line;
        }
        return true;
    }
    if (m.down()) {
        ++torp_desc_line;
        return true;
    }
    return false;
}
