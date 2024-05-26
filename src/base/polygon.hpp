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
//  A generic polygon (C)+(W) 2005 Thorsten Jordan
//

#pragma once

#include "error.hpp"
#include "matrix4.hpp"
#include "plane.hpp"

#include <iostream>
#include <vector>

/// a polygon in 3D space
template<typename D>
class polygon_t
{
  public:
    /// empty polygon
    polygon_t() = default;

    /// polygon with prepared space
    polygon_t(unsigned capacity_) { points.reserve(capacity_); }

    /// make from three points.
    polygon_t(const vector3t<D>& a, const vector3t<D>& b, const vector3t<D>& c)
        : points{a, b, c}
    {
    }

    /// make from four points
    polygon_t(const vector3t<D>& a, const vector3t<D>& b, const vector3t<D>& c, const vector3t<D>& d)
        : points{a, b, c, d}
    {
    }

    /// construct from other type
    template<typename E>
    polygon_t(const polygon_t<E>& other)
        : points(other.points.begin(), other.points.end())
    {
    }

    /// check if polygon is empty. Each polygon must have at least three
    /// vertices or it is empty
    [[nodiscard]] bool empty() const { return points.size() < 3; }

    /// Add new point
    void add_point(const vector3t<D>& p) { points.push_back(p); }

    /// return number of points
    [[nodiscard]] unsigned nr_of_points() const { return unsigned(points.size()); }

    /// Get next index from this
    [[nodiscard]] unsigned next_index(unsigned i) const
    {
        ++i;
        if (i >= nr_of_points())
            i = 0;
        return i;
    }

    /// Get previous index from this
    [[nodiscard]] unsigned prev_index(unsigned i) const
    {
        if (i == 0)
            i = nr_of_points();
        --i;
        return i;
    }

    /// compute normal of polygon. Only valid if all points are in a plane.
    [[nodiscard]] vector3t<D> normal() const
    {
        if (empty())
            return vector3t<D>();
        return (points[1] - points[0]).cross(points[2] - points[0]).normal();
    }

    /// Parameter type for returning front
    struct clip_result_front
    {
        polygon_t result;
        void set_front(const polygon_t& p) { result = p; }
        void add_front(const vector3t<D>& p) { result.add_point(p); }
        void add_back(const vector3t<D>& p) { }
    };

    /// Parameter type for returning front and back
    struct clip_result_front_and_back
    {
        std::pair<polygon_t, polygon_t> result;
        void set_front(const polygon_t& p) { result.first = p; }
        void add_front(const vector3t<D>& p) { result.first.add_point(p); }
        void add_back(const vector3t<D>& p) { result.second.add_point(p); }
    };

    /// Parameter type for returning clip points
    struct clip_point_result
    {
        vector3t<D> points[2];
        unsigned counter{0};
        clip_point_result() { }
        void set(unsigned index, const vector3t<D>& p)
        {
            points[index] = p;
            ++counter;
        }
    };

    /// Parameter type for NOT returning clip points
    struct clip_point_no_result
    {
        void set(unsigned /*index*/, const vector3t<D>& /*p*/) { }
    };

    /// Parameter type for generic planes
    struct plane_type_generic
    {
        plane_type_generic(const plane_t<D>& p)
            : pln(p)
        {
        }
        const plane_t<D>& pln;
        D distance(const vector3t<D>& p) { return pln.distance(p); }
        vector3t<D> intersection(const vector3t<D>& a, const vector3t<D>& b) { return pln.intersection(a, b); }
    };

    /// Parameter type for axis aligned planes
    template<axis A>
    struct plane_type_axis
    {
        plane_type_axis(D av = D(0))
            : axis_value(av)
        {
        }
        D axis_value;
        D distance(const vector3t<D>& p) { return plane_distance<D, A>(p, axis_value); }
        vector3t<D> intersection(const vector3t<D>& a, const vector3t<D>& b)
        {
            return plane_intersection<D, A>(a, b, axis_value);
        }
    };

    /// Clip polygon with plane, template arguments to return only front side or
    /// also back side, to use special plane and to get also cut points. This is
    /// some template overkill, but otherwise we would have the same function in
    /// many variants with much code duplication.
    template<typename result_type, typename cut_point_type, typename plane_type>
    bool clip_generic(result_type& rt, cut_point_type& cpt, plane_type& pt) const
    {
        // construct one or two resulting polygons.
        // iterate over points (edges), if a point is left or right, assign to
        // result polygon respectivly. If point is on different side than
        // previous point, compute intersection and insert it into both results.
        // Avoid to generate very short edges, so if points are very close to
        // the plane use them as intersection points. However points on the
        // plane should not be counted for side switching, a polygon touching
        // the plane is just kept as it is. If first point is on plane we have
        // to go back until we know which side it belongs to.
        if (empty()) {
            return false;
        }

        const D epsilon           = D(0.001);
        D last_point_distance     = pt.distance(points.back());
        unsigned last_point_index = nr_of_points() - 1;

        int last_point_side = (last_point_distance > epsilon) ? 1 : (last_point_distance < -epsilon ? -1 : 0);

        int current_side = last_point_side;

        if (current_side == 0) {
            // find a definitive side
            for (unsigned i = last_point_index; i > 0 && current_side == 0; --i) {
                D this_point_distance = pt.distance(points[i - 1]);
                current_side          = (this_point_distance > epsilon) ? 1 : (this_point_distance < -epsilon ? -1 : 0);
            }

            if (current_side == 0) {
                // if all points are on the plane, so just keep the polygon
                rt.set_front(*this);
                return true;
            }
        }

        for (unsigned i = 0; i < nr_of_points(); ++i) {
            D this_point_distance = pt.distance(points[i]);

            int this_point_side = (this_point_distance > epsilon) ? 1 : (this_point_distance < -epsilon ? -1 : 0);

            if (this_point_side > 0) {
                if (last_point_side < 0) {
                    // compute intersection
                    vector3t<D> its = pt.intersection(points[last_point_index], points[i]);
                    // We entered the clip space, so we have second cut point
                    cpt.set(1, its);
                    rt.add_back(its);
                    rt.add_front(its);
                } else if (last_point_side == 0) {
                    if (this_point_side != current_side) {
                        rt.add_front(points[last_point_index]);
                    }
                }
                // last point was front or on plane or intersection, just add to
                // front
                rt.add_front(points[i]);
                current_side = this_point_side;
            } else if (this_point_side < 0) {
                if (last_point_side > 0) {
                    // compute intersection
                    vector3t<D> its = pt.intersection(points[last_point_index], points[i]);
                    // We left the clip space, so we have first cut point
                    cpt.set(0, its);
                    rt.add_front(its);
                    rt.add_back(its);
                } else if (last_point_side == 0) {
                    if (this_point_side != current_side) {
                        rt.add_back(points[last_point_index]);
                    }
                }
                // last point was back or on plane or intersection, just add to
                // back
                rt.add_back(points[i]);
                current_side = this_point_side;
            } else {
                // this point is on plane, check which side to put it to
                if (current_side > 0) {
                    rt.add_front(points[i]);
                } else { // current_side is never 0
                    rt.add_back(points[i]);
                }
                // Point is on clip plane, if previous point as also on plane,
                // we have second cut point, else first
                cpt.set(last_point_side == 0 ? 1 : 0, points[i]);
            }
            last_point_index = i;
            last_point_side  = this_point_side;
        }
        return false;
    }

    /// clip (cut off) polygon against plane. Works only for convex polygons!
    [[nodiscard]] polygon_t cut(const plane_t<D>& plan) const
    {
        clip_result_front crf;
        clip_point_no_result cpnr;
        plane_type_generic pt(plan);
        clip_generic(crf, cpnr, pt);
        return crf.result;
    }

    /// Clip convex polygon by plane with frontside/backside result
    [[nodiscard]] std::pair<polygon_t, polygon_t> clip(const plane_t<D>& plan) const
    {
        clip_result_front_and_back crfb;
        clip_point_no_result cpnr;
        plane_type_generic pt(plan);
        clip_generic(crfb, cpnr, pt);
        return crfb.result;
    }

    /// Clip with axis aligned plane with frontside/backside result
    template<axis a>
    [[nodiscard]] std::pair<polygon_t, polygon_t> clip(D axis_value) const
    {
        clip_result_front_and_back crfb;
        clip_point_no_result cpnr;
        plane_type_axis<a> pta(axis_value);
        clip_generic(crfb, cpnr, pta);
        return crfb.result;
    }

    /// Clip with axis aligned plane with frontside/backside result
    template<axis a>
    std::pair<polygon_t, polygon_t> clip(plane_type_axis<a> pta) const
    {
        clip_result_front_and_back crfb;
        clip_point_no_result cpnr;
        clip_generic(crfb, cpnr, pta);
        return crfb.result;
    }

    /// print polygon (maybe offer << operator!)
    void print() const
    {
        std::cout << "Poly, pts=" << points.size() << "\n";
        for (unsigned i = 0; i < points.size(); ++i)
            std::cout << "P[" << i << "] = " << points[i] << "\n";
    }

    /// compute plane that poly lies in
    [[nodiscard]] plane_t<D> get_plane() const
    {
        if (empty())
            return plane_t<D>();
        return plane_t<D>(points[0], points[1], points[2]);
    }

    /// translate
    void translate(const vector3t<D>& delta)
    {
        for (unsigned i = 0; i < points.size(); ++i)
            points[i] += delta;
    }

    /// Check if a point is inside the polygon from a given direction. Here we
    /// assume z axis as direction. determine number of intersections of polygon
    /// with one axis going out of the point (e.g. X axis), even number is
    /// outside
    [[nodiscard]] bool is_inside(const vector2t<D>& point) const
    {
        if (empty())
            return false;
        D last_delta_y             = points.back().y - point.y;
        bool last_point_above      = last_delta_y > 0.0;
        unsigned last_index        = unsigned(points.size()) - 1;
        unsigned num_intersections = 0;

        // Check all edges for intersection of X axis going through the point
        for (unsigned i = 0; i < unsigned(points.size()); ++i) {
            D delta_y        = points[i].y - point.y;
            bool point_above = delta_y > 0.0;

            // std::cout<<"i="<<i<<" ldy"<<last_delta_y<<" dy"<<delta_y<<"\n";
            if (last_point_above != point_above) {
                // edge intersects X axis, determine where, if its right of the
                // point
                D t = delta_y / (delta_y - last_delta_y);
                D x = points[last_index].x * t + points[i].x * (D(1) - t);
                num_intersections += (x > point.x) ? 1 : 0;

                // std::cout<<"t="<<t<<" x="<<x<<" numi
                // "<<num_intersections<<"\n";//fixme seems ok, but needs more
                // testing
            }
        }
        return (num_intersections & 1);
    }

    /// Compute unnormalized normal of a special polygon, a triangle
    static vector3t<D> compute_normal(const vector3t<D>& a, const vector3t<D>& b, const vector3t<D>& c)
    {
        return (b - a).cross(c - a);
    }

    /// Clip a vector of polygons by one plane and fetch results to two other
    /// vectors
    template<axis a>
    static void
    clip(const std::vector<polygon_t>& src, std::vector<polygon_t>& front, std::vector<polygon_t>& back, D axis_value)
    {
        plane_type_axis<a> pta(axis_value);

        for (const auto& source_polygon : src) {
            auto front_and_back = source_polygon.clip(pta);

            if (!front_and_back.first.empty())
                front.push_back(std::move(front_and_back.first));

            if (!front_and_back.second.empty())
                back.push_back(std::move(front_and_back.second));
        }
    }
    /// Swap with other polygon
    void swap(polygon_t<D>& other) { points.swap(other.points); }

  public:
    std::vector<vector3t<D>> points; ///< The points in 3-space forming the polygon
};

using polygon  = polygon_t<double>;
using polygonf = polygon_t<float>;

#if 0                                                            // Test code for polygon clipping!
{
	// check special cases
#define A(x, y) p.add_point(vector3f(float(x), float(y), 0.0f)); // initializer list ctor is better
	std::cout << "case 0\n";
	planef pl(vector3f(0.f,-1.f,0.f), vector3f(0.f,2.f,0.f));
	{
		polygonf p;
		A(0,0);
		A(2,0);
		A(2,2);
		polygonf p1;
		polygonf p0 = p.clip(pl, &p1);
		p0.print();
		p1.print();
	}
	std::cout << "case 1\n";
	{
		polygonf p;
		A(0,0);
		A(4,0);
		A(4,2);
		A(2,2);
		polygonf p1;
		polygonf p0 = p.clip(pl, &p1);
		p0.print();
		p1.print();
	}
	std::cout << "case 2\n";
	{
		polygonf p;
		A(0,0);
		A(6,0);
		A(6,2);
		A(4,2);
		A(2,2);
		polygonf p1;
		polygonf p0 = p.clip(pl, &p1);
		p0.print();
		p1.print();
	}
	std::cout << "case 3\n";
	{
		polygonf p;
		A(0,0);
		A(4,0);
		A(4,2);
		A(4,4);
		A(2,2);
		polygonf p1;
		polygonf p0 = p.clip(pl, &p1);
		p0.print();
		p1.print();
	}
	std::cout << "case 4\n";
	{
		polygonf p;
		A(0,0);
		A(4,0);
		A(4,4);
		A(2,2);
		polygonf p1;
		polygonf p0 = p.clip(pl, &p1);
		p0.print();
		p1.print();
	}
	std::cout << "case 5\n";
	{
		polygonf p;
		A(0,0);
		A(4,0);
		A(4,4);
		polygonf p1;
		polygonf p0 = p.clip(pl, &p1);
		p0.print();
		p1.print();
	}
}
#endif
