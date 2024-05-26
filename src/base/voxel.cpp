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

// A 3d model
// (C)+(W) by Thorsten Jordan. See LICENSE

#include "voxel.hpp"

#include "box.hpp"
#include "error.hpp"
#include "vector3.hpp"
#include "xml.hpp"

#include <cmath>
#include <sstream>

voxel::voxel(const vector3f& rp, float pv, float m, float rv)
    : relative_position(rp)
    , part_of_volume(pv)
    , root3_part_of_volume(std::pow(pv, static_cast<float>(1.0 / 3.0)))
    , relative_mass(m)
    , relative_volume(rv)
{
}

/// Read voxel data from xml file
void voxel_data::load(const xml_elem& ve, const boxf& bbox, double volume)
{
    voxel_resolution = vector3i(ve.attri("x"), ve.attri("y"), ve.attri("z"));

    const auto nrvoxels = voxel_resolution.x * voxel_resolution.y * voxel_resolution.z;

    voxels.reserve(ve.attru("innr"));

    std::vector<float> insidevol(nrvoxels);
    std::istringstream iss3(ve.child_text());

    for (int k = 0; k < nrvoxels; ++k) {
        iss3 >> insidevol[k];
    }

    if (iss3.fail()) {
        THROW(file_context_error, "error reading inside volume data", ve.doc_name());
    }

    std::vector<float> massdistri;

    if (ve.has_child("mass-distribution")) {
        std::istringstream iss4(ve.child("mass-distribution").child_text());
        massdistri.resize(nrvoxels);

        for (int k = 0; k < nrvoxels; ++k) {
            iss4 >> massdistri[k];
        }

        if (iss4.fail()) {
            THROW(file_context_error, "error reading mass distribution data", ve.doc_name());
        }
    }

    const auto& bsize = bbox.size();
    const auto& bmin  = bbox.minpos;

    voxel_size = vector3f(bsize.x / voxel_resolution.x, bsize.y / voxel_resolution.y, bsize.z / voxel_resolution.z);

    const double voxel_volume = voxel_size.x * voxel_size.y * voxel_size.z;

    total_volume_by_voxels = ve.attrf("invol") * voxel_volume;

    voxel_radius = float(pow(voxel_volume * 3.0 / (4.0 * constant::PI),
                             1.0 / 3)); // sphere of same volume

    auto ptr              = 0;
    float mass_part_sum   = 0;
    const auto volume_rcp = 1.0 / volume;

    voxel_index_by_pos.resize(voxel_resolution.x * voxel_resolution.y * voxel_resolution.z, -1);

    for (int izz = 0; izz < voxel_resolution.z; ++izz) {
        // quick test hack, linear distribution top->down 0->1
        float mass_part = (voxel_resolution.z - izz) / float(voxel_resolution.z);

        for (int iyy = 0; iyy < voxel_resolution.y; ++iyy) {
            for (int ixx = 0; ixx < voxel_resolution.x; ++ixx) {
                float f = insidevol[ptr];

                if (f >= 1.0F / 255.0F) {
                    voxel_index_by_pos[ptr] = int(voxels.size());
                    float m                 = f * mass_part;

                    if (!massdistri.empty()) {
                        m = massdistri[ptr];
                    }

                    voxels.emplace_back(
                        vector3f(vector3(
                            ixx + 0.5 + bmin.x / voxel_size.x,
                            iyy + 0.5F + bmin.y / voxel_size.y,
                            izz + 0.5F + bmin.z / voxel_size.z)),
                        f,
                        m,
                        float(f * voxel_volume * volume_rcp));

                    mass_part_sum += m;
                }
                ++ptr;
            }
        }
    }

    // renormalize mass parts
    if (massdistri.empty()) {
        for (auto& elem : voxels) {
            elem.relative_mass /= mass_part_sum;
        }
    }

    // compute neighbouring information
    ptr       = 0;
    int dx[6] = {0, -1, 0, 1, 0, 0};
    int dy[6] = {0, 0, 1, 0, -1, 0};
    int dz[6] = {1, 0, 0, 0, 0, -1};

    for (int izz = 0; izz < voxel_resolution.z; ++izz) {
        for (int iyy = 0; iyy < voxel_resolution.y; ++iyy) {
            for (int ixx = 0; ixx < voxel_resolution.x; ++ixx) {
                int revvi = voxel_index_by_pos[ptr];

                if (revvi >= 0) {
                    // there is a voxel at that position
                    for (int k = 0; k < 6; ++k) {
                        int nx = ixx + dx[k];
                        int ny = iyy + dy[k];
                        int nz = izz + dz[k];

                        if (nx >= 0 && ny >= 0 && nz >= 0 && nx < voxel_resolution.x && ny < voxel_resolution.y
                            && nz < voxel_resolution.z) {
                            int ng = voxel_index_by_pos[(nz * voxel_resolution.y + ny) * voxel_resolution.x + nx];
                            if (ng >= 0) {
                                voxels[revvi].neighbour_idx[k] = ng;
                                // DBGOUT8(ixx,iyy,izz,k,ng,nx,ny,nz);
                            }
                        }
                    }
                }
                ++ptr;
            }
        }
    }
}

/// Determine which voxel is closest to position
/* unused! fixme
unsigned model::get_voxel_closest_to(const vector3f& pos)
{
    matrix4f transmat = get_base_mesh_transformation() *
matrix4f::diagonal(voxel_size); unsigned closestvoxel = 0; double dist = 1e30;
    for (unsigned i = 0; i < voxel_data.size(); ++i) {
        vector3f p = transmat.mul4vec3xlat(voxel_data[i].relative_position);
        double d = p.square_distance(pos);
        if (d < dist) {
            dist = d;
            closestvoxel = i+1;
        }
    }
    if (!closestvoxel)
        THROW(error, "no voxel data available");
    return closestvoxel-1;
}
*/
