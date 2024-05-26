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

// particle (C)+(W) 2004 Thorsten Jordan

#pragma once

#include "color.hpp"
#include "vector3.hpp"

#include <vector>

class game;
class texture;

// particles: smoke, water splashes, fire, explosions, spray caused by ship's
// bow fire particles can produce smoke particles!

using uint8_t = unsigned char;

///\brief Simulates and displays particles that are rendered as billboard
/// images.
class particle
{
  protected:
    vector3 position;
    vector3 velocity;
    double life{1.0}; // 0...1, 0 = faded out
    particle() = default;
    particle(const particle& other);
    particle& operator=(const particle& other);

    // returns wether particle is shown parallel to z-axis (true), or 3d
    // billboarding always (false)
    [[nodiscard]] virtual bool is_z_up() const { return true; }

    // returns wether image should be drawn above pos or centered around pos
    [[nodiscard]] virtual bool tex_centered() const { return true; }

    // helper struct for depth sorting
    struct particle_dist
    {
        const particle* pt;
        double dist;
        vector3 projpos;
        particle_dist(const particle* p, double d, const vector3& pp)
            : pt(p)
            , dist(d)
            , projpos(pp)
        {
        }
        bool operator<(const particle_dist& other) const { return dist > other.dist; }
    };

    // particle textures (generated and stored once)
    // fixme: why not use texture_cache here?
    static unsigned init_count;
    static std::vector<texture*> tex_smoke;
    static texture* tex_spray;
    static std::vector<texture*> tex_fire;
    static std::vector<texture*> explosionbig;
    static std::vector<texture*> explosionsml;
    static std::vector<texture*> watersplashes;
    static texture* tex_fireworks;
    static texture* tex_fireworks_flare;
    static texture* tex_marker;

    // wh must be power of two (returns a square). 1 <= 2^low <= 2^high <= wh
    static std::vector<float> interpolate_func;
    static std::vector<uint8_t> make_2d_smoothed_noise_map(unsigned wh);
    static unsigned
    interpolate_2d_map(const std::vector<uint8_t>& mp, unsigned res, unsigned x, unsigned y, unsigned res2);

    // 1 <= highest_level <= log2(wh)
    static std::vector<uint8_t> make_2d_perlin_noise(unsigned wh, unsigned highestlevel);
    static std::vector<uint8_t> compute_fire_frame(unsigned wh, const std::vector<uint8_t>& oldframe);

    [[nodiscard]] virtual vector3 get_acceleration() const { return {}; }

    /// must this type of particle be rendered specially?
    [[nodiscard]] virtual bool has_custom_rendering() const { return false; }
    /// renders a particle in custom way giving vectors parallel to screen's xy
    /// plane
    virtual void custom_display(const vector3& viewpos, const vector3& dx, const vector3& dy) const { }

  public:
    particle(const vector3& pos, const vector3& velo = vector3())
        : position(pos)
        , velocity(velo)
    {
    }
    virtual ~particle() = default;

    static void init();
    static void deinit();

    [[nodiscard]] virtual const vector3& get_pos() const { return position; }
    virtual void set_pos(const vector3& pos) { position = pos; }

    // class game is given so that particles can spawn other particles
    // (fire->smoke)
    virtual void simulate(game& gm, double delta_t);

    static void
    display_all(const std::vector<const particle*>& pts, const vector3& viewpos, game& gm, const colorf& light_color);

    // return width/height (in meters) of particle (length of quad edge)
    [[nodiscard]] virtual double get_width() const  = 0;
    [[nodiscard]] virtual double get_height() const = 0;

    virtual void kill() { life = 0.0; }
    [[nodiscard]] virtual bool is_dead() const { return life <= 0.0; }

    // set opengl texture by particle type or e.g. game time etc.
    virtual const texture& get_tex_and_col(game& gm, const colorf& light_color, colorf& col) const = 0;

    [[nodiscard]] virtual double get_life_time() const = 0;
};

class smoke_particle : public particle
{
    [[nodiscard]] bool is_z_up() const override { return false; }
    unsigned texnr;
    [[nodiscard]] vector3 get_acceleration() const override;

  public:
    smoke_particle(const vector3& pos); // set velocity by wind, fixme
    [[nodiscard]] double get_width() const override;
    [[nodiscard]] double get_height() const override;
    const texture& get_tex_and_col(game& gm, const colorf& light_color, colorf& col) const override;
    [[nodiscard]] double get_life_time() const override;
    static double get_produce_time();
};

class smoke_particle_escort : public smoke_particle
{
  public:
    smoke_particle_escort(const vector3& pos); // set velocity by wind, fixme
    [[nodiscard]] double get_width() const override;
    [[nodiscard]] double get_life_time() const override;
    static double get_produce_time();
};

class explosion_particle : public particle
{
    unsigned extype; // which texture
  public:
    // is_z_up could be false for this kind of particle
    explosion_particle(const vector3& pos);
    [[nodiscard]] double get_width() const override;
    [[nodiscard]] double get_height() const override;
    const texture& get_tex_and_col(game& gm, const colorf& light_color, colorf& col) const override;
    [[nodiscard]] double get_life_time() const override;
};

class fire_particle : public particle
{
    //	unsigned firetype;	// which texture
  public:
    // only particle where is_z_up should be true.
    fire_particle(const vector3& pos);
    void simulate(game& gm, double delta_t) override;
    [[nodiscard]] double get_width() const override;
    [[nodiscard]] double get_height() const override;
    const texture& get_tex_and_col(game& gm, const colorf& light_color, colorf& col) const override;
    [[nodiscard]] double get_life_time() const override;
};

class spray_particle : public particle
{
  public:
    // is_z_up could be false for this kind of particle
    spray_particle(const vector3& pos, const vector3& velo);
    [[nodiscard]] double get_width() const override;
    [[nodiscard]] double get_height() const override;
    const texture& get_tex_and_col(game& gm, const colorf& light_color, colorf& col) const override;
    [[nodiscard]] double get_life_time() const override;
};

class fireworks_particle : public particle
{
    [[nodiscard]] bool is_z_up() const override { return false; }

    [[nodiscard]] bool has_custom_rendering() const override { return true; }
    void custom_display(const vector3& viewpos, const vector3& dx, const vector3& dy) const override;

    struct flare
    {
        vector2 velocity;
    };

    std::vector<flare> flares;

    void simulate(game& gm, double delta_t) override;
    [[nodiscard]] double get_z(double life_fac) const;

  public:
    fireworks_particle(const vector3& pos);
    [[nodiscard]] double get_width() const override { return 0; }  // not needed
    [[nodiscard]] double get_height() const override { return 0; } // not needed
    const texture& get_tex_and_col(game& gm, const colorf& light_color, colorf& col) const override;
    [[nodiscard]] double get_life_time() const override;
};

class marker_particle : public particle
{
    [[nodiscard]] bool is_z_up() const override { return false; }

  public:
    marker_particle(const vector3& pos);
    [[nodiscard]] double get_width() const override;
    [[nodiscard]] double get_height() const override;
    const texture& get_tex_and_col(game& gm, const colorf& light_color, colorf& col) const override;
    [[nodiscard]] double get_life_time() const override;
};
