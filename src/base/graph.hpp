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
//  A graph represention (C)+(W) 2020 Thorsten Jordan
//

#pragma once

#include "vector2.hpp"

#include <algorithm>
#include <functional>
#include <vector>

/// A generic class for graphs
template<typename NodeData, typename EdgeData>
class graph
{
  public:
    /// Constructor
    graph()
        : adjacency_ok(true)
    {
    }

    /// Request number of nodes
    unsigned get_nr_of_nodes() const { return unsigned(node_data.size()); }

    /// Request number of edges
    unsigned get_nr_of_edges() const { return unsigned(edge_data.size()); }

    /// Add Node
    unsigned add_node(NodeData&& nd = NodeData())
    {
        unsigned n = unsigned(node_data.size());
        node_data.push_back(std::move(nd));
        adjacency_ok = false;
        return n;
    }

    /// Add edge
    unsigned add_edge(const vector2i& nodes, EdgeData&& ed = EdgeData())
    {
        unsigned n = unsigned(edge_data.size());
        edge_data.push_back(ed);
        nodes_of_edge.push_back(nodes);
        adjacency_ok = false;
        return n;
    }

    /// Remove all edges
    void clear_edges()
    {
        edge_data.clear();
        nodes_of_edge.clear();
        adjacency_ok = false;
    }

    /// Run function for all nodes
    void for_all_nodes(const std::function<void(const NodeData&)>& func)
    {
        for (auto& nd : node_data) {
            func(nd);
        }
    }

    /// Run function for all edges
    void for_all_edges(const std::function<void(const vector2i&, const EdgeData&)>& func)
    {
        for (std::size_t i = 0; i < edge_data.size(); ++i) {
            func(nodes_of_edge[i], edge_data[i]);
        }
    }

    /// Run function for all neighbors of a node
    void for_all_adjacent_nodes(unsigned n, const std::function<void(unsigned)>& func)
    {
        compute_adjacency();
        for (unsigned i = neighbor_indices[n]; i < neighbor_indices[n + 1]; ++i) {
            func(neighbors[i]);
        }
    }

    /// Request data of node
    const NodeData& get_node_data(unsigned n) const { return node_data[n]; }

    /// Request data of edge
    const EdgeData& get_edge_data(unsigned n) const { return edge_data[n]; }

    /// Set node data
    void set_node_data(unsigned n, NodeData&& d) { node_data[n] = std::move(d); }

    /// Set edge data
    void set_edge_data(unsigned n, EdgeData&& d) { edge_data[n] = std::move(d); }

    /// Request all node data
    const std::vector<NodeData>& get_all_node_data() const { return node_data; }

    /// Request all node data
    const std::vector<EdgeData>& get_all_edge_data() const { return edge_data; }

    /// Request nodes of edge
    vector2i get_nodes_of_edge(unsigned n) const { return nodes_of_edge[n]; }

    /// Clear all data
    void clear()
    {
        node_data.clear();
        nodes_of_edge.clear();
        edge_data.clear();
        neighbors.clear();
        neighbor_indices.clear();
        adjacency_ok = false;
    }

  protected:
    std::vector<NodeData> node_data;        //!< Data for every node
    std::vector<vector2i> nodes_of_edge;    //!< The two nodes forming the edge
    std::vector<EdgeData> edge_data;        //!< Data for every edge
    std::vector<unsigned> neighbors;        //!< Global list of neighbors
    std::vector<unsigned> neighbor_indices; //!< For every node an index in neighbors
    bool adjacency_ok;                      //!< Is adjacency ok?

    void compute_adjacency()
    {
        if (!adjacency_ok) {
            // store all edges and sort
            std::vector<vector2i> all_edges(nodes_of_edge.size() * 2);
            for (unsigned i = 0; i < get_nr_of_edges(); ++i) {
                all_edges[2 * i]     = nodes_of_edge[i];
                all_edges[2 * i + 1] = vector2i(nodes_of_edge[i].y, nodes_of_edge[i].x);
            }
            std::sort(all_edges.begin(), all_edges.end(), [](const vector2i& a, const vector2i& b) {
                return (a.x == b.x) ? (a.y < b.y) : (a.x < b.x);
            });
            // Now fill in adjacency information
            neighbors.resize(all_edges.size());
            neighbor_indices.resize(get_nr_of_nodes() + 1);

            unsigned k          = 0;
            neighbor_indices[0] = 0;

            for (unsigned i = 0; i < get_nr_of_nodes(); ++i) {
                while (all_edges[k].x == i && k < unsigned(all_edges.size())) {
                    neighbors[k] = all_edges[k].y;
                    ++k;
                }
                neighbor_indices[i + 1] = k;
            }
            adjacency_ok = true;
        }
    }
};
