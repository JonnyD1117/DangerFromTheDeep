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

#include "gpu_helper.hpp"

#include "system_interface.hpp"

#include <array>
#include <sstream>
using namespace gpu;

camera camera::create_look_at(
    const vector3& pos,
    const vector3& look_at,
    const vector3& up,
    double fovx,
    double aspectratio,
    double nearz,
    double farz)
{
    return create_look_dir(pos, look_at - pos, up, fovx, aspectratio, nearz, farz);
}

camera camera::create_look_dir(
    const vector3& pos,
    const vector3& look_dir,
    const vector3& up,
    double fovx,
    double aspectratio,
    double nearz,
    double farz)
{
    camera cam;
    cam.set_position_and_look_direction(pos, look_dir, up);
    cam.field_of_view_x = fovx;
    cam.aspect_ratio    = aspectratio;
    cam.near_z          = nearz;
    cam.far_z           = farz;
    return cam;
}

camera camera::create_generic(
    const vector3& pos,
    const quaternion& orientation,
    double fovx,
    double aspectratio,
    double nearz,
    double farz)
{
    camera cam;
    cam.position        = pos;
    cam.orientation     = orientation.rotmat();
    cam.field_of_view_x = fovx;
    cam.aspect_ratio    = aspectratio;
    cam.near_z          = nearz;
    cam.far_z           = farz;
    return cam;
}

camera camera::create_neutral()
{
    camera cam;
    cam.position    = vector3();
    cam.orientation = matrix3::one();
    cam.is_neutral  = true;
    return cam;
}

void camera::set_orientation(angle turn, angle up)
{
    auto look_dir = angle::direction_from_azimuth_and_elevation(turn, up);
    vector3 xaxis = look_dir.cross(vector3(axis::z)).normal();
    vector3 yaxis = xaxis.cross(look_dir).normal();
    vector3 zaxis = xaxis.cross(yaxis).normal();
    orientation   = matrix3(xaxis, yaxis, zaxis);
}

void camera::set_position_and_look_direction(const vector3& pos, const vector3& look_dir, const vector3& up)
{
    position               = pos;
    vector3 look_direction = look_dir;
    double len             = look_direction.length();
    if (len < epsilon<double>()) {
        // too risky, abort
        THROW(error, "camera look direction to short!");
    }
    look_direction *= 1.0 / len;
    vector3 side_direction = up.cross(-look_direction);
    len                    = side_direction.length();
    if (len < epsilon<double>()) {
        // up direction is not well defined, abort
        THROW(error, "up direction too close to look direction!");
    }
    side_direction *= 1.0 / len;
    // Build orthogonal system. Since side and look dir are already orthogonal
    // and normalized, the resulting up direction will be as well.
    const vector3 up_direction = side_direction.cross(look_direction); // X cross Z, so not -look_dir here
    orientation                = matrix3(side_direction, up_direction, -look_direction);
}

matrix4 camera::get_transformation() const
{
    // Compute transformation matrix - that is inverse of system combined of the
    // stored vectors. We build it by transposing the rotation part and
    // multiplying with position. X is side, Y is up, and Z is -look.
    vector3 pos             = position;
    vector3 side_direction  = orientation.column(0);
    vector3 up_direction    = orientation.column(1);
    vector3 nlook_direction = orientation.column(2);
    vector3 p(side_direction * pos, up_direction * pos, nlook_direction * pos);
    return matrix4(
        side_direction.x,
        side_direction.y,
        side_direction.z,
        -p.x,
        up_direction.x,
        up_direction.y,
        up_direction.z,
        -p.y,
        nlook_direction.x,
        nlook_direction.y,
        nlook_direction.z,
        -p.z,
        0.0,
        0.0,
        0.0,
        1.0);
}

matrix4 camera::get_projection_matrix() const
{
    return (is_neutral ? matrix4::one() : matrix4::frustum_fovx(field_of_view_x, aspect_ratio, near_z, far_z));
}

void camera::render_camera_frustum(const camera& cam) const
{
    draw::instance().wire_cube(cam, get_pmv_matrix().inverse(), 1.0f, color::white());
    std::vector<vector3f> positions(4);
    positions[0] = position;
    positions[1] = position + orientation.column(1) * 2.0;
    positions[2] = position;
    positions[3] = position + orientation.column(2) * -10.0;
    draw::instance().lines(cam, positions, color::white());
}

draw::draw()
    : texquad_dummy(1, 1, 3, data_type::u8, false)
    , texquadarray_dummy(1, 1, 1, 3, data_type::u8, false)
{
    // Initialize render context for drawing functions
    shader vsquadtex(
        "#version 430 core\n"
        // we can compute it from gl_VertexID and integer logic as well...
        "const vec2 positions[4] = { vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0) };\n"
        "const vec2 texcoords[4] = { vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0) };\n"
        "layout(std140, binding = 0) uniform in_uni { vec4 p_off_scal; vec4 t_off_scal; uint layer; } inp;\n"
        "out vec2 texcoord;\n"
        "void main() {\n"
        "texcoord = texcoords[gl_VertexID] * inp.t_off_scal.zw + inp.t_off_scal.xy;\n"
        "gl_Position = vec4(positions[gl_VertexID] * inp.p_off_scal.zw + inp.p_off_scal.xy, 0.0, 1.0);\n"
        "}\n",
        shader::type::vertex,
        true);
    shader vsquad(
        "#version 430 core\n"
        // we can compute it from gl_VertexID and integer logic as well...
        "const vec2 positions[4] = { vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0) };\n"
        "layout(std140, binding = 0) uniform in_uni { vec4 p_off_scal; vec4 t_off_scal; uint layer; } inp;\n"
        "void main() {\n"
        "gl_Position = vec4(positions[gl_VertexID] * inp.p_off_scal.zw + inp.p_off_scal.xy, 0.0, 1.0);\n"
        "}\n",
        shader::type::vertex,
        true);
    shader fstex(
        "#version 430 core\n"
        "in vec2 texcoord;\n"
        "layout(binding = 0) uniform sampler2D tex;\n"
        "out vec4 frag_color;\n"
        "void main() { frag_color = vec4(texture(tex, texcoord).xyz, 1.0); }\n",
        shader::type::fragment,
        true);
    shader fstexarray(
        "#version 430 core\n"
        "in vec2 texcoord;\n"
        "layout(binding = 0) uniform sampler2DArray tex;\n"
        "layout(std140, binding = 0) uniform in_uni { vec4 p_off_scal; vec4 "
        "t_off_scal; uint layer; } inp;\n"
        "out vec4 frag_color;\n"
        "void main() { frag_color = vec4(texture(tex, vec3(texcoord, "
        "inp.layer)).xyz, 1.0); }\n",
        shader::type::fragment,
        true);
    shader fslines(
        "#version 430 core\n"
        "in vec4 color;\n"
        "out vec4 frag_color;\n"
        "void main() {\n"
        "frag_color = color;\n"
        "}\n",
        shader::type::fragment,
        true);
    prg_quad         = program(vsquad, fslines);
    prg_texquad      = program(vsquadtex, fstex);
    prg_texarrayquad = program(vsquadtex, fstexarray);
    ubo_texquad.init(buffer::usage_type::dynamic_draw, texquad_udata());
    rc_quad.add(0, ubo_texquad);
    rc_quad.add(prg_quad);
    rc_quad.add(primitive_type::triangle_fan, 4);
    rc_quad.set_2D_drawing(true);
    rc_quad.init();
    rc_texquad.add(0, ubo_texquad);
    rc_texquad.add(prg_texquad);
    rc_texquad.add(primitive_type::triangle_fan, 4);
    rc_texquad.add(0, texquad_dummy, sampler::type::bilinear_clamp);
    rc_texquad.set_2D_drawing(true);
    rc_texquad.init();
    rc_texquad_n.add(0, ubo_texquad);
    rc_texquad_n.add(prg_texquad);
    rc_texquad_n.add(primitive_type::triangle_fan, 4);
    rc_texquad_n.add(0, texquad_dummy, sampler::type::nearest_clamp);
    rc_texquad_n.set_2D_drawing(true);
    rc_texquad_n.init();
    rc_texarrayquad.add(0, ubo_texquad);
    rc_texarrayquad.add(prg_texarrayquad);
    rc_texarrayquad.add(primitive_type::triangle_fan, 4);
    rc_texarrayquad.add(0, texquad_dummy, sampler::type::bilinear_clamp);
    rc_texarrayquad.set_2D_drawing(true);
    rc_texarrayquad.init();
    rc_texarrayquad_n.add(0, ubo_texquad);
    rc_texarrayquad_n.add(prg_texarrayquad);
    rc_texarrayquad_n.add(primitive_type::triangle_fan, 4);
    rc_texarrayquad_n.add(0, texquad_dummy, sampler::type::nearest_clamp);
    rc_texarrayquad_n.set_2D_drawing(true);
    rc_texarrayquad_n.init();
    shader vslines(
        "#version 430 core\n"
        "layout(std140, binding = 0, row_major) uniform in_uni { mat4 pmv; "
        "vec4 col; };\n"
        "layout(location = 0) in vec3 pos;\n"
        "out vec4 color;\n"
        "void main() {\n"
        "color = col;\n"
        "gl_Position = pmv * vec4(pos, 1.0);\n"
        "}\n",
        shader::type::vertex,
        true);
    prg_lines = program(vslines, fslines);
    ubo_lines.init(buffer::usage_type::stream_draw, line_udata());
    vbo_lines.init(2, (vector3f*) nullptr, buffer::usage_type::stream_draw);
    rc_lines.add(0, vbo_lines);
    rc_lines.add(0, ubo_lines);
    rc_lines.add(prg_lines);
    rc_lines.enable_depth_test(false);
    rc_lines.enable_depth_buffer_write(false);
    rc_lines.init();
    shader vscoltris(
        "#version 430 core\n"
        "layout(std140, binding = 0, row_major) uniform in_uni { mat4 pmv; };\n"
        "layout(location = 0) in vec3 pos;\n"
        "layout(location = 1) in vec4 col;\n"
        "out vec4 color;\n"
        "void main() {\n"
        "color = col;\n"
        "gl_Position = pmv * vec4(pos, 1.0);\n"
        "}\n",
        shader::type::vertex,
        true);
    ubo_pmv.init(buffer::usage_type::stream_draw, matrix4f());
    vbo_triangles.init(3, (vector3f*) nullptr, buffer::usage_type::stream_draw);
    vbo_colors.init(3, (color*) nullptr, buffer::usage_type::stream_draw);
    rc_coltris.add(0, vbo_triangles);
    rc_coltris.add(1, vbo_colors);
    rc_coltris.add(0, ubo_pmv);
    rc_coltris.add(program(vscoltris, fslines));
    rc_coltris.enable_depth_test(false);
    rc_coltris.enable_depth_buffer_write(false);
    // not always sensible:
    // rc_coltris.set_face_render_side(face_render_side::both);
    rc_coltris.init();
}

void draw::quad_col(const texture& tex, const vector2i& pos, const colorf& c)
{
    // we need to transform coordinates and widths/heights with a 2D scaling
    // factor. Y-Coordinates are also negative, so we have a scale and offset
    // for positions.
    vector2f p = SYS().translate_2D_coordinates(pos);
    vector2f s = SYS().translate_2D_size(tex.get_size());
    texquad_udata data(vector4f(p, s));
    data.col = c;
    ubo_texquad.update_data(data);
    rc_texquad.add(0, tex, sampler::type::bilinear_clamp);
    rc_texquad.render();
}

void draw::quad_horizontally_mirrored(const texture& tex, const vector2i& pos)
{
    // we need to transform coordinates and widths/heights with a 2D scaling
    // factor. Y-Coordinates are also negative, so we have a scale and offset
    // for positions.
    vector2f p = SYS().translate_2D_coordinates(pos);
    vector2f s = SYS().translate_2D_size(tex.get_size());
    texquad_udata data(vector4f(p, s), vector4f(1.f, 0.f, -1.f, 1.f));
    ubo_texquad.update_data(data);
    rc_texquad.add(0, tex, sampler::type::bilinear_clamp);
    rc_texquad.render();
}

void draw::quad(const texture_array& tex, unsigned layer, const vector2i& pos)
{
    // we need to transform coordinates and widths/heights with a 2D scaling
    // factor. Y-Coordinates are also negative, so we have a scale and offset
    // for positions.
    vector2f p = SYS().translate_2D_coordinates(pos);
    vector2f s = SYS().translate_2D_size(tex.get_size());
    texquad_udata data(vector4f(p, s));
    data.layer = layer;
    ubo_texquad.update_data(data);
    rc_texarrayquad.add(0, tex, sampler::type::bilinear_clamp);
    rc_texarrayquad.render();
}

void draw::quad(const texture& tex, const vector2i& pos)
{
    // we need to transform coordinates and widths/heights with a 2D scaling
    // factor. Y-Coordinates are also negative, so we have a scale and offset
    // for positions.
    vector2f p = SYS().translate_2D_coordinates(pos);
    vector2f s = SYS().translate_2D_size(tex.get_size());
    texquad_udata data(vector4f(p, s));
    ubo_texquad.update_data(data);
    rc_texquad.add(0, tex, sampler::type::bilinear_clamp);
    rc_texquad.render();
}

void draw::quad(const texture& tex, const vector2i& pos, const vector2i& size)
{
    // we need to transform coordinates and widths/heights with a 2D scaling
    // factor. Y-Coordinates are also negative, so we have a scale and offset
    // for positions.
    vector2f p = SYS().translate_2D_coordinates(pos);
    vector2f s = SYS().translate_2D_size(size);
    texquad_udata data(vector4f(p, s));
    ubo_texquad.update_data(data);
    rc_texquad.add(0, tex, sampler::type::bilinear_clamp);
    rc_texquad.render();
}

void draw::quad(color col, const vector2i& pos, const vector2i& size)
{
    // we need to transform coordinates and widths/heights with a 2D scaling
    // factor. Y-Coordinates are also negative, so we have a scale and offset
    // for positions.
    vector2f p = SYS().translate_2D_coordinates(pos);
    vector2f s = SYS().translate_2D_size(size);
    texquad_udata data(vector4f(p, s));
    ubo_texquad.update_data(data);
    rc_quad.render();
}

void draw::quad_n(const texture& tex, const vector2i& pos, const vector2i& size)
{
    // we need to transform coordinates and widths/heights with a 2D scaling
    // factor. Y-Coordinates are also negative, so we have a scale and offset
    // for positions.
    vector2f p = SYS().translate_2D_coordinates(pos);
    vector2f s = SYS().translate_2D_size(size);
    texquad_udata data(vector4f(p, s));
    ubo_texquad.update_data(data);
    rc_texquad_n.add(0, tex, sampler::type::nearest_clamp);
    rc_texquad_n.render();
}

void draw::quad(const texture_array& tex, unsigned layer, const vector2i& pos, const vector2i& size)
{
    // we need to transform coordinates and widths/heights with a 2D scaling
    // factor. Y-Coordinates are also negative, so we have a scale and offset
    // for positions.
    vector2f p = SYS().translate_2D_coordinates(pos);
    vector2f s = SYS().translate_2D_size(size);
    texquad_udata data(vector4f(p, s));
    data.layer = layer;
    ubo_texquad.update_data(data);
    rc_texarrayquad.add(0, tex, sampler::type::bilinear_clamp);
    rc_texarrayquad.render();
}

void draw::quad_n(const texture_array& tex, unsigned layer, const vector2i& pos, const vector2i& size)
{
    // we need to transform coordinates and widths/heights with a 2D scaling
    // factor. Y-Coordinates are also negative, so we have a scale and offset
    // for positions.
    vector2f p = SYS().translate_2D_coordinates(pos);
    vector2f s = SYS().translate_2D_size(size);
    texquad_udata data(vector4f(p, s));
    data.layer = layer;
    ubo_texquad.update_data(data);
    rc_texarrayquad_n.add(0, tex, sampler::type::nearest_clamp);
    rc_texarrayquad_n.render();
}

void draw::quad_rotated(const texture& tex, const vector2i& rotat_center, double angle)
{
    // fixme the code went missing!
}

void draw::lines_2d(const std::initializer_list<vector2i>& positions, color col)
{
    if (positions.size() < 2)
        return;
    line_udata data;
    data.pmv = matrix4f::one();
    data.col = col;
    ubo_lines.update_data(data);
    std::vector<vector3f> positions3d(positions.size());
    auto i = 0;
    for (auto& pos2d : positions) {
        positions3d[i++] = SYS().translate_2D_coordinates(pos2d).xy0();
    }
    vbo_lines.update(positions3d, buffer::usage_type::stream_draw);
    rc_lines.use();
    rc_lines.draw_primitives(primitive_type::lines, 0, unsigned(positions.size()) & ~1);
}

void draw::lines(const camera& cam, const std::vector<vector3f>& positions, color col)
{
    if (positions.size() < 2)
        return;
    line_udata data;
    data.pmv = cam.get_pmv_matrix();
    data.col = col;
    ubo_lines.update_data(data);
    vbo_lines.update(positions, buffer::usage_type::stream_draw);
    rc_lines.use();
    rc_lines.draw_primitives(primitive_type::lines, 0, unsigned(positions.size()) & ~1);
}

void draw::colored_triangles(
    const camera& cam,
    const std::vector<vector3f>& positions,
    const std::vector<color>& colors)
{
    if (positions.size() < 3)
        return;
    ubo_pmv.update_data(matrix4f(cam.get_pmv_matrix()));
    vbo_triangles.update(positions, buffer::usage_type::stream_draw);
    vbo_colors.update(colors, buffer::usage_type::stream_draw);
    rc_coltris.use();
    rc_coltris.draw_primitives(primitive_type::triangles, 0, unsigned(positions.size()));
}

void draw::line_strip(const camera& cam, const std::vector<vector3f>& positions, color col)
{
    if (positions.size() < 2)
        return;
    line_udata data;
    data.pmv = cam.get_pmv_matrix();
    data.col = col;
    ubo_lines.update_data(data);
    vbo_lines.update(positions, buffer::usage_type::stream_draw);
    rc_lines.use();
    rc_lines.draw_primitives(primitive_type::line_strip, 0, unsigned(positions.size()));
}

void draw::coordinate_system(const camera& cam, const matrix4f& cs)
{
    // Three colored lines from center of system in red,green,blue
    std::vector<vector3f> vertices(2);
    vertices[0]                    = cs.column3(3);
    static const float axis_length = 10.f;
    vertices[1]                    = vertices[0] + cs.column3(0) * axis_length;
    lines(cam, vertices, color::red());
    vertices[1] = vertices[0] + cs.column3(1) * axis_length;
    lines(cam, vertices, color::green());
    vertices[1] = vertices[0] + cs.column3(2) * axis_length;
    lines(cam, vertices, color::blue());
}

void draw::wire_cube(const camera& cam, const matrix4f& cs, float hel, color col)
{
    std::array<vector3f, 8> vertices;
    for (unsigned i = 0; i < 8; ++i) {
        vertices[i] = cs.mul4vec3(vector3((i & 1) ? hel : -hel, (i & 2) ? hel : -hel, (i & 4) ? hel : -hel));
    }
    std::vector<vector3f> positions(24);
    static const unsigned idx[24] = {0, 1, 1, 3, 3, 2, 2, 0, 0, 4, 1, 5, 2, 6, 3, 7, 4, 5, 5, 7, 7, 6, 6, 4};
    for (unsigned i = 0; i < 24; ++i) {
        positions[i] = vertices[idx[i]];
    }
    draw::instance().lines(cam, positions, col);
}

void draw::debug_cube(const camera& cam, const matrix4f& cs, float hel)
{
    std::array<vector3f, 8> vertices;
    for (unsigned i = 0; i < 8; ++i) {
        vertices[i] = cs.mul4vec3(vector3((i & 1) ? hel : -hel, (i & 2) ? hel : -hel, (i & 4) ? hel : -hel));
    }
    std::vector<color> colors(36);
    std::vector<vector3f> positions(36);
    static const unsigned idx[36] = {0, 1, 2, 2, 1, 3, 1, 5, 3, 3, 5, 7, 2, 3, 6, 6, 3, 7,
                                     5, 4, 7, 7, 4, 6, 4, 0, 6, 6, 0, 2, 4, 5, 0, 0, 5, 1};
    static const color cols[6]    = {
           color(255, 0, 0, 64),
           color(0, 255, 0, 64),
           color(0, 0, 255, 64),
           color(128, 0, 0, 64),
           color(0, 128, 0, 64),
           color(0, 0, 128, 64)};
    for (unsigned i = 0; i < 36; ++i) {
        positions[i] = vertices[idx[i]];
        colors[i]    = cols[i / 6];
    }
    draw::instance().colored_triangles(cam, positions, colors);
}

void draw::circle_2d(const camera& cam, const vector2f& center, float radius, color col)
{
    // use 2 pixels per line each
    const auto number_of_positions = unsigned(floor(M_PI * radius));
    if (number_of_positions < 2)
        return;
    line_udata data;
    data.pmv = matrix4f::one();
    data.col = col;
    ubo_lines.update_data(data);
    std::vector<vector3f> positions3d(number_of_positions);
    for (unsigned i = 0; i < number_of_positions; ++i) {
        float a        = i * 2 * M_PI / number_of_positions;
        positions3d[i] = SYS().translate_2D_coordinates(center + vector2f{std::sin(a), std::cos(a)} * radius).xy0();
    }
    vbo_lines.update(positions3d, buffer::usage_type::stream_draw);
    rc_lines.use();
    rc_lines.draw_primitives(primitive_type::lines, 0, number_of_positions & ~1);
}

scene::scene(camera&& mycamera)
{
    cameras.push_back(std::move(mycamera));

    // set some default values to the buffers and initialize them
    light_data ld;
    ld.ambient_factor = 0.1f;
    ld.color          = vector3f(1.f, 1.f, 1.f);
    lightpos          = vector4(0.0, 1.0, 1.0, 0.0).normal();
    ld.position       = vector4f(get_current_camera().get_transformation().inverse() * lightpos);
    light_ubo.init(buffer::usage_type::static_draw, ld);

    fog_data fd;
    fd.color   = vector3f(0.7f, 0.7f, 0.7f);
    fd.density = 0.0005f;
    fog_ubo.init(buffer::usage_type::static_draw, fd);

    clipplane_data cd;
    cd.clipplane = vector4f(0.f, 0.f, 1.f, 0.f);
    clip_ubo.init(buffer::usage_type::static_draw, cd);
}

void scene::select_camera(unsigned index)
{
    // set light pos and clipplane from world space data
    if (index != current_camera_index) {
        current_camera_index = index;
        // set only light position, leave color/ambient factor untouched!
        light_ubo.access_data<light_data>()->position = vector4f(lightpos);
        set_clip_plane(clipplane);
    }
}

void scene::set_current_camera_transformation(const matrix4& transform)
{
    cameras[current_camera_index].set_position(transform.column3(3));
    cameras[current_camera_index].set_orientation(transform.upper_left_3x3());
    ++current_camera_index; // force update
    select_camera(current_camera_index - 1);
}

void scene::set_current_camera_position_and_look_at(const vector3& pos, const vector3& look_at, const vector3& up)
{
    cameras[current_camera_index].set_position_and_look_at(pos, look_at, up);
    ++current_camera_index; // force update
    select_camera(current_camera_index - 1);
}

void scene::set_light_data(const light_data& ld)
{
    light_data ldc = ld;
    // light position is in world space. We need to transform it to camera
    // space, so inverse modelview matrix converts it from camera space to
    // object space later in shader.
    lightpos     = vector4(ld.position);
    ldc.position = vector4f(get_current_camera().get_transformation() * lightpos);
    light_ubo.update_data(ldc);
}

void scene::set_fog_data(const fog_data& fd)
{
    fog_ubo.update_data(fd);
}

void scene::set_clip_plane(const plane& clipplane_)
{
    clipplane = clipplane_;
    clipplane_data cd;
    cd.clipplane   = vector4f(get_current_camera().get_transformation() * clipplane.N.xyz0());
    cd.clipplane.w = float(clipplane.d);
    clip_ubo.update_data(cd);
}

compute_FFT::compute_FFT(texture& workspace_, bool forward, bool use_half_float)
    : workspace(workspace_)
{
    if (workspace.get_nr_of_channels() != 2 || workspace.get_width() != workspace.get_height())
        THROW(error, "must use 2 channel quadratic textures for FFT");
    // Compute size
    fft_size      = workspace.get_width();
    fft_size_log2 = 0;
    for (unsigned i = fft_size; i > 1; i >>= 1) {
        ++fft_size_log2;
    }
    if ((1U << fft_size_log2) != fft_size) {
        THROW(error, "FFT texture must be power of two size!");
    }
    // create other workspace texture
    temp_workspace = texture(fft_size, fft_size, 2, use_half_float ? data_type::f16 : data_type::f32);
    // Compute indices and prepare buffers
    compute_indices_and_factors(forward);
    // Prepare shaders. We construct them as immediate text.
    const char* texfmt = use_half_float ? "rg16f" : "rg32f";
    std::ostringstream oss;
    // note! std140 alignment will align array of vec2 to vec4! For std430
    // alignment this is hopefully better...
    oss << "#version 430 core\n"
        << "layout(binding = 0, " << texfmt << ") readonly uniform image2D input_values;\n"
        << "layout(binding = 1, " << texfmt << ") writeonly uniform image2D output_values;\n"
        << "layout(local_size_x = " << local_size << ", local_size_y = " << local_size << ") in;\n"
        << "layout(std430, binding = 0) readonly buffer index_data { uint "
           "indices["
        << fft_size << " * 2]; } id;\n"
        << "layout(std430, binding = 1) readonly buffer factor_data { vec2 "
           "factors["
        << fft_size << "]; } fc;\n"
        << "vec2 complex_mul(vec2 x, vec2 y) { return vec2(x.x * y.x - x.y * "
           "y.y, x.y * y.x + x.x * y.y); }\n"
        << "void main() {\n"
        << "ivec2 coords = ivec2(gl_GlobalInvocationID.x, "
           "gl_GlobalInvocationID.y);\n"; // Read the image data with input
                                          // indices
    std::string code_begin = oss.str();
    // Now row/column specific part
    std::string code_rows    = "ivec2 coords_a = ivec2(id.indices[2 * gl_GlobalInvocationID.x + 0], "
                               "gl_GlobalInvocationID.y);\n"
                               "ivec2 coords_b = ivec2(id.indices[2 * gl_GlobalInvocationID.x + 1], "
                               "gl_GlobalInvocationID.y);\n"
                               "vec2 factor = fc.factors[gl_GlobalInvocationID.x];\n";
    std::string code_columns = "ivec2 coords_a = ivec2(gl_GlobalInvocationID.x, id.indices[2 * "
                               "gl_GlobalInvocationID.y + 0]);\n"
                               "ivec2 coords_b = ivec2(gl_GlobalInvocationID.x, id.indices[2 * "
                               "gl_GlobalInvocationID.y + 1]);\n"
                               "vec2 factor = fc.factors[gl_GlobalInvocationID.y];\n";
    // Now common code
    std::string code_common = "vec2 input_a = imageLoad(input_values, coords_a).xy;\n"
                              "vec2 input_b = imageLoad(input_values, coords_b).xy;\n"
                              "vec2 outval = input_a + complex_mul(input_b, factor);\n";
    // Now possible output transformation
    std::string code_output_transform;
    if (forward) {
        std::ostringstream ot;
        ot << " outval = outval * (1.0/" << fft_size << ");\n";
        code_output_transform = ot.str();
    }
    // Now common end code
    std::string code_end = "imageStore(output_values, coords, vec4(outval, 0.0, 0.0));\n"
                           "}\n";

    // Create the programs and initialize compute contexts
    cc_fft_columns.add(0, workspace);
    cc_fft_columns.add(1, temp_workspace);
    cc_fft_columns.add(0, index_buffers[0]);
    cc_fft_columns.add(1, factor_buffers[0]);
    cc_fft_columns.add(shader(code_begin + code_columns + code_common + code_end, shader::type::compute, true));
    cc_fft_columns.set_compute_size(fft_size / local_size, fft_size / local_size, 1);
    cc_fft_rows.add(0, workspace);
    cc_fft_rows.add(1, temp_workspace);
    cc_fft_rows.add(0, index_buffers[0]);
    cc_fft_rows.add(1, factor_buffers[0]);
    cc_fft_rows.add(shader(code_begin + code_rows + code_common + code_end, shader::type::compute, true));
    cc_fft_rows.set_compute_size(fft_size / local_size, fft_size / local_size, 1);

    // Create specialized contexts for last level (with output transformation)
    cc_fft_columns_LL.add(0, workspace);
    cc_fft_columns_LL.add(1, temp_workspace);
    cc_fft_columns_LL.add(0, index_buffers[0]);
    cc_fft_columns_LL.add(1, factor_buffers[0]);
    cc_fft_columns_LL.add(shader(
        code_begin + code_columns + code_common + code_output_transform + code_end, shader::type::compute, true));
    cc_fft_columns_LL.set_compute_size(fft_size / local_size, fft_size / local_size, 1);
    cc_fft_rows_LL.add(0, workspace);
    cc_fft_rows_LL.add(1, temp_workspace);
    cc_fft_rows_LL.add(0, index_buffers[0]);
    cc_fft_rows_LL.add(1, factor_buffers[0]);
    cc_fft_rows_LL.add(
        shader(code_begin + code_rows + code_common + code_output_transform + code_end, shader::type::compute, true));
    cc_fft_rows.set_compute_size(fft_size / local_size, fft_size / local_size, 1);
}

void compute_FFT::compute()
{
    // Fastest result with local group size of 16.
    // Geforce 610: ~100ms for 1024 fft. Now 67ms for 1024 with half float. We
    // need at least 256 fft, should be <4ms and fast enough With 512 fft frame
    // <= 16ms. Computation is memory bound. Memory access order is not the
    // problem. It seems we can't go faster with compute shaders than this, nor
    // with fragment shaders. Maybe higher radix kernels can reduce memory
    // accesses (needs fewer output writes, but more index reads) Memory
    // bandwidth could be decreased by using half float textures. (On GT580 from
    // 400ms to 350ms, but there drawing already takes much bandwidth?!) So it
    // helps We can give output scaling (to equalize results) on last run. If we
    // compute inverse transform (frequencies to real coordinates) we don't need
    // the upper half of frequencies (too high), this could save some
    // computation steps.

    // Run compute shaders for rows
    for (unsigned level = 0; level + 1 < fft_size_log2; ++level) {
        cc_fft_rows.add(0, workspace);
        cc_fft_rows.add(1, temp_workspace);
        cc_fft_rows.add(0, index_buffers[level]);
        cc_fft_rows.add(1, factor_buffers[level]);
        cc_fft_rows.compute();
        workspace.swap(temp_workspace);
        cc_fft_rows.wait_for_output();
    }
    cc_fft_rows_LL.add(0, workspace);
    cc_fft_rows_LL.add(1, temp_workspace);
    cc_fft_rows_LL.add(0, index_buffers.back());
    cc_fft_rows_LL.add(1, factor_buffers.back());
    cc_fft_rows_LL.compute();
    workspace.swap(temp_workspace);
    cc_fft_rows_LL.wait_for_output();
    // Run compute shaders for columns
    for (unsigned level = 0; level + 1 < fft_size_log2; ++level) {
        cc_fft_columns.add(0, workspace);
        cc_fft_columns.add(1, temp_workspace);
        cc_fft_columns.add(0, index_buffers[level]);
        cc_fft_columns.add(1, factor_buffers[level]);
        cc_fft_columns.compute();
        workspace.swap(temp_workspace);
        cc_fft_columns.wait_for_output();
    }
    cc_fft_columns_LL.add(0, workspace);
    cc_fft_columns_LL.add(1, temp_workspace);
    cc_fft_columns_LL.add(0, index_buffers.back());
    cc_fft_columns_LL.add(1, factor_buffers.back());
    cc_fft_columns_LL.compute();
    workspace.swap(temp_workspace);
    cc_fft_columns_LL.wait_for_output();
}

std::vector<unsigned> compute_FFT::compute_reversed_bits_indices(unsigned exponent)
{
    const unsigned size = 1 << exponent;
    std::vector<unsigned> result(size);
    for (unsigned i = 0; i < size; ++i) {
        unsigned v = 0;
        for (unsigned k = 0; k < exponent; ++k) {
            v |= ((i >> (exponent - 1 - k)) & 1) << k;
        }
        result[i] = v;
    }
    return result;
}

void compute_FFT::compute_indices_and_factors(bool forward)
{
    index_buffers.resize(fft_size_log2);
    factor_buffers.resize(fft_size_log2);
    // We can store it as 2D table for the shader, so we won't need to rebind
    // uniform buffers, on the other hand we need to give an uniform number
    // which buffer part to use, so just binding a different buffer may be
    // faster. for the first level we need to read from reversed bit indices!
    // but be careful, u values are not reversed...
    // Maybe indices can be computed for levels 1... with clever bit arithmetic
    // in shader and that would be faster?
    vector2 c(-1.0, 0.0);
    auto reversed_bit_indices = compute_reversed_bits_indices(fft_size_log2);
    static const vector2f one(1.f, 0.f);
    for (unsigned level = 0; level < fft_size_log2; ++level) {
        std::vector<uint32_t> fft_indices(fft_size * 2);
        std::vector<vector2f> fft_factors(fft_size);
        unsigned l1 = 1 << level;
        unsigned l2 = 2 << level;
        // We compute in double for best precision, later store only float.
        vector2 u(1.0, 0.0);
        for (unsigned j = 0; j < l1; ++j) {
            float ux = float(u.x), uy = float(u.y);
            for (unsigned i0 = j; i0 < fft_size; i0 += l2) {
                unsigned i1 = i0 + l1;
                // store indices i,i1 to i and i1
                // Computation is done this way:
                // out[i0] = in[i0] + u * in[i1] with complex multiplications
                // out[i1] = in[i0] - u * in[i1] with complex multiplications
                // This means every output value is a linear combination of
                // input values. We need to store the two indices of the
                // computation source and the two factors.
                unsigned idx0 = i0;
                unsigned idx1 = i1;
                if (level == 0) {
                    idx0 = reversed_bit_indices[i0];
                    idx1 = reversed_bit_indices[i1];
                }
                fft_indices[2 * i0 + 0] = idx0;
                fft_indices[2 * i0 + 1] = idx1;
                fft_indices[2 * i1 + 0] = idx0;
                fft_indices[2 * i1 + 1] = idx1;
                fft_factors[i0]         = vector2f(ux, uy);
                fft_factors[i1]         = vector2f(-ux, -uy);
            }
            // multiply complex numbers u and c
            u = vector2(u.x * c.x - u.y * c.y, u.x * c.y + u.y * c.x);
        }
        if (forward) {
            c = vector2(sqrt((1.0 + c.x) * 0.5), -sqrt((1.0 - c.x) * 0.5));
        } else {
            c = vector2(sqrt((1.0 + c.x) * 0.5), sqrt((1.0 - c.x) * 0.5));
        }
        index_buffers[level].init(buffer::usage_type::static_draw, fft_indices);
        factor_buffers[level].init(buffer::usage_type::static_draw, fft_factors);
    }
}

program gpu::make(const shader_source_helper& ssh)
{
    std::string vs = "#version 430\n" + ssh.vertex_defs + "void main() {\n" + ssh.vertex_code + "}\n";
    std::string fs = "#version 430\n" + ssh.fragment_defs + "void main() {\n" + ssh.fragment_code + "}\n";
    return program(shader(vs, shader::type::vertex, true), shader(fs, shader::type::fragment, true));
}

shader_source_helper gpu::get_noise_shader_data(
    unsigned resolution,
    unsigned base_factor,
    unsigned nr_of_levels,
    unsigned tex_unit,
    unsigned offset_slot,
    float amplitude_scale)
{
    shader_source_helper ssh;
    std::ostringstream oss;
    oss << "layout(binding = " << tex_unit
        << ") uniform sampler2D tex_noise;\n"
           "layout(std140, binding = "
        << offset_slot << ") uniform inp { vec4 offsets[" << nr_of_levels
        << "]; }; \n" // only vec2, but alignment...
           "float noise_value(const vec2 coord) { \n"
           "return \n";
    // Hardcode the amplitudes and scales per level!
    float amplitude = 0.5f;
    for (unsigned level = 0; level < nr_of_levels; ++level) {
        oss << "texture(tex_noise, offsets[" << level << "].xy + coord * " << float(base_factor << level) / resolution
            << ").x * " << amplitude;
        amplitude *= amplitude_scale;
        if (level + 1 < nr_of_levels) {
            oss << " + \n";
        } else {
            oss << "; \n } \n";
        }
    }
    ssh.fragment_defs = oss.str();
    return ssh;
}

namespace
{
auto add_ubo_def(basic_shader_uniform_location bsul, const char* name, const char* add_defs = "")
{
    return std::string("layout(std140, binding = ") + helper::str(unsigned(bsul)) + add_defs + ") uniform " + name
           + " {\n";
}
} // namespace

std::string transform_data::get_definition()
{
    return add_ubo_def(
               basic_shader_uniform_location::transform,
               "transform_data",
               ", row_major")
           +                                 // declaration of row_major is essential!
           "  mat4 projection_modelview; \n" // needed for transformation
           "  mat4 modelview_inverse; \n"    // for lighting
           "} transform; \n";
}

std::string light_data::get_definition()
{
    return add_ubo_def(basic_shader_uniform_location::light, "light_data")
           + "  vec4 position; \n"
             "  vec3 color; \n"
             "  float ambient_factor; \n"
             "} light; \n";
}

std::string fog_data::get_definition()
{
    return add_ubo_def(basic_shader_uniform_location::fog, "fog_data")
           + "  vec3 color;\n"
             "  float density;\n"
             "} fog;\n";
}

std::string material_data::get_definition()
{
    return add_ubo_def(basic_shader_uniform_location::material, "material_data")
           + "  vec3 specular_color; \n"
             "  float shininess; \n"
             "  vec4 common_color; \n"
             "} material; \n";
}

std::string clipplane_data::get_definition()
{
    return add_ubo_def(basic_shader_uniform_location::clipplane, "clipplane_data")
           + "  vec4 clipplane; \n"
             "}; \n";
}

shader_source_helper gpu::generate_basic_shader_source(basic_shader_feature bsf)
{
    shader_source_helper result;
    auto add_v2f_param = [&result](const char* type_and_name, const char* qualifier = "") {
        result.vertex_defs += qualifier + std::string(" out ") + type_and_name + ";\n";
        result.fragment_defs += qualifier + std::string(" in ") + type_and_name + ";\n";
    };
    auto use = [&bsf](basic_shader_feature b) {
        return (int(bsf) & int(b)) != 0;
    };

    // fixme mirrorclip: just clip with some plane? modelviewmatrix has
    // determinate -1, a problem for rendering?? just give the mirrored matrix?
    // define plane in what space? projection space?! does that work?!

    // With lighting as option we can use the shader then for basic drawing like
    // 2D widgets.

    auto add_vattr_def = [](basic_shader_attribute_location bsal, const char* type_and_name) {
        return std::string("layout(location = ") + helper::str(unsigned(bsal)) + ") in " + type_and_name + "; \n";
    };
    auto add_tex_def = [](basic_shader_sampler_location bssl, const char* name) {
        return std::string("layout(binding = ") + helper::str(unsigned(bssl)) + ") uniform sampler2D " + name + "; \n";
    };

    // fragment shader always gives a color with alpha value.
    result.fragment_defs += "out vec4 frag_color; \n";

    // Position and transformation - Vertex shader always gets a position and
    // transforms it.
    result.vertex_defs +=
        add_vattr_def(basic_shader_attribute_location::position, "vec3 position") + transform_data::get_definition();
    result.vertex_code += "gl_Position = transform.projection_modelview * "
                          "vec4(position, 1.0); \n";

    // clip plane (optional)
    // Check first, so fragment discard is first fragment code!
    if (use(basic_shader_feature::clipplane)) {
        add_v2f_param("float clipplane_distance");
        result.vertex_defs += clipplane_data::get_definition();
        result.vertex_code += "clipplane_distance = dot(clipplane.xyz, position) + clipplane.w; "
                              "\n"; // same function as for caustics...
        result.fragment_code += "if (clipplane_distance < 0.0) discard; \n";
    }

    // Material definitions for lighting or if no colormap nor vertex colors
    if (use(basic_shader_feature::lighting)
        || (!use(basic_shader_feature::colormap) && !use(basic_shader_feature::vertex_color))) {
        result.fragment_defs += material_data::get_definition();
    }

    // Lighting (optional)
    if (use(basic_shader_feature::lighting)) {
        add_v2f_param("vec3 lightdir", "noperspective");  // directions should be
                                                          // interpolated noperspective!
        add_v2f_param("vec3 halfangle", "noperspective"); // same here.
        auto light_def = light_data::get_definition()
                         + "const vec3 light_color = light.color; \n"
                           "const float light_ambient_factor = light.ambient_factor; \n";
        // data used in both shaders.
        result.vertex_defs += light_def;
        result.fragment_defs += light_def;
        // we need material values then
        // We need normals for lighting.
        result.vertex_defs += add_vattr_def(basic_shader_attribute_location::normal, "vec3 vnormal");
        result.vertex_code +=
            // compute direction to light in object space (L)
            // light.position.w is 0 or 1, 0 for directional light, 1 for point
            // light
            "const vec3 lightpos_obj = vec3(transform.modelview_inverse * "
            "light.position); \n"
            "const vec3 lightdir_obj = normalize(lightpos_obj - position * "
            "light.position.w); \n"
            // compute direction to viewer (E) in object space (mvinv*(0,0,0,1)
            // - inputpos)
            "const vec3 viewerdir_obj = "
            "normalize(vec3(transform.modelview_inverse[3]) - position); \n"
            // compute halfangle vector (H = ||L+E||)
            "const vec3 halfangle_obj = normalize(viewerdir_obj + "
            "lightdir_obj); \n";
        result.fragment_code +=
            // get and normalize light direction and half angle
            "const vec3 L = normalize(lightdir); \n"
            "const vec3 H = normalize(halfangle); \n";
    }

    // texcoords
    if (use(basic_shader_feature::colormap) || use(basic_shader_feature::normalmap)
        || use(basic_shader_feature::specularmap) || use(basic_shader_feature::underwater)) {
        add_v2f_param("vec2 texcoord0");
        result.vertex_defs += add_vattr_def(basic_shader_attribute_location::texcoord, "vec2 texcoord");
        result.vertex_code += "texcoord0 = texcoord; \n";
    }

    // diffuse color
    if (use(basic_shader_feature::colormap)) {
        result.fragment_defs += add_tex_def(basic_shader_sampler_location::color_map,
                                            "tex_color"); // 3 channels
        result.fragment_code += "const float alpha = 1.0; \n"
                                "const vec3 material_color = vec3(texture(tex_color, texcoord0)); "
                                "\n"; // no alpha channel on colormaps
    } else if (use(basic_shader_feature::vertex_color)) {
        add_v2f_param("vec4 color");
        result.vertex_defs += add_vattr_def(basic_shader_attribute_location::color, "vec4 vcolor");
        result.vertex_code += "color = vcolor; \n";
        result.fragment_code += "const float alpha = color.w; \n"
                                "const vec3 material_color = color.xyz; \n";
    } else {
        // use global color
        result.fragment_code += "const float alpha = material.common_color.w; \n"
                                "const vec3 material_color = material.common_color.xyz; \n";
    }

    // normal maps
    if (use(basic_shader_feature::normalmap)) {
        if (!use(basic_shader_feature::lighting)) {
            THROW(error, "normalmaps without lighting not sensible");
        }
        result.vertex_defs += add_vattr_def(basic_shader_attribute_location::tangentx, "vec3 tangentx")
                              + add_vattr_def(
                                  basic_shader_attribute_location::righthanded,
                                  "int righthanded"); // stored as u8, no space wasted. type bool
                                                      // isn't accepted by OpenGL.
        result.vertex_code +=
            // compute tangenty, tangentz
            "const vec3 tangenty = cross(vnormal, tangentx) * (righthanded != "
            "0 ? 1.0 : -1.0); \n"
            "const vec3 tangentz = vnormal; \n"
            // transform light direction to tangent space
            "lightdir.x = dot(tangentx, lightdir_obj); \n"
            "lightdir.y = dot(tangenty, lightdir_obj); \n"
            "lightdir.z = dot(tangentz, lightdir_obj); \n"
            "halfangle.x = dot(tangentx, halfangle_obj); \n"
            "halfangle.y = dot(tangenty, halfangle_obj); \n"
            "halfangle.z = dot(tangentz, halfangle_obj); \n";
        result.fragment_defs += add_tex_def(basic_shader_sampler_location::normal_map,
                                            "tex_normal"); // 3 channels
        result.fragment_code +=
            // get and normalize normal vector from texmap
            "const vec3 N = normalize(vec3(texture(tex_normal, texcoord0)) * "
            "2.0 - 1.0); \n";
    } else if (use(basic_shader_feature::lighting)) {
        add_v2f_param("vec3 normal");
        result.vertex_code += "lightdir = lightdir_obj; \n"
                              "halfangle = halfangle_obj; \n"
                              "normal = vnormal; \n";
        // normalize normal interpolated between vertices
        result.fragment_code += "const vec3 N = normalize(normal);\n";
    }

    // Now we have a vec3 material_color and need to output a vec3
    // combined_color depending on lighting configuration. The ambient is a
    // factor of the light source defining a minimum brightness. So ambient is
    // computed together with brightness from light source as factor of diffuse
    // color. Specular color is a material property and can have a different
    // color than the diffuse color.
    if (use(basic_shader_feature::lighting)) {
        if (use(basic_shader_feature::specularmap)) {
            result.fragment_defs += add_tex_def(
                basic_shader_sampler_location::specular_map,
                "tex_specular"); // 1 channel
            result.fragment_code += "const float specular_factor = "
                                    "texture(tex_specular, texcoord0).x; \n";
        } else {
            result.fragment_code += "const float specular_factor = 1.0; \n";
        }
        result.fragment_code += "const float brightness_light = clamp(dot(L, N), 0.0, 1.0); \n"
                                "const float brightness_material = mix(brightness_light, 1.0, "
                                "light_ambient_factor); \n"
                                "const vec3 diffuse_color = material_color * brightness_material; "
                                "\n" // also contains ambient part
                                "const vec3 specular_color = material.specular_color * "
                                "(pow(clamp(dot(H, N), 0.0, 1.0), material.shininess) * "
                                "specular_factor); \n"
                                "const vec3 combined_color = (diffuse_color + specular_color) * "
                                "light_color; \n";
    } else {
        // Without lightning direct transition
        result.fragment_code += "const vec3 combined_color = material_color;\n";
    }

    // handling of color modfication by fog (under water: special fog)
    if (use(basic_shader_feature::underwater) || use(basic_shader_feature::fog)) {
        result.fragment_defs += fog_data::get_definition();
        add_v2f_param("float fog_frag_coord");
        result.vertex_code += "fog_frag_coord = gl_Position.z;\n"; // could also take distance
                                                                   // from camera (without
                                                                   // projection matrix!)
        // our fog is exponential fog.
        result.fragment_code += "const float fog_factor = clamp(exp2(-fog.density * "
                                "fog_frag_coord), 0.0, 1.0); \n";
    }
    if (use(basic_shader_feature::underwater)) {
        // mix in caustics, use special fog code
        add_v2f_param("vec2 caustic_texcoord");
        result.vertex_defs += "const vec4 plane_s = vec4(0.05, 0.0, 0.03, 0.0); \n"
                              "const vec4 plane_t = vec4(0.0, 0.05, 0.03, 0.0); \n"
                              "float calculate_caustic_coords(const vec3 pos, const vec4 plane) "
                              "{ return dot(pos, plane.xyz) + plane.w; } \n"; // this a function
                                                                              // to compute
                                                                              // distance to a
                                                                              // plane
        result.fragment_defs += add_tex_def(basic_shader_sampler_location::caustics_map,
                                            "tex_caustic"); // 1 channel
        result.vertex_code += "caustic_texcoord = vec2(calculate_caustic_coords(position, "
                              "plane_s), calculate_caustic_coords(position, plane_t)); \n";
        result.fragment_code += "const vec3 combined_caustic_color = combined_color * "
                                "max(texture(tex_caustic, caustic_texcoord).x *2.0, 0.5); \n"
                                "const vec3 fog_color = fog.color * clamp(150.0 / fog_frag_coord, "
                                "0.0, 1.0); \n"
                                "frag_color = vec4(mix(fog_color, combined_caustic_color.xyz, "
                                "fog_factor), alpha); \n";
    } else if (use(basic_shader_feature::fog)) {
        // normal fog code (linear fog)
        result.fragment_code += "frag_color = vec4(mix(fog.color, "
                                "combined_color.xyz, fog_factor), alpha); \n";
    } else {
        // direct transfer of color to output
        result.fragment_code += "frag_color = vec4(combined_color, alpha); \n";
    }
    return result;
}

#if 0
// draw_image
// fixme this are all 2d drawing functions that don't need modelview matrices, only kind of scaling, can be rendered with simple shaders and use culling face_render_side::both for 2d! tiles are just a texcoord manipulation!
// maybe put to default_stuff together with one program that handles all of this
// draw is never used with color!
void texture::draw_hm(int x, int y, const colorf& col) const
{
    draw_hm(x, y, width, height, col);
}

void texture::draw(int x, int y, int w, int h, const colorf& col) const
{
    float u = float(width)/gl_width;
    float v = float(height)/gl_height;
    primitives::textured_quad(vector2f(x,y),vector2f(x+w,y+h), *this,
                  vector2f(0,0),vector2f(u,v), col).render(SYS().get_modelview_matrix_2d());//fixme geht so nicht, braucht matrix von extern!!!! wird von draw_Rot aufgerufen! oder 2 funktionen, einmal so, einmal so
}

void texture::draw_hm(int x, int y, int w, int h, const colorf& col) const
{
    float u = float(width)/gl_width;
    float v = float(height)/gl_height;
    primitives::textured_quad(vector2f(x,y),vector2f(x+w,y+h), *this,
                  vector2f(u,0),vector2f(0,v), col).render(SYS().get_modelview_matrix_2d());
}

void texture::draw_rot(int x, int y, double angle, const colorf& col) const
{
    draw_rot(x, y, angle, get_width()/2, get_height()/2, col);
}

void texture::draw_rot(int x, int y, double angle, int tx, int ty, const colorf& col) const
{
    glPushMatrix();
    glTranslatef(x, y, 0);
    glRotatef(angle, 0, 0, 1);
    draw(-tx, -ty, col);
    glPopMatrix();
}

void texture::draw_tiles(int x, int y, int w, int h, const colorf& col) const
{
    float tilesx = float(w)/gl_width;
    float tilesy = float(h)/gl_height;
    primitives::textured_quad(vector2f(x,y),vector2f(x+w,y+h), *this,
                  vector2f(0,0),vector2f(tilesx,tilesy), col).render(SYS().get_modelview_matrix_2d());
}

void texture::draw_subimage(int x, int y, int w, int h, unsigned tx, unsigned ty,
                unsigned tw, unsigned th, const colorf& col) const
{
    float x1 = float(tx)/gl_width;
    float y1 = float(ty)/gl_height;
    float x2 = float(tx+tw)/gl_width;
    float y2 = float(ty+th)/gl_height;
    primitives::textured_quad(vector2f(x,y),vector2f(x+w,y+h), *this,
                  vector2f(x1,y1),vector2f(x2,y2), col).render(SYS().get_modelview_matrix_2d());
}

#endif

#if 0
    /// render polygon
    void draw(const gpu::camera& scene_cam, const color& col = color::white()) const
    {
        std::vector<vector3f> posns(points.size());
        for (unsigned i = 0; i < nr_of_points(); ++i)
            posns[i] = vector3f(points[i]);
        GPU_DRAW().line_strip(scene_cam, posns, col);
    }
    /// Render a polyhedron for debugging purposes
    void draw(const gpu::camera& scene_cam, float alpha = 1.0f)
    {
        std::vector<vector3f> positions;
        std::vector<color> colors;
        unsigned color_counter = 1;
        uint8_t a = uint8_t(255 * alpha);
        for (const auto& s : sides) {
            color col;
            col.r = 255 * (color_counter & 1);
            col.g = 255 * ((color_counter & 2) / 2);
            col.b = 255 * ((color_counter & 4) / 4);
            col.a = a;
            if (color_counter == 0) {
                col.r = 255;
                col.g = 128;
                col.b = 0;
            }
            for (unsigned i = 2; i < unsigned(s.edges.size()); ++i) {
                positions.push_back(points[s.edges.front().point_nr]);
                positions.push_back(points[s.edges[i - 1].point_nr]);
                positions.push_back(points[s.edges[i].point_nr]);
                colors.push_back(col);
                colors.push_back(col);
                colors.push_back(col);
            }
            ++color_counter;
        }
        GPU_DRAW().colored_triangles(scene_cam, positions, colors);
    }
#endif
