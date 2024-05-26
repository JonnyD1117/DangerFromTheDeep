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

#include "cfg.hpp"

#include "keys.hpp"
#include "log.hpp"
#include "xml.hpp"

#include <sstream>
using namespace std;

cfg* cfg::myinst = nullptr;

cfg::cfg() = default;

cfg::~cfg() = default;

auto cfg::set_str(const string& name, const string& value) -> bool
{
    auto it = valb.find(name);

    if (it != valb.end()) {
        if (value == "true" || value == "yes") {
            it->second = true;
        } else if (value == "false" || value == "no") {
            it->second = false;
        } else {
            it->second = bool(atoi(value.c_str()));
        }
    } else {
        auto ia = vali.find(name);
        if (ia != vali.end()) {
            ia->second = atoi(value.c_str());
        } else {
            auto ib = valf.find(name);
            if (ib != valf.end()) {
                ib->second = atof(value.c_str());
            } else {
                auto ic = vals.find(name);
                if (ic != vals.end()) {
                    ic->second = value;
                } else {
                    return false;
                }
            }
        }
    }
    return true;
}

void cfg::load(const string& filename)
{
    xml_doc doc(filename);
    doc.load();

    xml_elem root = doc.child("dftd-cfg");

    for (auto elem : root) {
        if (elem.get_name() == "keys") {
            for (auto keyelem : elem.iterate("key")) {
                std::string keyname = keyelem.attr("action");

                // get key number for this action from table
                auto nr = key_command::number;

                for (unsigned i = 0; i < unsigned(key_command::number); ++i) {
                    if (std::string(key_names[i].name) == keyname) {
                        nr = key_command(i);
                    }
                }

                if (nr == key_command::number) {
                    log_warning("found key with invalid name " << keyname << " in config file");
                    continue;
                }

                auto keycode = key_code(keyelem.attri("keycode"));
                bool ctrl    = keyelem.attrb("ctrl");
                bool alt     = keyelem.attrb("alt");
                bool shift   = keyelem.attrb("shift");

                // fixme build modifier here
                key_mod mod{key_mod::none};

                if (ctrl) {
                    mod = mod | key_mod::ctrl;
                }

                if (alt) {
                    mod = mod | key_mod::alt;
                }

                if (shift) {
                    mod = mod | key_mod::shift;
                }

                set_key(nr, keycode, mod);
            }
        } else {
            bool found = set_str(elem.get_name(), elem.attr());

            if (!found) {
                log_warning("config option not registered: " << elem.get_name());
            }
        }
    }
}

void cfg::save(const string& filename) const
{
    xml_doc doc(filename);
    xml_elem root = doc.add_child("dftd-cfg");

    for (const auto& it : valb) {
        root.add_child(it.first).set_attr(it.second);
    }

    for (const auto& it : vali) {
        root.add_child(it.first).set_attr(it.second);
    }

    for (const auto& it : valf) {
        root.add_child(it.first).set_attr(it.second);
    }

    for (const auto& val : vals) {
        root.add_child(val.first).set_attr(val.second);
    }

    xml_elem keys = root.add_child("keys");

    for (const auto& it : valk) {
        xml_elem key = keys.add_child("key");

        key.set_attr(it.second.action, "action");
        key.set_attr(int(it.second.keycode), "keycode");

        key.set_attr((it.second.keymod & key_mod::ctrl) != key_mod::none, "ctrl");

        key.set_attr((it.second.keymod & key_mod::alt) != key_mod::none, "alt");

        key.set_attr((it.second.keymod & key_mod::shift) != key_mod::none, "shift");
    }
    doc.save();
}

void cfg::register_option(const string& name, bool value)
{
    valb[name] = value;
}

void cfg::register_option(const string& name, int value)
{
    vali[name] = value;
}

void cfg::register_option(const string& name, float value)
{
    valf[name] = value;
}

void cfg::register_option(const string& name, const string& value)
{
    vals[name] = value;
}

void cfg::register_key(const std::string& name, key_code kc, key_mod km)
{
    auto nr = key_command::number;

    for (unsigned i = 0; i < unsigned(key_command::number); ++i) {
        if (std::string(key_names[i].name) == name) {
            nr = key_command(i);
        }
    }

    if (nr == key_command::number) {
        THROW(error, std::string("register_key with invalid name ") + name);
    }
    valk[nr] = key(name, kc, km);
}

void cfg::set(const string& name, bool value)
{
    auto it = valb.find(name);
    if (it != valb.end()) {
        it->second = value;
    } else {
        THROW(error, string("cfg: set(), name not registered: ") + name);
    }
}

void cfg::set(const string& name, int value)
{
    auto it = vali.find(name);
    if (it != vali.end()) {
        it->second = value;
    } else {
        THROW(error, string("cfg: set(), name not registered: ") + name);
    }
}

void cfg::set(const string& name, float value)
{
    auto it = valf.find(name);
    if (it != valf.end()) {
        it->second = value;
    } else {
        THROW(error, string("cfg: set(), name not registered: ") + name);
    }
}

void cfg::set(const string& name, const string& value)
{
    auto it = vals.find(name);
    if (it != vals.end()) {
        it->second = value;
    } else {
        THROW(error, string("cfg: set(), name not registered: ") + name);
    }
}

void cfg::set_key(key_command nr, key_code kc, key_mod km)
{
    auto it = valk.find(nr);
    if (it != valk.end()) {
        it->second = key(it->second.action, kc, km);
    } else {
        THROW(error, string("cfg: set_key(), key number not registered: "));
    }
}

auto cfg::getb(const string& name) const -> bool
{
    auto it = valb.find(name);
    if (it != valb.end()) {
        return it->second;
    }
    THROW(error, string("cfg: get(), name not registered: ") + name);

    return false;
}

auto cfg::geti(const string& name) const -> int
{
    auto it = vali.find(name);
    if (it != vali.end()) {
        return it->second;
    }
    THROW(error, string("cfg: get(), name not registered: ") + name);

    return 0;
}

auto cfg::getf(const string& name) const -> float
{
    auto it = valf.find(name);
    if (it != valf.end()) {
        return it->second;
    }
    THROW(error, string("cfg: get(), name not registered: ") + name);

    return 0;
}

auto cfg::gets(const string& name) const -> string
{
    auto it = vals.find(name);
    if (it != vals.end()) {
        return it->second;
    }
    THROW(error, string("cfg: get(), name not registered: ") + name);

    return nullptr;
}

auto cfg::getkey(key_command nr) const -> cfg::key
{
    auto it = valk.find(nr);
    if (it != valk.end()) {
        return it->second;
    }
    THROW(error, string("cfg: getkey(), key number not registered: "));

    return {};
}

// format of s must be --name=value or --name or --noname for bool values.
void cfg::parse_value(const string& s)
{
    // fixme: ignore values for unregistered names?

    if (s.length() < 3 || s[0] != '-' || s[1] != '-') {
        return; // ignore it
    }

    string::size_type st = s.find('=');
    string s0;
    string s1;

    if (st == string::npos) {
        if (s.substr(2, 2) == "no") {
            s0 = s.substr(4);
            s1 = "false";
        } else {
            s0 = s.substr(2);
            s1 = "true";
        }
    } else {
        s0 = s.substr(2, st - 2);
        s1 = s.substr(st + 1);
    }
    set_str(s0, s1); // ignore value if name is unkown
}

auto is_configured_key(key_command kc, const input_event_handler::key_data& kd) -> bool
{
    const auto& configured_key = cfg::instance().getkey(kc);
    return configured_key.keycode == kd.keycode && configured_key.keymod == (kd.mod & key_mod::basic);
}
