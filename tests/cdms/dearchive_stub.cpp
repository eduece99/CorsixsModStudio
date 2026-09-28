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

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string_view>

int main(int iCount, char *sArguments[])
{
    if (iCount != 7 || std::strcmp(sArguments[1], "-a") || std::strcmp(sArguments[3], "-c") ||
        std::strcmp(sArguments[5], "-r") || !std::filesystem::exists(sArguments[4]) ||
        !std::filesystem::is_directory(sArguments[6]))
    {
        return 8;
    }
    std::ofstream oArguments(std::string(sArguments[4]) + ".args", std::ios::binary);
    for (int i = 0; i < iCount; ++i)
    {
        oArguments << sArguments[i] << '\n';
    }
    if (!oArguments.good())
    {
        return 9;
    }
    oArguments.close();
    const std::string_view sArchive(sArguments[2]);
    if (sArchive.find("failure.sga") != std::string_view::npos)
    {
        return 17;
    }
    if (sArchive.find("missing.sga") != std::string_view::npos)
    {
        return 0;
    }
    std::ofstream oFile(sArguments[2], std::ios::binary);
    oFile << "test archive";
    return oFile.good() ? 0 : 9;
}
