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

#include "ScarReferenceLoader.h"
#include <cstdint>
#include <cstdio>
#include <rainman/core/RainmanLog.h>
#include <rainman/core/WideFile.h>

/// Read a length-prefixed string from a binary SCAR reference file.
static std::string ReadNiceString(FILE *f)
{
    uint32_t iLen;
    fread(&iLen, sizeof(uint32_t), 1, f);
    std::string s(iLen, '\0');
    fread(s.data(), iLen, 1, f);
    return s;
}

std::vector<ScarFunctionDef> LoadScarReference(const wchar_t *pFilePath)
{
    std::vector<ScarFunctionDef> result;

    FILE *f = RainmanFOpen(pFilePath, L"rb");
    if (!f)
    {
        CDMS_LOG_WARN("Could not open SCAR reference file");
        return result;
    }

    uint32_t iFnCount;
    fread(&iFnCount, sizeof(uint32_t), 1, f);
    result.reserve(iFnCount);

    for (uint32_t i = 0; i < iFnCount; ++i)
    {
        ScarFunctionDef oFunction;
        oFunction.sReturn = ReadNiceString(f);
        oFunction.sName = ReadNiceString(f);
        oFunction.iType = 0;

        if (oFunction.sName.empty())
        {
            // Constant entry: the "return" field is actually the constant name
            oFunction.iType = 1;
            oFunction.sName = std::move(oFunction.sReturn);
            oFunction.sReturn.clear();
        }
        else
        {
            uint32_t iArgCount;
            fread(&iArgCount, sizeof(uint32_t), 1, f);
            for (uint32_t j = 0; j < iArgCount; ++j)
            {
                oFunction.sArguments.push_back(ReadNiceString(f));
            }
            oFunction.sDesc = ReadNiceString(f);
        }

        result.push_back(std::move(oFunction));
    }

    fclose(f);
    return result;
}
