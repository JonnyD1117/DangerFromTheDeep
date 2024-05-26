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
//  A two-dimensional general vector (C)+(W) 2001 Thorsten Jordan
//

#pragma once

#include "bivector.hpp"
#include "helper.hpp"
#include "vector2.hpp"

#include <sstream>
#include <vector>

///\brief Template class for a two-dimensional generic vector.
template<typename T>
class bivector
{
  protected:
    vector2i datasize;   ///< 2d data size (non-negative)
    std::vector<T> data; ///< flat representation of data

  public:
    /// default c'tor
    bivector() = default;
    /// c'tor with size
    bivector(const vector2i& sz, const T& v = T())
        : datasize(sz)
        , data(sz.x * sz.y, v)
    {
        if (datasize.x < 0 || datasize.y < 0) {
            throw std::invalid_argument("bivector size must not be negative");
        }
    }
    /// c'tor from bivector with other data
    template<typename U>
    bivector(const bivector<U>& source)
        : datasize(source.datasize)
        , data(source.data.size())
    {
        for (std::size_t i = 0; i < data.size(); ++i) {
            data[i] = T(source.data[i]);
        }
    }
    /// generate new bivector when values are in [0...1] range
    template<typename U>
    bivector<U> convert_01() const
    {
        bivector<U> result(datasize);
        for (std::size_t i = 0; i < data.size(); ++i) {
            ::convert_01(result.data[i], data[i]);
        }
        return result;
    }
    /// access at position, with bounds check
    T& at(const vector2i& p)
    {
        if (p.x >= datasize.x || p.y >= datasize.y) {
            std::stringstream ss;
            ss << "bivector::at " << p;
            throw std::out_of_range(ss.str());
        }
        return data[p.x + p.y * datasize.x];
    }
    /// const access at position, with bounds check
    [[nodiscard]] const T& at(const vector2i& p) const
    {
        if (p.x >= datasize.x || p.y >= datasize.y) {
            std::stringstream ss;
            ss << "bivector::at " << p;
            throw std::out_of_range(ss.str());
        }
        return data[p.x + p.y * datasize.x];
    }
    /// access at position, with bounds check
    [[nodiscard]] T& at(int x, int y)
    {
        if (x >= datasize.x || y >= datasize.y) {
            std::stringstream ss;
            ss << "bivector::at x=" << x << " y=" << x;
            throw std::out_of_range(ss.str());
        }
        return data[x + y * datasize.x];
    }
    /// const access at position, with bounds check
    [[nodiscard]] const T& at(int x, int y) const
    {
        if (x >= datasize.x || y >= datasize.y) {
            std::stringstream ss;
            ss << "bivector::at x=" << x << " y=" << x;
            throw std::out_of_range(ss.str());
        }
        return data[x + y * datasize.x];
    }
    T& operator[](const vector2i& p) { return data[p.x + p.y * datasize.x]; }
    const T& operator[](const vector2i& p) const { return data[p.x + p.y * datasize.x]; }
    [[nodiscard]] const auto& size() const { return datasize; }
    void resize(const vector2i& newsz, const T& v = T());
    [[nodiscard]] bivector<T> sub_area(const vector2i& offset, const vector2i& sz) const;
    ///@note bivector must have power of two dimensions for this!
    [[nodiscard]] bivector<T> shifted(const vector2i& offset) const;
    [[nodiscard]] bivector<T> transposed() const;

    void swap(bivector<T>& other)
    {
        data.swap(other.data);
        std::swap(datasize, other.datasize);
    }

    template<class U>
    bivector<U> convert() const;

    template<class U>
    bivector<U> convert(const T& minv, const T& maxv) const;

    // get pointer to storage, be very careful with that!
    T* data_ptr() { return &data[0]; }
    [[nodiscard]] const T* data_ptr() const { return &data[0]; }

    // move out plain data
    std::vector<T>&& move_plain_data()
    {
        datasize = {};
        return std::move(data);
    }

    // special operations
    [[nodiscard]] bivector<T> upsampled(bool wrap = false) const;
    [[nodiscard]] bivector<T> downsampled(bool force_even_size = false) const;

    [[nodiscard]] T get_min() const;
    [[nodiscard]] T get_max() const;
    [[nodiscard]] T get_min_abs() const;
    [[nodiscard]] T get_max_abs() const;

    bivector<T>& operator*=(const T& s);
    bivector<T>& operator+=(const T& v);
    bivector<T>& operator+=(const bivector<T>& v);
    [[nodiscard]] bivector<T> smooth_upsampled(bool wrap = false) const;

    // algebraic operations, omponent-wise add, sub, multiply (of same datasize)
    // sum of square of differences etc.

    // bivector<T>& add_gauss_noise(const T& scal, random_generator& rg);
    ///@note other bivector must have power of two dimensions for this!
    bivector<T>& add_tiled(const bivector<T>& other, const T& scal);

    ///@note other bivector must have power of two dimensions for this!
    bivector<T>& add_shifted(const bivector<T>& other, const vector2i& offset);

    bivector<T>& insert(const bivector<T>& other, const vector2i& offset);

    template<class U>
    friend class bivector;

    // functions to allow range based for loops over all members
    auto begin() { return data.begin(); }
    auto end() { return data.end(); }
    auto begin() const { return data.begin(); }
    auto end() const { return data.end(); }

    /// for each value given xy position and absolute offset do sth
    template<typename Func>
    void for_each_xy(Func func) const
    {
        int z = 0;
        for (int y = 0; y < datasize.y; ++y) {
            for (int x = 0; x < datasize.x; ++x, ++z) {
                func(data[z], vector2i(x, y), z);
            }
        }
    }
    /// for each value given xy position and absolute offset do sth const
    template<typename Func>
    void for_each_xy(Func func)
    {
        int z = 0;
        for (int y = 0; y < datasize.y; ++y) {
            for (int x = 0; x < datasize.x; ++x, ++z) {
                func(data[z], vector2i(x, y), z);
            }
        }
    }
};

template<class T>
void bivector<T>::resize(const vector2i& newsz, const T& v)
{
    if (newsz.x < 0 || newsz.y < 0) {
        throw std::invalid_argument("bivector resize must not be negative");
    }
    std::vector<T> new_data(newsz.x * newsz.y);
    auto limit = datasize.min(newsz);
    for (int y = 0; y < limit.y; ++y) {
        for (int x = 0; x < limit.x; ++x) {
            new_data[newsz.x * y + x] = data[datasize.x * y + x];
        }
    }
    data     = std::move(new_data);
    datasize = newsz;
}

template<class T>
T bivector<T>::get_min() const
{
    if (data.empty())
        throw std::invalid_argument("bivector::get_min data empty");
    T m = data[0];
    for (auto v : data)
        m = std::min(m, v);
    return m;
}

template<class T>
T bivector<T>::get_max() const
{
    if (data.empty())
        throw std::invalid_argument("bivector::get_max data empty");
    T m = data[0];
    for (auto v : data)
        m = std::max(m, v);
    return m;
}

template<class T>
T bivector<T>::get_min_abs() const
{
    if (data.empty())
        throw std::invalid_argument("bivector::get_min_abs data empty");
    T m = std::abs(data[0]);
    for (auto v : data)
        m = std::min(m, std::abs(v));
    return m;
}

template<class T>
T bivector<T>::get_max_abs() const
{
    if (data.empty())
        throw std::invalid_argument("bivector::get_max_abs data empty");
    T m = std::abs(data[0]);
    for (auto v : data)
        m = std::max(m, std::abs(v));
    return m;
}

template<class T>
bivector<T>& bivector<T>::operator*=(const T& s)
{
    for (auto& v : data)
        v *= s;
    return *this;
}

template<class T>
bivector<T>& bivector<T>::operator+=(const T& a)
{
    for (auto& v : data)
        v += a;
    return *this;
}

template<class T>
bivector<T>& bivector<T>::operator+=(const bivector<T>& v)
{
    unsigned z = 0;
    for (auto& e : data)
        e += v[z++];
    return *this;
}

template<class T>
bivector<T> bivector<T>::sub_area(const vector2i& offset, const vector2i& sz) const
{
    if (offset.y + sz.y > datasize.y)
        throw std::invalid_argument("bivector::sub_area, offset.y invalid");
    if (offset.x + sz.x > datasize.x)
        throw std::invalid_argument("bivector::sub_area, offset.x invalid");
    bivector<T> result(sz);
    result.for_each_xy([&offset, this](auto& value, vector2i xy, unsigned) { value = (*this)[offset + xy]; });
    return result;
}

template<class T>
bivector<T> bivector<T>::shifted(const vector2i& offset) const
{
    bivector<T> result(datasize);
    for_each_xy([&](const auto& value, vector2i xy, unsigned z) {
        result[vector2i((xy.x + offset.x) & (datasize.x - 1), (xy.y + offset.y) & (datasize.y - 1))] = value;
    });
    return result;
}

template<class T>
bivector<T> bivector<T>::transposed() const
{
    bivector<T> result(vector2i(datasize.y, datasize.x));
    for_each_xy([&](const auto& value, vector2i xy, unsigned z) { result[vector2i(xy.y, xy.x)] = value; });
    return result;
}

template<class T>
template<class U>
bivector<U> bivector<T>::convert() const
{
    bivector<U> result(datasize);
    unsigned z = 0;
    for (auto v : data)
        result.data[z++] = U(v);
    return result;
}

template<class T>
template<class U>
bivector<U> bivector<T>::convert(const T& minv, const T& maxv) const
{
    bivector<U> result(datasize);
    unsigned z = 0;
    for (auto v : data)
        result.data[z++] = U(std::max(minv, std::min(maxv, v)));
    return result;
}

template<class T>
bivector<T> bivector<T>::upsampled(bool wrap) const
{
    /* upsampling generates 3 new values out of the 4 surrounding
       values like this: (x - surrounding values, numbers: generated)
       x1x
       23-
       x-x
       So 1x1 pixels are upsampled to 2x2 using the neighbourhood.
       This means we can't generate samples beyond the last column/row,
       thus n+1 samples generate 2n+1 resulting samples.
       With wrapping we can compute one more sample.
       So we have:
       n+1 -> 2n+1
       n   -> 2n    with wrapping
    */
    if (datasize.x < 1 || datasize.y < 1)
        throw std::invalid_argument("bivector::upsampled base size invalid");
    const auto resultsize = wrap ? datasize * 2 : datasize * 2 - vector2i(1, 1);
    bivector<T> result(resultsize);
    // copy values that are kept and interpolate missing values on even rows
    for (int y = 0; y < datasize.y; ++y) {
        for (int x = 0; x < datasize.x - 1; ++x) {
            result.at(2 * x, 2 * y)     = at(x, y);
            result.at(2 * x + 1, 2 * y) = T((at(x, y) + at(x + 1, y)) * 0.5);
        }
    }
    // handle special cases on last column
    if (wrap) {
        // copy/interpolate with wrap
        for (int y = 0; y < datasize.y; ++y) {
            result.at(2 * datasize.x - 2, 2 * y) = at(datasize.x - 1, y);
            result.at(2 * datasize.x - 1, 2 * y) = T((at(datasize.x - 1, y) + at(0, y)) * 0.5);
        }
    } else {
        // copy last column
        for (int y = 0; y < datasize.y; ++y) {
            result.at(2 * datasize.x - 2, 2 * y) = at(datasize.x - 1, y);
        }
    }
    // interpolate missing values on odd rows
    for (int y = 0; y < datasize.y - 1; ++y) {
        for (int x = 0; x < resultsize.x; ++x) {
            result.at(x, 2 * y + 1) = T((result.at(x, 2 * y) + result.at(x, 2 * y + 2)) * 0.5);
        }
    }
    // handle special cases on last row
    if (wrap) {
        // interpolate last row with first and second-to-last
        for (int x = 0; x < resultsize.x; ++x) {
            result.at(x, resultsize.y - 1) = T((result.at(x, resultsize.y - 2) + result.at(x, 0)) * 0.5);
        }
    }
    return result;
}

template<class T>
bivector<T> bivector<T>::downsampled(bool force_even_size) const
{
    /* downsampling builds the average of 2x2 pixels.
       if "force_even_size" is false:
       If the width/height is odd the last column/row is
       handled specially, here 1x2 or 2x1 pixels are averaged.
       If width and height are odd, the last pixel is kept,
       it can't be averaged.
       We could add wrapping for the odd case here though...
       if "force_even_size" is true:
       If the width/height is odd the last column/row is
       skipped and the remaining data averaged.
    */
    vector2i newsize(datasize.x >> 1, datasize.y >> 1);
    auto resultsize = newsize;
    if (!force_even_size) {
        resultsize.x += datasize.x & 1;
        resultsize.y += datasize.x & 1;
    }
    bivector<T> result(resultsize);
    for (int y = 0; y < newsize.y; ++y)
        for (int x = 0; x < newsize.x; ++x)
            result.at(x, y) =
                T((at(2 * x, 2 * y) + at(2 * x + 1, 2 * y) + at(2 * x, 2 * y + 1) + at(2 * x + 1, 2 * y + 1)) * 0.25);
    if (!force_even_size) {
        // downsample last row or column if original size is odd
        if (datasize.x & 1) {
            for (int y = 0; y < newsize.y; ++y)
                result.at(newsize.x, y) = T((at(datasize.x - 1, 2 * y) + at(datasize.x - 1, 2 * y + 1)) * 0.5);
        }
        if (datasize.y & 1) {
            for (int x = 0; x < newsize.x; ++x)
                result.at(x, newsize.y) = T((at(2 * x, datasize.y - 1) + at(2 * x + 1, datasize.y - 1)) * 0.5);
        }
        if ((datasize.x & datasize.y) & 1) {
            // copy corner, hasn't been handled yet
            result.at(newsize.x, newsize.y) = at(datasize.x - 1, datasize.y - 1);
        }
    }
    return result;
}

template<class T>
bivector<T> bivector<T>::smooth_upsampled(bool wrap) const
{
    /* interpolate one new value out of four neighbours,
       or out of 4x4 neighbours with a coefficient matrix:
       -1/16 9/16 9/16 -1/16 along one axis,
    */
    static const float c1[4] = {-1.0f / 16, 9.0f / 16, 9.0f / 16, -1.0f / 16};
    /* upsampling generates 3 new values out of the 16 surrounding
       values like this: (x - surrounding values, numbers: generated)
       x-x-x-x
       -------
       x-x1x-x
       --23---
       x-x-x-x
       -------
       x-x-x-x
       So 1x1 pixels are upsampled to 2x2 using the neighbourhood.
       This means we can't generate samples beyond the last two columns/rows,
       and before the second column/row.
       thus n+3 samples generate 2n+1 resulting samples (4->3, 5->5, 6->7, 7->9,
       ...) With wrap we can get 2n samples out of n. So we have: n+3  -> 2n+1
       n    -> 2n    with wrapping
    */
    if (datasize.x < 3 || datasize.y < 3)
        throw std::invalid_argument("bivector::smooth_upsampled base size invalid");
    const auto resultsize = wrap ? datasize * 2 : datasize * 2 - vector2i(1, 1);
    bivector<T> result(resultsize);
    // copy values that are kept and interpolate missing values on even rows
    for (int y = 0; y < datasize.y; ++y) {
        result.at(0, 2 * y) = at(0, y);
        for (int x = 1; x < datasize.x - 2; ++x) {
            result.at(2 * x, 2 * y) = at(x, y);
            result.at(2 * x + 1, 2 * y) =
                T(at(x - 1, y) * c1[0] + at(x, y) * c1[1] + at(x + 1, y) * c1[2] + at(x + 2, y) * c1[3]);
        }
        result.at(2 * datasize.x - 4, 2 * y) = at(datasize.x - 2, y);
        result.at(2 * datasize.x - 2, 2 * y) = at(datasize.x - 1, y);
    }
    if (wrap) {
        for (int y = 0; y < datasize.y; ++y) {
            result.at(1, 2 * y) =
                T(at(datasize.x - 1, y) * c1[0] + at(0, y) * c1[1] + at(1, y) * c1[2] + at(2, y) * c1[3]);
            result.at(2 * datasize.x - 3, 2 * y) =
                T(at(datasize.x - 3, y) * c1[0] + at(datasize.x - 2, y) * c1[1] + at(datasize.x - 1, y) * c1[2]
                  + at(0, y) * c1[3]);
            result.at(2 * datasize.x - 1, 2 * y) =
                T(at(datasize.x - 2, y) * c1[0] + at(datasize.x - 1, y) * c1[1] + at(0, y) * c1[2] + at(1, y) * c1[3]);
        }
    } else {
        for (int y = 0; y < datasize.y; ++y) {
            result.at(1, 2 * y) = T(at(0, y) * c1[0] + at(0, y) * c1[1] + at(1, y) * c1[2] + at(2, y) * c1[3]);
            result.at(2 * datasize.x - 3, 2 * y) =
                T(at(datasize.x - 3, y) * c1[0] + at(datasize.x - 2, y) * c1[1] + at(datasize.x - 1, y) * c1[2]
                  + at(datasize.x - 1, y) * c1[3]);
        }
    }
    // interpolate missing values on odd rows
    for (int y = 1; y < datasize.y - 2; ++y) {
        for (int x = 0; x < resultsize.x; ++x) {
            result.at(x, 2 * y + 1) =
                T(result.at(x, 2 * y - 2) * c1[0] + result.at(x, 2 * y) * c1[1] + result.at(x, 2 * y + 2) * c1[2]
                  + result.at(x, 2 * y + 4) * c1[3]);
        }
    }
    // handle special cases on last row
    if (wrap) {
        // interpolate 3 missing rows
        for (int x = 0; x < resultsize.x; ++x) {
            result.at(x, 1) =
                T(result.at(x, 2 * datasize.y - 2) * c1[0] + result.at(x, 0) * c1[1] + result.at(x, 2) * c1[2]
                  + result.at(x, 4) * c1[3]);
            result.at(x, 2 * datasize.y - 3) =
                T(result.at(x, 2 * datasize.y - 6) * c1[0] + result.at(x, 2 * datasize.y - 4) * c1[1]
                  + result.at(x, 2 * datasize.y - 2) * c1[2] + result.at(x, 0) * c1[3]);
            result.at(x, 2 * datasize.y - 1) =
                T(result.at(x, 2 * datasize.y - 4) * c1[0] + result.at(x, 2 * datasize.y - 2) * c1[1]
                  + result.at(x, 0) * c1[2] + result.at(x, 2) * c1[3]);
        }
    } else {
        for (int x = 0; x < resultsize.x; ++x) {
            result.at(x, 1) = T(
                result.at(x, 0) * c1[0] + result.at(x, 0) * c1[1] + result.at(x, 2) * c1[2] + result.at(x, 4) * c1[3]);
            result.at(x, 2 * datasize.y - 3) =
                T(result.at(x, 2 * datasize.y - 6) * c1[0] + result.at(x, 2 * datasize.y - 4) * c1[1]
                  + result.at(x, 2 * datasize.y - 2) * c1[2] + result.at(x, 2 * datasize.y - 2) * c1[3]);
        }
    }
    return result;
}

#if 0
template <class T>
bivector<T>& bivector<T>::add_gauss_noise(const T& scal, random_generator& rg)
{
    for (auto & elem : data) {
        double r, q, s;
        do {
            r=rg.get()*2-1;
            s=r*constants::PI;
            s=exp(-0.5*s*s);
            q=rg.get();
        } while (q > s);
        elem += T(r * scal);
    }
    //bivector_FOREACH(data[z] += T((rg.get()*2.0-1.0) * scal))
    return *this;
}
#endif

template<class T>
bivector<T>& bivector<T>::add_tiled(const bivector<T>& other, const T& scal)
{
    for_each_xy([&](auto& value, vector2i xy, unsigned) {
        value += other.at(xy.x & (other.datasize.x - 1), xy.y & (other.datasize.y - 1)) * scal;
    });
    return *this;
}

template<class T>
bivector<T>& bivector<T>::add_shifted(const bivector<T>& other, const vector2i& offset)
{
    for_each_xy([&](auto& value, vector2i xy, unsigned) {
        value += other.at((xy.x + offset.x) & (other.datasize.x - 1), (xy.y + offset.y) & (other.datasize.y - 1));
    });
    return *this;
}

template<class T>
bivector<T>& bivector<T>::insert(const bivector<T>& other, const vector2i& offset)
{
    other.for_each_xy([this, &offset](const auto& value, vector2i xy, unsigned) { (*this)[offset + xy] = value; });
    return *this;
}
