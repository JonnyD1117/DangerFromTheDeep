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

// SDL2/OpenGL based system input/output
// (C)+(W) by Thorsten Jordan. See LICENSE

#pragma once

#include "input_event_handler.hpp"
#include "singleton.hpp"
#include "vector2.hpp"

#include <memory>
#include <string>
#include <vector>

///\brief This class groups system related functions like graphic output or user
/// input.
class system_interface : public singleton<system_interface>
{
  public:
    /// parameter structure for class system_interface
    struct parameters
    {
        vector2i resolution{1024, 768};     ///< X/Y-Resolution of screen/window
        vector2i resolution2d{1024, 768};   ///< X/Y-Resolution of 2d mode to use (virtual resolution)
        std::string window_caption{"todo"}; ///< Window caption (UTF-8)
        bool fullscreen{false};             ///< Fullscreen mode instead of window?
        bool vertical_sync{true};           ///< Use vertical sync?
        [[nodiscard]] double get_aspect_ratio() const { return double(resolution.x) / resolution.y; }
        double near_z{1.0};    // not needed with new gpu interface, remove later
        double far_z{30000.0}; // not needed with new gpu interface, remove later
        parameters() { }
    };

    /// Constructor
    system_interface(parameters params = parameters());

    /// Destructor
    ~system_interface();

    /// Get the parameters
    [[nodiscard]] const auto& get_parameters() const { return params; }

    /// Set new parameters, returns true on success
    bool set_parameters(const parameters& params);

    /// Add new event handler on top of stack
    void add_input_event_handler(const std::shared_ptr<input_event_handler>&);

    /// Remove the topmost handler
    void remove_input_event_handler(std::shared_ptr<input_event_handler>);

    /// Get the name of a key
    static std::string get_key_name(key_code key, key_mod mod);

    void prepare_2d_drawing(); // must be called as pair!
    void unprepare_2d_drawing();

    /// Finish drawing of frame in window, will trigger fetching of input
    /// events, returns true if program should quit!
    bool finish_frame();

    /// Return global time stamp in milliseconds (inactive process time not
    /// counted!)
    uint32_t millisec() const;

    /// Set a directory for screenshots
    void set_screenshot_directory(const std::string& s) { screenshot_dir = s; }

    /// Take a screenshot
    void screenshot(const std::string& filename = std::string());

    // give FOV X in degrees, aspect (w/h), znear and zfar.
    static void gl_perspective_fovx(double fovx, double aspect, double znear, double zfar);

    /// Request current resolution
    [[nodiscard]] auto get_resolution() const { return params.resolution; }

    /// Request 2D offset on screen
    [[nodiscard]] auto get_2D_screen_offset() const { return offset_2D; }

    /// Request 2D size on screen
    [[nodiscard]] auto get_2D_screen_size() const { return size_2D; }

    /// Translate pseudo 2D coordinates (1024x768 range) to real coordinates.
    /// Note that y-coord is negated!
    [[nodiscard]] vector2f translate_2D_coordinates(const vector2i& c) const
    {
        return vector2f(scale_pseudo_2D.x * c.x + offset_pseudo_2D.x, scale_pseudo_2D.y * c.y + offset_pseudo_2D.y);
    }

    /// Translate pseudo 2D size (1024x768 range) to real size. Note that
    /// y-coord is NOT negated!
    [[nodiscard]] vector2f translate_2D_size(const vector2i& c) const
    {
        return vector2f(scale_pseudo_2D.x * c.x, scale_pseudo_2D.y * c.y);
    }

    [[nodiscard]] auto get_res() const { return params.resolution; }
    [[nodiscard]] auto get_res_2d() const { return params.resolution2d; }
    // old code compatibility layer
    [[nodiscard]] auto get_res_x() const { return params.resolution.x; }
    [[nodiscard]] auto get_res_y() const { return params.resolution.y; }
    [[nodiscard]] auto get_res_x_2d() const { return params.resolution2d.x; }
    [[nodiscard]] auto get_res_y_2d() const { return params.resolution2d.y; }
    // fixme only needed by uzo and widget, get rid of that!
    [[nodiscard]] auto get_res_area_2d_x() const { return offset_2D.x; }
    [[nodiscard]] auto get_res_area_2d_y() const { return offset_2D.y; }
    [[nodiscard]] auto get_res_area_2d_w() const { return size_2D.x; }
    [[nodiscard]] auto get_res_area_2d_h() const { return size_2D.y; }

    /// Request available screen resolutions
    [[nodiscard]] const auto& get_available_resolutions() const { return available_resolutions; }

  private:
    void prepare_new_resolution();

    system_interface(const system_interface&)            = delete;
    system_interface& operator=(const system_interface&) = delete;
    system_interface(system_interface&&)                 = delete;
    system_interface& operator=(system_interface&&)      = delete;

    void* sdl_main_window{nullptr};              ///< SDL window pointer (made opaque)
    void* sdl_gl_context{nullptr};               ///< SDL GL context pointer (made opaque)
    parameters params;                           ///< The current parameters.
    vector2i offset_2D;                          ///< The offset in 2D for drawing
    vector2i size_2D;                            ///< The size in 2D for drawing
    vector2f scale_pseudo_2D;                    ///< Scaling for pseudo 2D coordinates
    vector2f offset_pseudo_2D;                   ///< Offset for pseudo 2D coordinates
    uint32_t time_passed_while_sleeping{0};      ///< Total time in milliseconds that passed while application slept
    uint32_t sleep_time{0};                      ///< Time point when application went to sleep
    bool is_sleeping{false};                     ///< Flag if application is sleeping
    unsigned screenshot_nr{0};                   ///< The number of taken screenshots
    bool draw_2d{false};                         ///< Flag for 2d drawing
    vector2f mouse_position;                     ///< Last known mouse position
    vector2i mouse_position_2d;                  ///< Last known mouse position (pseudo 2d
                                                 ///< coordinates)
    std::string screenshot_dir;                  ///< Directory for screenshots
    std::vector<vector2i> available_resolutions; ///< List of available screen resolutions
    std::vector<std::weak_ptr<input_event_handler>> input_event_handlers; ///< stack of input event handlers
    bool quit_program{false};                                             ///< Global flag if we should quit

    /// Fetch event to latest handler @return true if handled
    bool fetch_event(std::function<bool(input_event_handler&)> func)
    {
        for (auto it = input_event_handlers.rbegin(); it != input_event_handlers.rend(); ++it) {
            auto ptr = it->lock();
            if (ptr != nullptr) {
                // call the event handler function and return
                return func(*ptr);
            }
        }
        return false;
    }
};

// to make user's code even shorter
inline class system_interface& SYS()
{
    return system_interface::instance();
}
