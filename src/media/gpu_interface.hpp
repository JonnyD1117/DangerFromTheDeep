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

#pragma once

#include "area.hpp"
#include "color.hpp"
#include "error.hpp"
#include "singleton.hpp"
#include "vector4.hpp"

#include <array>
#include <cstring>
#include <vector>

#ifdef interface // needed because of windows crap
#undef interface
#endif

/*
Modern rendering interface:

GPUs and their interface transformed to generic computing machines these days.

Since OpenGL3+ most hardcoded graphic stuff has been removed from the interface.
To render anything to screen one can either define global variables for
vertex or fragment shaders (called uniforms in OpenGL) or per vertex data given
as vertex attributes or define samplers (texture data) as input.
Uniforms can and should be grouped in buffers (UBO - uniform buffer objects).
Vertex data is stored in buffers on the GPU (VBO - vertex buffer objects).

Every vertex attribute is associated to one vertex buffer object (VBO) holding
the data. Data is not stored interleaved but with one VBO per attribute
(faster). Setup of a set of VBOs is stored as vertex attribute array object
(VAO). Together with shaders that can be set as current shader and indices for
primitives this is all one needs to access the gpu or render anything. Primitive
index data is stored in VBOs as well. Thus even standard attributes like
position, normal, color or texcoords are just vertex attributes. This means the
GPU interface is agnostic of special data like normals, colors etc., it is all
up to the user to give the data meaning.

This interface can later switched more easily to Vulkan or something else.
For DfTD recent OpenGL is sufficient, this interface uses OpenGL 4.5.

All the data about location of uniforms and vertex attributes should be defined
from C++ side and not requested from GPU side as it was done in old code. Thus
we can avoid to request data location from the GPU but instead define where it
is stored, thus user code is much easier to write and maintain.

All the GL specific stuff like VBOs and their organization should be hidden from
the user to make user code as easy as possible. User defines per vertex data as
gpu::vertex_buffer objects. To render anything the user sets up a
gpu::render_context and tells it which vertex_buffer goes to which location
(number of vertex attribute). We can even render something without any vertex
buffer attached. The user defines for the render context which shaders to use as
program and the uniform values to be used (also specific location). Finally
(s)he defines which textures are bound to what location and what samplers.
Samplers are descriptions how textures are filtered and bound to a texture unit.

Uniform data is stored in struct-like data structures.
To define unions give a struct as input.
(structs for uniforms need to follow certain alignment rules (std140):
vector3 is aligned to vector4, vector2 is aligned to vector2, single values stay
single values.) Textures are a special kind of uniforms and are set at once for
a shader.

It would be best to define one uniform buffer object that holds the common scene
data, as projection matrix, modelview matrix, light data, fog data etc.

This is also up to the user, so GPU interface doesn't need to know about
modelview matrices. We provide gpu_helper file for these.

User creates instance of interface, puts data in vertex_buffer, uniform_buffer
and optional in index_buffer, creates a program and optionally texture, combines
them all in a render_context to bring anything to screen. frame_buffer class is
used for rendering to offscreen buffers, like render to texture. samplers define
how texture unit works.

Note that OpenGL 4.5 was released august 2014, so demanding GL 4.5 is not really
hard, as it is already many years old.

fullscreen quad via triangle:
float x = -1.0 + float((gl_VertexID & 1) << 2);
float y = -1.0 + float((gl_VertexID & 2) << 1);
float tx = float((gl_VertexID & 1) << 1);
float ty = float((gl_VertexID & 2));
VertexID 0,1,2 gives
X: -1, 3, -1
Y: -1, -1, 3
TX: 0, 2, 0
TY: 0, 0, 2
perfect!

Open topics:
============

 add flags for:
    allow compressed textures
    request used texture memory
    is-nvidia-card
    allow shader optimization

 maybe offer indirect rendering (many draw calls in a buffer) as well

 GL4 related stuff
    update only parts of vertex/indexbuffer (with offset)
    explore buffer streaming
    introduce more blend functions
    Multisampling (anti aliasing)
    glReadPixels is still available in GL4+, can be used for screenshots, but
test SDL screenshot code first Direct State Access possible but doesn't help
much and would need GL 4.5 (which we need anyway).

 General ideas about computation and rendering:
 - quick and easy sum of values: use floating point texture for values and
compute mipmaps with one call to sum up all values (comparison check with least
sum of squares method).

*/

/// Everything that is GPU related goes to this namespace
namespace gpu
{

/// kind of data stored in buffers
enum class data_type
{
    u8,
    u16,
    u32,
    u64,
    i8,
    i16,
    i32,
    i64,
    f32,
    f64,
    f16,
    ubyte, ///< special data type for normalized u8 values in textures
    number
};

/// which side of faces to render
enum class face_render_side
{
    front,
    back,
    both,
    none,
    number
};

/// blending function
enum class blend_func_type
{
    standard,
    srcalpha,
    one_srccolor,
    number
};

/// type of primitive
enum class primitive_type
{
    points,         ///< Plain points to render
    lines,          ///< 2D/3D lines
    line_strip,     ///< Strip of lines
    line_loop,      ///< Closed strip of lines
    triangles,      ///< Triangles with 3 indices each, counter clockwise corner
                    ///< definition
    triangle_strip, ///< Triangle strip (3 + N vertices for N triangles)
    triangle_fan,   ///< Triangle fan (3 + N vertices for N triangles)
    number
};

/// deduce data type from argument type
template<typename T>
data_type to_data_type(T); // intentionally no definition, so linker fails! We
                           // need to provide explicit data type definitions!
template<>
inline data_type to_data_type(char)
{
    return data_type::ubyte;
} // use char as 8bit float representation
template<>
inline data_type to_data_type(uint8_t)
{
    return data_type::u8;
}
template<>
inline data_type to_data_type(uint16_t)
{
    return data_type::u16;
}
template<>
inline data_type to_data_type(uint32_t)
{
    return data_type::u32;
}
template<>
inline data_type to_data_type(uint64_t)
{
    return data_type::u64;
}
template<>
inline data_type to_data_type(int8_t)
{
    return data_type::i8;
}
template<>
inline data_type to_data_type(int16_t)
{
    return data_type::i16;
}
template<>
inline data_type to_data_type(int32_t)
{
    return data_type::i32;
}
template<>
inline data_type to_data_type(int64_t)
{
    return data_type::i64;
}
template<>
inline data_type to_data_type(float)
{
    return data_type::f32;
}
template<>
inline data_type to_data_type(double)
{
    return data_type::f64;
}
template<>
inline data_type to_data_type(color)
{
    return data_type::ubyte;
}
template<>
inline data_type to_data_type(colorf)
{
    return data_type::f32;
}
template<typename T>
inline data_type to_data_type(vector2t<T>)
{
    return to_data_type(T());
}
template<typename T>
inline data_type to_data_type(vector3t<T>)
{
    return to_data_type(T());
}
template<typename T, size_t sz>
inline data_type to_data_type(std::array<T, sz>)
{
    return to_data_type(T());
}
/// deduce data count from argument type
template<typename T>
inline int to_data_count(T)
{
    return 1;
}
template<typename T>
inline int to_data_count(vector2t<T>)
{
    return 2;
}
template<typename T>
inline int to_data_count(vector3t<T>)
{
    return 3;
}
template<typename T>
inline int to_data_count(vector4t<T>)
{
    return 4;
}
template<typename T, size_t sz>
inline int to_data_count(std::array<T, sz>)
{
    return int(sz);
}
template<>
inline int to_data_count(color)
{
    return 4;
}
template<>
inline int to_data_count(colorf)
{
    return 4;
}
/// deduce data type from argument type for texture data
template<typename T>
data_type to_tex_data_type(T); // intentionally no definition, so linker fails! We
                               // need to provide explicit data type definitions!
template<>
inline data_type to_tex_data_type(uint8_t)
{
    return data_type::ubyte;
}
template<>
inline data_type to_tex_data_type(float)
{
    return data_type::f32;
}
template<>
inline data_type to_tex_data_type(color)
{
    return data_type::ubyte;
}
template<>
inline data_type to_tex_data_type(colorf)
{
    return data_type::f32;
}

/// a base class for any object on GPU that has an ID. Can not be copied nor
/// assigned, only moved.
class object
{
  public:
    /// Get the ID on the GPU.
    unsigned get_gpu_id() const { return gpu_id; }

    /// Request if object is empty
    bool empty() const { return gpu_id == 0; }

  protected:
    object() = default;
    object(object&& source)
        : gpu_id(source.gpu_id)
    {
        source.gpu_id = 0;
    }
    object& operator=(object&& source)
    {
        unsigned id   = source.gpu_id;
        source.gpu_id = 0;
        gpu_id        = id;
        return *this;
    }
    mutable unsigned gpu_id{0}; ///< ID on the gpu. Mutable because bind() is
                                ///< const and can create it on demand.

  private:
    object(const object&)            = delete;
    object& operator=(const object&) = delete;
};

/// a generic GPU buffer handler class
class buffer : public object
{
    friend class render_context;
    friend class interface;

  public:
    /// destruct the buffer
    ~buffer();

    /// kind of buffer
    enum class buffer_type
    {
        vertex_attributes, ///< VBO
        indices,           ///< VBO with indices
        uniform,           ///< UBO
        shader_storage,    ///< SSBOs
        number
    };

    /// access type
    enum class access_type
    {
        read_only,  ///< only read data
        write_only, ///< only write data (used mostly)
        read_write, ///< random access
        number
    };

    /// usage type
    enum class usage_type
    {
        stream_draw,
        stream_read,
        stream_copy,
        static_draw,
        static_read,
        static_copy,
        dynamic_draw,
        dynamic_read,
        dynamic_copy,
        number
    };

  protected:
    /// construct the buffer
    buffer(buffer_type type_);

    /// Move the buffer
    buffer(buffer&& source);

    /// Move assign the buffer
    buffer& operator=(buffer&& source);

    /// bind the buffer
    void bind() const;

    /// unbind the buffer
    void unbind() const;

    /// map the buffer for access
    void* map(access_type ac = access_type::write_only);

    /// unmap the buffer
    void unmap();

    /// initialize buffer data, buffer is always set to requested size
    void init_buffer(unsigned byte_size, const void* data, usage_type usage);

    /// update buffer data, enlarges/creates buffer if needed
    void update_buffer(unsigned byte_size, const void* data, usage_type usage);

    /// update buffer data, if data does not fit, an exception is thrown
    void update_buffer_data(unsigned byte_size, const void* data);

    bool mapped{false};                    ///< Is buffer mapped?
    buffer_type type{buffer_type::number}; ///< Type of buffer
    std::size_t buffer_size{0};            ///< Size of buffer in bytes

    /// Bind buffer inside scope
    class scoped_bind
    {
      public:
        scoped_bind(buffer& buf)
            : mybuffer(buf)
        {
            mybuffer.bind();
        }
        ~scoped_bind() { mybuffer.unbind(); }

      private:
        buffer& mybuffer;

      public:
        scoped_bind(const scoped_bind&)            = delete;
        scoped_bind(scoped_bind&&)                 = delete;
        scoped_bind& operator=(const scoped_bind&) = delete;
        scoped_bind& operator=(scoped_bind&&)      = delete;
    };

    /// Map buffer inside scope
    class scoped_map
    {
      public:
        scoped_map(buffer& buf)
            : mybuffer(buf)
        {
            data = mybuffer.map();
        }
        ~scoped_map() { mybuffer.unmap(); }
        void* get_data() const { return data; }

      private:
        buffer& mybuffer;
        void* data{};

      public:
        scoped_map(const scoped_bind&)           = delete;
        scoped_map(scoped_bind&&)                = delete;
        scoped_map& operator=(const scoped_map&) = delete;
        scoped_map& operator=(scoped_map&&)      = delete;
    };

  private:
    buffer(const buffer&)            = delete;
    buffer& operator=(const buffer&) = delete;
};

/// a special buffer for vertex data
class vertex_buffer : public buffer
{
  public:
    /// access handle class
    template<typename T>
    class access
    {
        friend class vertex_buffer;

      public:
        ~access() { vbo.unmap(); }
        T& operator[](std::size_t index) { return data[index]; }
        std::size_t size() const { return data_size; }

      protected:
        access(vertex_buffer& vbo_, access_type ac)
            : vbo(vbo_)
            , data((T*) vbo.map(ac))
            , data_size(vbo.get_nr_elements())
        {
            // check that data types and count match
            if (to_data_type(T()) != vbo_.get_data_type())
                THROW(error, "access with invalid data type");
            if (to_data_count(T()) != int(vbo_.get_data_count()))
                THROW(error, "access with invalid data count");
        }
        vertex_buffer& vbo;
        T* data;
        std::size_t data_size;

      private:
        access();
    };

    /// create buffer. Tell the handler if you wish to store indices or other
    /// data.
    vertex_buffer();

    /// Move-Constructor
    vertex_buffer(vertex_buffer&& vb);

    /// Move-Assignment
    vertex_buffer& operator=(vertex_buffer&& vb);

    /** call to initialize and set data
     * @param nr_of_elements_ number of elements in the data
     * @param data the data to use
     * @param usage usage type of buffer
     */
    template<typename T>
    void init(unsigned nr_of_elements_, const T* data = nullptr, usage_type usage = usage_type::static_draw)
    {
        nr_of_elements   = nr_of_elements_;
        data_count       = to_data_count(T());
        buffer_data_type = to_data_type(T());
        init_buffer(sizeof(T) * nr_of_elements, data, usage);
    }

    /** call to initialize and set data
     * @param data the data to use
     * @param usage usage type of buffer
     */
    template<typename T>
    void init(const std::vector<T>& data, usage_type usage = usage_type::static_draw)
    {
        if (data.empty())
            THROW(error, "buffer init with empty data");
        nr_of_elements   = unsigned(data.size());
        data_count       = to_data_count(T());
        buffer_data_type = to_data_type(T());
        init_buffer(sizeof(T) * nr_of_elements, &data[0], usage);
    }

    /** call to update data (only growing)
     * @param data the data to use
     * @param usage usage type of buffer
     */
    template<typename T>
    void update(const std::vector<T>& data, usage_type usage = usage_type::static_draw)
    {
        if (data.empty())
            THROW(error, "buffer update with empty data");
        nr_of_elements   = unsigned(data.size());
        data_count       = to_data_count(T());
        buffer_data_type = to_data_type(T());
        init_buffer(sizeof(T) * nr_of_elements, &data[0], usage);
    }

    /** call to update data - size must fit in buffer!
     * @param data the data to use
     */
    template<typename T>
    void update_data(const std::vector<T>& data)
    {
        if (data.empty())
            THROW(error, "buffer update with empty data");
        nr_of_elements = unsigned(data.size()); // note that modifying these values may be
                                                // valid but mostly error prone...
        data_count       = to_data_count(T());
        buffer_data_type = to_data_type(T());
        update_buffer_data(sizeof(T) * nr_of_elements, &data[0]);
    }

    /// map VBO and access data easily
    template<typename T>
    access<T> access_data(access_type ac = access_type::write_only)
    {
        return access<T>(*this, ac);
    }

    /// copy data to buffer
    template<typename T>
    void copy_data_from(const std::vector<T>& vec)
    {
        if (vec.empty())
            THROW(error, "buffer copy_data_from with empty data");
        auto va = access_data<T>(access_type::write_only);
        // memcpy instead of plain copy (potentially faster)
        std::memcpy(&va[0], &vec[0], sizeof(T) * vec.size());
        // std::size_t ctr = 0;
        // for (const auto& a : vec) {
        //	va[ctr++] = a;
        //}
    }

    /// get number of elements in buffer
    auto get_nr_elements() const { return nr_of_elements; }

    /// get number of data per element
    auto get_data_count() const { return data_count; }

    /// get data type
    auto get_data_type() const { return buffer_data_type; }

  protected:
    unsigned nr_of_elements{0};                    ///< number of elements in the buffer
    unsigned data_count{0};                        ///< number of data values per element
    data_type buffer_data_type{data_type::number}; ///< type of data stored

  private:
    vertex_buffer(const vertex_buffer&)            = delete;
    vertex_buffer& operator=(const vertex_buffer&) = delete;
};

/// a special buffer for index data
class index_buffer : public vertex_buffer
{
  public:
    /// Constructor.
    index_buffer();

    /// Move-Constructor
    index_buffer(index_buffer&& ib)
        : vertex_buffer(std::move(ib))
    {
    }

    /// Move-Assignment
    index_buffer& operator=(index_buffer&& ib)
    {
        vertex_buffer::operator=(std::move(ib));
        return *this;
    }

  private:
    index_buffer(const index_buffer&)            = delete;
    index_buffer& operator=(const index_buffer&) = delete;
};

/// class to describe an uniform buffer.
class uniform_buffer : public buffer
{
  public:
    /// access handle class
    template<class U>
    class access
    {
        friend class uniform_buffer;

      public:
        ~access() { ubo.unmap(); }
        /// access data like struct members
        U* operator->() { return data; }
        /// access data array like - use with care! uniform data must be a
        /// struct with just simple array
        U& operator[](std::size_t index) { return data[index]; }

      protected:
        access(uniform_buffer& ubo_, buffer::access_type ac)
            : ubo(ubo_)
            , data((U*) ubo.map(ac))
        {
            if (sizeof(U) != ubo.buffer_size)
                THROW(error, "uniform buffer mapped with wrong buffer type");
        }
        uniform_buffer& ubo;
        U* data;

      private:
        access();
    };

    /// Constructor.
    uniform_buffer();

    /// Move the buffer
    uniform_buffer(uniform_buffer&& source);

    /// Move assign the buffer
    uniform_buffer& operator=(uniform_buffer&& source);

    /// Initialize for certain structure
    template<class U>
    void init(usage_type usage, const U* data = nullptr)
    {
        init_buffer(sizeof(U), data, usage);
    }

    /// Initialize for certain structure - note that U can be a pointer to a
    /// type, be careful! - so don't give pointers here!
    template<class U>
    void init(usage_type usage, const U& data = nullptr)
    {
        init_buffer(sizeof(U), &data, usage);
    }

    /// Initialize for vector auf simple data
    template<class U>
    void init(usage_type usage, const std::vector<U>& data)
    {
        init_buffer(sizeof(U) * unsigned(data.size()), &data[0], usage);
    }

    /// Initialize for array auf simple data
    template<class U, std::size_t sz>
    void init(usage_type usage, const std::array<U, sz>& data)
    {
        init_buffer(sizeof(U) * unsigned(sz), &data[0], usage);
    }

    /// Initialize for array auf simple data
    template<class U>
    void init(usage_type usage, unsigned nr_of_elements, const U* data = nullptr)
    {
        init_buffer(sizeof(U) * nr_of_elements, data, usage);
    }

    /// Update for array auf simple data (only growing)
    template<class U>
    void update(usage_type usage, const std::vector<U>& data)
    {
        update_buffer(sizeof(U) * unsigned(data.size()), &data[0], usage);
    }

    /// Update data of buffer - buffer must already be initialized!
    template<class U>
    void update_data(const U& data)
    {
        update_buffer_data(sizeof(U), &data);
    }

    /** call to update data - size must fit in buffer!
     * @param data the data to use
     */
    template<class U>
    void update_data(const std::vector<U>& data)
    {
        if (data.empty())
            THROW(error, "buffer update with empty data");
        update_buffer_data(sizeof(U) * unsigned(data.size()), &data[0]);
    }

    /** call to update data - size must fit in buffer!
     * @param data the data to use
     */
    template<class U, std::size_t sz>
    void update_data(const std::array<U, sz>& data)
    {
        if (data.empty())
            THROW(error, "buffer update with empty data");
        update_buffer_data(sizeof(U) * unsigned(sz), &data[0]);
    }

    /// map UBO and access data easily
    template<class U>
    access<U> access_data(access_type ac = access_type::write_only)
    {
        return access<U>(*this, ac);
    }
};

/// class to describe an uniform buffer.
class shader_storage_buffer : public uniform_buffer
{
  public:
    /// Constructor.
    shader_storage_buffer();

    /// Move the buffer
    shader_storage_buffer(shader_storage_buffer&& source);

    /// Move assign the buffer
    shader_storage_buffer& operator=(shader_storage_buffer&& source);
};

/// Handles a GPU based texture with loading.
class texture : public object
{
  public:
    /// Create empty texture object
    texture() = default;

    /** Create texture from file.
     * @param filename File name of image to load
     * @param dt pixel format per channel
     * @param use_mipmap create mipmaps?
     * @param use_compression use compression for internal data storage
     * @param bump_height create normal map from bump map if file is greyvalue
     * with that bump height
     */
    texture(
        const std::string& filename,
        data_type dt         = data_type::ubyte,
        bool use_mipmap      = false,
        bool use_compression = false,
        float bump_height    = -1.f);

    /** Create texture from raw pixel data with user defined data type.
     * @param pixels pixel data
     * @param w width
     * @param h height
     * @param nc number of color channels
     * @param use_mipmap create mipmaps?
     * @param use_compression use compression for internal data storage
     * @param dt pixel format per channel
     */
    texture(
        const std::vector<uint8_t>& pixels,
        int w,
        int h,
        int nc,
        bool use_mipmap      = false,
        bool use_compression = false,
        data_type dt         = data_type::ubyte);

    /** Create texture from raw pixel data with data type taken from template.
     * double will get converted to float!
     * @param pixels pixel data
     * @param w width
     * @param h height
     * @param nc number of color channels
     * @param use_mipmap create mipmaps?
     * @param use_compression use compression for internal data storage
     * @param use_half_float internally use half float texture
     */
    template<typename T>
    texture(
        const std::vector<T>& pixels,
        int w,
        int h,
        int nc,
        bool use_mipmap      = false,
        bool use_compression = false,
        bool use_half_float  = false)
        : gpu_format(0)
        , width(w)
        , height(h)
        , nr_of_channels(nc)
        , has_mipmap(use_mipmap)
    {
        data_type dt = to_data_type(T());
        if (dt == data_type::f64) {
            std::vector<float> tmp(pixels.size());
            for (std::size_t i = 0; i < pixels.size(); ++i) {
                tmp[i] = float(pixels[i]);
            }
            dt = use_half_float ? data_type::f16 : data_type::f32;
            init(&tmp[0], dt, use_compression);
        } else {
            if (dt == data_type::f32 && use_half_float)
                dt = data_type::f16;
            init(&pixels[0], dt, use_compression);
        }
    }

    /** Create empty texture
     * @param w width
     * @param h height
     * @param nc number of color channels
     * @param dt pixel format per channel
     * @param use_mipmap create mipmaps?
     */
    texture(int w, int h, int nc, data_type dt = data_type::ubyte, bool use_mipmap = false);

    /** Move to new object
     * @param source source object
     */
    texture(texture&& source);

    /// Move assign the texture
    texture& operator=(texture&& source);

    /// destructor
    ~texture();

    /// Clean up texture
    void reset();

    /// Get texture width
    auto get_width() const { return width; }

    /// Get texture height
    auto get_height() const { return height; }

    /// Get texture width/height as size value
    auto get_size() const { return vector2i(width, height); }

    /// Get texture number of color channels
    auto get_nr_of_channels() const { return nr_of_channels; }

    /// Get GPU format (for internal use!)
    auto get_gpu_format() const { return gpu_format; }

    /// Replace texture data with new values, optionally update mipmap if
    /// texture has one
    void set_data_generic(const void* pixels, int count, data_type dt, int mipmap_level = 0, bool update_mipmap = true);

    /// Replace texture data with new values, optionally update mipmap if
    /// texture has one
    template<typename T>
    void set_data(const std::vector<T>& pixels, int mipmap_level = 0, bool update_mipmap = true)
    {
        set_data_generic(
            &pixels[0], int(pixels.size()) * to_data_count(T()), to_data_type(T()), mipmap_level, update_mipmap);
    }

    /// change sub-area of texture from memory values
    /// @param ar area to change
    /// @param pixels pixel data
    /// @param data_offset offset index in data
    /// @param stride line width in pixels of data (not bytes!)
    /// @param update_mipmap update mipmap if texture has one?
    template<typename T>
    void sub_image(
        const area& ar,
        const std::vector<T>& pixels,
        int data_offset    = 0,
        int stride         = 0,
        bool update_mipmap = false)
    {
        data_type dt = to_tex_data_type(T());
        sub_image(ar, dt, &pixels[data_offset], stride, update_mipmap);
    }

    /// Update mipmap if texture has one
    void update_mipmap();

    /// Swap two textures
    void swap(texture& other);

  protected:
    int gpu_format{0};       ///< Data format used in GPU internally.
    int width{0};            ///< Width in pixels.
    int height{0};           ///< Height in pixels.
    int nr_of_channels{0};   ///< Number of color channels per pixel (1-4).
    bool has_mipmap{false};  ///< Texture has mipmaps.
    uint32_t used_memory{0}; ///< Approximate value of video memory used.

    /// copy data to GPU, set parameters, only used internally
    void init(const void* data, data_type dt, bool use_compression, const std::string* name = nullptr);

    /// copy data to GPU, set parameters, only used internally - custom mipmap
    /// version
    void
    init(const std::vector<const void*> data, data_type dt, bool use_compression, const std::string* name = nullptr);

    /// Update parts of texture, only used internally
    void sub_image(const area& ar, data_type dt, const void* pixels, int stride, bool update_mipmap = false);
};

/// Handles a GPU based texture array.
class texture_array : public object
{
  public:
    /// Create empty texture array object
    texture_array() = default;

    /** Create texture array from files.
     * @param filenames File names of images to load
     * @param dt pixel format per channel
     * @param use_mipmap create mipmaps?
     */
    texture_array(const std::vector<std::string>& filenames, data_type dt = data_type::ubyte, bool use_mipmap = false);

    /** Create empty texture array
     * @param w width
     * @param h height
     * @param l number of layers
     * @param nc number of color channels
     * @param dt pixel format per channel
     * @param use_mipmap create mipmaps?
     */
    texture_array(int w, int h, int l, int nc, data_type dt = data_type::ubyte, bool use_mipmap = false);

    /** Move to new object
     * @param source source object
     */
    texture_array(texture_array&& source);

    /// Move assign the texture array
    texture_array& operator=(texture_array&& source);

    /// destructor
    ~texture_array();

    /// Clean up texture_array
    void reset();

    /// Get texture width
    auto get_width() const { return width; }

    /// Get texture height
    auto get_height() const { return height; }

    /// Get texture width/height as size value
    auto get_size() const { return vector2i(width, height); }

    /// Get number of layers
    auto get_nr_of_layers() const { return nr_of_layers; }

    /// Get texture number of color channels
    auto get_nr_of_channels() const { return nr_of_channels; }

    /// Get GPU format (for internal use!)
    auto get_gpu_format() const { return gpu_format; }

    /// Replace texture data with new values, optionally update mipmap if
    /// texture has one
    void set_data_generic(
        int layer,
        const void* pixels,
        int count,
        data_type dt,
        int mipmap_level   = 0,
        bool update_mipmap = true);

    /// Replace texture data with new values, optionally update mipmap if
    /// texture has one
    template<typename T>
    void set_data(int layer, const std::vector<T>& pixels, int mipmap_level = 0, bool update_mipmap = true)
    {
        set_data_generic(
            layer, &pixels[0], int(pixels.size()) * to_data_count(T()), to_data_type(T()), mipmap_level, update_mipmap);
    }

    /// change sub-area of texture from memory values
    /// @param layer layer to change
    /// @param ar area to change
    /// @param pixels pixel data
    /// @param data_offset offset index in data
    /// @param stride line width in pixels of data (not bytes!)
    /// @param update_mipmap update mipmap if texture has one
    template<typename T>
    void sub_image(
        int layer,
        const area& ar,
        const std::vector<T>& pixels,
        int data_offset    = 0,
        int stride         = 0,
        bool update_mipmap = false)
    {
        data_type dt = to_tex_data_type(T());
        sub_image(layer, ar, dt, &pixels[data_offset], stride, update_mipmap);
    }

    /// Update mipmap if texture array has one
    void update_mipmap();

    /// Swap two texture arrays
    void swap(texture_array& other);

  protected:
    int gpu_format{0};       ///< Data format used in GPU internally.
    int width{0};            ///< Width in pixels.
    int height{0};           ///< Height in pixels.
    int nr_of_layers{0};     ///< Number of layers in array
    int nr_of_channels{0};   ///< Number of color channels per pixel (1-4).
    bool has_mipmap{false};  ///< Texture has mipmaps.
    uint32_t used_memory{0}; ///< Approximate value of video memory used.

    /// Create space for texture on GPU
    void create_space(data_type dt, const std::string* name = nullptr);

    /// copy data to GPU, set parameters, only used internally
    void init(const void** data, int nr_of_layers, data_type dt, const std::string* name = nullptr);

    /// Update parts of texture, only used internally
    void sub_image(int layer, const area& ar, data_type dt, const void* pixels, int stride, bool update_mipmap = false);
};

/// Handles a GPU based 3D texture.
class texture_3D : public object
{
  public:
    /// Create empty texture 3D object
    texture_3D() = default;

    /** Create empty texture 3D
     * @param w width
     * @param h height
     * @param d depth
     * @param nc number of color channels
     * @param dt pixel format per channel
     * @param use_mipmap create mipmaps?
     */
    texture_3D(int w, int h, int d, int nc, data_type dt = data_type::ubyte, bool use_mipmap = false);

    /** Move to new object
     * @param source source object
     */
    texture_3D(texture_3D&& source);

    /// Move assign the texture array
    texture_3D& operator=(texture_3D&& source);

    /// destructor
    ~texture_3D();

    /// Clean up texture_array
    void reset();

    /// Get texture width
    auto get_width() const { return width; }

    /// Get texture height
    auto get_height() const { return height; }

    /// Get texture depth
    auto get_depth() const { return depth; }

    /// Get texture width/height as size value
    auto get_size() const { return vector3i(width, height, depth); }

    /// Get texture number of color channels
    auto get_nr_of_channels() const { return nr_of_channels; }

    /// Get GPU format (for internal use!)
    auto get_gpu_format() const { return gpu_format; }

    /// Replace texture data with new values, optionally update mipmap if
    /// texture has one
    void set_data_generic(
        int z,
        const void* pixels,
        int count,
        data_type dt,
        int mipmap_level   = 0,
        bool update_mipmap = true);

    /// Replace texture data with new values, optionally update mipmap if
    /// texture has one
    template<typename T>
    void set_data(int z, const std::vector<T>& pixels, int mipmap_level = 0, bool update_mipmap = true)
    {
        set_data_generic(
            z, &pixels[0], int(pixels.size()) * to_data_count(T()), to_data_type(T()), mipmap_level, update_mipmap);
    }

    /// change sub-area of texture from memory values
    /// @param layer layer to change
    /// @param ar area to change
    /// @param pixels pixel data
    /// @param data_offset offset index in data
    /// @param stride line width in pixels of data (not bytes!)
    /// @param update_mipmap update mipmap if texture has one
    template<typename T>
    void sub_image(
        int z,
        const area& ar,
        const std::vector<T>& pixels,
        int data_offset    = 0,
        int stride         = 0,
        bool update_mipmap = false)
    {
        data_type dt = to_tex_data_type(T());
        sub_image(z, ar, dt, &pixels[data_offset], stride, update_mipmap);
    }

    /// Update mipmap if texture array has one
    void update_mipmap();

    /// Swap two texture arrays
    void swap(texture_3D& other);

  protected:
    int gpu_format{0};       ///< Data format used in GPU internally.
    int width{0};            ///< Width in pixels.
    int height{0};           ///< Height in pixels.
    int depth{0};            ///< Depth in pixels.
    int nr_of_channels{0};   ///< Number of color channels per pixel (1-4).
    bool has_mipmap{false};  ///< Texture has mipmaps.
    uint32_t used_memory{0}; ///< Approximate value of video memory used.

    /// Update parts of texture, only used internally
    void sub_image(int z, const area& ar, data_type dt, const void* pixels, int stride, bool update_mipmap = false);
};

/// Handles a GPU based cube map texture.
class texture_cube : public object
{
  public:
    /// Create empty texture array object
    texture_cube() = default;

    /** Create texture array from files.
     * @param filenames File names of images to load
     * @param dt pixel format per channel
     * @param use_mipmap create mipmaps?
     */
    texture_cube(const std::array<std::string, 6>& filenames, data_type dt = data_type::ubyte, bool use_mipmap = false);

    /** Create empty texture array
     * @param w width
     * @param h height
     * @param nc number of color channels
     * @param dt pixel format per channel
     * @param use_mipmap create mipmaps?
     */
    texture_cube(int w, int h, int nc, data_type dt = data_type::ubyte, bool use_mipmap = false);

    /** Move to new object
     * @param source source object
     */
    texture_cube(texture_cube&& source);

    /// Move assign the texture array
    texture_cube& operator=(texture_cube&& source);

    /// destructor
    ~texture_cube();

    /// Clean up texture cubemap
    void reset();

    /// Get texture width
    auto get_width() const { return width; }

    /// Get texture height
    auto get_height() const { return height; }

    /// Get texture width/height as size value
    auto get_size() const { return vector2i(width, height); }

    /// Get texture number of color channels
    auto get_nr_of_channels() const { return nr_of_channels; }

    /// Get GPU format (for internal use!)
    auto get_gpu_format() const { return gpu_format; }

    /// Replace texture data with new values, optionally update mipmap if
    /// texture has one
    void set_data_generic(
        int cube_side,
        const void* pixels,
        int count,
        data_type dt,
        int mipmap_level   = 0,
        bool update_mipmap = true);

    /// Replace texture data with new values, optionally update mipmap if
    /// texture has one
    template<typename T>
    void set_data(int cube_side, const std::vector<T>& pixels, int mipmap_level = 0, bool update_mipmap = true)
    {
        set_data_generic(
            cube_side,
            &pixels[0],
            int(pixels.size()) * to_data_count(T()),
            to_data_type(T()),
            mipmap_level,
            update_mipmap);
    }

    /// change sub-area of texture from memory values
    /// @param cube_side which side of the cube to change (0-5)
    /// @param ar area to change
    /// @param pixels pixel data
    /// @param data_offset offset index in data
    /// @param stride line width in pixels of data (not bytes!)
    /// @param update_mipmap update mipmap if texture has one
    template<typename T>
    void sub_image(
        int cube_side,
        const area& ar,
        const std::vector<T>& pixels,
        int data_offset    = 0,
        int stride         = 0,
        bool update_mipmap = false)
    {
        data_type dt = to_tex_data_type(T());
        sub_image(cube_side, ar, dt, &pixels[data_offset], stride, update_mipmap);
    }

    /// Update mipmap if texture array has one
    void update_mipmap();

    /// Swap two texture arrays
    void swap(texture_cube& other);

  protected:
    int gpu_format{0};       ///< Data format used in GPU internally.
    int width{0};            ///< Width in pixels.
    int height{0};           ///< Height in pixels.
    int nr_of_channels{0};   ///< Number of color channels per pixel (1-4).
    bool has_mipmap{false};  ///< Texture has mipmaps.
    uint32_t used_memory{0}; ///< Approximate value of video memory used.

    /// copy data to GPU, set parameters, only used internally
    void init(
        const std::array<const void*, 6>& data,
        data_type dt,
        const std::string* name = nullptr,
        bool use_compression    = false);

    /// Update parts of texture, only used internally
    void
    sub_image(int cube_side, const area& ar, data_type dt, const void* pixels, int stride, bool update_mipmap = false);
};

/// How a texture unit is used, e.g. a texture is sampled
class sampler : public object
{
  public:
    /// Possible types
    enum class type
    {
        nearest_repeat,        ///< nearest without mipmap, repeating
        nearest_clamp,         ///< nearest without mipmap, clamping
        bilinear_repeat,       ///< linear without mipmap, repeating
        bilinear_clamp,        ///< linear without mipmap, clamping
        trilinear_repeat,      ///< trilinear with mipmap, repeating
        trilinear_clamp,       ///< trilinear with mipmap, clamping
        nearest_mipmap_repeat, ///< bilinear with nearest mipmap, repeating
        nearest_mipmap_clamp,  ///< bilinear with nearest mipmap, clamping
        number
    };

    /** Create sampler
     * @param sampler_type Kind of sampler
     * @param anisotropic_level level to use for anisotropic filtering
     */
    sampler(type sampler_type, float anisotropic_level = 0.f);

    /// Move constructor
    sampler(sampler&& source);

    /// Move assignment.
    sampler& operator=(sampler&& source);

    /// Destructor
    ~sampler();

    /// Bind sampler to unit
    void bind_to_unit(unsigned tex_unit) const;
};

/// this class handles a GPU Frame Buffer Object
class frame_buffer : public object
{
  public:
    /** Create empty framebuffer object. Only used to store it in vectors
     */
    frame_buffer() = default;

    /** Create buffer object.
     * @param tex texture to use as target
     * @param withdepthbuffer add depth information buffer?
     */
    frame_buffer(texture&& tex, bool withdepthbuffer = false);

    /** Create buffer object with existing texture.
     * @param tex texture to use as target
     * @param level mipmap level of texture to use as target
     * @param withdepthbuffer add depth information buffer?
     */
    frame_buffer(const texture* tex, int level, bool withdepthbuffer = false);

    /// Move constructor.
    frame_buffer(frame_buffer&& source);

    /// Move assignment.
    frame_buffer& operator=(frame_buffer&& source);

    /// free buffer object
    ~frame_buffer();

    /// bind buffer and set up rendering
    void bind() const;

    /// unbind buffer
    void unbind() const;

    /// request the texture
    const texture& get_texture() const { return my_tex; }

    /// Bind buffer inside scope
    class scoped_bind
    {
      public:
        scoped_bind(const frame_buffer& buf)
            : mybuffer(buf)
        {
            mybuffer.bind();
        }
        ~scoped_bind() { mybuffer.unbind(); }

      private:
        const frame_buffer& mybuffer;

      public:
        scoped_bind(const scoped_bind&)            = delete;
        scoped_bind(scoped_bind&&)                 = delete;
        scoped_bind& operator=(const scoped_bind&) = delete;
        scoped_bind& operator=(scoped_bind&&)      = delete;
    };

  protected:
    void create(bool withdepthbuffer);

    unsigned depthbuf_id{0};     ///< ID of the optional depth buffer on the GPU.
    texture my_tex;              ///< If texture is managed by frame_buffer it is stored here
    const texture* tex{nullptr}; ///< The texture where data goes to.
    int mipmap_level{0};         ///< The mipmap level of the texture to use as framebuffer.
    mutable bool bound{false};   ///< Is buffer bound? for extra error checks

    void destroy();
};

/// A Shader as part of a render program
///@note shaders can be deleted after they have been linked to a program.
class shader : public object
{
  public:
    /// type of shader (we don't use tesselation shaders here)
    enum class type
    {
        vertex,
        fragment,
        geometry,
        compute,
        number
    };

    /** Create a shader
     * @param filename name of file or code when immediate is true
     * @param stype shader type
     * @param immediate give code immediately not filename
     * @param defines defines for shader
     */
    shader(const std::string& filename, type stype, bool immediate, std::initializer_list<std::string> defines = {});

    /// move a ahder
    shader(shader&& source);

    /// move assign a ahder
    shader& operator=(shader&& source);

    /// destroy a shader
    ~shader();
};

/// this class handles a GPU shader program, that is a link unit of shaders.
class program : public object
{
  public:
    /// create program
    program() = default;

    /** Create program from files (generates vertex and fragment shader from
     * basic filename)
     * @param basefilename basic filename without extension
     * @param defines common defines
     */
    program(const std::string& basefilename, std::initializer_list<std::string> defines = {});

    /** Create program from compute shader and link it */
    program(const shader& computeshader);

    /** Create program from compute shader and link it */
    program(const shader& vertexshader, const shader& fragmentshader);

    /// move program
    program(program&& source);

    /// Move from other program
    program& operator=(program&& source);

    /// destroy program
    ~program();

    /// link program using the given shader
    void link(const shader& computeshader);

    /// link program using the shaders given
    void link(const shader& vertexshader, const shader& fragmentshader);

    /// Init from files
    void init(const std::string& basefilename, std::initializer_list<std::string> defines);

  protected:
    bool linked{false}; ///< Flag if the program is linked and useable
    program& operator=(const program&);
};

/// A render contex combining vertex buffers and a program
class render_context : public object
{
  public:
    /// Constructor
    render_context() = default;

    /// Destructor
    ~render_context();

    /// Move Constructor
    render_context(render_context&& r);

    /// Move assignment
    render_context& operator=(render_context&& r);

    /// Type for textures and samplers
    using textures_and_samplers = std::vector<std::pair<const texture*, sampler::type>>;

    /** Bind a vertex buffer to a location
     * @param location shader location that buffer data is bound to
     * @param vb vertex buffer with data
     * @param attrib_divisor advance attribute index every attrib_divisor
     * instances (0 for every vertex)
     */
    void add(unsigned location, const vertex_buffer& vb, unsigned attrib_divisor = 0);

    /** Bind and hold a vertex buffer to a location
     * @param location shader location that buffer data is bound to
     * @param vb vertex buffer with data
     * @param attrib_divisor advance attribute index every attrib_divisor
     * instances (0 for every vertex)
     */
    void add(unsigned location, vertex_buffer&& vb, unsigned attrib_divisor = 0);

    /** Bind a uniform buffer to a location
     * @remarks Location does not interfere with vertex buffer location, so both
     * can start at 0.
     * @param location shader location that buffer data is bound to
     * @param ub uniform buffer with data
     */
    void add(unsigned location, const uniform_buffer& ub);

    /** Bind a shader storage buffer to a location
     * @param location shader location that buffer data is bound to
     * @param sb shader storage buffer with data
     */
    void add(unsigned location, const shader_storage_buffer& sb);

    /** Bind a texture to a texture unit
     * @param unit texture unit that texture is bound to
     * @param tex texture
     * @param smp sampler type to use
     */
    void add(unsigned unit, const texture& tex, sampler::type smp);

    /** Bind multiple textures and samplers to texture units
     * @param textures_and_samplers textures/samplers bound to units 0-x
     */
    void add(const textures_and_samplers& textures_and_samplers);

    /** Bind a texture array to a texture unit
     * @param unit texture unit that texture array is bound to
     * @param tex_arr texture array
     * @param smp sampler type to use
     */
    void add(unsigned unit, const texture_array& tex_arr, sampler::type smp);

    /** Bind a texture 3D to a texture unit
     * @param unit texture unit that texture 3D is bound to
     * @param tex_3d texture 3D
     * @param smp sampler type to use
     */
    void add(unsigned unit, const texture_3D& tex_3d, sampler::type smp);

    /** Bind a cubemap texture to a texture unit
     * @param unit texture unit that texture array is bound to
     * @param tex_cube texture cubemap
     * @param smp sampler type to use
     */
    void add(unsigned unit, const texture_cube& tex_cube, sampler::type smp);

    /** Bind an optional index buffer to be used for rendering
     * @param idxbuf index buffer
     */
    void add(const index_buffer& idxbuf);

    /** Bind and hold an optional index buffer to be used for rendering
     * @param idxbuf index buffer
     */
    void add(index_buffer&& idxbuf);

    /// Bind a program
    void add(const program& prg);

    /// Bind and hold a program
    void add(program&& prg);

    /** Add information how to render it (optional). Renders all existing
     * indices beginning from index 0.
     * @param type Type of primitives to render
     * @param nr_of_indices number of indices to use
     */
    void add(primitive_type type, int nr_of_indices);

    /// enable or disable depth testing
    void enable_depth_test(bool enable);

    /// enable or disable writing to depth buffer
    void enable_depth_buffer_write(bool enable);

    /// enable or disable wire frame rendering
    void enable_wire_frame(bool enable);

    /// enable or disable use of primitive restart index
    void use_primitive_restart_index(bool enable, uint32_t index = uint32_t(-1));

    /// define which face sides should be rendered
    void set_face_render_side(face_render_side side);

    /// define blending function to be used
    void set_blend_function(blend_func_type bf = blend_func_type::standard);

    /// Set up for 2D Rendering
    void set_2D_drawing(bool enable);

    /// Initialize rendering
    void init();

    /// Use for rendering
    void use() const;

    /** Draw primitives with current render context (directly or with indices,
     * depending of index_buffer existence)
     * @param type Type of primitives to render
     * @param first_index first index to use
     * @param nr_of_indices number of indices to use
     */
    void draw_primitives(primitive_type type, int first_index, int nr_of_indices) const;

    /** Draw primitives with current render context (directly or with indices,
     * depending of index_buffer existence)
     * @param type Type of primitives to render
     * @param first_index first index to use
     * @param nr_of_indices number of indices to use
     * @param nr_of_instances number of instances to render
     */
    void draw_primitives(primitive_type type, int first_index, int nr_of_indices, int nr_of_instances) const;

    /// Set number of instances to render, 0 to turn off instanced rendering
    void set_nr_of_instances(int nr_i);

    /// Plain render like set up
    void render() const;

    /// Render N instances
    void render(int nr_of_instances) const;

  protected:
    bool initialized{false};                               ///< Is the context initialized and ready?
    const program* shared_render_program{nullptr};         ///< Program to use for rendering
    program render_program;                                ///< Attached program (if hold here)
    std::vector<const vertex_buffer*> vertex_buffers;      ///< Vertex buffers to use (index == location)
    std::vector<unsigned> uniform_buffers;                 ///< Uniform buffers to use (index == location)
    std::vector<unsigned> sst_buffers;                     ///< Shader storage buffers to use (index == location)
    std::vector<unsigned> textures;                        ///< Textures to use (index == texture unit number)
    std::vector<unsigned> samplers;                        ///< Samplers to use (index == texture unit number)
    const index_buffer* idx_buffer{nullptr};               ///< Optional index buffer to be used
    index_buffer my_idx_buffer;                            ///< Index buffer hold by render context
    primitive_type primitivetype{primitive_type::number};  ///< Primitive type of indices (optional)
    int nr_of_indices{0};                                  ///< Number of indices to render (optional)
    bool depth_test{true};                                 ///< Enable depth buffer test for rendering
    bool depth_write{true};                                ///< Enable writing to depth buffer for rendering
    bool wire_frame{false};                                ///< Enable wire frame rendering of triangles
    bool use_primitive_restart{false};                     ///< Enable use of primitive restart index
    uint32_t primitive_restart_index{0};                   ///< Index to use for primitive restart
    face_render_side render_side{face_render_side::front}; ///< Which side of faces to render
    blend_func_type blend_func{blend_func_type::srcalpha}; ///< Blending function to use for rendering
    std::vector<vertex_buffer> my_vertex_buffers;          ///< Vertex buffers hold by render context
    std::vector<uint32_t> vertex_attrib_divisors;          ///< Divisor for vertex attributes
    int nr_of_instances{0};                                ///< The number of instances to render
    void assert_internal_vertex_buffer_refs();             ///< Make sure that vertex_buffers
                                                           ///< refer correctly to internally
                                                           ///< stored buffers
    void add_tex_id(unsigned unit, unsigned tex_id,
                    sampler::type smp); ///< Internal add texture ID
};

/// A compute contex combining uniform and shader storage buffers and a program
class compute_context
{
  public:
    /// Constructor
    compute_context() = default;

    /// Destructor
    ~compute_context() = default;

    /** Bind a uniform buffer to a location
     * @param location shader location that buffer data is bound to
     * @param ub uniform buffer with data
     */
    void add(unsigned location, const uniform_buffer& ub);

    /** Bind a shader storage buffer to a location
     * @param location shader location that buffer data is bound to
     * @param sb shader storage buffer with data
     */
    void add(unsigned location, const shader_storage_buffer& sb);

    /** Bind a texture to a texture unit (image texture!)
     * @param unit texture unit that texture is bound to
     * @param tex texture
     */
    void add(unsigned unit, const texture& tex);

    /** Bind a texture to a texture unit (image texture!)
     * @param unit texture unit that texture is bound to
     * @param tex texture
     * @param level mipmap level to bind
     */
    void add(unsigned unit, const texture& tex, int level);

    /** Bind a texture to a texture unit (image texture!)
     * @param unit texture unit that texture is bound to
     * @param tex_arr texture array
     */
    void add(unsigned unit, const texture_array& tex_arr);

    /** Bind a texture to a texture unit (image texture!)
     * @param unit texture unit that texture is bound to
     * @param tex_cube texture cube map
     */
    void add(unsigned unit, const texture_cube& tex_cube);

    /// Define shader to use
    void add(const shader& shd);

    /// Set up and use for computing
    void use() const;

    /// Configure the compute size. Beware, if shader local_size > 1 this is NOT
    /// the absolute width/height/depth divided by local_size!
    void set_compute_size(int x, int y, int z);

    /// Compute with already set size
    void compute() const;

    /// Prepare for using the output
    void wait_for_output() const;

  protected:
    bool initialized{false};               ///< Is the context initialized and ready?
    program compute_program;               ///< The program to use
    std::vector<unsigned> uniform_buffers; ///< Uniform buffers to use (index == location)
    std::vector<unsigned> sst_buffers;     ///< Shader storage buffers to use (index == location)
    std::vector<unsigned> textures;        ///< Textures to use (index == texture unit number)
    std::vector<unsigned> texture_levels;  ///< Texture mipmap levels to use (0
                                           ///< for default, basic level)
    std::vector<int> texture_formats;      ///< Internal gpu formats of textures
                                           ///< (needed for mipmap levels)
    vector3i compute_size;                 ///< Number of workgroups to compute in xyz direction

  private:
    compute_context(compute_context&&)                 = delete;
    compute_context(const compute_context&)            = delete;
    compute_context& operator=(const compute_context&) = delete;
};

/// the GPU interface
class interface : public singleton<interface>
{
    friend class singleton<interface>; // to call desctructor
    friend std::default_delete<interface>;

  protected:
    /// construct the interface
    interface();

    /// destruct the interface
    ~interface();

  public:
    /// Initialize frame buffer
    void init_frame_buffer(int width, int height);

    /// Initialize frame buffer
    void init_frame_buffer(int offset_x, int offset_y, int width, int height);

    /// get maximum size of textures
    auto get_max_texture_size() const { return max_texture_size; }

    /// clear framebuffer color
    void clear_frame_buffer(const color& c = color(0, 0, 0, 0));

    /// clear depth buffer
    void clear_depth_buffer();

    /// clear framebuffer color and depth buffer simultaneously
    void clear_depth_and_frame_buffer(const color& c = color(0, 0, 0, 0));

    /// enable or disable depth testing
    void enable_depth_test(bool enable);

    /// enable or disable writing to depth buffer
    void enable_depth_buffer_write(bool enable);

    /// enable or disable wire frame rendering
    void enable_wire_frame(bool enable);

    /// enable or disable use of primitive restart index
    void use_primitive_restart_index(bool enable, uint32_t index);

    /// define which face sides should be rendered
    void set_face_render_side(face_render_side side);

    /// define blending function to be used
    void set_blend_function(blend_func_type bf = blend_func_type::standard);

    /// request anisotropic level
    float get_anisotropic_level() const { return anisotropic_level; }

    /// Remember new VAO to use and return if it changed (for internal use!)
    bool bind_new_VAO(int vao);

    /// Set program to use and return if current program has changed (for
    /// internal use!)
    bool use_program(const program& prg);

    /// Bind a frame buffer (only used internally!)
    void bind_frame_buffer(unsigned id);

    /// Wait for GPU to complete operations (not really necessary, mere for
    /// testing)
    void wait();

    ///> Get the default samplers
    auto get_sampler_gpu_id(int type) const { return (type < 0) ? 0 : default_samplers[type].get_gpu_id(); }

  protected:
    int max_texture_size{0};                               ///< Maximum texture size
    float anisotropic_level{0.f};                          ///< Degree of anisotropic filtering (0.0 = disabled)
    const program* current_program{nullptr};               ///< Store currently used program to avoid unneccessary use
                                                           ///< calls
    int current_vao{0};                                    ///< Store currently used VAO to avoid unneccessary binding
    bool depth_test{false};                                ///< Enable depth buffer test for rendering
    bool depth_write{false};                               ///< Enable writing to depth buffer for rendering
    bool wire_frame{true};                                 ///< Enable wire frame rendering of triangles
    bool use_primitive_restart{true};                      ///< Enable use of primitive restart index
    uint32_t primitive_restart_index{0};                   ///< Index to use for primitive restart
    face_render_side render_side{face_render_side::both};  ///< Which side of faces to render
    blend_func_type blend_func{blend_func_type::srcalpha}; ///< Blending function to use for rendering
    std::vector<sampler> default_samplers;                 ///< Default samplers
    vector4i viewport_data;                                ///< Last viewport parameters
    unsigned current_fb_id{0};                             ///< Currently bound frame buffer (0 if direct rendering)

  private:
    interface(const interface&)            = delete;
    interface& operator=(const interface&) = delete;
};

} // namespace gpu

/// get the singleton
inline gpu::interface& GPU()
{
    return gpu::interface::instance();
}
