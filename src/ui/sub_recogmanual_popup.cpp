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

#include "sub_recogmanual_popup.hpp"

#include "datadirs.hpp"
#include "global_data.hpp"
#include "system_interface.hpp"
#include "user_interface.hpp"
#include "xml.hpp"

sub_recogmanual_popup::widget_button_next::widget_button_next(
    int x,
    int y,
    int w,
    int h,
    int dir,
    int& att_page,
    const std::string& text_,
    const std::string& bg_image_)
    : widget_button(x, y, w, h, text_, bg_image_)
    , direction(dir)
    , page(att_page)
{
}

void sub_recogmanual_popup::widget_button_next::draw() const
{
    redrawme   = false;
    vector2i p = get_pos();
    int bw     = int(background->get_width());
    int bh     = int(background->get_height());

    colorf col(1.0, 1.0, 1.0, 1.0);
    if (mouseover != this) {
        col = colorf(1.0, 1.0, 1.0, 1.0);
    }
    background->draw(p.x + size.x / 2 - bw / 2, p.y + size.y / 2 - bh / 2, col);
}

void sub_recogmanual_popup::widget_button_next::on_release()
{
    pressed = false;
    page += direction;
}

sub_recogmanual_popup::sub_recogmanual_popup(user_interface& ui_)
    : user_popup(ui_, "sub_recogmanual")
    , page(0)
    , btn_left(15, 690, 11, 31, -1, page, "", "BG_btn_left.png")
    , btn_right(414, 690, 11, 31, 1, page, "", "BG_btn_right.png")
{

    auto ship_ids = data_file_handler::instance().get_ship_list();
    for (auto& ship_id : ship_ids) {
        try {
            // this eats video ram, better load them on demand - but popup can
            // be shown any time...
            silhouettes.push_back(
                std::make_unique<image>(data_file_handler::instance().get_path(ship_id) + ship_id + "_silhouette.png"));

            xml_doc doc(data_file_handler::instance().get_filename(ship_id));
            doc.load();
            xml_elem elem = doc.child("dftd-ship");
            elem          = elem.child("shipmanual");
            displacements.push_back(elem.attr("displacement"));
            lengths.push_back(elem.attr("length"));
            classes.push_back(elem.attr("class"));
            weapons.push_back(elem.attr("weapons"));
            countries.push_back(elem.attr("countries"));
        }
        catch (std::exception& e) { // fixme: remove the try..catch when all
                                    // silhouette files are on place
        }
    }
}

auto sub_recogmanual_popup::handle_mouse_button_event(const mouse_click_data& m) -> bool
{
    if (btn_left.is_mouse_over(m.position_2d)) {
        widget::handle_mouse_button_event(btn_left, m);
    } else if (btn_right.is_mouse_over(m.position_2d)) {
        widget::handle_mouse_button_event(btn_right, m);
    }
    if (page < 0) {
        page = 0;
    }
    if (page >= static_cast<int>(silhouettes.size()) / 3) {
        page--;
    }
    return false;
}

auto sub_recogmanual_popup::handle_mouse_motion_event(const mouse_motion_data& m) -> bool
{
    if (btn_left.is_mouse_over(m.position_2d)) {
        widget::handle_mouse_motion_event(btn_left, m);
    } else if (btn_right.is_mouse_over(m.position_2d)) {
        widget::handle_mouse_motion_event(btn_right, m);
    }
    if (page < 0) {
        page = 0;
    }
    if (page >= static_cast<int>(silhouettes.size()) / 3) {
        page--;
    }
    return false;
}

auto sub_recogmanual_popup::handle_mouse_wheel_event(const mouse_wheel_data& /*m*/) -> bool
{
    if (page < 0) {
        page = 0;
    }
    if (page >= static_cast<int>(silhouettes.size()) / 3) {
        page--;
    }
    return false;
}

void sub_recogmanual_popup::display() const
{
    // display background
    user_popup::display();

    SYS().prepare_2d_drawing();
    int off_x      = 15;
    int off_y      = 82;
    int off_text_x = 40;
    int off_text_y = 237;
    int step_y     = 199;

    for (int i = page * 3; (i < page * 3 + 3) && (i < static_cast<int>(silhouettes.size())); i++) {

        silhouettes[i]->draw(off_x, off_y + step_y * (i % 3), colorf(1, 1, 1, 0.75));

        // fixme: change this after the authentic overlay is implemented
        auto& myfont = *font_vtremington12;
        myfont.print(off_text_x, off_text_y + step_y * (i % 3), classes[i], color(0, 0, 0));
        myfont.print(
            off_text_x,
            off_text_y + 15 + step_y * (i % 3),
            std::string("Length: ") + lengths[i] + std::string("   Displacement:") + displacements[i],
            color(0, 0, 0));
        myfont.print(
            off_text_x, off_text_y + 30 + step_y * (i % 3), std::string("Countries: ") + countries[i], color(0, 0, 0));
        myfont.print(
            off_text_x, off_text_y + 45 + step_y * (i % 3), std::string("Weapons: ") + weapons[i], color(0, 0, 0));
    }

    btn_left.draw();
    btn_right.draw();

    SYS().unprepare_2d_drawing();
}
