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

// interface class to compute terrain data

#pragma once

/// Interface class to generate heights and colors (texture map) for terrain
class terrain_generator
{
  public:
    /// Destructor
    virtual ~terrain_generator() { }
    /// Get number of detail levels that the generator can deliver
    virtual unsigned get_nr_of_available_levels() const = 0;
    /// Get spacing in world coordinates for samples in coarsest detail level
    virtual double get_coarsest_level_sample_spacing() const = 0;
    /** Generate height values for terrain
     * @param level the detail level to be used
     * @param ar world area to be filled in per level coordinates
     */
    virtual imagef get_height_values(unsigned level, const area& ar) = 0;
    /** Generate color (texture map) values for terrain
     * @param level the detail level to be used
     * @param ar world area to be filled in per level coordinates - Note that
     * resulting image is larger than area depending on color scale factor!
     */
    virtual image get_color_values(unsigned level, const area& ar) = 0;
    /// Get the resolution factor between heights and colors as power of two
    virtual unsigned get_color_to_height_resolution_factor_exp() const = 0;
    /// get absolute min/max height of all levels to be used for clipping
    virtual vector2 get_min_max_height() const = 0;
};
