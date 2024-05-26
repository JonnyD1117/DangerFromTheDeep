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

// this class holds the game's configuration
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#pragma once

#include "error.hpp"

#include <memory>

/// Implementation of the singleton pattern
template<typename D>
class singleton
{
  private:
    static auto& instance_ptr()
    {
        static std::unique_ptr<D> myinstanceptr;
        return myinstanceptr;
    }

  public:
    /// get the one and only instance
    /// @remarks since D is constructed not before first call, it avoids the
    /// static initialization order fiasco.
    static D& instance()
    {
        auto& p = instance_ptr();
        if (p == nullptr) {
            std::unique_ptr<D> new_p(new D{}); // make_unique not possible when c'tor is private
            p = std::move(new_p);
            std::atexit(destroy_instance);
        }
        return *p;
    }
    /// Create the first instance with given object
    template<typename... Types>
    static void create_instance(Types&&... args)
    {
        auto& p = instance_ptr();
        if (p != nullptr) {
            THROW(error, "tried to recreate existing singleton");
        }
        p = std::make_unique<D>(std::forward<Types>(args)...);
        std::atexit(destroy_instance);
    }
    /// Release the instance for possible custom deletion
    static D* release_instance()
    {
        auto& p = instance_ptr();
        return p.release();
    }

  protected:
    singleton() = default;
    static void destroy_instance()
    {
        auto& p = instance_ptr();
        p       = nullptr;
    }

  private:
    singleton(const singleton&)            = delete;
    singleton(singleton&&)                 = delete;
    singleton& operator=(const singleton&) = delete;
    singleton& operator=(singleton&&)      = delete;
};
