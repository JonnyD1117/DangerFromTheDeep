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

// A 3d mesh
// (C)+(W) by Thorsten Jordan. See LICENSE

#pragma once

#include "box.hpp"
#include "bv_tree.hpp"
#include "error.hpp"
#include "matrix3.hpp"
#include "matrix4.hpp"
#include "plane.hpp"
#include "vector3.hpp"
#include "xml.hpp"

#include <array>
#include <functional>
#include <string>
#include <vector>

/// A 3D model consisting of triangles
class mesh
{
  public:
    /// a triangle index, simple wrapper for uint32_t to make type unique
    class triangle_index
    {
      public:
        static const uint32_t invalid = uint32_t(-1);
        triangle_index(uint32_t value = invalid)
            : index(value)
        {
        }
        bool operator==(const triangle_index& t) const { return index == t.index; }
        bool operator!=(const triangle_index& t) const { return index != t.index; }
        auto get_index() const { return index; }

      protected:
        uint32_t index;
    };

    /// a vertex index, simple wrapper for uint32_t to make type unique
    class vertex_index
    {
      public:
        static const uint32_t invalid = uint32_t(-1);
        vertex_index(uint32_t value = invalid)
            : index(value)
        {
        }
        bool operator==(const vertex_index& v) const { return index == v.index; }
        bool operator!=(const vertex_index& v) const { return index != v.index; }
        auto get_index() const { return index; }

      protected:
        uint32_t index;
    };

    /// Constructor
    mesh() = default;

    /// Constructor to feed in data
    mesh(
        std::vector<vector3f>&& positions,
        std::vector<std::array<vertex_index, 3>>&& indices,
        std::vector<vector2f>&& texcoords  = std::vector<vector2f>(),
        std::vector<vector3f>&& normals    = std::vector<vector3f>(),
        std::vector<vector3f>&& tangentsx  = std::vector<vector3f>(),
        std::vector<uint8_t>&& righthanded = std::vector<uint8_t>());

    /// Return the number of vertices
    auto get_nr_of_vertices() const { return unsigned(positions.size()); }

    /// Return the number of triangles
    auto get_nr_of_triangles() const { return unsigned(indices.size()); }

    /// return corner vertex of triangle
    auto vertex(triangle_index ti, unsigned ci) const { return indices[ti.get_index()][ci]; }

    /// return position of vertex
    const auto& position(vertex_index vi) const { return positions[vi.get_index()]; }

    /// return normal of vertex
    const auto& normal(vertex_index vi) const { return normals[vi.get_index()]; }

    /// return texcoord of vertex
    const auto& texcoord(vertex_index vi) const { return texcoords[vi.get_index()]; }

    /// return normal of three positions
    static auto normal(const vector3f& p0, const vector3f& p1, const vector3f& p2)
    {
        return (p1 - p0).orthogonal(p2 - p0).normal();
    }

    /// return normal of triangle
    auto normal(triangle_index ti) const
    {
        return normal(position(vertex(ti, 0)), position(vertex(ti, 1)), position(vertex(ti, 2)));
    }

    const boxf& compute_bounds();
    boxf compute_bounds(const matrix4f& transmat) const;
    void compute_normals();
    bool compute_tangentx(vertex_index i0, vertex_index i1, vertex_index i2);

    /// transform vertices by matrix
    void transform(const matrix4f& m);
    void write_off_file(const std::string& fn) const;
    void read_off_file(const std::string& fn);

    /// split mesh by plane
    std::pair<mesh, mesh> split(const plane& p) const;

    void smooth_positions(unsigned num_iterations, float lambda, bool keep_border = true);

    /// check if a given point is inside the mesh
    ///@param p - point in vertex space, transformation not applied
    bool is_inside(const vector3f& p) const;
    double compute_volume() const;
    /// return volume of the mesh
    auto get_volume() const { return volume; }
    /// set volume of the mesh
    void set_volume(double v) { volume = v; }
    /// return name of the mesh
    const auto& get_name() const { return name; }
    /// return material ID of the mesh
    auto get_material_id() const { return material_id; }
    /// return the intertia tensor of the mesh
    const auto& get_inertia_tensor() const { return inertia_tensor; }
    /// set the inertia tensor
    void set_inertia_tensor(const matrix3& it) { inertia_tensor = it; }
    vector3 compute_center_of_gravity() const;
    /// give transformation matrix for vertices here (vertex->world space)
    matrix3 compute_inertia_tensor(const matrix4f& transmat) const;
    /// Given a triangle compute its center
    vector3f get_center_of_triangle(triangle_index t) const
    {
        return (position(vertex(t, 0)) + position(vertex(t, 1)) + position(vertex(t, 2))) * (1.f / 3);
    }

    bool has_adjacency_info() const;
    void compute_adjacency();
    bool check_adjacency() const;
    triangle_index get_adjacent_triangle(triangle_index triangle, unsigned edge) const
    {
        return triangle_adjacency[triangle.get_index()][edge];
    }
    triangle_index get_triangle_of_vertex(vertex_index vtx) const { return vertex_triangle_adjacency[vtx.get_index()]; }
    /// Given a triangle and one of its vertices return the corner index of the
    /// vertex.
    unsigned get_corner_index(triangle_index triangle, vertex_index vtx) const
    {
        for (unsigned k = 0; k < 3; ++k)
            if (vertex(triangle, k) == vtx)
                return k;
        THROW(error, "get_corner_index failed");
    }

    void for_all_adjacent_vertices(vertex_index vtx, const std::function<void(vertex_index)>& func);
    void for_all_adjacent_triangles(vertex_index vtx, const std::function<void(triangle_index)>& func);

    void compute_bv_tree();
    /// return whether the mesh has a bounding volume tree computed
    bool has_bv_tree() const { return !bounding_volume_tree.empty(); }
    const auto& get_bv_tree() const { return bounding_volume_tree; }
    const auto& get_bounds() const { return bounds; }

    /// slow intersection test on triangle-triangle tests
    bool intersects(const mesh& other, const matrix4f& transformation_this_to_other) const;

    /// check wether a triangle is degenerated
    static bool is_degenerated(const vector3f& v0, const vector3f& v1, const vector3f& v2, const float eps = 1e-3f);

    unsigned compute_tri_strip_size() const;
    std::vector<uint32_t> generate_tri_strip() const;

    std::vector<bool> compute_vertex_on_border_data() const;

    const auto& get_positions() const { return positions; }
    const auto& get_indices() const { return indices; }
    const auto& get_normals() const { return normals; }
    const auto& get_texcoords() const { return texcoords; }
    const auto& get_tangentsx() const { return tangentsx; }
    const auto& get_righthanded() const { return righthanded; }

    void load(const xml_elem& parent);
    xml_elem save(xml_elem& elem) const;

  protected:
    std::vector<vector3f> positions;                               ///< Vertex positions
    std::vector<std::array<vertex_index, 3>> indices;              ///< Triangle indices
    std::vector<vector3f> normals;                                 ///< Optional vertex normals
    std::vector<vector2f> texcoords;                               ///< Texture coordinates
    std::vector<vector3f> tangentsx;                               ///< Tangents
    std::vector<uint8_t> righthanded;                              ///< Is local coordinate system righthanded?
    std::vector<std::array<triangle_index, 3>> triangle_adjacency; ///< Adjacency information, computed on demand
    std::vector<triangle_index> vertex_triangle_adjacency;         ///< Adjacency information, computed on
                                                                   ///< demand
    bv_tree bounding_volume_tree;                                  ///< The optional tree for collision detection
    boxf bounds;                                                   ///< Bound values of positions
    matrix3 inertia_tensor{matrix3::one()};                        ///< Inertia tensor for physical simulation
    double volume{0};                                              ///< Volume of mesh in cubic meters
    std::string name;        ///< optionally a mesh can have a name (for use in model
                             ///< class or anywhere else)
    unsigned material_id{0}; ///< optionally a mesh can have a material assigned,
                             ///< stored as an ID.
};
