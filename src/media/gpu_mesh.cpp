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

#include "gpu_mesh.hpp"

#include "gpu_helper.hpp"

/// Constructor to display mesh with material programs (normal/mirror clip)
gpu::mesh::mesh(
    const ::mesh& m,
    const gpu::program& material_prog,
    const gpu::program& material_underwater_prog,
    const gpu::program& material_mirrorclip_prog,
    const gpu::program& material_silhuette_prog,
    const uniform_buffer& material_ubo,
    const scene& myscene_)
    : default_program(material_prog)
    , underwater_program(material_underwater_prog)
    , mirrorclip_program(material_mirrorclip_prog)
    , silhuette_program(material_silhuette_prog)
    , myscene(myscene_)
{
    init(m, material_prog, material_ubo);
}

/// Constructor to display mesh with custom program
gpu::mesh::mesh(
    const ::mesh& m,
    const gpu::program& material_custom_prog,
    const uniform_buffer& material_ubo,
    const scene& myscene_)
    : default_program(material_custom_prog)
    , underwater_program(material_custom_prog)
    , mirrorclip_program(material_custom_prog)
    , silhuette_program(material_custom_prog)
    , myscene(myscene_)
{
    init(m, material_custom_prog, material_ubo);
}

/// Display a mesh with transformation accumulated so far (camera / parent
/// objects)
void gpu::mesh::display(const matrix4& transformation)
{
    display_generic(default_program, transformation);
}

/// Display a mesh with transformation accumulated so far (camera / parent
/// objects)
void gpu::mesh::display_under_water(const matrix4& transformation)
{
    display_generic(underwater_program, transformation);
}

/// Display a mesh clipped and mirrored at z=0 plane with transformation
/// accumulated so far (camera / parent objects)
void gpu::mesh::display_mirror_clip(const matrix4& transformation)
{
    display_generic(mirrorclip_program, transformation);
}

/// Display a mesh with transformation accumulated so far (camera / parent objects),
/// but only the silhuette
void gpu::mesh::display_silhuette(const matrix4& transformation)
{
    // fixme add here some special z=0 transformation? or we can do that in the shader...
    display_generic(silhuette_program, transformation);
}

/// Set the textures and samples to use in the render context
void gpu::mesh::set_textures_and_samplers(const gpu::render_context::textures_and_samplers& textures_and_samplers)
{
    render_ctx.add(textures_and_samplers);
}

/// Method to display the mesh with genericity (program can be chosen)
void gpu::mesh::display_generic(const program& prg, const matrix4& transformation)
{
    transform_data td;
    matrix4 total_transform = myscene.get_current_camera().get_transformation() * transformation;
    td.projection_modelview = matrix4f(myscene.get_current_camera().get_projection_matrix() * total_transform);
    td.modelview_inverse    = matrix4f(total_transform.inverse());
    transform_ubo.update_data(td);
    render_ctx.add(prg);
    render_ctx.render();
}

/// Called by constructor for initialization
void gpu::mesh::init(const ::mesh& m, const program& ctx_init_program, const uniform_buffer& material_ubo)
{
    // transfer data to buffers and bind data to buffers
    vertex_buffer vbo_positions; ///< Vertex buffer for positions
    vbo_positions.init(m.get_positions());
    render_ctx.add(unsigned(basic_shader_attribute_location::position), std::move(vbo_positions));
    if (!m.get_normals().empty()) {
        vertex_buffer vbo_normals; ///< Vertex buffer for normals
        vbo_normals.init(m.get_normals());
        render_ctx.add(unsigned(basic_shader_attribute_location::normal), std::move(vbo_normals));
    }
    if (!m.get_texcoords().empty()) {
        vertex_buffer vbo_texcoords; ///< Vertex buffer for texture coordinates
        vbo_texcoords.init(m.get_texcoords());
        render_ctx.add(unsigned(basic_shader_attribute_location::texcoord), std::move(vbo_texcoords));
    }
    if (!m.get_tangentsx().empty()) {
        vertex_buffer vbo_tangents; ///< Vertex buffer for tangents
        vbo_tangents.init(m.get_tangentsx());
        render_ctx.add(unsigned(basic_shader_attribute_location::tangentx), std::move(vbo_tangents));
    }
    if (!m.get_righthanded().empty()) {
        vertex_buffer vbo_righthanded; ///< Vertex buffer for righthanded info
        vbo_righthanded.init(m.get_righthanded());
        render_ctx.add(unsigned(basic_shader_attribute_location::righthanded), std::move(vbo_righthanded));
    }

    index_buffer index_data; ///< Index buffer for primitives
    index_data.init(m.get_indices());
    render_ctx.add(std::move(index_data));
    render_ctx.add(ctx_init_program);
    render_ctx.add(primitive_type::triangles, m.get_nr_of_triangles() * 3);
    render_ctx.add(unsigned(basic_shader_uniform_location::light), myscene.get_light_ubo());
    render_ctx.add(unsigned(basic_shader_uniform_location::fog), myscene.get_fog_ubo());
    render_ctx.add(unsigned(basic_shader_uniform_location::clipplane), myscene.get_clipplane_ubo());
    render_ctx.add(unsigned(basic_shader_uniform_location::material), material_ubo);
    transform_ubo.init(buffer::usage_type::dynamic_draw, transform_data());
    render_ctx.add(unsigned(gpu::basic_shader_uniform_location::transform), transform_ubo);
    render_ctx.init();
}
