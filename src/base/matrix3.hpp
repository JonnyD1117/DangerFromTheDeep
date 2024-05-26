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
//  A 3x3 matrix (C)+(W) 2001 Thorsten Jordan
//

#pragma once

#include "matrix.hpp"

#include <cstring>

/// a 3x3 matrix, reimplemented for 3x3 case for speed issues
template<typename D>
class matrix3t
{
    D values[3 * 3];

    matrix3t(int /*dummy*/) { } // internal use

  public:
    /// empty matrix
    matrix3t()
    {
        for (auto& value : values)
            value = (D) (0);
    }

    /// create full matrix
    matrix3t(D e0, D e1, D e2, D e3, D e4, D e5, D e6, D e7, D e8)
    {
        values[0] = e0;
        values[1] = e1;
        values[2] = e2;
        values[3] = e3;
        values[4] = e4;
        values[5] = e5;
        values[6] = e6;
        values[7] = e7;
        values[8] = e8;
    }

    /// create matrix from column vectors
    matrix3t(const vector3t<D>& v0, const vector3t<D>& v1, const vector3t<D>& v2)
    {
        values[0] = v0.x;
        values[3] = v0.y;
        values[6] = v0.z;
        values[1] = v1.x;
        values[4] = v1.y;
        values[7] = v1.z;
        values[2] = v2.x;
        values[5] = v2.y;
        values[8] = v2.z;
    }

    /// return pointer to array of elements
    [[nodiscard]] const D* elemarray() const { return &values[0]; }

    D* elemarray() { return &values[0]; }

    /// construct 3x3 matrix from one with different template type but same
    /// dimension
    template<typename E>
    matrix3t(const matrix3t<E>& other)
    {
        const E* ea = other.elemarray();
        for (unsigned i = 0; i < 3 * 3; ++i)
            values[i] = D(ea[i]);
    }

    /// construct from stream
    matrix3t(std::istream& is)
    {
        for (auto& elem : values)
            is >> elem;
    }

    /// print to stream
    void to_stream(std::ostream& os) const
    {
        os << "/----\n";
        for (unsigned y = 0; y < 3; ++y) {
            os << "(\t";
            for (unsigned x = 0; x < 3; ++x) {
                os << values[y * 3 + x] << "\t";
            }
            os << ")\n";
        }
        os << "\\----\n";
    }

    /// multiply by scalar
    matrix3t<D> operator*(D s) const
    {
        matrix3t<D> r(int(0));
        for (unsigned i = 0; i < 3 * 3; ++i)
            r.values[i] = values[i] * s;
        return r;
    }

    /// add matrices
    matrix3t<D> operator+(const matrix3t<D>& other) const
    {
        matrix3t<D> r(int(0));
        for (unsigned i = 0; i < 3 * 3; ++i)
            r.values[i] = values[i] + other.values[i];
        return r;
    }

    /// subtract matrices
    matrix3t<D> operator-(const matrix3t<D>& other) const
    {
        matrix3t<D> r(int(0));
        for (unsigned i = 0; i < 3 * 3; ++i)
            r.values[i] = values[i] - other.values[i];
        return r;
    }

    /// multiply matrices
    matrix3t<D> operator*(const matrix3t<D>& other) const
    {
        matrix3t<D> r(int(0));
        r.values[0] = values[0] * other.values[0] + values[1] * other.values[3] + values[2] * other.values[6];
        r.values[1] = values[0] * other.values[1] + values[1] * other.values[4] + values[2] * other.values[7];
        r.values[2] = values[0] * other.values[2] + values[1] * other.values[5] + values[2] * other.values[8];
        r.values[3] = values[3] * other.values[0] + values[4] * other.values[3] + values[6] * other.values[6];
        r.values[4] = values[3] * other.values[1] + values[4] * other.values[4] + values[6] * other.values[7];
        r.values[5] = values[3] * other.values[2] + values[4] * other.values[5] + values[6] * other.values[8];
        r.values[6] = values[6] * other.values[0] + values[7] * other.values[3] + values[8] * other.values[6];
        r.values[7] = values[6] * other.values[1] + values[7] * other.values[4] + values[8] * other.values[7];
        r.values[8] = values[6] * other.values[2] + values[7] * other.values[5] + values[8] * other.values[8];
        return r;
    }

    /// negate matrix
    matrix3t<D> operator-() const
    {
        matrix3t<D> r(int(0));
        for (unsigned i = 0; i < 3 * 3; ++i)
            r.values[i] = -values[i];
        return r;
    }

    /// create identitiy matrix
    static matrix3t<D> one()
    {
        matrix3t<D> r;
        r.values[0] = r.values[4] = r.values[8] = D(1.0);
        return r;
    }

    /// get transposed matrix
    [[nodiscard]] matrix3t<D> transposed() const
    {
        return matrix3t<D>(
            values[0], values[3], values[6], values[1], values[4], values[7], values[2], values[5], values[8]);
    }

    /// get inverse of matrix
    [[nodiscard]] matrix3t<D> inverse() const
    {
        matrix3t<D> r(*this);
        matrix_invert<D, 3U>(r.elemarray());
        return r;
    }

    /// multiply matrix with vector
    vector3t<D> operator*(const vector3t<D>& v) const
    {
        return vector3t<D>(
            values[0] * v.x + values[1] * v.y + values[2] * v.z,
            values[3] * v.x + values[4] * v.y + values[5] * v.z,
            values[6] * v.x + values[7] * v.y + values[8] * v.z);
    }

    /// create matrix from square of vector
    static matrix3t<D> vec_sqr(const vector3t<D>& v)
    {
        return matrix3t<D>(
            v.x * v.x, v.x * v.y, v.x * v.z, v.y * v.x, v.y * v.y, v.y * v.z, v.z * v.x, v.z * v.y, v.z * v.z);
    }

    /// determinate
    [[nodiscard]] D determinate() const
    {
        return values[0] * values[4] * values[8] + values[1] * values[5] * values[6] + values[2] * values[3] * values[7]
               - values[0] * values[5] * values[7] - values[1] * values[3] * values[8]
               - values[2] * values[4] * values[6];
    }

    D& elem(unsigned col, unsigned row) { return values[col + row * 3]; }
    [[nodiscard]] const D& elem(unsigned col, unsigned row) const { return values[col + row * 3]; }

    [[nodiscard]] vector3t<D> row(unsigned i) const
    {
        return vector3t<D>(values[3 * i], values[3 * i + 1], values[3 * i + 2]);
    }
    [[nodiscard]] vector3t<D> column(unsigned i) const { return vector3t<D>(values[i], values[i + 3], values[i + 6]); }
};

using matrix3  = matrix3t<double>;
using matrix3f = matrix3t<float>;

template<typename D>
std::ostream& operator<<(std::ostream& os, const matrix3t<D>& m)
{
    m.to_stream(os);
    return os;
}
