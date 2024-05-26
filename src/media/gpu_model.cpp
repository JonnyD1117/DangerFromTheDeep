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

// A 3d model displayer
// (C)+(W) by Thorsten Jordan. See LICENSE

#include "gpu_model.hpp"

#include "datadirs.hpp"
#include "model.hpp"
#include "model_state.hpp"

// class wide statics.
unsigned gpu::model::init_count = 0;
std::unordered_map<gpu::basic_shader_feature, gpu::program> gpu::model::default_programs;
gpu::texture_array gpu::model::caustics;

/// Constructor to display given model
gpu::model::model(const ::model& m, const scene& myscene_)
    : mymodel(m)
    , myscene(myscene_)
    , texture_store(mymodel.get_filesystem_path())
{
    if (init_count++ == 0) {
        render_init();
    }

    // Initialize all necessary programs to render materials and also the UBOs
    // for material data
    materials.resize(m.get_nr_of_materials());
    for (unsigned i = 0; i < m.get_nr_of_materials(); ++i) {
        const auto& mat = m.get_material(i);
        if (!mat.get_shader_base_filename().empty()) {
            materials[i].myprogram = gpu::program(mat.get_shader_base_filename());
        }
        material_data md;
        md.common_color   = mat.get_diffuse_color();
        md.shininess      = mat.get_shininess();
        md.specular_color = mat.get_specular_color().vec3();
        materials[i].data_ubo.init(buffer::usage_type::static_draw, md);
    }

    // Initialize meshes from model
    const unsigned nr_of_meshes = m.get_nr_of_meshes();
    meshes.reserve(nr_of_meshes);
    for (unsigned i = 0; i < nr_of_meshes; ++i) {
        auto material_index = mymodel.get_mesh(i).get_material_id();
        if (material_index == 0) {
            THROW(error, "mesh without material index!");
        }
        --material_index;
        // custom shader is given, use it for all kinds of rendering
        const auto& mymaterial     = materials[material_index];
        const auto& custom_program = mymaterial.myprogram;
        if (!custom_program.empty()) {
            meshes.push_back(gpu::mesh(m.get_mesh(i), custom_program, mymaterial.data_ubo, myscene));
        } else {
            // determine shader programs for mesh, normal, underwater,
            // mirrorclip
            const auto& modelmaterial = mymodel.get_material(material_index);
            gpu::program* mat_prog    = nullptr;
            gpu::program* mat_uw_prog = nullptr;
            gpu::program* mat_mc_prog = nullptr;
            auto add                  = [](gpu::basic_shader_feature b0, gpu::basic_shader_feature b1) {
                return gpu::basic_shader_feature(int(b0) | int(b1));
            };
            auto bb = gpu::basic_shader_feature::lighting;
            if (modelmaterial.has_map(::model::map_type::diffuse)) {
                auto bc = add(bb, gpu::basic_shader_feature::colormap);
                if (modelmaterial.has_map(::model::map_type::normal)) {
                    auto bn = add(bc, gpu::basic_shader_feature::normalmap);
                    if (modelmaterial.has_map(::model::map_type::specular)) {
                        auto bs     = add(bn, gpu::basic_shader_feature::specularmap);
                        mat_prog    = &get_default_program(add(bs, gpu::basic_shader_feature::fog));
                        mat_uw_prog = &get_default_program(add(bs, gpu::basic_shader_feature::underwater));
                        mat_mc_prog = &get_default_program(add(bc, gpu::basic_shader_feature::clipplane));
                    } else {
                        mat_prog    = &get_default_program(add(bn, gpu::basic_shader_feature::fog));
                        mat_uw_prog = &get_default_program(add(bn, gpu::basic_shader_feature::underwater));
                        mat_mc_prog = &get_default_program(add(bc, gpu::basic_shader_feature::clipplane));
                    }
                } else {
                    mat_prog    = &get_default_program(add(bc, gpu::basic_shader_feature::fog));
                    mat_uw_prog = &get_default_program(add(bc, gpu::basic_shader_feature::underwater));
                    mat_mc_prog = &get_default_program(add(bc, gpu::basic_shader_feature::clipplane));
                }
            } else {
                mat_prog    = &get_default_program(add(bb, gpu::basic_shader_feature::fog));
                mat_uw_prog = &get_default_program(add(bb, gpu::basic_shader_feature::underwater));
                mat_mc_prog = &get_default_program(add(bb, gpu::basic_shader_feature::clipplane));
            }
            auto& mat_silhuette =
                get_default_program({}); // fixme check that this creates really an empty program. Just render black!
                                         // Most probably we need a special program for this.
            // at least scaling to z=0 will be missing... and maybe we don't need z-buffer checks?
            meshes.push_back(gpu::mesh(
                m.get_mesh(i), *mat_prog, *mat_uw_prog, *mat_mc_prog, mat_silhuette, mymaterial.data_ubo, myscene));
        }
    }
}

/// Destructor to free stuff when last model is gone
gpu::model::~model()
{
    if (--init_count == 0) {
        render_deinit();
    }
}

/// Prepare texture and sampler values for all materials
std::vector<gpu::render_context::textures_and_samplers>
gpu::model::prepare_textures_and_samplers(const std::string& layout)
{
    std::vector<gpu::render_context::textures_and_samplers> texsamp(mymodel.get_nr_of_materials());
    // reference all texture maps of new layout and unref all of old layout.
    // Iterate over all materials and get names of maps for new layout.
    // per material build texture and sampler lists
    for (unsigned material_index = 0; material_index < mymodel.get_nr_of_materials(); ++material_index) {
        const auto& mat               = mymodel.get_material(material_index);
        const auto is_default_program = mat.get_shader_base_filename().empty();
        texsamp[material_index].resize(mat.get_maps().size());
        for (unsigned map_index = 0; map_index < unsigned(mat.get_maps().size()); ++map_index) {
            const auto& mmap = mat.get_maps()[map_index];
            if (mmap.empty()) {
                // clear texture/sampler
                texsamp[material_index][map_index] = {nullptr, sampler::type::number};
            } else {
                const auto& new_filename = mmap.get_filename_for_layout(layout);
                auto filename_for_ref    = new_filename;
                float bump_height        = -1.f;
                if (map_index == unsigned(::model::map_type::normal) && mmap.has_bump_height()) {
                    // create normal map from bump map!
                    bump_height = mmap.get_bump_height();
                    filename_for_ref += "/bump/" + helper::str(bump_height);
                }
                // texture construction parameters and sampler type
                // depends on map type.
                bool use_mipmaps     = is_default_program;
                bool use_compression = false; // fixme when to use?
                // get texture reference and store it - will only create it if not existing yet!
                texsamp[material_index][map_index].first = texture_store.create(
                    filename_for_ref,
                    mymodel.get_filesystem_path() + new_filename,
                    data_type::ubyte,
                    use_mipmaps,
                    use_compression,
                    bump_height);
                auto dst = use_mipmaps ? sampler::type::trilinear_clamp : sampler::type::bilinear_clamp;
                texsamp[material_index][map_index].second = dst;
            }
        }
    }
    return texsamp;
}

/// Generic display method
void gpu::model::display_generic(const model_state& ms, mesh_display_method mdm)
{
    auto textures_and_samplers = prepare_textures_and_samplers(ms.get_layout());
    mymodel.iterate_objects(
        0, ms.get_transformation(), [&](unsigned object_index, const matrix4& parent_transformation) {
            const auto object_transformation = parent_transformation * ms.get_object_local_transformation(object_index);
            if (mymodel.has_object_a_mesh(object_index)) {
                const auto mesh_index     = mymodel.get_mesh_index_of_object(object_index);
                const auto material_index = mymodel.get_mesh(mesh_index).get_material_id();
                if (material_index == 0) {
                    THROW(error, "no material for mesh set!");
                }
                meshes[mesh_index].set_textures_and_samplers(textures_and_samplers[material_index - 1]);
                mdm(&meshes[mesh_index], object_transformation);
            }
            return object_transformation;
        });
}

/// Display the whole model with transformation accumulated so far (camera)
void gpu::model::display(const model_state& ms)
{
    display_generic(ms, &gpu::mesh::display);
}

/// Display the whole model with transformation accumulated so far (camera)
void gpu::model::display_under_water(const model_state& ms)
{
    display_generic(ms, &gpu::mesh::display_under_water);
}

/// Display a whole model clipped and mirrored and clipped at z=0 plane with
/// transformation accumulated so far (camera)
void gpu::model::display_mirror_clip(const model_state& ms)
{
    display_generic(ms, &gpu::mesh::display_mirror_clip);
}

/// Display a whole model as silhuette with
/// transformation accumulated so far (camera)
void gpu::model::display_silhuette(const model_state& ms)
{
    display_generic(ms, &gpu::mesh::display_silhuette);
}

/// Initialize global render data
void gpu::model::render_init()
{
    // uniform locations are the same for all shaders.
    // programs are created on demand.
    // fixme create caustics texture array here!
}

/// Deinitialize global render data
void gpu::model::render_deinit()
{
    default_programs.clear();
    caustics = gpu::texture_array();
}

gpu::program& gpu::model::get_default_program(gpu::basic_shader_feature bsf)
{
    auto pib = default_programs.insert(std::make_pair(bsf, gpu::program()));
    if (pib.second) {
        pib.first->second = gpu::make(gpu::generate_basic_shader_source(bsf));
    }
    return pib.first->second;
}
