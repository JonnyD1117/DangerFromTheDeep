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

//
//  A 2d vector (C)+(W) 2001 Thorsten Jordan
//

#pragma once

#include <algorithm> // for std::min/std::max
#include <cmath>     // for sqrt
#include <iostream>

template<typename D2>
class vector3t;

template<typename D>
inline D epsilon_sqr()
{
    return D(1e-6);
}

template<>
inline double epsilon_sqr()
{
    return 1e-8;
}

template<typename D>
inline D epsilon()
{
    return D(1e-3);
}

template<>
inline double epsilon()
{
    return 1e-4;
}

///\brief Template class for a mathematical vector with two coefficients.
template<typename D>
class vector2t
{
  public:
    D x, y;

    vector2t()
        : x(0)
        , y(0)
    {
    }
    vector2t(D x_, D y_)
        : x(x_)
        , y(y_)
    {
    }

    template<typename E>
    explicit vector2t(const vector2t<E>& other)
        : x(D(other.x))
        , y(D(other.y))
    {
    }

    [[nodiscard]] vector2t<D> normal() const
    {
        D len = D(1.0) / length();
        return vector2t(x * len, y * len);
    }

    void normalize()
    {
        D len = D(1.0) / length();
        x *= len;
        y *= len;
    }
    [[nodiscard]] vector2t<D> orthogonal() const { return vector2t(-y, x); }

    vector2t<D> operator*(D scalar) const { return vector2t(x * scalar, y * scalar); }

    vector2t<D> operator/(D scalar) const { return vector2t(x / scalar, y / scalar); }

    vector2t<D> operator+(const vector2t<D>& other) const { return vector2t(x + other.x, y + other.y); }

    vector2t<D> operator-(const vector2t<D>& other) const { return vector2t(x - other.x, y - other.y); }

    vector2t<D> operator-() const { return vector2t(-x, -y); }

    vector2t<D> operator&(D mask) const { return vector2t(x & mask, y & mask); } // won't work with float.

    vector2t<D>& operator+=(const vector2t<D>& other)
    {
        x += other.x;
        y += other.y;
        return *this;
    }

    vector2t<D>& operator-=(const vector2t<D>& other)
    {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    vector2t<D>& operator*=(D s)
    {
        x *= s;
        y *= s;
        return *this;
    }

    [[nodiscard]] vector2t<D> min(const vector2t<D>& other) const
    {
        return vector2t(std::min(x, other.x), std::min(y, other.y));
    }

    [[nodiscard]] vector2t<D> max(const vector2t<D>& other) const
    {
        return vector2t(std::max(x, other.x), std::max(y, other.y));
    }

    bool operator==(const vector2t<D>& other) const { return x == other.x && y == other.y; }

    bool operator!=(const vector2t<D>& other) const { return x != other.x || y != other.y; }

    [[nodiscard]] D square_length() const { return x * x + y * y; }
    [[nodiscard]] D length() const { return D(::sqrt(square_length())); }

    [[nodiscard]] D square_distance(const vector2t<D>& other) const
    {
        vector2t<D> n = *this - other;
        return n.square_length();
    }

    [[nodiscard]] D distance(const vector2t<D>& other) const
    {
        vector2t<D> n = *this - other;
        return n.length();
    }

    D operator*(const vector2t<D>& other) const { return x * other.x + y * other.y; }

    bool solve(const vector2t<D>& o1, const vector2t<D>& o2, D& s1, D& s2) const;

    /// multiplies 2x2 matrix (given in columns c0-c1) with *this.
    [[nodiscard]] vector2t<D> matrixmul(const vector2t<D>& c0, const vector2t<D>& c1) const;

    [[nodiscard]] vector2t<D> coeff_mul(const vector2t<D>& other) const { return vector2t(x * other.x, y * other.y); }

    [[nodiscard]] vector3t<D> xy0() const { return vector3t<D>(x, y, 0); }

    [[nodiscard]] vector3t<D> xyz(D z) const { return vector3t<D>(x, y, z); }

    template<typename D2>
    friend std::ostream& operator<<(std::ostream& os, const vector2t<D2>& v);

    template<typename E>
    void assign(const vector2t<E>& other)
    {
        x = D(other.x);
        y = D(other.y);
    }

    bool operator<(const vector2t<D>& other) const { return x < other.x ? true : (x == other.x ? y < other.y : false); }

    [[nodiscard]] vector2t<D> floor() const { return vector2t(D(::floor(x)), D(::floor(y))); }

    [[nodiscard]] vector2t<D> frac() const { return vector2t(D(x - ::floor(x)), D(y - ::floor(y))); }

    [[nodiscard]] vector2t<D> mod(D v) const
    {
        return *this - (*this / v).floor() * v;
    } // negative values are problematic for fmod. So we use own formula fmod(a,
      // b) = a - floor(a / b) * b

    [[nodiscard]] vector2t<D> abs() const { return vector2t<D>(x < D(0) ? -x : x, y < D(0) ? -y : y); }

    static vector2t<D> x_axis() { return vector2t<D>(D(1), D(0)); }
    static vector2t<D> y_axis() { return vector2t<D>(D(0), D(1)); }
};

template<typename D>
bool vector2t<D>::solve(const vector2t<D>& o1, const vector2t<D>& o2, D& s1, D& s2) const
{
    D det = o1.x * o2.y - o2.x * o1.y;

    if (!det)
        return false;

    s1 = (o2.y * x - o2.x * y) / det;
    s2 = (o1.x * y - o1.y * x) / det;

    return true;
}

template<typename D>
vector2t<D> vector2t<D>::matrixmul(const vector2t<D>& c0, const vector2t<D>& c1) const
{
    return vector2t<D>(c0.x * x + c1.x * y, c0.y * x + c1.y * y);
}

template<typename D2>
inline vector2t<D2> operator*(D2 scalar, const vector2t<D2>& v)
{
    return v * scalar;
}

template<typename D2>
std::ostream& operator<<(std::ostream& os, const vector2t<D2>& v)
{
    os << "x=" << v.x << "; y=" << v.y;
    return os;
}

using vector2  = vector2t<double>;
using vector2f = vector2t<float>;
using vector2i = vector2t<int>;
using vector2l = vector2t<long>;
