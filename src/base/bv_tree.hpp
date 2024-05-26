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
//  A bounding volume tree (spheres) (C)+(W) 2009 Thorsten Jordan
//

#pragma once

#include "cylinder.hpp"
#include "matrix4.hpp"
#include "sphere.hpp"

#include <array>
#include <vector>

/// a binary tree representing a bounding volume hierarchy
class bv_tree
{
  public:
    /// data representing a node (leaf or inner node)
    struct node
    {
        static const unsigned invalid_index{unsigned(-1)};
        std::array<uint32_t, 3> tri_idx = {invalid_index, invalid_index, invalid_index};

        spheref volume;
        [[nodiscard]] bool is_leaf() const { return tri_idx[2] != invalid_index; }

        [[nodiscard]] const vector3f& get_pos(const std::vector<vector3f>& vertices, unsigned corner) const
        {
            return vertices[tri_idx[corner]];
        }

        [[nodiscard]] vector3f get_center(const std::vector<vector3f>& vertices) const
        {
            return (get_pos(vertices, 0) + get_pos(vertices, 1) + get_pos(vertices, 2)) * (1.f / 3);
        }
    };

    /// parameters for collision
    struct param
    {
        const bv_tree& tree; ///< The tree to work on
        unsigned node_index; ///< index of tree node

        const std::vector<vector3f>& vertices; ///< vertex data to use for collision tests
        matrix4f transform;                    ///< Transformation to use for tree

        /// return the node for the subtree
        [[nodiscard]] const node& get_node() const { return tree.nodes[node_index]; }

        /// Is this a leaf node?
        [[nodiscard]] bool is_leaf() const { return get_node().is_leaf(); }

        /// Create param from whole bv tree
        param(const bv_tree& t, const std::vector<vector3f>& v, const matrix4f& m)
            : tree(t)
            , vertices(v)
            , transform(std::move(m))
        {
            node_index = unsigned(tree.nodes.size() - 1);
        }

        /// Create param with node index
        param(const bv_tree& t, uint32_t ni, const std::vector<vector3f>& v, const matrix4f& m)
            : tree(t)
            , node_index(ni)
            , vertices(v)
            , transform(std::move(m))
        {
        }

        /// Get subnode param
        [[nodiscard]] param children(unsigned i) const
        {
            auto& current_node = get_node();
            return param(tree, current_node.tri_idx[i], vertices, transform);
        }

        /// Get transformed volume
        [[nodiscard]] spheref get_transformed_volume() const
        {
            auto& current_node = get_node();
            return spheref(transform.mul4vec3xlat(current_node.volume.center), current_node.volume.radius);
        }

        /// Determine which child is closer
        [[nodiscard]] unsigned get_index_of_closer_child(const vector3f& pos) const
        {
            auto& current_node = get_node();
            if (current_node.is_leaf()) {
                return 2; // invalid index
            }

            vector3f cp0 = transform.mul4vec3xlat(tree.nodes[current_node.tri_idx[0]].volume.center);

            vector3f cp1 = transform.mul4vec3xlat(tree.nodes[current_node.tri_idx[1]].volume.center);

            return (cp0.square_distance(pos) < cp1.square_distance(pos)) ? 0 : 1;
        }
    };

    /// Create empty tree
    bv_tree() = default;

    /// Create a bounding volume tree
    bv_tree(const std::vector<vector3f>& vertices, std::vector<bv_tree::node>&& leaf_nodes);

    /// Check if position is inside the tree
    [[nodiscard]] bool is_inside(const vector3f& v) const;

    /// determine if two bv_trees intersect each other (are colliding). A list
    /// of contact points is computed. Note this can be very slow!
    static bool collides(const param& p0, const param& p1, std::vector<vector3f>& contact_points);

    /// determine if two bv_trees intersect each other (are colliding). The
    /// closest contact point is computed.
    static bool closest_collision(const param& p0, const param& p1, vector3f& contact_point);

    /// determine if bv_trees intersects sphere (are colliding).
    static bool collides(const param& p, const spheref& sp, vector3f& contact_point);

    /// determine if bv_tree intersects line (cylinder)
    static bool collides(const param& p, const cylinderf& cyl, vector3f& contact_point);

    /// Transform tree data
    void transform(const matrix4f& mat);

    /// Compute min and max value of all tree nodes
    void compute_min_max(vector3f& minv, vector3f& maxv) const;

    /// For tests
    void collect_volumes_of_tree_depth(std::vector<spheref>& volumes, unsigned depth) const;

    /// Is the tree undefined?
    [[nodiscard]] bool empty() const { return nodes.empty(); }

  protected:
    /// The nodes of the tree. The root node is always the last one.
    std::vector<node> nodes;
};
