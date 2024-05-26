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

// Base interface for user screens.
// subsim (C)+(W) Markus Petermann and Thorsten Jordan. SEE LICENSE

#pragma once

#include "angle.hpp"
#include "datadirs.hpp"
#include "input_event_handler.hpp"
#include "texture.hpp" // fixme replace by "gpu_interface.hpp"

#include <optional>
class image;
class xml_elem;

///\defgroup displays In-game user interface screens
///\brief Base class for a single screen of the ingame user interface.
///\ingroup displays
class user_display : public input_event_handler
{
  public:
    /// A 2D image element for normal or rotated image elements
    class elem2D
    {
      public:
        /// Construct from data file
        elem2D(
            const xml_elem& elem,
            const std::string& display_dir,
            const std::string& prefix_day,
            const std::string& prefix_night);
        /// Get number of phases
        auto nr_of_phases() const { return unsigned(tex.size()); }
        /// Set the phase to use for subimage in [0...numphases[
        void set_phase(unsigned phase_) const;
        /// Draw element (rotated/phased if defined)
        void draw() const;
        /// Get texture for user defined drawing
        auto& get_texture() const { return *tex[phase]; }
        /// Is Mouse over element? Does not check for rotation, just uses 2D
        /// area.
        bool is_mouse_over(const vector2i& mpos, int tolerance = 0) const;
        /// Initialize texture
        void init(bool is_day);
        /// Deinitialize texture
        void deinit();
        /// Get position
        const vector2i& get_position() const { return position; }
        /// For storage in vector
        elem2D() = default;
        /// Set the value
        void set_value(double v) const;
        /// Set the angle in valid range and thus value
        void set_angle(angle a) const;
        /// Get the set value
        auto get_value() const { return value; }
        /// Get the range of valid values
        auto get_value_range() const { return std::make_pair(start_value, end_value); }
        /// Set the value defined by mouse position @return new set value if
        /// valid
        std::optional<double> set_value(const vector2i& mpos) const;
        /// Set the value as unsigned defined by mouse position @return new set
        /// value if valid
        std::optional<unsigned> set_value_uint(const vector2i& mpos) const;
        /// Get the ID
        auto get_id() const { return id; }
        /// Get visibility
        auto is_visible() const { return visible; }
        /// Set visibility
        void set_visible(bool b) const
        {
            if (optional)
                visible = b;
        }
        /// Change filename for the element
        void set_filename(const std::string& fn, bool day = true, unsigned phase = 0);
        /// Get size - only valid after initialization or when defined in
        /// layout!
        auto get_size() const { return size; }

      protected:
        int id{-1};                               ///< ID for manipulation
        vector2i position;                        ///< Position (left/top) of the element on screen
        vector2i center;                          ///< Center of the element on screen (used for rotation)
        vector2i size;                            ///< Size used when no image exists (click only)
        bool rotateable{false};                   ///< Is this a rotateable element?
        bool clockwise{true};                     ///< Is rotation clockwise (default)?
        bool has_night{false};                    ///< Does night image data exist?
        mutable double value{0.0};                ///< The value to convert to angle
        angle start_angle;                        ///< For rotating elements the start angle
        double start_value{0.0};                  ///< For rotating elements the value at start angle
        angle end_angle;                          ///< For rotating elements the end angle
        double end_value{360.0};                  ///< For rotating elements the value at end angle
        angle rotation_offset;                    ///< The offset to use for display
        mutable unsigned phase{0};                ///< The subimage (phase) to use, mutable
                                                  ///< because called by const display()
        bool optional{false};                     ///< Is the element not always visible?
        mutable bool visible{true};               ///< Should the element get drawn?
        bool phase_by_value{false};               ///< Set phase by value?
        int click_radius{0};                      ///< Click area around center if > 0, as +/- value for x/y
        bool can_slide{false};                    ///< Can element slide?
        int slide_x;                              ///< Position to change x to when value is full
        std::vector<std::string> filenames_day;   // obsolete with new gpu interface
        std::vector<std::string> filenames_night; // obsolete with new gpu interface
        // std::vector<image> data_day;			///< Image data (always set)
        // std::vector<image> data_night;		///< Image data for night
        // (optional) std::vector<gpu::texture> tex;		///< Texture data
        // (only used when display is active!)
        std::vector<std::unique_ptr<texture>> tex; ///< Texture data (only used when display is active!) //
                                                   ///< obsolete with new gpu interface

        double get_angle_range() const; ///< Compute range of angles between start and end
    };

    // needed for correct destruction of heirs.
    ~user_display() override = default;
    // very basic. Just draw display
    virtual void display() const = 0;
    // mask contains one bit per popup (at most 31 popups)
    virtual unsigned get_popup_allow_mask() const { return 0; }

    /// overload with code that inits data for that display, like loading images
    virtual void enter(bool is_day);
    /// overload with code that deinits data for that display, like freeing
    /// images
    virtual void leave();

  protected:
    // common functions: draw_infopanel(class game& gm)
    user_display(class user_interface& ui_, const char* display_name = nullptr /* when no elements are used */);

    // the display needs to know its parent (user_interface) to access common
    // data
    class user_interface& ui;
    std::vector<elem2D> elements;                         ///< Elements to use for drawing
    std::unordered_map<unsigned, unsigned> id_to_element; ///< Mapping IDs from definition to elements
    const elem2D& element_for_id(unsigned id) const;      ///< Deliver element for a given ID
    elem2D& element_for_id(unsigned id);                  ///< Deliver element for a given ID
    void draw_elements(bool with_info_panel = true) const;

  private:
    // no empty construction, no copy
    user_display()                               = delete;
    user_display(user_display&)                  = delete;
    user_display& operator=(const user_display&) = delete;
};
