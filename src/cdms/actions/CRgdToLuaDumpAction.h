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

#pragma once

#include "views/frmFiles.h"
#include "common/ThemeColours.h"
#include "frame/Construct.h"
#include "common/Utility.h"
#include "common/strconv.h"
#include "common/strings.h"
#include <rainman/formats/CRgdFile.h>
#include <rainman/lua/CLuaFromRgd.h>

class CRgdToLuaDumpAction : public frmFiles::IHandler
{
  public:
    virtual wxString VGetExt() const { return wxT("rgd"); }

    virtual wxString VGetAction() const { return wxT("Dump RGD to Lua"); }

    virtual void VHandle(wxString sFile, wxTreeItemId &oParent, wxTreeItemId &oFile)
    {
        // CFilesTreeItemData *pData = (CFilesTreeItemData*)TheConstruct->GetFilesList()->GetTree()->GetItemData(oFile);
        auto saFile = wxStringToAscii(sFile);
        try
        {
            DoConvert(saFile.get());
        }
        catch (const CRainmanException &e)
        {
            ErrorBoxE(e);
            return;
        }

        char *saDir = strdup(saFile.get()), *pSlash;
        pSlash = strrchr(saDir, '\\');
        if (pSlash)
        {
            *pSlash = 0;
        }
        else
        {
            *saDir = 0;
        }
        auto itrResult = TheConstruct->GetFileService().Iterate(AsciiTowxString(saDir));
        free(saDir);
        IDirectoryTraverser::IIterator *pDir = itrResult ? itrResult.value().release() : nullptr;
        TheConstruct->GetFilesList()->UpdateDirectoryChildren(oParent, pDir);
        delete pDir;
        ThemeColours::ShowMessageBox(AppStr(luadump_good), VGetAction(), wxICON_INFORMATION, TheConstruct);
    }

    static void DoConvert(char *saFile)
    {
        char *saOutFile = strdup(saFile);
        if (!saOutFile)
        {
            throw CModStudioException(__FILE__, __LINE__, "Memory allocation error");
        }
        strcpy(strchr(saOutFile, '.'), ".lua");

        IFileStore::IStream *pIn = 0;
        IFileStore::IOutputStream *pOut = 0;
        CModuleFile *pMod = TheConstruct->GetModuleService().GetModule();
        try
        {
            auto inResult = TheConstruct->GetFileService().OpenStream(AsciiTowxString(saFile));
            if (!inResult)
            {
                throw CModStudioException(0, __FILE__, __LINE__, "Cannot open streams for \'%s\'", saFile);
            }
            pIn = inResult.value().release();
            auto outResult = TheConstruct->GetFileService().OpenOutputStream(AsciiTowxString(saOutFile), true);
            if (!outResult)
            {
                delete pIn;
                pIn = 0;
                throw CModStudioException(0, __FILE__, __LINE__, "Cannot open streams for \'%s\'", saFile);
            }
            pOut = outResult.value().release();
        }
        catch (const CRainmanException &e)
        {
            delete pIn;
            delete pOut;
            free(saOutFile);
            throw CModStudioException(e, __FILE__, __LINE__, "Cannot open streams for \'%s\'", saFile);
        }

        CRgdFile oRgd;
        oRgd.SetHashTable(TheConstruct->GetRgdHashTable());
        try
        {
            oRgd.Load(pIn, saFile);
        }
        catch (const CRainmanException &e)
        {
            delete pIn;
            delete pOut;
            free(saOutFile);
            throw CModStudioException(e, __FILE__, __LINE__, "Cannot load file \'%s\'", saFile);
        }

        try
        {
            // MakeLuaFromRgdQuickly(&oRgd, pOut);
            MakeLuaFromRgdAndNil(&oRgd, 0, pMod, pOut, pMod);
        }
        catch (const CRainmanException &e)
        {
            delete pIn;
            delete pOut;
            free(saOutFile);
            throw CModStudioException(e, __FILE__, __LINE__, "Cannot turn file into lua \'%s\'", saFile);
        }

        delete pIn;
        delete pOut;
        free(saOutFile);
    }
};
