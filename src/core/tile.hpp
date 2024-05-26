/*
 * Danger from the Deep - Open source submarine simulation
 * Copyright (C) 2003-2020  Thorsten Jordan, Luis Barrancos and others.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#pragma once

#include "bitstream.hpp"
#include "bzip.hpp"
#include "log.hpp"
#include "morton_bivector.hpp"
#include "system_interface.hpp"
#include "vector2.hpp"

#include <ctime>
#include <fstream>
#include <string>

template<class T>
class tile
{
  public:
    tile(const char* filename, vector2i& _bottom_left, unsigned size);
    tile(const tile<T>&);
    tile()
        : data(1){};

    void load(const char* filename, vector2i& _bottom_left, unsigned size);
    T get_value(vector2i coord);

    /* simple getters */
    [[nodiscard]] unsigned long get_last_access() const { return last_access; };
    [[nodiscard]] vector2i get_bottom_left() const { return bottom_left; };
    [[nodiscard]] const morton_bivector<T>& get_data() const { return data; };

  protected:
    morton_bivector<T> data;
    vector2i bottom_left;
    unsigned long last_access;
};

template<class T>
tile<T>::tile(const char* filename, vector2i& _bottom_left, unsigned size)
    : data(size, -200)
    , bottom_left(_bottom_left)
    , last_access(SYS().millisec())
{
    std::ifstream file;
    file.open(filename);

    if (file.is_open()) {
        bzip_istream bin(&file);
        bin.read((char*) data.data_ptr(), size * size * sizeof(T));
        bin.close();
        file.close();
    } else {
        std::stringstream msg;
        msg << "Cannot open file: ";
        msg << filename;
        log_warning(msg.str());
    }
}

template<class T>
void tile<T>::load(const char* filename, vector2i& _bottom_left, unsigned size)
{
    data.resize(size, -200);
    bottom_left = _bottom_left;
    last_access = SYS().millisec();

    std::ifstream file;
    file.open(filename);

    if (file.is_open()) {
        bzip_istream bin(&file);
        bin.read((char*) data.data_ptr(), size * size * sizeof(T));
        bin.close();
        file.close();
    } else {
        std::stringstream msg;
        msg << "Cannot open file: ";
        msg << filename;
        log_warning(msg.str());
    }
}

template<class T>
tile<T>::tile(const tile<T>& other)
    : data(other.get_data())
    , bottom_left(other.get_bottom_left())
    , last_access(other.get_last_access())
{
}

template<class T>
T tile<T>::get_value(vector2i coord)
{
    last_access = SYS().millisec();
    coord.y     = data.size() - coord.y - 1;
    return data.at(coord);
}
