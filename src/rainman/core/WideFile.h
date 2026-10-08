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

#pragma once

#include <cerrno>
#include <cstdio>
#include <cwchar>

#ifdef _WIN32
inline FILE *RainmanFOpen(const wchar_t *sPath, const wchar_t *sMode)
{
    return _wfopen(sPath, sMode);
}
#else
#include <codecvt>
#include <locale>
#include <stdexcept>
#include <string>

inline std::string RainmanWideToUtf8(const wchar_t *sText)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> oConverter;
    return oConverter.to_bytes(sText);
}

inline std::wstring RainmanUtf8ToWide(const char *sText)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> oConverter;
    return oConverter.from_bytes(sText);
}

inline FILE *RainmanFOpen(const wchar_t *sPath, const wchar_t *sMode)
{
    if (sPath == nullptr || sMode == nullptr)
    {
        errno = EINVAL;
        return nullptr;
    }

    try
    {
        const std::string sUtf8Path = RainmanWideToUtf8(sPath);
        const std::string sUtf8Mode = RainmanWideToUtf8(sMode);
        return std::fopen(sUtf8Path.c_str(), sUtf8Mode.c_str());
    }
    catch (const std::range_error &)
    {
        errno = EILSEQ;
        return nullptr;
    }
}
#endif
