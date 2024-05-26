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
// Parts taken and adapted from Martin Butterwecks's OOML SDL code
// (wws.sourceforget.net/projects/ooml/)

#include "system_interface.hpp"

#include <SDL.h>
//#include "gpu_interface.hpp"
#include "error.hpp"
#include "helper.hpp"
#include "log.hpp"
#include "shader.hpp" // for old OpenGL shader init

#include <GL/glu.h>
#include <chrono>
#include <fstream>
#include <sstream>
#include <utility>

static auto start_time = std::chrono::high_resolution_clock::now();

system_interface::system_interface(parameters params_)
    : params(std::move(params_))
{
    // Initialize SDL first
    int err = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_TIMER);
    if (err < 0) {
        THROW(error, "video init failed");
    }

    // request available screen modes (using display #0)
    const int display_nr = 0;
    int num_modes        = SDL_GetNumDisplayModes(display_nr);
    if (num_modes > 0) {
        available_resolutions.reserve(num_modes);
        for (int i = 0; i < num_modes; ++i) {
            SDL_DisplayMode mode;
            if (SDL_GetDisplayMode(display_nr, i, &mode) == 0) {
                // we only need w/h
                available_resolutions.emplace_back(mode.w, mode.h);
                log_info("Available resolution " << mode.w << "x" << mode.h << "\n");
            }
        }
        std::sort(available_resolutions.begin(), available_resolutions.end());
        available_resolutions.erase(
            std::unique(available_resolutions.begin(), available_resolutions.end()), available_resolutions.end());
    }

    // load default GL library
    SDL_GL_LoadLibrary(nullptr);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
#if 0
	// request GL 4.5 context.
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
#else
    // request GL 2.1 context.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    // Define depth buffer size, but we don't need it.
    // SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
#ifdef DEBUG
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif

    // if we request a fullscreen mode that is not available, fall back to
    // windowed mode
    if (params.fullscreen) {
        bool valid_mode = false;
        for (auto& r : available_resolutions) {
            if (r == params.resolution) {
                valid_mode = true;
                break;
            }
        }
        params.fullscreen = valid_mode;
    }

    SDL_Window* main_window = SDL_CreateWindow(
        params.window_caption.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        params.resolution.x,
        params.resolution.y,
        (params.fullscreen ? SDL_WINDOW_FULLSCREEN : 0) | SDL_WINDOW_OPENGL);
    if (main_window == nullptr) {
        THROW(error, "SDL Window creation failed");
    }
    sdl_main_window          = main_window;
    SDL_GLContext gl_context = SDL_GL_CreateContext(main_window);
    if (gl_context == nullptr) {
        THROW(error, "SDL GL Context creation failed");
    }
    sdl_gl_context = gl_context;
    // Enable V-Sync
    SDL_GL_SetSwapInterval(params.vertical_sync ? 1 : 0);

    // Call SDL_Quit any way at exit
    std::atexit(SDL_Quit);

    // Some SDL init.
    SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
    SDL_JoystickEventState(SDL_IGNORE);
    SDL_ShowCursor(SDL_ENABLE);

    // This creates the GPU interface
    prepare_new_resolution();
}

system_interface::~system_interface()
{
    // deinit gpu_interface first
    //	gpu::interface::destroy_instance();
    SDL_GL_DeleteContext(static_cast<SDL_GLContext>(sdl_gl_context));
    SDL_DestroyWindow(static_cast<SDL_Window*>(sdl_main_window));
    SDL_Quit();
}

auto system_interface::set_parameters(const parameters& params_) -> bool
{
    // if fullscreen mode is requested, check if it exists
    if (params_.fullscreen) {
        bool valid_mode = false;
        for (auto& r : available_resolutions) {
            if (r == params_.resolution) {
                valid_mode = true;
                break;
            }
        }
        if (!valid_mode) {
            return false;
        }
    }

    // If fullscreen state changes, toggle this
    if (params_.fullscreen != params.fullscreen) {
        SDL_SetWindowFullscreen(
            static_cast<SDL_Window*>(sdl_main_window), (params_.fullscreen ? SDL_WINDOW_FULLSCREEN : 0));
    }

    // If Vsync changes, toggle it
    if (params_.vertical_sync != params.vertical_sync) {
        SDL_GL_SetSwapInterval(params_.vertical_sync ? 1 : 0);
    }

    // Now change window resolution or screen mode
    if (params_.fullscreen) {
        // SDL_SetWindowDisplayMode((SDL_Window*)sdl_main_window, // fixme needs
        // an exact mode...
    } else {
        // fixme this maybe doesn't work for fullscreen modes...
        SDL_SetWindowSize(static_cast<SDL_Window*>(sdl_main_window), params_.resolution.x, params_.resolution.y);
    }

    // Parameters seem valid, so keep them
    params = params_;

    prepare_new_resolution();

    return true;
}

auto system_interface::millisec() const -> uint32_t
{
    // SDL_GetTicks seems to have very crude timings, same for new C++ chrono
    // stuff...
    auto current_time = std::chrono::high_resolution_clock::now();
    auto duration     = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
    //	std::cout << "duration " << duration << " tpws " <<
    // time_passed_while_sleeping << "\n";
    return uint32_t(duration) - time_passed_while_sleeping;
    //	return SDL_GetTicks() - time_passed_while_sleeping;
}

auto get_key_code(SDL_Keycode sym) -> key_code
{
    switch (sym) {
        case SDLK_BACKSPACE:
            return key_code::BACKSPACE;
        case SDLK_COMMA:
            return key_code::COMMA;
        case SDLK_DELETE:
            return key_code::DELETE;
        case SDLK_DOWN:
            return key_code::DOWN;
        case SDLK_END:
            return key_code::END;
        case SDLK_ESCAPE:
            return key_code::ESCAPE;
        case SDLK_HOME:
            return key_code::HOME;
        case SDLK_LCTRL:
            return key_code::LCTRL;
        case SDLK_LEFT:
            return key_code::LEFT;
        case SDLK_LESS:
            return key_code::LESS;
        case SDLK_LSHIFT:
            return key_code::LSHIFT;
        case SDLK_MINUS:
            return key_code::MINUS;
        case SDLK_PAGEDOWN:
            return key_code::PAGEDOWN;
        case SDLK_PAGEUP:
            return key_code::PAGEUP;
        case SDLK_PAUSE:
            return key_code::PAUSE;
        case SDLK_PERIOD:
            return key_code::PERIOD;
        case SDLK_PLUS:
            return key_code::PLUS;
        case SDLK_PRINTSCREEN:
            return key_code::PRINTSCREEN;
        case SDLK_RCTRL:
            return key_code::RCTRL;
        case SDLK_RETURN:
            return key_code::RETURN;
        case SDLK_RIGHT:
            return key_code::RIGHT;
        case SDLK_RSHIFT:
            return key_code::RSHIFT;
        case SDLK_SPACE:
            return key_code::SPACE;
        case SDLK_TAB:
            return key_code::TAB;
        case SDLK_UP:
            return key_code::UP;
        case SDLK_0:
            return key_code::_0;
        case SDLK_1:
            return key_code::_1;
        case SDLK_2:
            return key_code::_2;
        case SDLK_3:
            return key_code::_3;
        case SDLK_4:
            return key_code::_4;
        case SDLK_5:
            return key_code::_5;
        case SDLK_6:
            return key_code::_6;
        case SDLK_7:
            return key_code::_7;
        case SDLK_8:
            return key_code::_8;
        case SDLK_9:
            return key_code::_9;
        case SDLK_a:
            return key_code::a;
        case SDLK_b:
            return key_code::b;
        case SDLK_c:
            return key_code::c;
        case SDLK_d:
            return key_code::d;
        case SDLK_e:
            return key_code::e;
        case SDLK_f:
            return key_code::f;
        case SDLK_g:
            return key_code::g;
        case SDLK_h:
            return key_code::h;
        case SDLK_i:
            return key_code::i;
        case SDLK_j:
            return key_code::j;
        case SDLK_k:
            return key_code::k;
        case SDLK_l:
            return key_code::l;
        case SDLK_m:
            return key_code::m;
        case SDLK_n:
            return key_code::n;
        case SDLK_o:
            return key_code::o;
        case SDLK_p:
            return key_code::p;
        case SDLK_q:
            return key_code::q;
        case SDLK_r:
            return key_code::r;
        case SDLK_s:
            return key_code::s;
        case SDLK_t:
            return key_code::t;
        case SDLK_u:
            return key_code::u;
        case SDLK_v:
            return key_code::v;
        case SDLK_w:
            return key_code::w;
        case SDLK_x:
            return key_code::x;
        case SDLK_y:
            return key_code::y;
        case SDLK_z:
            return key_code::z;
        case SDLK_F1:
            return key_code::F1;
        case SDLK_F2:
            return key_code::F2;
        case SDLK_F3:
            return key_code::F3;
        case SDLK_F4:
            return key_code::F4;
        case SDLK_F5:
            return key_code::F5;
        case SDLK_F6:
            return key_code::F6;
        case SDLK_F7:
            return key_code::F7;
        case SDLK_F8:
            return key_code::F8;
        case SDLK_F9:
            return key_code::F9;
        case SDLK_F10:
            return key_code::F10;
        case SDLK_F11:
            return key_code::F11;
        case SDLK_F12:
            return key_code::F12;
        case SDLK_KP_1:
            return key_code::KP_1;
        case SDLK_KP_2:
            return key_code::KP_2;
        case SDLK_KP_3:
            return key_code::KP_3;
        case SDLK_KP_4:
            return key_code::KP_4;
        case SDLK_KP_5:
            return key_code::KP_5;
        case SDLK_KP_6:
            return key_code::KP_6;
        case SDLK_KP_7:
            return key_code::KP_7;
        case SDLK_KP_8:
            return key_code::KP_8;
        case SDLK_KP_9:
            return key_code::KP_9;
        case SDLK_KP_MINUS:
            return key_code::KP_MINUS;
        case SDLK_KP_PLUS:
            return key_code::KP_PLUS;
        default:
            return key_code::UNKNOWN;
    }
}

auto get_key_mod(uint16_t mod) -> key_mod
{
    auto km = key_mod::none;
    if ((mod & KMOD_LSHIFT) != 0) {
        km = km | key_mod::lshift;
    }
    if ((mod & KMOD_RSHIFT) != 0) {
        km = km | key_mod::rshift;
    }
    if ((mod & KMOD_CAPS) != 0) {
        km = km | key_mod::shift;
    }
    if ((mod & KMOD_LCTRL) != 0) {
        km = km | key_mod::lctrl;
    }
    if ((mod & KMOD_RCTRL) != 0) {
        km = km | key_mod::rctrl;
    }
    if ((mod & KMOD_LALT) != 0) {
        km = km | key_mod::lalt;
    }
    if ((mod & KMOD_RALT) != 0) {
        km = km | key_mod::ralt;
    }
    return km;
}

auto system_interface::get_key_name(key_code key, key_mod mod) -> std::string
{
    std::string result;
    if ((int(mod) & int(key_mod::shift)) != 0) {
        result = "Shift + ";
    }
    if ((int(mod) & int(key_mod::alt)) != 0) {
        result = "Alt + ";
    }
    if ((int(mod) & int(key_mod::ctrl)) != 0) {
        result = "Ctrl + ";
    }
    switch (key) {
        case key_code::BACKSPACE:
            return result + SDL_GetKeyName(SDLK_BACKSPACE);
        case key_code::COMMA:
            return result + SDL_GetKeyName(SDLK_COMMA);
        case key_code::DELETE:
            return result + SDL_GetKeyName(SDLK_DELETE);
        case key_code::DOWN:
            return result + SDL_GetKeyName(SDLK_DOWN);
        case key_code::END:
            return result + SDL_GetKeyName(SDLK_END);
        case key_code::ESCAPE:
            return result + SDL_GetKeyName(SDLK_ESCAPE);
        case key_code::HOME:
            return result + SDL_GetKeyName(SDLK_HOME);
        case key_code::LCTRL:
            return result + SDL_GetKeyName(SDLK_LCTRL);
        case key_code::LEFT:
            return result + SDL_GetKeyName(SDLK_LEFT);
        case key_code::LESS:
            return result + SDL_GetKeyName(SDLK_LESS);
        case key_code::LSHIFT:
            return result + SDL_GetKeyName(SDLK_LSHIFT);
        case key_code::MINUS:
            return result + SDL_GetKeyName(SDLK_MINUS);
        case key_code::PAGEDOWN:
            return result + SDL_GetKeyName(SDLK_PAGEDOWN);
        case key_code::PAGEUP:
            return result + SDL_GetKeyName(SDLK_PAGEUP);
        case key_code::PAUSE:
            return result + SDL_GetKeyName(SDLK_PAUSE);
        case key_code::PERIOD:
            return result + SDL_GetKeyName(SDLK_PERIOD);
        case key_code::PLUS:
            return result + SDL_GetKeyName(SDLK_PLUS);
        case key_code::PRINTSCREEN:
            return result + SDL_GetKeyName(SDLK_PRINTSCREEN);
        case key_code::RCTRL:
            return result + SDL_GetKeyName(SDLK_RCTRL);
        case key_code::RETURN:
            return result + SDL_GetKeyName(SDLK_RETURN);
        case key_code::RIGHT:
            return result + SDL_GetKeyName(SDLK_RIGHT);
        case key_code::RSHIFT:
            return result + SDL_GetKeyName(SDLK_RSHIFT);
        case key_code::SPACE:
            return result + SDL_GetKeyName(SDLK_SPACE);
        case key_code::TAB:
            return result + SDL_GetKeyName(SDLK_TAB);
        case key_code::UP:
            return result + SDL_GetKeyName(SDLK_UP);
        case key_code::_0:
            return result + SDL_GetKeyName(SDLK_0);
        case key_code::_1:
            return result + SDL_GetKeyName(SDLK_1);
        case key_code::_2:
            return result + SDL_GetKeyName(SDLK_2);
        case key_code::_3:
            return result + SDL_GetKeyName(SDLK_3);
        case key_code::_4:
            return result + SDL_GetKeyName(SDLK_4);
        case key_code::_5:
            return result + SDL_GetKeyName(SDLK_5);
        case key_code::_6:
            return result + SDL_GetKeyName(SDLK_6);
        case key_code::_7:
            return result + SDL_GetKeyName(SDLK_7);
        case key_code::_8:
            return result + SDL_GetKeyName(SDLK_8);
        case key_code::_9:
            return result + SDL_GetKeyName(SDLK_9);
        case key_code::a:
            return result + SDL_GetKeyName(SDLK_a);
        case key_code::b:
            return result + SDL_GetKeyName(SDLK_b);
        case key_code::c:
            return result + SDL_GetKeyName(SDLK_c);
        case key_code::d:
            return result + SDL_GetKeyName(SDLK_d);
        case key_code::e:
            return result + SDL_GetKeyName(SDLK_e);
        case key_code::f:
            return result + SDL_GetKeyName(SDLK_f);
        case key_code::g:
            return result + SDL_GetKeyName(SDLK_g);
        case key_code::h:
            return result + SDL_GetKeyName(SDLK_h);
        case key_code::i:
            return result + SDL_GetKeyName(SDLK_i);
        case key_code::j:
            return result + SDL_GetKeyName(SDLK_j);
        case key_code::k:
            return result + SDL_GetKeyName(SDLK_k);
        case key_code::l:
            return result + SDL_GetKeyName(SDLK_l);
        case key_code::m:
            return result + SDL_GetKeyName(SDLK_m);
        case key_code::n:
            return result + SDL_GetKeyName(SDLK_n);
        case key_code::o:
            return result + SDL_GetKeyName(SDLK_o);
        case key_code::p:
            return result + SDL_GetKeyName(SDLK_p);
        case key_code::q:
            return result + SDL_GetKeyName(SDLK_q);
        case key_code::r:
            return result + SDL_GetKeyName(SDLK_r);
        case key_code::s:
            return result + SDL_GetKeyName(SDLK_s);
        case key_code::t:
            return result + SDL_GetKeyName(SDLK_t);
        case key_code::u:
            return result + SDL_GetKeyName(SDLK_u);
        case key_code::v:
            return result + SDL_GetKeyName(SDLK_v);
        case key_code::w:
            return result + SDL_GetKeyName(SDLK_w);
        case key_code::x:
            return result + SDL_GetKeyName(SDLK_x);
        case key_code::y:
            return result + SDL_GetKeyName(SDLK_y);
        case key_code::z:
            return result + SDL_GetKeyName(SDLK_z);
        case key_code::F1:
            return result + SDL_GetKeyName(SDLK_F1);
        case key_code::F2:
            return result + SDL_GetKeyName(SDLK_F2);
        case key_code::F3:
            return result + SDL_GetKeyName(SDLK_F3);
        case key_code::F4:
            return result + SDL_GetKeyName(SDLK_F4);
        case key_code::F5:
            return result + SDL_GetKeyName(SDLK_F5);
        case key_code::F6:
            return result + SDL_GetKeyName(SDLK_F6);
        case key_code::F7:
            return result + SDL_GetKeyName(SDLK_F7);
        case key_code::F8:
            return result + SDL_GetKeyName(SDLK_F8);
        case key_code::F9:
            return result + SDL_GetKeyName(SDLK_F9);
        case key_code::F10:
            return result + SDL_GetKeyName(SDLK_F10);
        case key_code::F11:
            return result + SDL_GetKeyName(SDLK_F11);
        case key_code::F12:
            return result + SDL_GetKeyName(SDLK_F12);
        case key_code::KP_1:
            return result + SDL_GetKeyName(SDLK_KP_1);
        case key_code::KP_2:
            return result + SDL_GetKeyName(SDLK_KP_2);
        case key_code::KP_3:
            return result + SDL_GetKeyName(SDLK_KP_3);
        case key_code::KP_4:
            return result + SDL_GetKeyName(SDLK_KP_4);
        case key_code::KP_5:
            return result + SDL_GetKeyName(SDLK_KP_5);
        case key_code::KP_6:
            return result + SDL_GetKeyName(SDLK_KP_6);
        case key_code::KP_7:
            return result + SDL_GetKeyName(SDLK_KP_7);
        case key_code::KP_8:
            return result + SDL_GetKeyName(SDLK_KP_8);
        case key_code::KP_9:
            return result + SDL_GetKeyName(SDLK_KP_9);
        case key_code::KP_MINUS:
            return result + SDL_GetKeyName(SDLK_KP_MINUS);
        case key_code::KP_PLUS:
            return result + SDL_GetKeyName(SDLK_KP_PLUS);
        default:
            return result + "UNKNOWN";
    }
}

void system_interface::prepare_2d_drawing()
{
    if (draw_2d) {
        THROW(error, "2d drawing already turned on");
    }
    glFlush();
    glViewport(offset_2D.x, offset_2D.y, size_2D.x, size_2D.y);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, params.resolution2d.x, 0, params.resolution2d.y);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glTranslatef(0, params.resolution2d.y, 0);
    glScalef(1, -1, 1);
    glDisable(GL_DEPTH_TEST);
    glCullFace(GL_FRONT);
    draw_2d = true;
    glPixelZoom(float(size_2D.x) / params.resolution2d.x,
                -float(size_2D.y) / params.resolution2d.y); // flip images
}

void system_interface::unprepare_2d_drawing()
{
    if (!draw_2d) {
        THROW(error, "2d drawing already turned off");
    }
    glFlush();
    glPixelZoom(1.0F, 1.0F);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glEnable(GL_DEPTH_TEST);
    glCullFace(GL_BACK);
    draw_2d = false;
}

auto system_interface::finish_frame() -> bool
{
    if (quit_program) {
        return true;
    }

    // Switch window frame buffers
    SDL_GL_SwapWindow(static_cast<SDL_Window*>(sdl_main_window));

    // translate 2D motion/position to screen size -1...1, y axis up
    auto translate_p = [&](int x, int y) -> vector2f {
        return {2.F * x / params.resolution.x - 1.F, 1.F - 2.F * y / params.resolution.y};
    };
    // translate 2D motion to screen size -1...1, y axis up
    auto translate_m = [&](int x, int y) -> vector2f {
        return {2.F * x / params.resolution.x, -2.F * y / params.resolution.y};
    };
    // translate 2d motion to pseudo 2d coordinates, y axis down
    // Note that scaling depends on 2d screen size
    auto translate_m_2d = [&](int x, int y) -> vector2i {
        return {
            static_cast<int>(params.resolution2d.x * x / size_2D.x),
            static_cast<int>(params.resolution2d.y * y / size_2D.y)};
    };
    // translate 2D position to pseudo 2d coordinates, y axis down
    // coordinates inside area defined by offset_2D/size_2D should be mapped
    // to resolution2d. Note that this can give coordinates outside the
    // 0,0...1023,767 area.
    auto translate_p_2d = [&](int x, int y) -> vector2i {
        // as first remove offset
        x -= offset_2D.x;
        y -= offset_2D.y;
        // then translate to pseudo 2d
        x = int(params.resolution2d.x * x / size_2D.x);
        y = int(params.resolution2d.y * y / size_2D.y);
        return {x, y};
    };

    // Clean up empty input event handlers
    helper::erase_remove_if(input_event_handlers, [](const std::weak_ptr<input_event_handler>& ptr) {
        auto sptr = ptr.lock();
        return sptr == nullptr;
    });

    // Handle all events
    SDL_Event event;
    do {
        unsigned nr_of_events = 0;
        while (SDL_PollEvent(&event) != 0) {
            switch (event.type) {
                case SDL_QUIT: // Quit event
                    log_info("---------- immediate exit ----------");
                    log::instance().write(std::cerr, log::level::SYSINFO);
                    {
                        std::ofstream f("log.txt");
                        log::instance().write(f, log::level::SYSINFO);
                    }
                    quit_program = true;
                    return true;

                case SDL_WINDOWEVENT: // Application activation or focus event
                    if (event.window.event == SDL_WINDOWEVENT_ENTER) {
                        if (is_sleeping) {
                            is_sleeping = false;
                            time_passed_while_sleeping += SDL_GetTicks() - sleep_time;
                        }
                    } else if (event.window.event == SDL_WINDOWEVENT_LEAVE) {
                        if (!is_sleeping) {
                            is_sleeping = true;
                            sleep_time  = SDL_GetTicks();
                        }
                    }
                    continue; // filter these events

                case SDL_KEYDOWN:
                case SDL_KEYUP: {
                    input_event_handler::key_data kd;
                    kd.keycode = get_key_code(event.key.keysym.sym);
                    kd.mod     = get_key_mod(event.key.keysym.mod);
                    if (kd.keycode != key_code::UNKNOWN) {
                        kd.action = (event.type == SDL_KEYUP) ? input_action::up : input_action::down;
                        fetch_event([&kd](auto& handler) { return handler.handle_key_event(kd); });
                    }
                } break;
                case SDL_TEXTINPUT: {
                    const char* text = event.text.text;
                    if (text != nullptr) { // not empty
                        std::string the_text(text);
                        fetch_event([&the_text](auto& handler) { return handler.handle_text_input_event(the_text); });
                    }
                } break;
                case SDL_MOUSEMOTION: {
                    input_event_handler::mouse_motion_data md;
                    // Translate motion/position to 2D coordinates
                    md.relative_motion    = translate_m(event.motion.xrel, event.motion.yrel);
                    md.relative_motion_2d = translate_m_2d(event.motion.xrel, event.motion.yrel);
                    md.position           = translate_p(event.motion.x, event.motion.y);
                    md.position_2d        = translate_p_2d(event.motion.x, event.motion.y);
                    mouse_position        = md.position;
                    mouse_position_2d     = md.position_2d;
                    md.buttons_pressed    = mouse_button_state{};
                    if ((event.motion.state & SDL_BUTTON_LMASK) != 0U) {
                        md.buttons_pressed.pressed[unsigned(mouse_button::left)] = true;
                    }
                    if ((event.motion.state & SDL_BUTTON_MMASK) != 0U) {
                        md.buttons_pressed.pressed[unsigned(mouse_button::middle)] = true;
                    }
                    if ((event.motion.state & SDL_BUTTON_RMASK) != 0U) {
                        md.buttons_pressed.pressed[unsigned(mouse_button::right)] = true;
                    }
                    fetch_event([&md](auto& handler) { return handler.handle_mouse_motion_event(md); });
                } break;
                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP: {
                    input_event_handler::mouse_click_data md;
                    md.position       = translate_p(event.button.x, event.button.y);
                    md.position_2d    = translate_p_2d(event.button.x, event.button.y);
                    mouse_position    = md.position;
                    mouse_position_2d = md.position_2d;
                    // we get all mouse events here so we can track the button
                    // state and offer that info also in button down/up
                    // events... we would miss events outside the window then...
                    // but who cares, however this is not needed so far
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        md.button = mouse_button::left;
                    }
                    if (event.button.button == SDL_BUTTON_MIDDLE) {
                        md.button = mouse_button::middle;
                    }
                    if (event.button.button == SDL_BUTTON_RIGHT) {
                        md.button = mouse_button::right;
                    }
                    md.action = (event.type == SDL_MOUSEBUTTONUP) ? input_action::up : input_action::down;
                    fetch_event([&md](auto& handler) { return handler.handle_mouse_button_event(md); });
                } break;
                case SDL_MOUSEWHEEL: {
                    input_event_handler::mouse_wheel_data md;
                    md.relative_motion    = translate_m(event.wheel.x, event.wheel.y);
                    md.relative_motion_2d = translate_m_2d(event.wheel.x, event.wheel.y);
                    md.position           = mouse_position;
                    md.position_2d        = mouse_position_2d;
                    if (md.relative_motion.y < 0.F) {
                        md.action = input_action::up;
                    } else if (md.relative_motion.y > 0.F) {
                        md.action = input_action::down;
                    }
                    fetch_event([&md](auto& handler) { return handler.handle_mouse_wheel_event(md); });
                } break;
                default: // we don't handle unknown events
                    continue;
            }

            ++nr_of_events;
        }
        // do not waste CPU time when sleeping
        if (nr_of_events == 0 && is_sleeping) {
            SDL_Delay(25);
        }
    } while (is_sleeping);
    return false;
}

void system_interface::add_input_event_handler(const std::shared_ptr<input_event_handler>& ptr)
{
    input_event_handlers.push_back(ptr);
}

void system_interface::remove_input_event_handler(std::shared_ptr<input_event_handler> ptr_to_remove)
{
    helper::erase_remove_if(input_event_handlers, [&ptr_to_remove](const std::weak_ptr<input_event_handler>& ptr) {
        auto sptr = ptr.lock();
        if (sptr != nullptr) {
            return sptr == ptr_to_remove;
        }
        return false;
    });
}

void system_interface::screenshot(const std::string& filename)
{
    // We need to use SDL to get window buffer data, OpenGL ReadPixels is
    // obsolete with GL3+
    SDL_Surface* main_window_surface = SDL_GetWindowSurface(static_cast<SDL_Window*>(sdl_main_window));
    if (main_window_surface != nullptr) {
        std::string fn;
        if (filename.empty()) {
            std::ostringstream os;
            os << screenshot_dir << "screenshot" << screenshot_nr++ << ".bmp";
            fn = os.str();
        } else {
            fn = filename + ".bmp";
        }
        SDL_SaveBMP(main_window_surface, fn.c_str());
        log_info("screenshot taken as " << fn);
    }
}

void system_interface::gl_perspective_fovx(double fovx, double aspect, double znear, double zfar)
{
    double tanfovx2 = tan(M_PI * fovx / 360.0);
    double tanfovy2 = tanfovx2 / aspect;
    double r        = znear * tanfovx2;
    double t        = znear * tanfovy2;
    glFrustum(-r, r, -t, t, znear, zfar);
}

void system_interface::prepare_new_resolution()
{
    // compute 2D data and area and resolution. it must be 4:3 always.
    if (params.resolution.x * 3 >= params.resolution.y * 4) {
        // screen is wider than high
        size_2D.x          = params.resolution.y * 4 / 3;
        size_2D.y          = params.resolution.y;
        offset_2D.x        = (params.resolution.x - size_2D.x) / 2;
        offset_2D.y        = 0;
        offset_pseudo_2D.x = 2.F * offset_2D.x / params.resolution.x - 1.F;
        offset_pseudo_2D.y = 1.F;
        scale_pseudo_2D.x  = (2.F * size_2D.x) / (params.resolution.x * params.resolution2d.x);
        scale_pseudo_2D.y  = -2.F / params.resolution2d.y;
    } else {
        // screen is higher than wide
        size_2D.x          = params.resolution.x;
        size_2D.y          = params.resolution.x * 3 / 4;
        offset_2D.x        = 0;
        offset_2D.y        = (params.resolution.y - size_2D.y) / 2;
        offset_pseudo_2D.x = -1.F;
        offset_pseudo_2D.y = 1.F - 2.F * offset_2D.y / params.resolution.y;
        scale_pseudo_2D.x  = 2.F / params.resolution2d.x;
        scale_pseudo_2D.y  = -(2.F * size_2D.y) / (params.resolution.y * params.resolution2d.y);
    }

    // reinit frame buffer
    // fixme call glViewPort etc, compare old system.cpp
    //	GPU().init_frame_buffer(params.resolution.x, params.resolution.y);

    // OpenGL Init.
    glClearColor(32 / 255.0, 64 / 255.0, 192 / 255.0, 1.0);
    glClearDepth(1.0);
    glDepthFunc(GL_LEQUAL);
    glShadeModel(GL_SMOOTH);
    glDisable(GL_LIGHTING); // we use shaders for everything
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_NORMALIZE);
    glEnable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE); // should be obsolete
    // set up some things for drawing pixels
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glDrawBuffer(GL_BACK);

    // screen resize
    glViewport(0, 0, params.resolution.x, params.resolution.y);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gl_perspective_fovx(
        90.0,
        static_cast<GLdouble>(params.resolution.x) / static_cast<GLdouble>(params.resolution.y),
        params.near_z,
        params.far_z);
    float m[16];
    glGetFloatv(GL_PROJECTION_MATRIX, m);
    // xscal_2d = 2*params.near_z/m[0];
    // yscal_2d = 2*params.near_z/m[5];
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // enable texturing on all units
    GLint nrtexunits = 0;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &nrtexunits);
    for (unsigned i = 0; i < unsigned(nrtexunits); ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glEnable(GL_TEXTURE_2D);
    }

    /* fixme replace by modern stuff
    if (params.use_multisampling) {
        glEnable(GL_MULTISAMPLE);
        switch(params.hint_multisampling) {
            case 1:
                glHint(GL_MULTISAMPLE_FILTER_HINT_NV, GL_NICEST);
            break;
            case 2:
                glHint(GL_MULTISAMPLE_FILTER_HINT_NV, GL_FASTEST);
            break;
            default:
                glHint(GL_MULTISAMPLE_FILTER_HINT_NV, GL_DONT_CARE);
            break;
        }
    }
    switch (params.hint_fog) {
        case 1:
            glHint(GL_FOG_HINT, GL_NICEST);
        break;
        case 2:
            glHint(GL_FOG_HINT, GL_FASTEST);
        break;
        default:
            glHint(GL_FOG_HINT, GL_DONT_CARE);
        break;
    }
    switch (params.hint_mipmap) {
        case 1:
            glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);
        break;
        case 2:
            glHint(GL_GENERATE_MIPMAP_HINT, GL_FASTEST);
        break;
        default:
            glHint(GL_GENERATE_MIPMAP_HINT, GL_DONT_CARE);
        break;
    }
    switch (params.hint_texture_compression) {
        case 1:
            glHint(GL_TEXTURE_COMPRESSION_HINT, GL_NICEST);
        break;
        case 2:
            glHint(GL_TEXTURE_COMPRESSION_HINT, GL_FASTEST);
        break;
        default:
            glHint(GL_TEXTURE_COMPRESSION_HINT, GL_DONT_CARE);
        break;
    }
    */
    // since we use vertex arrays for every primitive, we can enable it
    // here and leave it enabled forever
    glEnableClientState(GL_VERTEX_ARRAY);

    glsl_shader_setup::default_init();
}
