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

// OpenGL GPU helper functions and classes
// (C)+(W) by Thorsten Jordan. See LICENSE

#pragma once

#include "angle.hpp"
#include "gpu_interface.hpp"
#include "plane.hpp"
#include "quaternion.hpp"

/// Everything that is GPU related goes to this namespace
namespace gpu
{

/** A camera class to make looking into the scene easier (sets up a projection
 * and modelview matrix in easy way)
 * @note Camera sets a combination of temporary projection and modelview
 * matrices as final real projection matrix, that will convert coordinates from
 * world space to screen space (world to camera with temporary modelview matrix
 *       and camera to screen with temporary projection matrix). Advantage is
 * that modelview matrices for objects can be designed to convert to world
 * space. Note that accuracy of double data type is high enough to have correct
 * transformations even if the camera is off 20,000km from the center (not with
 * float!). So we don't need any special viewer position handling like we did
 * with the old code!
 */
class camera
{
  public:
    /** Create a camera that looks at a position
     * @param pos position of camera
     * @param look_at look at this point
     * @param up up direction
     * @param fovx field of view in x direction in degrees
     * @param aspectratio aspect ratio of screen
     * @param nearz near z value of camera plane along view direction
     * @param farz far z value of camera plane along view direction
     */
    static camera create_look_at(
        const vector3& pos,
        const vector3& look_at,
        const vector3& up,
        double fovx,
        double aspectratio,
        double nearz,
        double farz);

    /** Create a camera that looks in a direction
     * @param pos position of camera
     * @param look_dir look in this direction
     * @param up up direction
     * @param fovx field of view in x direction in degrees
     * @param aspectratio aspect ratio of screen
     * @param nearz near z value of camera plane along view direction
     * @param farz far z value of camera plane along view direction
     */
    static camera create_look_dir(
        const vector3& pos,
        const vector3& look_dir,
        const vector3& up,
        double fovx,
        double aspectratio,
        double nearz,
        double farz);

    /** Create a camera with generic orientation
     * @param pos position of camera
     * @param orientation rotation of camera to use, represented as quaternion
     * @param fovx field of view in x direction in degrees
     * @param aspectratio aspect ratio of screen
     * @param nearz near z value of camera plane along view direction
     * @param farz far z value of camera plane along view direction
     */
    static camera create_generic(
        const vector3& pos,
        const quaternion& orientation,
        double fovx,
        double aspectratio,
        double nearz,
        double farz);

    /** Create a neutral orthograpic camera that results in identity matrix for
     * transformation.
     */
    static camera create_neutral();

    /// Set position of camera
    void set_position(const vector3& pos) { position = pos; }

    /// Set transformation of camera (look direction etc).
    void set_orientation(const matrix3& cs) { orientation = cs; }

    /// Set transformation of camera (look direction) by two angles. @param turn
    /// around z-axis. @param up around local y-axis, so 0 is looking in XY
    /// plane.
    void set_orientation(angle turn, angle up);

    /// Set position and look direction
    void set_position_and_look_direction(const vector3& pos, const vector3& look_dir, const vector3& up);

    /// Set position and look at
    void set_position_and_look_at(const vector3& pos, const vector3& look_at, const vector3& up)
    {
        set_position_and_look_direction(pos, look_at - pos, up);
    }

    /// Get position of the camera
    const vector3& get_position() const { return position; }

    /// Get the look direction of the camera
    vector3 get_look_dir() const { return -orientation.column(2); }

    /// Get the angle of the look direction in the XY plane
    angle get_look_angle() const { return angle::azimuth(get_look_dir()); }

    /// Get the angle of the upward look direction (difference to XY plane)
    angle get_up_angle() const { return angle::elevation(get_look_dir()); }

    /// Compute the modelview matrix so that objects rendered are shown as if
    /// camera was the screen.
    matrix4 get_transformation() const;

    /// Compute the matrix so that objects rendered are shown as if camera was
    /// the screen (combines world to camera and camera to screen space!)
    matrix4 get_projection_matrix() const;

    /// Compute combined projection and modelview matrix
    matrix4 get_pmv_matrix() const { return get_projection_matrix() * get_transformation(); }

    /// Render camera as frustum for debugging
    void render_camera_frustum(const camera& cam) const;

  protected:
    camera() = default;

    vector3 position;            ///< Position of camera in world space
    matrix3 orientation;         ///< Orientation of camera (xyz axes are side, up,
                                 ///< negative look direction). Note: a quaternion would
                                 ///< also be possible!
    double field_of_view_x{0.0}; ///< Field of view in x direction in degrees
    double aspect_ratio{0.0};    ///< Width to height aspect ratio
    double near_z{0.0};          ///< Near camera plane z value along look direction
    double far_z{0.0};           ///< Far camera plane z value along look direction
    bool is_neutral{false};      ///< Special flag that always returns identity for
                                 ///< transformation (more for debugging)
};

/// Class to hold stuff for common rendering stuff like quads / lines etc.
class draw : public singleton<draw>
{
  public:
    /// Constructor.
    draw();

    /** Draw textured quad
     * @param tex texture to use
     * @param pos 2d Position on screen to use
     */
    void quad_col(const texture& tex, const vector2i& pos, const colorf& c = colorf::transparency(1.f));

    /** Draw textured quad horizontally mirrored
     * @param tex texture to use
     * @param pos 2d Position on screen to use
     */
    void quad_horizontally_mirrored(const texture& tex, const vector2i& pos);

    /** Draw textured quad with one level of array texture
     * @param tex texture array to use
     * @param level which level of array to use
     * @param pos 2d Position on screen to use
     */
    void quad(const texture_array& tex, unsigned layer, const vector2i& pos = vector2i(0, 0));

    /** Draw textured quad
     * @param tex texture to use
     * @param pos 2d Position on screen to use
     */
    void quad(const texture& tex, const vector2i& pos);

    /** Draw textured quad scaled
     * @param tex texture to use
     * @param pos 2d Position on screen to use
     * @param size size to draw it
     */
    void quad(const texture& tex, const vector2i& pos, const vector2i& size);

    /** Draw textured quad scaled with nearest sampling
     * @param tex texture to use
     * @param pos 2d Position on screen to use
     * @param size size to draw it
     */
    void quad_n(const texture& tex, const vector2i& pos, const vector2i& size);

    /** Draw textured quad with one level of array texture
     * @param tex texture array to use
     * @param level which level of array to use
     * @param pos 2d Position on screen to use
     */
    void quad(const texture_array& tex, unsigned layer, const vector2i& pos, const vector2i& size);

    /** Draw colored quad
     * @param col color to use
     * @param pos 2d Position on screen to use
     * @param size size to draw it
     */
    void quad(color col, const vector2i& pos, const vector2i& size);

    /** Draw textured quad with one level of array texture scaled with nearest
     * sampling
     * @param tex texture array to use
     * @param level which level of array to use
     * @param pos 2d Position on screen to use
     */
    void quad_n(const texture_array& tex, unsigned layer, const vector2i& pos, const vector2i& size);

    /** Draw textured quad with rotation
     * @param tex texture to use
     * @param rotat_center 2d Position on screen to use as rotation center
     * @param angle angle to rotate around in degrees
     */
    void quad_rotated(const texture& tex, const vector2i& rotat_center, double angle);

    /** Draw lines (2d)
     * @param positions positions to use (two positions form one line)
     * @param col color to draw
     */
    void lines_2d(const std::initializer_list<vector2i>& positions, color col);

    /** Draw lines (3d)
     * @param cam camera to use for viewing
     * @param positions positions to use (two positions form one line)
     * @param col color to draw
     */
    void lines(const camera& cam, const std::vector<vector3f>& positions, color col);

    /** Draw colored triangles
     * @param cam camera to use for viewing
     * @param positions positions to use (three positions for one triangle)
     * @param colors colors to use (three colors for one triangle)
     */
    void colored_triangles(const camera& cam, const std::vector<vector3f>& positions, const std::vector<color>& colors);

    /** Draw line strip of vertices
     * @param cam camera to use for viewing
     * @param positions positions to use
     * @param col color to draw
     */
    void line_strip(const camera& cam, const std::vector<vector3f>& positions, color col);

    /** Draw coordinate system for debugging
     * @param cam camera to use for viewing
     * @param cs coordinate system as 4x4 matrix
     */
    void coordinate_system(const camera& cam, const matrix4f& cs = matrix4f::one());

    /** Draw wireframe cube for debugging
     * @param cs coordinate system for cube
     * @param hel half edge length
     * @param col color to use
     */
    void
    wire_cube(const camera& cam, const matrix4f& cs = matrix4f::one(), float hel = 1.0f, color col = color::white());

    /** Draw flat shaded cube for debugging, colors are taken from directions
     * @param cs coordinate system for cube
     * @param hel half edge length
     */
    void debug_cube(const camera& cam, const matrix4f& cs = matrix4f::one(), float hel = 1.0f);

    /** Draw 2d circle
     *  @param cam
     *  @param center center of circle
     *  @param radius radius of circle
     *  @param col color of circle
     */
    void circle_2d(const camera& cam, const vector2f& center, float radius, color col);

  protected:
    render_context rc_quad;           ///< Render context for quads (rectangles)
    render_context rc_texquad;        ///< Render context for textured quads
    render_context rc_texquad_n;      ///< Render context for textured quads with
                                      ///< nearest sampling
    render_context rc_texarrayquad;   ///< Render context for textured quads with
                                      ///< array textures
    render_context rc_texarrayquad_n; ///< Render context for textured quads
                                      ///< with array textures
    render_context rc_lines;          ///< Render context for lines / line strips
    render_context rc_coltris;        ///< Render context for colored triangles
    uniform_buffer ubo_texquad;       ///< Uniform data for texquad render context
    program prg_quad;                 ///< Render program for quads - fixme may use
                                      ///< default programs now!
    program prg_texquad;              ///< Render program for textured quads - fixme may use
                                      ///< default programs now!
    program prg_texarrayquad;         ///< Render program for textured quads with
                                      ///< texture array
    vertex_buffer vbo_lines;          ///< Vertex buffer for line data
    vertex_buffer vbo_triangles;      ///< Vertex buffer for triangle data
    vertex_buffer vbo_colors;         ///< Vertex buffer for color data
    uniform_buffer ubo_lines;         ///< Uniform data for lines render context
    uniform_buffer ubo_pmv;           ///< Uniform data for generic projection-modelview-matrix
    program prg_lines;                ///< Render program for lines
    /// Uniform data for texquad render context
    struct texquad_udata
    {
        vector4f position_offset_scaling;
        vector4f texcoord_offset_scaling;
        colorf col;
        uint32_t layer;
        texquad_udata(
            const vector4f& p = vector4f(0.f, 0.f, 1.f, 1.f),
            const vector4f& t = vector4f(0.f, 0.f, 1.f, 1.f),
            const colorf& c   = colorf(1.f, 1.f, 1.f, 1.f),
            unsigned l        = 0)
            : position_offset_scaling(p)
            , texcoord_offset_scaling(t)
            , col(c)
            , layer(l)
        {
        }
    };
    /// Uniform data for line/linestrip render contexts
    struct line_udata
    {
        matrix4f pmv;
        colorf col;
    };
    texture texquad_dummy;
    texture_array texquadarray_dummy;
};

/// Compute a Fast Fourier Transform with compute shaders on GPU
class compute_FFT
{
  public:
    /** Constructor
     * @param workspace texture that is used as input and output
     * @param forward compute forward transformation? otherwise backward
     * @param use_half_float use half precision floats for computation (faster)
     */
    compute_FFT(texture& workspace, bool forward, bool use_half_float);
    /// Compute the FFT
    void compute();
    /// Compute reversed bit indices
    static std::vector<unsigned> compute_reversed_bits_indices(unsigned exponent);
    /// Request the temporary working space texture (for debugging purposes)
    const texture& get_temp_workspace() const { return temp_workspace; }

  protected:
    void compute_indices_and_factors(bool forward);

    compute_context cc_fft_columns;
    compute_context cc_fft_columns_LL; // for last level with output transform
    compute_context cc_fft_rows;
    compute_context cc_fft_rows_LL; // for last level with output transform
    unsigned fft_size{0};
    unsigned fft_size_log2{0};
    std::vector<shader_storage_buffer> index_buffers;
    std::vector<shader_storage_buffer> factor_buffers;
    texture& workspace;
    texture temp_workspace;
    const unsigned local_size{16};
};

/// In which vertex attribute location are the attributes stored for default
/// shaders
enum class basic_shader_attribute_location
{
    position    = 0,
    normal      = 1,
    texcoord    = 2,
    tangentx    = 3,
    righthanded = 4,
    color       = 5,
};

/// In which slots to put the textures to for default shaders
enum class basic_shader_sampler_location
{
    color_map    = 0,
    normal_map   = 1,
    specular_map = 2,
    caustics_map = 3,
};

/// In which uniform slots to put the data in
enum class basic_shader_uniform_location
{
    transform = 0,  // 2 mat4 needs 8 slots (preferably store per object)
    light     = 8,  // 1 vec4 and 1 vec3/float needs 2 slots (preferably global)
    fog       = 10, // 1 vec3 and 3 float needs 2 slots (preferably global)
    material  = 11, // 1 vec3 and 1 float and 1 vec4 needs 2 slots (preferably
                    // store per object)
    clipplane = 13, // 1 vec4 as plane equation needs 1 slot (preferably global)
    user      = 14  // all user defined uniforms start here.
};

/// Basic shader features (most of them can be combined)
enum class basic_shader_feature
{
    colormap = 1,       // take colors from a RGB texture instead of other sources. If
                        // defined, vertex_color is ignored.
    normalmap = 2,      // take normals from a RGB texture instead of vertex normals,
                        // needs lighting
    specularmap = 4,    // shade specular color by 1-Channel texture, ignored
                        // without lighting
    fog          = 8,   // apply fog after shading
    underwater   = 16,  // render scenery like under water, fog is ignored then
    clipplane    = 32,  // clip scenery to front side of clipping plane
    lighting     = 64,  // use lighting in general (phong model)
    vertex_color = 128, // if no colormap exists, use colors per vertex. If off,
                        // use global color
};

#pragma pack(push, 4)
// Note: alignment rules, floats are 1-aligned, vector2f 2-aligned,
// vector3f/vector4f are 4-aligned, matrix4f uses 4 vector4f.
/// Scene transformation data used for uniforms in shaders
/// (basic_shader_uniform_location::transform)
struct transform_data
{
    matrix4f projection_modelview; ///< Multiplied projection and modelview matrix
                                   ///< (slots 0-3) - declare as row_major!
    matrix4f modelview_inverse;    ///< Inverse modelview matrix (camera to object
                                   ///< space) (slots 4-7) - declare as row_major!
    static std::string get_definition();
};

/// Light data used for uniforms in shaders
/// (basic_shader_uniform_location::light)
struct light_data
{
    vector4f position;         ///< Light position, can be directional (slot 8)
    vector3f color;            ///< Color of light (slot 9)
    float ambient_factor{0.f}; ///< Basic brightness level in [0...1] of ambient
                               ///< light. (4th value of slot 9)
    static std::string get_definition();
};

/// Fog data used for uniforms in shaders (basic_shader_uniform_location::fog)
struct fog_data
{
    vector3f color;     ///< Basic color of fog (slot 10)
    float density{0.f}; ///< Density (4th value of slot 10)
    static std::string get_definition();
};

/// Material data used for uniforms in shaders
/// (basic_shader_uniform_location::material)
struct material_data
{
    vector3f specular_color; ///< Basic color of fog (slot 11)
    float shininess{0.f};    ///< Density (4th value of slot 11)
    colorf common_color;     ///< Basic color with alpha, if no colormap nor vertex
                             ///< color given (slot 12)
    static std::string get_definition();
};

/// Clip plane data used for uniforms in shaders
/// (basic_shader_uniform_location::clipplane)
struct clipplane_data
{
    vector4f clipplane; ///< equation (slot 13)
    static std::string get_definition();
};
#pragma pack(pop)

/** Class describing a scene environment (cameras, light, fog), but not the
 * models.
 * @remarks Give all positions in world space.
 */
class scene
{
  public:
    /// Create new scene with a camera
    scene(camera&& mycamera);
    /// Get camera reference for display
    const camera& get_current_camera() const { return cameras[current_camera_index]; }
    /// Get camera by index
    const camera& get_camera(unsigned index) const { return cameras[index]; }
    /// Get camera by index
    camera& get_camera(unsigned index) { return cameras[index]; }
    /// Add new camera to the scene
    void add_camera(camera&& mycamera) { cameras.push_back(std::move(mycamera)); }
    /// Get number of cameras in scene
    unsigned get_nr_of_cameras() const { return unsigned(cameras.size()); }
    /// Select a camera of the scene for display
    void select_camera(unsigned index);
    /// Modify existing camera of scene
    void set_camera(unsigned index, camera&& mycamera)
    {
        cameras[index] = std::move(mycamera);
        select_camera(index);
    }
    /// Modify current camera position and orientation
    void set_current_camera_transformation(const matrix4& transform);
    /// Modify current camera look at
    void set_current_camera_position_and_look_at(const vector3& pos, const vector3& look_at, const vector3& up);
    /// Set light data in scene, light position in world space
    void set_light_data(const light_data& ld);
    /// Set fog data of scene
    void set_fog_data(const fog_data& fd);
    /// Set clip plane data of scene, plane in world space
    void set_clip_plane(const plane& clipplane);
    /// Give UBO for light
    const uniform_buffer& get_light_ubo() const { return light_ubo; }
    /// Give UBO for fog
    const uniform_buffer& get_fog_ubo() const { return fog_ubo; }
    /// Give UBO for clip plane
    const uniform_buffer& get_clipplane_ubo() const { return clip_ubo; }

  protected:
    matrix4 projection;               ///< Stored projection matrix
    matrix4 modelview;                ///< Stored modelview matrix (can be taken from camera or
                                      ///< used defined)
    std::vector<camera> cameras;      ///< All cameras in the scene
    unsigned current_camera_index{0}; ///< Index of current camera
    uniform_buffer light_ubo;         ///< Light data
    uniform_buffer fog_ubo;           ///< Fog data
    uniform_buffer clip_ubo;          ///< Data for mirror clipping
    plane clipplane;                  ///< Data about clipplane in world space
    vector4 lightpos;                 ///< Light position in world space

    scene() = delete;
};

/** helper structure for shader compilation */
struct shader_source_helper
{
    std::string vertex_defs, vertex_code, fragment_defs, fragment_code;
    /// Accumulate more
    shader_source_helper& operator+=(const shader_source_helper& source)
    {
        vertex_defs += source.vertex_defs;
        vertex_code += source.vertex_code;
        fragment_defs += source.fragment_defs;
        fragment_code += source.fragment_code;
        return *this;
    }
};

/// Generate a program from helper structure
program make(const shader_source_helper& ssh);

/** Generate code for fractal noise (fBM)
 * @param resolution noise texture resolution (must be power of two value)
 * @param base_factor number of samples in XY direction to use for base level of
 * noise
 * @param nr_of_levels number of levels to generate (note that base_resolution
 * << nr_of_levels should be <= texture width for no repeating to occur)
 * @param tex_unit which texture unit to use for noise map (2D, 1-channel R)
 * @param offset_slot uniform buffer slot to use for 2d offset values
 * (nr_of_levels values)
 * @param amplitude_scale scaling of amplitude when going to next finer level
 */
shader_source_helper get_noise_shader_data(
    unsigned resolution,
    unsigned base_factor,
    unsigned nr_of_levels,
    unsigned tex_unit,
    unsigned offset_slot,
    float amplitude_scale = 0.5f);

/** Generate source code for shaders with basic features
 * @param bsc which level of rendering to use
 */
shader_source_helper generate_basic_shader_source(basic_shader_feature bsf);

} // namespace gpu

/// get the draw singleton (convenience function)
inline gpu::draw& GPU_DRAW()
{
    return gpu::draw::instance();
}

// hack for gcc-5.3, fixme remove
namespace std
{
template<>
struct hash<gpu::basic_shader_feature>
{
    size_t operator()(const gpu::basic_shader_feature& bsf) const { return size_t(bsf); }
};
} // namespace std
