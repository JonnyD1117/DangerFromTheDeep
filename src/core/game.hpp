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

// game
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#pragma once

namespace acoustics
{
constexpr auto ping_remain_time      = 1.0;     // seconds
constexpr auto ping_angle            = 15;      // angle
constexpr auto ping_length           = 1000;    // meters. for drawing
constexpr auto asdic_range           = 1500.0;  // meters fixme: historic values?
constexpr auto enemy_contact_lost    = 50000.0; // meters
constexpr auto max_acoustic_contacts = 5;       // max. nr of simultaneous
                                                // trackable acustic contacts
} // namespace acoustics

// TODO: move to terrain
#define TERRAIN_NR_LEVELS    10
#define TERRAIN_RESOLUTION_N 7

#include "random_generator.hpp"
#include "thread.hpp"

#include <condition_variable>
#include <list>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

// use forward declarations to avoid unneccessary compile dependencies
class particle;
class water;
class height_generator;

#include "angle.hpp"
#include "color.hpp"
#include "date.hpp"
#include "event.hpp"
#include "logbook.hpp"
#include "model.hpp"
#include "sensors.hpp"
#include "sonar.hpp"
#include "vector2.hpp"
#include "vector3.hpp"
#include "xml.hpp"

// includes of sea_objects to store them
#include "airplane.hpp"
#include "convoy.hpp"
#include "depth_charge.hpp"
#include "gun_shell.hpp"
#include "particle.hpp"
#include "ship.hpp"
#include "submarine.hpp"
#include "torpedo.hpp"
#include "water_splash.hpp"

// Note! do NOT include user_interface here, class game MUST NOT call any method
// of class user_interface or its heirs.

///\brief Central object of the game world with physics simulation etc.
class game
{
  public:
    // fixme: may be redundant with event_ping !
    struct ping
    {
        vector2 pos;
        angle dir;
        double time;
        double range;
        angle ping_angle;
        ping(const vector2& p, angle d, double t, double range, const angle& ping_angle_)
            : pos(p)
            , dir(d)
            , time(t)
            , range(range)
            , ping_angle(ping_angle_)
        {
        }
        ~ping() = default;
        ping(const xml_elem& parent);
        void save(xml_elem& parent) const;
    };

    struct sink_record
    {
        date dat;
        std::string descr;        // fixme: store type, use a static ship function to
                                  // retrieve a matching description, via specfilename!
        std::string mdlname;      // model file name string
        std::string specfilename; // spec file name (base model name)
        std::string layoutname;   // model skin
        unsigned tons;
        sink_record(date d, std::string s, std::string m, std::string sn, std::string ln, unsigned t)
            : dat(d)
            , descr(std::move(s))
            , mdlname(std::move(m))
            , specfilename(std::move(sn))
            , layoutname(std::move(ln))
            , tons(t)
        {
        }
        sink_record(const xml_elem& parent);
        void save(xml_elem& parent) const;
    };

    struct player_info
    {
        std::string name;
        unsigned flotilla{1};
        std::string submarineid;
        unsigned photo{0};

        std::string soldbuch_nr;
        std::string gasmask_size;
        std::string bloodgroup;
        std::string marine_roll;
        std::string marine_group;
        /* 'cause the career list is linear we do not need to store
         * ranks or paygroups. a list of the dates should be enough
         */
        std::list<std::string> career;

        player_info();
        player_info(const xml_elem& parent);
        void save(xml_elem& parent) const;
    };

    // in which state is the game
    // normal mode (running), or stop on next cycle (reason given by value)
    enum run_state
    {
        running,
        player_killed,
        mission_complete,
        contact_lost
    };

    // time between records of trail positions
    static const double TRAIL_TIME;

  protected:
    // begin [SAVE]
    std::unordered_map<sea_object_id, ship> ships;
    std::unordered_map<sea_object_id, submarine> submarines;
    std::unordered_map<sea_object_id, airplane> airplanes;
    std::vector<torpedo> torpedoes;
    std::vector<depth_charge> depth_charges;
    std::vector<gun_shell> gun_shells;
    std::vector<water_splash> water_splashes;
    std::unordered_map<sea_object_id, convoy> convoys;
    std::vector<std::unique_ptr<particle>> particles;

    sea_object_id next_id;
    sea_object_id generate_id()
    {
        ++next_id.id;
        return next_id;
    }
    // end [SAVE]
    run_state my_run_state;

    std::vector<std::unique_ptr<event>> events;

    // the player (note that playing is not limited to submarines!)
    sea_object* player{nullptr};
    sea_object_id player_id; // [SAVE]

    std::list<sink_record> sunken_ships; // [SAVE]

    logbook players_logbook; // [SAVE]

    double time;            // global time (in seconds since 1.1.1939, 0:0 hrs) (universal
                            // time!) [SAVE]
    double last_trail_time; // for position trail recording	[SAVE]

    date equipment_date; // date that equipment was created. used for torpedo
                         // loading

    enum weathers
    {
        sunny,
        clouded,
        raining,
        storm
    };                    // fixme
    double max_view_dist; // maximum visibility according to weather conditions,
                          // fixme recomputed or save?

    std::list<ping> pings; // [SAVE]

    // time in milliseconds that game is paused between simulation steps.
    // for small pauses to compensate long image loading times
    unsigned freezetime{}, freezetime_start{};

    // water height data, and everything around it.
    std::unique_ptr<water> mywater;

    // terrain height data
    std::unique_ptr<height_generator> myheightgen;

    // helper for simulation
    void simulate_objects(double delta_t, bool record, double& nearest_contact);

    player_info playerinfo;

    /// check objects collide with any other object
    void check_collisions();
    static void collision_response(sea_object& a, sea_object& b, const vector3& collision_pos);

    random_generator_deprecated random_gen;

    object_store<model> model_store;

    game();
    game& operator=(const game& other);
    game(const game& other);

  public:
    // create new custom mission
    // expects: size small,medium,large, escort size none,small,medium,large,
    // time of day [0,4) night,dawn,day,dusk
    game(
        const std::string& subtype,
        unsigned cvsize,
        unsigned cvesc,
        unsigned timeofday,
        const date& timeperioddate,
        player_info pi         = player_info() /*fixme - must be always given*/,
        unsigned nr_of_players = 1);

    // create from mission file or savegame (xml file)
    game(const std::string& filename);

    virtual ~game();

    virtual void save(const std::string& savefilename, const std::string& description) const;
    static std::string read_description_of_savegame(const std::string& filename);

    void compute_max_view_dist(); // fixme - public?
    virtual void simulate(double delta_t);

    const std::list<sink_record>& get_sunken_ships() const { return sunken_ships; };
    const logbook& get_players_logbook() const { return players_logbook; }
    void add_logbook_entry(const std::string& s);
    double get_time() const { return time; };
    date get_date() const { return {unsigned(time)}; };
    date get_equipment_date() const { return equipment_date; }
    double get_max_view_distance() const { return max_view_dist; }
    /**
        This method is needed to verify for day and night mode for the
        display methods within the user interfaces.
        @return true when day mode, false when night mode
    */
    bool is_day_mode() const;
    /**
        This method calculates a depth depending factor. A deep diving
        submarine is harder to detect with ASDIC than a submarine at
        periscope depth.
        @param sub location vector of submarine
        @return depth factor
    */
    virtual double get_depth_factor(const vector3& sub) const;

    sea_object* get_player() const { return player; }
    sea_object_id get_player_id() const { return player_id; }

    double get_last_trail_record_time() const { return last_trail_time; }

    sea_object& get_object(sea_object_id id); // ship,sub,airplane
    ship& get_ship(sea_object_id id);
    convoy& get_convoy(sea_object_id id);
    sea_object_id get_id(const sea_object&) const; // fixme move to editor later!

    // compute visibility data
    // fixme: remove the single functions, they're always called together
    // by visible_sea/visible_surface objects
    // they all map to the same function.
    // if certain objects should not be reported, unmask them with
    // extra-parameter.
    virtual std::vector<const ship*> visible_ships(const sea_object* o) const;
    virtual std::vector<const submarine*> visible_submarines(const sea_object* o) const;
    virtual std::vector<const airplane*> visible_airplanes(const sea_object* o) const;
    virtual std::vector<const torpedo*> visible_torpedoes(const sea_object* o) const;
    virtual std::vector<const depth_charge*> visible_depth_charges(const sea_object* o) const;
    virtual std::vector<const gun_shell*> visible_gun_shells(const sea_object* o) const;
    virtual std::vector<const water_splash*> visible_water_splashes(const sea_object* o) const;
    virtual std::vector<const particle*> visible_particles(const sea_object* o) const;
    // computes visible ships, submarines (surfaced) and airplanes
    virtual std::vector<const sea_object*> visible_surface_objects(const sea_object* o) const;
    // computes ships, subs (surfaced), airplanes, torpedoes. But not fast
    // moving objects like shells/DCs, because they need to be detected more
    // often, and this function is called once per second normally.
    virtual std::vector<const sea_object*> visible_sea_objects(const sea_object* o) const;

    // fixme: maybe we should distuingish between passivly and activly detected
    // objects... passivly detected objects should store their noise source as
    // position and not their geometric center position!
    virtual std::vector<sonar_contact> sonar_ships(const sea_object* o) const;
    virtual std::vector<sonar_contact> sonar_submarines(const sea_object* o) const;
    virtual std::vector<sonar_contact> sonar_sea_objects(const sea_object* o) const;
    // fixme: return sonar_contact here (when the noise_pos fix is done...)
    virtual const ship* sonar_acoustical_torpedo_target(const torpedo* o) const;

    // std::list<*> radardetected_ships(...);	// later!
    virtual std::vector<const submarine*> radar_submarines(const sea_object* o) const;
    virtual std::vector<const ship*> radar_ships(const sea_object* o) const;
    // virtual std::vector<airplane*> radar_airplanes(const sea_object* o)
    // const;
    virtual std::vector<const sea_object*> radar_sea_objects(const sea_object* o) const;

    ///\brief compute sound strengths caused by all ships
    /** @param	listener		object that listens via passive sonar
        @passive	listening_direction	direction for listening
        @return	absolute freq. strength in dB and noise struct of received noise
       frequencies (in dB)
    */
    std::pair<double, noise> sonar_listen_ships(const ship* listener, angle rel_listening_dir) const;

    // append objects to vector
    template<class T>
    static void append_vec(std::vector<const sea_object*>& vec, const std::vector<T*>& vec2)
    {
        for (unsigned i = 0; i < vec2.size(); ++i)
            vec.push_back(vec2[i]);
    }

    std::vector<vector2> convoy_positions() const; // fixme

    // when submarine no longer inherits from ship use names spawn() directly
    // and determine via type only.
    std::pair<const sea_object_id, ship>& spawn_ship(ship&& obj);
    std::pair<const sea_object_id, submarine>& spawn_submarine(submarine&& obj);
    std::pair<const sea_object_id, airplane>& spawn_airplane(airplane&& obj);
    torpedo& spawn(torpedo&& obj);
    gun_shell& spawn(gun_shell&& obj);
    depth_charge& spawn(depth_charge&& obj);
    water_splash& spawn(water_splash&& obj);

    void spawn(std::unique_ptr<particle>&& p);
    std::pair<const sea_object_id, convoy>& spawn(convoy&& cv);

    // simulation events
    void dc_explosion(const depth_charge& dc); // depth charge exploding
    void torp_explode(const torpedo* t);       // torpedo explosion/impact
    void ship_sunk(const ship* s);             // a ship sinks

    // simulation actions, fixme send something over net for them, fixme : maybe
    // vector not list?
    virtual void
    ping_ASDIC(std::list<vector3>& contacts, sea_object* d, const bool& move_sensor, const angle& dir = angle(0.0f));

    // various functions (fixme: sort, cleanup)
    const std::list<ping>& get_pings() const { return pings; }; // fixme: maybe vector not list

    /// check if torpedo t hits any ship/sub and in that case spawn events
    bool check_torpedo_hit(torpedo* t, bool runlengthfailure);

    sea_object_id contact_in_direction(const sea_object* o, const angle& direction) const;
    sea_object_id ship_in_direction_from_pos(const sea_object* o, const angle& direction) const;
    sea_object_id sub_in_direction_from_pos(const sea_object* o, const angle& direction) const;

    const torpedo* get_torpedo_for_camera_track(unsigned nr) const;

    // old code, to be removed later, fixme.
    // bool is_collision(const sea_object* s1, const sea_object* s2) const;
    // bool is_collision(const sea_object* s, const vector2& pos) const;

    // is editor?
    virtual bool is_editor() const { return false; }

    // sun/moon and light color/brightness
    double compute_light_brightness(const vector3& viewpos, vector3& sundir) const; // depends on sun/moon
    colorf compute_light_color(const vector3& viewpos) const;                       // depends on sun/moon
    vector3 compute_sun_pos(const vector3& viewpos) const;
    vector3 compute_moon_pos(const vector3& viewpos) const;

    /// compute height of water at given world space position.
    double compute_water_height(const vector2& pos) const;

    void freeze_time();
    void unfreeze_time();

    void add_event(std::unique_ptr<event>&& e) { events.push_back(std::move(e)); }
    const auto& get_events() const { return events; }
    run_state get_run_state() const { return my_run_state; }
    unsigned get_freezetime() const { return freezetime; }
    unsigned get_freezetime_start() const { return freezetime_start; }
    unsigned process_freezetime()
    {
        unsigned f = freezetime;
        freezetime = 0;
        return f;
    }

    water& get_water() { return *mywater.get(); }
    const water& get_water() const { return *mywater.get(); }

    height_generator& get_height_gen() { return *myheightgen.get(); }
    const height_generator& get_height_gen() const { return *myheightgen.get(); }

    /// get pointers to all ships for collision tests.
    std::vector<const ship*> get_all_ships() const;

    virtual const player_info& get_player_info() const { return playerinfo; }

    /// return random integer number determining game behaviour
    unsigned random() { return random_gen.rnd(); }

    /// return random float number [0...1] determining game behaviour
    float randomf() { return random_gen.rndf(); }

    /// Check if sea_object_id is valid
    bool is_valid(sea_object_id id) const;

    /// Get store for models
    auto& get_model_store() { return model_store; }
};
