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

// Base interface for user screen popups.
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#include "user_popup.hpp"

#include "datadirs.hpp"
#include "game.hpp"
#include "system_interface.hpp"
#include "user_interface.hpp"
#include "xml.hpp"

user_popup::elem2D::elem2D(vector2i pos, const std::string& filename_day, const std::string& filename_night)
    : position(pos)
{
    tex_day = std::make_unique<image>(filename_day);
    if (!filename_night.empty()) {
        tex_night = std::make_unique<image>(filename_night);
    }
}

void user_popup::elem2D::draw(bool is_day) const
{
    if (is_day || tex_night == nullptr) {
        tex_day->draw(position.x, position.y);
    } else {
        tex_night->draw(position.x, position.y);
    }
}

user_popup::user_popup(user_interface& ui_, const char* popup_name)
    : ui(ui_)
{
    // if popups use no elements they give nullptr as definition
    if (popup_name != nullptr) {
        auto popup_dir = get_popup_dir() + popup_name + "/";
        xml_doc popup_config(popup_dir + "layout.xml");
        popup_config.load();
        for (auto elem : popup_config.child("dftd-popup").iterate("element")) {
            auto pos = elem.attrv2i();
            // IDs are not used yet
            std::string filename_day;
            if (!elem.has_child("day")) {
                THROW(error, std::string(popup_name) + ", invalid popup def xml file, day node missing");
            }
            filename_day = elem.child("day").child_text();
            std::string filename_night;
            if (elem.has_child("night")) {
                filename_night = elem.child("night").child_text();
            }
            elements.emplace_back(pos, popup_dir + filename_day, popup_dir + filename_night);
        }
    }
}

void user_popup::display() const
{
    SYS().prepare_2d_drawing();
    bool is_day = ui.get_game().is_day_mode();
    for (const auto& elem : elements) {
        elem.draw(is_day);
    }
    SYS().unprepare_2d_drawing();
}
