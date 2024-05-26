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

// a coastmap
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#pragma once

#include "bspline.hpp"
#include "object_store.hpp"
#include "texture.hpp"
#include "thread.hpp"
#include "vector2.hpp"
#include "vector3.hpp"

#include <list>
#include <string>
#include <utility>
#include <vector>
class model;

///\brief Handles a segment of the map represented by class coastmap.
class coastsegment
{
  public:
    using segpos = vector2t<unsigned short>;

    class segcl
    {
      public:
        int global_clnr{-1}; // created from which global cl? internal use.

        std::vector<segpos> points; // coordinates of the segcl. relative to segment.

        mutable std::vector<vector2> points2; // cached, real world per segment coordinates.
        mutable std::vector<vector2> normals; // cached, coastline normals.

        int beginpos{-1}; // positions are on border in 0-64k scale: s=65535.
                          // (-1 = not on border)

        int endpos{-1}; // then bottom,right,top,left border of segment are
                        // 0s+x, 1s+x, 2s+x, 3s+x

        int next{-1};       // successor of this cl. is itself for cyclic segcl's.
        bool cyclic{false}; // is segcl cyclic inside this segment (island)?
        void print() const; // for debugging

        segcl(int glcn = -1)
            : global_clnr(glcn)
        {
        }
        void push_back_point(const segpos& sp); // avoids double points
    };

    unsigned type{0}; // 0 - sea, 1 - land, 2 mixed
    std::vector<segcl> segcls;
    class texture* atlanticmap{nullptr};

    // terrain elevation (no matter if land or sea, total elevation in meters)
    // user for computation of water depth or terrain height (not yet)
    // bspline2dt<float> topo;

    // cache generated points.
    struct cacheentry
    {
        std::vector<vector2> points; // that is a 2d mesh, real world
                                     // coordinates relative to segm. offset
        std::vector<unsigned> indices;
        void push_back_point(const vector2& p); // avoids double points.
    };

    mutable int pointcachedetail{0};
    mutable std::vector<cacheentry> pointcache;

    // check if cache needs to be (re)generated, and do that
    void generate_point_cache(const class coastmap& cm, int x, int y, int detail) const;

    void compute_successor_for_cl(unsigned cln);

    void push_back_segcl(const segcl& scl); // avoids segcls with < 2 points.

    coastsegment(/*unsigned topon, const std::vector<float>& topod*/) = default;

    void draw_as_map(const class coastmap& cm, int x, int y, int detail = 0) const;
};

///\brief Handles a 2D map of coastlines or terrain with 3D rendering.
class coastmap
{
    friend class coastsegment;        // just request some values
    friend class coastsegment::segcl; // just request some values

    // some attributes used for map reading/processing
    std::vector<uint8_t> themap; // map file pixel data, y points up as OpenGL

    static const int dmx[4]; // some helper constants.
    static const int dmy[4];
    static const int dx[4];
    static const int dy[4];
    static const int runlandleft[16];
    static const int runlandright[16];
    static bool patternprocessok[16];

    unsigned pixels_per_seg;      // "n"
    unsigned mapw, maph;          // map width/height in pixels.
    unsigned segsx, segsy;        // nr of segs in x/y dimensions
    double realwidth, realheight; // width/height of map in reality (meters)
    double pixelw_real;           // width/height of one pixel in reality, in meters
    double segw_real;             // width/height of one segment in reality, in meters
    vector2 realoffset;           // offset in meters for map (position of pixel pos 0,0)
    std::vector<coastsegment> coastsegments;

    int global_clnr; // working counter.

    // city positions (real) and names
    std::vector<std::pair<vector2, std::string>> cities;

    // special 3d objects on map (rather a hack)
    struct prop
    {
        std::string modelname;
        vector2 pos;
        double dir;
        prop(std::string s, vector2 p, double d)
            : modelname(std::move(s))
            , pos(std::move(p))
            , dir(d)
        {
        }
    };
    std::vector<prop> props;

    std::unique_ptr<texture> atlanticmap;

    object_store<model>& model_store;

    coastmap()                           = delete;
    coastmap(const coastmap&)            = delete;
    coastmap& operator=(const coastmap&) = delete;

    // very fast integer clamping (no branch needed, only for 32bit signed
    // integers!)
    int32_t clamp_zero(int32_t x) { return x & ~(x >> 31); }
    uint8_t& mapf(int cx, int cy);

    // compute position on border (0...4*(2^16-1)-1) or -1 if not on border
    // give border number (-1,0...3) and position in segment.
    [[nodiscard]] static int borderpos(const coastsegment::segpos& p);

    // returns false for normal cl, true for islands/lakes.
    bool find_begin_of_coastline(int& x, int& y);
    bool find_coastline(int x, int y, std::vector<vector2i>& points, bool& cyclic);

    [[nodiscard]] static vector2i compute_segment(const vector2i& p0, const vector2i& p1);

    void divide_and_distribute_cl(const std::vector<vector2i>& cl, bool clcyclic);

    void process_coastline(int x, int y);
    void process_segment(int x, int y);

    std::unique_ptr<::thread> myworker;
    void construction_threaded();

  public:
    // returns quadrant of vector d (0: - 0 degr, 1: - ]0...90[ degr, 2 - 90
    // degr ... 7: ..360[ degr.)
    static unsigned quadrant(const vector2i& d);

    [[nodiscard]] vector2 segcoord_to_real(int segx, int segy, const coastsegment::segpos& sp) const;
    [[nodiscard]] vector2f segcoord_to_texc(int segx, int segy) const;

    /// create from xml file
    coastmap(const std::string& filename, object_store<model>& model_store_);
    ~coastmap();

    /// MUST be called after construction of coastmap and before using it!
    void finish_construction();

    [[nodiscard]] const auto& get_city_list() const { return cities; }

    // fixme: maybe it's better to give top,left and bottom,right corner of sub
    // area to draw
    void draw_as_map(const vector2& droff, double mapzoom, int detail = 0) const;

    // p is real word position of viewer, vr is range of view in meters.
    void render(const vector2& p, double vr, bool mirrored, int detail = 0, bool withterraintop = false) const;
};
