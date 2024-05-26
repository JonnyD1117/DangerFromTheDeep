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

#pragma once

#include "gpu_helper.hpp"
#include "gpu_interface.hpp"
#include "gpu_mesh.hpp"
#include "object_store.hpp"

#include <unordered_map>
class model;
class model_state;

/// Represents a 3D model display
namespace gpu
{
class model
{
  public:
    model(const ::model& m, const scene& myscene);
    ~model();
    void display(const model_state& ms);
    void display_under_water(const model_state& ms);
    void display_mirror_clip(const model_state& ms);
    void display_silhuette(const model_state& ms);

  protected:
    using mesh_display_method = std::function<void(mesh*, const matrix4&)>;

    std::vector<gpu::render_context::textures_and_samplers> prepare_textures_and_samplers(const std::string& layout);
    void display_generic(const model_state& ms, mesh_display_method mdm);

    std::vector<mesh> meshes; ///< All meshes of the model
    const ::model& mymodel;   ///< The model the viewer relates to
    const scene& myscene;     ///< The scene the model is in
    /// Representation of a model::material
    struct material
    {
        program myprogram;       ///< When custom shader is used the program for it
        uniform_buffer data_ubo; ///< Data about material: colors, shininess
    };
    std::vector<material> materials;     ///< The materials used
    object_store<texture> texture_store; ///< All textures of the model in a store

    // fixme set caustic animation time once globally by game time! have caustic
    // animation number here? or fraction of animation [0...1] as float here?
    // with static function to set it? or have that in scene class???

    static unsigned init_count; ///< Class wide init count
    static void render_init();
    static void render_deinit();
    static program& get_default_program(basic_shader_feature bsf);
    static std::unordered_map<basic_shader_feature, program> default_programs; ///< Default shader programs
    static texture_array caustics; ///< texture array with caustic data for default programs

  private:
    model() = delete;
};
} // namespace gpu
