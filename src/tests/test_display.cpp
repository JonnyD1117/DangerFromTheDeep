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

// test program for displays
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#include "mymain.cpp"
#include "system_interface.hpp"
#include "user_display.hpp"
#include "user_interface.hpp"

class test_display : public user_display
{
  public:
    test_display(user_interface& ui_, const char* name)
        : user_display(ui_, name)
    {
    }
    void set_time(double t)
    {
        // show value range every N seconds
        const auto secs_range     = 5.0;
        const auto secs_start_end = 1.0;
        const auto secs_mod       = secs_range + secs_start_end * 2;
        const auto value_factor   = std::clamp((helper::mod(t, secs_mod) - secs_start_end) / secs_range, 0.0, 1.0);
        const auto secs_visible   = 3.0;
        const bool visible        = helper::mod(t, secs_visible * 2) >= secs_visible;
        for (auto& elem : elements) {
            // switch also phases, visibility etc.
            // show start value for 1 sec, then range over values, then end
            // value for 1 sec
            elem.set_phase(unsigned(std::floor(helper::mod(t, double(elem.nr_of_phases())))));
            const auto vr = elem.get_value_range();
            elem.set_value(helper::interpolate(vr.first, vr.second, value_factor));
            elem.set_visible(visible);
        }
    }
    void check_mouse(const vector2i& mpos)
    {
        for (auto& elem : elements) {
            if (elem.is_mouse_over(mpos)) {
                // fixme
                elem.set_value(mpos);
            }
        }
    }
    void display() const override { user_display::draw_elements(false /* no infopanel */); }
};

int mymain(std::vector<std::string>& args)
{
    if (args.size() < 1 || args.size() > 2) {
        return -1;
    }
    bool is_day = true;
    if (args.size() == 2) {
        if (args[1] == "day") {
            is_day = true;
        }
        if (args[1] == "night") {
            is_day = false;
        }
    }

    system_interface::parameters params;
    params.near_z       = 1.0;
    params.far_z        = 1000.0;
    params.resolution   = {1024, 768};
    params.resolution2d = {1024, 768};
    params.fullscreen   = false;
    system_interface::create_instance(params);

    user_interface& ui = *(user_interface*) nullptr; // fixme we need a stub here that delivers
                                                     // valid data! at least for all calls
    test_display td(ui, args[0].c_str());
    td.enter(is_day);

    // allow change of every element by mouse click, variate all values by time,
    // until ESC pressed

    bool doquit = false;
    auto ic     = std::make_shared<input_event_handler_custom>();
    ic->set_handler([&](const input_event_handler::key_data& k) {
        if (k.down()) {
            switch (k.keycode) {
                case key_code::ESCAPE:
                    doquit = true;
                    break;
                default:
                    return false;
                    break;
            }
        }
        return true;
    });
    ic->set_handler([&](const input_event_handler::mouse_click_data& m) {
        if (m.down() && m.left()) {
            // set angle/value by click
            td.check_mouse(m.position_2d);
        }
        return true;
    });
    ic->set_handler([&](const input_event_handler::mouse_motion_data& m) {
        if (m.left()) {
            // set angle/value by dragging
            td.check_mouse(m.position_2d);
        }
        return true;
    });
    SYS().add_input_event_handler(ic);

    while (!doquit) {
        td.set_time(SYS().millisec() / 1000.0);
        td.display();
        doquit = SYS().finish_frame();
    }

    return 0;
}
