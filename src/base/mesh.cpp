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

#include "mesh.hpp"

#include "triangle_intersection.hpp"

#include <algorithm>
#include <fstream>
#include <unordered_map>

namespace
{
/// Subdivide string
static std::string next_part_of_string(const std::string& s, std::string::size_type& fromwhere)
{
    std::string::size_type st = s.find(" ", fromwhere);
    if (st == std::string::npos) {
        std::string tmp = s.substr(fromwhere);
        fromwhere       = st;
        return tmp;
    } else {
        std::string tmp = s.substr(fromwhere, st - fromwhere);
        fromwhere       = st + 1;
        if (fromwhere == s.length())
            fromwhere = std::string::npos;
        return tmp;
    }
}
} // namespace

/// Compute bounds of a mesh
const boxf& mesh::compute_bounds()
{
    bounds = boxf(positions);
    return bounds;
}

/// Compute bounds of a transformed mesh
boxf mesh::compute_bounds(const matrix4f& transmat) const
{
    boxf result;
    for (auto p : positions) {
        result.extend(transmat.mul4vec3(p));
    }
    return result;
}

/// Compute normals of a mesh
void mesh::compute_normals()
{
    // auto-detection of hard edges (creases) would be cool:
    // if the angle between faces at an edge is above a certain value,
    // the corners of the edge are duplicated and each instance gets their
    // own normals (like a mesh border), the same for vertices (cusps).
    // How to detect this: compute normals per face and adjacency information.
    // (For vertex cusps also vertex normals need to get computed).
    // If angle between normals (face to face or face to vertex) is higher than
    // treshold (e.g. 30 degrees) make a new instance of this vertex/edge for
    // each neighbour. Mark edges and vertexes if they are creases/cusps. The
    // corner vertices of an crease edge are also cusp vertices. Adjacency
    // information needed: face -> face, vertex -> faces

    // do not recompute normals if there are already some
    if (normals.size() != positions.size()) {
        normals.clear();
        normals.resize(positions.size());
        // compute normals for all triangles, sum them up in vertex normals
        for (unsigned trinr = 0; trinr < get_nr_of_triangles(); ++trinr) {
            auto ti        = triangle_index(trinr);
            auto trinormal = normal(ti);
            normals[vertex(ti, 0).get_index()] += trinormal;
            normals[vertex(ti, 1).get_index()] += trinormal;
            normals[vertex(ti, 2).get_index()] += trinormal;
        }
        // normalize vertex normals
        for (auto& n : normals) {
            n.normalize();
        }
    }

    // if we use normal mapping for this mesh, we need tangent values, too!
    // tangentsy get computed at runtime from normals and tangentsx
    // tangentsx are computed that way:
    // from each vertex we find a vector in positive u direction
    // and project it onto the plane given by the normal -> tangentx
    // because normal maps use stored texture coordinates (x = positive u!)
    if (!texcoords.empty()) {
        tangentsx.clear();
        tangentsx.resize(positions.size(), vector3f(axis::z));
        righthanded.clear();
        righthanded.resize(positions.size(), 0); // is set by compute_tangentx!
        std::vector<bool> vertexok(positions.size());
        for (auto& triidx : indices) {
            const auto i0 = triidx[0];
            const auto i1 = triidx[1];
            const auto i2 = triidx[2];
            if (!vertexok[i0.get_index()])
                vertexok[i0.get_index()] = compute_tangentx(i0, i1, i2);
            if (!vertexok[i1.get_index()])
                vertexok[i1.get_index()] = compute_tangentx(i1, i2, i0);
            if (!vertexok[i2.get_index()])
                vertexok[i2.get_index()] = compute_tangentx(i2, i0, i1);
        }
    }
}

/// Compute tangentx of a triangle
bool mesh::compute_tangentx(vertex_index i0, vertex_index i1, vertex_index i2)
{
    const auto& uv0  = texcoord(i0);
    const auto& uv1  = texcoord(i1);
    const auto& uv2  = texcoord(i2);
    const auto& n    = normal(i0);
    const auto d_uv0 = uv1 - uv0;
    const auto d_uv1 = uv2 - uv0;
    // compute inverse of matrix (d_uv0, d_uv1) below, here determinate A*D-B*C.
    const auto det = d_uv0.x * d_uv1.y - d_uv1.x * d_uv0.y;
    // dynamic limit for test against "zero"
    const auto med = float((fabs(d_uv0.x) + fabs(d_uv0.y) + fabs(d_uv1.x) + fabs(d_uv1.y)) * 0.25);
    const auto eps = med * med * 0.01f;
    // cout << "test " << d_uv0 << ", " << d_uv1 << ", med " << med << ", eps "
    // << eps << "\n";
    if (std::abs(det) <= eps) {
        // find sane solution for this situation!
        // if delta_u is zero for d_uv0 and d_uv1, but delta_v is not, we could
        // compute tangentsy from v and tangentsx with the cross product
        // or we just don't store a tangentsx value and hope that the vertex
        // can be computed via another triangle
        // just hope and wait seems to work, at least one face adjacent to the
        // vertex should give sane tangent values.

        // cout << "tangent comp failed for i0 " << i0 << ", uv0 " << d_uv0 <<
        // ", uv1 " << d_uv1 << ", det " << det << "\n";
        return false;
    } else {
        const auto v01 = position(i1) - position(i0);
        const auto v02 = position(i2) - position(i0);
        // compute tangentx by multiplying the inverted uv matrix with position
        // deltas
        const auto a              = d_uv1.y / det;
        const auto b              = -d_uv0.y / det;
        const auto rx             = v01 * a + v02 * b;
        tangentsx[i0.get_index()] = (rx - (rx * n) * n).normal();

        //		cout << "tangent * n " << i0 << ", " << tangentsx[i0] * n <<
        //"\n";

        // compute tangent y
        const auto c                = -d_uv1.x / det;
        const auto d                = d_uv0.x / det;
        const auto ry               = v01 * c + v02 * d;
        const auto tangentsy        = (ry - (ry * n) * n).normal();
        const auto g                = tangentsx[i0.get_index()].cross(tangentsy) * n;
        righthanded[i0.get_index()] = (g > 0);
        return true;
    }
}

/// Construct with existing data
mesh::mesh(
    std::vector<vector3f>&& positions_,
    std::vector<std::array<vertex_index, 3>>&& indices_,
    std::vector<vector2f>&& texcoords_,
    std::vector<vector3f>&& normals_,
    std::vector<vector3f>&& tangentsx_,
    std::vector<uint8_t>&& righthanded_)
    : positions(std::move(positions_))
    , indices(std::move(indices_))
    , normals(std::move(normals_))
    , texcoords(std::move(texcoords_))
    , tangentsx(std::move(tangentsx_))
    , righthanded(std::move(righthanded_))
{
    if (normals.empty()) {
        compute_normals();
    }
    if (!texcoords.empty() && texcoords.size() != positions.size()) {
        THROW(error, "texcoord count doesn't match position count");
    }
    if (!normals.empty() && normals.size() != normals.size()) {
        THROW(error, "normals count doesn't match position count");
    }
    if (!tangentsx.empty() && tangentsx.size() != positions.size()) {
        THROW(error, "tangentsx count doesn't match position count");
    }
    if (!righthanded.empty() && righthanded.size() != positions.size()) {
        THROW(error, "righthanded count doesn't match position count");
    }
    if (righthanded.empty() && !tangentsx.empty()) {
        righthanded.resize(positions.size(), true);
    }
}

/// Compute if two meshes intersect
bool mesh::intersects(const mesh& other, const matrix4f& transformation_this_to_other) const
{
    // This is some brute force method, could be done faster with bounding
    // volume trees. we need to handle transformation of meshes. compare
    // transformed vertices: T * v == o.T * o.v equivalent to v == T^-1 * o.T *
    // o.v
    // std::cout << "check intersection\n";
    for (auto& triidx : indices) {
        const vector3f& p0 = position(triidx[0]);
        const vector3f& p1 = position(triidx[1]);
        const vector3f& p2 = position(triidx[2]);
        if (!is_degenerated(p0, p1, p2)) {
            const vector3f v0 = transformation_this_to_other * p0;
            const vector3f v1 = transformation_this_to_other * p1;
            const vector3f v2 = transformation_this_to_other * p2;
            for (auto& triidx2 : other.indices) {
                const vector3f& v3 = other.position(triidx2[0]);
                const vector3f& v4 = other.position(triidx2[1]);
                const vector3f& v5 = other.position(triidx2[2]);
                if (!is_degenerated(v3, v4, v5)) {
                    if (triangle_intersection::compute<float>(v0, v1, v2, v3, v4, v5)) {
                        /*std::cout << "v0: " << v0 << "v1: " << v1 << "v2: " <<
                        v2 << "\n"; std::cout << "v3: " << v3 << "v4: " << v4 <<
                        "v5: " << v5 << "\n"; std::cout << "sqd " <<
                        v0.distance(v1) << "," << v0.distance(v2) << "," <<
                        v1.distance(v2) << "\n"; std::cout << "sqd " <<
                        v3.distance(v4) << "," << v3.distance(v5) << "," <<
                        v4.distance(v5) << "\n";*/
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

/// Check for degenerated triangles
bool mesh::is_degenerated(const vector3f& v0, const vector3f& v1, const vector3f& v2, const float eps)
{
    const auto delta01  = v1 - v0;
    const auto delta02  = v2 - v0;
    const auto delta12  = v2 - v1;
    const auto sqrlen01 = delta01.square_length();
    const auto sqrlen02 = delta02.square_length();
    const auto sqrlen12 = delta12.square_length();
    // If any edge of the triangle is of length near zero, it is degenerated.
    const auto eps2 = eps * eps;
    if (sqrlen01 < eps2) {
        return true;
    }
    if (sqrlen02 < eps2) {
        return true;
    }
    if (sqrlen12 < eps2) {
        return true;
    }
    // Check that triangle area is ok
    if (delta01.cross(delta02).length() < eps) {
        return true;
    }
    return false;
}

/// Transform positions and normals of mesh
void mesh::transform(const matrix4f& m)
{
    for (auto& elem : positions) {
        elem = m * elem;
    }
    // transform normals: only apply rotation
    auto m2       = m;
    m2.elem(3, 0) = m2.elem(3, 1) = m2.elem(3, 2) = 0.0f;
    for (auto& elem : normals) {
        elem = m2 * elem;
    }
    // do same for tangents!
    for (auto& elem : tangentsx) {
        elem = m2 * elem;
    }
}

/// Dump mesh to Object File Format (OFF)
void mesh::write_off_file(const std::string& fn) const
{
    std::ofstream out(fn.c_str());
    out << "OFF\n" << positions.size() << " " << indices.size() << " 0\n";
    for (auto& elem : positions) {
        out << elem.x << " " << elem.y << " " << elem.z << "\n";
    }
    for (auto& idx : indices) {
        out << "3 " << idx[0].get_index() << " " << idx[1].get_index() << " " << idx[2].get_index() << "\n";
    }
}

/// Read mesh from Object File Format (OFF)
void mesh::read_off_file(const std::string& fn)
{
    std::ifstream in(fn.c_str());
    if (!in.good()) {
        THROW(error, "Failed to read OFF file");
    }
    std::string offstr;
    unsigned nr_vertices = 0, nr_faces = 0, dummy = 0;
    in >> offstr >> nr_vertices >> nr_faces >> dummy;
    if (!in.good()) {
        THROW(error, "Failed to read OFF header");
    }
    positions.resize(nr_vertices);
    indices.resize(nr_faces);

    for (unsigned i = 0; i < nr_vertices; i++) {
        float a, b, c;
        in >> a >> b >> c;
        if (!in.good()) {
            THROW(error, "Short read on OFF vertices");
        }
        positions[i].x = a;
        positions[i].y = b;
        positions[i].z = c;
    }
    for (unsigned i = 0; i < nr_faces; i++) {
        unsigned v0, v1, v2;
        in >> dummy >> v0 >> v1 >> v2;
        if (!in.good()) {
            THROW(error, "Short read on OFF faces");
        }
        if (dummy != 3)
            return;
        indices[i][0] = vertex_index(v0);
        indices[i][1] = vertex_index(v1);
        indices[i][2] = vertex_index(v2);
    }
}

/// Split the mesh in two parts (cut by plane), that is return split parts as
/// new meshes
std::pair<mesh, mesh> mesh::split(const plane& split_plane) const
{
    mesh part0;
    mesh part1;
    part0.positions.reserve(positions.size() / 2);
    part1.positions.reserve(positions.size() / 2);
    part0.texcoords.reserve(texcoords.size() / 2);
    part1.texcoords.reserve(texcoords.size() / 2);
    part0.normals.reserve(normals.size() / 2);
    part1.normals.reserve(normals.size() / 2);
    part0.tangentsx.reserve(tangentsx.size() / 2);
    part1.tangentsx.reserve(tangentsx.size() / 2);
    part0.righthanded.reserve(righthanded.size() / 2);
    part1.righthanded.reserve(righthanded.size() / 2);
    part0.indices.reserve(indices.size() / 2);
    part1.indices.reserve(indices.size() / 2);

    // determine on which side the vertices are
    std::vector<float> dists(positions.size());
    std::vector<unsigned> ixtrans(positions.size());
    for (unsigned i = 0; i < positions.size(); ++i) {
        dists[i] = float(split_plane.distance(positions[i]));
        if (dists[i] >= 0) {
            ixtrans[i] = unsigned(part0.positions.size());
            part0.positions.push_back(positions[i]);
            if (!texcoords.empty())
                part0.texcoords.push_back(texcoords[i]);
            if (!normals.empty())
                part0.normals.push_back(normals[i]);
            if (!tangentsx.empty())
                part0.tangentsx.push_back(tangentsx[i]);
            if (!righthanded.empty())
                part0.righthanded.push_back(righthanded[i]);
        } else {
            ixtrans[i] = unsigned(part1.positions.size());
            part1.positions.push_back(positions[i]);
            if (!texcoords.empty())
                part1.texcoords.push_back(texcoords[i]);
            if (!normals.empty())
                part1.normals.push_back(normals[i]);
            if (!tangentsx.empty())
                part1.tangentsx.push_back(tangentsx[i]);
            if (!righthanded.empty())
                part0.righthanded.push_back(righthanded[i]);
        }
    }

    // now loop over all faces and split them
    for (unsigned i = 0; i < unsigned(indices.size()); ++i) {
        std::array<vertex_index, 3> ix;
        float ds[3];
        for (unsigned j = 0; j < 3; ++j) {
            ix[j] = indices[i][j];
            ds[j] = dists[ix[j].get_index()];
        }

        // check for faces completly on one side
        if (ds[0] >= 0 && ds[1] >= 0 && ds[2] >= 0) {
            part0.indices.push_back(
                {ixtrans[ix[0].get_index()], ixtrans[ix[1].get_index()], ixtrans[ix[2].get_index()]});
            continue;
        }
        if (ds[0] < 0 && ds[1] < 0 && ds[2] < 0) {
            part1.indices.push_back(
                {ixtrans[ix[0].get_index()], ixtrans[ix[1].get_index()], ixtrans[ix[2].get_index()]});
            continue;
        }

        // face needs to get splitted
        const auto p0v    = unsigned(part0.positions.size());
        const auto p1v    = unsigned(part1.positions.size());
        unsigned splitptr = 0;
        vertex_index newindi0[4]; // at most 4 indices
        unsigned newindi0ptr = 0;
        vertex_index newindi1[4]; // at most 4 indices
        unsigned newindi1ptr = 0;
        unsigned next[3]     = {1, 2, 0};
        for (unsigned j = 0; j < 3; ++j) {
            float d0 = ds[j], d1 = ds[next[j]];
            if (d0 >= 0)
                newindi0[newindi0ptr++] = ixtrans[ix[j].get_index()];
            else
                newindi1[newindi1ptr++] = ixtrans[ix[j].get_index()];
            if (d0 * d1 >= 0)
                continue;
            newindi0[newindi0ptr++] = p0v + splitptr;
            newindi1[newindi1ptr++] = p1v + splitptr;
            float fac               = fabs(d0) / (fabs(d0) + fabs(d1));
            vector3f newv           = position(ix[j]) * (1 - fac) + position(ix[next[j]]) * fac;
            part0.positions.push_back(newv);
            part1.positions.push_back(newv);
            if (!texcoords.empty()) {
                const auto newtexc =
                    texcoords[ix[j].get_index()] * (1 - fac) + texcoords[ix[next[j]].get_index()] * fac;
                part0.texcoords.push_back(newtexc);
                part1.texcoords.push_back(newtexc);
            }
            if (!normals.empty()) {
                const auto newnorm =
                    (normals[ix[j].get_index()] * (1 - fac) + normals[ix[next[j]].get_index()] * fac).normal();
                part0.normals.push_back(newnorm);
                part1.normals.push_back(newnorm);
            }
            if (!tangentsx.empty()) {
                const auto newtanx =
                    (tangentsx[ix[j].get_index()] * (1 - fac) + tangentsx[ix[next[j]].get_index()] * fac).normal();
                part0.tangentsx.push_back(newtanx);
                part1.tangentsx.push_back(newtanx);
            }
            if (!righthanded.empty()) {
                // fixme: check if this is correct
                part0.righthanded.push_back(righthanded[ix[j].get_index()]);
                part1.righthanded.push_back(righthanded[ix[j].get_index()]);
            }
            ++splitptr;
        }
        if (splitptr != 2) {
            THROW(error, "splitptr != 2 ?!");
        }
        // add indices to parts.
        part0.indices.push_back({newindi0[0], newindi0[1], newindi0[2]});
        if (newindi0ptr == 4) {
            part0.indices.push_back({newindi0[0], newindi0[2], newindi0[3]});
        }
        part1.indices.push_back({newindi1[0], newindi1[1], newindi1[2]});
        if (newindi1ptr == 4) {
            part1.indices.push_back({newindi1[0], newindi1[2], newindi1[3]});
        }
        if (!((newindi0ptr == 3 || newindi1ptr == 3) && (newindi0ptr + newindi1ptr == 7))) {
            THROW(error, "newindi ptr corrupt!");
        }
    }

    return std::make_pair(std::move(part0), std::move(part1));
}

/// Smooth positions by using defined number of iterations and lambda (1.0 =
/// full)
void mesh::smooth_positions(unsigned num_iterations, float lambda, bool keep_border)
{
    std::vector<vector3f> positions_tmp(positions);
    std::vector<bool> on_border;
    if (keep_border) {
        on_border = compute_vertex_on_border_data();
    }
    // we need an iteration of each vertex' one ring
    for (unsigned i = 0; i < num_iterations; ++i) {
        for (unsigned k = 0; k < unsigned(positions.size()); ++k) {
            if (!keep_border || !on_border[k]) {
                // only for not isolated vertices
                if (get_triangle_of_vertex(k) != triangle_index()) {
                    positions_tmp[k] = positions[k] * (1.f - lambda);
                    vector3f sum;
                    unsigned count = 0;
                    for_all_adjacent_vertices(k, [&](vertex_index j) {
                        sum += position(j);
                        ++count;
                    });
                    positions_tmp[k] += sum * (lambda / count);
                }
            }
        }
        positions.swap(positions_tmp);
    }
}

/// Test if a position is inside the 3d volume of the mesh
bool mesh::is_inside(const vector3f& p) const
{
    /* algorithm:
       for every triangle of the mesh, build a tetrahedron of the three
       points of the triangle and the center of the mesh (e.g. center
       of gravity). For all tetrahedrons that p is in, count the
       tetrahedrons with "positive" volume and "negative" volume.
       The former are all tetrahedrons where the triangle is facing
       away from the center point, the latter are all tetrahedrons,
       where the triangle is facing the center point.
       A point p is inside the tetrahedron consisting of A, B, C, D
       when: b = B-A, c = C-A, d = D-A, and p = A+r*b+s*c+t*d
       and r,s,t >= 0 and r+s+t <= 1.
       We can compute if the triangle is facing the center point D,
       by computing the sign of the dot product of the normal of
       triangle A,B,C and the vector D-A=d
       if (b cross c) * d >= 0 then A,B,C is facing D.
    */
    int in_out_count = 0;
    for (auto& triidx : indices) {
        const vector3f& A = position(triidx[0]);
        const vector3f& B = position(triidx[1]);
        const vector3f& C = position(triidx[2]);
        const vector3f D; // we use the center of mesh space for D.
        vector3f b = B - A;
        vector3f c = C - A;
        vector3f d = D - A;
        float s, r, t;
        if ((p - A).solve(b, c, d, s, r, t)) {
            if (r >= 0.0f && s >= 0.0f && t >= 0.0f && r + s + t <= 1.0f) {
                // p is inside the tetrahedron
                bool facing_to_D = b.cross(c) * d >= 0;
                in_out_count += facing_to_D ? -1 : 1;
            }
        }
    }
    // for tests:
    // std::cout << "is_inside p=" << p << " p=" << p << " ioc=" << in_out_count
    // << "\n";
    return in_out_count > 0;
}

/// Compute volume of mesh in cubic meters
double mesh::compute_volume() const
{
    double vsum = 0;
    for (auto& triidx : indices) {
        const vector3f& A = position(triidx[0]);
        const vector3f& B = position(triidx[1]);
        const vector3f& C = position(triidx[2]);
        const vector3f D; // we use the center of mesh space for D.
        vector3 a  = A - D;
        vector3 b  = B - D;
        vector3 c  = C - D;
        double V_i = (1.0 / 6.0) * (b.cross(c) * a);
        vsum += V_i;
    }
    // result is always matching vertex data, NOT treating the transformation!
    return vsum;
}

/** computing center of gravity:
   Divide sum over tetrahedrons with V_i * c_i each by sum over tetrahedrons
   with V_i each. Where V_i and c_i are volume and center of mass for each
   tetrahedron, given by c = 1/4 * (A+B+C+D) and V = 1/6 * (A-D)*(B-D)x(C-D)
*/
vector3 mesh::compute_center_of_gravity() const
{
    vector3 vsum;
    double vdiv = 0;
    for (auto& triidx : indices) {
        const vector3f& A = position(triidx[0]);
        const vector3f& B = position(triidx[1]);
        const vector3f& C = position(triidx[2]);
        const vector3f D; // we use the center of mesh space for D.
        vector3 a    = A - D;
        vector3 b    = B - D;
        vector3 c    = C - D;
        vector3 abcd = A + B + C + D;
        double V_i   = (1.0 / 6.0) * (b.cross(c) * a);
        vector3 c_i  = (1.0 / 4.0) * abcd;
        vsum += V_i * c_i;
        vdiv += V_i;
    }
    // std::cout << "center of gravity is " << vsum << "/" << vdiv << " = " <<
    // ((1.0/vdiv) * vsum) << "\n";
    // result is always matching vertex data, NOT treating the transformation!
    return (1.0 / vdiv) * vsum;
}

/// Check if mesh has adjacency info
bool mesh::has_adjacency_info() const
{
    return triangle_adjacency.size() == indices.size();
}

/// Compute adjacency information for triangles. Throws if mesh is corrupted
void mesh::compute_adjacency()
{
    unsigned nr_tri = get_nr_of_triangles();
    triangle_adjacency.clear();
    vertex_triangle_adjacency.clear();
    std::array<triangle_index, 3> empty_adjacencies;
    triangle_adjacency.resize(nr_tri, empty_adjacencies);
    vertex_triangle_adjacency.resize(positions.size());

    // we use an (unordered) map with vertex pair as key value (lower vertex
    // number first). triangle and edge index are stored for the pair. when we
    // encounter the pair the second time we know the adjacency information
    std::unordered_map<uint64_t, std::pair<triangle_index, unsigned>> adjacency_data;

    for (unsigned i = 0; i < nr_tri; ++i) {
        auto& triidx = indices[i];
        // Avoid degenerated triangles
        if (triidx[0] != triidx[1] && triidx[1] != triidx[2] && triidx[0] != triidx[2]) {
            for (unsigned j = 0; j < 3; ++j) {
                unsigned i0  = triidx[j].get_index();
                unsigned i1  = triidx[(j + 1) % 3].get_index();
                uint64_t key = (i0 < i1) ? i0 + (uint64_t(i1) << 32) : i1 + (uint64_t(i0) << 32);
                // std::cout << "Triangle " << i << " edgeindex " << j << "
                // vertices " << i0 << " to " << i1 << " key " << key << "\n";
                auto pib = adjacency_data.insert(std::make_pair(key, std::make_pair(triangle_index(i), j)));
                if (!pib.second) {
                    // vertex pair already encountered, so we can define
                    // adjacency
                    auto& pairtriedge = pib.first->second;
                    if (pairtriedge.first == triangle_index()) {
                        // edge was encountered more than twice
                        THROW(error, "mesh has more than two triangles on edge!");
                    }
                    triangle_adjacency[i][j]                                              = pairtriedge.first;
                    triangle_adjacency[pairtriedge.first.get_index()][pairtriedge.second] = i;
                    // mark edge as used
                    pairtriedge.first = triangle_index();
                }
            }
        }
    }

    // set vertex-triangle adjacency. vertex points to any triangle, but open
    // edges are preferred. so vertex points to triangle with open edge if there
    // is an open edge adjacent to the vertex
    for (unsigned i = 0; i < nr_tri; ++i) {
        for (unsigned j = 0; j < 3; ++j) {
            if (triangle_adjacency[i][j] == triangle_index()
                || vertex_triangle_adjacency[indices[i][j].get_index()] == triangle_index()) {
                vertex_triangle_adjacency[indices[i][j].get_index()] = triangle_index(i);
            }
        }
    }
}

/// Check that current adjacency data is correct
bool mesh::check_adjacency() const
{
    if (triangle_adjacency.size() != indices.size())
        return false;
    for (unsigned i = 0; i < unsigned(indices.size()); ++i) {
        auto& triidx = indices[i];
        for (unsigned j = 0; j < 3; ++j) {
            if (triangle_adjacency[i][j] != triangle_index()) {
                auto i0        = triidx[j];
                auto i1        = triidx[(j + 1) % 3];
                auto& nbtriidx = indices[triangle_adjacency[i][j].get_index()];
                bool ok        = false;
                for (unsigned k = 0; k < 3; ++k) {
                    if (nbtriidx[k] == i1) {
                        if (nbtriidx[(k + 1) % 3] != i0) {
                            // std::cout << "failed tri " << i << " edge " << j
                            // << " vertices " << i0 << " to " << i1 << " nb
                            // vertices " << nbtriidx[k] << " to " <<
                            // nbtriidx[(k
                            // + 1) % 3] << "\n";
                            return false;
                        }
                        ok = true;
                    }
                }
                // if (!ok) std::cout << "not ok for tri " << i << " edge " << j
                // << "\n";
                if (!ok)
                    return false;
            }
        }
    }
    return true;
}

void mesh::for_all_adjacent_vertices(vertex_index vtx, const std::function<void(vertex_index)>& func)
{
    if (!has_adjacency_info()) {
        THROW(error, "no adjacency info for 1-ring iteration");
    }
    auto tri = get_triangle_of_vertex(vtx);
    if (tri == triangle_index()) {
        THROW(error, "no triangle for vertex for 1-ring iteration");
    }
    auto first_tri = tri;
    // counter clockwise iteration
    auto endless_loop_protection = 1000;
    do {
        // Call for vertex at end of edge starting at vtx
        auto ci              = get_corner_index(tri, vtx);
        auto neighbor_vertex = indices[tri.get_index()][(ci + 1) % 3];
        func(neighbor_vertex);
        // Compute next triangle
        auto pci      = (ci + 2) % 3;
        auto next_tri = get_adjacent_triangle(tri, pci);
        if (next_tri == triangle_index()) {
            // we need to handle the last vertex especially
            neighbor_vertex = indices[tri.get_index()][pci];
            func(neighbor_vertex);
            break;
        }
        if (--endless_loop_protection == 0)
            THROW(error, "corrupt mesh for one ring iteration");
        tri = next_tri;
    } while (tri != first_tri);
}

void mesh::for_all_adjacent_triangles(vertex_index vtx, const std::function<void(triangle_index)>& func)
{
    if (!has_adjacency_info()) {
        THROW(error, "no adjacency info for 1-ring iteration");
    }
    auto tri = get_triangle_of_vertex(vtx);
    if (tri == triangle_index()) {
        THROW(error, "no triangle for vertex for 1-ring iteration");
    }
    auto first_tri = tri;
    // counter clockwise iteration
    auto endless_loop_protection = 1000;
    do {
        // Call for current triangle
        func(tri);
        // Compute next triangle
        auto pci      = (get_corner_index(tri, vtx) + 2) % 3;
        auto next_tri = get_adjacent_triangle(tri, pci);
        if (--endless_loop_protection == 0)
            THROW(error, "corrupt mesh for one ring iteration");
        tri = next_tri;
    } while (tri != first_tri && tri != triangle_index());
}

/** computing the inertia tensor for a mesh,
   from the RigidBodySimulation paper.
   The inertia tensor is:

   (M / Sum_over_i V_i) * Sum_over_i Integral over volume ...
   where M is total mass, i iterates over the tetrahedrons (hence triangles).
   and the integral (a matrix) can be written as:
   (1/120) * ((A-D)*(B-D)x(C-D))((A+B+C+D)(A+B+C+D)^T + AA^T + BB^T + CC^T +
   DD^T) where A,B,C form the base triangle and together with D the tetrahedron.
   D should be at center of gravity, for simplicities sake this should be
   (0,0,0), hence we can adjust the transformation matrix of the mesh or better
   the vertices.

   The formula decomposes to a scalar (1/120 and first brace) and a matrix
   (second brace).

   Problem: we can't manipulate the vertices or transformation matrices
   to shift center of gravity to (0,0,0) as we need both ways...

   however this routine should give inertia tensor matching the current
   object - but this can give problems for simulation later,
   if the c.o.g is not at 0,0,0 ...
*/
matrix3 mesh::compute_inertia_tensor(const matrix4f& transmat) const
{
    matrix3 msum;
    const auto mass              = 1.0; // is just a scalar to the matrix
    const auto center_of_gravity = transmat.mul4vec3xlat(compute_center_of_gravity());
    double vdiv                  = 0;
    for (auto& triidx : indices) {
        auto i0       = triidx[0];
        auto i1       = triidx[1];
        auto i2       = triidx[2];
        auto A        = transmat * position(i0);
        auto B        = transmat * position(i1);
        auto C        = transmat * position(i2);
        const auto& D = center_of_gravity;
        auto abcd     = A + B + C + D;
        auto V_i      = (1.0 / 6.0) * ((A - D) * (B - D).cross(C - D));
        auto fac0     = V_i / 20.0; // 6*20=120
        auto abcd2    = matrix3::vec_sqr(abcd);
        auto A2       = matrix3::vec_sqr(A);
        auto B2       = matrix3::vec_sqr(B);
        auto C2       = matrix3::vec_sqr(C);
        auto D2       = matrix3::vec_sqr(D);
        auto h        = (abcd2 + A2 + B2 + C2 + D2) * fac0;
        // we have to build the matrix with the integral
        // to compute out of sums / products of coefficients
        // of the helper matrix h.
        matrix3 im(
            h.elem(1, 1) + h.elem(2, 2),  // y^2+z^2
            -h.elem(1, 0),                // -xy
            -h.elem(2, 0),                // -xz
            -h.elem(1, 0),                // -xy
            h.elem(0, 0) + h.elem(2, 2),  // x^2+z^2
            -h.elem(2, 1),                // -yz
            -h.elem(2, 0),                // -xz
            -h.elem(2, 1),                // -yz
            h.elem(0, 0) + h.elem(1, 1)); // x^2+y^2
        msum = msum + im;
        vdiv += V_i;
    }
    // result is in model-space, not mesh-space
    return msum * (mass / vdiv);
}

/// Compute bounding volume tree of a mesh
void mesh::compute_bv_tree()
{
    // build leaf nodes for every triangle of m
    std::vector<bv_tree::node> leaf_nodes;
    leaf_nodes.reserve(indices.size());
    for (auto& triidx : indices) {
        leaf_nodes.push_back({{triidx[0].get_index(), triidx[1].get_index(), triidx[2].get_index()}});
    }
    // clear memory first
    bounding_volume_tree = bv_tree(positions, std::move(leaf_nodes)); // fixme later give triangle indices directly!
}

/// handle triangle strips
template<class AddFunc>
unsigned prepare_tri_strip(const std::vector<std::array<mesh::vertex_index, 3>>& indices, AddFunc add)
{
    // two triangles with indices A B C | D E F can be combined
    // as a tri strip the edge B->C occurs in reversed order
    // so D E F = C B x or B x C or x C B.
    // would be encoded as (with x = D):
    // A B C | C B D
    // Note that the tri strip switches orientation of triangles
    // every second triangle (clockwise vs counterclockwise)
    // if the last three indices are taken as triangle.
    // If the two triangles share no common vertices in that way,
    // we have to insert degenerate triangles to move from one
    // triangle to the other. Degenerate triangles have indices that
    // are not unique. It takes 4 degenerate triangles (4 triangle strip
    // indices) to encode that transition.
    // So A B C | D E F will become as tri strip:
    // A B C C D D E F
    if (indices.empty()) {
        return 0;
    }
    add(indices[0][0].get_index(), 0);
    add(indices[0][1].get_index(), 1);
    add(indices[0][2].get_index(), 2);
    unsigned nr_strip_indices    = 3; // first triangle
    unsigned last_two_indices[2] = {indices[0][1].get_index(), indices[0][2].get_index()};
    for (unsigned i = 1; i < unsigned(indices.size()); ++i) {
        const auto correction = nr_strip_indices & 1;
        const auto ltii       = 1 - correction;
        // index into that array is swapped every run, but also B,C are swapped,
        // so we can access always the same values.
        const auto B = last_two_indices[0];
        const auto C = last_two_indices[1];
        const auto D = indices[i][0].get_index();
        const auto E = indices[i][1].get_index();
        const auto F = indices[i][2].get_index();
        if (B == E && C == D) {
            // strip can be encoded with one value
            add(F, nr_strip_indices);
            last_two_indices[ltii] = F;
            ++nr_strip_indices;
        } else if (B == F && C == E) {
            // strip can be encoded with one value
            add(D, nr_strip_indices);
            last_two_indices[ltii] = D;
            ++nr_strip_indices;
        } else if (B == D && C == F) {
            // strip can be encoded with one value
            add(E, nr_strip_indices);
            last_two_indices[ltii] = E;
            ++nr_strip_indices;
        } else {
            // need two dummy indices and three new for this triangle
            add(C, nr_strip_indices);
            add(D, nr_strip_indices + 1);
            add(D, nr_strip_indices + 2);
            add(E, nr_strip_indices + 3);
            add(F, nr_strip_indices + 4);
            last_two_indices[correction] = E;
            last_two_indices[ltii]       = F;
            nr_strip_indices += 5;
        }
    }
    return nr_strip_indices;
}

/// determine number of indices that will be needed for a triangle strip
/// representation of this mesh.
unsigned mesh::compute_tri_strip_size() const
{
    return prepare_tri_strip(indices, [](unsigned, unsigned) {});
}

/// generate indices for a triangle strip that resembles this mesh.
std::vector<uint32_t> mesh::generate_tri_strip() const
{
    // two triangles with indices A B C | D E F can be combined
    // as a tri strip the edge B->C occurs in reversed order
    // so D E F = C B x or B x C or x C B.
    // would be encoded as (with x = D):
    // A B C | C B D
    // Note that the tri strip switches orientation of triangles
    // every second triangle (clockwise vs counterclockwise)
    // if the last three indices are taken as triangle.
    // If the two triangles share no common vertices in that way,
    // we have to insert degenerate triangles to move from one
    // triangle to the other. Degenerate triangles have indices that
    // are not unique. It takes 4 degenerate triangles (4 triangle strip
    // indices) to encode that transition.
    // So A B C | D E F will become as tri strip:
    // A B C C D D E F
    std::vector<uint32_t> result(compute_tri_strip_size());
    if (indices.empty()) {
        return result;
    }
    prepare_tri_strip(indices, [&result](unsigned index, unsigned offset) { result[offset] = index; });
    return result;
}

/// compute for every vertex if it is on the border of the mesh. Needs correct
/// adjacency data!
std::vector<bool> mesh::compute_vertex_on_border_data() const
{
    std::vector<bool> on_border(positions.size(), false);
    for (unsigned i = 0; i < get_nr_of_triangles(); ++i) {
        for (unsigned k = 0; k < 3; ++k) {
            if (get_adjacent_triangle(i, k) == triangle_index()) {
                on_border[indices[i][k].get_index()]           = true;
                on_border[indices[i][(k + 1) % 3].get_index()] = true;
            }
        }
    }
    return on_border;
}

/// Load mesh from DDXML node, @param elem the "mesh" node
void mesh::load(const xml_elem& elem)
{
    // clear all
    *this = mesh();
    // read all data
    name = elem.attr("name");
    // material: just read and store material id!
    if (elem.has_attr("material")) {
        material_id = elem.attru("material") + 1;
    }
    // vertices
    auto verts   = elem.child("vertices");
    auto nrverts = verts.attru("nr");
    auto values  = verts.child_text();
    positions.reserve(nrverts);
    std::string::size_type valuepos = 0;
    for (unsigned i = 0; i < nrverts; ++i) {
        float x, y, z;
        // no stream here because of NaN strings that would break the stream
        auto value = next_part_of_string(values, valuepos);
        x          = float(atof(value.c_str()));
        value      = next_part_of_string(values, valuepos);
        y          = float(atof(value.c_str()));
        value      = next_part_of_string(values, valuepos);
        z          = float(atof(value.c_str()));
        positions.push_back(vector3f(x, y, z));
    }
    // indices
    auto indis   = elem.child("indices");
    auto nrindis = indis.attru("nr");
    values       = indis.child_text();
    indices.resize(nrindis / 3);
    std::istringstream issi(values);
    for (unsigned i = 0; i < nrindis; ++i) {
        unsigned idx{};
        issi >> idx;
        if (idx >= nrverts)
            THROW(xml_error, std::string("vertex index out of range, mesh ") + name, elem.doc_name());
        indices[i / 3][i % 3] = vertex_index(idx);
    }
    // tex coords
    if (elem.has_child("texcoords")) {
        auto texcs = elem.child("texcoords");
        texcoords.reserve(nrverts);
        values                          = texcs.child_text();
        std::string::size_type valuepos = 0;
        for (unsigned i = 0; i < nrverts; ++i) {
            float x, y;
            // no stream here because of NaN strings that would break the stream
            auto value = next_part_of_string(values, valuepos);
            x          = float(atof(value.c_str()));
            value      = next_part_of_string(values, valuepos);
            y          = float(atof(value.c_str()));
            texcoords.push_back(vector2f(x, y));
        }
    }
    // normals
    if (elem.has_child("normals")) {
        normals.reserve(nrverts);
        values                          = elem.child("normals").child_text();
        std::string::size_type valuepos = 0;
        for (unsigned i = 0; i < nrverts; ++i) {
            float x, y, z;
            // no stream here because of NaN strings that would break the stream
            std::string value = next_part_of_string(values, valuepos);
            x                 = float(atof(value.c_str()));
            value             = next_part_of_string(values, valuepos);
            y                 = float(atof(value.c_str()));
            value             = next_part_of_string(values, valuepos);
            z                 = float(atof(value.c_str()));
            normals.push_back(vector3f(x, y, z));
        }
    }
    // compute normals and possible missing other data, same as constructor
    if (normals.empty()) {
        compute_normals();
    }
    if (!texcoords.empty() && texcoords.size() != positions.size()) {
        THROW(error, "texcoord count doesn't match position count");
    }
    if (!normals.empty() && normals.size() != normals.size()) {
        THROW(error, "normals count doesn't match position count");
    }
    if (!tangentsx.empty() && tangentsx.size() != positions.size()) {
        THROW(error, "tangentsx count doesn't match position count");
    }
    if (!righthanded.empty() && righthanded.size() != positions.size()) {
        THROW(error, "righthanded count doesn't match position count");
    }
    if (righthanded.empty() && !tangentsx.empty()) {
        righthanded.resize(positions.size(), true);
    }
}

/// Save mesh to DDxml node
xml_elem mesh::save(xml_elem& parent) const
{
    auto msh = parent.add_child("mesh");
    msh.set_attr(name, "name");

    // material.
    if (material_id != 0) {
        msh.set_attr(material_id - 1, "material");
    }

    // vertices.
    auto verts = msh.add_child("vertices");
    verts.set_attr(get_nr_of_vertices(), "nr");
    std::ostringstream ossv;
    for (const auto& v : positions) {
        ossv << v.x << " " << v.y << " " << v.z << " ";
    }
    verts.add_child_text(ossv.str());

    // indices.
    auto indis = msh.add_child("indices");
    indis.set_attr(get_nr_of_triangles() * 3, "nr");
    // need to convert to string for proper type selection of overloaded
    // function
    indis.set_attr("triangles", "type");
    std::ostringstream ossi;
    for (auto& idx : indices) {
        ossi << idx[0].get_index() << " " << idx[1].get_index() << " " << idx[2].get_index() << " ";
    }
    indis.add_child_text(ossi.str());

    // texcoords.
    if (!texcoords.empty()) {
        auto texcs = msh.add_child("texcoords");
        std::ostringstream osst;
        for (const auto& tc : texcoords) {
            osst << tc.x << " " << tc.y << " ";
        }
        texcs.add_child_text(osst.str());
    }

    // normals.
    if (!normals.empty()) {
        auto nrmls = msh.add_child("normals");
        std::ostringstream ossn;
        for (const auto& nrml : normals) {
            ossn << nrml.x << " " << nrml.y << " " << nrml.z << " ";
        }
        nrmls.add_child_text(ossn.str());
    }
    return msh;
}
