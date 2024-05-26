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

// logbook display
// subsim (C)+(W) Markus Petermann and Thorsten Jordan. SEE LICENSE

#include "logbook_display.hpp"

#include "angle.hpp"
#include "date.hpp"
#include "font.hpp"
#include "game.hpp"
#include "global_data.hpp"
#include "image.hpp"
#include "system_interface.hpp"
#include "texts.hpp"
#include "texture.hpp"
#include "user_display.hpp"
#include "user_interface.hpp"
#include "vector2.hpp"

#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <utility>

logbook_display::logbook_display(class user_interface& ui_)
    : user_display(ui_, "logbook")
    , page_left_offset(76, 118)
    , page_right_offset(554, 118)
    , page_size(400, 500)
{
}

void logbook_display::previous_page()
{
    if (current_page > 0) {
        current_page -= 2;
    }
}

void logbook_display::next_page()
{
    if (current_page + 2 < nr_of_pages) {
        current_page += 2;
    }
}

void logbook_display::display() const
{
    auto& myfont = *font_jphsl;
    // compute size of entries (number of lines for each entry)
    const logbook& lb = ui.get_game().get_players_logbook();
    std::vector<unsigned> lines_per_entry;
    unsigned total_lines = 0;
    lines_per_entry.reserve(lb.size());
    for (const auto& it : lb) {
        unsigned l = myfont.get_nr_of_lines_wrapped(page_size.x, it).first;
        lines_per_entry.push_back(l);
        total_lines += l;
    }

    unsigned lines_per_page = page_size.y / myfont.get_height();
    nr_of_pages             = (total_lines + lines_per_page - 1) / lines_per_page;

    // now compute distribution of entries over pages
    // for each entry compute starting page and line
    std::vector<std::pair<unsigned, unsigned>> entry_page_and_line;
    entry_page_and_line.reserve(lb.size());
    unsigned cur_page        = 0;
    unsigned cur_line        = 0;
    int first_entry_cp_left  = -1;
    int last_entry_cp_left   = -1;
    int first_entry_cp_right = -1;
    int last_entry_cp_right  = -1;
    for (unsigned i = 0; i < lines_per_entry.size(); ++i) {
        if (cur_page == current_page) {
            if (first_entry_cp_left < 0) {
                first_entry_cp_left = int(i);
            }
            last_entry_cp_left = int(i) + 1;
        }
        if (cur_page == current_page + 1) {
            if (first_entry_cp_right < 0) {
                first_entry_cp_right = int(i);
            }
            last_entry_cp_right = int(i) + 1;
        }
        entry_page_and_line.emplace_back(cur_page, cur_line);
        cur_line += lines_per_entry[i];
        if (cur_line >= lines_per_page) {
            cur_line -= lines_per_page;
            ++cur_page;
        }
    }

    // check range of entries to render for currently displayed page
    // render [first_entry_cp_left/right...last_entry_cp_left/right[
    // for current page... note: older entries may wrap, argh!

    draw_elements();

    SYS().prepare_2d_drawing();

    // print rest of part from previous double page, if available
    if ((first_entry_cp_left > 0 && entry_page_and_line[first_entry_cp_left].second > 0)
        || (lb.size() > 0 && first_entry_cp_left < 0)) {
        // there is one previous entry and this entry does not start on line 0
        // compute text pointer where to begin print
        unsigned i;
        if (first_entry_cp_left < 0) {
            // no entry matched current_page, so we must be on the last page,
            // and only the rest of the last entry is visible
            i = unsigned(lb.size() - 1);
        } else {
            i = unsigned(first_entry_cp_left - 1);
        }
        unsigned maxlines     = lines_per_page - entry_page_and_line[i].second;
        const std::string& et = *(lb.get_entry(i));
        unsigned textoff      = myfont.get_nr_of_lines_wrapped(page_size.x, et, maxlines).second;
        myfont.print_wrapped(
            page_left_offset.x, page_left_offset.y, page_size.x, 0, et.substr(textoff), color(10, 10, 10), false, 0);
    }

    for (int i = first_entry_cp_left; i < last_entry_cp_left; ++i) {
        const std::string& et = *(lb.get_entry(i));
        unsigned maxh         = (lines_per_page - entry_page_and_line[i].second) * myfont.get_height();
        unsigned textptr      = myfont.print_wrapped(
            page_left_offset.x,
            page_left_offset.y + entry_page_and_line[i].second * myfont.get_height(),
            page_size.x,
            0,
            et,
            color(10, 10, 10),
            false,
            maxh);
        if (textptr < et.length()) {
            // print rest of it on second page
            myfont.print_wrapped(
                page_right_offset.x,
                page_right_offset.y,
                page_size.x,
                0,
                et.substr(textptr),
                color(10, 10, 10),
                false,
                0);
        }
    }
    for (int i = first_entry_cp_right; i < last_entry_cp_right; ++i) {
        const std::string& et = *(lb.get_entry(i));
        unsigned maxh         = (lines_per_page - entry_page_and_line[i].second) * myfont.get_height();
        myfont.print_wrapped(
            page_right_offset.x,
            page_right_offset.y + entry_page_and_line[i].second * myfont.get_height(),
            page_size.x,
            0,
            et,
            color(10, 10, 10),
            false,
            maxh);
    }

    // Display page number.
    std::ostringstream oss1;
    oss1 << current_page + 1;
    myfont.print(260, 635, oss1.str(), color(10, 10, 10));
    std::ostringstream oss2;
    oss2 << current_page + 2;
    myfont.print(760, 635, oss2.str(), color(10, 10, 10));

    // Display arrows.
    myfont.print(160, 635, "<<", color(10, 10, 10));
    myfont.print(860, 635, ">>", color(10, 10, 10));

    // ui.draw_infopanel(); // already done in draw_elements

    SYS().unprepare_2d_drawing();
}

auto logbook_display::handle_key_event(const key_data& k) -> bool
{
    if (k.down() && k.keycode == key_code::LESS) {
        if (key_mod_shift(k.mod)) {
            next_page();
        } else {
            previous_page();
        }
        return true;
    }
    return false;
}

auto logbook_display::handle_mouse_button_event(const mouse_click_data& m) -> bool
{
    if (m.down()) {
        if (m.position_2d.x < 530) {
            previous_page();
        } else {
            next_page();
        }
        return true;
    }
    return false;
}
