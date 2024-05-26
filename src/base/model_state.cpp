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

// A 3d model state
// (C)+(W) by Thorsten Jordan. See LICENSE

#include "model_state.hpp"

#include "helper.hpp"

/// Construct with given model
model_state::model_state(const model& m, const std::string& layout_)
    : mymodel(&m)
    , object_parameters(m.get_nr_of_objects())
    , transformation(matrix4::one()) // set neutral transformation
{
    for (unsigned i = 0; i < m.get_nr_of_objects(); ++i) {
        object_parameters[i] = m.get_object_transformation_parameters(i);
    }
    set_layout(layout_);
}

/// Set animation values for this model's object
void model_state::set_object_parameters(unsigned object_index, float translation, float angle)
{
    if (object_index >= object_parameters.size()) {
        THROW(error, "invalid object id");
    }
    // note: check constraints of model - would be good, but computation of
    // transformation checks it anyway
    object_parameters[object_index] = vector2f(translation, angle);
}

/// Set general transformation for model
void model_state::compute_transformation(const vector3& position, const quaternion& orientation)
{
    // create matrix with translational part and rotational part (don't apply
    // rotation on translation!)
    transformation = matrix4::trans(position) * orientation.rotmat4();
}

/// Request object parameters
const auto& model_state::get_object_parameters(unsigned object_index) const
{
    if (object_index >= object_parameters.size()) {
        THROW(error, "invalid object id");
    }
    return object_parameters[object_index];
}

/// Get transformation of the object itself without parent transformation
matrix4 model_state::get_object_local_transformation(unsigned object_index) const
{
    return mymodel->get_object_local_transformation(object_index, get_object_parameters(object_index));
}

bv_tree::param model_state::get_bv_tree_param_of_object(unsigned index) const
{
    // Create BV-Tree parameters for the object and it's mesh
    const auto& main_mesh = mymodel->get_mesh_of_object(index);
    return bv_tree::param(main_mesh.get_bv_tree(), main_mesh.get_positions(), get_main_object_transformation());
}

bv_tree::param model_state::get_bv_tree_param_of_main_object() const
{
    return get_bv_tree_param_of_object(mymodel->get_main_object_index());
}

matrix4 model_state::get_main_object_transformation() const
{
    return get_object_transformation(mymodel->get_main_object_index());
}

matrix4 model_state::get_object_transformation_without_state(unsigned index) const
{
    auto combined_transformations_to_requested_object = get_object_local_transformation(index);
    index                                             = mymodel->get_parent_object_index(index);
    while (index >= 0) {
        combined_transformations_to_requested_object =
            get_object_local_transformation(index) * combined_transformations_to_requested_object;
        index = mymodel->get_parent_object_index(index);
    }
    return combined_transformations_to_requested_object;
}

matrix4 model_state::get_object_transformation(unsigned index) const
{
    return transformation * get_object_transformation_without_state(index);
}

matrix4 model_state::get_main_object_transformation_without_translation() const
{
    auto transformation_without_position = transformation;
    transformation_without_position.clear_trans();
    return transformation_without_position * get_object_transformation_without_state(mymodel->get_main_object_index());
}

void model_state::set_layout(const std::string& layout_)
{
    // Check that requested skin is valid for the model!
    auto all_layouts = mymodel->get_all_layout_names();
    if (!helper::contains(all_layouts, layout_)) {
        THROW(error, std::string("layout ") + layout_ + " not known in model");
    }
    layout = layout_;
}
