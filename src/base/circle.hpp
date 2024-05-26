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
//  A 2d circle (C)+(W) 2009 Thorsten Jordan
//

#pragma once

#include "vector3.hpp"

#include <vector>

/// a 2d circle
struct circle
{
    vector2f center;
    float radius_sqr;
    circle()
        : radius_sqr(0.f)
    {
    }
    circle(const vector2f& c, float rs)
        : center(c)
        , radius_sqr(rs)
    {
    }
};

inline circle get_circle(const std::vector<vector3f>& pos, int v0, int v1, int v2)
{
    // this code works, but may be too simple, other implementations are more
    // complex, for example Paul Bourke tests for the case that b-a or c-a is
    // null on the y value.
    // If we use the test_graph program with many nodes (>=2000) an error is
    // triggered in adjacency computation that may be a result of false checks
    // here!
    vector2f a = pos[v0].xy();
    vector2f b = pos[v1].xy();
    vector2f c = pos[v2].xy();

    vector2f bao = (b - a).orthogonal();
    vector2f cbo = (c - b).orthogonal();

    float s = 0.f, t = 0.f;
    ((c - a) * 0.5f).solve(bao, cbo, s, t);

    vector2f ct = ((a + b) * 0.5f) + bao * s;
    return circle(ct, ct.square_distance(a));
}

inline bool is_inside_circle(const vector3f& pos, const circle& c)
{
    static const float epsilon = 1e-5f;
    vector2f d                 = pos.xy() - c.center;
    return d.square_length() <= c.radius_sqr + epsilon;
}
