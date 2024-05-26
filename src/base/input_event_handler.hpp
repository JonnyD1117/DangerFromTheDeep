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

// SDL/OpenGL based system services
// (C)+(W) by Thorsten Jordan. See LICENSE

#pragma once

#include "vector2.hpp"

#include <array>
#include <functional>

#ifdef WIN32
#ifdef DELETE
#undef DELETE
#endif
#endif

/// Code for every key
/// @remarks extend for more keys, keys not listed here won't be recognized at
/// all!
enum class key_code
{
    UNKNOWN = 0,
    BACKSPACE,
    COMMA,
    DELETE,
    DOWN,
    END,
    ESCAPE,
    HOME,
    LCTRL,
    LEFT,
    LESS,
    LSHIFT,
    MINUS,
    PAGEDOWN,
    PAGEUP,
    PAUSE,
    PERIOD,
    PLUS,
    PRINTSCREEN,
    RCTRL,
    RETURN,
    RIGHT,
    RSHIFT,
    SPACE,
    TAB,
    UP,
    _0,
    _1,
    _2,
    _3,
    _4,
    _5,
    _6,
    _7,
    _8,
    _9,
    a,
    b,
    c,
    d,
    e,
    f,
    g,
    h,
    i,
    j,
    k,
    l,
    m,
    n,
    o,
    p,
    q,
    r,
    s,
    t,
    u,
    v,
    w,
    x,
    y,
    z,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    KP_1,
    KP_2,
    KP_3,
    KP_4,
    KP_5,
    KP_6,
    KP_7,
    KP_8,
    KP_9,
    KP_MINUS,
    KP_PLUS,
    number
};

/// Key modifier type
enum class key_mod
{
    none   = 0x0000,
    shift  = 0x0001, // bit 0: generally on, bits 1-2 left/right
    lshift = 0x0003,
    rshift = 0x0005,
    ctrl   = 0x0010, // bit 4: generally on, bits 4-5 left/right
    lctrl  = 0x0030,
    rctrl  = 0x0050,
    alt    = 0x0100, // bit 8: generally on, bits 9-10 left/right
    lalt   = 0x0300,
    ralt   = 0x0500,
    basic  = 0x0111, // basic bits for ctrl, alt, shift
    number
};

inline key_mod operator|(const key_mod& a, const key_mod& b)
{
    return key_mod(uint32_t(a) | uint32_t(b));
}
inline key_mod operator&(const key_mod& a, const key_mod& b)
{
    return key_mod(uint32_t(a) & uint32_t(b));
}

/// mouse button type
enum class mouse_button
{
    left,
    right,
    middle,
    number
};

/// key/button/wheel type
enum class input_action
{
    none,
    up,   // released for mouse
    down, // pressed for mouse
    number
};

/// mouse button(s) state
struct mouse_button_state
{
    std::array<bool, unsigned(mouse_button::number)> pressed{}; ///< for each button if pressed
    [[nodiscard]] bool is_pressed(mouse_button mb) const { return pressed[unsigned(mb)]; }
    [[nodiscard]] bool left() const { return is_pressed(mouse_button::left); }
    [[nodiscard]] bool middle() const { return is_pressed(mouse_button::middle); }
    [[nodiscard]] bool right() const { return is_pressed(mouse_button::right); }

    [[nodiscard]] bool any() const
    {
        return std::any_of(pressed.begin(), pressed.end(), [](bool b) { return b; });
    }
};

/// input event handler interface
class input_event_handler
{
  protected:
    inline bool key_mod_shift(key_mod m) { return (m & key_mod::shift) != key_mod::none; }

    inline bool key_mod_ctrl(key_mod m) { return (m & key_mod::ctrl) != key_mod::none; }

    inline bool key_mod_alt(key_mod m) { return (m & key_mod::alt) != key_mod::none; }

  public:
    /// key event data
    struct key_data
    {
        key_code keycode{key_code::UNKNOWN};     ///< The pressed key
        key_mod mod{key_mod::none};              ///< State of key modifiers
        input_action action{input_action::none}; ///< whether key was pressed or released

        [[nodiscard]] bool up() const { return action == input_action::up; }
        [[nodiscard]] bool down() const { return action == input_action::down; }
        [[nodiscard]] bool is_keypad_number() const { return key_code::KP_1 <= keycode && keycode <= key_code::KP_9; }
    };

    /// Mouse motion event data
    struct mouse_motion_data
    {
        vector2f position;                  ///< Absolute mouse position in screen coordinates
                                            ///< -1...1, y axis up
        vector2f relative_motion;           ///< Relative mouse motion, matching screen
                                            ///< coordinates
        vector2i position_2d;               ///< Absolute mouse position in 2D pseudo
                                            ///< coordinates (1024x768)
        vector2i relative_motion_2d;        ///< Relative motion in 2D pseudo
                                            ///< coordinates (1024x768)
        mouse_button_state buttons_pressed; ///< for each button if pressed
        [[nodiscard]] bool is_pressed(mouse_button mb) const { return buttons_pressed.is_pressed(mb); }
        [[nodiscard]] bool left() const { return is_pressed(mouse_button::left); }
        [[nodiscard]] bool middle() const { return is_pressed(mouse_button::middle); }
        [[nodiscard]] bool right() const { return is_pressed(mouse_button::right); }
    };

    /// Mouse click event data
    struct mouse_click_data
    {
        vector2f position;                       ///< Absolute mouse position in screen coordinates
                                                 ///< -1...1, y axis up
        vector2i position_2d;                    ///< Absolute mouse position in 2D pseudo
                                                 ///< coordinates (1024x768)
        mouse_button button{mouse_button::left}; ///< which mouse button was pressed

        input_action action{input_action::none}; ///< whether button was pressed or released

        mouse_button_state buttons_pressed; ///< for each button if pressed

        [[nodiscard]] bool up() const { return action == input_action::up; }
        [[nodiscard]] bool down() const { return action == input_action::down; }
        [[nodiscard]] bool left() const { return button == mouse_button::left; }
        [[nodiscard]] bool middle() const { return button == mouse_button::middle; }
        [[nodiscard]] bool right() const { return button == mouse_button::right; }
    };

    /// Mouse wheel event data
    struct mouse_wheel_data
    {
        vector2f relative_motion;    ///< Relative mouse motion, matching screen
                                     ///< coordinates
                                     ///
        vector2i relative_motion_2d; ///< Relative motion in 2D pseudo
                                     ///< coordinates (1024x768)
                                     ///
        vector2f position;           ///< Absolute mouse position in screen coordinates
                                     ///< -1...1, y axis up
                                     ///
        vector2i position_2d;        ///< Absolute mouse position in 2D pseudo
                                     ///< coordinates (1024x768)

        input_action action{input_action::none}; ///< whether wheel turned up or down
        [[nodiscard]] bool up() const { return action == input_action::up; }
        [[nodiscard]] bool down() const { return action == input_action::down; }
    };

    /// Handle key event, returns if handled
    virtual bool handle_key_event(const key_data& /*kd*/) { return false; }

    /// Handle mouse button event, returns if handled
    virtual bool handle_mouse_button_event(const mouse_click_data& /*mb*/) { return false; }

    /// Handle mouse button event, returns if handled
    virtual bool handle_mouse_motion_event(const mouse_motion_data& /*mmd*/) { return false; }

    /// Handle mouse button event, returns if handled
    virtual bool handle_mouse_wheel_event(const mouse_wheel_data& /*mwd*/) { return false; }

    /// Handle mouse button event, returns if handled
    virtual bool handle_text_input_event(const std::string& /*text*/) { return false; }

    /// destructor
    virtual ~input_event_handler() = default;
};

/// input event handler instance that can be customized
class input_event_handler_custom : public input_event_handler
{
  public:
    void set_handler(std::function<bool(const key_data&)> handler) { handler_key = handler; }

    void set_handler(std::function<bool(const mouse_click_data&)> handler) { handler_mouse_click = handler; }

    void set_handler(std::function<bool(const mouse_motion_data&)> handler) { handler_mouse_motion = handler; }

    void set_handler(std::function<bool(const mouse_wheel_data&)> handler) { handler_mouse_wheel = handler; }

    void set_handler(std::function<bool(const std::string&)> handler) { handler_text_input = handler; }

  protected:
    std::function<bool(const key_data&)> handler_key;
    std::function<bool(const mouse_click_data&)> handler_mouse_click;
    std::function<bool(const mouse_motion_data&)> handler_mouse_motion;
    std::function<bool(const mouse_wheel_data&)> handler_mouse_wheel;
    std::function<bool(const std::string&)> handler_text_input;

    bool handle_key_event(const key_data& kd) override
    {
        if (handler_key != nullptr) {
            return handler_key(kd);
        }
        return false;
    }

    bool handle_mouse_button_event(const mouse_click_data& md) override
    {
        if (handler_mouse_click != nullptr) {
            return handler_mouse_click(md);
        }
        return false;
    }

    bool handle_mouse_motion_event(const mouse_motion_data& mmd) override
    {
        if (handler_mouse_motion != nullptr) {
            return handler_mouse_motion(mmd);
        }
        return false;
    }

    bool handle_mouse_wheel_event(const mouse_wheel_data& mwd) override
    {
        if (handler_mouse_wheel != nullptr) {
            return handler_mouse_wheel(mwd);
        }
        return false;
    }

    bool handle_text_input_event(const std::string& text) override
    {
        if (handler_text_input != nullptr) {
            return handler_text_input(text);
        }
        return false;
    }
};
