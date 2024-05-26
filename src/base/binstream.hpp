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

// Binary Stream Tools
// (C) 2002 Thorsten Jordan

#pragma once

#include "quaternion.hpp"

#include <iostream>
#include <string>

// Data is stored in little endian mode.
// On big endian machines the data is converted for reading and writing.
// This is done with swap functions that are identical to their inverse
// versions.

// So far we assume sizeof(float)==4 and sizeof(double)==8 (IEEE standard!)
// The only system dependent assumption is that sizeof(int) >= 4 (32 bits).
// C(++) guarantees that short has at least 16 bits, long has at least 32,
// and int has some bit number between short and long.

// these unions are a way to handle C99 strict aliasing rules.
// float f = *(float*)&u;   is a bad idea with C99, instructions
// can be reordered that shouldn't because u is hidden.
union float_u32_shared
{
    float f;
    uint32_t u;
};

union double_u64_shared
{
    double d;
    uint64_t u;
};

#ifdef BIG_ENDIAN
inline uint16_t swap_endianess_on_big_endian_machine(uint16_t v)
{
    return ((v & 0xFF00) >> 8) | ((v & 0xFF) << 8);
}
inline uint32_t swap_endianess_on_big_endian_machine(uint32_t v)
{
    return ((v & 0xFF000000) >> 24) | ((v & 0xFF0000) >> 8) | ((v & 0xFF00) << 8) | ((v & 0xFF) << 24);
}
inline uint64_t swap_endianess_on_big_endian_machine(uint64_t v)
{
    return uint64_t(swap_endianess_on_big_endian_machine(uint32_t(v >> 32)))
           | (uint64_t(swap_endianess_on_big_endian_machine(uint32_t(v))) << 32);
}
#else
inline uint16_t swap_endianess_on_big_endian_machine(uint16_t v)
{
    return v;
}
inline uint32_t swap_endianess_on_big_endian_machine(uint32_t v)
{
    return v;
}
inline uint64_t swap_endianess_on_big_endian_machine(uint64_t v)
{
    return v;
}
#endif

inline void write_i8(std::ostream& out, int8_t i)
{
    // no LE/BE swap needed.
    out.write((char*) &i, 1);
}

inline void write_i16(std::ostream& out, int16_t i)
{
    uint16_t ii = swap_endianess_on_big_endian_machine((uint16_t) i);
    out.write((char*) &ii, 2);
}

inline void write_i32(std::ostream& out, int32_t i)
{
    uint32_t ii = swap_endianess_on_big_endian_machine((uint32_t) i);
    out.write((char*) &ii, 4);
}

inline void write_i64(std::ostream& out, int64_t i)
{
    uint64_t ii = swap_endianess_on_big_endian_machine((uint64_t) i);
    out.write((char*) &ii, 8);
}

inline void write_u8(std::ostream& out, uint8_t i)
{
    // no LE/BE swap needed.
    out.write((char*) &i, 1);
}

inline void write_u16(std::ostream& out, uint16_t i)
{
    uint16_t ii = swap_endianess_on_big_endian_machine(i);
    out.write((char*) &ii, 2);
}

inline void write_u32(std::ostream& out, uint32_t i)
{
    uint32_t ii = swap_endianess_on_big_endian_machine(i);
    out.write((char*) &ii, 4);
}

inline void write_u64(std::ostream& out, uint64_t i)
{
    uint64_t ii = swap_endianess_on_big_endian_machine(i);
    out.write((char*) &ii, 8);
}

inline int8_t read_i8(std::istream& in)
{
    int8_t i = 0; // no LE/BE swap needed.
    in.read((char*) &i, 1);
    return i;
}

inline int16_t read_i16(std::istream& in)
{
    uint16_t i = 0;
    in.read((char*) &i, 2);
    return int16_t(swap_endianess_on_big_endian_machine(i));
}

inline int32_t read_i32(std::istream& in)
{
    uint32_t i = 0;
    in.read((char*) &i, 4);
    return int32_t(swap_endianess_on_big_endian_machine(i));
}

inline int64_t read_i64(std::istream& in)
{
    uint64_t i = 0;
    in.read((char*) &i, 8);
    return int64_t(swap_endianess_on_big_endian_machine(i));
}

inline uint8_t read_u8(std::istream& in)
{
    uint8_t i = 0; // no LE/BE swap needed.
    in.read((char*) &i, 1);
    return i;
}

inline uint16_t read_u16(std::istream& in)
{
    uint16_t i = 0;
    in.read((char*) &i, 2);
    return uint16_t(swap_endianess_on_big_endian_machine(i));
}

inline uint32_t read_u32(std::istream& in)
{
    uint32_t i = 0;
    in.read((char*) &i, 4);
    return uint32_t(swap_endianess_on_big_endian_machine(i));
}

inline uint64_t read_u64(std::istream& in)
{
    uint64_t i = 0;
    in.read((char*) &i, 8);
    return uint64_t(swap_endianess_on_big_endian_machine(i));
}

inline void write_bool(std::ostream& out, bool b)
{
    write_u8(out, b ? 1 : 0);
}

inline bool read_bool(std::istream& in)
{
    return (read_u8(in) != 0);
}

inline void write_float(std::ostream& out, float f)
{
    float_u32_shared s;
    s.f = f;
    write_u32(out, s.u);
}

inline float read_float(std::istream& in)
{
    float_u32_shared s;
    s.u = read_u32(in);
    return s.f;
}

inline void write_double(std::ostream& out, double d)
{
    double_u64_shared s;
    s.d = d;
    write_u64(out, s.u);
}

inline double read_double(std::istream& in)
{
    double_u64_shared s;
    s.u = read_u64(in);
    return s.d;
}

inline void write_string(std::ostream& out, const std::string& s)
{
    unsigned l = s.size();
    write_u32(out, l);
    if (l > 0)
        out.write(s.c_str(), l);
}

inline std::string read_string(std::istream& in)
{
    unsigned l = read_u32(in);
    if (l > 0) {
        std::string s(l, 'x');
        in.read(&s[0], l);
        return s;
    } else {
        return std::string();
    }
}

inline vector2 read_vector2(std::istream& in)
{
    vector2 v;
    v.x = read_double(in);
    v.y = read_double(in);
    return v;
}

inline void write_vector2(std::ostream& out, const vector2& v)
{
    write_double(out, v.x);
    write_double(out, v.y);
}

inline vector3 read_vector3(std::istream& in)
{
    vector3 v;
    v.x = read_double(in);
    v.y = read_double(in);
    v.z = read_double(in);
    return v;
}

inline void write_vector3(std::ostream& out, const vector3& v)
{
    write_double(out, v.x);
    write_double(out, v.y);
    write_double(out, v.z);
}

inline quaternion read_quaternion(std::istream& in)
{
    double s = read_double(in);
    return quaternion(s, read_vector3(in));
}

inline void write_quaternion(std::ostream& out, const quaternion& q)
{
    write_double(out, q.s);
    write_vector3(out, q.v);
}
