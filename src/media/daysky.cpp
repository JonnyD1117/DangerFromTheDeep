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

#include "daysky.hpp"

#include "constant.hpp"
#include "helper.hpp"
#include "vector3.hpp"

#include <cmath>

// Distribution coefficients for the luminance(Y) distribution function
static float YDC[5][2] =
    {{0.1787F, -1.4630F}, {-0.3554F, 0.4275F}, {-0.0227F, 5.3251F}, {0.1206F, -2.5771F}, {-0.0670F, 0.3703F}};

// Distribution coefficients for the x distribution function
static float xDC[5][2] =
    {{-0.0193F, -0.2592F}, {-0.0665F, 0.0008F}, {-0.0004F, 0.2125F}, {-0.0641F, -0.8989F}, {-0.0033F, 0.0452F}};

// Distribution coefficients for the y distribution function
static float yDC[5][2] =
    {{-0.0167F, -0.2608F}, {-0.0950F, 0.0092F}, {-0.0079F, 0.2102F}, {-0.0441F, -1.6537F}, {-0.0109F, 0.0529F}};

// Zenith x value
static float xZC[3][4] = {
    {0.00166F, -0.00375F, 0.00209F, 0.0F},
    {-0.02903F, 0.06377F, -0.03203F, 0.00394F},
    {0.11693F, -0.21196F, 0.06052F, 0.25886F}};
// Zenith y value
static float yZC[3][4] = {
    {0.00275F, -0.00610F, 0.00317F, 0.0F},
    {-0.04214F, 0.08970F, -0.04153F, 0.00516F},
    {0.15346F, -0.26756F, 0.06670F, 0.26688F}};

// Angle between (thetav, theta) and  (phiv,phi)
inline auto angle_between(float thetav, float phiv, float theta, float phi) -> float
{
    double cospsi = std::sin(thetav) * std::sin(theta) * std::cos(phi - phiv) + std::cos(thetav) * std::cos(theta);
    return float(acos(std::clamp(cospsi, -1.0, 1.0)));
}

inline auto perez_function(float A, float B, float C, float D, float E, float Theta, float Gamma) -> float
{
    float cosGamma = std::cos(Gamma);
    float d        = (1 + A * std::exp(B / std::cos(Theta))) * (1 + C * std::exp(D * Gamma) + E * cosGamma * cosGamma);
    return d;
}

// Constructor
daysky::daysky()
{
    set_turbidity(2.0F);
    set_sun_position(0.0F, 0.0F);
}

daysky::daysky(float azimuth, float elevation, float turbidity)
{
    m_T  = turbidity;
    m_T2 = m_T * m_T;

    set_sun_position(azimuth, elevation);
    recalculate_alphabet();
}

// Set turbidity and precalc power of two
void daysky::set_turbidity(float pT)
{
    m_T  = pT;
    m_T2 = m_T * m_T;
    recalculate_chroma();
    recalculate_alphabet();
}

// Set sun position
void daysky::set_sun_position(float azimuth, float elevation)
{
    // maybe give sun direction as vector3 here.
    m_sun_theta = azimuth;

    m_sun_phi = float(constant::PI_2 - elevation);
    recalculate_chroma();
}

// Calculate color
auto daysky::get_color(float theta, float phi) const -> colorf
{
    phi = float(constant::PI_2 - phi);

    // Angle between sun (zenith=0.0!!) and point(phi,theta) to get compute
    // color for
    float gamma = angle_between(phi, theta, m_sun_phi, m_sun_theta);

    vector3f skycolor_xyY;

    // Zenith luminance
    auto chi      = (4.0 / 9.0 - m_T / 120.0) * (constant::PI - 2 * m_sun_phi);
    auto zenith_Y = (4.0453 * m_T - 4.9710) * tan(chi) - 0.2155 * m_T + 2.4192;
    if (zenith_Y < 0.0) {
        zenith_Y = -zenith_Y;
    }

    //  A = YDC[0][0]*m_T + YDC[0][1];
    //  B = YDC[1][0]*m_T + YDC[1][1];
    //  C = YDC[2][0]*m_T + YDC[2][1];
    //  D = YDC[3][0]*m_T + YDC[3][1];
    //  E = YDC[4][0]*m_T + YDC[4][1];

    // Sky luminance
    auto d         = distribution(m_luminance, phi, gamma);
    skycolor_xyY.z = float(zenith_Y * d);

    // Zenith x
    // Zenith.x = chromaticity( xZC );
    //  A = xDC[0][0]*m_T + xDC[0][1];
    //  B = xDC[1][0]*m_T + xDC[1][1];
    //  C = xDC[2][0]*m_T + xDC[2][1];
    //  D = xDC[3][0]*m_T + xDC[3][1];
    //  E = xDC[4][0]*m_T + xDC[4][1];

    // Sky x
    d              = distribution(m_x, phi, gamma);
    skycolor_xyY.x = /*Zenith.x*/ m_chroma_xZC * d;

    // Zenith y
    // Zenith.y = chromaticity( yZC );
    //  A = yDC[0][0]*m_T + yDC[0][1];
    //  B = yDC[1][0]*m_T + yDC[1][1];
    //  C = yDC[2][0]*m_T + yDC[2][1];
    //  D = yDC[3][0]*m_T + yDC[3][1];
    //  E = yDC[4][0]*m_T + yDC[4][1];

    // Sky y
    d              = distribution(m_y, phi, gamma);
    skycolor_xyY.y = /*Zenith.y*/ m_chroma_yZC * d;

    // fixme: compute alpha
    // fixme: Stellarium computes luminance value for display with a more
    // complex model that takes moon and sun pos into account as well as viewer
    // height etc.
    float colors[3] = {skycolor_xyY.x, skycolor_xyY.y, skycolor_xyY.z};
    // printf("colors xyY: %f %f %f\n", colors[0], colors[1], colors[2]);
    tonerepro.xyY_to_RGB(colors);
    // printf("colors RGB: %f %f %f\n", colors[0], colors[1], colors[2]);

    // intensity rescaling for turbidity 2.0

    float scalefac;
    auto elevation = float(constant::PI_2 - m_sun_phi);
    if (elevation >= 0) {
        if (elevation >= constant::PI * 0.5 - 0.35) { // - 0.3 with safety margin
            scalefac = 100;
        } else {
            // when evelation + 0.3 is larger than PI/2, cos of that value
            // goes below zero, leading to a NaN result of the pow.
            scalefac = float(100 - pow(7 * pow(cos(elevation + 0.3), 1.8), 2));
        }
    } else {
        scalefac = 50;
    }

    // the scalefac scales RGB values over 1.0, up to roughly sqrt(2).
    // (1.445...) we clamp the values here to avoid visible errors. we encode
    // brightness for star rendering (fixme todo) as alpha, total brightness
    // below level as star factor, i.e. minimum of rgb? but this can be done in
    // star shader as well!
    return {
        std::min(1.F, colors[0] * scalefac), std::min(1.F, colors[1] * scalefac), std::min(1.F, colors[2] * scalefac)};
}

auto daysky::distribution(const alphabet& ABCDE, float Theta, float Gamma) const -> float
{
    //                       Perez_f0(Theta,Gamma)
    //    calculates:   d = -----------------------
    //                       Perez_f1(0,ThetaSun)
    float f0 = perez_function(ABCDE.A, ABCDE.B, ABCDE.C, ABCDE.D, ABCDE.E, Theta, Gamma);
    float f1 = perez_function(ABCDE.A, ABCDE.B, ABCDE.C, ABCDE.D, ABCDE.E, 0, m_sun_phi);
    return (f0 / f1);
}

auto daysky::chromaticity(const float ZC[3][4], float sun_phi2, float sun_phi3) const -> float
{
    float c = (ZC[0][0] * sun_phi3 + ZC[0][1] * sun_phi2 + ZC[0][2] * m_sun_phi + ZC[0][3]) * m_T2
              + (ZC[1][0] * sun_phi3 + ZC[1][1] * sun_phi2 + ZC[1][2] * m_sun_phi + ZC[1][3]) * m_T
              + (ZC[2][0] * sun_phi3 + ZC[2][1] * sun_phi2 + ZC[2][2] * m_sun_phi + ZC[2][3]);
    return c;
}

void daysky::recalculate_chroma()
{
    auto sun_phi2 = m_sun_phi * m_sun_phi;
    auto sun_phi3 = m_sun_phi * sun_phi2;
    m_chroma_xZC  = chromaticity(xZC, sun_phi2, sun_phi3);
    m_chroma_yZC  = chromaticity(yZC, sun_phi2, sun_phi3);
}

void daysky::recalculate_alphabet()
{
    m_luminance.A = YDC[0][0] * m_T + YDC[0][1];
    m_luminance.B = YDC[1][0] * m_T + YDC[1][1];
    m_luminance.C = YDC[2][0] * m_T + YDC[2][1];
    m_luminance.D = YDC[3][0] * m_T + YDC[3][1];
    m_luminance.E = YDC[4][0] * m_T + YDC[4][1];

    m_x.A = xDC[0][0] * m_T + xDC[0][1];
    m_x.B = xDC[1][0] * m_T + xDC[1][1];
    m_x.C = xDC[2][0] * m_T + xDC[2][1];
    m_x.D = xDC[3][0] * m_T + xDC[3][1];
    m_x.E = xDC[4][0] * m_T + xDC[4][1];

    m_y.A = yDC[0][0] * m_T + yDC[0][1];
    m_y.B = yDC[1][0] * m_T + yDC[1][1];
    m_y.C = yDC[2][0] * m_T + yDC[2][1];
    m_y.D = yDC[3][0] * m_T + yDC[3][1];
    m_y.E = yDC[4][0] * m_T + yDC[4][1];
}
