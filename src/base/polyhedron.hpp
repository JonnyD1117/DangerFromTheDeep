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
//  A generic polyhedron (C)+(W) 2020 Thorsten Jordan
//

#pragma once

#include "error.hpp"
#include "matrix4.hpp"
#include "plane.hpp"
#include "polygon.hpp"

#include <iostream>
#include <vector>

/// a polyhedron in 3D space (closed body!)
template<typename D>
class polyhedron_t
{
  public:
    /// One (half) edge of a polygonal side
    struct half_edge
    {
        uint32_t point_nr;
        uint32_t neighbor_side;
        half_edge(uint32_t pn, uint32_t ns)
            : point_nr(pn)
            , neighbor_side(ns)
        {
        }

        static const uint32_t no_neighbor = uint32_t(-1);
    };

    /// Class for a polyhedron side (polygon)
    struct side
    {
        std::vector<half_edge> edges; ///< edges forming the side

        /// Default ctor
        side() { }

        /// Construct from half edges
        side(std::vector<half_edge>&& e)
            : edges(e)
        {
        }

        /// Get next edge index
        uint32_t next_edge(uint32_t edge_index) const
        {
            ++edge_index;
            if (edge_index >= edges.size())
                edge_index = 0;
            return edge_index;
        }

        /// Get previous edge index
        uint32_t prev_edge(uint32_t edge_index) const
        {
            if (edge_index == 0)
                edge_index = unsigned(edges.size());
            --edge_index;
            return edge_index;
        }

        /// Get edge index from one side to another
        uint32_t get_edge_index(uint32_t side_index) const
        {
            uint32_t edge_index = 0;
            for (const auto& e : edges) {
                if (e.neighbor_side == side_index) {
                    return edge_index;
                }
                ++edge_index;
            }
            return half_edge::no_neighbor;
        }

        /// Split edge and insert point
        void split_edge(unsigned edge_index, unsigned point_index)
        {
            edges.insert(edges.begin() + edge_index + 1, half_edge(point_index, edges[edge_index].neighbor_side));
        }
    };

    /// empty polyhedron
    polyhedron_t() { }

    /// get number of sides
    unsigned nr_of_sides() const { return unsigned(sides.size()); }

    /// Check validity of polyhedron
    bool check() const
    {
        // We need at least 4 points and sides
        if (points.size() < 4 || sides.size() < 4)
            return false;

        // Every side needs to have set neighbors and points must match
        for (unsigned side_index = 0; side_index != sides.size(); ++side_index) {
            const auto& s = sides[side_index];

            // Each side needs at least three edges.
            if (s.edges.size() < 3)
                return false;

            // Check that point indices are unique
            std::vector<bool> point_referenced(points.size());

            for (const auto& e : s.edges) {
                if (e.point_nr >= points.size())
                    return false;

                if (point_referenced[e.point_nr])
                    return false;

                point_referenced[e.point_nr] = true;
            }

            // Check all neighbors for validity
            std::vector<bool> sides_referenced(sides.size());

            for (unsigned edge_index = 0; edge_index != s.edges.size(); ++edge_index) {
                auto neighbor_side = s.edges[edge_index].neighbor_side;

                // reference must be valid, not to self side and not twice to
                // same side
                if (neighbor_side == half_edge::no_neighbor || neighbor_side == side_index)
                    return false;

                if (sides_referenced[neighbor_side])
                    return false;

                sides_referenced[neighbor_side] = true;

                auto neighbor_edge_index = sides[neighbor_side].get_edge_index(side_index);

                if (neighbor_edge_index == half_edge::no_neighbor)
                    return false;

                const auto& sn = sides[neighbor_side];

                // Points must match
                if (s.edges[edge_index].point_nr != sn.edges[sn.next_edge(neighbor_edge_index)].point_nr)
                    return false;

                if (s.edges[s.next_edge(edge_index)].point_nr != sn.edges[neighbor_edge_index].point_nr)
                    return false;
            }
        }
        return true;
    }
    /// Create polyhedron pyramid from five points (p0-p3 base plate and p4
    /// head)
    static polyhedron_t make_pyramid(
        const vector3t<D>& p0,
        const vector3t<D>& p1,
        const vector3t<D>& p2,
        const vector3t<D>& p3,
        const vector3t<D>& p4)
    {
        polyhedron_t p;
        p.points = {p0, p1, p2, p3, p4};

        // p.sides =
        std::vector<side> sides{
            side({half_edge(0, 3), half_edge(4, 1), half_edge(2, 4)}),
            side({half_edge(2, 0), half_edge(4, 2), half_edge(3, 4)}),
            side({half_edge(3, 1), half_edge(4, 3), half_edge(1, 4)}),
            side({half_edge(1, 2), half_edge(4, 0), half_edge(0, 4)}),
            side({half_edge(0, 0), half_edge(2, 1), half_edge(3, 2), half_edge(1, 3)})};

        p.sides = std::move(sides);

        // if (!p.check()) THROW(error, "invalid pyramid");
        return p;
    }

    /// check if polyhedron is empty. Each polyhedron must have at least four
    /// vertices or it is empty
    bool empty() const { return points.size() < 4; }

    /// Convert side to polygon
    polygon_t<D> convert_side(unsigned n) const
    {
        polygon_t<D> p(unsigned(sides[n].edges.size()));
        for (const auto& e : sides[n].edges) {
            p.add_point(points[e.point_nr]);
        }
        return p;
    }

    /// Remove neighbor information
    void remove_neighbor(unsigned side_index, unsigned edge_index)
    {
        sides[side_index].edges[edge_index].neighbor_side = half_edge::no_neighbor;
    }

    /// Remove adjacency if existing
    void remove_adjacency(unsigned side_index, unsigned edge_index)
    {
        auto& neighbor_side = sides[side_index].edges[edge_index].neighbor_side;

        if (neighbor_side != half_edge::no_neighbor) {
            auto neighbor_edge_index = sides[neighbor_side].get_edge_index(side_index);

            if (neighbor_edge_index != half_edge::no_neighbor)
                remove_neighbor(neighbor_side, neighbor_edge_index);

            neighbor_side = half_edge::no_neighbor;
        }
    }

    /// Remove point from edge (and sets remaining edge to open)
    void remove_point(unsigned side_index, unsigned corner_index)
    {
        auto& s                        = sides[side_index];
        unsigned previous_corner_index = s.prev_edge(corner_index);
        // only remove neighbor info from this side! For split edges
        // get_edge_index is not reliable.
        remove_neighbor(side_index, previous_corner_index);
        remove_neighbor(side_index, corner_index);
        s.edges.erase(s.edges.begin() + corner_index);
    }

    /// From an open edge find next one
    void find_next_open_edge(uint32_t& side_index, uint32_t& edge_index) const
    {
        while (true) {
            // go to previous edge
            const auto& s = sides[side_index];
            edge_index    = s.prev_edge(edge_index);
            // if we have a neighbor there, switch to neighbor and repeat
            uint32_t neighbor_side = s.edges[edge_index].neighbor_side;

            if (neighbor_side == half_edge::no_neighbor)
                return;

            edge_index = sides[neighbor_side].get_edge_index(side_index);
            side_index = neighbor_side;
        }
    }

    /// Clip polyhedron by generic plane
    void clip(const plane_t<D>& pln)
    {
        // For every point determine on which side of plane it is or if it is on
        // the plane
        std::vector<int> point_side(points.size());
        const D epsilon              = D(0.001);
        bool has_point_on_front_side = false;
        bool has_point_on_back_side  = false;

        for (unsigned i = 0; i < unsigned(points.size()); ++i) {
            D point_distance = pln.distance(points[i]);

            if (point_distance < -epsilon) {
                has_point_on_back_side = true;
                point_side[i]          = -1;
            } else if (point_distance > epsilon) {
                has_point_on_front_side = true;
                point_side[i]           = 1;
            } else {
                // point_side[i] = 0; // already 0 from vector init
            }
        }

        // If there is no point on backside, we don't need to cut anything
        if (!has_point_on_back_side)
            return;

        if (!has_point_on_front_side) {
            // clear polyhedron
            points.clear();
            sides.clear();
            return;
        }

        // remember for every side if it has been cut
        std::vector<bool> side_cut(sides.size());

        // create new points for edges intersecting the plane
        for (unsigned k = 0; k < unsigned(sides.size()); ++k) {
            auto& s    = sides[k];
            unsigned j = unsigned(s.edges.size()) - 1;

            for (unsigned i = 0; i < unsigned(s.edges.size()); ++i) {
                auto p0 = s.edges[i].point_nr;
                auto p1 = s.edges[j].point_nr;

                if (p0 < p1) {
                    // check intersection
                    if (point_side[p0] * point_side[p1] == -1) {
                        // one side must have been 1, one -1, so edge is
                        // crossing
                        vector3t<D> its = pln.intersection(points[p0], points[p1]);

                        // add new point and modify both sides by inserting
                        // point. Neighboring information is updated accordingly
                        // (kept for new edges)
                        unsigned point_index = unsigned(points.size());
                        points.push_back(its);
                        point_side.push_back(0);

                        uint32_t nb_side       = s.edges[j].neighbor_side;
                        uint32_t nb_edge_index = sides[nb_side].get_edge_index(k);

                        s.split_edge(j, point_index);
                        sides[nb_side].split_edge(nb_edge_index, point_index);

                        side_cut[k] = true;
                    }
                }
                j = i;
            }
        }

        // change cut sides by removing parts on backside,
        // remove all sides that have at least one point on backside.
        // mark open edges.
        bool needs_new_cap_side = false;
        bool side_was_cleared   = false;

        for (unsigned k = 0; k < unsigned(sides.size()); ++k) {
            auto& s                     = sides[k];
            bool has_parts_on_backside  = false;
            bool has_parts_on_frontside = false;

            for (unsigned i = 0; i < unsigned(s.edges.size()); ++i) {
                int ps = point_side[s.edges[i].point_nr];

                if (ps < 0) {
                    has_parts_on_backside = true;
                    needs_new_cap_side    = true;
                } else if (ps > 0) {
                    has_parts_on_frontside = true;
                }
            }
            if (has_parts_on_backside) {
                // we have to do something
                if (has_parts_on_frontside) {
                    // side has to be cut (modified). Just remove all corners
                    // that are on backside from array, clear neighboring
                    // information before. If a corner is on backside, remove it
                    // and clear neighboring info. We MUST NOT clear neighbor
                    // info, as neighbor side has two references to this side
                    // and thus get_edge_index is not reliable. However for
                    // neighbors we also check points and remove the neighbor
                    // information there.
                    for (unsigned i = 0; i < unsigned(s.edges.size());) {
                        if (point_side[s.edges[i].point_nr] < 0) {
                            remove_point(k, i);
                        } else {
                            ++i;
                        }
                    }
                } else {
                    // Side needs to get removed entirely.
                    // Remove neighboring info from all neighbors.
                    for (unsigned i = 0; i < unsigned(s.edges.size()); ++i) {
                        remove_adjacency(k, i);
                    }
                    // Clear side data
                    s.edges.clear();
                    side_was_cleared = true;
                }
            }
        }

        if (side_was_cleared) {
            // remove empty sides at back
            while (!sides.empty() && sides.back().edges.empty()) {
                sides.pop_back();
            }

            // check for empty sides in between
            for (unsigned side_index = 0; side_index < unsigned(sides.size());) {
                if (sides[side_index].edges.empty()) {
                    unsigned valid_side_index = unsigned(sides.size()) - 1;

                    // replace every occurence of j in all neighbor infos by k.
                    for (auto& s : sides) {
                        for (auto& e : s.edges) {
                            if (e.neighbor_side == valid_side_index) {
                                e.neighbor_side = side_index;
                            }
                        }
                    }

                    sides[side_index].edges = std::move(sides.back().edges);
                    sides.pop_back();
                } else {
                    ++side_index;
                }
            }
        }

        // check for open edges and close them
        if (needs_new_cap_side) {
            uint32_t current_side       = 0;
            uint32_t current_edge_index = 0;

            for (unsigned k = 0; k < unsigned(sides.size()); ++k) {
                auto& s = sides[k];

                for (unsigned i = 0; i < unsigned(s.edges.size()); ++i) {
                    if (s.edges[i].neighbor_side == half_edge::no_neighbor) {
                        current_side       = k;
                        current_edge_index = i;
                        k                  = unsigned(sides.size());
                        break;
                    }
                }
            }

            unsigned cap_side_index = unsigned(sides.size());
            sides.push_back(side());
            auto start_side       = current_side;
            auto start_edge_index = current_edge_index;

            do {
                const auto& s        = sides[current_side];
                auto next_edge_index = s.next_edge(current_edge_index);

                sides.back().edges.push_back(half_edge(s.edges[next_edge_index].point_nr, current_side));

                find_next_open_edge(current_side, current_edge_index);

                // set neighbor info for this open edge to new side. Sets it for
                // start side as well on last iteration.
                sides[current_side].edges[current_edge_index].neighbor_side = cap_side_index;

            } while (current_side != start_side || current_edge_index != start_edge_index);
        }

        // Remove points on backside
        while (!point_side.empty() && point_side.back() < 0) {
            points.pop_back();
            point_side.pop_back();
        }

        // check for backside points in between
        for (unsigned k = 0; k < unsigned(points.size()); ++k) {
            if (point_side[k] < 0) {
                // point is on backside, replace with last valid point
                unsigned j = unsigned(points.size()) - 1;
                // replace every occurence of j in all point_nr infos by k.
                for (auto& s : sides) {
                    for (auto& e : s.edges) {
                        if (e.point_nr == j) {
                            e.point_nr = k;
                        }
                    }
                }
                points[k]     = points[j];
                point_side[k] = point_side[j];
                points.pop_back();
                point_side.pop_back();
            }
        }
        if (!check())
            THROW(error, "invalid split result"); // fixme remove later!
    }

  public:
    std::vector<vector3t<D>> points; ///< The points in 3-space forming the polyhedron
    std::vector<side> sides;         ///< The sides of the polyhedron
};

typedef polyhedron_t<double> polyhedron;
typedef polyhedron_t<float> polyhedronf;
