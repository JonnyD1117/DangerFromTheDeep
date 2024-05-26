/*
Danger from the Deep - Open source submarine simulation
Copyright (C) 2007  Matthew Lawrence, Thorsten Jordan,
Luis Barrancos and others.

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

// OpenGL capabilities tester program

// TODO(tj): 	WGL support
// 		Test under OSX
//

#include "tests.hpp"

#include "OglExt.h"

#include <cstdlib>
#include <iostream>
#include <set>
#include <sstream>

tests::~tests() = default;

using namespace std;

auto tests::main() -> int
{
    if (loadlibs() != 0) {
        cerr << BAD << "Failed to load libraries" << endl;
        return 1;
    }

    if (load_ctx() != 0) {
        cerr << BAD << "Failed to init GL connection" << endl;
        return 1;
    }

    if (0 == do_gl_tests()) {
        cout << endl
             << BAD
             << "Not all tests returned successful. Dangerdeep might not run "
                "well or at all on your hardware! Problems include:"
             << endl;
        {
            for (const auto& it : error_log) {
                cout << it.c_str() << endl;
            }
        }
    } else {
        cout << endl
             << GOOD
             << "No problems were found. You should have no problems running "
                "Dangerdeep."
             << endl;
    }

    unload_ctx();

    unloadlibs();

    return 0;
}

void tests::load_gl_info()
{
    const char* c_vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char* c_render = reinterpret_cast<const char*>(glGetString(GL_RENDERER));

#if defined(__APPLE__) || defined(__MACOSX__) || defined(MINGW32)
    const char* c_glsl = "Not available";
#else
    const char* c_glsl = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
#endif

    c_version    = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    c_extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));

    string vendor = c_vendor != nullptr ? c_vendor : "Unknown";
    string render = c_render != nullptr ? c_render : "Unknown";
    string glsl   = c_glsl != nullptr ? c_glsl : "Unknown";
    version       = c_version != nullptr ? c_version : "Unknown";
    extensions    = c_extensions != nullptr ? c_extensions : "Unknown";

    cout << START_ITEM << "Vendor: " << STOP_ITEM << vendor << endl;
    cout << START_ITEM << "Render: " << STOP_ITEM << render << endl;
    cout << START_ITEM << "Version: " << STOP_ITEM << version << endl;
    cout << START_ITEM << "GLSL: " << STOP_ITEM << glsl << endl;

    // some modifled dftd code to parse the extensions
    if (c_extensions != nullptr) {
        unsigned spos = 0;
        while (spos < extensions.length()) {
            string::size_type pos = extensions.find(' ', spos);
            if (pos == string::npos) {
                supported_extensions.insert(extensions.substr(spos));
                spos = extensions.length();
            } else {
                supported_extensions.insert(extensions.substr(spos, pos - spos));

                spos = pos + 1;
            }
        }
    }
}

auto tests::pt_out(const std::string& message, enum status status) -> int
{
    switch (status) {
        case sGOOD:
            cout << GOOD << message << endl;
            return 1;
            break;
        case sMED:
            cout << MED << message << endl;
            warn_log.insert(message);
            break;
        case sBAD:
            cout << BAD << message << endl;
            error_log.insert(message);
            break;
    }

    return 0;
}

auto tests::do_version_check() -> int
{
    if (c_version != nullptr) {
        unsigned spos  = 0;
        unsigned count = 0;
        unsigned last  = 0;

        enum status status;

        int major = 0;
        int minor = 0;

        while (spos < version.length()) {
            if ('.' == version.c_str()[spos]) {
                string temp = version.substr(last, spos - last);
                last        = spos + 1;

                if (0 == count) {
                    major = atoi(temp.c_str());
                } else if (1 == count) {
                    minor = atoi(temp.c_str());
                    break;
                }
                count++;
            }
            spos++;
        }

        if (major == 2) {
            status = sGOOD;

            if (0 == minor) {
                status = sMED;
            }
        } else if (major >= 3) {
            status = sGOOD;
        } else {
            status = sBAD;
        }
        MPT_OUT("OpenGL Version: " << major << "." << minor << ".x ", status);
    } else {
        return pt_out("No version", sBAD);
    }
}

auto tests::do_texunit_check() -> int
{
    int texture_units       = 0;
    int texture_image_units = 0;
    enum status status;

#ifndef MINGW32
    glGetIntegerv(GL_MAX_TEXTURE_UNITS, &texture_units);
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &texture_image_units);
#endif

    if (texture_units > 8) {
        status = sGOOD;
    } else if (texture_units == 8) {
        status = sMED;
    } else {
        status = sBAD;
    }

    if (texture_image_units > 15) {
        status = sGOOD;
    } else if (texture_image_units > 7) {
        status = sMED;
    } else {
        status = sBAD;
    }

    MPT_OUT(
        "Found " << texture_units << " Texture Units and " << texture_image_units << " Image Texture Units ", status);
}

auto tests::do_vbo_check() -> int
{
    enum status status;
    if (extension_supported("GL_ARB_vertex_buffer_object")) {
        status = sGOOD;
    } else {
        status = sBAD;
    }

    return pt_out("Support for vertex buffer objects", status);
}

auto tests::do_fb_check() -> int
{
    enum status status;
    if (extension_supported("GL_EXT_framebuffer_object")) {
        status = sGOOD;
    } else {
        status = sBAD;
    }

    return pt_out("Support for framebuffer objects", status);
}
auto tests::do_power2_check() -> int
{
    enum status status;
    if (extension_supported("GL_ARB_texture_non_power_of_two")) {
        status = sGOOD;
    } else {
        status = sMED;
    }

    return pt_out("Support for non power of two textures", status);
}

auto tests::do_fshader_check() -> int
{
    enum status status;
    if (extension_supported("GL_ARB_fragment_shader")) {
        status = sGOOD;
    } else {
        status = sBAD;
    }

    return pt_out("Support for fragment shaders", status);
}

auto tests::do_vshader_check() -> int
{
    enum status status;
    if (extension_supported("GL_ARB_vertex_shader")) {
        status = sGOOD;
    } else {
        status = sBAD;
    }

    return pt_out("Support for vertex shaders", status);
}

auto tests::do_shaderobj_check() -> int
{
    enum status status;
    if (extension_supported("GL_ARB_shader_objects")) {
        status = sGOOD;
    } else {
        status = sBAD;
    }

    return pt_out("Support for shader objects", status);
}

auto tests::do_compression_check() -> int
{
    enum status status;
    if (extension_supported("GL_EXT_texture_compression_s3tc")
        || extension_supported("GL_ARB_texture_compression_s3tc")) {
        status = sGOOD;
    } else {
        status = sMED;
    }

    return pt_out("Support for texture compression", status);
}

auto tests::do_halffloat_check() -> int
{
    enum status status;
    if (extension_supported("ARB_half_float_pixel") || extension_supported("GL_NV_half_float")) {
        status = sGOOD;
    } else {
        status = sMED;
    }

    return pt_out("Support for 16bit floats", status);
}

auto tests::do_gl_tests() -> int
{
    int retval = 1;

    load_gl_info();

    if (0 == do_version_check()) {
        retval = 0;
    }

    if (0 == do_texunit_check()) {
        retval = 0;
    }

    if (0 == do_vbo_check()) {
        retval = 0;
    }

    if (0 == do_fb_check()) {
        retval = 0;
    }

    if (0 == do_power2_check()) {
        retval = 0;
    }

    if (0 == do_fshader_check()) {
        retval = 0;
    }

    if (0 == do_vshader_check()) {
        retval = 0;
    }

    if (0 == do_shaderobj_check()) {
        retval = 0;
    }

    if (0 == do_compression_check()) {
        retval = 0;
    }

    if (0 == do_halffloat_check()) {
        retval = 0;
    }

    return retval;
}

// more stolen code
auto tests::extension_supported(const string& s) -> bool
{
    auto it = supported_extensions.find(s);
    return (it != supported_extensions.end());
}

#if 0

int tests::loadlibs()
{
	char *error;
	opengl = dlopen( "libGL.so", RTLD_LAZY );

	if ( NULL == opengl )
	{
		cerr << "Failed to load: libGL.so" << endl;
		return 1;
	}

	dlerror();

	*(void **) (&DFTD_glGetString) = dlsym( opengl, "glGetString" );
	*(void **) (&DFTD_glGetIntegerv) = dlsym( opengl, "glGetIntegerv" );

	if ( ( error = dlerror() ) != NULL )
	{
		cerr << "Failed to load OpenGL symbols" << endl;
		return 1;
	}
	return 0;
}

int tests::unloadlibs()
{
	dlclose( opengl );
	return 1;
}

#else

auto tests::loadlibs() -> int
{
    return 0;
}
auto tests::unloadlibs() -> int
{
    return 0;
}

#endif
