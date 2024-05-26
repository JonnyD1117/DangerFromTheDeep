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

// a 32bit fixed point data type, handles only positive values
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#pragma once

#include <cstdint>

///\brief Implementation of a fixed point number.
class fixed32
{
    static const int32_t ONE    = 0x10000;
    static const int32_t HALF   = 0x08000;
    static const unsigned SHIFT = 16;
    int32_t x{0};

  public:
    fixed32()                            = default;
    fixed32(const fixed32& f)            = default;
    fixed32& operator=(const fixed32& f) = default;
    fixed32(int32_t n)
        : x(n)
    {
    }
    fixed32(float f)
        : x(int32_t(f * ONE))
    {
    }
    [[nodiscard]] fixed32 frac() const { return fixed32(x & (ONE - 1)); }
    [[nodiscard]] fixed32 floor() const { return fixed32(x & (~(ONE - 1))); }
    [[nodiscard]] fixed32 ceil() const { return fixed32((x + (ONE - 1)) & (~(ONE - 1))); }
    fixed32 operator+(const fixed32& f) const { return fixed32(x + f.x); }
    fixed32 operator-(const fixed32& f) const { return fixed32(x - f.x); }
    fixed32 operator-() const { return fixed32(-x); }
    fixed32 operator*(const fixed32& f) const { return fixed32(int32_t(int64_t(x) * f.x >> SHIFT)); }
    fixed32 operator*(int n) const { return fixed32(x * n); }
    fixed32& operator+=(const fixed32& f)
    {
        x += f.x;
        return *this;
    }
    fixed32& operator-=(const fixed32& f)
    {
        x -= f.x;
        return *this;
    }
    fixed32& operator*=(const fixed32& f)
    {
        x = int32_t(int64_t(x) * f.x >> SHIFT);
        return *this;
    }
    bool operator==(const fixed32& f) const { return x == f.x; }
    bool operator!=(const fixed32& f) const { return x != f.x; }
    bool operator<=(const fixed32& f) const { return x <= f.x; }
    bool operator>=(const fixed32& f) const { return x >= f.x; }
    bool operator<(const fixed32& f) const { return x < f.x; }
    bool operator>(const fixed32& f) const { return x > f.x; }
    static fixed32 one() { return fixed32(ONE); }
    fixed32 operator/(const fixed32& f) const { return fixed32(int32_t((int64_t(x) << SHIFT) / f.x)); }
    fixed32 operator/(int n) const { return fixed32(x / n); }
    [[nodiscard]] int intpart() const { return int(x >> SHIFT); }
    [[nodiscard]] int round() const { return int((x + HALF) >> SHIFT); }
    [[nodiscard]] int32_t value() const { return x; }
};
