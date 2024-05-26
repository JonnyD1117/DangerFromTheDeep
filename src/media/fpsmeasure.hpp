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

//
//  FPS measuring (C)+(W) 2008 Thorsten Jordan
//

#pragma once

class fpsmeasure
{
  public:
    fpsmeasure(float mi = 5.0f);
    float account_frame();
    [[nodiscard]] float get_total_fps() const;
    [[nodiscard]] unsigned get_slowest_frame_time_ms() const { return slowest_frame; }
    [[nodiscard]] unsigned get_fastest_frame_time_ms() const { return fastest_frame; }

  protected:
    const unsigned measure_interval;
    unsigned tm0{0}, tm_lastframe{0}, tm_lastmeasure{0}, nr_frames{0}, frames_lastmeasure{0};
    float curfps{0};
    unsigned slowest_frame{0}, fastest_frame{0};
};
