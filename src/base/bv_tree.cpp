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

#include "bv_tree.hpp"

#include "error.hpp"
#include "triangle_intersection.hpp"

//#define DEBUG_OUTPUT
#undef DEBUG_OUTPUT
#ifdef DEBUG_OUTPUT
#include <iostream>
#endif

namespace
{
auto create_bv_subtree(
    const std::vector<vector3f>& vertices,
    std::vector<bv_tree::node>& nodes,
    unsigned index_begin,
    unsigned index_end) -> unsigned
{
    if (index_begin == index_end) {
        THROW(error, "bv_tree create on empty data");
    }

    // compute bounding box for leaves
    vector3f bbox_min = nodes[index_begin].get_pos(vertices, 0);
    vector3f bbox_max = bbox_min;

    for (auto index = index_begin; index < index_end; ++index) {
        auto& node = nodes[index];
        for (unsigned i = 0; i < 3; ++i) {
            bbox_min = bbox_min.min(node.get_pos(vertices, i));
            bbox_max = bbox_max.max(node.get_pos(vertices, i));
        }
    }

    // new sphere center is center of bbox
    spheref bound_sphere((bbox_min + bbox_max) * 0.5F, 0.0F);

    // compute sphere radius by vertex distances to center (more accurate than
    // approximating by bbox size)
    for (auto index = index_begin; index < index_end; ++index) {
        auto& node = nodes[index];
        for (unsigned i = 0; i < 3; ++i) {
            float r             = node.get_pos(vertices, i).distance(bound_sphere.center);
            bound_sphere.radius = std::max(r, bound_sphere.radius);
        }
    }

    // if list has one entry, return that
    if (index_begin + 1 == index_end) {
        nodes[index_begin].volume = bound_sphere;
        return index_begin;
    }

    //
    // split leaf node list in two parts
    //
    vector3f deltav = bbox_max - bbox_min;

    // chose axis with longest value range, sort along that axis,
    // split in center of bound_sphere.
    unsigned split_axis = 0; // x - default

    if (deltav.y > deltav.x) {
        if (deltav.z > deltav.y) {
            split_axis = 2; // z
        } else {
            split_axis = 1; // y
        }
    } else if (deltav.z > deltav.x) {
        split_axis = 2; // z
    }

#ifdef DEBUG_OUTPUT
    std::cout << "Create subtree [" << index_begin << "..." << index_end << "[ deltav " << deltav
              << " split_axis=" << split_axis << "\n";
#endif

    float vcenter[3];
    bound_sphere.center.to_mem(vcenter);

    auto index_end_left    = index_begin;
    auto index_begin_right = index_end;

    while (index_end_left < index_begin_right) {
        float vc[3];
        nodes[index_end_left].get_center(vertices).to_mem(vc);

        if (vc[split_axis] < vcenter[split_axis]) {
            // node is left, keep left of split and advance
            ++index_end_left;
        } else if (index_end_left + 1 < index_begin_right) {
            // node is right, swap with last node in range that has no side
            // defined and test again
            std::swap(nodes[index_end_left], nodes[index_begin_right - 1]);
            --index_begin_right;
        } else {
            // last node in range is right
            --index_begin_right;
        }
    }

    if (index_begin == index_end_left || index_begin_right == index_end) {
        // special case: force division
        index_end_left = index_begin_right = (index_begin + index_end) / 2;
    }

    // Create subtrees for left and right part of nodes
    auto left_child_index = create_bv_subtree(vertices, nodes, index_begin, index_end_left);

    auto right_child_index = create_bv_subtree(vertices, nodes, index_begin_right, index_end);

    // Create new node as parent for the sub trees and use the bounding sphere
    // over all nodes for it
    nodes.push_back({{left_child_index, right_child_index, bv_tree::node::invalid_index}, bound_sphere});

    return unsigned(nodes.size() - 1);
}

auto is_inside(const vector3f& v, const std::vector<bv_tree::node>& nodes, unsigned node_index) -> bool
{
    const auto& node = nodes[node_index];

    if (node.volume.is_inside(v)) {
        if (!node.is_leaf()) {
            // Check children
            for (unsigned i = 0; i < 2; ++i) {
                if (node.tri_idx[i] != bv_tree::node::invalid_index) {
                    if (is_inside(v, nodes, node.tri_idx[i])) {
                        return true;
                    }
                }
            }
        }
        return true;
    }
    return false;
}

void collect_volumes_of_tree_depth(
    std::vector<spheref>& volumes,
    unsigned depth,
    const std::vector<bv_tree::node>& nodes,
    unsigned node_index)
{
    const auto& node = nodes[node_index];

    if (depth == 0) {
        volumes.push_back(node.volume);
        return;
    }

    if (!node.is_leaf()) {
        collect_volumes_of_tree_depth(volumes, depth - 1, nodes, node.tri_idx[0]);

        collect_volumes_of_tree_depth(volumes, depth - 1, nodes, node.tri_idx[1]);
    }
}
} // namespace

bv_tree::bv_tree(const std::vector<vector3f>& vertices, std::vector<bv_tree::node>&& leaf_nodes)
    : nodes(std::move(leaf_nodes))
{
    if (!nodes.empty()) {
        create_bv_subtree(vertices, nodes, 0, unsigned(nodes.size()));
    }

    // Note that ships and objects are mostly of box shape we could store an
    // additional bounding box For a bit more precise checking. It would even be
    // sufficient to check for box intersections ONLY! However we have to apply
    // the transformations to each box then and check for intersections which is
    // a bit of code but not too complicated. However normally objects don't get
    // that close to each other, so we don't need this acceleration.
}

auto bv_tree::is_inside(const vector3f& v) const -> bool
{
    return ::is_inside(v, nodes, unsigned(nodes.size() - 1));
}

auto bv_tree::collides(const param& p0, const param& p1, std::vector<vector3f>& contact_points) -> bool
{
    // Note: code is nearly identical to function below. We need a solution to
    // have that code only once!

    // Transform vertices of p1 and then compare to p0
    const auto inverse_p0_tree_transform = p0.transform.inverse();
    const auto combined_transform        = inverse_p0_tree_transform * p1.transform;

    // Iterate over tree of p0, p1 and check for intersections, closest child
    // first
    std::function<bool(const bv_tree::node&, const bv_tree::node&)> check_intersection;

    check_intersection = [&](const bv_tree::node& node0, const bv_tree::node& node1) {
        const auto transformed_volume1 =
            spheref(combined_transform.mul4vec3xlat(node1.volume.center), node1.volume.radius);

        if (!node0.volume.intersects(transformed_volume1)) {
            return false;
        }

        bool split_node1 = false;

        if (node0.is_leaf()) {
            if (node1.is_leaf()) {
                // direct face to face collision test
                // handle transform here
                const auto& v0t = p0.vertices[node0.tri_idx[0]];
                const auto& v1t = p0.vertices[node0.tri_idx[1]];
                const auto& v2t = p0.vertices[node0.tri_idx[2]];

                vector3f v3t = combined_transform.mul4vec3xlat(p1.vertices[node1.tri_idx[0]]);

                vector3f v4t = combined_transform.mul4vec3xlat(p1.vertices[node1.tri_idx[1]]);

                vector3f v5t = combined_transform.mul4vec3xlat(p1.vertices[node1.tri_idx[2]]);

                // note that degenerated triangles would be a critical problem
                // here, but they would have a bounding sphere of radius zero
                // and thus we never would compare with them, so we don't need
                // to check for them here.
                bool c = triangle_intersection::compute<float>(v0t, v1t, v2t, v3t, v4t, v5t);

                if (c) {
                    // fixme: compute more accurate position here, maybe
                    // weight by triangle area between centers of triangles.
                    contact_points.push_back(
                        p0.transform.mul4vec3xlat((v0t + v1t + v2t + v3t + v4t + v5t) * (1.F / 6)));
                }
                return c;
            }
            split_node1 = true;

        } else if (!node1.is_leaf() && node0.volume.radius < node1.volume.radius) {
            split_node1 = true;
        }
        if (!split_node1) {
            const auto& left_child_node  = p0.tree.nodes[node0.tri_idx[0]];
            const auto& right_child_node = p0.tree.nodes[node0.tri_idx[1]];
            const auto rl                = check_intersection(left_child_node, node1);
            const auto rr                = check_intersection(right_child_node, node1);

            return rl || rr;
        }
        const auto& left_child_node  = p1.tree.nodes[node1.tri_idx[0]];
        const auto& right_child_node = p1.tree.nodes[node1.tri_idx[1]];
        const auto rl                = check_intersection(node0, left_child_node);
        const auto rr                = check_intersection(node0, right_child_node);

        return rl || rr;
    };

    return check_intersection(p0.tree.nodes.back(), p1.tree.nodes.back());
}

auto bv_tree::closest_collision(const param& p0, const param& p1, vector3f& contact_point) -> bool
{
    // Transform vertices of p1 and then compare to p0
    const auto inverse_p0_tree_transform = p0.transform.inverse();
    const auto combined_transform        = inverse_p0_tree_transform * p1.transform;

    const auto combined_inverse_transform = p1.transform.inverse() * p0.transform;

    // Iterate over tree of p0, p1 and check for intersections, closest child
    // first
    std::function<bool(const bv_tree::node&, const bv_tree::node&)> check_intersection;

    check_intersection = [&](const bv_tree::node& node0, const bv_tree::node& node1) {
        const auto transformed_volume1 =
            spheref(combined_transform.mul4vec3xlat(node1.volume.center), node1.volume.radius);

        if (!node0.volume.intersects(transformed_volume1)) {
            return false;
        }

        bool split_node1 = false;

        if (node0.is_leaf()) {
            if (node1.is_leaf()) {
                // direct face to face collision test
                // handle transform here
                const auto& v0t = p0.vertices[node0.tri_idx[0]];
                const auto& v1t = p0.vertices[node0.tri_idx[1]];
                const auto& v2t = p0.vertices[node0.tri_idx[2]];

                vector3f v3t = combined_transform.mul4vec3xlat(p1.vertices[node1.tri_idx[0]]);

                vector3f v4t = combined_transform.mul4vec3xlat(p1.vertices[node1.tri_idx[1]]);

                vector3f v5t = combined_transform.mul4vec3xlat(p1.vertices[node1.tri_idx[2]]);

                // note that degenerated triangles would be a critical problem
                // here, but they would have a bounding sphere of radius zero
                // and thus we never would compare with them, so we don't need
                // to check for them here.
                bool c = triangle_intersection::compute<float>(v0t, v1t, v2t, v3t, v4t, v5t);

                if (c) {
                    // fixme: compute more accurate position here, maybe
                    // weight by triangle area between centers of triangles.
                    contact_point = p0.transform.mul4vec3xlat((v0t + v1t + v2t + v3t + v4t + v5t) * (1.F / 6));
                }
                return c;
            }
            split_node1 = true;

        } else if (!node1.is_leaf() && node0.volume.radius < node1.volume.radius) {
            split_node1 = true;
        }

        if (!split_node1) {
            const auto& left_child_node  = p0.tree.nodes[node0.tri_idx[0]];
            const auto& right_child_node = p0.tree.nodes[node0.tri_idx[1]];

            if (left_child_node.volume.center.square_distance(transformed_volume1.center)
                < right_child_node.volume.center.square_distance(transformed_volume1.center)) {
                return check_intersection(left_child_node, node1) || check_intersection(right_child_node, node1);
            }
            return check_intersection(right_child_node, node1) || check_intersection(left_child_node, node1);
        }
        const auto& left_child_node  = p1.tree.nodes[node1.tri_idx[0]];
        const auto& right_child_node = p1.tree.nodes[node1.tri_idx[1]];

        const auto transformed_volume0_center = combined_inverse_transform.mul4vec3xlat(node0.volume.center);

        if (left_child_node.volume.center.square_distance(transformed_volume0_center)
            < right_child_node.volume.center.square_distance(transformed_volume0_center)) {
            return check_intersection(node0, left_child_node) || check_intersection(node0, right_child_node);
        }
        return check_intersection(node0, right_child_node) || check_intersection(node0, left_child_node);
    };

    return check_intersection(p0.tree.nodes.back(), p1.tree.nodes.back());
}

auto bv_tree::collides(const param& p, const spheref& sp, vector3f& contact_point) -> bool
{
    const auto inverse_tree_transform = p.transform.inverse();
    const auto transformed_sphere     = spheref(inverse_tree_transform * sp.center, sp.radius);

    // Iterate over tree of p and check for intersection with transformed
    // sphere, closest child first
    std::function<bool(const bv_tree::node&)> check_intersection;

    check_intersection = [&](const bv_tree::node& node) {
        if (node.volume.intersects(transformed_sphere)) {
            if (node.is_leaf()) {
                contact_point = (p.transform.mul4vec3xlat(node.volume.center) + sp.center) * 0.5F;
                return true;
            }

            const auto& left_child_node  = p.tree.nodes[node.tri_idx[0]];
            const auto& right_child_node = p.tree.nodes[node.tri_idx[1]];

            if (left_child_node.volume.center.square_distance(transformed_sphere.center)
                < right_child_node.volume.center.square_distance(transformed_sphere.center)) {
                return check_intersection(left_child_node) || check_intersection(right_child_node);
            }
            return check_intersection(right_child_node) || check_intersection(left_child_node);
        }
        return false;
    };

    return check_intersection(p.tree.nodes.back());
}

auto bv_tree::collides(const param& p, const cylinderf& cyl, vector3f& contact_point) -> bool
{
    const auto inverse_tree_transform = p.transform.inverse();

    const auto transformed_cylinder =
        cylinderf(inverse_tree_transform * cyl.start, inverse_tree_transform * cyl.end, cyl.radius);

    // Iterate over tree of p and check for intersection with transformed
    // cylinder, closest child first
    std::function<bool(const bv_tree::node&)> check_intersection;

    check_intersection = [&](const bv_tree::node& node) {
        if (transformed_cylinder.intersects(node.volume)) {
            if (node.is_leaf()) {
                const auto delta = transformed_cylinder.end - transformed_cylinder.start;

                const auto t = std::clamp(
                    (node.volume.center - transformed_cylinder.start) * delta / delta.square_length(), 0.0F, 1.0F);

                // position: center between projection of volume on cylinder
                // axis and volume center
                contact_point =
                    (helper::interpolate(cyl.start, cyl.end, t) + p.transform.mul4vec3xlat(node.volume.center)) * 0.5F;

                return true;
            }

            const auto& left_child_node  = p.tree.nodes[node.tri_idx[0]];
            const auto& right_child_node = p.tree.nodes[node.tri_idx[1]];

            if (transformed_cylinder.distance(left_child_node.volume.center)
                < transformed_cylinder.distance(right_child_node.volume.center)) {
                return check_intersection(left_child_node) || check_intersection(right_child_node);
            }
            return check_intersection(right_child_node) || check_intersection(left_child_node);
        }
        return false;
    };

    return check_intersection(p.tree.nodes.back());
}

void bv_tree::transform(const matrix4f& mat)
{
    for (auto& node : nodes) {
        node.volume.center = mat.mul4vec3xlat(node.volume.center);
    }
}

void bv_tree::compute_min_max(vector3f& minv, vector3f& maxv) const
{
    for (const auto& node : nodes) {
        node.volume.compute_min_max(minv, maxv);
    }
}

void bv_tree::collect_volumes_of_tree_depth(std::vector<spheref>& volumes, unsigned depth) const
{
    ::collect_volumes_of_tree_depth(volumes, depth, nodes, unsigned(nodes.size() - 1));
}
