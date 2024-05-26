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

// this class holds the game's configuration
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#pragma once

#include "input_event_handler.hpp"
#include "keys.hpp"
#include "singleton.hpp"

#include <map>
#include <string>
#include <utility>

///\brief This class stores and manages the global game configuration.
///\todo replace tinyxml code and include with own xml class.
class cfg : public singleton<class cfg>
{
    friend class singleton<cfg>;

  public:
    ///\brief Documentation is build of keys. Each key has a name and an value.
    struct key
    {
        std::string action;
        key_code keycode{key_code::UNKNOWN};
        key_mod keymod{key_mod::none};
        key()  = default;
        ~key() = default;
        key(std::string ac, key_code kc, key_mod km)
            : action(std::move(ac))
            , keycode(kc)
            , keymod(km & key_mod::basic)
        {
        }
        [[nodiscard]] bool equal(key_code kc, key_mod km) const;
    };

  private:
    cfg(const cfg&)            = delete;
    cfg& operator=(const cfg&) = delete;

    std::map<std::string, bool> valb;
    std::map<std::string, int> vali;
    std::map<std::string, unsigned> valu;
    std::map<std::string, float> valf;
    std::map<std::string, std::string> vals;
    std::map<key_command, key> valk;

    static cfg* myinst;

    cfg();

    // set dependent on registered type, used by load() and parse(), returns
    // false if name is unknown
    bool set_str(const std::string& name, const std::string& value);

  public:
    ~cfg();

    // load the values from a config file. Note! register() calls must be happen
    // *before* loading the values!
    void load(const std::string& filename);
    void save(const std::string& filename) const;

    void register_option(const std::string& name, bool value);
    void register_option(const std::string& name, int value);
    void register_option(const std::string& name, unsigned value);
    void register_option(const std::string& name, float value);
    void register_option(const std::string& name, const std::string& value);
    void register_key(const std::string& name, key_code kc, key_mod km);

    void set(const std::string& name, bool value);
    void set(const std::string& name, int value);
    void set(const std::string& name, unsigned value);
    void set(const std::string& name, float value);
    void set(const std::string& name, const std::string& value);
    void set_key(key_command nr, key_code kc, key_mod km);

    [[nodiscard]] bool getb(const std::string& name) const;
    [[nodiscard]] int geti(const std::string& name) const;
    [[nodiscard]] int getu(const std::string& name) const;
    [[nodiscard]] float getf(const std::string& name) const;
    [[nodiscard]] std::string gets(const std::string& name) const;
    [[nodiscard]] key getkey(key_command nr) const;

    void parse_value(const std::string& s); // give elements of command line array to it!
};

bool is_configured_key(key_command kc, const input_event_handler::key_data& kd);
