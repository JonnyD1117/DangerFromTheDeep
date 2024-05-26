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

// Base interface for user screen popups.
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#pragma once

#include "image.hpp"
#include "input_event_handler.hpp"
class user_interface;

class user_popup : public input_event_handler
{
  private:
    // no empty construction, no copy
    user_popup()                             = delete;
    user_popup(user_popup&)                  = delete;
    user_popup& operator=(const user_popup&) = delete;

  protected:
    /// the display needs to know its parent (user_interface) to access common
    /// data
    user_interface& ui;

    /// Constructor
    user_popup(user_interface& ui_, const char* popup_name = nullptr);

  public:
    /// Destructor. needed for correct destruction of heirs.
    ~user_popup() override = default;
    /// Display method - very basic. Just draw display (elements)
    virtual void display() const;

  protected:
    /// A 2D image element
    class elem2D
    {
      public:
        /// Construct static element
        elem2D(vector2i pos, const std::string& filename_day, const std::string& filename_night = std::string());
        /// Draw element normally/static
        void draw(bool is_day) const;

      protected:
        std::unique_ptr<image> tex_day;   ///< Texture data
        std::unique_ptr<image> tex_night; ///< Texture data (optional)
        vector2i position;                ///< Position on the screen
    };

    std::vector<elem2D> elements; ///< Elements for display
};
