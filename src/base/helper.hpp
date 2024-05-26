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

// common helper functions
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <sstream>

// Functions to convert between data types
template<typename D, typename S>
inline void convert_pm1(D& d, S s)
{
    d = D(s);
}

template<typename D>
inline void convert_pm1(D& d, uint8_t s)
{
    d = D(D(s) - D(128)) / D(127);
}

template<typename S>
inline void convert_pm1(uint8_t& d, S s)
{
    d = uint8_t(s * S(127) + S(128));
}

template<>
inline void convert_pm1<uint8_t, uint8_t>(uint8_t& d, uint8_t s)
{
    d = s;
}

template<typename D, typename S>
inline void convert_01(D& d, S s)
{
    d = D(s);
}

template<typename D>
inline void convert_01(D& d, uint8_t s)
{
    d = D(s) / D(255);
}

template<typename S>
inline void convert_01(uint8_t& d, S s)
{
    d = uint8_t(s * S(255));
}

template<>
inline void convert_01<uint8_t, uint8_t>(uint8_t& d, uint8_t s)
{
    d = s;
}

// handle modulo calculation for negative values the way I need it
inline float myfrac(float a)
{
    return a - floorf(a);
}

inline double myfrac(double a)
{
    return a - floor(a);
}

/// helper functions
namespace helper
{
/// interpolate two values linearily
template<typename T, typename S>
T interpolate(const T& a, const T& b, const S& v)
{
    return T(a * (S(1) - v) + b * v);
}

/// check if value is power of two
inline bool is_power2(unsigned x)
{
    return (x & (x - 1)) == 0;
}

/// compare with tolerance value
template<typename T>
bool is_equal_with_tolerance(const T a, const T b, const T tolerance)
{
    return std::abs(a - b) <= tolerance;
}

/// compare with tolerance value
template<typename T>
bool is_zero_with_tolerance(const T a, const T tolerance)
{
    return std::abs(a) <= tolerance;
}

/// mathematic modulo, works also with negative values, fmod does not
template<typename T>
T mod(const T a, const T b)
{
    return a - std::floor(a / b) * b;
}

/// return fractional part
template<typename T>
T frac(const T a)
{
    return a - std::floor(a);
}

/// return sign of value, -1, 0, 1
template<typename T>
T sgn(const T a)
{
    return a < T(0) ? -T(1) : (a > T(0) ? T(1) : T(0));
}

/// add to value with saturation
template<typename T>
void add_saturated(T& sum, const T add, const T maxv)
{
    sum = std::min(sum + add, maxv);
}

/// round value
template<typename T>
T round(const T v)
{
    return std::floor(v + T(0.5));
}

/// for each element that a predicate returns true execute a function
template<class C, class P, class F>
void for_each_if_do(C& cnt, P predicate, F function)
{
    for (auto& item : cnt) {
        if (predicate(item)) {
            function(item);
        }
    }
}

/// Check if element is in container
template<class C, typename T>
bool contains(const C& cnt, const T& value)
{
    return std::find(begin(cnt), end(cnt), value) != end(cnt);
}

/// remove all values for that predicate returns true and compacts the
/// container.
template<class C, class P>
void erase_remove_if(C& cnt, P predicate)
{
    cnt.erase(std::remove_if(std::begin(cnt), std::end(cnt), predicate), std::end(cnt));
}

/// Convert to string
template<typename T>
std::string str(const T& v)
{
    std::ostringstream oss;
    oss << v;
    return oss.str();
}

/// Check if string ends with another string like python's endswith()
inline bool endswith(const std::string& s, const std::string& suffix)
{
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

/// Unsigned logarithm base 2
inline unsigned ulog2(unsigned x)
{
    unsigned i = 0;
    for (; x > 0; ++i, x >>= 1)
        ;
    return i - 1;
}

/// int logarithm base 2
inline int log2(int x)
{
    int i = 0;
    for (; x > 0; ++i, x >>= 1)
        ;
    return i - 1;
}

/// any_of with range
template<class C, class P>
bool any_of(C& cnt, P predicate)
{
    return std::any_of(begin(cnt), end(cnt), predicate);
}

/// any_of with range
template<class C, class P>
bool all_of(C& cnt, P predicate)
{
    return std::all_of(begin(cnt), end(cnt), predicate);
}

/// any_of with range
template<class C, class P>
bool none_of(C& cnt, P predicate)
{
    return std::none_of(begin(cnt), end(cnt), predicate);
}

/// count and call for every number
template<typename T>
void count(const T start, const T limit, std::function<void(const T)> func)
{
    for (auto n = start; n != limit; n = T(unsigned(n) + 1)) {
        func(n);
    }
}

/// count and call for every number
template<typename T>
void count(const T number, std::function<void(const T)> func)
{
    count(T{}, number, func);
}

/// find object from range with least value (distance)
template<template<typename, typename...> class C, typename D, typename T>
T nearest(const C<T>& cnt, std::function<D(const T&)> dist)
{
    auto it = std::begin(cnt);
    if (it == std::end(cnt))
        return T();
    const auto it_result    = it;
    const auto compare_dist = dist(*it);
    for (++it; it != std::end(cnt); ++it) {
        const auto next_dist = dist(*it);
        if (next_dist < compare_dist) {
            compare_dist = next_dist;
            it_result    = it;
        }
    }
    return *it_result;
}

/// Replace substring and return replaced
inline std::string replace_first(std::string s, const std::string& pattern, const std::string& replace_with)
{
    auto pos = s.find(pattern);
    if (pos != std::string::npos) {
        s.replace(pos, pattern.length(), replace_with);
    }
    return s;
}

/// Knots to meters per second
template<typename T>
T kts2ms(T knots)
{
    return T(knots * 1852.0 / 3600.0);
}
/// meters per second to knots
template<typename T>
T ms2kts(T meters)
{
    return T(meters * 3600.0 / 1852.0);
}
/// kilometers per hour to knots
template<typename T>
T kmh2ms(T kmh)
{
    return T(kmh / 3.6);
}
/// Knots to kilometers per hour
template<typename T>
T ms2kmh(T meters)
{
    return T(meters * 3.6);
}
} // namespace helper
