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

#pragma once

#include "object_store.hpp"
#include "singleton.hpp"
#include "vector2.hpp"

#include <cmath>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

std::string get_program_version();

///> all global data grouped in one class
class global_data : public singleton<class global_data>
{
  private:
    // no copy
    global_data(const global_data&)            = delete;
    global_data& operator=(const global_data&) = delete;

  public:
    global_data();
    ~global_data();

    shared_object_store<class image> image_store;
    shared_object_store<class texture> texture_store;
};

inline auto& image_store()
{
    return global_data::instance().image_store;
}

inline auto& texture_store()
{
    return global_data::instance().texture_store;
}

// only used as shortcut, fonts are managed by class global_data
extern std::unique_ptr<class font> font_arial, font_jphsl, font_vtremington10, font_vtremington12, font_typenr16;

// display loading progress
void reset_loading_screen();
void add_loading_screen(const std::string& msg);

// transform time in seconds to 24h time of clock string (takes remainder of
// 86400 seconds first = 1 day)
std::string get_time_string(double tm);

inline float mysgn(float a)
{
    return (a < 0) ? -1.0f : ((a > 0) ? 1.0f : 0.0f);
}

inline double mysgn(double a)
{
    return (a < 0) ? -1.0 : ((a > 0) ? 1.0 : 0.0);
}

template<class T>
inline T myclamp(const T& v, const T& minv, const T& maxv)
{
    return std::min(maxv, std::max(minv, v));
}

// return a random value in [0, 1(
inline double rnd()
{
    return double(rand()) / RAND_MAX;
}

inline unsigned rnd(unsigned b)
{
    return unsigned(b * rnd());
}

inline unsigned ulog2(unsigned x)
{
    unsigned i = 0;
    for (; x > 0; ++i, x >>= 1) { }
    return i - 1;
}
// give degrees,minutes like this 123/45x with x = N,W,E,S
double transform_nautic_posx_to_real(const std::string& s);
double transform_nautic_posy_to_real(const std::string& s);

void jacobi_amp(double u, double k, double& sn, double& cn);
vector2f transform_real_to_geo(const vector2f& pos);

///> transform any data type to a string for easier error reporting etc.
template<typename T>
std::string str(const T& x)
{
    std::ostringstream oss;
    oss << x;
    return oss.str();
}

///> do the same with width and filler
template<typename T>
std::string str_wf(const T& x, unsigned width, char filler = '0')
{
    std::ostringstream oss;
    oss << std::setw(width) << std::setfill(filler) << x;
    return oss.str();
}
