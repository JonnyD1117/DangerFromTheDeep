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

// A generic store for named objects with reference counter.
// (C)+(W) by Thorsten Jordan. See LICENSE

#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

// fixme template specialization to encode data directories?
// so we can use that for images and everything else?
// Just loads and keeps data
// We can implement a cleanup function then to manually remove data when needed.
// That way widgets can also use shared_ptrs to data.

/// Manages shared_storage of named objects
template<typename C, typename Key = std::string>
class shared_object_store
{
  public:
    shared_object_store(std::string base_dir)
        : base_directory(std::move(base_dir))
    {
    }

    template<typename... Types>
    std::shared_ptr<C> create(const Key& name, Types&&... args)
    {
        if (name.empty()) {
            throw std::invalid_argument("shared_object_store::create without name");
        }
        const auto& [it, inserted] = storage.insert({name, {}});
        if (inserted) {
            it->second = std::make_shared<C>(std::forward<Types>(args)...);
        }
        return it->second;
    }

    std::shared_ptr<C> ref(const Key& name)
    {
        // Valid shortcut, if there is no name, just return nullptr. Makes calling code easier
        if (name.empty()) {
            return {};
        }
        const auto& [it, inserted] = storage.insert({name, nullptr});
        if (inserted) {
            it->second = std::make_shared<C>(base_directory + name);
        }
        return it->second;
    }

    void cleanup()
    {
        for (auto it = storage.begin(); it != storage.end();) {
            if (it->second.use_count() == 1) {
                // only storage holds it
                it = storage.erase(it);
            } else {
                ++it;
            }
        }
    }

  private:
    /// Base directory for files
    std::string base_directory;
    /// Data storage
    std::unordered_map<Key, std::shared_ptr<C>> storage;
};

/// Manages storage of named objects
template<typename C, typename Key = std::string>
class object_store
{
  public:
    object_store(std::string base_dir)
        : base_directory(std::move(base_dir))
    {
    }

    template<typename... Types>
    C* create(const Key& name, Types&&... args)
    {
        if (name.empty()) {
            throw std::invalid_argument("object_store::create without name");
        }
        return &storage.try_emplace(name, std::forward<Types>(args)...).first->second;
    }

    C* ref(const Key& name)
    {
        // Valid shortcut, if there is no name, just return nullptr. Makes calling code easier
        if (name.empty()) {
            return {};
        }
        return &storage.try_emplace(name, base_directory + name).first->second;
    }

    C* find(const Key& name)
    {
        if (name.empty()) {
            throw std::invalid_argument("object_store::find without name");
        }
        const auto it = storage.find(name);
        if (it == storage.end()) {
            return {};
        }
        return &it->second;
    }

  private:
    /// Base directory for files
    std::string base_directory;
    /// Data storage
    std::unordered_map<Key, C> storage;
};
