/*
    This file is part of Corsix's Mod Studio.

    Corsix's Mod Studio is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Corsix's Mod Studio is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Corsix's Mod Studio; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "CRgdEditorPresenter.h"
#include "common/strconv.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <string_view>

extern "C"
{
    typedef unsigned long int ub4;
    typedef unsigned char ub1;
    ub4 hash(ub1 *k, ub4 length, ub4 initval);
}

CRgdEditorPresenter::CRgdEditorPresenter(IRgdEditorView &view) : m_view(view) {}

wxString CRgdEditorPresenter::FormatNodeName(const char *sName, unsigned long iNameHash)
{
    if (sName != nullptr && sName[0] != '\0')
        return AsciiTowxString(sName);

    wxString sResult = wxT("0x");
    for (int iNibble = 7; iNibble >= 0; --iNibble)
    {
        sResult.Append("0123456789ABCDEF"[(iNameHash >> (iNibble << 2)) & 15]);
    }
    return sResult;
}

wxString CRgdEditorPresenter::FormatTableReference(const wxString &sReference, bool bIsGameData, bool bHasReference)
{
    if (!bHasReference)
        return wxT("{}");

    if (bIsGameData)
        return wxString(wxT("Inherit([[")).Append(sReference).Append(wxT("]])"));

    return wxString(wxT("Reference([[")).Append(sReference).Append(wxT("]])"));
}

unsigned long CRgdEditorPresenter::ComputeMultiHash(const wchar_t *sName)
{
    size_t iLen = wcslen(sName);
    while (iLen > 0 && sName[iLen - 1] >= '0' && sName[iLen - 1] <= '9')
    {
        --iLen;
    }
    auto saAscii = std::unique_ptr<char[]>(UnicodeToAscii(sName));
    return hash(reinterpret_cast<ub1 *>(saAscii.get()), static_cast<ub4>(iLen), 0);
}

std::string CRgdEditorPresenter::NormaliseLua2SavePath(const char *sFilePath)
{
    const std::string_view sPath(sFilePath);
    const auto startsWithIgnoringCase = [sPath](std::string_view sPrefix)
    {
        return sPath.size() >= sPrefix.size() &&
               std::equal(sPrefix.begin(), sPrefix.end(), sPath.begin(), [](unsigned char cLeft, unsigned char cRight)
                          { return std::tolower(cLeft) == std::tolower(cRight); });
    };

    size_t iChop = 0;
    if (startsWithIgnoringCase("Generic\\Attrib\\"))
        iChop = 15;
    else if (startsWithIgnoringCase("Data\\Attrib\\"))
        iChop = 12;
    else if (startsWithIgnoringCase("Attrib\\Attrib\\"))
        iChop = 14;

    std::string sResult;
    if (iChop > 0)
    {
        const char *pSrc = sFilePath + iChop;
        sResult.reserve(std::strlen(pSrc));
        while (*pSrc != '\0')
        {
            sResult.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(*pSrc))));
            ++pSrc;
        }
    }
    else
    {
        sResult = sFilePath;
        for (auto &ch : sResult)
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return sResult;
}
