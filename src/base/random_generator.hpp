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
//  A generic generator for random numbers (C)+(W) 2008 Thorsten Jordan
//

#pragma once

#include <algorithm>
#include <random>

class random_generator_deprecated
{
  public:
    random_generator_deprecated(unsigned seed = 0)
        : reg(seed)
    {
    }
    virtual ~random_generator_deprecated() = default;

    virtual unsigned rnd()
    {
        chaos();
        return reg;
    }

    virtual float rndf()
    {
        unsigned n = rnd();
        return float(double(n) / unsigned(-1));
    }

    virtual void set_seed(unsigned seed) { reg = seed; }

  protected:
    virtual void chaos() { reg = reg * 9699691 + 223092870; }

    unsigned reg;
};

/// A simple class to generate random numbers
class random_generator
{
  public:
    /// Construct a generator that delivers true random numbers
    random_generator()
        : generator(std::random_device()())
        , distribution(0.0, 1.0)
    {
    }

    /// Construct a generator with defined seed value
    random_generator(uint32_t seed)
        : generator(seed)
        , distribution(0.0, 1.0)
    {
    }

    /// Return the next pseudo random number in range [0...1]
    double get() { return distribution(generator); }

    /// Return unsigned value in range [0...n[
    /// @remark a tiny bit biased, because 1.0 will be mapped to limit-1, but
    /// that's ok for our use
    unsigned get(unsigned limit) { return std::min(limit - 1, unsigned(std::floor(limit * get()))); }

    /// Return random value in -v...+v range
    double variance(double v) { return (2.0 * get() - 1.0) * v; }

  protected:
    std::mt19937 generator;
    std::uniform_real_distribution<double> distribution;
};
