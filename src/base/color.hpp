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

// a color
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#pragma once

#include "vector4.hpp"

#include <ostream>

///\brief Color representation with some basic transformations and OpenGL usage.
struct color
{
    uint8_t r, g, b, a;
    color(uint8_t r_ = 0, uint8_t g_ = 0, uint8_t b_ = 0, uint8_t a_ = 255)
        : r(r_)
        , g(g_)
        , b(b_)
        , a(a_){};
    color(const color& c1, const color& c2, float scal)
    {
        r = (uint8_t) (c1.r * (1 - scal) + c2.r * scal);
        g = (uint8_t) (c1.g * (1 - scal) + c2.g * scal);
        b = (uint8_t) (c1.b * (1 - scal) + c2.b * scal);
        a = (uint8_t) (c1.a * (1 - scal) + c2.a * scal);
    }
    color(const struct colorf& c);

    color operator*(const color& c) const
    {
        return {
            (uint8_t) (unsigned(r) * unsigned(c.r) / 255),
            (uint8_t) (unsigned(g) * unsigned(c.g) / 255),
            (uint8_t) (unsigned(b) * unsigned(c.b) / 255),
            (uint8_t) (unsigned(a) * unsigned(c.a) / 255)};
    }

    void store_rgb(uint8_t* ptr) const
    {
        ptr[0] = r;
        ptr[1] = g;
        ptr[2] = b;
    }
    void store_rgba(uint8_t* ptr) const
    {
        ptr[0] = r;
        ptr[1] = g;
        ptr[2] = b;
        ptr[3] = a;
    }
    void store_rgb(float* ptr) const
    {
        ptr[0] = float(r) / 255;
        ptr[1] = float(g) / 255;
        ptr[2] = float(b) / 255;
    }
    void store_rgba(float* ptr) const
    {
        ptr[0] = float(r) / 255;
        ptr[1] = float(g) / 255;
        ptr[2] = float(b) / 255;
        ptr[3] = float(a) / 255;
    }

    // transform color to grey value (model of human vision, 29.9% to 58.7%
    // to 11.4% RGB)
    [[nodiscard]] float brightness() const { return (r * 0.299 + g * 0.587 + b * 0.114) / 255; }
    [[nodiscard]] color grey_value() const
    {
        auto c = (uint8_t) (r * 0.299 + g * 0.587 + b * 0.114);
        return {c, c, c, a};
    }

    // color(istream& in) { r = read_u8(in); g = read_u8(in); b = read_u8(in); a
    // = read_u8(in); } void save(ostream& out) const { write_u8(out, r);
    // write_u8(out, g); write_u8(out, b); write_u8(out, a); }

    [[nodiscard]] color more_contrast(unsigned fac) const
    {
        int rr = (int(r) - 128) * fac + 128;
        int gg = (int(g) - 128) * fac + 128;
        int bb = (int(b) - 128) * fac + 128;
        return {
            uint8_t(
                rr > 255 ? 255
                : rr < 0 ? 0
                         : rr),
            uint8_t(
                gg > 255 ? 255
                : gg < 0 ? 0
                         : gg),
            uint8_t(
                bb > 255 ? 255
                : bb < 0 ? 0
                         : bb)};
    }

    [[nodiscard]] vector4f vec4() const
    {
        // *1/255
        return {r * 0.003921569f, g * 0.003921569f, b * 0.003921569f, a * 0.003921569f};
    }

    [[nodiscard]] vector3f vec3() const
    {
        // *1/255
        return {r * 0.003921569f, g * 0.003921569f, b * 0.003921569f};
    }

    [[nodiscard]] bool operator==(const color& other) const
    {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }
    [[nodiscard]] bool operator!=(const color& other) const { return !(*this == other); }

    // some useful standard colors
    static color black() { return {0, 0, 0}; }
    static color blue() { return {0, 0, 255}; }
    static color green() { return {0, 255, 0}; }
    static color red() { return {255, 0, 0}; }
    static color magenta() { return {255, 0, 255}; }
    static color cyan() { return {0, 255, 255}; }
    static color yellow() { return {255, 255, 0}; }
    static color orange() { return {255, 128, 0}; }
    static color lightgrey() { return {192, 192, 192}; }
    static color grey() { return {128, 128, 128}; }
    static color darkgrey() { return {64, 64, 64}; }
    static color white() { return {255, 255, 255}; }
};

///\brief Color representation with some basic transformations and OpenGL usage.
/// Float values!
struct colorf
{
    float r, g, b, a;
    colorf(float r_ = 0, float g_ = 0, float b_ = 0, float a_ = 1.0f)
        : r(r_)
        , g(g_)
        , b(b_)
        , a(a_)
    {
    }
    /// Allow initialization with double values
    // colorf(double r_, double g_, double b_, double a_ = 1.0) : r(float(r_)),
    // g(float(g_)), b(float(b_)), a(float(a_)) {}
    colorf(const color& c)
        : r(c.r * 0.003921569f)
        , g(c.g * 0.003921569f)
        , b(c.b * 0.003921569f)
        , a(c.a * 0.003921569f)
    {
    } // 0.003921569 = 1/255
    colorf(const colorf& c1, const colorf& c2, float scal)
    {
        r = (c1.r * (1 - scal) + c2.r * scal);
        g = (c1.g * (1 - scal) + c2.g * scal);
        b = (c1.b * (1 - scal) + c2.b * scal);
        a = (c1.a * (1 - scal) + c2.a * scal);
    }

    static colorf transparency(float t) { return colorf(1.f, 1.f, 1.f, t); }

    colorf operator*(const colorf& c) const { return {r * c.r, g * c.g, b * c.b, a * c.a}; }

    colorf operator*(float f) const { return {r * f, g * f, b * f, a * f}; }

    /// component wise linear interpolation
    [[nodiscard]] colorf lerp(const colorf& c1, const colorf& c2) const
    {
        return {
            c1.r * (1 - r) + c2.r * r, c1.g * (1 - g) + c2.g * g, c1.b * (1 - b) + c2.b * b, c1.a * (1 - a) + c2.a * a};
    }

    [[nodiscard]] vector3f vec3() const { return {r, g, b}; }

    [[nodiscard]] vector4f vec4() const { return {r, g, b, a}; }

    void store_rgb(uint8_t* ptr) const
    {
        ptr[0] = uint8_t(r * 255);
        ptr[1] = uint8_t(g * 255);
        ptr[2] = uint8_t(b * 255);
    }
    void store_rgba(uint8_t* ptr) const
    {
        ptr[0] = uint8_t(r * 255);
        ptr[1] = uint8_t(g * 255);
        ptr[2] = uint8_t(b * 255);
        ptr[3] = uint8_t(a * 255);
    }
    void store_rgb(float* ptr) const
    {
        ptr[0] = r;
        ptr[1] = g;
        ptr[2] = b;
    }
    void store_rgba(float* ptr) const
    {
        ptr[0] = r;
        ptr[1] = g;
        ptr[2] = b;
        ptr[3] = a;
    }

    // transform color to grey value (model of human vision, 29.9% to 58.7%
    // to 11.4% RGB)
    [[nodiscard]] float brightness() const { return (r * 0.299 + g * 0.587 + b * 0.114); }
    [[nodiscard]] colorf grey_value() const
    {
        float c = (r * 0.299 + g * 0.587 + b * 0.114);
        return {c, c, c, a};
    }
};

inline color::color(const colorf& c)
    : r(uint8_t(c.r * 255))
    , g(uint8_t(c.g * 255))
    , b(uint8_t(c.b * 255))
    , a(uint8_t(c.a * 255))
{
}

inline std::ostream& operator<<(std::ostream& os, const color& c)
{
    os << "R=" << unsigned(c.r) << ", G=" << unsigned(c.g) << ", B=" << unsigned(c.b) << ", A=" << unsigned(c.a) << ".";
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const colorf& c)
{
    os << "R=" << c.r << ", G=" << c.g << ", B=" << c.b << ", A=" << c.a << ".";
    return os;
}
