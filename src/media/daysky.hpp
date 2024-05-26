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
#include "tone_reproductor.hpp"

/// Create the colormap of sky colors
class daysky
{
  public:
    /// Constructor. Create with default turbidity and default sun position
    daysky();
    /// Constructor with sun position and turbidity
    daysky(float azimuth, float elevation, float turbidity);
    //! Set turbidity.
    void set_turbidity(float pT);
    //! Get turbidity.
    float get_turbidity() const { return m_T; }
    //! Set sun position.
    void set_sun_position(float azimuth, float elevation);
    //! Get color. theta is angle in XY plane, phi is elevation angle.
    colorf get_color(float theta, float phi) const;

  private:
    struct alphabet
    {
        float A, B, C, D, E;
    };

    //! Calculate distribution
    inline float distribution(const alphabet& ABCDE, float Theta, float Gamma) const;
    //! Calc the actual color/chromaticity
    float chromaticity(const float ZC[3][4], float sun_phi2, float sun_phi3) const;

    inline void recalculate_chroma();
    inline void recalculate_alphabet();

    float m_T, m_T2; // Turbidity T and T^2
    float m_sun_theta;
    float m_sun_phi;
    float m_chroma_xZC, m_chroma_yZC;
    alphabet m_luminance, m_x, m_y;

    mutable tone_reproductor tonerepro;
};
