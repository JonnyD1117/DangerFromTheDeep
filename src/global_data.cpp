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

// global data
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#include "global_data.hpp"

#include "constant.hpp"
#include "datadirs.hpp"
#include "font.hpp"
#include "helper.hpp"
#include "image.hpp"
#include "log.hpp"
#include "system_interface.hpp"
#include "texture.hpp"

#include <SDL_image.h>
#include <iomanip>
#include <list>
#include <sstream>
#include <stdexcept>

// same with version string
auto get_program_version() -> std::string
{
    return std::string(VERSION);
}

global_data::global_data()
    : image_store(get_image_dir())
    , texture_store(get_texture_dir())
{
    font_arial         = std::make_unique<font>(get_font_dir() + "font_arial");
    font_jphsl         = std::make_unique<font>(get_font_dir() + "font_jphsl");
    font_vtremington10 = std::make_unique<font>(get_font_dir() + "font_vtremington10");
    font_vtremington12 = std::make_unique<font>(get_font_dir() + "font_vtremington12");
    font_typenr16      = std::make_unique<font>(get_font_dir() + "font_typenr16");
}

global_data::~global_data()
{
    font_arial         = nullptr;
    font_jphsl         = nullptr;
    font_vtremington10 = nullptr;
    font_vtremington12 = nullptr;
    font_typenr16      = nullptr;
}

std::unique_ptr<class font> font_arial, font_jphsl, font_vtremington10, font_vtremington12, font_typenr16;

// display loading progress
std::list<std::string> loading_screen_messages;
unsigned starttime;

void display_loading_screen()
{
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    SYS().prepare_2d_drawing();

    // display a nice loading image in the background
    auto background = image_store().ref("entryscreen.png");
    background->draw(0, 0);
    unsigned fh = font_arial->get_height();
    unsigned y  = 0;

    for (auto& loading_screen_message : loading_screen_messages) {
        font_arial->print(0, y, loading_screen_message);
        y += fh;
    }
    SYS().unprepare_2d_drawing();
    SYS().finish_frame();
}

void reset_loading_screen()
{
    loading_screen_messages.clear();
    loading_screen_messages.emplace_back("Loading...");
    log_info("Loading...");
    display_loading_screen();
    starttime = SYS().millisec();
}

void add_loading_screen(const std::string& msg)
{
    unsigned tm        = SYS().millisec();
    unsigned deltatime = tm - starttime;
    starttime          = tm;
    std::ostringstream oss;
    oss << msg << " (" << deltatime << "ms)";
    loading_screen_messages.push_back(oss.str());
    log_info(oss.str());
    display_loading_screen();
}

auto get_time_string(double tm) -> std::string
{
    auto seconds     = unsigned(floor(helper::mod(tm, 86400.0)));
    unsigned hours   = seconds / 3600;
    unsigned minutes = (seconds % 3600) / 60;
    seconds          = seconds % 60;
    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << hours << ":" << std::setw(2) << std::setfill('0') << minutes << ":"
        << std::setw(2) << std::setfill('0') << seconds;
    return oss.str();
}

static constexpr double CA = 0.0003;
void jacobi_amp(double u, double k, double& sn, double& cn)
{
    double emc = 1.0 - k * k;

    double a;
    double b;
    double c;
    double d = 0.0;
    double dn;
    double em[14];
    double en[14];
    int i;
    int ii;
    int l;
    int bo;

    if (emc != 0.0) {
        bo = static_cast<int>(emc < 0.0);
        if (bo != 0) {
            d = 1.0 - emc;
            emc /= -1.0 / d;
            u *= (d = sqrt(d));
        }
        a  = 1.0;
        dn = 1.0;

        for (i = 1; i <= 13; i++) {
            l     = i;
            em[i] = a;
            en[i] = (emc = sqrt(emc));
            c     = 0.5 * (a + emc);
            if (fabs(a - emc) <= CA * a) {
                break;
            }
            emc *= a;
            a = c;
        }

        u *= c;
        sn = sin(u);
        cn = cos(u);

        if (sn != 0.0) {
            a = (cn) / (sn);
            c *= a;

            for (ii = l; ii >= 1; ii--) {
                b = em[ii];
                a *= c;
                c *= (dn);
                dn = (en[ii] + a) / (b + a);
                a  = c / b;
            }

            a  = 1.0 / sqrt(c * c + 1.0);
            sn = (sn >= 0.0 ? a : -a);
            cn = c * sn;
        }
        if (bo != 0) {
            cn = dn;
            sn /= d;
        }
    } else {
        cn = 1.0 / cosh(u);
        sn = tanh(u);
    }
}
#undef CA

auto transform_real_to_geo(const vector2f& pos) -> vector2f
{
    double sn;
    double cn;
    double r;
    vector2f coord;

    jacobi_amp(pos.y / constant::WGS84_A, constant::WGS84_K, sn, cn);

    r = sqrt((constant::WGS84_B * constant::WGS84_B) / (1.0 - constant::WGS84_K * constant::WGS84_K * cn * cn));

    coord.x = (180.0 * pos.x) / (M_PI * r);
    coord.y = (asin(sn) * 180.0) / M_PI;

    return coord;
}

static auto transform_nautic_coord_to_real(const std::string& s, char minus, char plus, int degmax) -> double
{
    if (s.length() < 2) {
        THROW(error, std::string("nautic coordinate invalid ") + s);
    }
    char sign = s[s.length() - 1];
    if (sign != minus && sign != plus) {
        THROW(error, std::string("nautic coordinate (direction sign) invalid ") + s);
    }
    // find separator
    std::string::size_type st = s.find('/');
    if (st == std::string::npos) {
        THROW(error, std::string("no separator in position string ") + s);
    }
    std::string degrees = s.substr(0, st);
    std::string minutes = s.substr(st + 1, s.length() - st - 2);
    int deg             = atoi(degrees.c_str());
    if (deg < 0 || deg > degmax) {
        THROW(error, std::string("degrees are not in range [0...180/360] in position string ") + s);
    }
    int mts = atoi(minutes.c_str());
    if (mts < 0 || mts > 59) {
        THROW(error, std::string("minutes are not in [0...59] in position string ") + s);
    }
    return (sign == minus ? -1 : 1) * ((constant::DEGREE_IN_METERS * deg) + (constant::MINUTE_IN_METERS * mts));
}

auto transform_nautic_posx_to_real(const std::string& s) -> double
{
    return transform_nautic_coord_to_real(s, 'W', 'E', 180);
}

auto transform_nautic_posy_to_real(const std::string& s) -> double
{
    return transform_nautic_coord_to_real(s, 'S', 'N', 90);
}
