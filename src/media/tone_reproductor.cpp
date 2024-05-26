/*
 * Copyright (C) 2003 Fabien Chereau
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "tone_reproductor.hpp"

#include "constant.hpp"

#include <cstdio>

namespace
{
auto mypow10(double x)
{
    return pow(10.0, x);
}
} // namespace

// Set some values to prevent bugs in case of bad use
tone_reproductor::tone_reproductor()
{
    // Update alpha_da and beta_da values
    float log10Lwa = log10f(Lwa);
    alpha_wa       = 0.4F * log10Lwa + 1.519F;
    beta_wa        = -0.4F * log10Lwa * log10Lwa + 0.218F * log10Lwa + 6.1642F;

    set_display_adaptation_luminance(Lda);
    set_world_adaptation_luminance(Lwa);
}

tone_reproductor::~tone_reproductor() = default;

// Set the eye adaptation luminance for the display and precompute what can be
// Usual luminance range is 1-100 cd/m^2 for a CRT screen
void tone_reproductor::set_display_adaptation_luminance(float Lda)
{
    Lda = Lda;

    // Update alpha_da and beta_da values
    float log10Lda = log10f(Lda);
    alpha_da       = 0.4F * log10Lda + 1.519F;
    beta_da        = -0.4F * log10Lda * log10Lda + 0.218F * log10Lda + 6.1642F;

    // Update terms
    alpha_wa_over_alpha_da = alpha_wa / alpha_da;
    term2                  = float(mypow10((beta_wa - beta_da) / alpha_da) / (constant::PI * 0.0001F));
}

// Set the eye adaptation luminance for the world and precompute what can be
void tone_reproductor::set_world_adaptation_luminance(float Lwa)
{
    Lwa = Lwa;

    // Update alpha_da and beta_da values
    float log10Lwa = log10f(Lwa);
    alpha_wa       = 0.4F * log10Lwa + 1.519F;
    beta_wa        = -0.4F * log10Lwa * log10Lwa + 0.218F * log10Lwa + 6.1642F;

    // Update terms
    alpha_wa_over_alpha_da = alpha_wa / alpha_da;
    term2                  = float(mypow10((beta_wa - beta_da) / alpha_da) / (constant::PI * 0.0001F));
}

// Convert from xyY color system to RGB according to the adaptation
// The Y component is in cd/m^2
void tone_reproductor::xyY_to_RGB(float* color) const
{
    // 1. Hue conversion
    float log10Y = log10f(color[2]);
    // if log10Y>0.6, photopic vision only (with the cones, colors are seen)
    // else scotopic vision if log10Y<-2 (with the rods, no colors, everything
    // blue), else mesopic vision (with rods and cones, transition state)
    if (log10Y < 0.6) {
        // Compute s, ratio between scotopic and photopic vision
        float s = 0.F;
        if (log10Y > -2.F) {
            float op = (log10Y + 2.F) / 2.6F;
            s        = 3.F * op * op - 2 * op * op * op;
        }

        // Do the blue shift for scotopic vision simulation (night vision) [3]
        // The "night blue" is x,y(0.25, 0.25)
        color[0] = (1.F - s) * 0.25F + s * color[0]; // Add scotopic + photopic components
        color[1] = (1.F - s) * 0.25F + s * color[1]; // Add scotopic + photopic components

        // Take into account the scotopic luminance approximated by V [3] [4]
        float V  = color[2] * (1.33F * (1.F + color[1] / color[0] + color[0] * (1.F - color[0] - color[1])) - 1.68F);
        color[2] = 0.4468F * (1.F - s) * V + s * color[2];
    }

    // 2. Adapt the luminance value and scale it to fit in the RGB range [2]
    color[2] = powf(adapt_luminance(color[2]) / MaxdL, 1.F / gamma);

    // Convert from xyY to XZY
    float X = color[0] * color[2] / color[1];
    float Y = color[2];
    float Z = (1.F - color[0] - color[1]) * color[2] / color[1];

    // Use a XYZ to Adobe RGB (1998) matrix which uses a D65 reference white
    // Oh no you don't, use rec.709 HDTV instead (D65 white point as well)
    color[0] = 3.240479F * X - 1.537150F * Y - 0.498535F * Z;
    color[1] = -0.969256F * X + 1.875992F * Y + 0.041556F * Z;
    color[2] = 0.055648F * X - 0.204043F * Y + 1.057311F * Z;
}
