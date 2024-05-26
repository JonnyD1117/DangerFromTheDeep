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

#pragma once

#include "color.hpp"
#include "vector3.hpp"
#include "vertexbufferobject.hpp"

#include <vector>

/*
    FIXME:
     Add motion and accurate position calculation.
     Add credit for stars catalogue (http://astronexus.com/)

*/

///\brief Stars rendering.
class stars
{
  private:
    vertexbufferobject star_positions;
    mutable vertexbufferobject star_colors_VBO;
    std::vector<colorf> star_colors;
    unsigned int star_count_static{0}, star_count{0};

  public:
    stars(const float max_magnitude = 5.8f); // mag==6.0 => ~5000 stars

    void display(const float max_view_dist) const;
};
