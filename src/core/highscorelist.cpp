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

#include "highscorelist.hpp"

#include "binstream.hpp"
#include "texts.hpp"
#include "widget.hpp"

#include <fstream>
#include <sstream>
using std::ifstream;
using std::ios;
using std::istream;
using std::ofstream;
using std::ostream;
using std::ostringstream;
using std::string;

highscorelist::entry::entry(istream& in)
{
    points = read_u32(in);
    name   = read_string(in);
}

void highscorelist::entry::save(ostream& out) const
{
    write_u32(out, points);
    write_string(out, name);
}

auto highscorelist::entry::is_worse_than(unsigned pts) const -> bool
{
    return points < pts;
}

highscorelist::highscorelist(unsigned maxentries)
{
    entries.resize(maxentries);
}

highscorelist::highscorelist(const string& filename)
{
    ifstream in(filename.c_str(), ios::in | ios::binary);
    unsigned maxentries = read_u8(in);
    entries.resize(maxentries);

    for (unsigned i = 0; i < maxentries; ++i) {
        entries[i] = entry(in);
    }
}

void highscorelist::save(const string& filename) const
{
    ofstream out(filename.c_str(), ios::out | ios::binary);
    auto n = uint8_t(entries.size());
    write_u8(out, n);

    for (unsigned i = 0; i < n; ++i) {
        entries[i].save(out);
    }
}

auto highscorelist::get_listpos_for(unsigned points) const -> unsigned
{
    unsigned i = 0;

    for (; i < entries.size(); ++i) {
        if (entries[i].is_worse_than(points)) {
            break;
        }
    }
    return i;
}

auto highscorelist::is_good_enough(unsigned points) const -> bool
{
    return get_listpos_for(points) < entries.size();
}

void highscorelist::record(unsigned points, const string& name)
{
    unsigned pos = get_listpos_for(points);
    if (pos < entries.size()) {
        // move rest down one step
        for (auto j = unsigned(entries.size()); j > pos + 1; --j) {
            entries[j - 1] = entries[j - 2];
        }
        entries[pos] = entry(points, name);
    }
}

// fixme if we could separate this method we would separate data from view,
// better for include!
void highscorelist::show(widget* parent) const
{
    const font* fnt = widget::get_theme()->myfont;
    unsigned lh     = fnt->get_height();
    unsigned scw    = fnt->get_size("0000000").x;
    unsigned y      = 2 * lh;
    parent->add_child<widget_text>(scw / 2, y, 0, 0, texts::get(202));
    parent->add_child<widget_text>(2 * scw, y, 0, 0, texts::get(203));
    y += 2 * lh;
    for (const auto& elem : entries) {
        ostringstream osp;
        osp << elem.points;
        parent->add_child<widget_text>(scw / 2, y, 0, 0, osp.str());
        parent->add_child<widget_text>(2 * scw, y, 0, 0, elem.name);
        y += lh * 3 / 2;
    }
}
