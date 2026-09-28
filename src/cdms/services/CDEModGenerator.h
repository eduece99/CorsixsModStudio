/*
Rainman Library
Copyright (C) 2006 Corsix <corsix@gmail.com>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef _C_DE_MOD_GENERATOR_H_
#define _C_DE_MOD_GENERATOR_H_

#include <filesystem>
#include <string>
#include <string_view>

class CDEModGenerator
{
  public:
    struct Options
    {
        std::string identifier;
        std::string displayName;
        std::string description;
        std::string parent = "DoWDE";
    };

    // The caller supplies the mods root, never the game-install directory.
    // Throws std::runtime_error on invalid input, collisions or I/O failure.
    [[nodiscard]] static std::filesystem::path Create(const std::filesystem::path &modsRoot, const Options &options);
    [[nodiscard]] static bool IsValidParent(std::string_view parent) noexcept;
};

#endif
