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

// OpenGL GPU interface
// (C)+(W) by Thorsten Jordan. See LICENSE

#include "gpu_interface.hpp"

#include "image.hpp"
#include "log.hpp"

#include <SDL_endian.h>
#include <fstream>
#include <iomanip>
#include <memory>
#ifdef _WIN32
#include <windows.h>
#ifdef interface // needed because of windows crap
#undef interface
#endif
#else
#define APIENTRY
#endif
#ifdef WIN32
#include <GL/glew.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/glcorearb.h>
#include <GL/glext.h> // for compressed texture definitions and anisotropy definitions
#endif
#include "helper.hpp"
using namespace gpu;

// Note! OpenGL 4.5 offers direct state access that makes many glBind* obsolete.
// However this is no real benefit for us, so we don't use it yet.

/*
Note!
Linux/Nvidia, use:

export __GL_WriteProgramObjectAssembly=1
export __GL_WriteProgramObjectSource=1

to get ASM source.
*/

static const GLenum targets[size_t(buffer::buffer_type::number)] =
    {GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER, GL_UNIFORM_BUFFER, GL_SHADER_STORAGE_BUFFER};

static const GLenum usage_types[size_t(buffer::usage_type::number)] = {
    GL_STREAM_DRAW,
    GL_STREAM_READ,
    GL_STREAM_COPY,
    GL_STATIC_DRAW,
    GL_STATIC_READ,
    GL_STATIC_COPY,
    GL_DYNAMIC_DRAW,
    GL_DYNAMIC_READ,
    GL_DYNAMIC_COPY};

static const GLenum access_types[size_t(buffer::access_type::number)] = {GL_READ_ONLY, GL_WRITE_ONLY, GL_READ_WRITE};

#ifdef DEBUG
static void APIENTRY opengl_error_callback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam)
{
    std::string source_desc;
    switch (source) {
        case GL_DEBUG_SOURCE_API:
            source_desc = "API";
            break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER:
            source_desc = "ShaderCompiler";
            break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
            source_desc = "WindowSystem";
            break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:
            source_desc = "ThirdParty";
            break;
        case GL_DEBUG_SOURCE_APPLICATION:
            source_desc = "Application";
            break;
        case GL_DEBUG_SOURCE_OTHER:
            source_desc = "Other";
            break;
        default:
            source_desc = "?unknown?";
    };
    std::string type_desc;
    switch (type) {
        case GL_DEBUG_TYPE_ERROR:
            type_desc = "Error";
            break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
            type_desc = "DeprecatedBehavior";
            break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            type_desc = "UndefinedBehavior";
            break;
        case GL_DEBUG_TYPE_PERFORMANCE:
            type_desc = "Performance";
            break;
        case GL_DEBUG_TYPE_PORTABILITY:
            type_desc = "Portability";
            break;
        case GL_DEBUG_TYPE_MARKER:
            type_desc = "Marker";
            break;
        case GL_DEBUG_TYPE_PUSH_GROUP:
            type_desc = "PushGroup";
            break;
        case GL_DEBUG_TYPE_POP_GROUP:
            type_desc = "PopGroup";
            break;
        case GL_DEBUG_TYPE_OTHER:
            type_desc = "Other";
            break;
        default:
            type_desc = "?unknown?";
    };
    std::string severity_desc;
    switch (severity) {
        case GL_DEBUG_SEVERITY_HIGH:
            severity_desc = "HIGH";
            break;
        case GL_DEBUG_SEVERITY_MEDIUM:
            severity_desc = "Medium";
            break;
        case GL_DEBUG_SEVERITY_LOW:
            severity_desc = "low";
            break;
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            severity_desc = "_note_";
            break;
        default:
            severity_desc = "?unknown?";
    };
    // Notifications are too verbose, so we skip them.
    if (severity != GL_DEBUG_SEVERITY_NOTIFICATION) {
        if (severity == GL_DEBUG_SEVERITY_LOW && source == GL_DEBUG_SOURCE_API && type == GL_DEBUG_TYPE_OTHER) {
            // gives: "Texture state usage warning: Texture 0 is base level
            // inconsistent. Check texture size." seems to be a driver bug and
            // can be ignored.
        } else {
            std::ostringstream oss;
            oss << "OpenGL Debug/Error: source=" << source_desc << " type=" << type_desc << " id=" << id
                << " severity=" << severity_desc << " \"" << message << "\"";
            log_debug(oss.str());
            // std::cout << oss.str() << "\n";	// show for testing
            if (severity != GL_DEBUG_SEVERITY_LOW && type != GL_DEBUG_TYPE_OTHER && type != GL_DEBUG_TYPE_PERFORMANCE) {
                THROW(error, oss.str());
            }
            if (severity == GL_DEBUG_SEVERITY_HIGH) {
                // we would need to quit the program...
            }
        }
    } else {
        // what to do with notifications?
        // ignore them, nothing important. Tons of them, about buffer bounding
        // etc.
    }
}
#endif

buffer::~buffer()
{
    if (gpu_id != 0) {
        if (mapped)
            unmap();
        glDeleteBuffers(1, &gpu_id);
    }
}

int to_data_size(data_type dt)
{
    switch (dt) {
        case data_type::u8:
        case data_type::i8:
        case data_type::ubyte:
            return 1;
        case data_type::u16:
        case data_type::i16:
        case data_type::f16:
            return 2;
        case data_type::u32:
        case data_type::i32:
        case data_type::f32:
            return 4;
        case data_type::u64:
        case data_type::i64:
        case data_type::f64:
            return 8;
        default:
            THROW(error, "invalid data type");
    }
}

unsigned to_gl_type(data_type dt)
{
    switch (dt) { // no 64bit integer types possible!
        case data_type::u8:
            return GL_UNSIGNED_BYTE;
        case data_type::u16:
            return GL_UNSIGNED_SHORT;
        case data_type::u32:
            return GL_UNSIGNED_INT;
        case data_type::i8:
            return GL_BYTE;
        case data_type::i16:
            return GL_SHORT;
        case data_type::i32:
            return GL_INT;
        case data_type::f32:
            return GL_FLOAT;
        case data_type::f64:
            return GL_DOUBLE;
        case data_type::f16:
            return GL_HALF_FLOAT;
        case data_type::ubyte:
            return GL_UNSIGNED_BYTE;
        default:
            THROW(error, "invalid data type");
    }
}

bool is_integer_format(data_type dt)
{
    switch (dt) {
        case data_type::u8:
        case data_type::u16:
        case data_type::u32:
        case data_type::u64:
        case data_type::i8:
        case data_type::i16:
        case data_type::i32:
        case data_type::i64:
            return true;
        case data_type::f32:
        case data_type::f64:
        case data_type::f16:
        case data_type::ubyte:
            return false;
        default:
            THROW(error, "invalid data type");
    }
}

buffer::buffer(buffer_type type_)
    : type(type_)
{
}

buffer::buffer(buffer&& source)
    : object(std::move(source))
    , mapped(source.mapped)
    , type(source.type)
    , buffer_size(source.buffer_size)
{
    source.mapped      = false;
    source.buffer_size = 0;
}

buffer& buffer::operator=(buffer&& source)
{
    if (&source != this) {
        object::operator=(std::move(source));
        mapped             = source.mapped;
        type               = source.type;
        buffer_size        = source.buffer_size;
        source.mapped      = false;
        source.buffer_size = 0;
    }
    return *this;
}

void buffer::bind() const
{
    // create buffer if not done yet
    if (gpu_id == 0) {
        glGenBuffers(1, &gpu_id);
    }
    glBindBuffer(targets[unsigned(type)], gpu_id);
}

void buffer::unbind() const
{
    glBindBuffer(targets[unsigned(type)], 0);
}

void* buffer::map(access_type access)
{
    if (mapped)
        THROW(error, "buffer object mapped twice");
    // create buffer if not done yet
    if (gpu_id == 0) {
        glGenBuffers(1, &gpu_id);
    }
    bind();
    void* addr = glMapBuffer(targets[unsigned(type)], access_types[unsigned(access)]);
    if (addr == nullptr)
        THROW(error, "buffer object mapping failed");
    mapped = true;
    return addr;
}

void buffer::unmap()
{
    if (!mapped)
        THROW(error, "buffer object not mapped before unmap()");
    mapped = false;
    if (glUnmapBuffer(targets[unsigned(type)]) != GL_TRUE) {
        log_warning("failed to unmap buffer object, data invalid");
    }
    unbind();
}

void buffer::init_buffer(unsigned byte_size, const void* data, usage_type usage)
{
    // create buffer if not done yet
    if (gpu_id == 0) {
        glGenBuffers(1, &gpu_id);
    }
    GPU().bind_new_VAO(0);
    bind();
    glBufferData(targets[unsigned(type)], byte_size, data, usage_types[unsigned(usage)]);
    unbind();
    buffer_size = byte_size;
}

void buffer::update_buffer(unsigned byte_size, const void* data, usage_type usage)
{
    // create buffer if not done yet
    if (gpu_id == 0) {
        glGenBuffers(1, &gpu_id);
    }
    GPU().bind_new_VAO(0);
    bind();
    if (byte_size > buffer_size) {
        glBufferData(targets[unsigned(type)], byte_size, data, usage_types[unsigned(usage)]);
        buffer_size = byte_size;
    } else {
        glBufferSubData(targets[unsigned(type)], 0, byte_size, data);
    }
    unbind();
}

void buffer::update_buffer_data(unsigned byte_size, const void* data)
{
    if (gpu_id == 0)
        THROW(error, "update_buffer_data with invalid buffer");
    if (byte_size > buffer_size)
        THROW(error, "update_buffer_data with too much data size");
    GPU().bind_new_VAO(0);
    bind();
    glBufferSubData(targets[unsigned(type)], 0, byte_size, data);
    unbind();
}

vertex_buffer::vertex_buffer()
    : buffer(buffer_type::vertex_attributes)
    , buffer_data_type(data_type::u8)
{
}

vertex_buffer::vertex_buffer(vertex_buffer&& vb)
    : buffer(std::move(vb))
    , nr_of_elements(vb.nr_of_elements)
    , data_count(vb.data_count)
    , buffer_data_type(vb.buffer_data_type)
{
    vb.nr_of_elements = 0;
    vb.data_count     = 0;
}

vertex_buffer& vertex_buffer::operator=(vertex_buffer&& vb)
{
    if (this != &vb) {
        buffer::operator=(std::move(vb));
        nr_of_elements    = vb.nr_of_elements;
        data_count        = vb.data_count;
        buffer_data_type  = vb.buffer_data_type;
        vb.nr_of_elements = 0;
        vb.data_count     = 0;
    }
    return *this;
}

index_buffer::index_buffer()
{
    type = buffer_type::indices;
}

uniform_buffer::uniform_buffer()
    : buffer(buffer_type::uniform)
{
}

uniform_buffer::uniform_buffer(uniform_buffer&& source)
    : buffer(std::move(source))
{
}

uniform_buffer& uniform_buffer::operator=(uniform_buffer&& source)
{
    buffer::operator=(std::move(source));
    return *this;
}

shader_storage_buffer::shader_storage_buffer()
{
    type = buffer_type::shader_storage;
}

shader_storage_buffer::shader_storage_buffer(shader_storage_buffer&& source)
    : uniform_buffer(std::move(source))
{
}

shader_storage_buffer& shader_storage_buffer::operator=(shader_storage_buffer&& source)
{
    uniform_buffer::operator=(std::move(source));
    return *this;
}

/// Internal data about texture usage
static uint64_t texture_mem_used    = 0;
static uint64_t texture_mem_alloced = 0;
static uint64_t texture_mem_freed   = 0;

/// give OpenGL internal format enum for this kind of pixel data
int make_internal_format(unsigned nr_of_channels, data_type dt, bool use_compression)
{
    if (use_compression) {
        switch (nr_of_channels) {
            case 1:
                return GL_COMPRESSED_RED;
            case 2:
                return GL_COMPRESSED_RG;
            case 3:
                return GL_COMPRESSED_RGB;
            case 4:
                return GL_COMPRESSED_RGBA;
            default:
                break;
        }
    } else {
        switch (nr_of_channels) {
            case 1:
                switch (dt) {
                    case data_type::u8:
                        return GL_R8UI;
                    case data_type::u16:
                        return GL_R16UI;
                    case data_type::u32:
                        return GL_R32UI;
                    case data_type::f32:
                        return GL_R32F;
                    case data_type::f16:
                        return GL_R16F;
                    case data_type::ubyte:
                        return GL_R8;
                    default:
                        break;
                }
                break;
            case 2:
                switch (dt) {
                    case data_type::u8:
                        return GL_RG8UI;
                    case data_type::u16:
                        return GL_RG16UI;
                    case data_type::u32:
                        return GL_RG32UI;
                    case data_type::f32:
                        return GL_RG32F;
                    case data_type::f16:
                        return GL_RG16F;
                    case data_type::ubyte:
                        return GL_RG8;
                    default:
                        break;
                }
                break;
            case 3:
                switch (dt) {
                    case data_type::u8:
                        return GL_RGB8UI;
                    case data_type::u16:
                        return GL_RGB16UI;
                    case data_type::u32:
                        return GL_RGB32UI;
                    case data_type::f32:
                        return GL_RGB32F;
                    case data_type::f16:
                        return GL_RGB16F;
                    case data_type::ubyte:
                        return GL_RGB8;
                    default:
                        break;
                }
                break;
            case 4:
                switch (dt) {
                    case data_type::u8:
                        return GL_RGBA8UI;
                    case data_type::u16:
                        return GL_RGBA16UI;
                    case data_type::u32:
                        return GL_RGBA32UI;
                    case data_type::f32:
                        return GL_RGBA32F;
                    case data_type::f16:
                        return GL_RGBA16F;
                    case data_type::ubyte:
                        return GL_RGBA8;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    }
    THROW(error, "invalid texture format combination");
}

/// give OpenGL data format enum for this kind of pixel data
GLenum make_user_layout_format(unsigned nr_of_channels, bool is_integer)
{
    if (is_integer) {
        switch (nr_of_channels) {
            case 1:
                return GL_RED_INTEGER;
            case 2:
                return GL_RG_INTEGER;
            case 3:
                return GL_RGB_INTEGER;
            case 4:
                return GL_RGBA_INTEGER;
            default:
                THROW(error, "invalid texture data format");
        }
    } else {
        switch (nr_of_channels) {
            case 1:
                return GL_RED;
            case 2:
                return GL_RG;
            case 3:
                return GL_RGB;
            case 4:
                return GL_RGBA;
            default:
                THROW(error, "invalid texture data format");
        }
    }
}

/// give OpenGL data format enum for this kind of user data
GLenum make_user_data_format(data_type dt)
{
    switch (dt) {
        case data_type::u8:
        case data_type::i8:
        case data_type::u16:
        case data_type::i16:
        case data_type::u32:
        case data_type::i32:
        case data_type::u64:
        case data_type::i64:
        case data_type::ubyte:
            return GL_UNSIGNED_BYTE;
        case data_type::f32:
        case data_type::f64:
        case data_type::f16:
            return GL_FLOAT;
        default:
            THROW(error, "invalid data type");
    }
}

/// Header of DDS compressed images, taken from DevIL library
struct dds_header
{
    int8_t Signature[4];

    uint32_t Size1;         // size of the structure (minus MagicNum)
    uint32_t Flags1;        // determines what fields are valid
    uint32_t Height;        // height of surface to be created
    uint32_t Width;         // width of input surface
    uint32_t LinearSize;    // Formless late-allocated optimized surface size
    uint32_t Depth;         // Depth if a volume texture
    uint32_t MipMapCount;   // number of mip-map levels requested
    uint32_t AlphaBitDepth; // depth of alpha buffer requested

    uint32_t NotUsed[10];

    uint32_t Size2;           // size of structure
    uint32_t Flags2;          // pixel format flags
    uint32_t FourCC;          // (FOURCC code)
    uint32_t RGBBitCount;     // how many bits per pixel
    uint32_t RBitMask;        // mask for red bit
    uint32_t GBitMask;        // mask for green bits
    uint32_t BBitMask;        // mask for blue bits
    uint32_t RGBAlphaBitMask; // mask for alpha channel

    uint32_t ddsCaps1, ddsCaps2, ddsCaps3,
        ddsCaps4; // direct draw surface capabilities
    uint32_t TextureStage;
};

gpu::texture::texture(
    const std::string& filename,
    data_type dt,
    bool use_mipmap,
    bool use_compression,
    float bump_height)
    : has_mipmap(use_mipmap)
{
    // if extension is dds, use special dds loader
    std::string::size_type st = filename.rfind(".");
    std::string extension     = filename.substr(st);
    if (extension == ".dds") {
        dds_header header;
        std::ifstream file(filename.c_str());
        if (!file.good())
            THROW(file_context_error, "couldn't find, or failed to load ", filename);
        file.read((char*) &header, sizeof(header));
        if (std::string((char*) header.Signature, 4) != "DDS ")
            THROW(file_context_error, "not a valid .dds file: ", filename);
        //
        // This .dds loader supports the loading of compressed formats DXT1,
        // DXT3 and DXT5.
        //
        int factor;
        GLenum format;
#ifdef MAKEFOURCC // exists already on windows
#undef MAKEFOURCC
#endif
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                                                                                 \
    ((int32_t) (int8_t) (ch0) | ((int32_t) (int8_t) (ch1) << 8) | ((int32_t) (int8_t) (ch2) << 16)                     \
     | ((int32_t) (int8_t) (ch3) << 24))
        switch (SDL_SwapLE32(header.FourCC)) {
            case MAKEFOURCC('D', 'X', 'T', '1'):
                // DXT1's compression ratio is 8:1
                format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
                factor = 2;
                break;

            case MAKEFOURCC('D', 'X', 'T', '3'):
                // DXT3's compression ratio is 4:1
                format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
                factor = 4;
                break;

            case MAKEFOURCC('D', 'X', 'T', '5'):
                // DXT5's compression ratio is 4:1
                format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
                factor = 4;
                break;

            default:
                THROW(file_context_error, "no supported compression type on file: ", filename);
        }
#undef MAKEFOURCC

        // How big will the buffer need to be to load all of the pixel data
        // including mip-maps?
        if (SDL_SwapLE32(header.LinearSize) == 0)
            THROW(file_context_error, "linear size in dds file is 0: ", filename);

        int bufferSize = SDL_SwapLE32(header.LinearSize) * ((header.MipMapCount > 1) ? factor : 1);

        std::vector<GLubyte> pixels(bufferSize);
        file.read((char*) &pixels[0], bufferSize);

        // Close the file
        file.close();

        width          = SDL_SwapLE32(header.Width);
        height         = SDL_SwapLE32(header.Height);
        int numMipMaps = SDL_SwapLE32(header.MipMapCount);

        int block_size = (format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) ? 8 : 16;
        glGenTextures(1, &gpu_id);
        glBindTexture(GL_TEXTURE_2D, gpu_id);

        used_memory = bufferSize;
        texture_mem_used += used_memory;
        texture_mem_alloced += used_memory;
        int m_size, m_offset = 0, m_width = width, m_height = height;

        // Load the mip-map levels
        for (int i = 0; i < numMipMaps; ++i) {
            if (m_width == 0)
                m_width = 1;
            if (m_height == 0)
                m_height = 1;

            m_size = ((m_width + 3) / 4) * ((m_height + 3) / 4) * block_size;

            glTexImage2D(GL_TEXTURE_2D, i, format, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glCompressedTexSubImage2D(GL_TEXTURE_2D, i, 0, 0, m_width, m_height, format, m_size, &pixels[m_offset]);

            m_offset += m_size;

            // Half the image size for the next mip-map level...
            m_width  = (m_width / 2);
            m_height = (m_height / 2);
        }
    } else {
        // normal data initialization
        image teximage(filename);
        width          = teximage.get_width();
        height         = teximage.get_height();
        nr_of_channels = teximage.get_nr_of_channels();
        // if bump height is > 0 we create normal map (RGB/RGBA) from the L/LA
        // bump map. mipmaps must be computed specially then. if the input map
        // is already RGB/RGBA we assume it's in reality grey scale and take
        // just the first channel for height values.
        bool make_normal_map = (bump_height > 0.f);
        if (make_normal_map) {
            // extract channel 0 has bump height data
            auto bumpimage = teximage.extract_channel(0);
            // build mipmap pyramid (additional levels)
            auto mipmapdata = bumpimage.make_mipmaps();
            // create normal map for every level
            std::vector<image> normalmaps(1 + mipmapdata.size());
            normalmaps[0] = bumpimage.make_normal_map(bump_height);
            for (unsigned i = 0; i < unsigned(mipmapdata.size()); ++i) {
                normalmaps[i + 1] = mipmapdata[i].make_normal_map(bump_height);
            }
            // if input has alpha, transport that to every normal map
            if (nr_of_channels == 2 || nr_of_channels == 4) {
                auto alphainfo = teximage.extract_channel(nr_of_channels - 1);
                for (unsigned i = 0; i < unsigned(normalmaps.size()); ++i) {
                    normalmaps[i] = normalmaps[i].append_channels(alphainfo);
                }
                // with alpha always 4 channels now
                nr_of_channels = 4;
            } else {
                // without alpha always 3 channels now
                nr_of_channels = 3;
            }
            // init basic and every mipmap level
            std::vector<const void*> data(normalmaps.size());
            for (unsigned i = 0; i < unsigned(normalmaps.size()); ++i) {
                data[i] = &(normalmaps[i][0]);
            }
            init(data, dt, use_compression, &filename);
        } else {
            // normal initialization
            init(&(teximage[0]), dt, use_compression, &filename);
        }
    }
}

gpu::texture::texture(
    const std::vector<uint8_t>& pixels,
    int w,
    int h,
    int nc,
    bool use_mipmap,
    bool use_compression,
    data_type dt)
    : width(w)
    , height(h)
    , nr_of_channels(nc)
    , has_mipmap(use_mipmap)
{
    init(&pixels[0], dt, use_compression);
}

gpu::texture::texture(int w, int h, int nc, data_type dt, bool use_mipmap)
    : gpu_format(make_internal_format(nc, dt, false /* no compression */))
    , width(w)
    , height(h)
    , nr_of_channels(nc)
    , has_mipmap(use_mipmap)
{
    auto& g = GPU();
    if (width > g.get_max_texture_size() || height > g.get_max_texture_size()) {
        THROW(error, "texture size too large, not supported by card");
    }
    // Create 2D texture and initialize with empty pixels
    glGenTextures(1, &gpu_id);
    glBindTexture(GL_TEXTURE_2D, gpu_id);
    // Even if we give a nullptr we have to give it in correct format (float or
    // unsigned) otherwise textures seem to be not fully initialized or wrong
    // sized and artifacts will occur. That is what make_user_data_format is
    // for.
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        gpu_format,
        width,
        height,
        0,
        make_user_layout_format(nr_of_channels, is_integer_format(dt)),
        make_user_data_format(dt),
        (void*) nullptr);
    used_memory = width * height * nr_of_channels * to_data_size(dt);
    texture_mem_used += used_memory;
    texture_mem_alloced += used_memory;
    // if mipmapping is selected, create mipmaps
    update_mipmap();
}

gpu::texture::texture(texture&& source)
    : object(std::move(source))
    , gpu_format(source.gpu_format)
    , width(source.width)
    , height(source.height)
    , nr_of_channels(source.nr_of_channels)
    , has_mipmap(source.has_mipmap)
    , used_memory(source.used_memory)
{
    source.gpu_format     = 0;
    source.width          = 0;
    source.height         = 0;
    source.nr_of_channels = 0;
    source.has_mipmap     = false;
    source.used_memory    = 0;
}

gpu::texture& gpu::texture::operator=(texture&& source)
{
    if (&source != this) {
        reset();
        object::operator=(std::move(source));
        gpu_format            = source.gpu_format;
        width                 = source.width;
        height                = source.height;
        nr_of_channels        = source.nr_of_channels;
        has_mipmap            = source.has_mipmap;
        used_memory           = source.used_memory;
        source.gpu_format     = 0;
        source.width          = 0;
        source.height         = 0;
        source.nr_of_channels = 0;
        source.has_mipmap     = false;
        source.used_memory    = 0;
    }
    return *this;
}

gpu::texture::~texture()
{
    reset();
}

void gpu::texture::reset()
{
    texture_mem_used -= used_memory;
    texture_mem_freed += used_memory;
    if (gpu_id != 0)
        glDeleteTextures(1, &gpu_id);
    gpu_id      = 0;
    used_memory = 0;
}

void gpu::texture::set_data_generic(const void* pixels, int count, data_type dt, int mipmap_level, bool update_mipmap_)
{
    const auto work_width  = width >> mipmap_level;
    const auto work_height = height >> mipmap_level;
    if (work_width * work_height * nr_of_channels != int(count))
        THROW(error, "invalid data size!");
    if (gpu_id == 0)
        THROW(error, "trying to set data on invalid texture");
    glBindTexture(GL_TEXTURE_2D, gpu_id);
    glTexImage2D(
        GL_TEXTURE_2D,
        mipmap_level,
        gpu_format,
        work_width,
        work_height,
        0,
        make_user_layout_format(nr_of_channels, false),
        make_user_data_format(dt),
        pixels);

    // if mipmapping is selected, create mipmaps
    if (mipmap_level == 0 && update_mipmap_) {
        update_mipmap();
    }
}

void texture::update_mipmap()
{
    if (has_mipmap && gpu_id != 0) {
        // If normals are stored as pixel data this won't give the correct
        // result. Be warned.
        glBindTexture(GL_TEXTURE_2D, gpu_id);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
}

void gpu::texture::swap(texture& other)
{
    std::swap(gpu_id, other.gpu_id);
    std::swap(gpu_format, other.gpu_format);
    std::swap(width, other.width);
    std::swap(height, other.height);
    std::swap(nr_of_channels, other.nr_of_channels);
    std::swap(has_mipmap, other.has_mipmap);
    std::swap(used_memory, other.used_memory);
}

void gpu::texture::init(const void* data, data_type dt, bool use_compression, const std::string* name)
{
    auto& g = GPU();
    if (width > g.get_max_texture_size() || height > g.get_max_texture_size()) {
        if (name) {
            THROW(file_context_error, "texture size too large, not supported by card", *name);
        } else {
            THROW(error, "texture size too large, not supported by card");
        }
    }

    gpu_format = make_internal_format(nr_of_channels, dt, use_compression);

    // Create 2D texture
    if (gpu_id == 0) {
        glGenTextures(1, &gpu_id);
    }
    glBindTexture(GL_TEXTURE_2D, gpu_id);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        gpu_format,
        width,
        height,
        0,
        make_user_layout_format(nr_of_channels, is_integer_format(dt)),
        make_user_data_format(dt),
        data);

    // if mipmapping is selected, create mipmaps
    update_mipmap();

    texture_mem_used -= used_memory;
    texture_mem_freed += used_memory;
    used_memory = width * height * nr_of_channels * to_data_size(dt);
    if (has_mipmap)
        used_memory = (4 * used_memory) / 3;
    texture_mem_used += used_memory;
    texture_mem_alloced += used_memory;
}

void gpu::texture::init(
    const std::vector<const void*> data,
    data_type dt,
    bool use_compression,
    const std::string* name)
{
    auto& g = GPU();
    if (width > g.get_max_texture_size() || height > g.get_max_texture_size()) {
        if (name) {
            THROW(file_context_error, "texture size too large, not supported by card", *name);
        } else {
            THROW(error, "texture size too large, not supported by card");
        }
    }

    gpu_format = make_internal_format(nr_of_channels, dt, use_compression);

    // Create 2D texture
    if (gpu_id == 0) {
        glGenTextures(1, &gpu_id);
    }
    glBindTexture(GL_TEXTURE_2D, gpu_id);

    // write data to mipmap levels
    auto ulf        = make_user_layout_format(nr_of_channels, is_integer_format(dt));
    auto udf        = make_user_data_format(dt);
    auto w          = width;
    auto h          = height;
    int texel_count = 0;
    for (int level = 0; level < int(data.size()); ++level) {
        glTexImage2D(GL_TEXTURE_2D, level, gpu_format, w, h, 0, ulf, udf, data[level]);
        texel_count += w * h;
        w /= 2;
        h /= 2;
    }

    texture_mem_used -= used_memory;
    texture_mem_freed += used_memory;
    used_memory = texel_count * nr_of_channels * to_data_size(dt);
    texture_mem_used += used_memory;
    texture_mem_alloced += used_memory;
}

void gpu::texture::sub_image(const area& ar, data_type dt, const void* pixels, int stride, bool update_mipmap_)
{
    auto line_width_in_bytes = nr_of_channels * to_data_size(dt);
    // stride is in pixels.
    if (stride != 0) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, stride);
        line_width_in_bytes *= stride;
    } else {
        line_width_in_bytes *= ar.size.x;
    }
    // Default data alignment is 4 for each pixel row. We can adjust this to
    // 1,2,4 or 8.
    auto alignment = 4;
    if (line_width_in_bytes & 2) {
        alignment = 2;
    }
    if (line_width_in_bytes & 1) {
        alignment = 1;
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
    glBindTexture(GL_TEXTURE_2D, gpu_id);
    glTexSubImage2D(
        GL_TEXTURE_2D,
        0 /* mipmap level */,
        ar.offset.x,
        ar.offset.y,
        ar.size.x,
        ar.size.y,
        make_user_layout_format(nr_of_channels, is_integer_format(dt)),
        make_user_data_format(dt),
        pixels);
    if (stride != 0) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }
    if (update_mipmap_) {
        update_mipmap();
    }
}

texture_array::texture_array(const std::vector<std::string>& filenames, data_type dt, bool use_mipmap)
    : nr_of_layers(int(filenames.size()))
    , has_mipmap(use_mipmap)
{
    // Load first image to know width/height
    if (filenames.empty()) {
        THROW(error, "trying to create zero layer texture array");
    }
    image teximage(filenames[0]);
    width          = teximage.get_width();
    height         = teximage.get_height();
    nr_of_channels = teximage.get_nr_of_channels();
    // First create empty texture array then initialize every layer with image
    // data
    create_space(dt, &filenames[0]);
    try {
        set_data(0, teximage.get_data());
        for (auto i = 1; i < nr_of_layers; ++i) {
            teximage = image(filenames[i]);
            if (width != teximage.get_width() || height != teximage.get_height()
                || nr_of_channels != teximage.get_nr_of_channels()) {
                THROW(file_context_error, "image dimensions do not match in texture array", filenames[i]);
            }
            set_data(i, teximage.get_data());
        }
    }
    catch (std::exception&) {
        reset();
        throw;
    }
}

texture_array::texture_array(int w, int h, int l, int nc, data_type dt, bool use_mipmap)
    : gpu_format(make_internal_format(nc, dt, false /* no compression */))
    , width(w)
    , height(h)
    , nr_of_layers(l)
    , nr_of_channels(nc)
    , has_mipmap(use_mipmap)
{
    create_space(dt);
}

texture_array::texture_array(texture_array&& source)
    : object(std::move(source))
    , gpu_format(source.gpu_format)
    , width(source.width)
    , height(source.height)
    , nr_of_layers(source.nr_of_layers)
    , nr_of_channels(source.nr_of_channels)
    , has_mipmap(source.has_mipmap)
    , used_memory(source.used_memory)
{
    source.gpu_format     = 0;
    source.width          = 0;
    source.height         = 0;
    source.nr_of_layers   = 0;
    source.nr_of_channels = 0;
    source.has_mipmap     = false;
    source.used_memory    = 0;
}

texture_array& texture_array::operator=(texture_array&& source)
{
    if (&source != this) {
        reset();
        object::operator=(std::move(source));
        gpu_format            = source.gpu_format;
        width                 = source.width;
        height                = source.height;
        nr_of_layers          = source.nr_of_layers;
        nr_of_channels        = source.nr_of_channels;
        has_mipmap            = source.has_mipmap;
        used_memory           = source.used_memory;
        source.gpu_format     = 0;
        source.width          = 0;
        source.height         = 0;
        source.nr_of_layers   = 0;
        source.nr_of_channels = 0;
        source.has_mipmap     = false;
        source.used_memory    = 0;
    }
    return *this;
}

texture_array::~texture_array()
{
    reset();
}

void texture_array::reset()
{
    texture_mem_used -= used_memory;
    texture_mem_freed += used_memory;
    if (gpu_id != 0)
        glDeleteTextures(1, &gpu_id);
    gpu_id      = 0;
    used_memory = 0;
}

void texture_array::set_data_generic(
    int layer,
    const void* pixels,
    int count,
    data_type dt,
    int mipmap_level,
    bool update_mipmap_)
{
    const auto work_width  = width >> mipmap_level;
    const auto work_height = height >> mipmap_level;
    if (work_width * work_height * nr_of_channels != int(count))
        THROW(error, "invalid data size!");
    if (gpu_id == 0)
        THROW(error, "trying to set data on invalid texture");
    glBindTexture(GL_TEXTURE_2D_ARRAY, gpu_id);
    glTexSubImage3D(
        GL_TEXTURE_2D_ARRAY,
        mipmap_level,
        0,
        0,
        layer,
        width,
        height,
        1,
        make_user_layout_format(nr_of_channels, false),
        make_user_data_format(dt),
        pixels);

    // if mipmapping is selected, create mipmaps
    if (mipmap_level == 0 && update_mipmap_) {
        update_mipmap();
    }
}

void texture_array::update_mipmap()
{
    if (has_mipmap && gpu_id != 0) {
        glBindTexture(GL_TEXTURE_2D_ARRAY, gpu_id);
        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    }
}

void texture_array::swap(texture_array& other)
{
    std::swap(gpu_id, other.gpu_id);
    std::swap(gpu_format, other.gpu_format);
    std::swap(width, other.width);
    std::swap(height, other.height);
    std::swap(nr_of_layers, other.nr_of_layers);
    std::swap(nr_of_channels, other.nr_of_channels);
    std::swap(has_mipmap, other.has_mipmap);
    std::swap(used_memory, other.used_memory);
}

void texture_array::create_space(data_type dt, const std::string* name)
{
    auto& g = GPU();
    if (width > g.get_max_texture_size() || height > g.get_max_texture_size()) {
        if (name) {
            THROW(file_context_error, "texture size too large, not supported by card", *name);
        } else {
            THROW(error, "texture size too large, not supported by card");
        }
    }

    gpu_format = make_internal_format(nr_of_channels, dt, false /*no compression*/);

    // Create 2D texture array
    if (gpu_id == 0) {
        glGenTextures(1, &gpu_id);
    }
    glBindTexture(GL_TEXTURE_2D_ARRAY, gpu_id);
    // Create storage for data. Note! We need to create space for mipmaps as
    // well, so we have to compute the number of mipmap levels.
    auto levels = 1;
    if (has_mipmap) {
        levels = 0;
        for (auto u = std::max(width, height); u > 0; u >>= 1) {
            ++levels;
        }
    }
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, levels, gpu_format, width, height, nr_of_layers);

    texture_mem_used -= used_memory;
    texture_mem_freed += used_memory;
    used_memory = width * height * nr_of_channels * nr_of_layers * to_data_size(dt);
    if (has_mipmap)
        used_memory = (4 * used_memory) / 3;
    texture_mem_used += used_memory;
    texture_mem_alloced += used_memory;
}

void texture_array::init(const void** data, int nr_of_layers, data_type dt, const std::string* name)
{
    // fixme not used, can be called by template init like for texture class!

    create_space(dt, name);
    // Initialize data
    for (auto layer = 0; layer < nr_of_layers; ++layer) {
        glTexSubImage3D(
            GL_TEXTURE_2D_ARRAY,
            0,
            0,
            0,
            layer,
            width,
            height,
            1,
            make_user_layout_format(nr_of_channels, is_integer_format(dt)),
            make_user_data_format(dt),
            data[layer]);
    }
    // if mipmapping is selected, create mipmaps
    update_mipmap();
}

void texture_array::sub_image(
    int layer,
    const area& ar,
    data_type dt,
    const void* pixels,
    int stride,
    bool update_mipmap_)
{
    auto line_width_in_bytes = nr_of_channels * to_data_size(dt);
    if (stride != 0) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, stride);
        line_width_in_bytes *= stride;
    } else {
        line_width_in_bytes *= ar.size.x;
    }
    // Default data alignment is 4 for each pixel row. We can adjust this to
    // 1,2,4 or 8.
    auto alignment = 4;
    if (line_width_in_bytes & 2) {
        alignment = 2;
    }
    if (line_width_in_bytes & 1) {
        alignment = 1;
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
    glBindTexture(GL_TEXTURE_2D_ARRAY, gpu_id);
    glTexSubImage3D(
        GL_TEXTURE_2D_ARRAY,
        0,
        ar.offset.x,
        ar.offset.y,
        layer,
        ar.size.x,
        ar.size.y,
        1,
        make_user_layout_format(nr_of_channels, is_integer_format(dt)),
        make_user_data_format(dt),
        pixels);
    if (stride != 0) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }
    // if mipmapping is selected, create mipmaps
    if (update_mipmap_) {
        update_mipmap();
    }
}

texture_3D::texture_3D(int w, int h, int d, int nc, data_type dt, bool use_mipmap)
    : gpu_format(make_internal_format(nc, dt, false /* no compression */))
    , width(w)
    , height(h)
    , depth(d)
    , nr_of_channels(nc)
    , has_mipmap(use_mipmap)
{
    auto& g = GPU();
    if (width > g.get_max_texture_size() || height > g.get_max_texture_size() || depth > g.get_max_texture_size()) {
        THROW(error, "texture size too large, not supported by card");
    }

    gpu_format = make_internal_format(nr_of_channels, dt, false /*no compression*/);

    // Create 2D texture array
    if (gpu_id == 0) {
        glGenTextures(1, &gpu_id);
    }
    glBindTexture(GL_TEXTURE_3D, gpu_id);
    // Create storage for data. Note! We need to create space for mipmaps as
    // well, so we have to compute the number of mipmap levels.
    auto levels = 1;
    if (has_mipmap) {
        levels = 0;
        for (auto u = std::max(std::max(width, height), depth); u > 0; u >>= 1) {
            ++levels;
        }
    }
    glTexStorage3D(GL_TEXTURE_3D, levels, gpu_format, width, height, depth);

    texture_mem_used -= used_memory;
    texture_mem_freed += used_memory;
    used_memory = width * height * depth * nr_of_channels * to_data_size(dt);
    if (has_mipmap)
        used_memory = (4 * used_memory) / 3;
    texture_mem_used += used_memory;
    texture_mem_alloced += used_memory;
}

texture_3D::texture_3D(texture_3D&& source)
    : object(std::move(source))
    , gpu_format(source.gpu_format)
    , width(source.width)
    , height(source.height)
    , depth(source.depth)
    , nr_of_channels(source.nr_of_channels)
    , has_mipmap(source.has_mipmap)
    , used_memory(source.used_memory)
{
    source.gpu_format     = 0;
    source.width          = 0;
    source.height         = 0;
    source.depth          = 0;
    source.nr_of_channels = 0;
    source.has_mipmap     = false;
    source.used_memory    = 0;
}

texture_3D& texture_3D::operator=(texture_3D&& source)
{
    if (&source != this) {
        reset();
        object::operator=(std::move(source));
        gpu_format            = source.gpu_format;
        width                 = source.width;
        height                = source.height;
        depth                 = source.depth;
        nr_of_channels        = source.nr_of_channels;
        has_mipmap            = source.has_mipmap;
        used_memory           = source.used_memory;
        source.gpu_format     = 0;
        source.width          = 0;
        source.height         = 0;
        source.depth          = 0;
        source.nr_of_channels = 0;
        source.has_mipmap     = false;
        source.used_memory    = 0;
    }
    return *this;
}

texture_3D::~texture_3D()
{
    reset();
}

void texture_3D::reset()
{
    texture_mem_used -= used_memory;
    texture_mem_freed += used_memory;
    if (gpu_id != 0)
        glDeleteTextures(1, &gpu_id);
    gpu_id      = 0;
    used_memory = 0;
}

void texture_3D::set_data_generic(
    int z,
    const void* pixels,
    int count,
    data_type dt,
    int mipmap_level,
    bool update_mipmap_)
{
    const auto work_width  = width >> mipmap_level;
    const auto work_height = height >> mipmap_level;
    if (work_width * work_height * nr_of_channels != int(count))
        THROW(error, "invalid data size!");
    if (gpu_id == 0)
        THROW(error, "trying to set data on invalid texture");
    glBindTexture(GL_TEXTURE_3D, gpu_id);
    glTexSubImage3D(
        GL_TEXTURE_3D,
        mipmap_level,
        0,
        0,
        z,
        width,
        height,
        1,
        make_user_layout_format(nr_of_channels, false),
        make_user_data_format(dt),
        pixels);

    // if mipmapping is selected, create mipmaps
    if (mipmap_level == 0 && update_mipmap_) {
        update_mipmap();
    }
}

void texture_3D::update_mipmap()
{
    if (has_mipmap && gpu_id != 0) {
        glBindTexture(GL_TEXTURE_3D, gpu_id);
        glGenerateMipmap(GL_TEXTURE_3D);
    }
}

void texture_3D::swap(texture_3D& other)
{
    std::swap(gpu_id, other.gpu_id);
    std::swap(gpu_format, other.gpu_format);
    std::swap(width, other.width);
    std::swap(height, other.height);
    std::swap(depth, other.depth);
    std::swap(nr_of_channels, other.nr_of_channels);
    std::swap(has_mipmap, other.has_mipmap);
    std::swap(used_memory, other.used_memory);
}

void texture_3D::sub_image(int z, const area& ar, data_type dt, const void* pixels, int stride, bool update_mipmap_)
{
    auto line_width_in_bytes = nr_of_channels * to_data_size(dt);
    if (stride != 0) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, stride);
        line_width_in_bytes *= stride;
    } else {
        line_width_in_bytes *= ar.size.x;
    }
    // Default data alignment is 4 for each pixel row. We can adjust this to
    // 1,2,4 or 8.
    auto alignment = 4;
    if (line_width_in_bytes & 2) {
        alignment = 2;
    }
    if (line_width_in_bytes & 1) {
        alignment = 1;
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
    glBindTexture(GL_TEXTURE_3D, gpu_id);
    glTexSubImage3D(
        GL_TEXTURE_3D,
        0,
        ar.offset.x,
        ar.offset.y,
        z,
        ar.size.x,
        ar.size.y,
        1,
        make_user_layout_format(nr_of_channels, is_integer_format(dt)),
        make_user_data_format(dt),
        pixels);
    if (stride != 0) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }
    // if mipmapping is selected, create mipmaps
    if (update_mipmap_) {
        update_mipmap();
    }
}

texture_cube::texture_cube(const std::array<std::string, 6>& filenames, data_type dt, bool use_mipmap)
    : has_mipmap(use_mipmap)
{
    // Load first image to know width/height
    image teximage(filenames[0]);
    width          = teximage.get_width();
    height         = teximage.get_height();
    nr_of_channels = teximage.get_nr_of_channels();
    // First create empty texture array then initialize every layer with image
    // data
    try {
        set_data(0, teximage.get_data());
        for (auto i = 1; i < 6; ++i) {
            teximage = image(filenames[i]);
            if (width != teximage.get_width() || height != teximage.get_height()
                || nr_of_channels != teximage.get_nr_of_channels()) {
                THROW(file_context_error, "image dimensions do not match in texture array", filenames[i]);
            }
            set_data(i, teximage.get_data());
        }
    }
    catch (std::exception&) {
        reset();
        throw;
    }
}

texture_cube::texture_cube(int w, int h, int nc, data_type dt, bool use_mipmap)
    : gpu_format(make_internal_format(nc, dt, false /* no compression */))
    , width(w)
    , height(h)
    , nr_of_channels(nc)
    , has_mipmap(use_mipmap)
{
    auto& g = GPU();
    if (width > g.get_max_texture_size() || height > g.get_max_texture_size()) {
        THROW(error, "texture size too large, not supported by card");
    }

    // Create and initialize with empty pixels
    glGenTextures(1, &gpu_id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, gpu_id);
    for (auto cube_side = 0; cube_side < 6; ++cube_side) {
        glTexImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + cube_side,
            0,
            gpu_format,
            width,
            height,
            0,
            make_user_layout_format(nr_of_channels, is_integer_format(dt)),
            make_user_data_format(dt),
            (void*) nullptr);
    }
    used_memory = width * height * nr_of_channels * 6 * to_data_size(dt);
    texture_mem_used += used_memory;
    texture_mem_alloced += used_memory;
    // if mipmapping is selected, create mipmaps
    update_mipmap();
}

texture_cube::texture_cube(texture_cube&& source)
    : object(std::move(source))
    , gpu_format(source.gpu_format)
    , width(source.width)
    , height(source.height)
    , nr_of_channels(source.nr_of_channels)
    , has_mipmap(source.has_mipmap)
    , used_memory(source.used_memory)
{
    source.gpu_format     = 0;
    source.width          = 0;
    source.height         = 0;
    source.nr_of_channels = 0;
    source.has_mipmap     = false;
    source.used_memory    = 0;
}

texture_cube& texture_cube::operator=(texture_cube&& source)
{
    if (&source != this) {
        reset();
        object::operator=(std::move(source));
        gpu_format            = source.gpu_format;
        width                 = source.width;
        height                = source.height;
        nr_of_channels        = source.nr_of_channels;
        has_mipmap            = source.has_mipmap;
        used_memory           = source.used_memory;
        source.gpu_format     = 0;
        source.width          = 0;
        source.height         = 0;
        source.nr_of_channels = 0;
        source.has_mipmap     = false;
        source.used_memory    = 0;
    }
    return *this;
}

texture_cube::~texture_cube()
{
    reset();
}

void texture_cube::reset()
{
    texture_mem_used -= used_memory;
    texture_mem_freed += used_memory;
    if (gpu_id != 0)
        glDeleteTextures(1, &gpu_id);
    gpu_id      = 0;
    used_memory = 0;
}

void texture_cube::set_data_generic(
    int cube_side,
    const void* pixels,
    int count,
    data_type dt,
    int mipmap_level,
    bool update_mipmap_)
{
    if (cube_side >= 6) {
        THROW(error, "invalid cube map side index!");
    }
    const auto work_width  = width >> mipmap_level;
    const auto work_height = height >> mipmap_level;
    if (work_width * work_height * nr_of_channels != int(count))
        THROW(error, "invalid data size!");
    if (gpu_id == 0)
        THROW(error, "trying to set data on invalid texture");
    glBindTexture(GL_TEXTURE_CUBE_MAP, gpu_id);
    glTexImage2D(
        GL_TEXTURE_CUBE_MAP_POSITIVE_X + cube_side,
        mipmap_level,
        gpu_format,
        work_width,
        work_height,
        0,
        make_user_layout_format(nr_of_channels, false),
        make_user_data_format(dt),
        pixels);

    // if mipmapping is selected, create mipmaps
    if (mipmap_level == 0 && update_mipmap_) {
        update_mipmap();
    }
}

void texture_cube::update_mipmap()
{
    if (has_mipmap && gpu_id != 0) {
        glBindTexture(GL_TEXTURE_CUBE_MAP, gpu_id);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    }
}

void texture_cube::swap(texture_cube& other)
{
    std::swap(gpu_id, other.gpu_id);
    std::swap(gpu_format, other.gpu_format);
    std::swap(width, other.width);
    std::swap(height, other.height);
    std::swap(nr_of_channels, other.nr_of_channels);
    std::swap(has_mipmap, other.has_mipmap);
    std::swap(used_memory, other.used_memory);
}

void texture_cube::init(
    const std::array<const void*, 6>& data,
    data_type dt,
    const std::string* name,
    bool use_compression)
{
    // fixme not used, can be called by template init like for texture class!

    auto& g = GPU();
    if (width > g.get_max_texture_size() || height > g.get_max_texture_size()) {
        if (name) {
            THROW(file_context_error, "texture size too large, not supported by card", *name);
        } else {
            THROW(error, "texture size too large, not supported by card");
        }
    }

    gpu_format = make_internal_format(nr_of_channels, dt, use_compression);

    // Create 2D texture
    if (gpu_id == 0) {
        glGenTextures(1, &gpu_id);
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, gpu_id);

    // write data to mipmap levels
    auto ulf         = make_user_layout_format(nr_of_channels, is_integer_format(dt));
    auto udf         = make_user_data_format(dt);
    auto w           = width;
    auto h           = height;
    auto texel_count = w * h * 6;
    for (auto cube_side = 0; cube_side < 6; ++cube_side) {
        glTexImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + cube_side,
            0 /* mipmap level */,
            gpu_format,
            w,
            h,
            0,
            ulf,
            udf,
            data[cube_side]);
    }

    texture_mem_used -= used_memory;
    texture_mem_freed += used_memory;
    used_memory = texel_count * nr_of_channels * to_data_size(dt);
    texture_mem_used += used_memory;
    texture_mem_alloced += used_memory;
}

void texture_cube::sub_image(
    int cube_side,
    const area& ar,
    data_type dt,
    const void* pixels,
    int stride,
    bool update_mipmap_)
{
    auto line_width_in_bytes = nr_of_channels * to_data_size(dt);
    // stride is in pixels.
    if (stride != 0) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, stride);
        line_width_in_bytes *= stride;
    } else {
        line_width_in_bytes *= ar.size.x;
    }
    // Default data alignment is 4 for each pixel row. We can adjust this to
    // 1,2,4 or 8.
    auto alignment = 4;
    if (line_width_in_bytes & 2) {
        alignment = 2;
    }
    if (line_width_in_bytes & 1) {
        alignment = 1;
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
    glBindTexture(GL_TEXTURE_CUBE_MAP, gpu_id);
    glTexSubImage2D(
        GL_TEXTURE_CUBE_MAP_POSITIVE_X + cube_side,
        0 /* mipmap level */,
        ar.offset.x,
        ar.offset.y,
        ar.size.x,
        ar.size.y,
        make_user_layout_format(nr_of_channels, is_integer_format(dt)),
        make_user_data_format(dt),
        pixels);
    if (stride != 0) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }
    // if mipmapping is selected, create mipmaps
    if (update_mipmap_) {
        update_mipmap();
    }
}

sampler::sampler(sampler::type sampler_type, float anisotropic_level)
{
    glGenSamplers(1, &gpu_id);
    switch (sampler_type) {
        case type::nearest_repeat:
        case type::nearest_clamp:
            glSamplerParameteri(gpu_id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glSamplerParameteri(gpu_id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            break;
        case type::bilinear_repeat:
        case type::bilinear_clamp:
            glSamplerParameteri(gpu_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glSamplerParameteri(gpu_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            break;
        case type::trilinear_repeat:
        case type::trilinear_clamp:
            glSamplerParameteri(gpu_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glSamplerParameteri(gpu_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            break;
        case type::nearest_mipmap_repeat:
        case type::nearest_mipmap_clamp:
            glSamplerParameteri(gpu_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
            glSamplerParameteri(gpu_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            break;
        default:
            THROW(error, "invalid sampler type");
    }
    switch (sampler_type) {
        case type::nearest_repeat:
        case type::bilinear_repeat:
        case type::trilinear_repeat:
        case type::nearest_mipmap_repeat:
            glSamplerParameteri(gpu_id, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glSamplerParameteri(gpu_id, GL_TEXTURE_WRAP_T, GL_REPEAT);
            // in case of 3D textures or cube maps set also R parameter
            glSamplerParameteri(gpu_id, GL_TEXTURE_WRAP_R, GL_REPEAT);
            break;
        case type::nearest_clamp:
        case type::bilinear_clamp:
        case type::trilinear_clamp:
        case type::nearest_mipmap_clamp:
            glSamplerParameteri(gpu_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glSamplerParameteri(gpu_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            // in case of 3D textures or cube maps set also R parameter
            glSamplerParameteri(gpu_id, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            break;
        default:
            THROW(error, "invalid sampler type");
    }
    if (anisotropic_level != 0.0f)
        glSamplerParameterf(gpu_id, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropic_level);
}

sampler::sampler(sampler&& source)
    : object(std::move(source))
{
}

sampler& sampler::operator=(sampler&& source)
{
    if (&source != this) {
        object::operator=(std::move(source));
    }
    return *this;
}

sampler::~sampler()
{
    if (gpu_id != 0)
        glDeleteSamplers(1, &gpu_id);
}

void sampler::bind_to_unit(unsigned tex_unit) const
{
    glBindSampler(tex_unit, gpu_id);
}

const char* frame_buffer_init_failure_reason(int status)
{
    switch (status) {
        case GL_FRAMEBUFFER_UNDEFINED:
            return "default frame buffer does not exist";
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            return "Incomplete attachment";
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            return "Missing attachment";
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            return "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
            return "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
        case GL_FRAMEBUFFER_UNSUPPORTED:
            return "GL_FRAMEBUFFER_UNSUPPORTED";
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
        case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
            return "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS";
        default:
            return "Unknown";
    }
}

frame_buffer::frame_buffer(texture&& tex_, bool withdepthbuffer)
    : my_tex(std::move(tex_))
    , tex(&my_tex)
{
    create(withdepthbuffer);
}

frame_buffer::frame_buffer(const texture* tex_, int level, bool withdepthbuffer)
    : tex(tex_)
    , mipmap_level(level)
{
    create(withdepthbuffer);
}

void frame_buffer::create(bool withdepthbuffer)
{
    // create and bind FBO
    glGenFramebuffers(1, &gpu_id);
    glBindFramebuffer(GL_FRAMEBUFFER, gpu_id);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex->get_gpu_id(), mipmap_level);

    // attach depth buffer if requested
    if (withdepthbuffer) {
        glGenRenderbuffers(1, &depthbuf_id);
        glBindRenderbuffer(GL_RENDERBUFFER, depthbuf_id);
        glRenderbufferStorage(
            GL_RENDERBUFFER, GL_DEPTH_COMPONENT, tex->get_width() >> mipmap_level, tex->get_height() >> mipmap_level);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthbuf_id);
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        destroy();
        log_warning("FBO initialization check failed: " << frame_buffer_init_failure_reason(status));
        THROW(error, "FBO initialization check failed");
    }
    // unbind for now
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

frame_buffer::frame_buffer(frame_buffer&& source)
    : object(std::move(source))
    , depthbuf_id(source.depthbuf_id)
    , my_tex(std::move(source.my_tex))
    , tex(source.tex)
    , mipmap_level(source.mipmap_level)
    , bound(source.bound)
{
    source.depthbuf_id = 0;
    source.bound       = false;
    if (source.tex == &source.my_tex) {
        tex = &my_tex;
    }
}

frame_buffer& frame_buffer::operator=(frame_buffer&& source)
{
    if (&source != this) {
        object::operator=(std::move(source));
        depthbuf_id        = source.depthbuf_id;
        my_tex             = std::move(source.my_tex);
        tex                = (source.tex == &source.my_tex) ? &my_tex : source.tex;
        mipmap_level       = source.mipmap_level;
        bound              = source.bound;
        source.depthbuf_id = 0;
        source.bound       = false;
    }
    return *this;
}

frame_buffer::~frame_buffer()
{
    destroy();
}

void frame_buffer::bind() const
{
    if (gpu_id == 0) {
        THROW(error, "try to bind empty frame buffer");
    }
    if (bound)
        THROW(error, "FBO bind(): already bound!");
    GPU().bind_frame_buffer(gpu_id);
    if (gpu_id != 0) {
        glViewport(0, 0, tex->get_width() >> mipmap_level, tex->get_height() >> mipmap_level);
        bound = true;
    }
}

void frame_buffer::unbind() const
{
    if (!bound)
        THROW(error, "FBO unbind(): not bound yet!");
    GPU().bind_frame_buffer(0);
    bound = false;
    glTextureBarrier(); // fixme test that data is there!
}

void frame_buffer::destroy()
{
    if (gpu_id != 0)
        glDeleteFramebuffers(1, &gpu_id);
    if (depthbuf_id != 0)
        glDeleteRenderbuffers(1, &depthbuf_id);
    gpu_id      = 0;
    depthbuf_id = 0;
}

shader::shader(const std::string& filename, type stype, bool immediate, std::initializer_list<std::string> defines)
{
    GLenum st;
    switch (stype) {
        case type::vertex:
            st = GL_VERTEX_SHADER;
            break;
        case type::fragment:
            st = GL_FRAGMENT_SHADER;
            break;
        case type::geometry:
            st = GL_GEOMETRY_SHADER;
            break;
        case type::compute:
            st = GL_COMPUTE_SHADER;
            break;
        default:
            THROW(error, "invalid shader type");
    }
    gpu_id = glCreateShader(st);
    if (gpu_id == 0)
        THROW(error, "can't create glsl shader");
    try {
        // read shader source if requested
        std::unique_ptr<std::ifstream> ifprg;
        if (!immediate) {
            ifprg.reset(new std::ifstream(filename.c_str(), std::ios::in));
            if (ifprg->fail())
                THROW(file_read_error, filename);
        }

        // the program as string
        std::string prg;

        // add special optimizations for Nvidia cards
        if (0 /*is_nvidia_card fixme */) {
            // add some more performance boost stuff if requested
            if (0) { // fixme: later add cfg-switch for it
                prg += "#pragma optionNV(fastmath on)\n"
                       "#pragma optionNV(fastprecision on)\n"
                       /*"#pragma optionNV(ifcvt all)\n"*/
                       "#pragma optionNV(inline all)\n"
                    //"#pragma optionNV(unroll all)\n"  not faster on 7x00
                    // hardware
                    ;
            }
        }

        // add global hqsfx flag, but define first, so user can override it
        // if (enable_hqsfx)
        //	prg += "#define HQSFX\n";

        // read lines.
        auto nr_of_lines_read = 0;
        if (ifprg.get()) {
            while (!ifprg->eof()) {
                std::string s;
                std::getline(*ifprg, s);
                ++nr_of_lines_read;
                // handle includes
                if (s.substr(0, 8) == "#include") {
                    // handle including
                    std::string include_filename = s.substr(10, s.length() - 11);
                    std::ifstream includeprg(include_filename);
                    while (!includeprg.eof()) {
                        std::getline(includeprg, s);
                        prg += s + "\n";
                    }
                } else {
                    prg += s + "\n";
                    if (nr_of_lines_read == 1) {
                        // add defines after #version core line
                        for (const auto& definition : defines) {
                            prg += "#define " + definition + "\n";
                        }
                    }
                }
            }
        } else {
            prg += filename;
        }
        // For debugging
        // std::cout << prg;

        const char* prg_cstr = prg.c_str();
        glShaderSource(gpu_id, 1, &prg_cstr, nullptr);

        glCompileShader(gpu_id);

        GLint compiled = GL_FALSE;
        glGetShaderiv(gpu_id, GL_COMPILE_STATUS, &compiled);

        // get compile log
        GLint maxlength = 0;
        glGetShaderiv(gpu_id, GL_INFO_LOG_LENGTH, &maxlength);
        std::string compilelog(maxlength + 1, ' ');
        GLsizei length = 0;
        glGetShaderInfoLog(gpu_id, maxlength, &length, &compilelog[0]);

        if (!compiled) {
            log_warning("compiling failed, log:");
            log_warning(compilelog);
            THROW(file_context_error, "compiling of shader failed : ", filename);
        }

        log_info(std::string("shader compiled successfully, log: ") + compilelog + "\n");
    }
    catch (...) {
        glDeleteShader(gpu_id);
        throw;
    }
}

shader::shader(shader&& source)
    : object(std::move(source))
{
}

shader& shader::operator=(shader&& source)
{
    if (&source != this) {
        object::operator=(std::move(source));
    }
    return *this;
}

shader::~shader()
{
    if (gpu_id != 0)
        glDeleteShader(gpu_id);
}

program::program(const std::string& basefilename, std::initializer_list<std::string> defines)
{
    init(basefilename, defines);
}

program::program(const shader& computeshader)
{
    link(computeshader);
}

program::program(const shader& vertexshader, const shader& fragmentshader)
{
    link(vertexshader, fragmentshader);
}

program::program(program&& source)
    : object(std::move(source))
    , linked(source.linked)
{
    source.linked = false;
}

program::~program()
{
    if (gpu_id != 0) {
        glDeleteProgram(gpu_id);
    }
}

program& program::operator=(program&& source)
{
    if (&source != this) {
        if (gpu_id != 0) {
            glDeleteProgram(gpu_id);
        }
        gpu_id        = source.gpu_id;
        linked        = source.linked;
        source.gpu_id = 0;
        source.linked = false;
    }
    return *this;
}

void program::link(const shader& computeshader)
{
    if (computeshader.get_gpu_id() == 0)
        THROW(error, "linking with invalid shader");
    if (gpu_id == 0) {
        gpu_id = glCreateProgram();
    }
    glAttachShader(gpu_id, computeshader.get_gpu_id());
    glLinkProgram(gpu_id);
    GLint waslinked = GL_FALSE;
    glGetProgramiv(gpu_id, GL_LINK_STATUS, &waslinked);
    glDetachShader(gpu_id, computeshader.get_gpu_id());

    if (!waslinked) {
        log_warning("linking failed, log:");
        GLint maxlength = 0;
        glGetProgramiv(gpu_id, GL_INFO_LOG_LENGTH, &maxlength);
        std::string compilelog(maxlength + 1, ' ');
        GLsizei length = 0;
        glGetProgramInfoLog(gpu_id, maxlength, &length, &compilelog[0]);
        log_warning(compilelog);
        THROW(error, "linking of program failed");
    }

    linked = true;
}

void program::link(const shader& vertexshader, const shader& fragmentshader)
{
    if (vertexshader.get_gpu_id() == 0 || fragmentshader.get_gpu_id() == 0)
        THROW(error, "linking with invalid shader");
    if (gpu_id == 0) {
        gpu_id = glCreateProgram();
    }
    glAttachShader(gpu_id, vertexshader.get_gpu_id());
    glAttachShader(gpu_id, fragmentshader.get_gpu_id());
    glLinkProgram(gpu_id);
    GLint waslinked = GL_FALSE;
    glGetProgramiv(gpu_id, GL_LINK_STATUS, &waslinked);
    glDetachShader(gpu_id, fragmentshader.get_gpu_id());
    glDetachShader(gpu_id, vertexshader.get_gpu_id());

    if (!waslinked) {
        log_warning("linking failed, log:");
        GLint maxlength = 0;
        glGetProgramiv(gpu_id, GL_INFO_LOG_LENGTH, &maxlength);
        std::string compilelog(maxlength + 1, ' ');
        GLsizei length = 0;
        glGetProgramInfoLog(gpu_id, maxlength, &length, &compilelog[0]);
        log_warning(compilelog);
        // fixme where does the log go to?
        THROW(error, compilelog + "linking of program failed");
    }

    linked = true;
}

void program::init(const std::string& basefilename, std::initializer_list<std::string> defines)
{
    shader vs(basefilename + ".vshader", shader::type::vertex, false, defines);
    shader fs(basefilename + ".fshader", shader::type::fragment, false, defines);
    link(vs, fs);
}

render_context::~render_context()
{
    if (gpu_id != 0) {
        GPU().bind_new_VAO(0);
        glDeleteVertexArrays(1, &gpu_id);
    }
}

render_context::render_context(render_context&& r)
    : object(std::move(r))
    , initialized(r.initialized)
    , shared_render_program(r.shared_render_program)
    , render_program(std::move(r.render_program))
    , vertex_buffers(std::move(r.vertex_buffers))
    , uniform_buffers(std::move(r.uniform_buffers))
    , sst_buffers(std::move(r.sst_buffers))
    , textures(std::move(r.textures))
    , samplers(std::move(r.samplers))
    , idx_buffer(nullptr)
    , my_idx_buffer(std::move(r.my_idx_buffer))
    , primitivetype(r.primitivetype)
    , nr_of_indices(r.nr_of_indices)
    , depth_test(r.depth_test)
    , depth_write(r.depth_write)
    , wire_frame(r.wire_frame)
    , use_primitive_restart(r.use_primitive_restart)
    , primitive_restart_index(r.primitive_restart_index)
    , render_side(r.render_side)
    , blend_func(r.blend_func)
    , my_vertex_buffers(std::move(r.my_vertex_buffers))
    , vertex_attrib_divisors(std::move(r.vertex_attrib_divisors))
    , nr_of_instances(r.nr_of_instances)
{
    r.initialized           = false;
    r.shared_render_program = nullptr;
    // idx_buffer must point to my_idx_buffer when it did on source!
    if (r.idx_buffer == &r.my_idx_buffer) {
        idx_buffer = &my_idx_buffer;
    } else {
        idx_buffer = r.idx_buffer;
    }
    r.idx_buffer      = nullptr;
    r.nr_of_indices   = 0;
    r.nr_of_instances = 0;
    assert_internal_vertex_buffer_refs();
}

render_context& render_context::operator=(render_context&& r)
{
    if (this != &r) {
        object::operator=(std::move(r));
        initialized             = r.initialized;
        r.initialized           = false;
        shared_render_program   = r.shared_render_program;
        r.shared_render_program = nullptr;
        render_program          = std::move(r.render_program);
        vertex_buffers          = std::move(r.vertex_buffers);
        uniform_buffers         = std::move(r.uniform_buffers);
        sst_buffers             = std::move(r.sst_buffers);
        textures                = std::move(r.textures);
        samplers                = std::move(r.samplers);
        my_idx_buffer           = std::move(r.my_idx_buffer);
        // idx_buffer must point to my_idx_buffer when it did on source!
        if (r.idx_buffer == &r.my_idx_buffer) {
            idx_buffer = &my_idx_buffer;
        } else {
            idx_buffer = r.idx_buffer;
        }
        r.idx_buffer            = nullptr;
        primitivetype           = r.primitivetype;
        nr_of_indices           = r.nr_of_indices;
        r.nr_of_indices         = 0;
        depth_test              = r.depth_test;
        depth_write             = r.depth_write;
        wire_frame              = r.wire_frame;
        use_primitive_restart   = r.use_primitive_restart;
        primitive_restart_index = r.primitive_restart_index;
        render_side             = r.render_side;
        blend_func              = r.blend_func;
        my_vertex_buffers       = std::move(r.my_vertex_buffers);
        assert_internal_vertex_buffer_refs();
        vertex_attrib_divisors = std::move(r.vertex_attrib_divisors);
        nr_of_instances        = r.nr_of_instances;
        r.nr_of_instances      = 0;
    }
    return *this;
}

void render_context::add(unsigned location, const vertex_buffer& vb, unsigned attrib_divisor)
{
    initialized = false;
    if (vertex_buffers.size() <= location) {
        vertex_buffers.resize(location + 1);
        vertex_attrib_divisors.resize(location + 1);
    }
    vertex_buffers[location]         = &vb;
    vertex_attrib_divisors[location] = attrib_divisor;
}

void render_context::add(unsigned location, vertex_buffer&& vb, unsigned attrib_divisor)
{
    initialized = false;
    if (my_vertex_buffers.size() <= location) {
        my_vertex_buffers.resize(location + 1);
        // pointers are now broken from resize, so fix them, if they refer to
        // elements in the vector
        assert_internal_vertex_buffer_refs();
    }
    my_vertex_buffers[location] = std::move(vb);
    if (vertex_buffers.size() <= location) {
        vertex_buffers.resize(location + 1);
        vertex_attrib_divisors.resize(location + 1);
    }
    vertex_buffers[location]         = &my_vertex_buffers[location];
    vertex_attrib_divisors[location] = attrib_divisor;
}

void render_context::add(unsigned location, const uniform_buffer& ub)
{
    if (ub.get_gpu_id() == 0)
        THROW(error, "trying to attach invalid uniform buffer to render context");
    // Uniform buffers are bound on use() call, so we don't need to reinitialize
    // render context if they change
    if (uniform_buffers.size() <= location) {
        uniform_buffers.resize(location + 1);
    }
    uniform_buffers[location] = ub.get_gpu_id();
}

void render_context::add(unsigned location, const shader_storage_buffer& sb)
{
    if (sb.get_gpu_id() == 0)
        THROW(error, "trying to attach invalid shader storage buffer to render context");
    if (sst_buffers.size() <= location) {
        sst_buffers.resize(location + 1);
    }
    sst_buffers[location] = sb.get_gpu_id();
}

void render_context::add(unsigned unit, const texture& tex, sampler::type smp)
{
    add_tex_id(unit, tex.get_gpu_id(), smp);
}

void render_context::add(const textures_and_samplers& textures_and_samplers)
{
    textures.resize(textures_and_samplers.size());
    samplers.resize(textures_and_samplers.size());
    auto& ii = interface::instance();
    for (unsigned i = 0; i < unsigned(textures_and_samplers.size()); ++i) {
        textures[i] = textures_and_samplers[i].first == nullptr ? 0 : textures_and_samplers[i].first->get_gpu_id();
        samplers[i] = ii.get_sampler_gpu_id(
            textures_and_samplers[i].first == nullptr ? -1 : int(textures_and_samplers[i].second));
    }
}

void render_context::add(unsigned unit, const texture_array& tex_arr, sampler::type smp)
{
    add_tex_id(unit, tex_arr.get_gpu_id(), smp);
}

void render_context::add(unsigned unit, const texture_3D& tex_3d, sampler::type smp)
{
    add_tex_id(unit, tex_3d.get_gpu_id(), smp);
}

void render_context::add(unsigned unit, const texture_cube& tex_cube, sampler::type smp)
{
    add_tex_id(unit, tex_cube.get_gpu_id(), smp);
}

void render_context::add(const index_buffer& idxbuf)
{
    initialized   = false;
    my_idx_buffer = index_buffer();
    idx_buffer    = &idxbuf;
}

void render_context::add(index_buffer&& idxbuf)
{
    initialized   = false;
    my_idx_buffer = std::move(idxbuf);
    idx_buffer    = &my_idx_buffer;
}

void render_context::add(const program& prg)
{
    // Programs are used on use() call, so we don't need to reinitialize
    // render context if they change
    shared_render_program = &prg;
}

void render_context::add(program&& prg)
{
    // Programs are used on use() call, so we don't need to reinitialize
    // render context if they change
    render_program        = std::move(prg);
    shared_render_program = &render_program;
}

void render_context::add(primitive_type type, int nr_of_indices_)
{
    primitivetype = type;
    nr_of_indices = nr_of_indices_;
}

void render_context::enable_depth_test(bool enable)
{
    depth_test = enable;
}

void render_context::enable_depth_buffer_write(bool enable)
{
    depth_write = enable;
}

void render_context::enable_wire_frame(bool enable)
{
    wire_frame = enable;
}

void render_context::use_primitive_restart_index(bool enable, uint32_t index)
{
    use_primitive_restart   = enable;
    primitive_restart_index = index;
}

void render_context::set_face_render_side(face_render_side side)
{
    switch (side) {
        case face_render_side::back:
        case face_render_side::front:
        case face_render_side::both:
        case face_render_side::none:
            render_side = side;
            break;
        default:
            THROW(error, "Invalid face render side value");
    }
}

void render_context::set_blend_function(blend_func_type bf)
{
    blend_func = bf;
}

void render_context::set_2D_drawing(bool enable)
{
    if (enable) {
        enable_depth_test(false);
        enable_depth_buffer_write(false);
        set_face_render_side(face_render_side::both);
    } else {
        enable_depth_test(true);
        enable_depth_buffer_write(true);
        set_face_render_side(face_render_side::front);
    }
}

void render_context::init()
{
    if (textures.size() != samplers.size())
        THROW(error, "texture units used must match samplers set!");
    auto& g = GPU();
    if (gpu_id != 0) {
        g.bind_new_VAO(0);
        glDeleteVertexArrays(1, &gpu_id);
        gpu_id = 0;
    }
    // If we have no vertex attributes nor indices (valid situation!), we still
    // need an VAO, even if that is empty! Create vertex array object for VBO
    // set.
    glGenVertexArrays(1, &gpu_id);
    g.bind_new_VAO(gpu_id);
    unsigned location = 0;
    for (auto vb : vertex_buffers) {
        if (vb != nullptr) {
            glEnableVertexAttribArray(location);
            vb->bind();
            // offset (always 0), number of data elements, data type,
            // normalize?, stride, pointer always use packed data (stride 0)
            if (is_integer_format(vb->get_data_type())) {
                // Note there is an glVertexAttribLPointer for 64bit values, but
                // only for double...
                glVertexAttribIPointer(location, vb->get_data_count(), to_gl_type(vb->get_data_type()), 0, nullptr);
            } else {
                // Normalizing has to be done for fixed point data values that
                // mean normalized data, that is only ubyte.
                glVertexAttribPointer(
                    location,
                    vb->get_data_count(),
                    to_gl_type(vb->get_data_type()),
                    (vb->get_data_type() == data_type::ubyte) ? GL_TRUE : GL_FALSE,
                    0,
                    nullptr);
            }
            if (vertex_attrib_divisors[location] != 0) {
                glVertexAttribDivisor(location, vertex_attrib_divisors[location]);
            }
        } else {
            glDisableVertexAttribArray(location);
        }
        ++location;
    }
    // glBindBuffer(GL_ARRAY_BUFFER, 0);	// paranoia, make sure nothing else
    // is bound if we have an index buffer for the rendering context, bind it,
    // so that the VAO will remember and use it
    if (idx_buffer != nullptr) {
        idx_buffer->bind();
    }
    g.bind_new_VAO(0);
    // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);	// paranoia, don't bind any
    // index buffer
    initialized = true;
}

static bool use_binding(unsigned& first, unsigned& count, const std::vector<unsigned>& vec)
{
    const unsigned sz = unsigned(vec.size());
    first             = sz;
    count             = 0;
    for (unsigned i = 0; i < sz; ++i) {
        if (vec[i] != 0) {
            first = i;
            count = 1;
            for (++i; i < sz; ++i) {
                if (vec[i] != 0) {
                    count = i + 1 - first;
                }
            }
            break;
        }
    }
    return count > 0;
}

void render_context::use() const
{
    if (!initialized || shared_render_program == nullptr)
        THROW(error, "trying to use uninitialized render context");
    //
    // Here is the main setup code!
    //
    auto& g = GPU();

    // Bind vertex array object to use the vertex data (VBOs).
    g.bind_new_VAO(gpu_id);

    unsigned first = 0, count = 0;
    // Bind uniform buffer objects.
    if (use_binding(first, count, uniform_buffers)) {
        glBindBuffersBase(GL_UNIFORM_BUFFER, first, count, &uniform_buffers[first]);
    }

    // Bind shader storage buffer objects.
    if (use_binding(first, count, sst_buffers)) {
        glBindBuffersBase(GL_SHADER_STORAGE_BUFFER, first, count, &sst_buffers[first]);
    }

    // Bind textures and samplers.
    if (textures.size() > 32)
        THROW(error, "maximum texture count exceeded");
    if (use_binding(first, count, textures)) {
        glBindTextures(first, count, &textures[first]);
        glBindSamplers(first, count, &samplers[first]);
    }

    // set up interface (do this first)
    g.enable_depth_test(depth_test);
    g.enable_depth_buffer_write(depth_write);
    g.enable_wire_frame(wire_frame);
    g.use_primitive_restart_index(use_primitive_restart, primitive_restart_index);
    g.set_face_render_side(render_side);
    g.set_blend_function(blend_func);

    // Use program.
    g.use_program(*shared_render_program);
}

static GLuint primitive_type_table[size_t(primitive_type::number)] =
    {GL_POINTS, GL_LINES, GL_LINE_STRIP, GL_LINE_LOOP, GL_TRIANGLES, GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN};

void render_context::draw_primitives(primitive_type type, int first_index, int nr_of_indices) const
{
    if (idx_buffer != nullptr) {
        // to give start index we need to know data size of index buffer, to
        // give correct offset.
        auto byte_offset   = to_data_size(idx_buffer->get_data_type()) * first_index;
        const uint8_t* ptr = (const uint8_t*) uintptr_t(byte_offset);
        glDrawElements(primitive_type_table[size_t(type)], nr_of_indices, to_gl_type(idx_buffer->get_data_type()), ptr);
    } else {
        // straight forward
        glDrawArrays(primitive_type_table[size_t(type)], first_index, nr_of_indices);
    }
}

void render_context::draw_primitives(primitive_type type, int first_index, int nr_of_indices, int nr_of_instances) const
{
    if (idx_buffer != nullptr) {
        // to give start index we need to know data size of index buffer, to
        // give correct offset.
        auto byte_offset   = to_data_size(idx_buffer->get_data_type()) * first_index;
        const uint8_t* ptr = (const uint8_t*) uintptr_t(byte_offset);
        glDrawElementsInstanced(
            primitive_type_table[size_t(type)],
            nr_of_indices,
            to_gl_type(idx_buffer->get_data_type()),
            ptr,
            nr_of_instances);
    } else {
        // straight forward
        glDrawArraysInstanced(primitive_type_table[size_t(type)], first_index, nr_of_indices, nr_of_instances);
    }
}

void render_context::set_nr_of_instances(int nr_i)
{
    nr_of_instances = nr_i;
}

void render_context::render() const
{
    if (primitivetype == primitive_type::number)
        THROW(error, "no primitive type set for render()");
    use();
    if (nr_of_instances <= 1) {
        draw_primitives(primitivetype, 0, nr_of_indices);
    } else {
        draw_primitives(primitivetype, 0, nr_of_indices, nr_of_instances);
    }
}

void render_context::render(int nr_of_instances) const
{
    if (primitivetype == primitive_type::number)
        THROW(error, "no primitive type set for render()");
    use();
    draw_primitives(primitivetype, 0, nr_of_indices, nr_of_instances);
}

void render_context::assert_internal_vertex_buffer_refs()
{
    for (std::size_t i = 0; i < vertex_buffers.size(); ++i) {
        if (i < my_vertex_buffers.size() && !my_vertex_buffers[i].empty()) {
            vertex_buffers[i] = &my_vertex_buffers[i];
        }
    }
}

void render_context::add_tex_id(unsigned unit, unsigned tex_id, sampler::type smp)
{
    if (tex_id == 0)
        THROW(error, "trying to attach invalid texture to render context");
    // Textures are bound on use() call, so we don't need to reinitialize
    // render context if they change
    if (textures.size() <= unit) {
        textures.resize(unit + 1);
        samplers.resize(unit + 1);
    }
    textures[unit] = tex_id;
    samplers[unit] = interface::instance().get_sampler_gpu_id(int(smp));
}

void compute_context::add(unsigned location, const uniform_buffer& ub)
{
    if (ub.get_gpu_id() == 0)
        THROW(error, "trying to attach invalid uniform buffer to compute context");
    if (uniform_buffers.size() <= location) {
        uniform_buffers.resize(location + 1);
    }
    uniform_buffers[location] = ub.get_gpu_id();
}

void compute_context::add(unsigned location, const shader_storage_buffer& sb)
{
    if (sb.get_gpu_id() == 0)
        THROW(
            error,
            "trying to attach invalid shader storage buffer to compute "
            "context");
    if (sst_buffers.size() <= location) {
        sst_buffers.resize(location + 1);
    }
    sst_buffers[location] = sb.get_gpu_id();
}

void compute_context::add(unsigned unit, const texture& tex)
{
    if (tex.get_gpu_id() == 0)
        THROW(error, "trying to attach invalid texture to compute context");
    if (textures.size() <= unit) {
        textures.resize(unit + 1);
    }
    textures[unit] = tex.get_gpu_id();
    // set texture parameters, otherwise it won't work (no samplers are used)
    glBindTexture(GL_TEXTURE_2D, tex.get_gpu_id());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void compute_context::add(unsigned unit, const texture& tex, int level)
{
    add(unit, tex);
    if (texture_levels.size() <= unit) {
        texture_levels.resize(unit + 1);
        texture_formats.resize(unit + 1);
    }
    texture_levels[unit]  = level;
    texture_formats[unit] = tex.get_gpu_format();
}

void compute_context::add(unsigned unit, const texture_array& tex_arr)
{
    if (tex_arr.get_gpu_id() == 0)
        THROW(error, "trying to attach invalid texture array to compute context");
    if (textures.size() <= unit) {
        textures.resize(unit + 1);
    }
    textures[unit] = tex_arr.get_gpu_id();
    // set texture parameters, otherwise it won't work (no samplers are used)
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex_arr.get_gpu_id());
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void compute_context::add(unsigned unit, const texture_cube& tex_cube)
{
    if (tex_cube.get_gpu_id() == 0)
        THROW(error, "trying to attach invalid texture cubemap to compute context");
    if (textures.size() <= unit) {
        textures.resize(unit + 1);
    }
    textures[unit] = tex_cube.get_gpu_id();
    // set texture parameters, otherwise it won't work (no samplers are used)
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex_cube.get_gpu_id());
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}

void compute_context::add(const shader& shd)
{
    compute_program.link(shd);
    initialized = true;
}

void compute_context::use() const
{
    if (!initialized)
        THROW(error, "trying to use uninitialized compute context");

    unsigned first = 0, count = 0;
    // Bind uniform buffer objects.
    if (use_binding(first, count, uniform_buffers)) {
        glBindBuffersBase(GL_UNIFORM_BUFFER, first, count, &uniform_buffers[first]);
    }

    // Bind shader storage buffer objects.
    if (use_binding(first, count, sst_buffers)) {
        glBindBuffersBase(GL_SHADER_STORAGE_BUFFER, first, count, &sst_buffers[first]);
    }

    // Bind textures
    if (textures.size() > 32)
        THROW(error, "maximum texture count exceeded");
    // Check if specific mipmap levels are requested, then we need special
    // binding code
    bool special_mipmap_requested = helper::any_of(texture_levels, [](unsigned l) { return l > 0; });
    if (special_mipmap_requested) {
        for (unsigned index = 0; index < unsigned(textures.size()); ++index) {
            if (textures[index] == 0) {
                glBindImageTexture(index, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8);
            } else {
                unsigned level = (index < texture_levels.size()) ? texture_levels[index] : 0;
                glBindImageTexture(index, textures[index], level, GL_TRUE, 0, GL_READ_WRITE, texture_formats[index]);
            }
        }
    } else {
        // Samplers are not used
        if (use_binding(first, count, textures)) {
            // Note: combines call binds images as read/write, but shader
            // defines it differently. Not sure if that affects performance.
            glBindImageTextures(first, count, &textures[first]);
        }
    }

    // Use program.
    GPU().use_program(compute_program);
}

void compute_context::set_compute_size(int x, int y, int z)
{
    if (x == 0 || y == 0 || z == 0) {
        THROW(error, "invalid compute size");
    }
    compute_size = vector3i(x, y, z);
}

void compute_context::compute() const
{
    use();
    glDispatchCompute(compute_size.x, compute_size.y, compute_size.z);
}

void compute_context::wait_for_output() const
{
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
    // If we would like to wait for any kind of data:
    // glMemoryBarrier(GL_ALL_BARRIER_BITS);
    // This call could be used to measure times of computes.
    // glFinish();
}

interface::interface()
{
#ifdef WIN32
    glewExperimental = GL_TRUE; // needed for GL3.3+ core
    if (glewInit() != GLEW_OK) {
        THROW(error, "glew init failed");
    }
#endif
    // request max. texture size
    GLint i;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &i);
    max_texture_size = int(i);
    // request GL version etc. for logging
    const char* vendorc      = (const char*) glGetString(GL_VENDOR);
    const char* rendererc    = (const char*) glGetString(GL_RENDERER);
    const char* versionc     = (const char*) glGetString(GL_VERSION);
    const char* extensionsc  = (const char*) glGetString(GL_EXTENSIONS);
    std::string vendor       = vendorc ? vendorc : "unknown vender";
    std::string renderer     = rendererc ? rendererc : "unknown render";
    std::string version      = versionc ? versionc : "unknown version";
    std::string extensions   = extensionsc ? extensionsc : "none";
    GLint nrtexunits         = 0;
    GLint maxviewportdims[2] = {0, 0};
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &nrtexunits);
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, maxviewportdims);
    // Request maximum anisotropic filter level
    float max_anisotropic_level = 0.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropic_level);

    log_info(
        "***** OpenGL Information *****\n\n\n"
        << "OpenGL vendor : " << vendor << "\n"
        << "GL renderer : " << renderer << "\n"
        << "GL version : " << version << "\n"
        << "GL max texture size : " << max_texture_size << "\n"
        << "GL number of texture units : " << nrtexunits << "\n"
        << "GL maximum viewport dimensions : " << maxviewportdims[0] << "x" << maxviewportdims[1] << "\n"
        << "GL maximum anisotropic level : " << max_anisotropic_level << "\n"
        << "Supported GL extensions :\n"
        << extensions << "\n");

    // Initialize basic GL stuff.
    // Allow culling and blending, set default depth value and depth comparison
    // function.
    glEnable(GL_CULL_FACE);
    glClearDepth(1.0);
    glDepthFunc(GL_LESS);
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    // Render only pixels as points
    glPointSize(1);

    // use anisotropic filtering maximally, doesn't hurt performance much and is
    // nearly always worth it
    anisotropic_level = max_anisotropic_level;

    // Set up basic values for depth buffer etc. - note different values than in
    // initializer list, so that they are definitly set by the functions!
    enable_depth_buffer_write(true);
    enable_depth_test(true);
    enable_wire_frame(false);
    use_primitive_restart_index(false, 0);
    set_face_render_side(face_render_side::front);
    set_blend_function(blend_func_type::standard);

#ifdef DEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(opengl_error_callback, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, true);
#endif

    // Initialize default samplers
    default_samplers.reserve(int(sampler::type::number));
    for (int i = 0; i < int(sampler::type::number); ++i) {
        default_samplers.emplace_back((sampler::type) i, anisotropic_level);
    }
}

interface::~interface() = default;

void interface::init_frame_buffer(int width, int height)
{
    glViewport(0, 0, width, height);
    viewport_data = vector4t<int>(0, 0, width, height);
}

void interface::init_frame_buffer(int offset_x, int offset_y, int width, int height)
{
    glViewport(offset_x, offset_y, width, height);
    viewport_data = vector4i(offset_x, offset_y, width, height);
}

void interface::clear_frame_buffer(const color& c)
{
    const float f = 1.f / 255;
    glClearColor(c.r * f, c.g * f, c.b * f, c.a * f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void interface::clear_depth_buffer()
{
    // we have to allow writing to the depth buffer first or clearing is not
    // effective.
    enable_depth_buffer_write(true);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void interface::clear_depth_and_frame_buffer(const color& c)
{
    // we have to allow writing to the depth buffer first or clearing is not
    // effective.
    enable_depth_buffer_write(true);
    const float f = 1.f / 255;
    glClearColor(c.r * f, c.g * f, c.b * f, c.a * f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void interface::enable_depth_test(bool enable)
{
    if (depth_test != enable) {
        if (enable) {
            glEnable(GL_DEPTH_TEST);
        } else {
            glDisable(GL_DEPTH_TEST);
        }
        depth_test = enable;
    }
}

void interface::enable_depth_buffer_write(bool enable)
{
    if (depth_write != enable) {
        glDepthMask(enable ? GL_TRUE : GL_FALSE);
        depth_write = enable;
    }
}

void interface::enable_wire_frame(bool enable)
{
    if (wire_frame != enable) {
        // Note that we may have to set GL_LEQUAL or even GL_EQUAL for depth
        // comparison then.
        glPolygonMode(GL_FRONT_AND_BACK, enable ? GL_LINE : GL_FILL);
        wire_frame = enable;
    }
}

void interface::use_primitive_restart_index(bool enable, uint32_t index)
{
    if (use_primitive_restart != enable) {
        if (enable) {
            glEnable(GL_PRIMITIVE_RESTART);
            if (index != primitive_restart_index) {
                glPrimitiveRestartIndex(index);
                primitive_restart_index = index;
            }
        } else {
            glDisable(GL_PRIMITIVE_RESTART);
        }
        use_primitive_restart = enable;
    }
}

void interface::set_face_render_side(face_render_side side)
{
    if (render_side != side) {
        // (re)enable culling when something should be culled (cull = skip)
        if (render_side == face_render_side::both) {
            glEnable(GL_CULL_FACE);
        }
        switch (side) {
            case face_render_side::back:
                glCullFace(GL_FRONT);
                break;
            case face_render_side::front:
                glCullFace(GL_BACK);
                break;
            case face_render_side::both:
                glDisable(GL_CULL_FACE);
                break;
            case face_render_side::none:
                glCullFace(GL_FRONT_AND_BACK);
                break;
            default:
                THROW(error, "Invalid face render side value");
        }
        render_side = side;
    }
}

void interface::set_blend_function(blend_func_type bf)
{
    if (blend_func != bf) {
        switch (bf) {
            case blend_func_type::srcalpha:
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                break;
            case blend_func_type::one_srccolor:
                glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
                break;
            default:
                glBlendFunc(GL_ONE, GL_ZERO); // no blending, direkt copy of input
                break;
        }
        blend_func = bf;
    }
}

bool interface::bind_new_VAO(int vao)
{
    if (vao != current_vao) {
        glBindVertexArray(vao);
        current_vao = vao;
        return true;
    }
    return false;
}

bool interface::use_program(const program& prg)
{
    if (&prg != current_program) {
        current_program = &prg;
        glUseProgram(current_program->get_gpu_id());
        return true;
    }
    return false;
}

void interface::bind_frame_buffer(unsigned id)
{
    // may be check for stacked binding, which is not allowed or for double
    // binding of same frame buffer
    glBindFramebuffer(GL_FRAMEBUFFER, id);
    current_fb_id = id;
    if (id == 0) {
        glViewport(viewport_data.x, viewport_data.y, viewport_data.z, viewport_data.w);
    }
}

void interface::wait()
{
    glFlush();
    glFinish();
}
