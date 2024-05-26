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

#pragma once

#include "ai.hpp"
#include "vector2.hpp"

#include <list>
#include <memory>
#include <new>

///\brief Grouping of ships and other objects with central control.
/** This class stores and manages groups of ships and other objects forming a
   convoy. Ships are listed as escorts, merchants or warships. Convoy control is
   handled via special AI.
*/
class convoy
{
  protected:
    std::list<std::pair<sea_object_id, vector2>> merchants, warships, escorts;
    std::list<vector2> waypoints;

    std::unique_ptr<ai> myai; // fixme: maybe one ship should act for the
                              // convoy, the ship with the convoy commander.
                              // when it is sunk, convoy is desorganized etc.

    double remaining_time{0.0}; // time to next thought/situation analysis, fixme move to ai!
    vector2 position;
    double velocity{}; // local (forward) velocity.
    // alive_stat?
    std::string name;

  public:
    enum types
    {
        small,
        medium,
        large,
        battleship,
        supportgroup,
        carrier
    };
    enum esctypes
    {
        etnone,
        etsmall,
        etmedium,
        etlarge
    }; // escort size

    /// create empty convoy for loading (used by class game)
    convoy() = default;

    /// create custom convoy
    convoy(game& gm, types type_, esctypes esct_);

    /// create empty convoy (only used in the editor!)
    convoy(const vector2& pos, std::string name);

    convoy(convoy&&) = default;

    virtual ~convoy() = default;

    /// add ship to convoy. returns false if this is impossible (wrong type of
    /// ship)
    bool add_ship(game& gm, sea_object_id id);

    void load(const xml_elem& parent);
    void save(xml_elem& parent) const;

    [[nodiscard]] unsigned get_nr_of_ships() const;

    [[nodiscard]] vector2 get_pos() const { return position; }
    [[nodiscard]] std::string get_name() const { return name; }

    virtual class ai* get_ai() { return myai.get(); }

    virtual void simulate(double delta_time, game& gm);

    // used for AI and control of convoy. Add known enemy contact.
    virtual void add_contact(game& gm, const vector3& pos);
};
