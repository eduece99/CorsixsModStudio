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
#include "actions/CBfxToLuaDumpAction.h"
#include <rainman/formats/CRgtFile.h>
#include <rainman/core/RainmanLog.h>
#include <rainman/core/WideFile.h>
#include <memory>
#include <wx/progdlg.h>
#include <zlib.h>
extern "C"
{
#include <lauxlib.h>
#include <lualib.h>
}

class CBFXRGTDeBurnAction : public frmFiles::IHandler
{
  protected:
    wxProgressDialog *m_pProgress;

  public:
    CBFXRGTDeBurnAction() : m_pProgress(0) {}
    virtual wxString VGetExt() const { return wxT(""); }

    virtual wxString VGetAction() const { return wxT("Unburn all BFX/RGT files in this folder"); }

    virtual size_t Recurse(wxString sFolder, wxTreeItemId &oParent, bool bCountOnly, size_t iCountBase,
                           size_t iCountDiv)
    {
        size_t iCount = 0;
        wxTreeItemIdValue oCookie;
        wxTreeCtrl *pTree = TheConstruct->GetFilesList()->GetTree();
        wxTreeItemId oChild = pTree->GetFirstChild(oParent, oCookie);
        while (oChild.IsOk())
        {
            CFilesTreeItemData *pData = (CFilesTreeItemData *)pTree->GetItemData(oChild);
            if (pData->sMod)
            {
                wxString sFile = pTree->GetItemText(oChild);
                if (sFile.AfterLast('.').IsSameAs(wxT("bfx"), false) ||
                    sFile.AfterLast('.').IsSameAs(wxT("rgt"), false))
                {
                    ++iCount;
                    if (!bCountOnly)
                    {
                        int iPVal = (int)((iCountBase + iCount) / iCountDiv);
                        m_pProgress->Update(iPVal, sFile);

                        sFile = sFolder + wxT("\\") + pTree->GetItemText(oChild);
                        auto saFile = wxStringToAscii(sFile);

                        if (sFile.AfterLast('.').IsSameAs(wxT("bfx"), false))
                        {
                            CBfxToLuaDumpAction::DoConvert(saFile.get(), L);
                        }
                        else
                        {
                            auto streamResult = TheConstruct->GetFileService().OpenStream(sFile);
                            if (!streamResult)
                            {
                                ErrorBox("Cannot open file");
                                return iCount;
                            }
                            auto &stream = streamResult.value();

                            auto pRgt = std::make_unique<CRgtFile>();
                            pRgt->Load(stream.get());

                            strcpy(strrchr(saFile.get(), '.'), ".tga");
                            auto outResult =
                                TheConstruct->GetFileService().OpenOutputStream(AsciiTowxString(saFile.get()), true);
                            pRgt->SaveTGA(outResult ? outResult.value().get() : nullptr);
                        }
                    }
                }
            }
            else
            {
                wxString sNewPath = sFolder;
                sNewPath.Append('\\');
                sNewPath.Append(pTree->GetItemText(oChild));
                iCount += Recurse(sNewPath, oChild, bCountOnly, iCountBase + iCount, iCountDiv);
            }
            oChild = pTree->GetNextChild(oChild, oCookie);
        }
        if (!bCountOnly)
        {
            if (pTree->IsExpanded(oParent))
            {
                auto itrResult = TheConstruct->GetFileService().Iterate(sFolder);
                auto pDir =
                    itrResult ? std::unique_ptr<IDirectoryTraverser::IIterator>(itrResult.value().release()) : nullptr;
                TheConstruct->GetFilesList()->UpdateDirectoryChildren(oParent, pDir.get());
            }
        }
        return iCount;
    }

    lua_State *L;
    void load_lua()
    {
        L = lua_open();
        luaopen_base(L);

        FILE *f = RainmanFOpen(AppStr(app_bfxmapfile), L"rb");
        if (!f)
        {
            CDMS_LOG_WARN("Could not open BFX map file");
            return;
        }
        unsigned long iSizeComp, iSizeDecomp;
        fread(&iSizeDecomp, sizeof(unsigned long), 1, f);
        fread(&iSizeComp, sizeof(unsigned long), 1, f);
        unsigned char *pCompressed = new unsigned char[iSizeComp];
        fread(pCompressed, 1, iSizeComp, f);
        fclose(f);

        auto pUncompressed = std::make_unique<unsigned char[]>(iSizeDecomp);
        uncompress(pUncompressed.get(), &iSizeDecomp, pCompressed, iSizeComp);
        delete[] pCompressed;

        int iE = luaL_loadbuffer(L, (const char *)pUncompressed.get(), iSizeDecomp, "map");

        if (iE == 0)
        {
            iE = lua_pcall(L, 0, 0, 0);
            if (iE == 0)
            {
                lua_getglobal(L, "map");
            }
        }
        if (iE)
        {
            const char *sE = lua_tostring(L, -1);
            sE = sE;
        }
    }

    virtual void VHandle(wxString sFile, wxTreeItemId &oParent, wxTreeItemId &oFile)
    {
        UNUSED(oFile);
        load_lua();

        size_t iCount = Recurse(sFile, oParent, true, 0, 1);
        size_t iDiv = 1;
        while ((iCount / iDiv) > (size_t)0x200)
        {
            ++iDiv;
        }
        m_pProgress = new wxProgressDialog(VGetAction(), wxT(""), (int)(iCount / iDiv) + 1, TheConstruct,
                                           wxPD_SMOOTH | wxPD_AUTO_HIDE | wxPD_ELAPSED_TIME | wxPD_REMAINING_TIME);
        Recurse(sFile, oParent, false, 0, iDiv);
        ThemeColours::ShowMessageBox(AppStr(bfxrgt_convertgood), VGetAction(), wxICON_INFORMATION, TheConstruct);
        delete m_pProgress;

        lua_close(L);
    }
};
