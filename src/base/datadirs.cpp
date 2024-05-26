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

// global directory data
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#include "datadirs.hpp"

#include "error.hpp"
#include "filehelper.hpp"
#include "log.hpp"

static auto get_global_data_dir() -> std::string&
{
    static std::string global_datadir = DFTD_DATADIR;
    return global_datadir;
}

/// Get global data directory
auto get_data_dir() -> const std::string&
{
    return get_global_data_dir();
}

/// Set global data directory, Note! call this only once, and very early in
/// main()!
void set_data_dir(const std::string& datadir)
{
    get_global_data_dir() = datadir;
}

data_file_handler::data_file_handler()
{
    // scan data dir for all .data files
    std::string dir = "objects/";

    parse_for_data_files(dir + "airplanes/", airplane_ids);
    parse_for_data_files(dir + "ships/", ship_ids);
    parse_for_data_files(dir + "submarines/", submarine_ids);
    parse_for_data_files(dir + "torpedoes/", torpedo_ids);
    parse_for_data_files(dir + "props/", prop_ids);
}

static const std::string data_file_ext = ".data";

void data_file_handler::parse_for_data_files(const std::string& dir, std::vector<std::string>& idlist)
{
    directory d(get_data_dir() + dir);

    for (std::string f = d.read(); !f.empty(); f = d.read()) {
        if (f[0] == '.' || f == "CVS") {
            // avoid . and .. entries, as well as hidden files, and CVS
            // directories as well
            continue;
        }
        if (is_directory(get_data_dir() + dir + f)) {
            parse_for_data_files(dir + f + "/", idlist);
        } else if (
            f.length() > data_file_ext.length() && f.substr(f.length() - data_file_ext.length()) == data_file_ext) {
            std::string id = f.substr(0, f.length() - data_file_ext.length());
            // 			log_info("found file " << dir << " for id " << id);
            data_files[id] = dir;
            idlist.push_back(id);
        }
    }
}

auto data_file_handler::get_rel_path(const std::string& objectid) const -> const std::string&
{
    static std::string emptystr;
    auto it = data_files.find(objectid);

    if (it == data_files.end()) {
        THROW(error, std::string("can't find path for object '") + objectid + std::string("'"));
    }
    return it->second;
}

auto data_file_handler::get_path(const std::string& objectid) const -> std::string
{
    return get_data_dir() + get_rel_path(objectid);
}

auto data_file_handler::get_rel_filename(const std::string& objectid) const -> std::string
{
    return get_rel_path(objectid) + objectid + data_file_ext;
}

auto data_file_handler::get_filename(const std::string& objectid) const -> std::string
{
    return get_data_dir() + get_rel_filename(objectid);
}
