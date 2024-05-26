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

// Base interface for user screens.
// subsim (C)+(W) Markus Petermann and Thorsten Jordan. SEE LICENSE

#include "user_display.hpp"

#include "system_interface.hpp"
#include "user_interface.hpp"
#include "xml.hpp"

user_display::user_display(class user_interface& ui_, const char* display_name)
    : ui(ui_)
{
    // if displays use no elements they give nullptr as definition
    if (display_name != nullptr) {
        auto display_dir = get_display_dir() + display_name + "/";
        xml_doc display_config(display_dir + "layout.xml");
        display_config.load();
        const auto display_node = display_config.child("dftd-display");
        std::string prefix_day;
        std::string prefix_night;
        if (display_node.has_child("day")) {
            prefix_day = display_node.child("day").child_text();
        }
        if (display_node.has_child("night")) {
            prefix_night = display_node.child("night").child_text();
        }
        for (auto elem : display_node.iterate("element")) {
            elements.emplace_back(elem, display_dir, prefix_day, prefix_night);
            const auto id = elements.back().get_id();
            if (id != -1) {
                id_to_element[id] = unsigned(elements.size() - 1);
            }
        }
    }
}

user_display::elem2D::elem2D(
    const xml_elem& elem,
    const std::string& display_dir,
    const std::string& prefix_day,
    const std::string& prefix_night)
{
    position = elem.attrv2i();
    if (elem.has_attr("id")) {
        id = elem.attri("id");
    }
    std::string filename_day;
    std::string filename_night;
    bool has_file{true};
    if (elem.has_child("file")) {
        filename_day = prefix_day + elem.child("file").child_text();
        if (!prefix_night.empty()) {
            filename_night = prefix_night + elem.child("file").child_text();
            has_night      = true;
        }
    } else if (elem.has_child("day")) {
        filename_day = elem.child("day").child_text();
        if (elem.has_child("night")) {
            filename_night = elem.child("night").child_text();
            has_night      = true;
        }
    } else if (elem.has_child("end")) {
        has_file = false;
        size     = elem.child("end").attrv2i() - position;
    } else {
        THROW(xml_error, "invalid display def xml file, day or file or end node missing", elem.doc_name());
    }
    if (elem.has_attr("visible")) {
        const auto visible_flag = elem.attri("visible");
        if (visible_flag >= 0) {
            optional = true;
            visible  = visible_flag > 0;
        } else {
            optional = false;
            visible  = false;
        }
    }
    // Without scale tag we assume that we can rotate freely 360 degrees and the
    // value is the angle (0...360)
    if (elem.has_child("scale")) {
        // If a scale tag is given the value is limited and scaled
        auto elem_scale = elem.child("scale");
        auto elem_start = elem_scale.child("start");
        auto elem_end   = elem_scale.child("end");
        rotation_offset = -angle(elem_scale.attrf("pointer"));
        if (elem_scale.has_attr("clockwise")) {
            clockwise = elem_scale.attrb("clockwise");
        }
        start_angle = angle(elem_start.attrf("angle"));
        start_value = elem_start.attrf("value");
        end_angle   = angle(elem_end.attrf("angle"));
        end_value   = elem_end.attrf("value");
    }
    if (elem.has_child("center")) {
        auto elem_center = elem.child("center");
        center           = elem_center.attrv2i();
        rotateable       = true;
    } else if (elem.has_child("size")) {
        auto elem_size = elem.child("size");
        center         = position + elem_size.attrv2i() / 2;
        rotateable     = true;
    } else if (elem.has_child("slider")) {
        // one of the coordinates for draw can be variable (take it from value)
        const auto elem_slider = elem.child("slider");
        slide_x                = elem_slider.attri("x"); // for now only X sliding possible
        start_value            = 0.0;
        end_value              = 1.0;
        can_slide              = true;
        if (elem_slider.has_attr("start")) {
            start_value = elem_slider.attrf("start");
        }
        if (elem_slider.has_attr("end")) {
            end_value = elem_slider.attrf("end");
        }
    }
    if (rotateable) {
        // Compute radius wher we can click on
        const auto delta = position - center;
        click_radius     = std::max(std::abs(delta.x), std::abs(delta.y));
    }
    if (rotateable || !elem.has_child("phases")) {
        if (has_file) {
            filenames_day.push_back(display_dir + filename_day);
            filenames_night.push_back(display_dir + filename_night);
            tex.resize(1);
        }
    } else {
        auto elem_phases = elem.child("phases");
        std::vector<std::string> phase_names;
        if (elem_phases.has_child("phase")) {
            // phases defined by own names
            for (auto elem_phase : elem_phases.iterate("phase")) {
                phase_names.push_back(elem_phase.child_text());
            }
        } else {
            auto phases = elem_phases.attru("nr");
            auto offset = elem_phases.attru("offset");
            for (unsigned i = 0; i < phases; ++i) {
                phase_names.push_back(helper::str(offset + i));
            }
        }
        if (elem_phases.has_attr("by_angle")) {
            end_angle       = elem.attrf("by_angle");
            start_angle     = 0.0;
            start_value     = 0.0;
            end_value       = double(phase_names.size());
            rotation_offset = 0.0;
            phase_by_value  = true;
        }
        filenames_day.resize(phase_names.size());
        filenames_night.resize(phase_names.size());
        tex.resize(phase_names.size());
        for (unsigned i = 0; i < unsigned(phase_names.size()); ++i) {
            filenames_day[i] = display_dir + helper::replace_first(filename_day, "%u", phase_names[i]);
            if (has_night) {
                filenames_night[i] = display_dir + helper::replace_first(filename_night, "%u", phase_names[i]);
            }
        }
    }
}

void user_display::elem2D::set_phase(unsigned phase_) const
{
    phase = std::min(nr_of_phases() - 1, phase_);
}

void user_display::elem2D::draw() const
{
    if (!tex.empty() && visible) {
        if (rotateable) {
            // rotation around pixel center (offset +0.5) could be sensible but
            // it looks correct that way.
            const auto display_angle =
                rotation_offset + start_angle + (get_angle_range() * (value - start_value) / (end_value - start_value));
            tex[phase]->draw_rot(
                center.x,
                center.y,
                clockwise ? display_angle.value() : -display_angle.value(),
                center.x - position.x,
                center.y - position.y);
        } else {
            const auto pos_x =
                can_slide ? int(std::floor(
                    0.5
                    + helper::interpolate(
                        double(position.x), double(slide_x), (value - start_value) / (end_value - start_value))))
                          : position.x;
            tex[phase]->draw(pos_x, position.y);
        }
    }
}

auto user_display::elem2D::is_mouse_over(const vector2i& pos, int tolerance) const -> bool
{
    if (click_radius > 0) {
        return (
            pos.x + tolerance >= center.x - click_radius && pos.y + tolerance >= center.y - click_radius
            && pos.x - tolerance < center.x + click_radius + int(tex[phase]->get_width())
            && pos.y - tolerance < center.y + click_radius + int(tex[phase]->get_height()));
    }
    if (tex.empty()) {
        return (
            pos.x + tolerance >= position.x && pos.y + tolerance >= position.y
            && pos.x - tolerance < position.x + size.x && pos.y - tolerance < position.y + size.y);
    }
    return (
        pos.x + tolerance >= position.x && pos.y + tolerance >= position.y
        && pos.x - tolerance < position.x + int(tex[phase]->get_width())
        && pos.y - tolerance < position.y + int(tex[phase]->get_height()));
}

void user_display::elem2D::init(bool is_day)
{
    // init all textures
    for (auto i = 0U; i < nr_of_phases(); ++i) {
        tex[i] = std::make_unique<texture>(
            (is_day || !has_night || filenames_night[i].empty()) ? filenames_day[i] : filenames_night[i],
            texture::LINEAR);
    }
    // Determine size from image if there is one (only use first phase for size)
    if (!tex.empty()) {
        size = {int(tex[0]->get_width()), int(tex[0]->get_height())};
    }
}

void user_display::elem2D::deinit()
{
    // delete all textures
    for (auto i = 0U; i < nr_of_phases(); ++i) {
        tex[i] = nullptr;
    }
}

void user_display::elem2D::set_value(double v) const
{
    value = std::clamp(v, std::min(start_value, end_value), std::max(start_value, end_value));
    if (phase_by_value) {
        // we have phases, set phase by value if desired
        phase = std::min(nr_of_phases() - 1, unsigned(std::floor(value)));
    }
}

void user_display::elem2D::set_angle(angle a) const
{
    set_value(helper::interpolate(start_value, end_value, (a - start_angle).value() / get_angle_range()));
}

auto user_display::elem2D::set_value(const vector2i& pos) const -> std::optional<double>
{
    // need to negate y, because onscreen y is down
    const auto a = angle(vector2(pos.x - center.x, center.y - pos.y)) - start_angle;
    if (a.value() > get_angle_range()) {
        return std::nullopt;
    }
    value = helper::interpolate(start_value, end_value, std::min(1.0, a.value() / get_angle_range()));
    return value;
}

auto user_display::elem2D::set_value_uint(const vector2i& pos) const -> std::optional<unsigned>
{
    const auto v = set_value(pos);
    if (!v.has_value() || v.value() < 0.0 || v.value() >= end_value) {
        return std::nullopt;
    }
    value = v.value();
    return unsigned(std::floor(v.value()));
}

void user_display::elem2D::set_filename(const std::string& fn, bool day, unsigned phase)
{
    if (phase < nr_of_phases()) {
        (day ? filenames_day : filenames_night)[phase] = fn;
    }
}

auto user_display::elem2D::get_angle_range() const -> double
{
    auto range = end_angle - start_angle;
    // if start and end angle match use full range
    if (range.value() < 1.0) {
        return 360.0;
    }
    return range.value();
}

auto user_display::element_for_id(unsigned id) const -> const user_display::elem2D&
{
    auto it = id_to_element.find(id);
    if (it == id_to_element.end()) {
        THROW(error, "invalid display definition, id not found");
    }
    return elements[it->second];
}

auto user_display::element_for_id(unsigned id) -> user_display::elem2D&
{
    auto it = id_to_element.find(id);
    if (it == id_to_element.end()) {
        THROW(error, "invalid display definition, id not found");
    }
    return elements[it->second];
}

void user_display::draw_elements(bool with_info_panel) const
{
    SYS().prepare_2d_drawing();
    for (const auto& e : elements) {
        e.draw();
    }
    if (with_info_panel) {
        ui.draw_infopanel();
    }
    SYS().unprepare_2d_drawing();
}

void user_display::enter(bool is_day)
{
    // default: call init on all elements
    for (auto& e : elements) {
        e.init(is_day);
    }
}

void user_display::leave()
{
    // default: call deinit on all elements
    for (auto& e : elements) {
        e.deinit();
    }
}
