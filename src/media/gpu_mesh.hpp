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

// Displayer for a mesh
// (C)+(W) by Thorsten Jordan. See LICENSE

#pragma once

#include "gpu_helper.hpp"
#include "mesh.hpp"

/// A viewer or gpu representation for mesh class
namespace gpu
{
class mesh
{
  public:
    mesh(
        const ::mesh& m,
        const program& material_prog,
        const program& material_underwater_prog,
        const program& material_mirrorclip_prog,
        const program& material_silhuette_prog,
        const uniform_buffer& material_ubo,
        const scene& myscene);
    mesh(
        const ::mesh& m,
        const program& material_custom_prog,
        const uniform_buffer& material_ubo,
        const scene& myscene);
    void display(const matrix4& transformation);
    void display_under_water(const matrix4& transformation);
    void display_mirror_clip(const matrix4& transformation);
    void display_silhuette(const matrix4& transformation);
    void set_textures_and_samplers(const gpu::render_context::textures_and_samplers& textures_and_samplers);

  protected:
    uniform_buffer transform_ubo;      ///< Transformation data to display mesh with
                                       ///< current camera
    render_context render_ctx;         ///< Way to render all data, program is changed
                                       ///< depending on display method
    const program& default_program;    ///< Shader program for default material rendering
    const program& underwater_program; ///< Shader program for underwater
                                       ///< rendering of material
    const program& mirrorclip_program; ///< Shader program for mirrorclip
                                       ///< rendering of material
    const program& silhuette_program;  ///< Shader program for silhuette
                                       ///< rendering of material
    const scene& myscene;              ///< Reference to the scene the mesh is displayed in

    void display_generic(const program& prg, const matrix4& transformation);
    void init(const ::mesh& m, const program& ctx_init_program, const uniform_buffer& material_ubo);

  private:
    mesh() = delete;
};

/// provide data type deduction
template<>
inline data_type to_data_type(::mesh::vertex_index)
{
    return data_type::u32;
}
} // namespace gpu
