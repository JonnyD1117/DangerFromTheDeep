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

#include "game.hpp"

#include "airplane.hpp"
#include "cfg.hpp"
#include "convoy.hpp"
#include "datadirs.hpp"
#include "depth_charge.hpp"
#include "global_data.hpp"
#include "gun_shell.hpp"
#include "log.hpp"
#include "matrix4.hpp"
#include "model.hpp"
#include "particle.hpp"
#include "quaternion.hpp"
#include "sensors.hpp"
#include "ship.hpp"
#include "sonar.hpp"
#include "submarine.hpp"
#include "system_interface.hpp"
#include "terrain.hpp"
#include "texts.hpp"
#include "torpedo.hpp"
#include "water.hpp"
#include "water_splash.hpp"

#include <cfloat>
#include <mutex>
#include <sstream>
#include <utility>
using std::list;
using std::make_pair;
using std::ostringstream;
using std::pair;
using std::string;
using std::vector;

const unsigned SAVEVERSION = 1;
const unsigned GAMETYPE    = 0; // fixme, 0-mission , 1-patrol etc.

const double game::TRAIL_TIME = 1.0;

/***************************************************************************/

game::ping::ping(const xml_elem& parent)
{
    pos.x      = parent.attrf("posx");
    pos.y      = parent.attrf("posy");
    dir        = angle(parent.attrf("dir"));
    time       = parent.attrf("time");
    range      = parent.attrf("range");
    ping_angle = angle(parent.attrf("ping_angle"));
}

void game::ping::save(xml_elem& parent) const
{
    parent.set_attr(pos.x, "posx");
    parent.set_attr(pos.y, "posy");
    parent.set_attr(dir.value(), "dir");
    parent.set_attr(time, "time");
    parent.set_attr(range, "range");
    parent.set_attr(ping_angle.value(), "ping_angle");
}

game::sink_record::sink_record(const xml_elem& parent)
{
    dat.load(parent);
    descr        = parent.attr("descr");
    mdlname      = parent.attr("mdlname");
    tons         = parent.attru("tons");
    specfilename = parent.attr("specfilename");
    layoutname   = parent.attr("layoutname");
}

void game::sink_record::save(xml_elem& parent) const
{
    dat.save(parent);
    parent.set_attr(descr, "descr");
    parent.set_attr(mdlname, "mdlname");
    parent.set_attr(tons, "tons");
    parent.set_attr(specfilename, "specfilename");
    parent.set_attr(layoutname, "layoutname");
}

game::player_info::player_info()
    : name("Heinz Mustermann")
    , submarineid("U 999")
    , photo(1)
{
    // generate a random soldbuch_nr between 1 and 9999
    soldbuch_nr = str(rnd(9999) + 1);

    // generate a random bloodgroup
    const string bloodgroups[] = {"A", "B", "AB", "0"};
    bloodgroup                 = bloodgroups[rnd(4)];

    // there are just 3 sizes
    gasmask_size = str(rnd(3) + 1);

    /* first part of the marine roll nr is a character that specifies the naval
     * command for submarines that should be the naval command west --> W the
     * second part is a contnious number that is unique for every soldier in the
     * roll (of his flotilla) 20.000 as max value should be high enough third
     * part is unknown so just take the soldbuch nr */
    marine_roll = "W " + str(rnd(20000) + 1) + " / " + soldbuch_nr;
}

game::player_info::player_info(const xml_elem& parent)
{
    name         = parent.attr("name");
    photo        = parent.attru("photo");
    flotilla     = parent.attru("flotilla");
    submarineid  = parent.attr("submarineid");
    soldbuch_nr  = parent.attr("soldbuch_nr");
    gasmask_size = parent.attr("gasmask_size");
    bloodgroup   = parent.attr("bloodgroup");
    marine_roll  = parent.attr("marine_roll");
    marine_group = parent.attr("marine_group");
    for (auto elem : parent.iterate("promotions")) {
        career.push_back(elem.attr("date"));
    }
}

void game::player_info::save(xml_elem& parent) const
{
    parent.set_attr(name, "name");
    parent.set_attr(flotilla, "flottila");
    parent.set_attr(submarineid, "submarineid");
    parent.set_attr(soldbuch_nr, "soldbuch_nr");
    parent.set_attr(gasmask_size, "gasmask_size");
    parent.set_attr(bloodgroup, "bloodgroup");
    parent.set_attr(marine_roll, "marine_roll");
    parent.set_attr(marine_group, "marine_group");
    xml_elem xml_career = parent.add_child("promotions");
    for (const auto& it : career) {
        xml_elem elem = xml_career.add_child("promotion");
        elem.set_attr(it, "date");
    }
}

game::game()
    : model_store(get_data_dir())
{
    // empty, so that heirs can construct a game object. Needed for editor

    mywater = std::make_unique<water>(0.0);
    // myheightgen.reset(new height_generator_map("default.xml"));

    myheightgen = std::make_unique<terrain<int16_t>>(
        get_map_dir() + "terrain/terrain.xml", get_map_dir() + "terrain/", TERRAIN_NR_LEVELS + 1);
}

game::game(
    const string& subtype,
    unsigned cvsize,
    unsigned cvesc,
    unsigned timeofday,
    const date& timeperioddate,
    player_info pi,
    unsigned nr_of_players)
    : playerinfo(std::move(pi))
    , model_store(get_data_dir())
{
    /****************************************************************
        custom mission generation:
        As first find a random date and time, using time of day (tod).
        Whe have to calculate time of sunrise and sunfall for that, with some
    time until this time of day expires (5:59am is not really "night" when
    sunrise is at 6:00am). Also weather computation is neccessary. Then we
    calculate size and structure of the convoy (to allow calculation of its map
    area). Then we have to calculate maximum viewing distance to know the
    distance of the sub relative to the convoy. We have to find a probable
    convoy position in the atlantic (convoy routes, enough space for convoy and
    sub). Then we place the convoy with probable course and path there. To do
    this we need a simulation of convoys in the atlantic. Then we place the sub
    somewhere randomly around the convoy with maximum viewing distance.
        Multiplayer: place several subs around the convoy with a minimum
    distance between each. Sub placement: compute a random angle. Place the sub
    on a line given by that angle around the convoy's center. Line is (0,0) + t
    * (dx, dy). Compute value t for each convoy ship so that the ship can be
    seen from the point t*(dx,dy), with maximum t (e.g. with binary subdivision
        approximation).
        The maximum t over all ships is choosen for the position.
        To do that we create and use a lookout sensor.
        This technique ignores the fact that convoys could be heared earlier
    than seen (below surface, passive sonar) or even detected by their smell
    (smoke)!
    ***********************************************************************/

    // fixme: show some info like in Silent Service II? sun/moon
    // pos,time,visibility?

    time = timeperioddate.get_time();

    // all code from here on is fixme and experimental.
    // fixme: we need exact sunrise and fall times for a day. (also moon state
    // is needed later) The compute_sun_pos func is not enough
    switch (timeofday) {
        case 0:
            time += helper::mod(20 + 10 * rnd(), 24.0) * 3600;
            break; // night
        case 1:
            time += (6 + 2 * rnd()) * 3600;
            break; // dawn
        case 2:
            time += (8 + 10 * rnd()) * 3600;
            break; // day
        case 3:
            time += (18 + 2 * rnd()) * 3600;
            break; // dusk
    }

    date currentdate(static_cast<unsigned>(time));
    equipment_date = currentdate; // fixme: another crude guess or hack

    mywater = std::make_unique<water>(time);
    // myheightgen.reset(new height_generator_map("default.xml"));

    myheightgen = std::make_unique<terrain<int16_t>>(
        get_map_dir() + "terrain/terrain.xml", get_map_dir() + "terrain/", TERRAIN_NR_LEVELS + 1);

    // Convoy-constructor creates all the objects and spawns them in this game
    // object. fixme: creation of convoys should be rather moved to this class,
    // so object creation and logic is centralized.
    spawn(convoy(*this, static_cast<convoy::types>(cvsize), static_cast<convoy::esctypes>(cvesc)));

    lookout_sensor tmpsensor;
    vector<angle> subangles;
    for (unsigned i = 0; i < nr_of_players; ++i) {
        xml_doc doc(data_file().get_filename(subtype));
        doc.load();
        submarine sub(get_date(), get_model_store(), doc.first_child());

        sub.set_skin_layout(model::default_layout);
        sub.init_fill_torpedo_tubes(currentdate);

        // distribute subs randomly around convoy.
        angle tmpa;
        double anglediff    = 90.0;
        bool angleok        = false;
        unsigned angletries = 0;
        do {
            angleok = true;
            tmpa    = (rnd() * 360.0);
            for (auto subangle : subangles) {
                if (tmpa.diff(subangle) < anglediff) {
                    angleok = false;
                    break;
                }
            }
            if (!angleok) {
                ++angletries;
                if (angletries >= nr_of_players) {
                    angletries = 0;
                    anglediff /= 2.0;
                }
            }
        } while (!angleok);

        // now tmpa holds the angle of the sub's position around the convoy.
        double maxt = 0;
        for (auto& [id, ship] : ships) {
            double maxt1 = 0;
            double maxt2 = get_max_view_distance() / 2;
            double maxt3 = get_max_view_distance();
            sub.manipulate_position((maxt2 * tmpa.direction()).xy0());
            // find maximum distance t along line (0,0)+t*tmpa.dir() for this
            // ship
            while (maxt3 - maxt1 > 50.0) {
                if (tmpsensor.is_detected(this, &sub, &ship)) {
                    maxt1 = maxt2;
                } else {
                    maxt3 = maxt2;
                }
                maxt2 = (maxt1 + maxt3) / 2;
                sub.manipulate_position((maxt2 * tmpa.direction()).xy0());
            }
            if (maxt2 > maxt) {
                maxt = maxt2;
            }
        }
        vector3 subpos = (maxt * tmpa.direction()).xy0();
        subpos.z       = (timeofday == 2) ? 0 : -12; // fixme maybe always surfaced, except late in war
        sub.manipulate_position(subpos);
        // heading should be facing to the convoy (+-90deg), as it is
        // unrealistic to detect a convoy while moving away from it
        sub.manipulate_heading(angle(rnd() * 180.0 + 90.0) + tmpa);
        auto& [id, thesub] = spawn_submarine(std::move(sub));
        if (i == 0) {
            player    = &thesub;
            player_id = id;
            compute_max_view_dist();
        }
    }

    my_run_state    = running;
    last_trail_time = time - TRAIL_TIME;

    freezetime       = 0;
    freezetime_start = 0;
}

// --------------------------------------------------------------------------------
//                        LOAD GAME (SAVEGAME OR MISSION)
// --------------------------------------------------------------------------------
game::game(const string& filename)
    : my_run_state(running)
    , time(0)
    , last_trail_time(0)
    , max_view_dist(0)
    , freezetime(0)
    , freezetime_start(0)
    , model_store(get_data_dir())
{
    xml_doc doc(filename);
    doc.load();
    // could be savegame or mission, maybe check...
    // has_child("dftd-savegame") or has_child("dftd-mission");
    xml_elem sg = doc.first_child();
    // fixme: check for savegames.
    //	unsigned v = sg.attr(version);
    //	if (v != SAVEVERSION)
    //		THROW(error, "invalid game version");

    // load state first, because time is stored there and we need time/date for
    // checks while loading the rest.
    xml_elem gst = sg.child("state");
    time         = gst.attrf("time");

    last_trail_time = gst.attrf("last_trail_time");
    equipment_date.load(gst.child("equipment_date"));
    max_view_dist = gst.attrf("max_view_dist");

    // fixme: save original water creation time and random seed with that water
    // was generated. set the same seed here again, so water is exactly like it
    // was at game start.
    mywater = std::make_unique<water>(time);

    // myheightgen.reset(new height_generator_map("default.xml"));

    myheightgen = std::make_unique<terrain<int16_t>>(
        get_map_dir() + "terrain/terrain.xml", get_map_dir() + "terrain/", TERRAIN_NR_LEVELS + 1);

    // create empty objects so references can be filled.
    // there must be ships in a mission...
    xml_elem sh = sg.child("ships");
    for (auto elem : sh.iterate("ship")) {
        xml_doc spec(data_file().get_filename(elem.attr("type")));
        spec.load();
        spawn_ship(ship(get_date(), get_model_store(), spec.first_child())).second.load(elem);
    }

    // there must be submarines in a mission...
    xml_elem su = sg.child("submarines");
    for (auto elem : su.iterate("submarine")) {
        xml_doc spec(data_file().get_filename(elem.attr("type")));
        spec.load();
        spawn_submarine(submarine(get_date(), get_model_store(), spec.first_child())).second.load(elem);
    }

    if (sg.has_child("airplanes")) {
        xml_elem ap = sg.child("airplanes");
        for (auto elem : ap.iterate("airplane")) {
            xml_doc spec(data_file().get_filename(elem.attr("type")));
            spec.load();
            spawn_airplane(airplane(get_date(), get_model_store(), spec.first_child())).second.load(elem);
        }
    }

    if (sg.has_child("torpedoes")) {
        xml_elem tp = sg.child("torpedoes");
        for (auto elem : tp.iterate("torpedo")) {
            xml_doc spec(data_file().get_filename(elem.attr("type")));
            spec.load();
            spawn(
                torpedo(get_date(), get_equipment_date(), get_model_store(), spec.first_child(), torpedo::setup_data()))
                .load(elem);
        }
    }

    if (sg.has_child("depth_charges")) {
        xml_elem dc = sg.child("depth_charges");
        for (auto elem : dc.iterate("depth_charge")) {
            // xml_doc spec(get_depth_charge_dir() + elem.attr("type") +
            // ".xml"); spec.load();
            spawn(depth_charge(/*, spec.first_child()*/ get_model_store())).load(elem);
        }
    }

    if (sg.has_child("gun_shells")) {
        xml_elem gs = sg.child("gun_shells");
        for (auto elem : gs.iterate("gun_shell")) {
            // xml_doc spec(get_gun_shell_dir() + elem.attr("type") + ".xml");
            // spec.load();
            spawn(gun_shell(/*, spec.first_child()*/ get_model_store())).load(elem);
        }
    }

    if (sg.has_child("convoys")) {
        xml_elem cv = sg.child("convoys");
        for (auto elem : cv.iterate("convoy")) {
            // xml_doc spec(get_convoy_dir() + elem.attr("type") + ".xml");
            // spec.load();
            spawn(convoy(/*, spec.first_child()*/)).second.load(elem);
        }
    }

    // fixme: handle water splashes too.

#if 0
    if (sg.has_child("particles")) {
        xml_elem pt = sg.child("particles");
        for (auto elem : pt.iterate("particle")) {
            //xml_doc spec(get_particle_dir() + elem.attr("type") + ".xml");
            //spec.load();
            particles.push_back(new particle(*this/*, spec.first_child()*/));
        }
    }
#endif

    // fixme: handle water splashes too

#if 0
    if (particles.size() > 0) {
        k = 0;
        for (auto elem : sg.child("particles").iterate("particle")) {
            particles[k++].load(elem);
        }
    }
#endif

    // load player
    xml_elem pl = sg.child("player");
    player_id   = sea_object_id(pl.attru("ref"));
    player      = const_cast<sea_object*>(&get_object(player_id));
    // fixme: maybe check if type matches!

    // ui is created from client of game!

    xml_elem sks = sg.child("sunken_ships");
    for (auto elem : sks.iterate("sink_record")) {
        sunken_ships.emplace_back(elem);
    }

    // fixme save and load logbook

    xml_elem pgs = sg.child("pings");
    for (auto elem : pgs.iterate("ping")) {
        pings.emplace_back(elem);
    }

    playerinfo = player_info(sg.child("player_info"));
}

game::~game() = default;

// --------------------------------------------------------------------------------
//                        SAVE GAME
// --------------------------------------------------------------------------------

void game::save(const string& savefilename, const string& description) const
{
    xml_doc doc(savefilename);
    xml_elem sg = doc.add_child("dftd-savegame");
    sg.set_attr(description, "description");
    sg.set_attr(SAVEVERSION, "version");
    sg.set_attr(GAMETYPE, "type");

    xml_elem sh = sg.add_child("ships");
    sh.set_attr(unsigned(ships.size()), "nr");
    for (const auto& [id, ship] : ships) {
        xml_elem e = sh.add_child("ship");
        e.set_attr(ship.get_specfilename(), "type");
        ship.save(e);
    }

    xml_elem su = sg.add_child("submarines");
    su.set_attr(unsigned(submarines.size()), "nr");
    for (const auto& [id, submarine] : submarines) {
        xml_elem e = su.add_child("submarine");
        e.set_attr(submarine.get_specfilename(), "type");
        submarine.save(e);
    }

    xml_elem ap = sg.add_child("airplanes");
    ap.set_attr(unsigned(airplanes.size()), "nr");
    for (const auto& [id, airplane] : airplanes) {
        xml_elem e = ap.add_child("airplane");
        e.set_attr(airplane.get_specfilename(), "type");
        airplane.save(e);
    }

    xml_elem tp = sg.add_child("torpedoes");
    tp.set_attr(unsigned(torpedoes.size()), "nr");
    for (const auto& torpedo : torpedoes) {
        xml_elem e = tp.add_child("torpedo");
        e.set_attr(torpedo.get_specfilename(), "type");
        torpedo.save(e);
    }

    xml_elem dc = sg.add_child("depth_charges");
    dc.set_attr(unsigned(depth_charges.size()), "nr");
    for (const auto& depth_charge : depth_charges) {
        xml_elem e = dc.add_child("depth_charge");
        // e.set_attr(depth_charges[k].get_specfilename(), "type");//no
        // specfilename for DCs
        depth_charge.save(e);
    }

    xml_elem gs = sg.add_child("gun_shells");
    gs.set_attr(unsigned(gun_shells.size()), "nr");
    for (const auto& gun_shell : gun_shells) {
        xml_elem e = gs.add_child("gun_shell");
        // e.set_attr(gun_shells[k].get_specfilename(), "type");//no
        // specfilename for shells
        gun_shell.save(e);
    }

    xml_elem cv = sg.add_child("convoys");
    cv.set_attr(unsigned(convoys.size()), "nr");
    for (const auto& [id, convoy] : convoys) {
        xml_elem e = cv.add_child("convoy");
        // e.set_attr(convoy.get_specfilename(), "type");//no specfilename for
        // convoys
        convoy.save(e);
    }

#if 0 // fixme later!!!
    xml_elem pt = sg.add_child("particles");
    pt.set_attr(particles.size(), "nr");
    for (unsigned k = 0; k < particles.size(); ++k) {
        xml_elem e = pt.add_child("particle");
        //e.set_attr(particles[k].get_specfilename(), "type");//no specfilename for particles
        particles[k].save(e);
    }
#endif

    // my_run_state doesn't need to be saved

    // save player
    auto* tmpsub = dynamic_cast<submarine*>(player);
    string pltype;
    if (tmpsub != nullptr) {
        pltype = "submarine";
    } else {
        ship* tmpship = dynamic_cast<ship*>(player);
        if (tmpship != nullptr) {
            pltype = "ship";
        } else {
            auto* tmpairpl = dynamic_cast<airplane*>(player);
            if (tmpairpl != nullptr) {
                pltype = "airplane";
            } else {
                THROW(error, "internal error: player is no sub, ship or airplane");
            }
        }
    }
    xml_elem pl = sg.add_child("player");
    pl.set_attr(player_id.id, "ref");
    pl.set_attr(pltype, "type");

    // user interface is generated according to player object by dftd

    xml_elem sks = sg.add_child("sunken_ships");
    sks.set_attr(unsigned(sunken_ships.size()), "nr");
    for (const auto& sunken_ship : sunken_ships) {
        sunken_ship.save(sks);
    }

    // fixme save and load logbook

    xml_elem gst = sg.add_child("state");
    gst.set_attr(time, "time");
    // save current date as reference for human readers.
    date(unsigned(time)).save(gst);
    gst.set_attr(last_trail_time, "last_trail_time");
    xml_elem equ = gst.add_child("equipment_date");
    equipment_date.save(equ);
    gst.set_attr(max_view_dist, "max_view_dist");

    xml_elem pgs = sg.add_child("pings");
    pgs.set_attr(unsigned(pings.size()), "nr");
    for (const auto& it : pings) {
        it.save(pgs);
    }

    xml_elem pi = sg.add_child("player_info");
    playerinfo.save(pi);

    // fixme: later save and load random_gen seed value, to make randomness
    // repeatable

    // finally save file
    doc.save();
}

auto game::read_description_of_savegame(const string& filename) -> string
{
    // causes 90mb mem leak fixme
    xml_doc doc(filename);
    doc.load();
    xml_elem sg = doc.child("dftd-savegame");
    unsigned v  = sg.attru("version");
    if (v != SAVEVERSION) {
        return "<ERROR> Invalid version";
    }
    string d = sg.attr("description");
    if (d.length() == 0) {
        return "<ERROR> Empty description";
    }
    return d;
}

void game::compute_max_view_dist()
{
    // a bit unprecise here, since the viewpos is not always the same as the
    // playerpos this must depend also on weather, fog, rain etc.
    vector3 sundir;
    max_view_dist = 5000.0 + compute_light_brightness(player->get_pos(), sundir) * 25000;
}

template<class T>
void cleanup(std::unordered_map<sea_object_id, T>& s)
{
    for (auto it = s.begin(); it != s.end();) {
        if (it->second.is_dead()) {
            it = s.erase(it);
        } else {
            ++it;
        }
    }
}

template<class T>
void cleanup(std::vector<T>& s)
{
    for (auto it = s.begin(); it != s.end();) {
        if (it->is_dead()) {
            it = s.erase(it);
        } else {
            ++it;
        }
    }
}

void game::simulate(double delta_t)
{
    if (!is_editor()) {
        if (my_run_state != running) {
            return;
        }
    }

    // protect physics simulation from bad values, simulation step must not
    // be less than 20fps.
    const double max_dt_rate = 1.0 / 20.0;
    if (delta_t > max_dt_rate) {
        // do some intermediate steps. All larger than max_dt_rate, so add a
        // small amount.
        auto steps = unsigned(ceil(delta_t / max_dt_rate + 0.001));
        double ddt = delta_t / steps;
        log_debug("Large delta_t (" << delta_t << "), using " << steps << " steps in between.");
        for (unsigned s = 1; s < steps; ++s) {
            simulate(ddt);
            delta_t -= ddt;
        }
        simulate(delta_t);
        return;
    }

    // kill events left over from last run
    events.clear();

    if (!is_editor()) {
        if (!player->is_alive()) {
            log_info("player killed!"); // testing fixme
#ifdef COD_MODE
            player->reanimate();
#else
            my_run_state = player_killed;
            return;
#endif // COD_MODE
        }

        if (/* submarines.size() == 0 && */ ships.empty() && torpedoes.empty() && depth_charges.empty()
            && airplanes.empty() && gun_shells.empty()) {
            log_info("no objects except player left!"); // testing fixme
            my_run_state = mission_complete;            // or also contact lost?
            return;
        }
    }

    compute_max_view_dist();

    bool record = false;
    if (get_time() >= last_trail_time + TRAIL_TIME) {
        last_trail_time = get_time();
        record          = true;
    }

    // fixme 2003/07/11: time compression trashes trail recording.

    double nearest_contact = 1e10;

    // simulation for each object
    // Note! Simulation order does not matter, because every killed
    // object is kept for two rounds (state change to dead2, then
    // defunct) because state change happens only in
    // sea_object::simulate.
    // Even if e.g. three objects A, B, C are simulated in that order.
    // A has reference to C and B kills C when B is simulated. Then
    // C's state is dead, and when C is simulated afterwards, its
    // state changes to dead2. On the next call to game::simulate
    // the object C is NOT deleted, because it is not defunct
    // yet, only one round later.
    // This example shows how it works, and also that we need the
    // extra dead state "dead2". Otherwise C would have been deleted
    // before A is simulated again, when A still has a reference to
    // C which would give a SIGSEGV.
    // And because every reference to a sea_object must be checked for
    // validity at least every round, we can avoid to reference
    // deleted objects.

    // step 1: check for invalidity of every object and remove
    // defunct objects. do NOT mix simulate() calls with real
    // calls to delete an object.
    cleanup(ships);
    cleanup(submarines);
    cleanup(airplanes);
    cleanup(torpedoes);
    cleanup(depth_charges);
    cleanup(gun_shells);
    cleanup(water_splashes);

    // step 2: simulate all objects, possibly setting state to dead/defunct.
    simulate_objects(delta_t, record, nearest_contact);

    // Now check for collisions. As a result objects could be set to dead state.
    // If we would call this before simulate() an object could go from alive
    // to dead (by collision with grenade) to defunct in one round. We avoid
    // this by calling check_collision() after simulate().
    // If objects collide, apply forces so their physic values change.
    // fixme: collision forces should be handled in compute_force_and_torque()
    // so we need to know wether there is a collision in advance.
    // maybe we should compute collisions before simulate and then
    // give each object a list of colliding objects...
    // problem: when a collision happens, both objects need to have forces
    // applied. That is if we check a->collides(b) and this is true,
    // we know that b has collision with a before simulating b. Since
    // collision check is expensive, do not call b->collides(a) later,
    // but reuse collision info.
    // can be solved by storing a list of collision partners per object,
    // that is cleared every round and generated by this check_collision()
    // function. In that case we should call it _before_ simulate()...
    check_collisions();

    time += delta_t;

    // remove old pings
    for (auto it = pings.begin(); it != pings.end();) {
        auto it2 = it++;
        if (time - it2->time > acoustics::ping_remain_time) {
            pings.erase(it2);
        }
    }

    if (!is_editor()) {
        if (nearest_contact > acoustics::enemy_contact_lost) {
            log_info("player lost contact to enemy!"); // testing fixme
            my_run_state = contact_lost;
        }
    }
}

void game::simulate_objects(double delta_t, bool record, double& nearest_contact)
{
    // ------------------------------ ships ------------------------------
    for (auto& [id, ship] : ships) {
        if (&ship != player) {
            double dist = ship.get_pos().distance(player->get_pos());
            if (dist < nearest_contact) {
                nearest_contact = dist;
            }
        }
        ship.simulate(delta_t, *this);
        if (record) {
            ship.remember_position(get_time());
        }
    }

    // ------------------------------ submarines ------------------------------
    for (auto& [id, submarine] : submarines) {
        if (&submarine != player) {
            double dist = submarine.get_pos().distance(player->get_pos());
            if (dist < nearest_contact) {
                nearest_contact = dist;
            }
        }
        submarine.simulate(delta_t, *this);
        if (record) {
            submarine.remember_position(get_time());
        }
    }

    // ------------------------------ airplanes ------------------------------
    for (auto& [id, airplane] : airplanes) {
        if (&airplane != player) {
            double dist = airplane.get_pos().distance(player->get_pos());
            if (dist < nearest_contact) {
                nearest_contact = dist;
            }
        }
        airplane.simulate(delta_t, *this);
    }

    // ------------------------------ torpedoes ------------------------------
    for (auto& torpedo : torpedoes) {
        torpedo.simulate(delta_t, *this);
        if (record) {
            torpedo.remember_position(get_time());
        }
    }

    // ------------------------------ depth_charges -------------------------
    for (auto& depth_charge : depth_charges) {
        depth_charge.simulate(delta_t, *this);
    }

    // ------------------------------ gun_shells ----------------------------
    for (auto& gun_shell : gun_shells) {
        gun_shell.simulate(delta_t, *this);
    }

    // ------------------------------ water_splashes -----------------------
    for (auto& water_splash : water_splashes) {
        water_splash.simulate(delta_t, *this);
    }

    // ------------------------------ convoys ------------------------------
    for (auto& [id, convoy] : convoys) {
        convoy.simulate(delta_t, *this); // fixme: handle erasing of empty convoys!
    }

    // particles
    for (auto& particle : particles) {
        if (particle == nullptr) {
            continue;
        }

        if (particle->is_dead()) {
            particle = nullptr;
        } else {
            particle->simulate(*this, delta_t);
        }
    }
    helper::erase_remove_if(particles, [](const std::unique_ptr<particle>& p) { return p == nullptr; });
}

void game::add_logbook_entry(const string& s)
{
    // fixme: format of date is fix in logbook then, this is not optimal.
    // when player changes language, format is not changed on display...
    players_logbook.add_entry(texts::numeric_from_daytime(date(unsigned(get_time()))) + " : " + s);
}

/*
    Visibility computation
    ----------------------

    Visibility is determined by two factors:
    1) overall visibility
        - time of day (sun position -> brightness)
        - weather
        - moon phase and position during night
    2) specific visibility
        - object type
        - surfaced: course, speed, engine type etc.
        - submerged: speed, scope height etc.
        - relative position to sun/moon
    The visibility computation gives a distance within that the object can be
seen by other objects. This distance depends on relative distance and courses of
    both objects (factor2) and overall visibility (factor1).
    Maybe some randomization should be added (quality of crew, experience,
overall visibility +- some meters)

    Visibility of ships can be determined by area that is visible and this
depends on relative course between watcher and visible object. For ships this
shouldn't make much difference (their shape is much higher than that of
submarines), but for subs the visibility is like an ellipse given by an implicit
function, rotated by course, roughly proportional to length and with. So we can
compute visibiltiy by just multiplying relative coordinates (x, y, 1) with a 3x3
matrix from left and right and evaluate the result.

*/

template<class T>
inline auto visible_obj(const game* gm, const std::unordered_map<sea_object_id, T>& v, const sea_object* o)
    -> vector<const T*>
{
    vector<const T*> result;
    const sensor* s = o->get_sensor(o->lookout_system);
    if (!s) {
        return result;
    }
    const auto* ls = dynamic_cast<const lookout_sensor*>(s);
    if (!ls) {
        return result;
    }
    result.reserve(v.size());
    for (auto& [id, obj] : v) {
        // do not handle dead or defunct objects!
        if (obj.is_reference_ok()) {
            if (ls->is_detected(gm, o, &obj)) {
                result.push_back(&obj);
            }
        }
    }
    return result;
}

template<class T>
inline auto visible_obj(const game* gm, const std::vector<T>& v, const sea_object* o) -> vector<const T*>
{
    vector<const T*> result;
    const sensor* s = o->get_sensor(o->lookout_system);
    if (!s) {
        return result;
    }
    const auto* ls = dynamic_cast<const lookout_sensor*>(s);
    if (!ls) {
        return result;
    }
    result.reserve(v.size());
    for (auto& obj : v) {
        // do not handle dead or defunct objects!
        if (obj.is_reference_ok()) {
            if (ls->is_detected(gm, o, &obj)) {
                result.push_back(&obj);
            }
        }
    }
    return result;
}

auto game::visible_ships(const sea_object* o) const -> vector<const ship*>
{
    return visible_obj<ship>(this, ships, o);
}

auto game::visible_submarines(const sea_object* o) const -> vector<const submarine*>
{
    return visible_obj<submarine>(this, submarines, o);
}

auto game::visible_airplanes(const sea_object* o) const -> vector<const airplane*>
{
    return visible_obj<airplane>(this, airplanes, o);
}

auto game::visible_torpedoes(const sea_object* o) const -> vector<const torpedo*>
{
    return visible_obj<torpedo>(this, torpedoes, o);
}

auto game::visible_depth_charges(const sea_object* o) const -> vector<const depth_charge*>
{
    return visible_obj<depth_charge>(this, depth_charges, o);
}

auto game::visible_gun_shells(const sea_object* o) const -> vector<const gun_shell*>
{
    return visible_obj<gun_shell>(this, gun_shells, o);
}

auto game::visible_water_splashes(const sea_object* /*o*/) const -> vector<const water_splash*>
{
    // testing: draw all
    vector<const water_splash*> result(water_splashes.size());
    unsigned k = 0;
    for (const auto& obj : water_splashes) {
        result[k++] = &obj;
    }
    return result;
    //	return visible_obj<water_splash>(this, water_splashes, o);
}

auto game::visible_particles(const sea_object* o) const -> vector<const particle*>
{
    // fixme: this is called for every particle. VERY costly!!!
    std::vector<const particle*> result;
    const sensor* s = o->get_sensor(o->lookout_system);
    if (s == nullptr) {
        return result;
    }
    const auto* ls = dynamic_cast<const lookout_sensor*>(s);
    if (ls == nullptr) {
        return result;
    }
    result.reserve(particles.size());
    for (const auto& particle : particles) {
        if (particle == nullptr) { // obsolete test? should be so...
            THROW(error, "particles[i] is 0!");
        }

        if (ls->is_detected(this, o, particle.get())) {
            result.push_back(particle.get());
        }
    }
    return result;
}

auto game::sonar_ships(const sea_object* o) const -> vector<sonar_contact>
{
    vector<sonar_contact> result;
    const sensor* s = o->get_sensor(o->passive_sonar_system);
    if (s == nullptr) {
        return result;
    }
    const auto* pss = dynamic_cast<const passive_sonar_sensor*>(s);
    if (pss == nullptr) {
        return result;
    }

    result.reserve(ships.size());

    // collect the nearest contacts, limited to some value!
    vector<pair<double, const ship*>> contacts(acoustics::max_acoustic_contacts, make_pair(1e30, (ship*) nullptr));

    for (const auto& [id, ship] : ships) {
        // do not handle dead/defunct objects
        if (!ship.is_reference_ok()) {
            continue;
        }

        // When the detecting unit is a ship it should not detect itself.
        if (o == &ship) {
            continue;
        }

        double d   = ship.get_pos().xy().square_distance(o->get_pos().xy());
        unsigned i = 0;
        for (; i < contacts.size(); ++i) {
            if (contacts[i].first > d) {
                break;
            }
        }

        if (i < contacts.size()) {
            for (unsigned j = contacts.size() - 1; j > i; --j) {
                contacts[j] = contacts[j - 1];
            }

            contacts[i] = make_pair(d, &ship);
        }
    }

    unsigned size = contacts.size();
    for (unsigned i = 0; i < size; i++) {
        const auto* sh = contacts[i].second;
        if (sh == nullptr) {
            break;
        }

        if (pss->is_detected(this, o, sh)) {
            result.emplace_back(sh->get_pos().xy(), sh->get_class());
        }
    }
    return result;
}

auto game::sonar_submarines(const sea_object* o) const -> vector<sonar_contact>
{
    vector<sonar_contact> result;
    const sensor* s = o->get_sensor(o->passive_sonar_system);
    if (s == nullptr) {
        return result;
    }
    const auto* pss = dynamic_cast<const passive_sonar_sensor*>(s);
    if (pss == nullptr) {
        return result;
    }
    result.reserve(submarines.size());
    for (const auto& [id, submarine] : submarines) {
        // do not handle dead/defunct objects
        if (!submarine.is_reference_ok()) {
            continue;
        }

        // When the detecting unit is a submarine it should not
        // detect itself.
        if (o == &submarine) {
            continue;
        }

        if (pss->is_detected(this, o, &submarine)) {
            result.emplace_back(submarine.get_pos().xy(), submarine.get_class());
        }
    }
    return result;
}

auto game::sonar_sea_objects(const sea_object* o) const -> vector<sonar_contact>
{
    vector<sonar_contact> sships      = sonar_ships(o);
    vector<sonar_contact> ssubmarines = sonar_submarines(o);
    sships.reserve(sships.size() + ssubmarines.size());
    for (const auto& ssubmarine : ssubmarines) {
        sships.push_back(ssubmarine);
    }
    return sships;
}

auto game::radar_submarines(const sea_object* o) const -> vector<const submarine*>
{
    vector<const submarine*> result;
    const sensor* s = o->get_sensor(o->radar_system);
    if (s == nullptr) {
        return result;
    }
    const auto* ls = dynamic_cast<const radar_sensor*>(s);
    if (ls == nullptr) {
        return result;
    }
    result.reserve(submarines.size());
    for (const auto& [id, submarine] : submarines) {
        if (ls->is_detected(this, o, &submarine)) {
            result.push_back(&submarine);
        }
    }
    return result;
}

auto game::radar_ships(const sea_object* o) const -> vector<const ship*>
{
    vector<const ship*> result;
    const sensor* s = o->get_sensor(o->radar_system);
    if (s == nullptr) {
        return result;
    }
    const auto* ls = dynamic_cast<const radar_sensor*>(s);
    if (ls == nullptr) {
        return result;
    }
    result.reserve(ships.size());
    for (const auto& [id, ship] : ships) {
        if (ls->is_detected(this, o, &ship)) {
            result.push_back(&ship);
        }
    }
    return result;
}

auto game::radar_sea_objects(const sea_object* o) const -> vector<const sea_object*>
{
    vector<const ship*> rships           = radar_ships(o);
    vector<const submarine*> rsubmarines = radar_submarines(o);
    vector<const sea_object*> result;
    result.reserve(rships.size() + rsubmarines.size());
    append_vec(result, rships);
    append_vec(result, rsubmarines);
    return result;
}

auto game::convoy_positions() const -> vector<vector2>
{
    vector<vector2> result;
    result.reserve(convoys.size());
    for (const auto& [id, convoy] : convoys) {
        result.push_back(convoy.get_pos());
    }
    return result;
}

auto game::sonar_listen_ships(const ship* listener, angle rel_listening_dir) const -> pair<double, noise>
{
    // collect all ships for sound strength measurement
    vector<const ship*> tmpships;
    tmpships.reserve(ships.size() + submarines.size() /* + torpedoes.size() */ - 1);
    for (const auto& [id, ship] : ships) {
        if (&ship != listener) {
            tmpships.push_back(&ship);
        }
    }
    for (const auto& [id, submarine] : submarines) {
        if (dynamic_cast<const ship*>(&submarine) != listener) {
            tmpships.push_back(&submarine);
        }
    }
    // fixme: add torpedoes here as well... later...

#if 0
    // fixme, test, only detect one ship
    tmpships.resize(1);
#endif

    // fixme: the lower part of this function is sonar dependent and should go
    // to a sonar class...
    // TODO(tj): add Acoustics class, move constants, sound simulation

    // compute noise strengths for all ships for all frequency bands, real
    // strengths, not dB!
    noise n;
#if 1
    // as first, add background noise
    n += noise::compute_ambient_noise_strength(0.2 /* sea state, fixme make dynamic later */);
#endif

    // next, add noise from receiver vessel
    // if we do that, weaker noises are wiped out...
    // fixme: GHG/BG have blind spots at aft, so the receiver caused noise is
    // reduced much more. we should handle receiver vessel as additional noise
    // source with distance 50, direction 180° relative!
    n += listener->get_noise_signature().compute_signal_strength(
        50 /* distance */, listener->get_speed(), false /*cavitation=off for listener*/);

    angle hdg                = listener->get_heading();
    bool listen_to_starboard = (rel_listening_dir.value_pm180() >= 0);

    // detection formula:
    // compute noise of target = L_t
    // compute ambient noise = L_a
    // compute sensor background noise (noise from own vessel, receiver) = L_r
    // store sensor sensitivity = S
    // The signal is detected when L_t - (L_a + L_r) > S
    // which is equivalent to L_t > S + L_a + L_r
    // This means loud background noise (e.g. rough seas) or high noise from own
    // vessel shadows target noise. We need to quantize the received noise
    // somehow or we could always find loudest source if background and own
    // noise is constant, because target noise would be the only varying factor.
    // The current way of detecting sounds is thus not realistic, as the highest
    // noise signal is always detected, no matter how big the difference to the
    // back- ground signals is... By using the formulas above, noise strengths
    // are multiplied (logarithmic scale!) instead of copied. Maybe this models
    // shadowing better... weak signal of 1 dB and strong signal of 50 dB.
    // Adding them and computing dB of addition gives 50.000055 dB, total
    // shadowed. Adding them in dB scale gives 51 dB, which is not right. What
    // about subtraction? 50 dB - 1 dB = 49 dB, in real scale 49.99995 dB,
    // nearly 50. So adding dB values is bad, as weaker signals get accounted
    // much stronger than they are. Solution maybe: quantize the target's noise,
    // so weaker signals have the same quantum as the background noise and
    // vanish.

    // fixme: ghost images appear with higher frequencies!!! seems to be a ghg
    // "feature"

    // add noise of vessels
    vector2 lp = listener->get_pos().xy();
    for (const auto* s : tmpships) {
        vector2 relpos  = s->get_pos().xy() - lp;
        double distance = relpos.length();
        double speed    = s->get_speed(); // s->get_throttle_speed();
        bool cavit      = s->screw_cavitation();
        angle direction_to_noise(relpos);
        angle rel_dir_to_noise  = direction_to_noise - hdg;
        bool noise_is_starboard = (rel_dir_to_noise.value_pm180() >= 0);
        // check if noise is on active side of phones
        if (listen_to_starboard == noise_is_starboard) {
            noise nsig = s->get_noise_signature().compute_signal_strength(distance, speed, cavit);
            // compute strengths for all bands
            for (unsigned b = 0; b < noise::NR_OF_FREQUENCY_BANDS; ++b) {
                double signalstrength =
                    compute_signal_strength_GHG(rel_dir_to_noise, noise::typical_frequency[b], rel_listening_dir);
                // printf("signalstrength is = %f\n", signalstrength);
                nsig.frequencies[b] *= signalstrength;
            }
            n += nsig;
        }
    }
    // now compute back to dB, quantize to integer dB values, to
    // simulate shadowing of weak signals by background noise
    // divide by receiver sensitivity before doing so, to avoid cutting off weak
    // signals.
    const double GHG_receiver_sensitivity_dB = -3; // weakest signal strength to be detectable
    double abs_strength = floor(std::max(n.compute_total_noise_strength_dB() - GHG_receiver_sensitivity_dB, 0.0))
                          + GHG_receiver_sensitivity_dB;

    // fixme: depending on listener angle, use only port or starboard phones to
    // listen to signals!
    //        (which set to use must be given as parameter) <OK>
    // fixme: add sensitivity of receiver (see harpoon docs...)  TO BE DONE NEXT
    // fixme: add noise produced by receiver/own ship            TO BE DONE NEXT
    // fixme: discretize strengths!!! (by angle and frequency!) <OK, MAYBE A BIT
    // CRUDE>
    //        this could be done also by quantizing the strength (in dB) of the
    //        signal, or both. To be tested...
    // fixme: identify type of noise (by sonarman). compute similarity to known
    //        noise signatures (minimum sim of squares of distances between
    //        measured values and known reference values). this should be done
    //        in another function... To do this store a list of typical noise
    //        signatures per ship category - that is also needed for creating
    //        the noise signals. <OK> BUT: this doesnt work well. To determine
    //        the signal type by distribution to just four frequency bands is
    //        not realistic. Signals are distuingished by their frequency
    //        mixture., CHANGE THIS LATER
    return make_pair(abs_strength, n.to_dB());
}

auto game::spawn_ship(ship&& obj) -> std::pair<const sea_object_id, ship>&
{
    return *ships.insert(std::make_pair(generate_id(), std::move(obj))).first;
}

auto game::spawn_submarine(submarine&& obj) -> std::pair<const sea_object_id, submarine>&
{
    return *submarines.insert(std::make_pair(generate_id(), std::move(obj))).first;
}

auto game::spawn_airplane(airplane&& obj) -> std::pair<const sea_object_id, airplane>&
{
    return *airplanes.insert(std::make_pair(generate_id(), std::move(obj))).first;
}

auto game::spawn(torpedo&& obj) -> torpedo&
{
    torpedoes.push_back(std::move(obj));
    // add events here, fixme torpedo fired event or so, launch noise
    return torpedoes.back();
}

auto game::spawn(gun_shell&& obj) -> gun_shell&
{
    // vary the sound effect based on the gun size
    auto calibre = obj.get_caliber();
    if (calibre <= 120.0) {
        events.push_back(std::make_unique<event_gunfire_light>(obj.get_pos()));
    } else if (calibre <= 200.0) {
        events.push_back(std::make_unique<event_gunfire_medium>(obj.get_pos()));
    } else {
        events.push_back(std::make_unique<event_gunfire_heavy>(obj.get_pos()));
    }
    gun_shells.push_back(std::move(obj));
    return gun_shells.back();
}

auto game::spawn(depth_charge&& obj) -> depth_charge&
{
    events.push_back(std::make_unique<event_depth_charge_in_water>(obj.get_pos()));
    depth_charges.push_back(std::move(obj));
    return depth_charges.back();
}

auto game::spawn(water_splash&& obj) -> water_splash&
{
    water_splashes.push_back(std::move(obj));
    // add events here
    return water_splashes.back();
}

auto game::spawn(convoy&& cv) -> std::pair<const sea_object_id, convoy>&
{
    return *convoys.insert(std::make_pair(generate_id(), std::move(cv))).first;
}

void game::spawn(std::unique_ptr<particle>&& pt)
{
    // fixme, maybe limit size of particles
    particles.push_back(std::move(pt));
}

void game::dc_explosion(const depth_charge& dc)
{
    // Create water splash.
    spawn(water_splash::depth_charge(dc.get_pos().xy().xy0(), get_model_store()));
    events.push_back(std::make_unique<event_depth_charge_exploding>(dc.get_pos()));

    // are subs affected?
    // fixme: ships can be damaged by DCs also...
    // fixme: ai should not be able to release dcs with a depth less than 30m or
    // so, to avoid suicide
    for (auto& [id, submarine] : submarines) {
        submarine.depth_charge_explosion(dc);
    }
}

void game::torp_explode(const torpedo* t)
{
    // each torpedo seems to explode twice, if it's only drawn twice or adds
    // twice the damage is unknown. fixme!
    spawn(water_splash::torpedo(t->get_pos().xy().xy0(), get_model_store()));
    events.push_back(std::make_unique<event_torpedo_explosion>(t->get_pos()));
}

void game::ship_sunk(const ship* s)
{
    events.push_back(std::make_unique<event_ship_sunk>());
    ostringstream oss;
    oss << texts::get(83) << " " << s->get_description(2);
    date d(static_cast<unsigned>(time));
    sunken_ships.emplace_back(
        d, s->get_description(2), s->get_modelname(), s->get_specfilename(), s->get_skin_layout(), s->get_tonnage());
}

/*
    fixme: does this function make sense in this place?
    it does:
    - move sensor (could be done in sensor's parent simulate function)
    - stores ping (could be done in a spawn_ping function)
    - detects objects (could be done in a get_asdic_detected_objects(thisping) )
    This function is yet the only "action" function. This concept doesn't seem
   to match with class game or the rest of the simulation. maybe ged rid of this
   (for simplicity of network game this would be useful)
*/
void game::ping_ASDIC(list<vector3>& contacts, sea_object* d, const bool& move_sensor, const angle& dir)
{
    sensor* s                = d->get_sensor(d->active_sonar_system);
    active_sonar_sensor* ass = nullptr;
    if (s != nullptr) {
        ass = dynamic_cast<active_sonar_sensor*>(s);
    }

    if (ass != nullptr) {
        if (!move_sensor) {
            ass->set_bearing(dir - d->get_heading());
        }

        // remember ping (for drawing)
        // fixme: seems redundant with event list...!
        pings.emplace_back(
            d->get_pos().xy(),
            ass->get_bearing() + d->get_heading(),
            time,
            ass->get_range(),
            ass->get_detection_cone());
        events.push_back(std::make_unique<event_ping>(d->get_pos()));

        // fixme: noise from ships can disturb ASDIC or may generate more
        // contacs. ocean floor echoes ASDIC etc...
        for (auto& [id, submarine] : submarines) {
            if (ass->is_detected(this, d, &submarine)) {
                contacts.push_back(submarine.get_pos() + vector3(rnd(40) - 20.0F, rnd(40) - 20.0F, rnd(40) - 20.0F));
            }
        }

        if (move_sensor) {
            sensor::sensor_move_mode mode = sensor::sweep;
            // Ships cannot rotate the active sonar sensor because of
            // their screws. A submarine can do so when it is submerged
            // and running on electric engines.
            auto* sub = dynamic_cast<submarine*>(d);
            if ((sub != nullptr) && sub->is_submerged() && sub->is_electric_engine()) {
                mode = sensor::rotate;
            }
            ass->auto_move_bearing(mode);
        }
    }
}

template<class C>
auto check_units(torpedo* t, std::unordered_map<sea_object_id, C>& units) -> ship*
{
    const vector3& t_pos = t->get_pos();
    bv_tree::param p0    = t->compute_bv_tree_params();
    for (auto& [id, obj] : units) {
        // fixme use bv_trees here with special code for magnetic ignition
        // torpedoes like intersection of sphere around torpedo head with bv
        // tree
        const vector3& partner_pos = obj.get_pos();
        matrix4 rel_trans          = matrix4::trans(partner_pos - t_pos);
        bv_tree::param p1          = obj.compute_bv_tree_params();
        p1.transform               = rel_trans * p1.transform;
        vector3f contact_point;
        if (bv_tree::closest_collision(p0, p1, contact_point)) {
            return &obj;
        }
        // old code:
        // if ( is_collision ( t, obj ) )
        //	return obj;
    }

    return nullptr;
}

auto game::check_torpedo_hit(torpedo* t, bool runlengthfailure) -> bool
{
    auto* s = check_units(t, ships);

    if (s == nullptr) {
        s = check_units(t, submarines);
    }

    if (s != nullptr) {
        if (runlengthfailure) {
            events.push_back(std::make_unique<event_torpedo_dud_shortrange>());
        } else {
            // Only ships that are alive can be sunk. Already sinking
            // or destroyed ships cannot be destroyed again.
            if (!s->is_alive()) {
                return false;
            }

            // now check if torpedo fuse works
            if (!t->test_contact_fuse(*this)) {
                events.push_back(std::make_unique<event_torpedo_dud>());
                return true;
            }

            if (s->damage(t->get_pos(), t->get_hit_points(), *this)) {
                ship_sunk(s);
            } else {
                s->ignite(*this);
            }

            // explosion of torpedo
            spawn(std::make_unique<explosion_particle>(s->get_pos() + vector3(0, 0, 5)));
            torp_explode(t);
        }
        return true;
    }

    return false;
}

auto game::contact_in_direction(const sea_object* o, const angle& direction) const -> sea_object_id
{
    sea_object_id result;

    // Try ship first.
    result = ship_in_direction_from_pos(o, direction);

    // Now submarines.
    if (!is_valid(result)) {
        result = sub_in_direction_from_pos(o, direction);
    }

    return result;
}

auto game::ship_in_direction_from_pos(const sea_object* o, const angle& direction) const -> sea_object_id
{
    const sensor* s          = o->get_sensor(o->lookout_system);
    const lookout_sensor* ls = nullptr;
    sea_object_id result;

    if (s != nullptr) {
        ls = dynamic_cast<const lookout_sensor*>(s);
    }

    if (ls != nullptr) {
        double angle_diff = 30; // fixme: use range also, use ship width's etc.
        for (const auto& [id, ship] : ships) {
            // Only a visible and intact submarine can be selected.
            if (ls->is_detected(this, o, &ship) && (ship.is_alive())) {
                vector2 df          = ship.get_pos().xy() - o->get_pos().xy();
                double new_ang_diff = (angle(df)).diff(direction);
                if (new_ang_diff < angle_diff) {
                    angle_diff = new_ang_diff;
                    result     = id;
                }
            }
        }
    }
    return result;
}

auto game::sub_in_direction_from_pos(const sea_object* o, const angle& direction) const -> sea_object_id
{
    const sensor* s          = o->get_sensor(o->lookout_system);
    const lookout_sensor* ls = nullptr;
    sea_object_id result;

    if (s != nullptr) {
        ls = dynamic_cast<const lookout_sensor*>(s);
    }

    if (ls != nullptr) {
        double angle_diff = 30; // fixme: use range also, use ship width's etc.
        for (const auto& [id, submarine] : submarines) {
            // Only a visible and intact submarine can be selected.
            if (ls->is_detected(this, o, &submarine) && (submarine.is_alive())) {
                vector2 df          = submarine.get_pos().xy() - o->get_pos().xy();
                double new_ang_diff = (angle(df)).diff(direction);
                if (new_ang_diff < angle_diff) {
                    angle_diff = new_ang_diff;
                    result     = id;
                }
            }
        }
    }
    return result;
}

auto game::get_torpedo_for_camera_track(unsigned nr) const -> const torpedo*
{
    if (nr < torpedoes.size()) {
        for (const auto& torpedo : torpedoes) {
            if (torpedo.is_reference_ok()) {
                return &torpedo;
            }
        }
    }
    return nullptr;
}

/* old code for torpedo collision. to be removed later, fixme.
bool game::is_collision(const sea_object* s1, const sea_object* s2) const
{
    // bounding volume collision test
    float br1 = s1->get_bounding_radius();
    float br2 = s2->get_bounding_radius();
    float br = br1 + br2;
    vector2 p1 = s1->get_pos().xy();
    vector2 p2 = s2->get_pos().xy();
    if (p1.square_distance(p2) > br*br) return false;

    // exact collision test
    // compute direction and their normals of both objects
    vector2 d1 = s1->get_heading().direction();
    vector2 n1 = d1.orthogonal();
    vector2 d2 = s2->get_heading().direction();
    vector2 n2 = d2.orthogonal();
    double l1 = s1->get_length(), l2 = s2->get_length();
    double w1 = s1->get_width(), w2 = s2->get_width();

    // base points
    vector2 pb1 = p1 - d1 * (l1/2) - n1 * (w1/2);
    vector2 pb2 = p2 - d2 * (l2/2) - n2 * (w2/2);

    // check if any of obj2 corners is inside obj1
    vector2 pd2[4] = {d2*l2, n2*w2, -d2*l2, -n2*w2};
    vector2 pdiff = pb2 - pb1;
    for (int i = 0; i < 4; ++i) {
        double s = pdiff.x * d1.x + pdiff.y * d1.y;
        if (0 <= s && s <= l1) {
            double t = pdiff.y * d1.x - pdiff.x * d1.y;
            if (0 <= t && t <= w1) {
                return true;
            }
        }
        pdiff += pd2[i];
    }

    // check if any of obj1 corners is inside obj2
    vector2 pd1[4] = {d1*l1, n1*w1, -d1*l1, -n1*w1};
    pdiff = pb1 - pb2;
    for (int i = 0; i < 4; ++i) {
        double s = pdiff.x * d2.x + pdiff.y * d2.y;
        if (0 <= s && s <= l2) {
            double t = pdiff.y * d2.x - pdiff.x * d2.y;
            if (0 <= t && t <= w2) {
                return true;
            }
        }
        pdiff += pd1[i];
    }
    return false;
}

bool game::is_collision(const sea_object* s, const vector2& pos) const
{
    // bounding volume collision test
    float br = s->get_bounding_radius();
    vector2 p = s->get_pos().xy();
    if (p.square_distance(pos) > br*br) return false;

    // exact collision test
    // compute direction and their normals
    vector2 d = s->get_heading().direction();
    vector2 n = d.orthogonal();
    double l = s->get_length(), w = s->get_width();

    vector2 pb = p - d * (l/2) - n * (w/2);
    vector2 pdiff = pos - pb;
    double r = pdiff.x * d.x + pdiff.y * d.y;
    if (0 <= r && r <= l) {
        double t = pdiff.y * d.x - pdiff.x * d.y;
        if (0 <= t && t <= w) {
            return true;
        }
    }
    return false;
}
*/

auto game::get_depth_factor(const vector3& sub) const -> double
{
    return (1.0F - 0.5F * sub.z / 400.0F);
}

auto game::get_object(sea_object_id id) -> sea_object&
{
    // fixme need more here?
    auto it = ships.find(id);
    if (it == ships.end()) {
        auto it2 = submarines.find(id);
        if (it2 == submarines.end()) {
            THROW(error, "invalid sea_object_id for ship");
        }
        return it2->second;
    }
    return it->second;
}

auto game::get_ship(sea_object_id id) -> ship&
{
    auto it = ships.find(id);
    if (it == ships.end()) {
        THROW(error, "invalid sea_object_id for ship");
    }
    return it->second;
}

auto game::get_convoy(sea_object_id id) -> convoy&
{
    auto it = convoys.find(id);
    if (it == convoys.end()) {
        THROW(error, "invalid sea_object_id for convoy");
    }
    return it->second;
}

auto game::get_id(const sea_object& s) const -> sea_object_id
{
    // fixme ugly! should only be available in game_editor later!
    for (const auto& [id, ship] : ships) {
        if (&ship == &s) {
            return id;
        }
    }
    for (const auto& [id, submarine] : submarines) {
        if (&submarine == &s) {
            return id;
        }
    }
    // fixme more here?
    THROW(error, "Invalid sea_object to request id");
}

auto game::visible_surface_objects(const sea_object* o) const -> vector<const sea_object*>
{
    auto vships      = visible_ships(o);
    auto vsubmarines = visible_submarines(o);
    auto vairplanes  = visible_airplanes(o);

    // fixme: adding RADAR-detected ships to a VISIBLE-objects function is a bit
    // weird... this leads to wrong results if radar detected objects are
    // handled differently, like different display on map, or drawing (not
    // visible!), or for AI!
    auto rships      = radar_ships(o);
    auto rsubmarines = radar_submarines(o);

    vector<const sea_object*> result;
    result.reserve(vships.size() + vsubmarines.size() + vairplanes.size() + rships.size() + rsubmarines.size());
    append_vec(result, vships);
    append_vec(result, vsubmarines);
    append_vec(result, vairplanes);
    append_vec(result, rships);
    append_vec(result, rsubmarines);
    return result;
}

auto game::visible_sea_objects(const sea_object* o) const -> vector<const sea_object*>
{
    auto vships      = visible_ships(o);
    auto vsubmarines = visible_submarines(o);
    auto vairplanes  = visible_airplanes(o);
    auto vtorpedoes  = visible_torpedoes(o);
    vector<const sea_object*> result;
    result.reserve(vships.size() + vsubmarines.size() + vairplanes.size() + vtorpedoes.size());
    append_vec(result, vships);
    append_vec(result, vsubmarines);
    append_vec(result, vairplanes);
    append_vec(result, vtorpedoes);
    return result;
}

auto game::sonar_acoustical_torpedo_target(const torpedo* o) const -> const ship*
{
    const ship* loudest_object      = nullptr;
    double loudest_object_sf        = 0.0F;
    const sensor* s                 = o->get_sensor(o->passive_sonar_system);
    const passive_sonar_sensor* pss = nullptr;

    if (s != nullptr) {
        pss = dynamic_cast<const passive_sonar_sensor*>(s);
    }

    if (pss != nullptr) {
        for (const auto& [id, ship] : ships) {
            double sf = 0.0F;
            if (pss->is_detected(sf, this, o, &ship)) {
                if (sf > loudest_object_sf) {
                    loudest_object_sf = sf;
                    loudest_object    = &ship;
                }
            }
        }

        for (const auto& [id, submarine] : submarines) {
            double sf = 0.0F;
            if (pss->is_detected(sf, this, o, &submarine)) {
                if (sf > loudest_object_sf) {
                    loudest_object_sf = sf;
                    loudest_object    = &submarine;
                }
            }
        }
    }

    return loudest_object;
}

auto game::is_day_mode() const -> bool
{
    vector3 sundir;
    double br = compute_light_brightness(player->get_pos(), sundir);
    return (br > 0.3); // fixme: a bit crude. brightness has 0.2 ambient...
}

// fixme: it would be better to keep such a vector around and not recompute it
// for every object that needs it it must be recomputed only when spawn is
// called or compress removes objects
auto game::get_all_ships() const -> vector<const ship*>
{
    vector<const ship*> allships(torpedoes.size() + submarines.size() + ships.size());
    unsigned k = 0;
    // fixme awkward, torpedo is no ship!
    for (const auto& torpedo : torpedoes) {
        allships[k++] = &torpedo;
    }
    for (const auto& [id, submarine] : submarines) {
        allships[k++] = &submarine;
    }
    for (const auto& [id, ship] : ships) {
        allships[k++] = &ship;
    }
    return allships;
}

void game::check_collisions()
{
    // torpedoes are special... check collision only for impact fuse?
    auto allships = get_all_ships();
    unsigned m    = torpedoes.size();

    // now check for collisions for all ships idx i with partner index >
    // max(m,i) so we have N^2/2 tests and not N^2. we don't check for
    // torpedo<->torpedo collisions.
    for (unsigned i = 0; i < allships.size(); ++i) {
        const vector3& actor_pos = allships[i]->get_pos();
        // use partner's position relative to actor
        bv_tree::param p0 = allships[i]->compute_bv_tree_params();
        for (unsigned j = std::max(i + 1, m); j < allships.size(); ++j) {
            const vector3& partner_pos = allships[j]->get_pos();
            matrix4 rel_trans          = matrix4::trans(partner_pos - actor_pos);
            bv_tree::param p1          = allships[j]->compute_bv_tree_params();
            p1.transform               = rel_trans * p1.transform;
#if 0
            std::list<vector3f> contact_points;
            bool intersects = bv_tree::collides(p0, p1, contact_points);
            if (intersects) {
                // compute intersection pos, sum of contact points
                vector3f sum;
                unsigned sum_count = 0;
                for (std::list<vector3f>::iterator it = contact_points.begin(); it != contact_points.end(); ++it) {
                    sum += *it;
                    ++sum_count;
                }
                sum *= 1.0f/sum_count;
                collision_response(*allships[i], *allships[j], vector3(sum) + actor_pos);
            }
#else
            vector3f contact_point;
            bool intersects = bv_tree::closest_collision(p0, p1, contact_point);
            if (intersects) {
                collision_response(
                    const_cast<ship&>(*allships[i]), const_cast<ship&>(*allships[j]), contact_point + actor_pos);
            }
#endif
        }
    }

    // collision response:
    // the two objects collide at a position P that is relative to their center
    // (P(a) and P(b)). We have to compute their velocity (direction and
    // strength, a vector), which can be derived from the rigid body state
    // variables. The direction of the response force is orthogonal to the
    // surface normal, but maybe direction of P(a)-P(b) would do it as well. In
    // theory response vector is just +/- (P(a)-P(b)), for A/B (or vice versa),
    // multiplied by some dampening factor simulating friction. just apply the
    // force instantly to the body states to change their velocities and we are
    // done...

    // fixme: collision checks between fast moving small objects and bigger
    // objects (like shells vs. ships) should be done here too, and not only in
    // gun_shell class. Later other objects may need that code too (machine
    // cannons, guns etc).

    // fixme remove obsolete code from bbox/voxel collision checking
}

void game::collision_response(sea_object& a, sea_object& b, const vector3& collision_pos)
{
#if 0
    // for debugging - fixme not visible. is position correct?!
    spawn_particle(new marker_particle(collision_pos));
#endif
    // compute directions to A, B to compute collision response direction
    const vector3& A = a.get_pos();
    const vector3& B = b.get_pos();
    // fixme: relative position is ok, why there arent any visible markers?!
    //	printf("pos %f %f %f   a %f %f %f    b %f %f
    //%f\n",collision_pos.x,collision_pos.y,collision_pos.z,
    //	       A.x,A.y,A.z,B.x,B.y,B.z);
    vector3 dA = (A - collision_pos).normal();
    vector3 dB = (B - collision_pos).normal();
    vector3 N;
    if (dA * dB < 1e-4) {
        N = dA;
    } else {
        N = dA.cross(dB).normal().cross((dA + dB).normal()).normal();
    }
    log_debug("collision response dir=" << N);

    // compute speed of A and B at the collision point, compute opposing force
    // and apply directly to A, B, modifying their speed we need to compute
    // velocity at collision point of A, B. the scalar product with N gives
    // collision speed, its negative value must be dampened and applied as force
    // along N. clip speed so positive values along N are not used, i.e. when
    // collision response has already been applied, and velocity vector points
    // already along N, do not apply it again, even if objects still collide at
    // next physics step. Speed at collision point is linear speed plus relative
    // vector cross linear velocity add a function to sea_object to compute
    // linear speed of any relative point.
    // fixme
    vector3 vA = a.compute_linear_velocity(collision_pos);
    vector3 vB = b.compute_linear_velocity(collision_pos);

    double vrel = N * (vA - vB);
    log_debug("linear velocity A=" << vA << "   B=" << vB << " vrel=" << vrel);
    // if the contact points move away each other, do nothing
    if (vrel > 0) {
        return;
    }
    const double epsilon = vrel < -4.0 ? 0.5 : 1.0 + vrel / 8.0; // dampening of response force
    double j =
        -(1.0 + epsilon) * vrel
        / (a.compute_collision_response_value(collision_pos, N) + b.compute_collision_response_value(collision_pos, N));
    log_debug("j=" << j << " force=" << (j * N));
    a.apply_collision_impulse(collision_pos, j * N);
    b.apply_collision_impulse(collision_pos, -j * N);
}

auto game::compute_light_brightness(const vector3& viewpos, vector3& sundir) const -> double
{
    // fixme: if sun is blocked by clouds, light must be darker...
    sundir = compute_sun_pos(viewpos).normal();
    // in reality the brightness is equal to sundir.z, but the sun is so bright
    // that we stretch and clamp this value
    double lightbrightness = sundir.z * 2.0;
    if (lightbrightness > 1.0) {
        lightbrightness = 1.0;
    }
    if (lightbrightness < 0.0) {
        lightbrightness = 0.0;
    }
    // fixme add moon light at night
    return lightbrightness * 0.8 + 0.2; // some ambient value
}

auto game::compute_light_color(const vector3& viewpos) const -> colorf
{
    // fixme: sun color can be yellow/orange at dusk/dawn
    // attempt at having some warm variation at light color, previously it was
    // uniform, so we'll try a function of elevation (sundir.z to be precise)
    // Ratios of R, G, B channels are meant to remain in the orange area
    vector3 sundir;
    double lbrit           = compute_light_brightness(viewpos, sundir);
    double color_elevation = sundir.z;
    // check for clamping here...
    double lr = lbrit * (1 - pow(cos(color_elevation + .47), 25));
    double lg = lbrit * (1 - pow(cos(color_elevation + .39), 20));
    double lb = lbrit * (1 - pow(cos(color_elevation + .22), 15));

    return {static_cast<float>(lr), static_cast<float>(lg), static_cast<float>(lb)};
}

/*	************** sun and moon *********************
    The model:
    Sun, moon and earth have an local space, moon and earth rotate around their
   y-axis. y-axes are all up, that means earth's y-axis points to the north
   pole. The moon rotates counter clockwise around the earth in 27 1/3 days (one
   sidereal month). The earth rotates counter clockwise around the sun in 365d
   5h 48m 46.5s. The earth rotates around itself in 23h 56m 4.1s (one sidereal
   day). Earths rotational axis is rotated by 23.45 degrees. Moon orbits in a
   plane that is 5,15 degress rotated to the xz-plane (plane that earth rotates
   in, sun orbit). The moon is at its southmost position when it is a full moon
    Due to the earth rotation around the sun, the days/months appear longer (the
   earth rotation must compensate the movement). So the experienced lengths are
   24h for a day and 29.5306 days for a full moon cycle. Earth rotational axis
   points towards the sun at top of summer on the northern hemisphere (around
   21st. of June). On top of summer (northern hemisphere) the earth orbit pos is
   0. On midnight at longitude 0, the earth rotation is 0. At a full moon the
   moon rotation/orbit position is 0. As result the earth takes ~ 366 rotations
   per year (365d 5h 48m 46.5s / 23h 56m 4.09s = 366.2422) We need the exact
   values/configuration on 1.1.1939, 0:0am. And we need the configuration of the
   moon rotational plane at this date and time.
*/

/*
what has to be fixed for sun/earth/moon simulation:
get exact distances and diameters (done)
get exact rotation times (sidereal day, not solar day) for earth and moon (done)
get exact orbit times for earth and moon around sun / earth (done)
get angle of rotational axes for earth and moon (fixme, 23.45 and 5.15) (done)
get direction of rotation for earth and moon relative to each other (done)
get position of objects and axis states for a fix date (optimum 1.1.1939) (!only
moon needed, fixme!) compute formulas for determining the positions for the
following years (fixme) write code that computes sun/moon pos relative to earth
and relative to local coordinates (fixme) draw moon with phases (fixme)
*/

auto game::compute_sun_pos(const vector3& viewpos) const -> vector3
{
    double yearang    = 360.0 * myfrac((time + 10 * 86400) / constant::EARTH_ORBIT_TIME);
    double dayang     = 360.0 * (viewpos.x / constant::EARTH_PERIMETER + myfrac(time / 86400.0));
    double longang    = 360.0 * viewpos.y / constant::EARTH_PERIMETER;
    matrix4 sun2earth = matrix4::rot_y(-90.0) * matrix4::rot_z(-longang) * matrix4::rot_y(-(yearang + dayang))
                        * matrix4::rot_z(constant::EARTH_ROT_AXIS_ANGLE) * matrix4::rot_y(yearang)
                        * matrix4::trans(-constant::EARTH_SUN_DISTANCE, 0, 0) * matrix4::rot_y(-yearang);
    return sun2earth.column3(3);
}

auto game::compute_moon_pos(const vector3& viewpos) const -> vector3
{
    double yearang  = 360.0 * myfrac((time + 10 * 86400) / constant::EARTH_ORBIT_TIME);
    double dayang   = 360.0 * (viewpos.x / constant::EARTH_PERIMETER + myfrac(time / 86400.0));
    double longang  = 360.0 * viewpos.y / constant::EARTH_PERIMETER;
    double monthang = 360.0 * myfrac(time / constant::MOON_ORBIT_TIME_SYNODIC) + constant::MOON_POS_ADJUST;

    matrix4 moon2earth = matrix4::rot_y(-90.0) * matrix4::rot_z(-longang) * matrix4::rot_y(-(yearang + dayang))
                         * matrix4::rot_z(constant::EARTH_ROT_AXIS_ANGLE) * matrix4::rot_y(yearang)
                         * matrix4::rot_z(-constant::MOON_ORBIT_AXIS_ANGLE)
                         * matrix4::rot_y(monthang + constant::MOON_POS_ADJUST)
                         * matrix4::trans(constant::MOON_EARTH_DISTANCE, 0, 0);

    return moon2earth.column3(3);
}

auto game::compute_water_height(const vector2& pos) const -> double
{
    return mywater->get_height(pos);
}

// function is not used yet.
// give relative position, length*vis, width*vis and course
auto is_in_ellipse(const vector2& p, double xl, double yl, const angle& head) -> bool
{
    const vector2 hd = head.direction();
    const double t1  = (p.x * hd.x + p.y * hd.y);
    const double t2  = (p.y * hd.x - p.x * hd.y);
    return ((t1 * t1) / (xl * xl) + (t2 * t2) / (yl * yl)) < 1;
}

void game::freeze_time()
{
    if (freezetime_start > 0) {
        THROW(error, "freeze_time() called twice!");
    }
    freezetime_start = SYS().millisec();
    //	printf("time frozen at: %u\n", freezetime_start);
}

void game::unfreeze_time()
{
    unsigned freezetime_end = SYS().millisec();
    //	printf("time UNfrozen at: %u (%u)\n", freezetime_end, freezetime_end -
    // freezetime_start);
    freezetime += freezetime_end - freezetime_start;
    freezetime_start = 0;
}

auto game::is_valid(sea_object_id id) const -> bool
{
    if (id == sea_object_id::invalid) {
        return false;
    }
    // Only ships or submarines can be targeted (later airplanes)
    auto it = ships.find(id);
    if (it == ships.end()) {
        auto it2 = submarines.find(id);
        if (it2 == submarines.end()) {
            return false;
        }
        return it2->second.is_reference_ok();
    }
    return it->second.is_reference_ok();
}
