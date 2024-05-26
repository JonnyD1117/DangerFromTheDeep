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

// a highscore list
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#pragma once

#include <iostream>
#include <string>
#include <utility>
#include <vector>

///\brief Handles a list of high scores (hall of fame).
class highscorelist
{
    friend void check_for_highscore(const class game*);

  public:
    struct entry
    {
        unsigned points{0};
        std::string name;

        // missing: maybe start & end date, realism factor, rank/merits,
        // submarine number
        entry()
            : name("--------")
        {
        }
        entry(unsigned p, std::string n)
            : points(p)
            , name(std::move(n))
        {
        }
        entry(std::istream& in);
        ~entry()                         = default;
        entry(const entry& e)            = default;
        entry& operator=(const entry& e) = default;
        void save(std::ostream& out) const;

        [[nodiscard]] bool is_worse_than(unsigned pts) const; // is entry worse than given value?
    };

  protected:
    std::vector<entry> entries;

  public:
    highscorelist(unsigned maxentries = 10);
    ~highscorelist() = default;
    highscorelist(const std::string& filename); // read from file

    void save(const std::string& filename) const;

    [[nodiscard]] unsigned
    get_listpos_for(unsigned points) const; // returns place in list or entries.size() if not in list

    [[nodiscard]] bool is_good_enough(unsigned points) const; // check if score is good enough for an entry

    void record(unsigned points,
                const std::string& name); // record entry if it is good enough

    void show(class widget* parent) const;
};
