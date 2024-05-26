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

// A 3d voxel representation
// (C)+(W) by Thorsten Jordan. See LICENSE

#pragma once

#include "box.hpp"
#include "error.hpp"
#include "vector3.hpp"

#include <array>
#include <vector>
class xml_elem;

/// voxel representation, the space of a model is partitioned in subspaces
class voxel
{
  public:
    /// position of center of voxel relative to the base mesh
    vector3f relative_position;

    /// part of voxel that is filled with model volume (0...1)
    float part_of_volume;

    /// third root of part_of_volume, used for collision detection
    float root3_part_of_volume;

    /// relative mass of the voxel of total mass (0...1)
    float relative_mass;

    /// relative volume of the voxel of total volume (0...1)
    float relative_volume;

    /// indices of neighbouring voxels: top, left, forward, right, backward,
    /// bottom, -1 means no neighbour
    std::array<int, 6> neighbour_idx;

    /// construct a voxel
    voxel(const vector3f& rp, float pv, float m, float rv);
};

/// voxel data container
class voxel_data
{
  public:
    vector3i voxel_resolution;     ///< nr of voxels in every dimension
    vector3f voxel_size;           ///< size of a voxel in 3-space
    float voxel_radius;            ///< "radius" of a voxel in 3-space
    double total_volume_by_voxels; ///< total volume of model defined by voxels

    std::vector<voxel> voxels; ///< per voxel: relative 3d position and part of
                               ///< volume that is inside (0...1)

    std::vector<int> voxel_index_by_pos; ///< voxel for 3-space coordinate of
                                         ///< it, -1 if not existing

    /// get voxel data by position, may return 0 for not existing voxels
    [[nodiscard]] const voxel* get_voxel_by_pos(const vector3i& v) const
    {
        int i = voxel_index_by_pos[(v.z * voxel_resolution.y + v.y) * voxel_resolution.x + v.x];

        return (i >= 0) ? &voxels[i] : (const voxel*) nullptr;
    }

    /// Get a voxel
    voxel& operator[](const unsigned idx) { return voxels[idx]; }
    /// Get a voxel
    const voxel& operator[](const unsigned idx) const { return voxels[idx]; }

    /// Get number of voxels
    auto get_nr_of_voxels() const { return unsigned(voxels.size()); }

    /// get voxel closest to a real world position
    unsigned get_voxel_closest_to(const vector3f& pos);

    /// get voxels within a sphere around a real world position
    ///@returns list of voxels which center is inside the sphere
    std::vector<unsigned> get_voxels_within_sphere(const vector3f& pos, double radius);

    void load(const xml_elem& ve, const boxf& bbox, double volume);
};
