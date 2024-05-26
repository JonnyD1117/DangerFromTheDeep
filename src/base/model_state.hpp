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

#pragma once

//#include "model.hpp"//fixme only when model is in base
#include "quaternion.hpp"

/// Represents a 3D model state - values for all changeable transformations and
/// layout
class model_state
{
  public:
    /// Default c'tor
    model_state() = default;
    /// Construct with given model
    model_state(const model& m, const std::string& layout_);
    /// Set animation values for this model's object
    void set_object_parameters(unsigned object_index, float translation, float angle);
    /// Set general transformation for model - must be done ONCE per FRAME, i.e.
    /// in simulate call or in class game
    void compute_transformation(const vector3& position, const quaternion& orientation);
    /// Get the current layout
    [[nodiscard]] const auto& get_layout() const { return layout; }
    /// Get the set transformation
    [[nodiscard]] const auto& get_transformation() const { return transformation; }
    /// Get the parameters for an object
    [[nodiscard]] const auto& get_object_parameters(unsigned object_index) const;
    /// Get transformation of the object itself without parent transformation
    matrix4 get_object_local_transformation(unsigned object_index) const;
    /// Get the bv_tree parameter object for a certain object index
    bv_tree::param get_bv_tree_param_of_object(unsigned index) const;
    /// Get the bv_tree parameter object for the main object
    bv_tree::param get_bv_tree_param_of_main_object() const;
    /// Get object transformation without modelstate transformation
    matrix4 get_object_transformation_without_state(unsigned index) const;
    /// Get absolute transformation of main object
    matrix4 get_main_object_transformation() const;
    /// Get absolute transformation of main object
    matrix4 get_main_object_transformation_without_translation() const;
    /// Get absolute transformation of requested object
    matrix4 get_object_transformation(unsigned index) const;
    /// Set a layout
    void set_layout(const std::string& layout_);

  protected:
    /// Pointer to the model
    // const model* mymodel{nullptr};//fixme
    /// the selected model layout name
    std::string layout; // fixme {model::default_layout};
    /// per object ID and translation/angle
    std::vector<vector2f> object_parameters;
    /// the transformation matrix to use for the model, computed from object
    /// position/orientation
    matrix4 transformation;
};
