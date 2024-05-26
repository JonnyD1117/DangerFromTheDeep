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
//  A 4x4 matrix (C)+(W) 2001 Thorsten Jordan
//

#pragma once

#include "constant.hpp"
#include "matrix3.hpp"

#define USE_GL_CRAP_REMOVE_LATER
#ifdef USE_GL_CRAP_REMOVE_LATER
#include "OglExt.h"

#include <cstring>
#endif

/// a 4x4 matrix, reimplemented for 4x4 case for speed issues
template<typename D>
class matrix4t
{
    D values[4 * 4];

    matrix4t(int /*dummy*/) { } // internal use

  public:
    /// empty matrix
    matrix4t()
    {
        for (auto& value : values)
            value = (D) (0);
    }

    /// create full matrix
    matrix4t(D e0, D e1, D e2, D e3, D e4, D e5, D e6, D e7, D e8, D e9, D e10, D e11, D e12, D e13, D e14, D e15)
    {
        values[0]  = e0;
        values[1]  = e1;
        values[2]  = e2;
        values[3]  = e3;
        values[4]  = e4;
        values[5]  = e5;
        values[6]  = e6;
        values[7]  = e7;
        values[8]  = e8;
        values[9]  = e9;
        values[10] = e10;
        values[11] = e11;
        values[12] = e12;
        values[13] = e13;
        values[14] = e14;
        values[15] = e15;
    }

    /// create matrix from column vectors
    matrix4t(const vector3t<D>& v0, const vector3t<D>& v1, const vector3t<D>& v2, const vector3t<D>& v3)
    {
        values[0]  = v0.x;
        values[4]  = v0.y;
        values[8]  = v0.z;
        values[12] = D(0.0);
        values[1]  = v1.x;
        values[5]  = v1.y;
        values[9]  = v1.z;
        values[13] = D(0.0);
        values[2]  = v2.x;
        values[6]  = v2.y;
        values[10] = v2.z;
        values[14] = D(0.0);
        values[3]  = v3.x;
        values[7]  = v3.y;
        values[11] = v3.z;
        values[15] = D(1.0);
    }

    /// create matrix from column vectors
    matrix4t(const vector4t<D>& v0, const vector4t<D>& v1, const vector4t<D>& v2, const vector4t<D>& v3)
    {
        values[0]  = v0.x;
        values[4]  = v0.y;
        values[8]  = v0.z;
        values[12] = v0.w;
        values[1]  = v1.x;
        values[5]  = v1.y;
        values[9]  = v1.z;
        values[13] = v1.w;
        values[2]  = v2.x;
        values[6]  = v2.y;
        values[10] = v2.z;
        values[14] = v2.w;
        values[3]  = v3.x;
        values[7]  = v3.y;
        values[11] = v3.z;
        values[15] = v3.w;
    }

    /// return pointer to array of elements
    [[nodiscard]] const D* elemarray() const { return &values[0]; }

    /// return pointer to array of elements
    D* elemarray() { return &values[0]; }

    /// construct 4x4 matrix from one with different template type but same
    /// dimension
    template<typename E>
    matrix4t(const matrix4t<E>& other)
    {
        const E* ea = other.elemarray();
        for (unsigned i = 0; i < 4 * 4; ++i)
            values[i] = D(ea[i]);
    }

    /// construct from stream
    matrix4t(std::istream& is)
    {
        for (auto& elem : values)
            is >> elem;
    }

    /// print to stream
    void to_stream(std::ostream& os) const
    {
        os << "/----\n";
        for (unsigned y = 0; y < 4; ++y) {
            os << "(\t";
            for (unsigned x = 0; x < 4; ++x) {
                os << values[y * 4 + x] << "\t";
            }
            os << ")\n";
        }
        os << "\\----\n";
    }

    /// multiply by scalar
    matrix4t<D> operator*(D s) const
    {
        matrix4t<D> r(int(0));
        for (unsigned i = 0; i < 4 * 4; ++i)
            r.values[i] = values[i] * s;
        return r;
    }

    /// add matrices
    matrix4t<D> operator+(const matrix4t<D>& other) const
    {
        matrix4t<D> r(int(0));
        for (unsigned i = 0; i < 4 * 4; ++i)
            r.values[i] = values[i] + other.values[i];
        return r;
    }

    /// subtract matrices
    matrix4t<D> operator-(const matrix4t<D>& other) const
    {
        matrix4t<D> r(int(0));
        for (unsigned i = 0; i < 4 * 4; ++i)
            r.values[i] = values[i] - other.values[i];
        return r;
    }

    /// multiply matrices
    matrix4t<D> operator*(const matrix4t<D>& other) const
    {
        matrix4t<D> r;
        r.values[0] = values[0] * other.values[0] + values[1] * other.values[4] + values[2] * other.values[8]
                      + values[3] * other.values[12];
        r.values[1] = values[0] * other.values[1] + values[1] * other.values[5] + values[2] * other.values[9]
                      + values[3] * other.values[13];
        r.values[2] = values[0] * other.values[2] + values[1] * other.values[6] + values[2] * other.values[10]
                      + values[3] * other.values[14];
        r.values[3] = values[0] * other.values[3] + values[1] * other.values[7] + values[2] * other.values[11]
                      + values[3] * other.values[15];
        r.values[4] = values[4] * other.values[0] + values[5] * other.values[4] + values[6] * other.values[8]
                      + values[7] * other.values[12];
        r.values[5] = values[4] * other.values[1] + values[5] * other.values[5] + values[6] * other.values[9]
                      + values[7] * other.values[13];
        r.values[6] = values[4] * other.values[2] + values[5] * other.values[6] + values[6] * other.values[10]
                      + values[7] * other.values[14];
        r.values[7] = values[4] * other.values[3] + values[5] * other.values[7] + values[6] * other.values[11]
                      + values[7] * other.values[15];
        r.values[8] = values[8] * other.values[0] + values[9] * other.values[4] + values[10] * other.values[8]
                      + values[11] * other.values[12];
        r.values[9] = values[8] * other.values[1] + values[9] * other.values[5] + values[10] * other.values[9]
                      + values[11] * other.values[13];
        r.values[10] = values[8] * other.values[2] + values[9] * other.values[6] + values[10] * other.values[10]
                       + values[11] * other.values[14];
        r.values[11] = values[8] * other.values[3] + values[9] * other.values[7] + values[10] * other.values[11]
                       + values[11] * other.values[15];
        r.values[12] = values[12] * other.values[0] + values[13] * other.values[4] + values[14] * other.values[8]
                       + values[15] * other.values[12];
        r.values[13] = values[12] * other.values[1] + values[13] * other.values[5] + values[14] * other.values[9]
                       + values[15] * other.values[13];
        r.values[14] = values[12] * other.values[2] + values[13] * other.values[6] + values[14] * other.values[10]
                       + values[15] * other.values[14];
        r.values[15] = values[12] * other.values[3] + values[13] * other.values[7] + values[14] * other.values[11]
                       + values[15] * other.values[15];
        return r;
    }

    matrix4t<D>& operator*=(const matrix4t<D>& other)
    {
        matrix4t<D> r;
        r.values[0] = values[0] * other.values[0] + values[1] * other.values[4] + values[2] * other.values[8]
                      + values[3] * other.values[12];
        r.values[1] = values[0] * other.values[1] + values[1] * other.values[5] + values[2] * other.values[9]
                      + values[3] * other.values[13];
        r.values[2] = values[0] * other.values[2] + values[1] * other.values[6] + values[2] * other.values[10]
                      + values[3] * other.values[14];
        r.values[3] = values[0] * other.values[3] + values[1] * other.values[7] + values[2] * other.values[11]
                      + values[3] * other.values[15];
        r.values[4] = values[4] * other.values[0] + values[5] * other.values[4] + values[6] * other.values[8]
                      + values[7] * other.values[12];
        r.values[5] = values[4] * other.values[1] + values[5] * other.values[5] + values[6] * other.values[9]
                      + values[7] * other.values[13];
        r.values[6] = values[4] * other.values[2] + values[5] * other.values[6] + values[6] * other.values[10]
                      + values[7] * other.values[14];
        r.values[7] = values[4] * other.values[3] + values[5] * other.values[7] + values[6] * other.values[11]
                      + values[7] * other.values[15];
        r.values[8] = values[8] * other.values[0] + values[9] * other.values[4] + values[10] * other.values[8]
                      + values[11] * other.values[12];
        r.values[9] = values[8] * other.values[1] + values[9] * other.values[5] + values[10] * other.values[9]
                      + values[11] * other.values[13];
        r.values[10] = values[8] * other.values[2] + values[9] * other.values[6] + values[10] * other.values[10]
                       + values[11] * other.values[14];
        r.values[11] = values[8] * other.values[3] + values[9] * other.values[7] + values[10] * other.values[11]
                       + values[11] * other.values[15];
        r.values[12] = values[12] * other.values[0] + values[13] * other.values[4] + values[14] * other.values[8]
                       + values[15] * other.values[12];
        r.values[13] = values[12] * other.values[1] + values[13] * other.values[5] + values[14] * other.values[9]
                       + values[15] * other.values[13];
        r.values[14] = values[12] * other.values[2] + values[13] * other.values[6] + values[14] * other.values[10]
                       + values[15] * other.values[14];
        r.values[15] = values[12] * other.values[3] + values[13] * other.values[7] + values[14] * other.values[11]
                       + values[15] * other.values[15];
        *this = r;
        return *this;
    }

    /// negate matrix
    matrix4t<D> operator-() const
    {
        matrix4t<D> r(int(0));
        for (unsigned i = 0; i < 4 * 4; ++i)
            r.values[i] = -values[i];
        return r;
    }

    /// create identitiy matrix
    static matrix4t<D> one()
    {
        matrix4t<D> r;
        r.values[0] = r.values[5] = r.values[10] = r.values[15] = D(1.0);
        return r;
    }

    /// get transposed matrix
    [[nodiscard]] matrix4t<D> transposed() const
    {
        return matrix4t<D>(
            values[0],
            values[4],
            values[8],
            values[12],
            values[1],
            values[5],
            values[9],
            values[13],
            values[2],
            values[6],
            values[10],
            values[14],
            values[3],
            values[7],
            values[11],
            values[15]);
    }

    /// get inverse of matrix
    [[nodiscard]] matrix4t<D> inverse() const
    {
        matrix4t<D> r(*this);
        matrix_invert<D, 4U>(r.elemarray());
        return r;
    }

    /// get upper left 3x3 matrix
    [[nodiscard]] matrix3t<D> upper_left_3x3() const
    {
        return matrix3t<D>(
            values[0], values[1], values[2], values[4], values[5], values[6], values[8], values[9], values[10]);
    }

    /// multiply matrix with vector
    vector4t<D> operator*(const vector4t<D>& v) const
    {
        return vector4t<D>(
            values[0] * v.x + values[1] * v.y + values[2] * v.z + values[3] * v.w,
            values[4] * v.x + values[5] * v.y + values[6] * v.z + values[7] * v.w,
            values[8] * v.x + values[9] * v.y + values[10] * v.z + values[11] * v.w,
            values[12] * v.x + values[13] * v.y + values[14] * v.z + values[15] * v.w);
    }

    // get n-th row/col, ignores last value
    [[nodiscard]] vector3t<D> row3(unsigned i) const
    {
        return vector3t<D>(values[4 * i], values[4 * i + 1], values[4 * i + 2]);
    }
    [[nodiscard]] vector3t<D> column3(unsigned i) const { return vector3t<D>(values[i], values[i + 4], values[i + 8]); }

    // get n-th row/col, with last value
    [[nodiscard]] vector4t<D> row(unsigned i) const
    {
        return vector4t<D>(values[4 * i], values[4 * i + 1], values[4 * i + 2], values[4 * i + 3]);
    }
    [[nodiscard]] vector4t<D> column(unsigned i) const
    {
        return vector4t<D>(values[i], values[i + 4], values[i + 8], values[i + 12]);
    }

#ifdef USE_GL_CRAP_REMOVE_LATER
    void set_gl(GLenum pname);  // GL_PROJECTION, GL_MODELVIEW, GL_TEXTURE
    void set_glf(GLenum pname); // GL_PROJECTION, GL_MODELVIEW, GL_TEXTURE
    void multiply_gl() const;
    void multiply_glf() const;

    static matrix4t<D> get_gl(GLenum pname); // GL_PROJECTION_MATRIX, GL_MODELVIEW_MATRIX,
                                             // GL_TEXTURE_MATRIX

    static matrix4t<D> get_glf(GLenum pname); // GL_PROJECTION_MATRIX, GL_MODELVIEW_MATRIX,
                                              // GL_TEXTURE_MATRIX
#endif

    static matrix4t<D> rot_x(D degrees)
    {
        D a = degrees * D(constant::PI / 180);
        D c = D(cos(a)), s = D(sin(a)), o = D(1.0), n = D(0.0);
        return matrix4t<D>(o, n, n, n, n, c, -s, n, n, s, c, n, n, n, n, o);
    }

    static matrix4t<D> rot_y(D degrees)
    {
        D a = degrees * D(constant::PI / 180);
        D c = D(cos(a)), s = D(sin(a)), o = D(1.0), n = D(0.0);
        return matrix4t<D>(c, n, s, n, n, o, n, n, -s, n, c, n, n, n, n, o);
    }

    static matrix4t<D> rot_z(D degrees)
    {
        D a = degrees * D(constant::PI / 180);
        D c = D(cos(a)), s = D(sin(a)), o = D(1.0), n = D(0.0);
        return matrix4t<D>(c, -s, n, n, s, c, n, n, n, n, o, n, n, n, n, o);
    }

    static matrix4t<D> rot_x_rad(D a)
    {
        D c = D(cos(a)), s = D(sin(a)), o = D(1.0), n = D(0.0);
        return matrix4t<D>(o, n, n, n, n, c, -s, n, n, s, c, n, n, n, n, o);
    }

    static matrix4t<D> rot_y_rad(D a)
    {
        D c = D(cos(a)), s = D(sin(a)), o = D(1.0), n = D(0.0);
        return matrix4t<D>(c, n, s, n, n, o, n, n, -s, n, c, n, n, n, n, o);
    }

    static matrix4t<D> rot_z_rad(D a)
    {
        D c = D(cos(a)), s = D(sin(a)), o = D(1.0), n = D(0.0);
        return matrix4t<D>(c, -s, n, n, s, c, n, n, n, n, o, n, n, n, n, o);
    }

    static matrix4t<D> trans(D x, D y, D z)
    {
        D o = D(1.0), n = D(0.0);
        return matrix4t<D>(o, n, n, x, n, o, n, y, n, n, o, z, n, n, n, o);
    }

    static matrix4t<D> trans(const vector3t<D>& v)
    {
        D o = D(1.0), n = D(0.0);
        return matrix4t<D>(o, n, n, v.x, n, o, n, v.y, n, n, o, v.z, n, n, n, o);
    }

    static matrix4t<D> diagonal(D x, D y, D z, D w = D(1.0))
    {
        D n = D(0.0);
        return matrix4t<D>(x, n, n, n, n, y, n, n, n, n, z, n, n, n, n, w);
    }

    static matrix4t<D> diagonal(const vector3t<D>& v, D w = D(1.0))
    {
        D n = D(0.0);
        return matrix4t<D>(v.x, n, n, n, n, v.y, n, n, n, n, v.z, n, n, n, n, w);
    }

    static matrix4t<D> scale(D factor)
    {
        return diagonal(factor, factor, factor);
    }

    void clear_rot()
    {
        values[0] = values[5] = values[10] = D(1.0);
        values[1] = values[2] = values[4] = values[6] = values[8] = values[9] = D(0.0);
    }

    void clear_trans()
    {
        values[3] = values[7] = values[11] = D(0.0);
    }

    static matrix4t<D> frustum_fovx(D fovx, D aspect, D znear, D zfar)
    {
        D tanfovx2 = D(tan(constant::PI * fovx / 360.0));
        D tanfovy2 = tanfovx2 / aspect;
        D r        = znear * tanfovx2;
        D t        = znear * tanfovy2;
        D n_m_f    = znear - zfar;
        // glFrustum(l,r,b,t,n,f) generates
        // 2n/(r-l)   0      (r+l)/(r-l)   0
        //    0     2n/(t-b) (t+b)/(t-b)   0
        //    0       0      -(f+n)/(f-n) -2f*n/(f-n)
        //    0       0       -1           0
        // here we generate glFrustum(-r, r, -t, t, n, f);
        // since l = -r and b = -t we get a matrix:
        //   n/r      0       0            0
        //    0      n/t      0            0
        //    0       0      (f+n)/(n-f) 2f*n/(n-f)
        //    0       0       -1           0
        return matrix4t<D>(
            znear / r,
            0,
            0,
            0,
            0,
            znear / t,
            0,
            0,
            0,
            0,
            (zfar + znear) / n_m_f,
            2 * zfar * znear / n_m_f,
            0,
            0,
            -1,
            0);
    }

    static matrix4t<D> ortho(D left, D right, D bottom, D top, D znear, D zfar)
    {
        D r_m_l = right - left;
        D t_m_b = top - bottom;
        D n_m_f = znear - zfar;
        // glOrtho(l,r,b,t,n,f) generates
        // 2/(r-l)    0       0           -(r+l)/(r-l)
        //    0     2/(t-b)   0           -(t+b)/(t-b)
        //    0       0      -2/(f-n)     -(f+n)/(f-n)
        //    0       0       0           1
        // gluOrtho2D is like calling glOrtho with near=-1, far=1
        return matrix4t<D>(
            2 / r_m_l,
            0,
            0,
            -(right + left) / r_m_l,
            0,
            2 / t_m_b,
            0,
            -(top + bottom) / t_m_b,
            0,
            0,
            2 / n_m_f,
            (zfar + znear) / n_m_f,
            0,
            0,
            0,
            1);
    }

    static matrix4t<D> ortho2D(D left, D right, D bottom, D top)
    {
        return ortho(left, right, bottom, top, -1.0, 1.0);
    }

    /// multiply 4x4 matrix with 3-vector, with w-renormalization
    [[nodiscard]] vector3t<D> mul4vec3(const vector3t<D>& v) const
    {
        D r[4];
        for (unsigned j = 0; j < 4; ++j) { // rows of "this"
            r[j] = values[j * 4 + 0] * v.x + values[j * 4 + 1] * v.y + values[j * 4 + 2] * v.z + values[j * 4 + 3];
        }
        // divide x,y,z by w
        return vector3t<D>(r[0] / r[3], r[1] / r[3], r[2] / r[3]);
    }

    /// multiply 4x4 matrix with 3-vector, ignore projection part (faster)
    [[nodiscard]] vector3t<D> mul4vec3xlat(const vector3t<D>& v) const
    {
        D r[3];
        for (unsigned j = 0; j < 3; ++j) { // rows of "this"
            r[j] = values[j * 4 + 0] * v.x + values[j * 4 + 1] * v.y + values[j * 4 + 2] * v.z + values[j * 4 + 3];
        }
        return vector3t<D>(r[0], r[1], r[2]);
    }

    /// multiply matrix with vector3
    vector3t<D> operator*(const vector3t<D>& v) const
    {
        return mul4vec3xlat(v);
    }

    D& elem(unsigned col, unsigned row)
    {
        return values[col + row * 4];
    }
    [[nodiscard]] const D& elem(unsigned col, unsigned row) const
    {
        return values[col + row * 4];
    }
};

#ifdef USE_GL_CRAP_REMOVE_LATER
template<class D>
void matrix4t<D>::set_gl(GLenum pname)
{
    GLdouble m[16];
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned j = 0; j < 4; ++j)
            m[i + j * 4] = GLdouble(values[j + i * 4]);
    glMatrixMode(pname);
    glLoadMatrixd(m);
    glMatrixMode(GL_MODELVIEW);
}

template<class D>
void matrix4t<D>::set_glf(GLenum pname)
{
    GLfloat m[16];
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned j = 0; j < 4; ++j)
            m[i + j * 4] = GLfloat(values[j + i * 4]);
    glMatrixMode(pname);
    glLoadMatrixf(m);
    glMatrixMode(GL_MODELVIEW);
}

template<class D>
void matrix4t<D>::multiply_gl() const
{
    GLdouble m[16];
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned j = 0; j < 4; ++j)
            m[i + j * 4] = GLdouble(values[j + i * 4]);
    glMultMatrixd(m);
}

template<class D>
void matrix4t<D>::multiply_glf() const
{
    GLfloat m[16];
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned j = 0; j < 4; ++j)
            m[i + j * 4] = GLfloat(values[j + i * 4]);
    glMultMatrixf(m);
}

template<class D>
matrix4t<D> matrix4t<D>::get_gl(GLenum pname)
{
    GLdouble m[16];
    glGetDoublev(pname, m);
    matrix4t<D> r;
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned j = 0; j < 4; ++j)
            r.values[j + i * 4] = D(m[i + j * 4]);
    return r;
}

template<class D>
matrix4t<D> matrix4t<D>::get_glf(GLenum pname)
{
    GLfloat m[16];
    glGetFloatv(pname, m);
    matrix4t<D> r;
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned j = 0; j < 4; ++j)
            r.values[j + i * 4] = D(m[i + j * 4]);
    return r;
}
#endif

using matrix4  = matrix4t<double>;
using matrix4f = matrix4t<float>;

template<typename D>
std::ostream& operator<<(std::ostream& os, const matrix4t<D>& m)
{
    m.to_stream(os);
    return os;
}
