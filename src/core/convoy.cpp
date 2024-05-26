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

// convoys
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#include "convoy.hpp"

#include "ai.hpp"
#include "datadirs.hpp"
#include "game.hpp"
#include "model.hpp"
#include "ship.hpp"
#include "system_interface.hpp"

#include <cmath>
#include <utility>
using std::string;

// fixme: the whole file is covered with outcommented lines that have to be
// implemented in a sane way with the new class model (2004/07/15)

struct ship_probability
{
    float prob;
    const char* name;
};

ship_probability escortships[] = {
    {0.1, "destroyer_oclass"},
    {0.3, "destroyer_javelin"},
    {0.4, "destroyer_tribal"},
    {0.5, "destroyer_kclass"},
    {0.5, "flowercorvette_hcn"},
    {0.5, "flowercorvette_hrcn"},
    {0.5, "FlowerCorvette_RN"},
    {0.0, nullptr}};

ship_probability civilships[] = {
    //	{ 0.1, "troopship_medium" },
    //	{ 0.1, "tanker_small" },
    {0.1, "tanker_kennebak"},
    {0.1, "northsands_camship"},
    //	{ 0.1, "merchant_large" },
    //	{ 0.1, "merchant_medium" },
    //	{ 0.1, "merchant_small" },
    //	{ 0.1, "freighter_large" },
    {0.1, "libertyship1941"},
    {0.1, "libertyship1942"},
    {0.1, "libertyship1943"},
    {0.1, "fortship1941"},
    {0.1, "fortship1943"},
    {0.1, "fortshipciv"},
    {0.1, "empireprotector"},
    {0.1, "empirefaithcam"},
    {0.1, "empirelawrencecam"},
    {0.1, "empiredabchick"},
    {0.0, nullptr}};

auto get_random_ship(ship_probability* p, game& gm) -> string
{
    // count how many ships we have, sum of probabilities
    double psum = 0;
    unsigned k  = 0;
    for (; p[k].name != nullptr; ++k) {
        psum += p[k].prob;
    }
    // printf("psum %f k %u\n", psum, k);
    // now compute one random ship
    double r = gm.randomf() * psum;
    for (unsigned i = 0; i < k; ++i) {
        r -= p[i].prob;
        if (r < 0) {
            // found it!
            // printf("chosen %u %s\n", i, p[i].name);
            return string(p[i].name);
        }
    }
    // weird error, off limits, return first
    return string(p[0].name);
}

convoy::convoy(game& gm, convoy::types type_, convoy::esctypes esct_)
{
    // myai = new ai(this, ai::convoy);

    waypoints.emplace_back(0, 0);
    for (int wp = 0; wp < 4; ++wp) {
        waypoints.emplace_back(gm.randomf() * 300000 - 150000, gm.randomf() * 300000 - 150000);
    }
    angle heading     = angle(*(++waypoints.begin()) - *(waypoints.begin()));
    vector2 coursevec = heading.direction();
    // position = vector2(0, 0);

    double intershipdist = 1000.0;    // distance between ships. it is 1000m
                                      // sidewards and 600m forward... fixme
    double convoyescortdist = 3000.0; // distance of escorts to convoy
    double interescortdist  = 1500.0; // distance between escorts, seems a bit low...

    // merchant convoy?
    if (type_ == small || type_ == medium || type_ == large) {
        name        = "Custom"; // fixme...
        auto cvsize = unsigned(type_);

        // speed? could be a slow or fast convoy (~4 or ~8 kts).
        int throttle = 4 + gm.randomf() * 8;
        velocity     = sea_object::kts2ms(throttle);

        // compute size and structure of convoy
        unsigned nrships = (2 << cvsize) * 10 + gm.randomf() * 10 - 5;
        auto sqrtnrships = unsigned(std::floor(std::sqrt(float(nrships))));
        unsigned shps    = 0;
        for (unsigned j = 0; j <= sqrtnrships; ++j) {
            if (shps >= nrships) {
                break;
            }
            int dy = int(j) - sqrtnrships / 2;
            for (unsigned i = 0; i <= sqrtnrships; ++i) {
                if (shps >= nrships) {
                    break;
                }
                int dx = int(i) - sqrtnrships / 2;
                // fixme!!! replace by parsing ship dir for available types or
                // hardcode them!!! each ship in data/ships stores its type. but
                // probability can not be stored...
                string shiptype = get_random_ship(civilships, gm);
                xml_doc doc(data_file().get_filename(shiptype));
                doc.load();
                ship s(gm.get_date(), gm.get_model_store(), doc.first_child());
                s.set_random_skin_name(gm.get_date());
                vector2 pos = vector2(
                    dx * intershipdist + gm.randomf() * 60.0 - 30.0, dy * intershipdist + gm.randomf() * 60.0 - 30.0);
                pos = pos.matrixmul(coursevec, coursevec.orthogonal());
                s.manipulate_position((waypoints.front() + pos).xy0());
                s.manipulate_heading(heading);
                s.manipulate_speed(velocity);
                s.set_throttle(throttle);
                merchants.emplace_back(gm.spawn_ship(std::move(s)).first, pos);
                ++shps;
            }
        }

        // compute nr of escorts
        unsigned nrescs = esct_ * 5;
        for (unsigned i = 0; i < nrescs; ++i) {
            // fixme: give commands/tasks to escorts: "patrol left side of
            // convoy" etc.
            int side = i % 4;
            float dx = 0;
            float dy = 0;
            float nx = 0;
            float ny = 0;
            switch (side) {
                case 0:
                    dx = 0;
                    dy = 1;
                    break;
                case 1:
                    dx = 1;
                    dy = 0;
                    break;
                case 2:
                    dx = 0;
                    dy = -1;
                    break;
                case 3:
                    dx = -1;
                    dy = 0;
                    break;
            }
            nx = -dy;
            ny = dx;
            dx *= sqrtnrships / 2 * intershipdist + convoyescortdist;
            dy *= sqrtnrships / 2 * intershipdist + convoyescortdist;
            nx *= (int(nrescs / 4) - 1) * interescortdist - int(i / 4) * interescortdist;
            ny *= (int(nrescs / 4) - 1) * interescortdist - int(i / 4) * interescortdist;
            string shiptype = get_random_ship(escortships, gm);
            xml_doc doc(data_file().get_filename(shiptype));
            doc.load();
            ship s(gm.get_date(), gm.get_model_store(), doc.first_child());
            s.set_random_skin_name(gm.get_date());
            vector2 pos = vector2(dx + nx + gm.randomf() * 100.0 - 50.0, dy + ny + gm.randomf() * 100.0 - 50.0);
            pos         = pos.matrixmul(coursevec, coursevec.orthogonal());
            s.manipulate_position((waypoints.front() + pos).xy0());
            s.manipulate_heading(heading);
            s.manipulate_speed(velocity);
            s.set_throttle(throttle);
            escorts.emplace_back(gm.spawn_ship(std::move(s)).first, pos);
        }

        return;
    }

    // fixme
    name = "SC-122";

    // fixme
    switch (type_) {
        case small:
            break;
        case medium:
            break;
        case large:
            break;
        case battleship:
            break;
        case supportgroup:
            break;
        case carrier:
            break;
    }
}

convoy::convoy(const vector2& pos, std::string name_)
    : position(pos)
    , name(std::move(name_))
{
}

auto convoy::add_ship(game& gm, sea_object_id id) -> bool
{
    auto& shp    = gm.get_ship(id);
    vector2 spos = shp.get_pos().xy() - position;
    switch (shp.get_class()) {
        case WARSHIP:
            warships.emplace_back(id, spos);
            return true;
        case ESCORT:
            escorts.emplace_back(id, spos);
            return true;
        case MERCHANT:
            merchants.emplace_back(id, spos);
            return true;
        default:
            // can't add this to convoy
            return false;
    }
}

void convoy::load(const xml_elem& parent)
{
    name        = parent.attr("name");
    position    = parent.child("position").attrv2();
    velocity    = parent.child("velocity").attrf();
    xml_elem mc = parent.child("merchants");
    merchants.clear();
    // merchants.reserve(mc.attru("nr"));
    for (auto elem : mc.iterate("merchant")) {
        merchants.emplace_back(elem.attru("ref"), elem.attrv2());
    }
    xml_elem ws = parent.child("warships");
    warships.clear();
    // warships.reserve(ws.attru("nr"));
    for (auto elem : ws.iterate("warship")) {
        warships.emplace_back(elem.attru("ref"), elem.attrv2());
    }
    xml_elem es = parent.child("escorts");
    escorts.clear();
    // escorts.reserve(es.attru("nr"));
    for (auto elem : es.iterate("escort")) {
        escorts.emplace_back(elem.attru("ref"), elem.attrv2());
    }
    xml_elem wp = parent.child("waypoints");
    waypoints.clear();
    // waypoints.reserve(wp.attru("nr"));
    for (auto elem : wp.iterate("waypoint")) {
        waypoints.push_back(elem.attrv2());
    }
}

void convoy::save(xml_elem& parent) const
{
    parent.set_attr(name, "name");
    parent.add_child("position").set_attr(position);
    parent.add_child("velocity").set_attr(velocity);
    xml_elem mc = parent.add_child("merchants");
    mc.set_attr(unsigned(merchants.size()), "nr");
    for (const auto& merchant : merchants) {
        xml_elem mc2 = mc.add_child("merchant");
        mc2.set_attr(merchant.first.id, "ref");
        mc2.set_attr(merchant.second);
    }
    xml_elem ws = parent.add_child("warships");
    ws.set_attr(unsigned(warships.size()), "nr");
    for (const auto& warship : warships) {
        xml_elem ws2 = ws.add_child("warship");
        ws2.set_attr(warship.first.id, "ref");
        ws2.set_attr(warship.second);
    }
    xml_elem es = parent.add_child("escorts");
    es.set_attr(unsigned(escorts.size()), "nr");
    for (const auto& escort : escorts) {
        xml_elem es2 = es.add_child("escort");
        es2.set_attr(escort.first.id, "ref");
        es2.set_attr(escort.second);
    }
    xml_elem wp = parent.add_child("waypoints");
    wp.set_attr(unsigned(waypoints.size()), "nr");
    for (auto waypoint : waypoints) {
        wp.add_child("pos").set_attr(waypoint);
    }
}

auto convoy::get_nr_of_ships() const -> unsigned
{
    return merchants.size() + warships.size() + escorts.size();
}

void convoy::simulate(double delta_time, game& gm)
{
    // fixme: only control target every n seconds or so.
    // compute global velocity. as first direction to destination.
    // aim to next waypoint if that isn't already reached.
    vector2 dir;
    while (!waypoints.empty()) {
        if (position.square_distance(waypoints.front()) < 10.0) { // 3.3m are exact enough
            waypoints.pop_front();
        } else {
            dir = (waypoints.front() - position).normal();
            break;
        }
    }
    // note that convoy stops after last waypoints was reached because dir is
    // zero in that case
    vector2 global_velocity = dir * velocity;

    // add better moving simulation? but it should be exact enough for convoys.
    position += global_velocity * delta_time;

    // check for ships to be erased
    for (auto it = merchants.begin(); it != merchants.end();) {
        auto it2 = it;
        ++it;
        if (!gm.is_valid(it2->first)) {
            merchants.erase(it2);
        }
    }
    for (auto it = warships.begin(); it != warships.end();) {
        auto it2 = it;
        ++it;
        if (!gm.is_valid(it2->first)) {
            warships.erase(it2);
        }
    }
    for (auto it = escorts.begin(); it != escorts.end();) {
        auto it2 = it;
        ++it;
        if (!gm.is_valid(it2->first)) {
            escorts.erase(it2);
        }
    }

    // fixme: set target course for ships here, but only every n seconds.
    // note that this must not override the ship's current steering, because it
    // could do an evasive manouver etc. An better alternative would be if ships
    // could request their target position at the convoy and do that every n
    // seconds. The AI of a ship could do so.

    //	if ( myai )//fixme: my is ai sometimes != 0 although it is disabled?!
    //		myai->act(gm, delta_time);

    // convoy erased?
    if (merchants.size() + warships.size() + escorts.size() == 0) {
        // fixme
        // destroy();
    }
}

void convoy::add_contact(game& gm, const vector3& pos) // fixme: simple, crude, ugly
{
    for (auto& escort : escorts) {
        gm.get_object(escort.first).get_ai()->attack_contact(pos);
    }
}
